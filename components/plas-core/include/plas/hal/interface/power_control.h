#pragma once

#include <string>

#include "plas/core/result.h"
#include "plas/core/units.h"

namespace plas::hal {

class Device;  // forward declaration

class PowerControl {
public:
    virtual ~PowerControl() = default;

    virtual std::string InterfaceName() const { return "PowerControl"; }
    virtual Device* GetDevice() = 0;

    virtual core::Result<void> SetVoltage(core::Voltage voltage) = 0;
    virtual core::Result<core::Voltage> GetVoltage() = 0;
    virtual core::Result<void> SetCurrent(core::Current current) = 0;
    virtual core::Result<core::Current> GetCurrent() = 0;
    virtual core::Result<void> PowerOn() = 0;
    virtual core::Result<void> PowerOff() = 0;
    virtual core::Result<bool> IsPowerOn() = 0;
};

}  // namespace plas::hal
