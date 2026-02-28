#include "plas/hal/driver/ft4222h/ft4222h_device.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#ifdef PLAS_HAS_FT4222H
extern "C" {
#include <ftd2xx.h>
#include <libft4222.h>
}
#endif

#include "plas/core/error.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/log/logger.h"

namespace plas::hal::driver {

// ---------------------------------------------------------------------------
// Error mapping (SDK → ErrorCode)
// ---------------------------------------------------------------------------

#ifdef PLAS_HAS_FT4222H
static core::ErrorCode MapFtStatus(FT_STATUS status) {
    switch (status) {
        case FT_OK:
            return core::ErrorCode::kSuccess;
        case FT_DEVICE_NOT_FOUND:
            return core::ErrorCode::kNotFound;
        case FT_DEVICE_NOT_OPENED:
            return core::ErrorCode::kNotInitialized;
        case FT_IO_ERROR:
            return core::ErrorCode::kIOError;
        case FT_INSUFFICIENT_RESOURCES:
            return core::ErrorCode::kOutOfMemory;
        case FT_INVALID_PARAMETER:
            return core::ErrorCode::kInvalidArgument;
        default:
            return core::ErrorCode::kIOError;
    }
}

static core::ErrorCode MapFt4222Status(FT4222_STATUS status) {
    switch (status) {
        case FT4222_OK:
            return core::ErrorCode::kSuccess;
        case FT4222_DEVICE_NOT_FOUND:
            return core::ErrorCode::kNotFound;
        case FT4222_IO_ERROR:
        case FT4222_FAILED_TO_WRITE_DEVICE:
        case FT4222_FAILED_TO_READ_DEVICE:
            return core::ErrorCode::kIOError;
        case FT4222_INVALID_PARAMETER:
        case FT4222_WRONG_I2C_ADDR:
        case FT4222_INVALID_BAUD_RATE:
            return core::ErrorCode::kInvalidArgument;
        case FT4222_IS_NOT_I2C_MODE:
        case FT4222_NOT_SUPPORTED:
            return core::ErrorCode::kNotSupported;
        case FT4222_DEVICE_NOT_OPENED:
            return core::ErrorCode::kNotInitialized;
        case FT4222_INSUFFICIENT_RESOURCES:
            return core::ErrorCode::kOutOfMemory;
        default:
            return core::ErrorCode::kIOError;
    }
}
#endif

// ---------------------------------------------------------------------------
// URI parsing: ft4222h://master_idx:slave_idx
// ---------------------------------------------------------------------------

bool Ft4222hDevice::ParseUri(const std::string& uri, uint16_t& master_idx,
                             uint16_t& slave_idx) {
    const std::string prefix = "ft4222h://";
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

    auto master_str = body.substr(0, colon);
    auto slave_str = body.substr(colon + 1);

    // Parse master index (decimal only)
    char* end = nullptr;
    unsigned long master_val = std::strtoul(master_str.c_str(), &end, 10);
    if (end == master_str.c_str() || *end != '\0' || master_val > 65535) {
        return false;
    }

    // Parse slave index (decimal only)
    end = nullptr;
    unsigned long slave_val = std::strtoul(slave_str.c_str(), &end, 10);
    if (end == slave_str.c_str() || *end != '\0' || slave_val > 65535) {
        return false;
    }

    // Same chip cannot be both master and slave
    if (master_val == slave_val) {
        return false;
    }

    master_idx = static_cast<uint16_t>(master_val);
    slave_idx = static_cast<uint16_t>(slave_val);
    return true;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Ft4222hDevice::Ft4222hDevice(const config::DeviceEntry& entry)
    : name_(entry.nickname),
      uri_(entry.uri),
      state_(DeviceState::kUninitialized),
      master_idx_(0),
      slave_idx_(0),
      master_handle_(nullptr),
      slave_handle_(nullptr),
      bitrate_(400000),
      slave_addr_(0x40),
      sys_clock_(0),
      rx_timeout_ms_(1000),
      rx_poll_interval_us_(100) {
    // Parse optional config args
    auto it = entry.args.find("bitrate");
    if (it != entry.args.end()) {
        char* end = nullptr;
        unsigned long val = std::strtoul(it->second.c_str(), &end, 10);
        if (end != it->second.c_str() && *end == '\0' && val > 0) {
            bitrate_ = static_cast<uint32_t>(val);
        }
    }

    it = entry.args.find("slave_addr");
    if (it != entry.args.end()) {
        char* end = nullptr;
        unsigned long val = std::strtoul(it->second.c_str(), &end, 0);
        if (end != it->second.c_str() && *end == '\0' && val <= 0x7F) {
            slave_addr_ = static_cast<uint8_t>(val);
        }
    }

    it = entry.args.find("sys_clock");
    if (it != entry.args.end()) {
        if (it->second == "60") {
            sys_clock_ = 0;
        } else if (it->second == "24") {
            sys_clock_ = 1;
        } else if (it->second == "48") {
            sys_clock_ = 2;
        } else if (it->second == "80") {
            sys_clock_ = 3;
        }
    }

    it = entry.args.find("rx_timeout_ms");
    if (it != entry.args.end()) {
        char* end = nullptr;
        unsigned long val = std::strtoul(it->second.c_str(), &end, 10);
        if (end != it->second.c_str() && *end == '\0') {
            rx_timeout_ms_ = static_cast<uint32_t>(val);
        }
    }

    it = entry.args.find("rx_poll_interval_us");
    if (it != entry.args.end()) {
        char* end = nullptr;
        unsigned long val = std::strtoul(it->second.c_str(), &end, 10);
        if (end != it->second.c_str() && *end == '\0') {
            rx_poll_interval_us_ = static_cast<uint32_t>(val);
        }
    }
}

Ft4222hDevice::~Ft4222hDevice() {
    if (state_ == DeviceState::kOpen) {
        Close();
    }
}

// ---------------------------------------------------------------------------
// Device interface
// ---------------------------------------------------------------------------

core::Result<void> Ft4222hDevice::Init() {
    if (state_ != DeviceState::kUninitialized &&
        state_ != DeviceState::kClosed) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (!ParseUri(uri_, master_idx_, slave_idx_)) {
        PLAS_LOG_ERROR("Ft4222hDevice::Init() invalid URI: " + uri_);
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    PLAS_LOG_INFO("Ft4222hDevice::Init() master=" +
                  std::to_string(master_idx_) + " slave=" +
                  std::to_string(slave_idx_) + " device='" + name_ + "'");
    state_ = DeviceState::kInitialized;
    return core::Result<void>::Ok();
}

core::Result<void> Ft4222hDevice::Open() {
    if (state_ != DeviceState::kInitialized) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }

#ifdef PLAS_HAS_FT4222H
    // Open master FT device
    FT_HANDLE master_h = nullptr;
    FT_STATUS ft_status = FT_Open(master_idx_, &master_h);
    if (ft_status != FT_OK) {
        PLAS_LOG_ERROR("Ft4222hDevice::Open() FT_Open(master) failed: " +
                       std::to_string(ft_status));
        return core::Result<void>::Err(MapFtStatus(ft_status));
    }

    // Open slave FT device
    FT_HANDLE slave_h = nullptr;
    ft_status = FT_Open(slave_idx_, &slave_h);
    if (ft_status != FT_OK) {
        PLAS_LOG_ERROR("Ft4222hDevice::Open() FT_Open(slave) failed: " +
                       std::to_string(ft_status));
        FT_Close(master_h);
        return core::Result<void>::Err(MapFtStatus(ft_status));
    }

    // Set system clock
    FT4222_STATUS status = FT4222_SetClock(
        master_h, static_cast<FT4222_ClockRate>(sys_clock_));
    if (status != FT4222_OK) {
        PLAS_LOG_ERROR("Ft4222hDevice::Open() FT4222_SetClock failed: " +
                       std::to_string(status));
        FT_Close(slave_h);
        FT_Close(master_h);
        return core::Result<void>::Err(MapFt4222Status(status));
    }

    // Init master as I2C master (SDK takes kbps)
    status = FT4222_I2CMaster_Init(master_h, bitrate_ / 1000);
    if (status != FT4222_OK) {
        PLAS_LOG_ERROR("Ft4222hDevice::Open() I2CMaster_Init failed: " +
                       std::to_string(status));
        FT_Close(slave_h);
        FT_Close(master_h);
        return core::Result<void>::Err(MapFt4222Status(status));
    }

    // Init slave as I2C slave
    status = FT4222_I2CSlave_Init(slave_h);
    if (status != FT4222_OK) {
        PLAS_LOG_ERROR("Ft4222hDevice::Open() I2CSlave_Init failed: " +
                       std::to_string(status));
        FT4222_UnInitialize(master_h);
        FT_Close(slave_h);
        FT_Close(master_h);
        return core::Result<void>::Err(MapFt4222Status(status));
    }

    // Set slave address
    status = FT4222_I2CSlave_SetAddress(slave_h, slave_addr_);
    if (status != FT4222_OK) {
        PLAS_LOG_ERROR("Ft4222hDevice::Open() I2CSlave_SetAddress failed: " +
                       std::to_string(status));
        FT4222_UnInitialize(slave_h);
        FT4222_UnInitialize(master_h);
        FT_Close(slave_h);
        FT_Close(master_h);
        return core::Result<void>::Err(MapFt4222Status(status));
    }

    master_handle_ = master_h;
    slave_handle_ = slave_h;

    PLAS_LOG_INFO("Ft4222hDevice::Open() device='" + name_ + "'");
#else
    PLAS_LOG_WARN(
        "Ft4222hDevice::Open() [stub — SDK not available] device='" + name_ +
        "'");
#endif

    state_ = DeviceState::kOpen;
    return core::Result<void>::Ok();
}

core::Result<void> Ft4222hDevice::Close() {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kAlreadyClosed);
    }

#ifdef PLAS_HAS_FT4222H
    if (slave_handle_ != nullptr) {
        FT4222_UnInitialize(static_cast<FT_HANDLE>(slave_handle_));
        FT_Close(static_cast<FT_HANDLE>(slave_handle_));
    }
    if (master_handle_ != nullptr) {
        FT4222_UnInitialize(static_cast<FT_HANDLE>(master_handle_));
        FT_Close(static_cast<FT_HANDLE>(master_handle_));
    }
    PLAS_LOG_INFO("Ft4222hDevice::Close() device='" + name_ + "'");
#else
    PLAS_LOG_INFO("Ft4222hDevice::Close() [stub] device='" + name_ + "'");
#endif

