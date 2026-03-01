#pragma once

#include <optional>
#include <string>
#include <vector>

#include "plas/core/result.h"
#include "plas/hal/interface/pci/types.h"

namespace plas::hal::pci {

/// Information about a PCI device in the topology.
struct PciDeviceNode {
    PciAddress address;
    PciePortType port_type;
    bool is_bridge;          ///< Header Type 1
    std::string sysfs_path;  ///< e.g., /sys/bus/pci/devices/0000:03:00.0
};

/// sysfs-based PCI topology traversal and device management.
///
/// All methods are static. The sysfs root can be overridden for testing.
class PciTopology {
public:
    /// Get the sysfs path for a PCI device.
    static std::string GetSysfsPath(const PciAddress& addr);

    /// Check if a device exists in sysfs.
    static bool DeviceExists(const PciAddress& addr);

    /// Get device node info (port type, bridge flag, sysfs path).
    static core::Result<PciDeviceNode> GetDeviceInfo(const PciAddress& addr);

    /// Find the immediate parent (upstream bridge/port).
    /// Returns std::nullopt if the device is a root complex host bridge.
    static core::Result<std::optional<PciAddress>> FindParent(
        const PciAddress& addr);

    /// Enumerate direct child devices under a bridge or root port.
    /// Returns empty vector if the device has no children (endpoint).
    /// Returns kNotFound if the device does not exist in sysfs.
    static core::Result<std::vector<PciAddress>> FindChildren(
        const PciAddress& bridge_addr);

    /// Find the root port by traversing up the topology.
    static core::Result<PciAddress> FindRootPort(const PciAddress& addr);

    /// Get full path from device to root port (device first, root last).
    static core::Result<std::vector<PciDeviceNode>> GetPathToRoot(
        const PciAddress& addr);

    /// Remove a device via sysfs (writes "1" to remove file).
    static core::Result<void> RemoveDevice(const PciAddress& addr);

    /// Rescan from a specific bridge's secondary bus.
    static core::Result<void> RescanBridge(const PciAddress& bridge_addr);

    /// Global PCI bus rescan.
    static core::Result<void> RescanAll();

    /// Override sysfs root for unit testing (default: "/sys").
    static void SetSysfsRoot(const std::string& root);

private:
    static std::string sysfs_root_;

    static core::Result<std::string> ReadSysfsFile(const std::string& path);
    static core::Result<void> WriteSysfsFile(const std::string& path,
                                              const std::string& value);
    static PciePortType ReadPortType(const std::string& sysfs_device_path);
    static bool ReadIsBridge(const std::string& sysfs_device_path);
    static core::Result<std::vector<PciAddress>> ParseTopologyPath(
        const std::string& real_path);
};

}  // namespace plas::hal::pci
