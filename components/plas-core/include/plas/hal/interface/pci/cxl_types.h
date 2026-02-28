#pragma once

#include <cstdint>
#include <vector>

namespace plas::hal::pci {

/// CXL vendor ID (CXL Consortium).
namespace cxl {
constexpr uint16_t kCxlVendorId = 0x1E98;
}  // namespace cxl

/// CXL DVSEC IDs (CXL 3.1 Table 8-1).
enum class CxlDvsecId : uint16_t {
    kCxlDevice = 0x0000,
    kNonCxlFunction = 0x0002,
    kCxlExtensionsPort = 0x0003,
    kGpfPort = 0x0004,
    kGpfDevice = 0x0005,
    kFlexBusPort = 0x0007,
    kRegisterLocator = 0x0008,
    kMld = 0x0009,
};

/// CXL device types.
enum class CxlDeviceType : uint8_t {
    kType1 = 1,
    kType2 = 2,
    kType3 = 3,
    kUnknown = 0xFF,
};

/// CXL DVSEC header information.
struct DvsecHeader {
    uint16_t offset;         ///< Offset of this DVSEC in config space
    uint16_t vendor_id;      ///< DVSEC vendor ID
    uint16_t dvsec_revision; ///< DVSEC revision
    CxlDvsecId dvsec_id;     ///< DVSEC ID
    uint16_t dvsec_length;   ///< Total DVSEC length in bytes

    constexpr bool operator==(const DvsecHeader& other) const {
        return offset == other.offset && vendor_id == other.vendor_id &&
               dvsec_revision == other.dvsec_revision &&
               dvsec_id == other.dvsec_id &&
               dvsec_length == other.dvsec_length;
    }

    constexpr bool operator!=(const DvsecHeader& other) const {
        return !(*this == other);
    }
};

/// CXL Register Block IDs (Register Locator DVSEC).
enum class CxlRegisterBlockId : uint8_t {
    kEmpty = 0,
    kComponentRegister = 1,
    kBarVirtualized = 2,
    kCxlDeviceRegister = 3,
};

/// Entry from the CXL Register Locator DVSEC.
struct RegisterBlockEntry {
    CxlRegisterBlockId block_id;
    uint8_t bar_index;
    uint64_t offset;

    constexpr bool operator==(const RegisterBlockEntry& other) const {
        return block_id == other.block_id && bar_index == other.bar_index &&
               offset == other.offset;
    }

    constexpr bool operator!=(const RegisterBlockEntry& other) const {
        return !(*this == other);
    }
};

/// CXL Mailbox opcodes (CXL 3.1 Table 8-93).
enum class CxlMailboxOpcode : uint16_t {
    kIdentify = 0x0001,
    kGetSupportedLogs = 0x0400,
    kGetLog = 0x0401,
    kGetSupportedFeatures = 0x0500,
    kGetFeature = 0x0501,
    kSetFeature = 0x0502,
    kIdentifyMemoryDevice = 0x4000,
    kGetHealthInfo = 0x4200,
    kGetAlertConfig = 0x4201,
    kSetAlertConfig = 0x4202,
    kGetPoisonList = 0x4300,
    kInjectPoison = 0x4301,
    kClearPoison = 0x4302,
    kSanitize = 0x4400,
    kSecureErase = 0x4401,
    kGetScanMediaCapabilities = 0x4302,
    kGetTimestamp = 0x0300,
    kSetTimestamp = 0x0301,
    kGetFwInfo = 0x0200,
    kTransferFw = 0x0201,
    kActivateFw = 0x0202,
};

/// CXL Mailbox return codes (CXL 3.1 Table 8-95).
enum class CxlMailboxReturnCode : uint16_t {
    kSuccess = 0x0000,
    kBackgroundCmdStarted = 0x0001,
    kInvalidInput = 0x0002,
    kUnsupported = 0x0003,
    kInternalError = 0x0004,
    kRetryRequired = 0x0005,
    kBusy = 0x0006,
    kMediaDisabled = 0x0007,
    kFwTransferInProgress = 0x0008,
    kFwTransferOutOfOrder = 0x0009,
    kFwAuthenticationFailed = 0x000A,
    kInvalidSlot = 0x000B,
    kActivationFailedFwRolled = 0x000C,
    kActivationFailedColdReset = 0x000D,
    kInvalidHandle = 0x000E,
    kInvalidPhysicalAddress = 0x000F,
    kPayloadLengthError = 0x0012,
};

/// CXL Mailbox payload (byte vector).
using CxlMailboxPayload = std::vector<uint8_t>;

/// Result of a CXL Mailbox command execution.
struct CxlMailboxResult {
    CxlMailboxReturnCode return_code;
    CxlMailboxPayload payload;
};

/// DOE data object type for IDE-KM (CXL/PCIe IDE Key Management).
namespace doe_type {
constexpr uint8_t kIdeKm = 0x02;
}  // namespace doe_type

/// IDE-KM message types.
enum class IdeKmMessageType : uint8_t {
    kQuery = 0x00,
    kQueryResp = 0x01,
    kKeyProg = 0x02,
    kKeyProgAck = 0x03,
    kKeySetGo = 0x04,
    kKeySetGoAck = 0x05,
    kKeySetStop = 0x06,
    kKeySetStopAck = 0x07,
};

/// IDE Stream identifier.
struct IdeStreamId {
    uint8_t stream_id;
    uint8_t key_set;
    uint8_t direction;  ///< 0 = TX, 1 = RX
    uint8_t sub_stream;

    constexpr bool operator==(const IdeStreamId& other) const {
        return stream_id == other.stream_id && key_set == other.key_set &&
               direction == other.direction && sub_stream == other.sub_stream;
    }

    constexpr bool operator!=(const IdeStreamId& other) const {
        return !(*this == other);
    }
};

}  // namespace plas::hal::pci
