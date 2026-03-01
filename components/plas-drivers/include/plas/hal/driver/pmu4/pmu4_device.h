#pragma once

#include <string>

#include "plas/hal/interface/device.h"
#include "plas/hal/interface/power_control.h"
#include "plas/hal/interface/ssd_gpio.h"
#include "plas/config/device_entry.h"

namespace plas::hal::driver {

class Pmu4Device : public Device, public PowerControl, public SsdGpio {
public:
    explicit Pmu4Device(const config::DeviceEntry& entry);
    ~Pmu4Device() override = default;

    // Device interface
    core::Result<void> Init() override;
    core::Result<void> Open() override;
    core::Result<void> Close() override;
    core::Result<void> Reset() override;
    DeviceState GetState() const override;
    std::string GetName() const override;
    std::string GetUri() const override;
    std::string GetDriverName() const override;

    // PowerControl/SsdGpio interfaces — GetDevice() (one impl satisfies both)
    Device* GetDevice() override;

    // PowerControl interface
    core::Result<void> SetVoltage(core::Voltage voltage) override;
    core::Result<core::Voltage> GetVoltage() override;
    core::Result<void> SetCurrent(core::Current current) override;
    core::Result<core::Current> GetCurrent() override;
    core::Result<void> PowerOn() override;
    core::Result<void> PowerOff() override;
    core::Result<bool> IsPowerOn() override;

    // SsdGpio interface
    core::Result<void> SetPerst(bool active) override;
    core::Result<bool> GetPerst() override;
    core::Result<void> SetClkReq(bool active) override;
    core::Result<bool> GetClkReq() override;
    core::Result<void> SetDualPort(bool enable) override;
    core::Result<bool> GetDualPort() override;

    /// Register this driver with the DeviceFactory.
    static void Register();

private:
    std::string name_;
    std::string uri_;
    DeviceState state_;
    int handle_;
};

}  // namespace plas::hal::driver
