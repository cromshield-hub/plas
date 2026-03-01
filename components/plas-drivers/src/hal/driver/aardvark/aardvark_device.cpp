#include "plas/hal/driver/aardvark/aardvark_device.h"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

#ifdef PLAS_HAS_AARDVARK
extern "C" {
#include <aardvark.h>
}
#endif

#include "plas/core/error.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/log/logger.h"

// ---------------------------------------------------------------------------
// AardvarkBusState — shared SDK handle + bus mutex per port
// ---------------------------------------------------------------------------

namespace plas::hal::driver {

struct AardvarkBusState {
    uint16_t   port           = 0;
    int        handle         = -1;    // aa_open() result; -1 in stub mode
    std::mutex bus_mutex;              // serializes Read/Write/WriteRead
    uint32_t   active_bitrate = 0;    // 0 = unset (first Open records it)
    bool       active_pullup  = true;
    uint16_t   active_timeout = 200;
    int        ref_count      = 0;    // Open +1 / Close -1; aa_close at 0
};

}  // namespace plas::hal::driver

// ---------------------------------------------------------------------------
// Bus registry: port → weak_ptr<AardvarkBusState>
//
// Two mutexes — never held simultaneously:
//   GetRegistryMutex()      guards map CRUD (Open/Close registry section only)
//   bus_state_->bus_mutex   guards SDK handle I/O (Read/Write/WriteRead)
// ---------------------------------------------------------------------------

namespace {

using BusRegistry =
    std::unordered_map<uint16_t,
                       std::weak_ptr<plas::hal::driver::AardvarkBusState>>;

BusRegistry& GetBusRegistry() {
    static BusRegistry registry;
    return registry;
}

std::mutex& GetRegistryMutex() {
    static std::mutex m;
    return m;
}

}  // namespace

