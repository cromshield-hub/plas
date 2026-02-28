#pragma once

#include "plas/core/result.h"
#include "plas/hal/interface/pci/cxl_types.h"
#include "plas/hal/interface/pci/types.h"

namespace plas::hal::pci {

/// Pure virtual interface for CXL Mailbox command execution.
class CxlMailbox {
public:
    virtual ~CxlMailbox() = default;

    /// Execute a CXL Mailbox command with a typed opcode.
    virtual core::Result<CxlMailboxResult> ExecuteCommand(
        Bdf bdf, CxlMailboxOpcode opcode,
        const CxlMailboxPayload& payload) = 0;

    /// Execute a CXL Mailbox command with a raw opcode value.
    virtual core::Result<CxlMailboxResult> ExecuteCommand(
        Bdf bdf, uint16_t raw_opcode,
        const CxlMailboxPayload& payload) = 0;

    /// Query the maximum payload size supported by the mailbox.
    virtual core::Result<uint32_t> GetPayloadSize(Bdf bdf) = 0;

    /// Check whether the mailbox is ready to accept a new command.
    virtual core::Result<bool> IsReady(Bdf bdf) = 0;

    /// Query the status of a running background command.
    virtual core::Result<CxlMailboxResult> GetBackgroundCmdStatus(
        Bdf bdf) = 0;
};

}  // namespace plas::hal::pci
