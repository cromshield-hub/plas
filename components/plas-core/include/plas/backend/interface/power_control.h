#pragma once

#include "plas/core/result.h"
#include "plas/core/units.h"

namespace plas::backend {

class PowerControl {
public:
    virtual ~PowerControl() = default;

    virtual core::Result<void> SetVoltage(core::Voltage voltage) = 0;
    virtual core::Result<core::Voltage> GetVoltage() = 0;
    virtual core::Result<void> SetCurrent(core::Current current) = 0;
    virtual core::Result<core::Current> GetCurrent() = 0;
    virtual core::Result<void> PowerOn() = 0;
    virtual core::Result<void> PowerOff() = 0;
    virtual core::Result<bool> IsPowerOn() = 0;
};

}  // namespace plas::backend
