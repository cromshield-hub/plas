#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "plas/core/result.h"
#include "plas/core/types.h"
#include "plas/core/units.h"

namespace plas::hal {

class Device;  // forward declaration

class I3c {
public:
    virtual ~I3c() = default;

    virtual std::string InterfaceName() const { return "I3c"; }
    virtual Device* GetDevice() = 0;

    virtual core::Result<size_t> Read(core::Address addr, core::Byte* data,
                                      size_t length, bool stop = true) = 0;
    virtual core::Result<size_t> Write(core::Address addr,
                                       const core::Byte* data, size_t length,
                                       bool stop = true) = 0;

    // Broadcast CCC: sent to all devices on the bus, no response expected.
    virtual core::Result<void> SendBroadcastCcc(uint8_t ccc_id,
                                                const core::Byte* data,
                                                size_t length) = 0;

    // Direct CCC SET: sent to a specific device, no response expected.
    virtual core::Result<void> SendDirectCcc(uint8_t ccc_id, core::Address addr,
                                             const core::Byte* data,
                                             size_t length) = 0;

    // Direct CCC GET: receive data from a specific device.
    virtual core::Result<size_t> RecvDirectCcc(uint8_t ccc_id,
                                               core::Address addr,
                                               core::Byte* data,
                                               size_t length) = 0;

    virtual core::Result<void> SetFrequency(core::Frequency freq) = 0;
};

}  // namespace plas::hal