    master_handle_ = nullptr;
    slave_handle_ = nullptr;
    state_ = DeviceState::kClosed;
    return core::Result<void>::Ok();
}

core::Result<void> Ft4222hDevice::Reset() {
    if (state_ == DeviceState::kOpen) {
        auto result = Close();
        if (result.IsError()) {
            return result;
        }
    }
    state_ = DeviceState::kUninitialized;
    master_handle_ = nullptr;
    slave_handle_ = nullptr;
    return Init();
}

DeviceState Ft4222hDevice::GetState() const {
    return state_;
}

std::string Ft4222hDevice::GetName() const {
    return name_;
}

std::string Ft4222hDevice::GetUri() const {
    return uri_;
}

// ---------------------------------------------------------------------------
// I2c interface
// ---------------------------------------------------------------------------

core::Result<size_t> Ft4222hDevice::Write(core::Address addr,
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

#ifdef PLAS_HAS_FT4222H
    std::lock_guard<std::mutex> lock(i2c_mutex_);
    uint16 transferred = 0;
    // START_AND_STOP (0x06) = normal transfer; START (0x02) = no STOP (Repeated START possible)
    uint8 flag = stop ? START_AND_STOP : START;
    FT4222_STATUS status = FT4222_I2CMaster_WriteEx(
        static_cast<FT_HANDLE>(master_handle_),
        static_cast<uint16>(addr),
        flag,
        const_cast<uint8*>(data),
        static_cast<uint16>(length),
        &transferred);
    if (status != FT4222_OK) {
        return core::Result<size_t>::Err(MapFt4222Status(status));
    }
    return core::Result<size_t>::Ok(static_cast<size_t>(transferred));
#else
    (void)addr;
    (void)data;
    (void)length;
    (void)stop;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
#endif
}

