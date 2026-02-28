#pragma once

#include <vector>

#include "plas/backend/interface/pci/types.h"
#include "plas/core/result.h"

namespace plas::backend::pci {

/// Pure virtual interface for PCI DOE (Data Object Exchange) protocol.
class PciDoe {
public:
    virtual ~PciDoe() = default;

    /// Enumerate supported DOE protocols at the given DOE capability offset.
    virtual core::Result<std::vector<DoeProtocolId>> DoeDiscover(
        Bdf bdf, ConfigOffset doe_offset) = 0;

    /// Perform a DOE request/response exchange. Mailbox handshake is internal.
    virtual core::Result<DoePayload> DoeExchange(
        Bdf bdf, ConfigOffset doe_offset, DoeProtocolId protocol,
        const DoePayload& request) = 0;
};

}  // namespace plas::backend::pci
