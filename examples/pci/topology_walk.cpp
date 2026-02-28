/// @file topology_walk.cpp
/// @brief Example: PCI Topology Traversal via sysfs.
///
/// Demonstrates how to:
///  1. Parse a PCI address string (DDDD:BB:DD.F)
///  2. Query device info from sysfs
///  3. Walk up the topology to find parent bridges and root port
///  4. Display the full path from endpoint to root port
///
/// Usage: ./topology_walk <DDDD:BB:DD.F>
/// Example: ./topology_walk 0000:41:00.0
///
/// Requires Linux sysfs (runs on real hardware only).

#include <cstdio>
#include <cstdlib>
#include <string>

#include "plas/hal/interface/pci/pci_topology.h"

static const char* PortTypeName(plas::hal::pci::PciePortType type) {
    switch (type) {
        case plas::hal::pci::PciePortType::kEndpoint:
            return "Endpoint";
        case plas::hal::pci::PciePortType::kLegacyEndpoint:
            return "Legacy Endpoint";
        case plas::hal::pci::PciePortType::kRootPort:
            return "Root Port";
        case plas::hal::pci::PciePortType::kUpstreamPort:
            return "Upstream Port";
        case plas::hal::pci::PciePortType::kDownstreamPort:
            return "Downstream Port";
        case plas::hal::pci::PciePortType::kPcieToPciBridge:
            return "PCIe-to-PCI Bridge";
        case plas::hal::pci::PciePortType::kPciToPcieBridge:
            return "PCI-to-PCIe Bridge";
        case plas::hal::pci::PciePortType::kRcIntegratedEndpoint:
            return "RC Integrated Endpoint";
        case plas::hal::pci::PciePortType::kRcEventCollector:
            return "RC Event Collector";
        case plas::hal::pci::PciePortType::kUnknown:
        default:
            return "Unknown";
    }
}

int main(int argc, char* argv[]) {
    using namespace plas::hal::pci;

    if (argc < 2) {
        std::printf("Usage: %s <DDDD:BB:DD.F>\n", argv[0]);
        std::printf("Example: %s 0000:41:00.0\n", argv[0]);
        return 1;
    }

    // --- Parse PCI address ---
    auto addr_result = PciAddress::FromString(argv[1]);
    if (addr_result.IsError()) {
        std::printf("Error: invalid PCI address '%s' "
                    "(expected DDDD:BB:DD.F format)\n",
                    argv[1]);
        return 1;
    }
    PciAddress addr = addr_result.Value();
    std::printf("[*] Target device: %s\n", addr.ToString().c_str());

    // --- Check device exists ---
    if (!PciTopology::DeviceExists(addr)) {
        std::printf("Error: device %s not found in sysfs\n",
                    addr.ToString().c_str());
        return 1;
    }

    // --- Get device info ---
    auto info_result = PciTopology::GetDeviceInfo(addr);
    if (info_result.IsError()) {
        std::printf("Error: GetDeviceInfo failed: %s\n",
                    info_result.Error().message().c_str());
        return 1;
    }

    const auto& info = info_result.Value();
    std::printf("[*] Device info:\n");
    std::printf("    sysfs path: %s\n", info.sysfs_path.c_str());
    std::printf("    port type:  %s (0x%02X)\n", PortTypeName(info.port_type),
                static_cast<int>(static_cast<uint8_t>(info.port_type)));
    std::printf("    is bridge:  %s\n", info.is_bridge ? "yes" : "no");

    // --- Find immediate parent ---
    auto parent_result = PciTopology::FindParent(addr);
    if (parent_result.IsOk()) {
        if (parent_result.Value().has_value()) {
            std::printf("[*] Parent: %s\n",
                        parent_result.Value()->ToString().c_str());
        } else {
            std::printf("[*] No parent (directly under root complex)\n");
        }
    }

    // --- Find root port ---
    auto root_result = PciTopology::FindRootPort(addr);
    if (root_result.IsOk()) {
        std::printf("[*] Root port: %s\n",
                    root_result.Value().ToString().c_str());
    } else {
        std::printf("[*] Root port not found\n");
    }

    // --- Walk full path to root ---
    auto path_result = PciTopology::GetPathToRoot(addr);
    if (path_result.IsError()) {
        std::printf("Error: GetPathToRoot failed: %s\n",
                    path_result.Error().message().c_str());
        return 1;
    }

    const auto& path = path_result.Value();
    std::printf("\n[*] Topology path (%zu devices, endpoint → root):\n",
                path.size());

    for (size_t i = 0; i < path.size(); ++i) {
        const auto& node = path[i];
        const char* indent = (i == 0) ? "  → " : "    ";
        std::printf("%s[%zu] %s  %-20s  bridge=%s\n", indent, i,
                    node.address.ToString().c_str(),
                    PortTypeName(node.port_type),
                    node.is_bridge ? "yes" : "no");
    }

    std::printf("\n[*] Done.\n");
    return 0;
}