core::Result<size_t> Ft4222hDevice::Read(core::Address addr, core::Byte* data,
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
    // Slave-mode receive: addr and stop are DUT-controlled, not used by host.
    (void)addr;
    (void)stop;

#ifdef PLAS_HAS_FT4222H
    std::lock_guard<std::mutex> lock(i2c_mutex_);

    // Poll slave RX buffer until enough data is available
    auto poll_result = PollSlaveRx(length);
    if (poll_result.IsError()) {
        return core::Result<size_t>::Err(poll_result.Error());
    }

    uint16 transferred = 0;
    FT4222_STATUS status = FT4222_I2CSlave_Read(
        static_cast<FT_HANDLE>(slave_handle_),
        data,
        static_cast<uint16>(length),
        &transferred);
    if (status != FT4222_OK) {
        return core::Result<size_t>::Err(MapFt4222Status(status));
    }
    return core::Result<size_t>::Ok(static_cast<size_t>(transferred));
#else
    (void)data;
    (void)length;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
#endif
}

core::Result<size_t> Ft4222hDevice::WriteRead(core::Address addr,
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

#ifdef PLAS_HAS_FT4222H
    std::lock_guard<std::mutex> lock(i2c_mutex_);

    // Master write without STOP — Repeated START follows for the slave read.
    uint16 write_transferred = 0;
    FT4222_STATUS status = FT4222_I2CMaster_WriteEx(
        static_cast<FT_HANDLE>(master_handle_),
        static_cast<uint16>(addr),
        START,
        const_cast<uint8*>(write_data),
        static_cast<uint16>(write_len),
        &write_transferred);
    if (status != FT4222_OK) {
        return core::Result<size_t>::Err(MapFt4222Status(status));
    }

    // Poll slave RX buffer
    auto poll_result = PollSlaveRx(read_len);
    if (poll_result.IsError()) {
        return core::Result<size_t>::Err(poll_result.Error());
    }

    // Slave read
    uint16 read_transferred = 0;
    status = FT4222_I2CSlave_Read(
        static_cast<FT_HANDLE>(slave_handle_),
        read_data,
        static_cast<uint16>(read_len),
        &read_transferred);
    if (status != FT4222_OK) {
        return core::Result<size_t>::Err(MapFt4222Status(status));
    }
    return core::Result<size_t>::Ok(static_cast<size_t>(read_transferred));
#else
    (void)addr;
    (void)write_data;
    (void)write_len;
    (void)read_data;
    (void)read_len;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
#endif
}

