#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "plas/hal/interface/pci/types.h"
#include "plas/core/result.h"
#include "plas/core/types.h"

namespace plas::hal { class Device; }  // forward declaration

namespace plas::hal::pci {

/// Pure virtual interface for PCI BAR (Base Address Register) MMIO access.
class PciBar {
public:
    virtual ~PciBar() = default;

    virtual std::string InterfaceName() const { return "PciBar"; }
    virtual plas::hal::Device* GetDevice() = 0;

    virtual core::Result<core::DWord> BarRead32(Bdf bdf, uint8_t bar_index,
                                                 uint64_t offset) = 0;
    virtual core::Result<core::QWord> BarRead64(Bdf bdf, uint8_t bar_index,
                                                 uint64_t offset) = 0;
    virtual core::Result<void> BarWrite32(Bdf bdf, uint8_t bar_index,
                                          uint64_t offset,
                                          core::DWord value) = 0;
    virtual core::Result<void> BarWrite64(Bdf bdf, uint8_t bar_index,
                                          uint64_t offset,
                                          core::QWord value) = 0;
    virtual core::Result<void> BarReadBuffer(Bdf bdf, uint8_t bar_index,
                                              uint64_t offset, void* buffer,
                                              std::size_t length) = 0;
    virtual core::Result<void> BarWriteBuffer(Bdf bdf, uint8_t bar_index,
                                               uint64_t offset,
                                               const void* buffer,
                                               std::size_t length) = 0;
};

}  // namespace plas::hal::pci
