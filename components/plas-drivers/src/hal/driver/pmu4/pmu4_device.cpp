#include "plas/hal/driver/pmu4/pmu4_device.h"

#include "plas/hal/interface/device_factory.h"
#include "plas/log/logger.h"

namespace plas::hal::driver {

Pmu4Device::Pmu4Device(const config::DeviceEntry& entry)
    : name_(entry.nickname),
      uri_(entry.uri),
      state_(DeviceState::kUninitialized),
      handle_(-1) {}

// ---------------------------------------------------------------------------
// Device interface
// ---------------------------------------------------------------------------

core::Result<void> Pmu4Device::Init() {
    PLAS_LOG_INFO("Pmu4Device::Init() [stub] for device '" + name_ + "'");
    state_ = DeviceState::kInitialized;
    return core::Result<void>::Ok();
}

core::Result<void> Pmu4Device::Open() {
    PLAS_LOG_INFO("Pmu4Device::Open() [stub] for device '" + name_ + "'");
    state_ = DeviceState::kOpen;
    return core::Result<void>::Ok();
}

core::Result<void> Pmu4Device::Close() {
    PLAS_LOG_INFO("Pmu4Device::Close() [stub] for device '" + name_ + "'");
    handle_ = -1;
    state_ = DeviceState::kClosed;
    return core::Result<void>::Ok();
}

core::Result<void> Pmu4Device::Reset() {
    PLAS_LOG_INFO("Pmu4Device::Reset() [stub] for device '" + name_ + "'");
    auto result = Close();
    if (result.IsError()) {
        return result;
    }
    return Init();
}

DeviceState Pmu4Device::GetState() const {
    return state_;
}

std::string Pmu4Device::GetName() const {
    return name_;
}

std::string Pmu4Device::GetUri() const {
    return uri_;
}

std::string Pmu4Device::GetDriverName() const {
    return "pmu4";
}

Device* Pmu4Device::GetDevice() {
    return this;
}

// ---------------------------------------------------------------------------
// PowerControl interface
// ---------------------------------------------------------------------------

core::Result<void> Pmu4Device::SetVoltage(core::Voltage voltage) {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    (void)voltage;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<core::Voltage> Pmu4Device::GetVoltage() {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    return core::Result<core::Voltage>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu4Device::SetCurrent(core::Current current) {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    (void)current;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<core::Current> Pmu4Device::GetCurrent() {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    return core::Result<core::Current>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu4Device::PowerOn() {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu4Device::PowerOff() {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<bool> Pmu4Device::IsPowerOn() {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    return core::Result<bool>::Err(core::ErrorCode::kNotSupported);
}

// ---------------------------------------------------------------------------
// SsdGpio interface
// ---------------------------------------------------------------------------

core::Result<void> Pmu4Device::SetPerst(bool active) {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    (void)active;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<bool> Pmu4Device::GetPerst() {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    return core::Result<bool>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu4Device::SetClkReq(bool active) {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    (void)active;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<bool> Pmu4Device::GetClkReq() {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    return core::Result<bool>::Err(core::ErrorCode::kNotSupported);
}

core::Result<void> Pmu4Device::SetDualPort(bool enable) {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    (void)enable;
    return core::Result<void>::Err(core::ErrorCode::kNotSupported);
}

core::Result<bool> Pmu4Device::GetDualPort() {
    PLAS_LOG_WARN("PMU4 driver not yet implemented");
    return core::Result<bool>::Err(core::ErrorCode::kNotSupported);
}

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------

void Pmu4Device::Register() {
    DeviceFactory::RegisterDriver(
        "pmu4", [](const config::DeviceEntry& entry) {
            return std::make_unique<Pmu4Device>(entry);
        });
}

}  // namespace plas::hal::driver