namespace plas::hal::driver {

// ---------------------------------------------------------------------------
// Error mapping (SDK → ErrorCode)
// ---------------------------------------------------------------------------

#ifdef PLAS_HAS_AARDVARK
static core::ErrorCode MapAardvarkError(int status) {
    switch (status) {
        case AA_UNABLE_TO_OPEN:
        case AA_UNABLE_TO_CLOSE:
        case AA_COMMUNICATION_ERROR:
            return core::ErrorCode::kIOError;
        case AA_I2C_NOT_AVAILABLE:
            return core::ErrorCode::kNotSupported;
        case AA_I2C_SLAVE_TIMEOUT:
            return core::ErrorCode::kTimeout;
        case AA_I2C_DROPPED_EXCESS_BYTES:
            return core::ErrorCode::kDataLoss;
        case AA_I2C_BUS_ALREADY_FREE:
            return core::ErrorCode::kSuccess;
        default:
            return core::ErrorCode::kIOError;
    }
}
#endif

// ---------------------------------------------------------------------------
// URI parsing: aardvark://port:address
// ---------------------------------------------------------------------------

bool AardvarkDevice::ParseUri(const std::string& uri, uint16_t& port,
                              uint16_t& addr) {
    const std::string prefix = "aardvark://";
    if (uri.size() <= prefix.size() ||
        uri.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }

    auto body = uri.substr(prefix.size());
    auto colon = body.find(':');
    if (colon == std::string::npos || colon == 0 ||
        colon == body.size() - 1) {
        return false;
    }

    auto port_str = body.substr(0, colon);
    auto addr_str = body.substr(colon + 1);

    // Parse port (decimal)
    char* end = nullptr;
    unsigned long port_val = std::strtoul(port_str.c_str(), &end, 10);
    if (end == port_str.c_str() || *end != '\0' || port_val > 65535) {
        return false;
    }

    // Parse address (hex or decimal via strtoul base 0)
    end = nullptr;
    unsigned long addr_val = std::strtoul(addr_str.c_str(), &end, 0);
    if (end == addr_str.c_str() || *end != '\0' || addr_val > 0x7F) {
        return false;
    }

    port = static_cast<uint16_t>(port_val);
    addr = static_cast<uint16_t>(addr_val);
    return true;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AardvarkDevice::AardvarkDevice(const config::DeviceEntry& entry)
    : name_(entry.nickname),
      uri_(entry.uri),
      state_(DeviceState::kUninitialized),
      bitrate_(100000),
      port_(0),
      default_addr_(0),
      pullup_enabled_(true),
      bus_timeout_ms_(200) {
    // Parse optional config args
    auto it = entry.args.find("bitrate");
    if (it != entry.args.end()) {
        char* end = nullptr;
        unsigned long val = std::strtoul(it->second.c_str(), &end, 10);
        if (end != it->second.c_str() && *end == '\0' && val > 0) {
            bitrate_ = static_cast<uint32_t>(val);
        }
    }

    it = entry.args.find("pullup");
    if (it != entry.args.end()) {
        if (it->second == "false" || it->second == "0") {
            pullup_enabled_ = false;
        } else if (it->second == "true" || it->second == "1") {
            pullup_enabled_ = true;
        }
    }

    it = entry.args.find("bus_timeout_ms");
    if (it != entry.args.end()) {
        char* end = nullptr;
        unsigned long val = std::strtoul(it->second.c_str(), &end, 10);
        if (end != it->second.c_str() && *end == '\0' && val <= 65535) {
            bus_timeout_ms_ = static_cast<uint16_t>(val);
        }
    }
}

AardvarkDevice::~AardvarkDevice() {
    if (state_ == DeviceState::kOpen) {
        Close();
    }
}

// ---------------------------------------------------------------------------
// Device interface
// ---------------------------------------------------------------------------

core::Result<void> AardvarkDevice::Init() {
    if (state_ != DeviceState::kUninitialized &&
        state_ != DeviceState::kClosed) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (!ParseUri(uri_, port_, default_addr_)) {
        PLAS_LOG_ERROR("AardvarkDevice::Init() invalid URI: " + uri_);
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    PLAS_LOG_INFO("AardvarkDevice::Init() port=" + std::to_string(port_) +
                  " addr=0x" +
                  std::to_string(default_addr_) + " device='" + name_ + "'");
    state_ = DeviceState::kInitialized;
    return core::Result<void>::Ok();
}

core::Result<void> AardvarkDevice::Open() {
    if (state_ != DeviceState::kInitialized) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }

    {
        std::lock_guard<std::mutex> reg_lock(GetRegistryMutex());

        auto& registry = GetBusRegistry();
        auto it = registry.find(port_);
        if (it != registry.end()) {
            auto existing = it->second.lock();
            if (existing) {
                // Check for config conflicts and warn
                if (existing->active_bitrate != 0) {
                    if (bitrate_ != existing->active_bitrate) {
                        PLAS_LOG_WARN(
                            "AardvarkDevice::Open() device='" + name_ +
                            "' bitrate conflict: requested=" +
                            std::to_string(bitrate_) + " active=" +
                            std::to_string(existing->active_bitrate));
                    }
                    if (pullup_enabled_ != existing->active_pullup) {
                        PLAS_LOG_WARN(
                            "AardvarkDevice::Open() device='" + name_ +
                            "' pullup conflict");
                    }
                    if (bus_timeout_ms_ != existing->active_timeout) {
                        PLAS_LOG_WARN(
                            "AardvarkDevice::Open() device='" + name_ +
                            "' bus_timeout_ms conflict: requested=" +
                            std::to_string(bus_timeout_ms_) + " active=" +
                            std::to_string(existing->active_timeout));
                    }
                }
                bus_state_ = existing;
                bus_state_->ref_count++;
                PLAS_LOG_INFO(
                    "AardvarkDevice::Open() device='" + name_ +
                    "' joined shared bus port=" + std::to_string(port_) +
                    " ref=" + std::to_string(bus_state_->ref_count));
                state_ = DeviceState::kOpen;
                return core::Result<void>::Ok();
            }
            // weak_ptr expired — remove stale entry
            registry.erase(it);
        }

        // No existing bus state — open a new one
        auto new_state = std::make_shared<AardvarkBusState>();
        new_state->port = port_;

#ifdef PLAS_HAS_AARDVARK
        Aardvark handle = aa_open(static_cast<int>(port_));
        if (handle <= 0) {
            PLAS_LOG_ERROR("AardvarkDevice::Open() aa_open failed: " +
                           std::to_string(handle));
            return core::Result<void>::Err(MapAardvarkError(handle));
        }
        new_state->handle = static_cast<int>(handle);

        // Configure as I2C master
        aa_configure(handle, AA_CONFIG_GPIO_I2C);
        aa_i2c_bitrate(handle, static_cast<int>(bitrate_ / 1000));
        aa_i2c_pullup(handle,
                      pullup_enabled_ ? AA_I2C_PULLUP_BOTH : AA_I2C_PULLUP_NONE);
        aa_i2c_bus_timeout(handle, bus_timeout_ms_);
#else
        PLAS_LOG_WARN(
            "AardvarkDevice::Open() [stub — SDK not available] device='" +
            name_ + "'");
#endif

        new_state->active_bitrate = bitrate_;
        new_state->active_pullup  = pullup_enabled_;
        new_state->active_timeout = bus_timeout_ms_;
        new_state->ref_count      = 1;

        registry[port_] = new_state;  // store weak_ptr
        bus_state_      = new_state;  // hold strong ref

        PLAS_LOG_INFO("AardvarkDevice::Open() device='" + name_ +
                      "' opened new bus port=" + std::to_string(port_) +
                      " handle=" + std::to_string(new_state->handle));
    }

    state_ = DeviceState::kOpen;
    return core::Result<void>::Ok();
}

core::Result<void> AardvarkDevice::Close() {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kAlreadyClosed);
    }

