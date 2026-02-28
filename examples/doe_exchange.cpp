/// @file doe_exchange.cpp
/// @brief Example: DOE Discovery and Data Exchange over PCI.
///
/// Demonstrates how to:
///  1. Create a device that implements PciConfig + PciDoe
///  2. Locate the DOE Extended Capability via config space walking
///  3. Discover supported DOE protocols
///  4. Perform a DOE request/response exchange
///
/// This example uses a stub device. Replace with a real driver when available.

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "plas/backend/interface/device.h"
#include "plas/backend/interface/pci/pci_config.h"
#include "plas/backend/interface/pci/pci_doe.h"
#include "plas/core/error.h"
#include "plas/core/result.h"
#include "plas/core/types.h"

// ---------------------------------------------------------------------------
// Stub device — replace with a real PCI driver implementation
// ---------------------------------------------------------------------------
class StubPciDevice : public plas::backend::Device,
                      public plas::backend::pci::PciConfig,
                      public plas::backend::pci::PciDoe {
public:
    // Device
    plas::core::Result<void> Init() override {
        return plas::core::Result<void>::Ok();
    }
    plas::core::Result<void> Open() override {
        return plas::core::Result<void>::Ok();
    }
    plas::core::Result<void> Close() override {
        return plas::core::Result<void>::Ok();
    }
    plas::core::Result<void> Reset() override {
        return plas::core::Result<void>::Ok();
    }
    plas::backend::DeviceState GetState() const override {
        return plas::backend::DeviceState::kOpen;
    }
    std::string GetName() const override { return "stub_pci"; }
    std::string GetUri() const override { return "stub://0:0"; }

    // PciConfig — stub reads/writes
    plas::core::Result<plas::core::Byte> ReadConfig8(
        plas::backend::pci::Bdf, plas::backend::pci::ConfigOffset) override {
        return plas::core::Result<plas::core::Byte>::Ok(0);
    }
    plas::core::Result<plas::core::Word> ReadConfig16(
        plas::backend::pci::Bdf, plas::backend::pci::ConfigOffset) override {
        return plas::core::Result<plas::core::Word>::Ok(0);
    }
    plas::core::Result<plas::core::DWord> ReadConfig32(
        plas::backend::pci::Bdf, plas::backend::pci::ConfigOffset) override {
        return plas::core::Result<plas::core::DWord>::Ok(0);
    }
    plas::core::Result<void> WriteConfig8(
        plas::backend::pci::Bdf, plas::backend::pci::ConfigOffset,
        plas::core::Byte) override {
        return plas::core::Result<void>::Ok();
    }
    plas::core::Result<void> WriteConfig16(
        plas::backend::pci::Bdf, plas::backend::pci::ConfigOffset,
        plas::core::Word) override {
        return plas::core::Result<void>::Ok();
    }
    plas::core::Result<void> WriteConfig32(
        plas::backend::pci::Bdf, plas::backend::pci::ConfigOffset,
        plas::core::DWord) override {
        return plas::core::Result<void>::Ok();
    }

    plas::core::Result<std::optional<plas::backend::pci::ConfigOffset>>
    FindCapability(plas::backend::pci::Bdf,
                   plas::backend::pci::CapabilityId) override {
        return plas::core::Result<
            std::optional<plas::backend::pci::ConfigOffset>>::Ok(std::nullopt);
    }

    plas::core::Result<std::optional<plas::backend::pci::ConfigOffset>>
    FindExtCapability(plas::backend::pci::Bdf,
                      plas::backend::pci::ExtCapabilityId id) override {
        using namespace plas::backend::pci;
        if (id == ExtCapabilityId::kDoe) {
            return plas::core::Result<std::optional<ConfigOffset>>::Ok(
                std::optional<ConfigOffset>(0x150));
        }
        return plas::core::Result<std::optional<ConfigOffset>>::Ok(
            std::nullopt);
    }

    // PciDoe — stub discover/exchange
    plas::core::Result<std::vector<plas::backend::pci::DoeProtocolId>>
    DoeDiscover(plas::backend::pci::Bdf,
                plas::backend::pci::ConfigOffset) override {
        using namespace plas::backend::pci;
        return plas::core::Result<std::vector<DoeProtocolId>>::Ok(
            {{doe_vendor::kPciSig, doe_type::kDoeDiscovery},
             {doe_vendor::kPciSig, doe_type::kCma}});
    }

    plas::core::Result<plas::backend::pci::DoePayload> DoeExchange(
        plas::backend::pci::Bdf, plas::backend::pci::ConfigOffset,
        plas::backend::pci::DoeProtocolId,
        const plas::backend::pci::DoePayload&) override {
        // Echo back a dummy response
        return plas::core::Result<plas::backend::pci::DoePayload>::Ok(
            {0xDEADBEEF, 0xCAFEBABE});
    }
};

