#pragma once

#include <optional>

#include "plas/hal/interface/pci/types.h"
#include "plas/core/result.h"
#include "plas/core/types.h"

namespace plas::hal::pci {

/// Pure virtual interface for PCI configuration space access.
class PciConfig {
public:
    virtual ~PciConfig() = default;

    // Typed reads
    virtual core::Result<core::Byte> ReadConfig8(Bdf bdf,
                                                  ConfigOffset offset) = 0;
    virtual core::Result<core::Word> ReadConfig16(Bdf bdf,
                                                   ConfigOffset offset) = 0;
    virtual core::Result<core::DWord> ReadConfig32(Bdf bdf,
                                                    ConfigOffset offset) = 0;

    // Typed writes
    virtual core::Result<void> WriteConfig8(Bdf bdf, ConfigOffset offset,
                                            core::Byte value) = 0;
    virtual core::Result<void> WriteConfig16(Bdf bdf, ConfigOffset offset,
                                             core::Word value) = 0;
    virtual core::Result<void> WriteConfig32(Bdf bdf, ConfigOffset offset,
                                             core::DWord value) = 0;

    // Capability walking
    virtual core::Result<std::optional<ConfigOffset>> FindCapability(
        Bdf bdf, CapabilityId id) = 0;
    virtual core::Result<std::optional<ConfigOffset>> FindExtCapability(
        Bdf bdf, ExtCapabilityId id) = 0;
};

}  // namespace plas::hal::pci
