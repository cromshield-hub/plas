#pragma once

#include <cstddef>
#include <cstdint>

#include "plas/core/result.h"
#include "plas/core/types.h"
#include "plas/core/units.h"

namespace plas::backend {

class I3c {
public:
    virtual ~I3c() = default;

    virtual core::Result<size_t> Read(core::Address addr, core::Byte* data,
                                      size_t length) = 0;
    virtual core::Result<size_t> Write(core::Address addr,
                                       const core::Byte* data,
                                       size_t length) = 0;
    virtual core::Result<void> SendCcc(uint8_t ccc_id, const core::Byte* data,
                                       size_t length) = 0;
    virtual core::Result<void> SetFrequency(core::Frequency freq) = 0;
};

}  // namespace plas::backend
