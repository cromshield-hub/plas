#pragma once

#include <string>

#include "plas/core/result.h"

namespace plas::hal {

class Device;  // forward declaration

class SsdGpio {
public:
    virtual ~SsdGpio() = default;

    virtual std::string InterfaceName() const { return "SsdGpio"; }
    virtual Device* GetDevice() = 0;

    virtual core::Result<void> SetPerst(bool active) = 0;
    virtual core::Result<bool> GetPerst() = 0;

    virtual core::Result<void> SetClkReq(bool active) = 0;
    virtual core::Result<bool> GetClkReq() = 0;

    virtual core::Result<void> SetDualPort(bool enable) = 0;
    virtual core::Result<bool> GetDualPort() = 0;
};

}  // namespace plas::hal
