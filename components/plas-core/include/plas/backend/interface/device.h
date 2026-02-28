#pragma once

#include <string>

#include "plas/core/result.h"

namespace plas::backend {

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
};

}  // namespace plas::backend
