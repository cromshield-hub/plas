#include "plas/hal/driver/aardvark/aardvark_device.h"

#include "plas/hal/interface/device_factory.h"
#include "plas/log/logger.h"

namespace plas::hal::driver {

AardvarkDevice::AardvarkDevice(const config::DeviceEntry& entry)
    : name_(entry.nickname),
      uri_(entry.uri),
      state_(DeviceState::kUninitialized),
      bitrate_(100000),
      handle_(-1) {}

// ---------------------------------------------------------------------------
// Device interface
// ---------------------------------------------------------------------------

core::Result<void> AardvarkDevice::Init() {
    PLAS_LOG_INFO("AardvarkDevice::Init() [stub] for device '" + name_ + "'");
    state_ = DeviceState::kInitialized;
    return core::Result<void>::Ok();
}

core::Result<void> AardvarkDevice::Open() {
    PLAS_LOG_INFO("AardvarkDevice::Open() [stub] for device '" + name_ + "'");
    state_ = DeviceState::kOpen;
    return core::Result<void>::Ok();
}

core::Result<void> AardvarkDevice::Close() {
    PLAS_LOG_INFO("AardvarkDevice::Close() [stub] for device '" + name_ + "'");
    handle_ = -1;
    state_ = DeviceState::kClosed;
    return core::Result<void>::Ok();
}

core::Result<void> AardvarkDevice::Reset() {
    PLAS_LOG_INFO("AardvarkDevice::Reset() [stub] for device '" + name_ + "'");
    auto result = Close();
    if (result.IsError()) {
        return result;
    }
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
    PLAS_LOG_WARN("Aardvark driver not yet implemented");
    (void)addr;
    (void)data;
    (void)length;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
}

core::Result<size_t> AardvarkDevice::Write(core::Address addr,
                                           const core::Byte* data,
                                           size_t length) {
    PLAS_LOG_WARN("Aardvark driver not yet implemented");
    (void)addr;
    (void)data;
    (void)length;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
}

core::Result<size_t> AardvarkDevice::WriteRead(core::Address addr,
                                               const core::Byte* write_data,
                                               size_t write_len,
                                               core::Byte* read_data,
                                               size_t read_len) {
    PLAS_LOG_WARN("Aardvark driver not yet implemented");
    (void)addr;
    (void)write_data;
    (void)write_len;
    (void)read_data;
    (void)read_len;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> AardvarkDevice::SetBitrate(uint32_t bitrate) {
    PLAS_LOG_WARN("Aardvark driver not yet implemented");
    bitrate_ = bitrate;
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