// ---------------------------------------------------------------------------
// Helper: parse "bus:device.function" string into Bdf
// ---------------------------------------------------------------------------
static std::optional<plas::backend::pci::Bdf> ParseBdf(const char* str) {
    unsigned bus = 0, dev = 0, func = 0;
    if (std::sscanf(str, "%x:%x.%x", &bus, &dev, &func) != 3) {
        return std::nullopt;
    }
    if (bus > 0xFF || dev > 0x1F || func > 0x07) {
        return std::nullopt;
    }
    return plas::backend::pci::Bdf{static_cast<uint8_t>(bus),
                                    static_cast<uint8_t>(dev),
                                    static_cast<uint8_t>(func)};
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    using namespace plas::backend::pci;

    // --- Parse BDF from command line ---
    if (argc < 2) {
        std::printf("Usage: %s <bus:device.function>\n", argv[0]);
        std::printf("Example: %s 00:1f.0\n", argv[0]);
        return 1;
    }

    auto bdf_opt = ParseBdf(argv[1]);
    if (!bdf_opt) {
        std::printf("Error: invalid BDF format '%s' (expected XX:XX.X)\n",
                    argv[1]);
        return 1;
    }
    Bdf bdf = *bdf_opt;
    std::printf("[*] Target BDF: %02X:%02X.%X (packed: 0x%04X)\n", bdf.bus,
                bdf.device, bdf.function, bdf.Pack());

    // --- Create device ---
    auto device = std::make_unique<StubPciDevice>();
    device->Init();
    device->Open();

    // --- Check PCI capabilities ---
    auto* pci_config = dynamic_cast<PciConfig*>(device.get());
    auto* pci_doe = dynamic_cast<PciDoe*>(device.get());

    if (!pci_config || !pci_doe) {
        std::printf("Error: device does not support PciConfig + PciDoe\n");
        return 1;
    }
    std::printf("[*] Device '%s' supports PciConfig + PciDoe\n",
                device->GetName().c_str());

    // --- Find DOE Extended Capability ---
    auto find_result = pci_config->FindExtCapability(bdf, ExtCapabilityId::kDoe);
    if (find_result.IsError()) {
        std::printf("Error: FindExtCapability failed: %s\n",
                    find_result.Error().message().c_str());
        return 1;
    }
    if (!find_result.Value().has_value()) {
        std::printf("Error: DOE Extended Capability not found\n");
        return 1;
    }

    ConfigOffset doe_offset = find_result.Value().value();
    std::printf("[*] DOE Extended Capability at offset 0x%03X\n", doe_offset);

    // --- Discover DOE protocols ---
    auto disc_result = pci_doe->DoeDiscover(bdf, doe_offset);
    if (disc_result.IsError()) {
        std::printf("Error: DoeDiscover failed: %s\n",
                    disc_result.Error().message().c_str());
        return 1;
    }

    auto& protocols = disc_result.Value();
    std::printf("[*] Discovered %zu DOE protocol(s):\n", protocols.size());
    for (size_t i = 0; i < protocols.size(); ++i) {
        std::printf("    [%zu] vendor=0x%04X type=0x%02X\n", i,
                    protocols[i].vendor_id, protocols[i].data_object_type);
    }

    // --- DOE Exchange (using CMA protocol if available) ---
    DoeProtocolId cma_protocol{doe_vendor::kPciSig, doe_type::kCma};

    bool cma_found = false;
    for (const auto& p : protocols) {
        if (p == cma_protocol) {
            cma_found = true;
            break;
        }
    }

    if (!cma_found) {
        std::printf("[*] CMA protocol not supported, skipping exchange\n");
        device->Close();
        return 0;
    }

    std::printf("[*] Sending DOE exchange (CMA protocol)...\n");
    DoePayload request = {0x00000001, 0x00000002};
    std::printf("    Request:  ");
    for (auto dw : request) {
        std::printf("0x%08X ", dw);
    }
    std::printf("\n");

    auto xchg_result =
        pci_doe->DoeExchange(bdf, doe_offset, cma_protocol, request);
    if (xchg_result.IsError()) {
        std::printf("Error: DoeExchange failed: %s\n",
                    xchg_result.Error().message().c_str());
        return 1;
    }

    auto& response = xchg_result.Value();
    std::printf("    Response: ");
    for (auto dw : response) {
        std::printf("0x%08X ", dw);
    }
    std::printf("\n");

    // --- Cleanup ---
    device->Close();
    std::printf("[*] Done.\n");
    return 0;
}