    {
        std::lock_guard<std::mutex> reg_lock(GetRegistryMutex());

        bus_state_->ref_count--;
        if (bus_state_->ref_count == 0) {
#ifdef PLAS_HAS_AARDVARK
            if (bus_state_->handle >= 0) {
                aa_close(bus_state_->handle);
            }
#endif
            GetBusRegistry().erase(port_);
            PLAS_LOG_INFO("AardvarkDevice::Close() device='" + name_ +
                          "' closed bus port=" + std::to_string(port_));
        } else {
            PLAS_LOG_INFO(
                "AardvarkDevice::Close() device='" + name_ +
                "' released shared ref, remaining=" +
                std::to_string(bus_state_->ref_count));
        }
    }

    bus_state_.reset();
    state_ = DeviceState::kClosed;
    return core::Result<void>::Ok();
}

core::Result<void> AardvarkDevice::Reset() {
    if (state_ == DeviceState::kOpen) {
        auto result = Close();
        if (result.IsError()) {
            return result;
        }
    }
    // Reset fields so Init can re-parse
    state_ = DeviceState::kUninitialized;
    return Init();
}

DeviceState AardvarkDevice::GetState() const {
    return state_;
}

std::string AardvarkDevice::GetName() const {
    return name_;
}

std::string AardvarkDevice::GetUri() const {
    return uri_;
}

std::string AardvarkDevice::GetDriverName() const {
    return "aardvark";
}

Device* AardvarkDevice::GetDevice() {
    return this;
}

// ---------------------------------------------------------------------------
// I2c interface
// ---------------------------------------------------------------------------

core::Result<size_t> AardvarkDevice::Read(core::Address addr,
                                          core::Byte* data, size_t length,
                                          bool stop) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<size_t>::Err(core::ErrorCode::kNotInitialized);
    }
    if (data == nullptr || length == 0) {
        return core::Result<size_t>::Err(core::ErrorCode::kInvalidArgument);
    }
    if (length > 0xFFFF) {
        return core::Result<size_t>::Err(core::ErrorCode::kInvalidArgument);
    }

#ifdef PLAS_HAS_AARDVARK
    std::lock_guard<std::mutex> lock(bus_state_->bus_mutex);
    auto flags = static_cast<uint16_t>(stop ? AA_I2C_NO_FLAGS : AA_I2C_NO_STOP);
    int result = aa_i2c_read(bus_state_->handle, static_cast<uint16_t>(addr),
                             flags, static_cast<uint16_t>(length), data);
    if (result < 0) {
        auto err = MapAardvarkError(result);
        PLAS_LOG_ERROR("[" + name_ + "][I2c] Read addr=" + std::to_string(addr) +
                       " len=" + std::to_string(length) + " failed: " +
                       make_error_code(err).message());
        return core::Result<size_t>::Err(err);
    }
    return core::Result<size_t>::Ok(static_cast<size_t>(result));
#else
    (void)addr;
    (void)data;
    (void)length;
    (void)stop;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
#endif
}

core::Result<size_t> AardvarkDevice::Write(core::Address addr,
                                           const core::Byte* data,
                                           size_t length, bool stop) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<size_t>::Err(core::ErrorCode::kNotInitialized);
    }
    if (data == nullptr || length == 0) {
        return core::Result<size_t>::Err(core::ErrorCode::kInvalidArgument);
    }
    if (length > 0xFFFF) {
        return core::Result<size_t>::Err(core::ErrorCode::kInvalidArgument);
    }

