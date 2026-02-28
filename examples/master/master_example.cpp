/// @file master_example.cpp
/// @brief End-to-end demo using Bootstrap for simplified initialization.
///
/// Shows how Bootstrap eliminates boilerplate: driver registration,
/// config parsing, device creation, init/open — all in one Init() call.
///
///  1. Bootstrap::Init() — registers drivers, loads config, opens devices
///  2. List all loaded devices and check interface support
///  3. I2C read on the first Aardvark
///  4. PCI config register reads (Vendor/Device ID, Class Code, capabilities)
///  5. PCI topology walk for the NVMe device
///  6. Bootstrap::Deinit() — close all, cleanup
///
/// Usage:
///   ./master_example                          -- run with default config
///   ./master_example fixtures/custom.yaml     -- run with a custom config
///
/// When built without libpci, the pciutils device is gracefully skipped.

#include <cstdio>
#include <string>

#include "plas/bootstrap/bootstrap.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/i2c.h"
#include "plas/hal/interface/pci/pci_config.h"
#include "plas/hal/interface/pci/pci_topology.h"
#include "plas/hal/interface/pci/types.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* StateName(plas::hal::DeviceState s) {
    switch (s) {
        case plas::hal::DeviceState::kUninitialized: return "Uninitialized";
        case plas::hal::DeviceState::kInitialized:   return "Initialized";
        case plas::hal::DeviceState::kOpen:           return "Open";
        case plas::hal::DeviceState::kClosed:         return "Closed";
        case plas::hal::DeviceState::kError:          return "Error";
    }
    return "?";
}

