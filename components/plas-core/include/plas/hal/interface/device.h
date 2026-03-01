#pragma once

#include <string>

#include "plas/core/result.h"

namespace plas::hal {

enum class DeviceState {
    kUninitialized,
    kInitialized,
    kOpen,
    kClosed,
    kError
};

class Device {
public:
    virtual ~Device() = default;

    virtual core::Result<void> Init() = 0;
    virtual core::Result<void> Open() = 0;
    virtual core::Result<void> Close() = 0;
    virtual core::Result<void> Reset() = 0;

    virtual DeviceState GetState() const = 0;
    virtual std::string GetName() const = 0;
    virtual std::string GetUri() const = 0;

    /// Alias for GetName() — returns the user-assigned nickname.
    std::string GetNickname() const { return GetName(); }

    /// Returns the driver type name (e.g. "aardvark", "pciutils").
    virtual std::string GetDriverName() const = 0;
};

}  // namespace plas::hal
