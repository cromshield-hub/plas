#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "plas/config/device_entry.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/pci/pci_config.h"
#include "plas/hal/interface/pci/pci_doe.h"

// Forward-declare libpci types to keep the header free of pci/pci.h.
struct pci_access;
struct pci_dev;

namespace plas::hal::driver {

/// PCI config-space driver backed by the system libpci (pciutils).
///
/// URI format: pciutils://DDDD:BB:DD.F
///
/// Optional DeviceEntry args:
///   doe_timeout_ms      — DOE mailbox timeout in milliseconds (default 1000)
///   doe_poll_interval_us — DOE polling interval in microseconds (default 100)
class PciUtilsDevice : public Device,
                       public pci::PciConfig,
                       public pci::PciDoe {
public:
    explicit PciUtilsDevice(const config::DeviceEntry& entry);
    ~PciUtilsDevice() override;

    // -- Device interface -----------------------------------------------------
    core::Result<void> Init() override;
    core::Result<void> Open() override;
    core::Result<void> Close() override;
    core::Result<void> Reset() override;
    DeviceState GetState() const override;
    std::string GetName() const override;
    std::string GetUri() const override;

    // -- PciConfig interface --------------------------------------------------
    core::Result<core::Byte> ReadConfig8(pci::Bdf bdf,
                                         pci::ConfigOffset offset) override;
    core::Result<core::Word> ReadConfig16(pci::Bdf bdf,
                                          pci::ConfigOffset offset) override;
    core::Result<core::DWord> ReadConfig32(pci::Bdf bdf,
                                           pci::ConfigOffset offset) override;
    core::Result<void> WriteConfig8(pci::Bdf bdf, pci::ConfigOffset offset,
                                    core::Byte value) override;
    core::Result<void> WriteConfig16(pci::Bdf bdf, pci::ConfigOffset offset,
                                     core::Word value) override;
    core::Result<void> WriteConfig32(pci::Bdf bdf, pci::ConfigOffset offset,
                                     core::DWord value) override;
    core::Result<std::optional<pci::ConfigOffset>> FindCapability(
        pci::Bdf bdf, pci::CapabilityId id) override;
    core::Result<std::optional<pci::ConfigOffset>> FindExtCapability(
        pci::Bdf bdf, pci::ExtCapabilityId id) override;

    // -- PciDoe interface -----------------------------------------------------
    core::Result<std::vector<pci::DoeProtocolId>> DoeDiscover(
        pci::Bdf bdf, pci::ConfigOffset doe_offset) override;
    core::Result<pci::DoePayload> DoeExchange(
        pci::Bdf bdf, pci::ConfigOffset doe_offset,
        pci::DoeProtocolId protocol,
        const pci::DoePayload& request) override;

    /// Self-register with DeviceFactory under driver name "pciutils".
    static void Register();

private:
    // RAII wrapper for pci_dev* obtained via pci_get_dev().
    struct PciDevDeleter {
        void operator()(pci_dev* d) const;
    };
    using PciDevPtr = std::unique_ptr<pci_dev, PciDevDeleter>;

    /// Create a PciDevPtr for the given BDF within our domain.
    PciDevPtr GetPciDev(pci::Bdf bdf);

    /// Parse a "pciutils://DDDD:BB:DD.F" URI.
    static bool ParseUri(const std::string& uri, uint16_t& domain,
                         uint8_t& bus, uint8_t& device, uint8_t& function);

    // DOE helpers
    core::Result<void> DoeAbort(pci_dev* dev, pci::ConfigOffset doe_offset);
    core::Result<bool> DoePollReady(pci_dev* dev,
                                    pci::ConfigOffset doe_offset);
    core::Result<void> DoeWriteMailbox(pci_dev* dev,
                                       pci::ConfigOffset doe_offset,
                                       const pci::DoePayload& payload);
    core::Result<pci::DoePayload> DoeReadMailbox(pci_dev* dev,
                                                  pci::ConfigOffset doe_offset);

    std::string name_;
    std::string uri_;
    DeviceState state_;
    uint16_t domain_;
    uint8_t bus_;
    uint8_t device_num_;
    uint8_t function_;
    pci_access* pacc_;
    uint32_t doe_timeout_ms_;
    uint32_t doe_poll_interval_us_;
    std::mutex doe_mutex_;
};

}  // namespace plas::hal::driver