#ifdef PLAS_HAS_AARDVARK
    std::lock_guard<std::mutex> lock(bus_state_->bus_mutex);
    auto flags = static_cast<uint16_t>(stop ? AA_I2C_NO_FLAGS : AA_I2C_NO_STOP);
    int result = aa_i2c_write(bus_state_->handle, static_cast<uint16_t>(addr),
                              flags, static_cast<uint16_t>(length), data);
    if (result < 0) {
        auto err = MapAardvarkError(result);
        PLAS_LOG_ERROR("[" + name_ + "][I2c] Write addr=" + std::to_string(addr) +
                       " len=" + std::to_string(length) + " failed: " +
                       make_error_code(err).message());
        return core::Result<size_t>::Err(err);
    }
    return core::Result<size_t>::Ok(static_cast<size_t>(result));
#else
    (void)addr;
    (void)data;
    (void)length;
    (void)stop;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
#endif
}

core::Result<size_t> AardvarkDevice::WriteRead(core::Address addr,
                                               const core::Byte* write_data,
                                               size_t write_len,
                                               core::Byte* read_data,
                                               size_t read_len) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<size_t>::Err(core::ErrorCode::kNotInitialized);
    }
    if (write_data == nullptr || write_len == 0 || read_data == nullptr ||
        read_len == 0) {
        return core::Result<size_t>::Err(core::ErrorCode::kInvalidArgument);
    }
    if (write_len > 0xFFFF || read_len > 0xFFFF) {
        return core::Result<size_t>::Err(core::ErrorCode::kInvalidArgument);
    }

#ifdef PLAS_HAS_AARDVARK
    std::lock_guard<std::mutex> lock(bus_state_->bus_mutex);
    int result = aa_i2c_write_read(bus_state_->handle,
                                   static_cast<uint16_t>(addr),
                                   AA_I2C_NO_FLAGS,
                                   static_cast<uint16_t>(write_len),
                                   write_data,
                                   static_cast<uint16_t>(read_len),
                                   read_data);
    if (result < 0) {
        auto err = MapAardvarkError(result);
        PLAS_LOG_ERROR("[" + name_ + "][I2c] WriteRead addr=" + std::to_string(addr) +
                       " wlen=" + std::to_string(write_len) +
                       " rlen=" + std::to_string(read_len) + " failed: " +
                       make_error_code(err).message());
        return core::Result<size_t>::Err(err);
    }
    return core::Result<size_t>::Ok(static_cast<size_t>(result));
#else
    (void)addr;
    (void)write_data;
    (void)write_len;
    (void)read_data;
    (void)read_len;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
#endif
}

core::Result<void> AardvarkDevice::SetBitrate(uint32_t bitrate) {
    if (bitrate == 0) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }
    bitrate_ = bitrate;  // update local cache (GetBitrate() return value)

#ifdef PLAS_HAS_AARDVARK
    if (state_ == DeviceState::kOpen && bus_state_) {
        std::lock_guard<std::mutex> lock(bus_state_->bus_mutex);
        int actual_khz = aa_i2c_bitrate(bus_state_->handle,
                                        static_cast<int>(bitrate_ / 1000));
        if (actual_khz < 0) {
            return core::Result<void>::Err(MapAardvarkError(actual_khz));
        }
        bus_state_->active_bitrate = bitrate_;
        PLAS_LOG_INFO("AardvarkDevice::SetBitrate() actual=" +
                      std::to_string(actual_khz) + " kHz");
    }
#endif

    return core::Result<void>::Ok();
}

uint32_t AardvarkDevice::GetBitrate() const {
    return bitrate_;
}

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------

void AardvarkDevice::Register() {
    DeviceFactory::RegisterDriver(
        "aardvark", [](const config::DeviceEntry& entry) {
            return std::make_unique<AardvarkDevice>(entry);
        });
}

// ---------------------------------------------------------------------------
// Test support
// ---------------------------------------------------------------------------

void AardvarkDevice::ResetBusRegistry() {
    std::lock_guard<std::mutex> lock(GetRegistryMutex());
    GetBusRegistry().clear();
}

}  // namespace plas::hal::driver
