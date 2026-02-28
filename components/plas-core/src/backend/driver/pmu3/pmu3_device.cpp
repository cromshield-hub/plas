#include "plas/backend/driver/pmu3/pmu3_device.h"

#include "plas/backend/interface/device_factory.h"
#include "plas/log/logger.h"

namespace plas::backend::driver {

Pmu3Device::Pmu3Device(const config::DeviceEntry& entry)
    : name_(entry.nickname),
      uri_(entry.uri),
      state_(DeviceState::kUninitialized),
      handle_(-1) {}

// ---------------------------------------------------------------------------
// Device interface
// ---------------------------------------------------------------------------

core::Result<void> Pmu3Device::Init() {
    PLAS_LOG_INFO("Pmu3Device::Init() [stub] for device '" + name_ + "'");
    state_ = DeviceState::kInitialized;
    return core::Result<void>::Ok();
}

core::Result<void> Pmu3Device::Open() {
    PLAS_LOG_INFO("Pmu3Device::Open() [stub] for device '" + name_ + "'");
    state_ = DeviceState::kOpen;
    return core::Result<void>::Ok();
}

core::Result<void> Pmu3Device::Close() {
    PLAS_LOG_INFO("Pmu3Device::Close() [stub] for device '" + name_ + "'");
    handle_ = -1;
    state_ = DeviceState::kClosed;
    return core::Result<void>::Ok();
}

core::Result<void> Pmu3Device::Reset() {
    PLAS_LOG_INFO("Pmu3Device::Reset() [stub] for device '" + name_ + "'");
    auto result = Close();
    if (result.IsError()) {
        return result;
    }
    return Init();
}

DeviceState Pmu3Device::GetState() const {
    return state_;
}

std::string Pmu3Device::GetName() const {
    return name_;
}

std::string Pmu3Device::GetUri() const {
    return uri_;
}

// ---------------------------------------------------------------------------
// PowerControl interface
// ---------------------------------------------------------------------------

core::Result<void> Pmu3Device::SetVoltage(core::Voltage voltage) {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    (void)voltage;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<core::Voltage> Pmu3Device::GetVoltage() {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    return core::Result<core::Voltage>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu3Device::SetCurrent(core::Current current) {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    (void)current;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<core::Current> Pmu3Device::GetCurrent() {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    return core::Result<core::Current>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu3Device::PowerOn() {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu3Device::PowerOff() {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<bool> Pmu3Device::IsPowerOn() {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    return core::Result<bool>::Err(core::ErrorCode::kNotSupported);
}

// ---------------------------------------------------------------------------
// SsdGpio interface
// ---------------------------------------------------------------------------

core::Result<void> Pmu3Device::SetPerst(bool active) {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    (void)active;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<bool> Pmu3Device::GetPerst() {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    return core::Result<bool>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu3Device::SetClkReq(bool active) {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    (void)active;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<bool> Pmu3Device::GetClkReq() {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    return core::Result<bool>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu3Device::SetDualPort(bool enable) {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    (void)enable;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<bool> Pmu3Device::GetDualPort() {
    PLAS_LOG_WARN("PMU3 driver not yet implemented");
    return core::Result<bool>::Err(core::ErrorCode::kNotSupported);
}

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------

void Pmu3Device::Register() {
    DeviceFactory::RegisterDriver(
        "pmu3", [](const config::DeviceEntry& entry) {
            return std::make_unique<Pmu3Device>(entry);
        });
}

}  // namespace plas::backend::driver
