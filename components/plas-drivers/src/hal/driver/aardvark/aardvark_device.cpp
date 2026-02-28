#include "plas/hal/driver/aardvark/aardvark_device.h"

#include <cstdlib>
#include <cstring>
#include <string>

#ifdef PLAS_HAS_AARDVARK
extern "C" {
#include <aardvark.h>
}
#endif

#include "plas/core/error.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/log/logger.h"

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
      handle_(-1),
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

#ifdef PLAS_HAS_AARDVARK
    Aardvark handle = aa_open(static_cast<int>(port_));
    if (handle <= 0) {
        PLAS_LOG_ERROR("AardvarkDevice::Open() aa_open failed: " +
                       std::to_string(handle));
        return core::Result<void>::Err(MapAardvarkError(handle));
    }
    handle_ = handle;

    // Configure as I2C master
    aa_configure(handle_, AA_CONFIG_GPIO_I2C);

    // Set bitrate (SDK takes kHz, we store Hz)
    aa_i2c_bitrate(handle_, static_cast<int>(bitrate_ / 1000));

    // Pull-up resistors
    aa_i2c_pullup(handle_,
                  pullup_enabled_ ? AA_I2C_PULLUP_BOTH : AA_I2C_PULLUP_NONE);

    // Bus timeout
    aa_i2c_bus_timeout(handle_, bus_timeout_ms_);

    PLAS_LOG_INFO("AardvarkDevice::Open() device='" + name_ +
                  "' handle=" + std::to_string(handle_));
#else
    PLAS_LOG_WARN("AardvarkDevice::Open() [stub — SDK not available] device='" +
                  name_ + "'");
#endif

    state_ = DeviceState::kOpen;
    return core::Result<void>::Ok();
}

core::Result<void> AardvarkDevice::Close() {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kAlreadyClosed);
    }

#ifdef PLAS_HAS_AARDVARK
    if (handle_ >= 0) {
        aa_close(handle_);
        PLAS_LOG_INFO("AardvarkDevice::Close() device='" + name_ + "'");
    }
#else
    PLAS_LOG_INFO("AardvarkDevice::Close() [stub] device='" + name_ + "'");
#endif

    handle_ = -1;
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
    handle_ = -1;
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

// ---------------------------------------------------------------------------
// I2c interface
// ---------------------------------------------------------------------------

core::Result<size_t> AardvarkDevice::Read(core::Address addr,
                                          core::Byte* data, size_t length) {
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
    std::lock_guard<std::mutex> lock(i2c_mutex_);
    int result = aa_i2c_read(handle_, static_cast<uint16_t>(addr),
                             AA_I2C_NO_FLAGS, static_cast<uint16_t>(length),
                             data);
    if (result < 0) {
        return core::Result<size_t>::Err(MapAardvarkError(result));
    }
    return core::Result<size_t>::Ok(static_cast<size_t>(result));
#else
    (void)addr;
    (void)data;
    (void)length;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
#endif
}

core::Result<size_t> AardvarkDevice::Write(core::Address addr,
                                           const core::Byte* data,
                                           size_t length) {
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
    std::lock_guard<std::mutex> lock(i2c_mutex_);
    int result = aa_i2c_write(handle_, static_cast<uint16_t>(addr),
                              AA_I2C_NO_FLAGS, static_cast<uint16_t>(length),
                              data);
    if (result < 0) {
        return core::Result<size_t>::Err(MapAardvarkError(result));
    }
    return core::Result<size_t>::Ok(static_cast<size_t>(result));
#else
    (void)addr;
    (void)data;
    (void)length;
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
    std::lock_guard<std::mutex> lock(i2c_mutex_);
    int result = aa_i2c_write_read(handle_, static_cast<uint16_t>(addr),
                                   AA_I2C_NO_FLAGS,
                                   static_cast<uint16_t>(write_len),
                                   write_data,
                                   static_cast<uint16_t>(read_len),
                                   read_data);
    if (result < 0) {
        return core::Result<size_t>::Err(MapAardvarkError(result));
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
    bitrate_ = bitrate;

#ifdef PLAS_HAS_AARDVARK
    if (state_ == DeviceState::kOpen && handle_ >= 0) {
        int actual_khz = aa_i2c_bitrate(handle_,
                                        static_cast<int>(bitrate_ / 1000));
        if (actual_khz < 0) {
            return core::Result<void>::Err(MapAardvarkError(actual_khz));
        }
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

}  // namespace plas::hal::driver
