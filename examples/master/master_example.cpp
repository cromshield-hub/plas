/// @file master_example.cpp
/// @brief End-to-end demo using Bootstrap for simplified initialization.
///
/// Shows how Bootstrap eliminates boilerplate: driver registration,
/// config parsing, device creation, init/open — all in one Init() call.
///
///  1. Bootstrap::Init() — registers drivers, loads config, opens devices
///  2. DumpDevices() — quick audit of everything that was loaded
///  3. GetDevicesByInterface<T>() — find all I2C-capable devices
///  4. GetDeviceByUri() — look up a device by its URI
///  5. DeviceFailure::detail — rich error reporting on failures
///  6. I2C read on the first Aardvark
///  7. PCI config register reads (Vendor/Device ID, Class Code, capabilities)
///  8. PCI BAR0 register reads (NVMe Controller Registers)
///  9. PCI topology walk for the NVMe device
/// 10. Bootstrap::Init() with in-memory ConfigNode (no file I/O)
/// 11. Bootstrap::Deinit() — close all, cleanup
///
/// Usage:
///   ./master_example                          -- run with default config
///   ./master_example fixtures/custom.yaml     -- run with a custom config
///
/// When built without libpci, the pciutils device is gracefully skipped.

#include <cstdio>
#include <string>

#include "plas/bootstrap/bootstrap.h"
#include "plas/config/config_node.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/i2c.h"
#include "plas/hal/interface/pci/pci_bar.h"
#include "plas/hal/interface/pci/pci_config.h"
#include "plas/hal/interface/pci/pci_topology.h"
#include "plas/hal/interface/pci/types.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

    std::printf("=== PLAS Master Example (Bootstrap + DX Features) ===\n\n");

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

    // -----------------------------------------------------------------------
    // 2. DumpDevices — one-call audit of all loaded devices
    // -----------------------------------------------------------------------
    std::printf("\n[2] DumpDevices() — config audit:\n");
    std::printf("%s", bs.DumpDevices().c_str());

    // -----------------------------------------------------------------------
    // 3. GetDevicesByInterface<I2c> — find all I2C-capable devices
    // -----------------------------------------------------------------------
    std::printf("\n[3] GetDevicesByInterface<I2c>() — interface filtering:\n");
    auto i2c_devices = bs.GetDevicesByInterface<I2c>();
    if (i2c_devices.empty()) {
        std::printf("    No I2C devices found\n");
    } else {
        std::printf("    Found %zu I2C device(s):\n", i2c_devices.size());
        for (const auto& [name, iface] : i2c_devices) {
            auto* dev = bs.GetDevice(name);
            std::printf("      %-16s  %s  (bitrate: %u Hz)\n",
                        name.c_str(),
                        dev ? dev->GetUri().c_str() : "?",
                        iface->GetBitrate());
        }
    }

    // -----------------------------------------------------------------------
    // 4. GetDeviceByUri — look up a specific device by URI
    // -----------------------------------------------------------------------
    std::printf("\n[4] GetDeviceByUri() — URI-based lookup:\n");
    const std::string lookup_uri = "aardvark://0:0x50";
    auto* found = bs.GetDeviceByUri(lookup_uri);
    if (found) {
        std::printf("    Found \"%s\" for URI %s\n",
                    found->GetName().c_str(), lookup_uri.c_str());
    } else {
        std::printf("    No device with URI %s\n", lookup_uri.c_str());
    }

    // Show ValidateUri as a static utility
    std::printf("    ValidateUri examples:\n");
    const char* test_uris[] = {
        "aardvark://0:0x50",
        "pciutils://0000:03:00.0",
        "bad_uri_no_scheme",
        "aardvark://missing_colon",
    };
    for (const auto* uri : test_uris) {
        std::printf("      %-30s -> %s\n",
                    uri, Bootstrap::ValidateUri(uri) ? "valid" : "INVALID");
    }

    // -----------------------------------------------------------------------
    // 5. DeviceFailure::detail — rich error context
    // -----------------------------------------------------------------------
    std::printf("\n[5] DeviceFailure detail — enhanced error reporting:\n");
    const auto& failures = bs.GetFailures();
    if (failures.empty()) {
        std::printf("    No failures (all devices loaded successfully)\n");
    } else {
        for (const auto& f : failures) {
            std::printf("    [%s] %s (%s): %s\n",
                        f.phase.c_str(), f.nickname.c_str(), f.driver.c_str(),
                        f.error.message().c_str());
            if (!f.detail.empty()) {
                std::printf("      detail: %s\n", f.detail.c_str());
            }
        }
    }

    // -----------------------------------------------------------------------
    // 6. I2C read attempt on the first Aardvark
    // -----------------------------------------------------------------------
    std::printf("\n[6] I2C read test (eeprom_reader):\n");
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
    // 7. PCI config register reads on the NVMe device
    // -----------------------------------------------------------------------
    std::printf("\n[7] PCI config read (nvme0):\n");
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
    // 8. PCI BAR0 register reads (NVMe Controller Registers)
    // -----------------------------------------------------------------------
    std::printf("\n[8] PCI BAR0 read (nvme0):\n");
    auto* pcibar = bs.GetInterface<PciBar>("nvme0");
    if (!pcibar) {
        std::printf("    Skipped -- PciBar interface not available\n");
    } else {
        auto* nvme_bar = bs.GetDevice("nvme0");
        auto bar_addr = PciAddress::FromString(
            nvme_bar->GetUri().substr(12));
        if (bar_addr.IsError()) {
            std::printf("    Cannot parse BDF from URI\n");
        } else {
            Bdf bar_bdf = bar_addr.Value().bdf;

            // NVMe CAP register (BAR0, offset 0x00, 64-bit)
            auto cap = pcibar->BarRead64(bar_bdf, 0, 0x00);
            if (cap.IsOk())
                std::printf("    CAP:  0x%016lX\n",
                            static_cast<unsigned long>(cap.Value()));
            else
                std::printf("    CAP:  read failed (%s)\n",
                            cap.Error().message().c_str());

            // NVMe VS register (BAR0, offset 0x08, 32-bit)
            auto vs = pcibar->BarRead32(bar_bdf, 0, 0x08);
            if (vs.IsOk()) {
                uint32_t v = vs.Value();
                std::printf("    VS:   %u.%u.%u\n",
                            (v >> 16) & 0xFFFF, (v >> 8) & 0xFF, v & 0xFF);
            }

            // NVMe CSTS register (BAR0, offset 0x1C, 32-bit)
            auto csts = pcibar->BarRead32(bar_bdf, 0, 0x1C);
            if (csts.IsOk())
                std::printf("    CSTS: 0x%08X (RDY=%u)\n",
                            csts.Value(), csts.Value() & 0x1);
        }
    }

    // -----------------------------------------------------------------------
    // 9. PCI topology walk for the NVMe device
    // -----------------------------------------------------------------------
    std::printf("\n[9] PCI topology walk (nvme0):\n");
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
    // 10. Bootstrap::Init with in-memory ConfigNode (no file I/O)
    // -----------------------------------------------------------------------
    std::printf("\n[10] In-memory ConfigNode init:\n");
    {
        // Tear down current bootstrap first
        bs.Deinit();

        // Load file into ConfigNode, then pass the subtree directly
        auto node = plas::config::ConfigNode::LoadFromFile(config_path);
        if (node.IsError()) {
            std::printf("    Failed to load config: %s\n",
                        node.Error().message().c_str());
        } else {
            auto subtree = node.Value().GetSubtree("plas.devices");
            if (subtree.IsError()) {
                std::printf("    Failed to get subtree: %s\n",
                            subtree.Error().message().c_str());
            } else {
                BootstrapConfig mem_cfg;
                mem_cfg.device_config_node = subtree.Value();
                mem_cfg.skip_unknown_drivers = true;
                mem_cfg.skip_device_failures = true;

                auto mem_result = bs.Init(mem_cfg);
                if (mem_result.IsOk()) {
                    std::printf("    Success: %zu devices via ConfigNode "
                                "(no temp files!)\n",
                                mem_result.Value().devices_opened);
                } else {
                    std::printf("    Init failed: %s\n",
                                mem_result.Error().message().c_str());
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // 11. Cleanup — let Bootstrap handle it
    // -----------------------------------------------------------------------
    std::printf("\n[11] Shutting down...\n");
    bs.Deinit();

    std::printf("\n=== Done ===\n");
    return 0;
}
