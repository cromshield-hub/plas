#pragma once

#include <cstddef>
#include <cstdint>

#include "plas/core/result.h"
#include "plas/core/types.h"

namespace plas::hal {

class I2c {
public:
    virtual ~I2c() = default;

    virtual core::Result<size_t> Read(core::Address addr, core::Byte* data,
                                      size_t length) = 0;
    virtual core::Result<size_t> Write(core::Address addr,
                                       const core::Byte* data,
                                       size_t length) = 0;
    virtual core::Result<size_t> WriteRead(core::Address addr,
                                           const core::Byte* write_data,
                                           size_t write_len,
                                           core::Byte* read_data,
                                           size_t read_len) = 0;
    virtual core::Result<void> SetBitrate(uint32_t bitrate) = 0;
    virtual uint32_t GetBitrate() const = 0;
};

}  // namespace plas::hal