static const char* PortTypeName(plas::hal::pci::PciePortType t) {
    switch (t) {
        case plas::hal::pci::PciePortType::kEndpoint:            return "Endpoint";
        case plas::hal::pci::PciePortType::kLegacyEndpoint:      return "Legacy Endpoint";
        case plas::hal::pci::PciePortType::kRootPort:            return "Root Port";
        case plas::hal::pci::PciePortType::kUpstreamPort:        return "Upstream Port";
        case plas::hal::pci::PciePortType::kDownstreamPort:      return "Downstream Port";
        case plas::hal::pci::PciePortType::kPcieToPciBridge:     return "PCIe-to-PCI Bridge";
        case plas::hal::pci::PciePortType::kPciToPcieBridge:     return "PCI-to-PCIe Bridge";
        case plas::hal::pci::PciePortType::kRcIntegratedEndpoint: return "RC Integrated EP";
        case plas::hal::pci::PciePortType::kRcEventCollector:    return "RC Event Collector";
        case plas::hal::pci::PciePortType::kUnknown:             return "Unknown";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    using namespace plas::hal;
    using namespace plas::hal::pci;
    using namespace plas::bootstrap;

    const std::string config_path =
        (argc > 1) ? argv[1] : "fixtures/master_config.yaml";

    std::printf("=== PLAS Master Example (Bootstrap) ===\n\n");

    // -----------------------------------------------------------------------
    // 1. Bootstrap::Init — one call does everything
    // -----------------------------------------------------------------------
    BootstrapConfig cfg;
    cfg.device_config_path = config_path;
    cfg.device_config_key_path = "plas.devices";
    cfg.skip_unknown_drivers = true;
    cfg.skip_device_failures = true;

    Bootstrap bs;
    auto init_result = bs.Init(cfg);
    if (init_result.IsError()) {
        std::printf("Bootstrap::Init FAILED: %s\n",
                    init_result.Error().message().c_str());
        return 1;
    }

    const auto& br = init_result.Value();
    std::printf("[1] Bootstrap complete: %zu opened, %zu failed, %zu skipped\n",
                br.devices_opened, br.devices_failed, br.devices_skipped);
    for (const auto& f : br.failures) {
        std::printf("    SKIP: %s (%s) — %s at %s\n",
                    f.nickname.c_str(), f.driver.c_str(),
                    f.error.message().c_str(), f.phase.c_str());
    }

    // -----------------------------------------------------------------------
    // 2. List devices + interface support
    // -----------------------------------------------------------------------
    std::printf("\n[2] Devices and interface support:\n");
    std::printf("    %-16s %-30s %-6s %-8s %-6s\n",
                "Nickname", "URI", "State", "I2C", "PciCfg");
    std::printf("    %-16s %-30s %-6s %-8s %-6s\n",
                "--------", "---", "-----", "---", "------");

    for (const auto& name : bs.DeviceNames()) {
        auto* dev = bs.GetDevice(name);
        if (!dev) continue;
        bool has_i2c    = (bs.GetInterface<I2c>(name) != nullptr);
        bool has_pcicfg = (bs.GetInterface<PciConfig>(name) != nullptr);
        std::printf("    %-16s %-30s %-6s %-8s %-6s\n",
                    name.c_str(), dev->GetUri().c_str(),
                    StateName(dev->GetState()),
                    has_i2c ? "yes" : "-",
                    has_pcicfg ? "yes" : "-");
    }

    // -----------------------------------------------------------------------
    // 3. I2C read attempt on the first Aardvark
    // -----------------------------------------------------------------------
    std::printf("\n[3] I2C read test (eeprom_reader):\n");
    auto* i2c = bs.GetInterface<I2c>("eeprom_reader");
    if (!i2c) {
        std::printf("    Skipped -- interface not available\n");
    } else {
        std::vector<uint8_t> buf(16, 0);
        auto rd = i2c->Read(0x50, buf.data(), buf.size());
        if (rd.IsError()) {
            std::printf("    Read FAILED: %s\n", rd.Error().message().c_str());
        } else {
            std::printf("    Read %zu bytes:", rd.Value());
            for (size_t i = 0; i < rd.Value() && i < buf.size(); ++i) {
                std::printf(" %02X", buf[i]);
            }
            std::printf("\n");
        }
    }

    // -----------------------------------------------------------------------
    // 4. PCI config register reads on the NVMe device
    // -----------------------------------------------------------------------
    std::printf("\n[4] PCI config read (nvme0):\n");
    auto* pcicfg = bs.GetInterface<PciConfig>("nvme0");
    if (!pcicfg) {
        std::printf("    Skipped -- PciConfig interface not available\n");
    } else {
        auto* nvme_dev = bs.GetDevice("nvme0");
        auto addr = PciAddress::FromString(
            nvme_dev->GetUri().substr(12));  // strip "pciutils://"
        if (addr.IsError()) {
            std::printf("    Cannot parse BDF from URI\n");
        } else {
            Bdf bdf = addr.Value().bdf;

            auto vid = pcicfg->ReadConfig16(bdf, 0x00);
            if (vid.IsOk())
                std::printf("    Vendor ID:  0x%04X\n", vid.Value());

            auto did = pcicfg->ReadConfig16(bdf, 0x02);
            if (did.IsOk())
                std::printf("    Device ID:  0x%04X\n", did.Value());

            auto class_rev = pcicfg->ReadConfig32(bdf, 0x08);
            if (class_rev.IsOk()) {
                uint32_t val = class_rev.Value();
                uint8_t rev = static_cast<uint8_t>(val & 0xFF);
                uint32_t cc = (val >> 8) & 0xFFFFFF;
                std::printf("    Class Code: 0x%06X (rev %u)\n", cc, rev);
            }

            auto pcie_cap = pcicfg->FindCapability(bdf,
                                                    CapabilityId::kPciExpress);
            if (pcie_cap.IsOk() && pcie_cap.Value().has_value()) {
                ConfigOffset cap_off = pcie_cap.Value().value();
                std::printf("    PCIe Cap:   found at 0x%02X\n", cap_off);

                auto link_stat = pcicfg->ReadConfig16(bdf, cap_off + 0x12);
                if (link_stat.IsOk()) {
                    uint16_t ls = link_stat.Value();
                    uint8_t speed = ls & 0x0F;
                    uint8_t width = (ls >> 4) & 0x3F;
                    std::printf("    Link:       Gen%u x%u\n", speed, width);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // 5. PCI topology walk for the NVMe device
    // -----------------------------------------------------------------------
    std::printf("\n[5] PCI topology walk (nvme0):\n");
    {
        auto* nvme_dev = bs.GetDevice("nvme0");
        if (!nvme_dev) {
            std::printf("    Skipped -- nvme0 device not loaded\n");
        } else {
            auto addr = PciAddress::FromString(
                nvme_dev->GetUri().substr(12));
            if (addr.IsError()) {
                std::printf("    Cannot parse address from URI\n");
            } else if (PciTopology::DeviceExists(addr.Value())) {
                auto path = PciTopology::GetPathToRoot(addr.Value());
                if (path.IsOk()) {
                    std::printf("    Path to root (%zu hops):\n",
                                path.Value().size());
                    for (size_t i = 0; i < path.Value().size(); ++i) {
                        const auto& node = path.Value()[i];
                        std::printf("      [%zu] %s  %s%s\n",
                                    i, node.address.ToString().c_str(),
                                    PortTypeName(node.port_type),
                                    node.is_bridge ? " (bridge)" : "");
                    }
                }
            } else {
                std::printf("    Device does not exist in sysfs "
                            "(expected on non-PCI systems)\n");
            }
        }
    }

    // -----------------------------------------------------------------------
    // 6. Cleanup — let Bootstrap handle it
    // -----------------------------------------------------------------------
    std::printf("\n[6] Shutting down...\n");
    bs.Deinit();

    std::printf("\n=== Done ===\n");
    return 0;
}