// ---------------------------------------------------------------------------
// PollSlaveRx — deadline-based polling
// ---------------------------------------------------------------------------

core::Result<uint16_t> Ft4222hDevice::PollSlaveRx(size_t expected_len) {
#ifdef PLAS_HAS_FT4222H
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(rx_timeout_ms_);

    while (std::chrono::steady_clock::now() < deadline) {
        uint16 rx_size = 0;
        FT4222_STATUS status = FT4222_I2CSlave_GetRxStatus(
            static_cast<FT_HANDLE>(slave_handle_), &rx_size);
        if (status != FT4222_OK) {
            return core::Result<uint16_t>::Err(MapFt4222Status(status));
        }
        if (rx_size >= static_cast<uint16>(expected_len)) {
            return core::Result<uint16_t>::Ok(rx_size);
        }
        std::this_thread::sleep_for(
            std::chrono::microseconds(rx_poll_interval_us_));
    }

    return core::Result<uint16_t>::Err(core::ErrorCode::kTimeout);
#else
    (void)expected_len;
    return core::Result<uint16_t>::Err(core::ErrorCode::kNotSupported);
#endif
}

// ---------------------------------------------------------------------------
// Bitrate
// ---------------------------------------------------------------------------

core::Result<void> Ft4222hDevice::SetBitrate(uint32_t bitrate) {
    if (bitrate == 0) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }
    bitrate_ = bitrate;

#ifdef PLAS_HAS_FT4222H
    if (state_ == DeviceState::kOpen && master_handle_ != nullptr) {
        FT4222_STATUS status = FT4222_I2CMaster_Init(
            static_cast<FT_HANDLE>(master_handle_), bitrate_ / 1000);
        if (status != FT4222_OK) {
            return core::Result<void>::Err(MapFt4222Status(status));
        }
        PLAS_LOG_INFO("Ft4222hDevice::SetBitrate() " +
                      std::to_string(bitrate_) + " Hz");
    }
#endif

    return core::Result<void>::Ok();
}

uint32_t Ft4222hDevice::GetBitrate() const {
    return bitrate_;
}

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------

void Ft4222hDevice::Register() {
    DeviceFactory::RegisterDriver(
        "ft4222h", [](const config::DeviceEntry& entry) {
            return std::make_unique<Ft4222hDevice>(entry);
        });
}

}  // namespace plas::hal::driver
