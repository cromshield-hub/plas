#pragma once

#include <optional>
#include <string>
#include <vector>

#include "plas/core/result.h"
#include "plas/core/types.h"
#include "plas/hal/interface/pci/cxl_types.h"
#include "plas/hal/interface/pci/types.h"

namespace plas::hal { class Device; }  // forward declaration

namespace plas::hal::pci {

/// Pure virtual interface for CXL DVSEC (Designated Vendor-Specific Extended
/// Capability) operations.
class Cxl {
public:
    virtual ~Cxl() = default;

    virtual std::string InterfaceName() const { return "Cxl"; }
    virtual plas::hal::Device* GetDevice() = 0;

    /// Enumerate all CXL DVSECs for the given device.
    virtual core::Result<std::vector<DvsecHeader>> EnumerateCxlDvsecs(
        Bdf bdf) = 0;

    /// Find a specific CXL DVSEC by ID.
    virtual core::Result<std::optional<DvsecHeader>> FindCxlDvsec(
        Bdf bdf, CxlDvsecId dvsec_id) = 0;

    /// Determine the CXL device type from the CXL Device DVSEC.
    virtual core::Result<CxlDeviceType> GetCxlDeviceType(Bdf bdf) = 0;

    /// Read register block entries from the Register Locator DVSEC.
    virtual core::Result<std::vector<RegisterBlockEntry>> GetRegisterBlocks(
        Bdf bdf) = 0;

    /// Read a DWord from a DVSEC register at a given base offset + register
    /// offset.
    virtual core::Result<core::DWord> ReadDvsecRegister(
        Bdf bdf, ConfigOffset dvsec_offset, uint16_t reg_offset) = 0;

    /// Write a DWord to a DVSEC register at a given base offset + register
    /// offset.
    virtual core::Result<void> WriteDvsecRegister(
        Bdf bdf, ConfigOffset dvsec_offset, uint16_t reg_offset,
        core::DWord value) = 0;
};

}  // namespace plas::hal::pci
