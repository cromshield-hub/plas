#include "plas/backend/driver/ft4222h/ft4222h_device.h"

#include "plas/backend/interface/device_factory.h"
#include "plas/log/logger.h"

namespace plas::backend::driver {

Ft4222hDevice::Ft4222hDevice(const config::DeviceEntry& entry)
    : name_(entry.nickname),
      uri_(entry.uri),
      state_(DeviceState::kUninitialized),
      bitrate_(100000),
      handle_(-1) {}

// ---------------------------------------------------------------------------
// Device interface
// ---------------------------------------------------------------------------

core::Result<void> Ft4222hDevice::Init() {
    PLAS_LOG_INFO("Ft4222hDevice::Init() [stub] for device '" + name_ + "'");
    state_ = DeviceState::kInitialized;
    return core::Result<void>::Ok();
}

core::Result<void> Ft4222hDevice::Open() {
    PLAS_LOG_INFO("Ft4222hDevice::Open() [stub] for device '" + name_ + "'");
    state_ = DeviceState::kOpen;
    return core::Result<void>::Ok();
}

core::Result<void> Ft4222hDevice::Close() {
    PLAS_LOG_INFO("Ft4222hDevice::Close() [stub] for device '" + name_ + "'");
    handle_ = -1;
    state_ = DeviceState::kClosed;
    return core::Result<void>::Ok();
}

core::Result<void> Ft4222hDevice::Reset() {
    PLAS_LOG_INFO("Ft4222hDevice::Reset() [stub] for device '" + name_ + "'");
    auto result = Close();
    if (result.IsError()) {
        return result;
    }
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

core::Result<size_t> Ft4222hDevice::Read(core::Address addr,
                                         core::Byte* data, size_t length) {
    PLAS_LOG_WARN("FT4222H driver not yet implemented");
    (void)addr;
    (void)data;
    (void)length;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
}

core::Result<size_t> Ft4222hDevice::Write(core::Address addr,
                                          const core::Byte* data,
                                          size_t length) {
    PLAS_LOG_WARN("FT4222H driver not yet implemented");
    (void)addr;
    (void)data;
    (void)length;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
}

core::Result<size_t> Ft4222hDevice::WriteRead(core::Address addr,
                                              const core::Byte* write_data,
                                              size_t write_len,
                                              core::Byte* read_data,
                                              size_t read_len) {
    PLAS_LOG_WARN("FT4222H driver not yet implemented");
    (void)addr;
    (void)write_data;
    (void)write_len;
    (void)read_data;
    (void)read_len;
    return core::Result<size_t>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Ft4222hDevice::SetBitrate(uint32_t bitrate) {
    PLAS_LOG_WARN("FT4222H driver not yet implemented");
    bitrate_ = bitrate;
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

}  // namespace plas::backend::driver
