#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "plas/core/result.h"
#include "plas/core/types.h"

namespace plas::hal::pci {

/// PCI Bus-Device-Function address.
struct Bdf {
    uint8_t bus;       ///< Bus number [7:0]
    uint8_t device;    ///< Device number [4:0] (5 bits valid)
    uint8_t function;  ///< Function number [2:0] (3 bits valid)

    /// Pack into 16-bit BDF encoding: bus[15:8] | device[7:3] | function[2:0].
    constexpr uint16_t Pack() const {
        return static_cast<uint16_t>((static_cast<uint16_t>(bus) << 8) |
                                     ((device & 0x1F) << 3) |
                                     (function & 0x07));
    }

    /// Unpack a 16-bit BDF encoding.
    static constexpr Bdf FromPacked(uint16_t packed) {
        return Bdf{static_cast<uint8_t>((packed >> 8) & 0xFF),
                   static_cast<uint8_t>((packed >> 3) & 0x1F),
                   static_cast<uint8_t>(packed & 0x07)};
    }

    constexpr bool operator==(const Bdf& other) const {
        return bus == other.bus && device == other.device &&
               function == other.function;
    }

    constexpr bool operator!=(const Bdf& other) const {
        return !(*this == other);
    }
};

/// PCI configuration space offset (0x000–0xFFF for extended config).
using ConfigOffset = uint16_t;

/// Standard PCI capability IDs (Type 0 config space, offset 0x34 chain).
enum class CapabilityId : uint8_t {
    kPowerManagement = 0x01,
    kAgp = 0x02,
    kVpd = 0x03,
    kMsi = 0x05,
    kPciExpress = 0x10,
    kMsix = 0x11,
};

/// Extended PCI capability IDs (config space 0x100+).
enum class ExtCapabilityId : uint16_t {
    kAer = 0x0001,
    kSriov = 0x0010,
    kDvsec = 0x0023,
    kDoe = 0x002E,
};

/// DOE (Data Object Exchange) protocol identifier.
struct DoeProtocolId {
    uint16_t vendor_id;
    uint8_t data_object_type;

    constexpr bool operator==(const DoeProtocolId& other) const {
        return vendor_id == other.vendor_id &&
               data_object_type == other.data_object_type;
    }

    constexpr bool operator!=(const DoeProtocolId& other) const {
        return !(*this == other);
    }
};

/// Well-known DOE vendor IDs.
namespace doe_vendor {
constexpr uint16_t kPciSig = 0x0001;
}  // namespace doe_vendor

/// Well-known DOE data object types.
namespace doe_type {
constexpr uint8_t kDoeDiscovery = 0x00;
constexpr uint8_t kCma = 0x01;
}  // namespace doe_type

/// DWord-aligned DOE payload.
using DoePayload = std::vector<core::DWord>;

/// Full PCI address including domain.
struct PciAddress {
    uint16_t domain;
    Bdf bdf;

    /// Format as "DDDD:BB:DD.F".
    std::string ToString() const;

    /// Parse "DDDD:BB:DD.F" string.
    static core::Result<PciAddress> FromString(const std::string& str);

    constexpr bool operator==(const PciAddress& other) const {
        return domain == other.domain && bdf == other.bdf;
    }

    constexpr bool operator!=(const PciAddress& other) const {
        return !(*this == other);
    }
};

/// PCIe device/port type (from PCIe Capability register bits [7:4]).
enum class PciePortType : uint8_t {
    kEndpoint = 0x00,
    kLegacyEndpoint = 0x01,
    kRootPort = 0x04,
    kUpstreamPort = 0x05,
    kDownstreamPort = 0x06,
    kPcieToPciBridge = 0x07,
    kPciToPcieBridge = 0x08,
    kRcIntegratedEndpoint = 0x09,
    kRcEventCollector = 0x0A,
    kUnknown = 0xFF,
};

}  // namespace plas::hal::pci
