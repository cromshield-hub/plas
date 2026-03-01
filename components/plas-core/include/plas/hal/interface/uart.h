#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "plas/core/result.h"
#include "plas/core/types.h"

namespace plas::hal {

class Device;  // forward declaration

enum class Parity {
    kNone,
    kOdd,
    kEven
};

class Uart {
public:
    virtual ~Uart() = default;

    virtual std::string InterfaceName() const { return "Uart"; }
    virtual Device* GetDevice() = 0;

    virtual core::Result<size_t> Read(core::Byte* data, size_t length) = 0;
    virtual core::Result<size_t> Write(const core::Byte* data,
                                       size_t length) = 0;
    virtual core::Result<void> SetBaudRate(uint32_t baud_rate) = 0;
    virtual uint32_t GetBaudRate() const = 0;
    virtual core::Result<void> SetParity(Parity parity) = 0;
};

}  // namespace plas::hal
