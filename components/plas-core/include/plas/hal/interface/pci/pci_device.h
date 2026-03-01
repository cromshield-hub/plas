#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "plas/core/result.h"
#include "plas/core/types.h"
#include "plas/hal/interface/pci/pci_topology.h"
#include "plas/hal/interface/pci/types.h"

namespace plas::hal::pci {

/// sysfs-based PCI device facade providing unified config space, BAR MMIO,
/// and topology access through a single object.
///
/// Unlike the PciConfig/PciBar ABCs that require a separate Device + Bootstrap
/// and redundant Bdf parameters, PciDevice owns its address and resources,
/// providing a zero-boilerplate API for PCI access.
///
/// Usage:
///   auto dev = PciDevice::Open("0000:03:00.0");
///   auto val = dev.Value().ReadConfig32(0x00);   // vendor/device ID
///   auto bar = dev.Value().BarRead32(0, 0x14);   // BAR0 MMIO
///   auto parent = dev.Value().FindParent();       // returns PciDevice
class PciDevice {
public:
    /// Open a device by PciAddress. Fails if device does not exist in sysfs.
    static core::Result<PciDevice> Open(const PciAddress& addr);

    /// Open a device by address string ("DDDD:BB:DD.F").
    static core::Result<PciDevice> Open(const std::string& addr_str);

    ~PciDevice();
    PciDevice(PciDevice&& other) noexcept;
    PciDevice& operator=(PciDevice&& other) noexcept;

    PciDevice(const PciDevice&) = delete;
    PciDevice& operator=(const PciDevice&) = delete;

    // --- Device Info (cached from Open) ---
    const PciAddress& Address() const;
    std::string AddressString() const;
    PciePortType PortType() const;
    bool IsBridge() const;
    const std::string& SysfsPath() const;

    // --- Config Space (sysfs /config pread/pwrite) ---
    core::Result<core::Byte> ReadConfig8(ConfigOffset offset);
    core::Result<core::Word> ReadConfig16(ConfigOffset offset);
    core::Result<core::DWord> ReadConfig32(ConfigOffset offset);
    core::Result<void> WriteConfig8(ConfigOffset offset, core::Byte value);
    core::Result<void> WriteConfig16(ConfigOffset offset, core::Word value);
    core::Result<void> WriteConfig32(ConfigOffset offset, core::DWord value);
    core::Result<std::optional<ConfigOffset>> FindCapability(CapabilityId id);
    core::Result<std::optional<ConfigOffset>> FindExtCapability(
        ExtCapabilityId id);

    // --- BAR MMIO (sysfs resourceN mmap) ---
    core::Result<core::DWord> BarRead32(uint8_t bar_index, uint64_t offset);
    core::Result<core::QWord> BarRead64(uint8_t bar_index, uint64_t offset);
    core::Result<void> BarWrite32(uint8_t bar_index, uint64_t offset,
                                  core::DWord value);
    core::Result<void> BarWrite64(uint8_t bar_index, uint64_t offset,
                                  core::QWord value);
    core::Result<void> BarReadBuffer(uint8_t bar_index, uint64_t offset,
                                     void* buffer, std::size_t length);
    core::Result<void> BarWriteBuffer(uint8_t bar_index, uint64_t offset,
                                      const void* buffer, std::size_t length);

    // --- Topology (delegates to PciTopology, returns PciDevice) ---
    core::Result<std::optional<PciDevice>> FindParent();
    core::Result<std::vector<PciDevice>> FindChildren();
    core::Result<PciDevice> FindRootPort();
    core::Result<std::vector<PciDeviceNode>> GetPathToRoot();

    // --- Lifecycle (sysfs) ---
    core::Result<void> Remove();
    core::Result<void> Rescan();

private:
    explicit PciDevice(PciDeviceNode&& info);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace plas::hal::pci
