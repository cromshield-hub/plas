#include "plas/hal/driver/pciutils/pciutils_device.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C" {
#include <pci/pci.h>
}

#include "plas/core/error.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/log/logger.h"

namespace plas::hal::driver {

// ---------------------------------------------------------------------------
// DOE register offsets (relative to DOE extended capability base)
// ---------------------------------------------------------------------------
namespace doe_reg {
constexpr pci::ConfigOffset kControl = 0x08;
constexpr pci::ConfigOffset kStatus = 0x0C;
constexpr pci::ConfigOffset kWriteMailbox = 0x10;
constexpr pci::ConfigOffset kReadMailbox = 0x14;
}  // namespace doe_reg

namespace doe_ctrl {
constexpr uint32_t kAbort = 1u << 0;
constexpr uint32_t kGo = 1u << 31;
}  // namespace doe_ctrl

namespace doe_status {
constexpr uint32_t kBusy = 1u << 0;
constexpr uint32_t kError = 1u << 2;
constexpr uint32_t kReady = 1u << 31;
}  // namespace doe_status

// ---------------------------------------------------------------------------
// PciDevDeleter
// ---------------------------------------------------------------------------

void PciUtilsDevice::PciDevDeleter::operator()(pci_dev* d) const {
    if (d) {
        pci_free_dev(d);
    }
}

// ---------------------------------------------------------------------------
// URI parsing
// ---------------------------------------------------------------------------

bool PciUtilsDevice::ParseUri(const std::string& uri, uint16_t& domain,
                               uint8_t& bus, uint8_t& device,
                               uint8_t& function) {
    const std::string prefix = "pciutils://";
    if (uri.size() <= prefix.size() ||
        uri.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }

    const std::string bdf_str = uri.substr(prefix.size());

    unsigned d, b, dev, f;
    if (std::sscanf(bdf_str.c_str(), "%x:%x:%x.%x", &d, &b, &dev, &f) != 4) {
        return false;
    }

    if (d > 0xFFFF || b > 0xFF || dev > 0x1F || f > 0x07) {
        return false;
    }

    domain = static_cast<uint16_t>(d);
    bus = static_cast<uint8_t>(b);
    device = static_cast<uint8_t>(dev);
    function = static_cast<uint8_t>(f);
    return true;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

PciUtilsDevice::PciUtilsDevice(const config::DeviceEntry& entry)
    : name_(entry.nickname),
      uri_(entry.uri),
      state_(DeviceState::kUninitialized),
      domain_(0),
      bus_(0),
      device_num_(0),
      function_(0),
      pacc_(nullptr),
      doe_timeout_ms_(1000),
      doe_poll_interval_us_(100) {
    // Parse optional DOE args.
    auto it = entry.args.find("doe_timeout_ms");
    if (it != entry.args.end()) {
        try {
            doe_timeout_ms_ = static_cast<uint32_t>(std::stoul(it->second));
        } catch (...) {
            // keep default
        }
    }
    it = entry.args.find("doe_poll_interval_us");
    if (it != entry.args.end()) {
        try {
            doe_poll_interval_us_ =
                static_cast<uint32_t>(std::stoul(it->second));
        } catch (...) {
            // keep default
        }
    }
}

PciUtilsDevice::~PciUtilsDevice() {
    UnmapAllBars();
    if (pacc_) {
        pci_cleanup(pacc_);
        pacc_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Device lifecycle
// ---------------------------------------------------------------------------

core::Result<void> PciUtilsDevice::Init() {
    if (state_ != DeviceState::kUninitialized &&
        state_ != DeviceState::kClosed) {
        return core::Result<void>::Err(core::ErrorCode::kAlreadyOpen);
    }

    if (!ParseUri(uri_, domain_, bus_, device_num_, function_)) {
        PLAS_LOG_ERROR("PciUtilsDevice::Init() invalid URI: " + uri_);
        state_ = DeviceState::kError;
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    PLAS_LOG_INFO("PciUtilsDevice::Init() " + name_ + " → " + uri_);
    state_ = DeviceState::kInitialized;
    return core::Result<void>::Ok();
}

core::Result<void> PciUtilsDevice::Open() {
    if (state_ != DeviceState::kInitialized) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }

    pacc_ = pci_alloc();
    if (!pacc_) {
        PLAS_LOG_ERROR("PciUtilsDevice::Open() pci_alloc failed");
        state_ = DeviceState::kError;
        return core::Result<void>::Err(core::ErrorCode::kOutOfMemory);
    }

    pci_init(pacc_);

    PLAS_LOG_INFO("PciUtilsDevice::Open() " + name_);
    state_ = DeviceState::kOpen;
    return core::Result<void>::Ok();
}

core::Result<void> PciUtilsDevice::Close() {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }

    UnmapAllBars();

    if (pacc_) {
        pci_cleanup(pacc_);
        pacc_ = nullptr;
    }

    PLAS_LOG_INFO("PciUtilsDevice::Close() " + name_);
    state_ = DeviceState::kClosed;
    return core::Result<void>::Ok();
}

core::Result<void> PciUtilsDevice::Reset() {
    PLAS_LOG_INFO("PciUtilsDevice::Reset() " + name_);
    if (state_ == DeviceState::kOpen) {
        auto result = Close();
        if (result.IsError()) {
            return result;
        }
    }
    return Init();
}

DeviceState PciUtilsDevice::GetState() const {
    return state_;
}

std::string PciUtilsDevice::GetName() const {
    return name_;
}

std::string PciUtilsDevice::GetUri() const {
    return uri_;
}

std::string PciUtilsDevice::GetDriverName() const {
    return "pciutils";
}

plas::hal::Device* PciUtilsDevice::GetDevice() {
    return this;
}

// ---------------------------------------------------------------------------
// PciDevPtr helper
// ---------------------------------------------------------------------------

PciUtilsDevice::PciDevPtr PciUtilsDevice::GetPciDev(pci::Bdf bdf) {
    if (!pacc_) {
        return nullptr;
    }
    pci_dev* dev =
        pci_get_dev(pacc_, domain_, bdf.bus, bdf.device, bdf.function);
    return PciDevPtr(dev);
}

// ---------------------------------------------------------------------------
// PciConfig — typed reads
// ---------------------------------------------------------------------------

core::Result<core::Byte> PciUtilsDevice::ReadConfig8(
    pci::Bdf bdf, pci::ConfigOffset offset) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<core::Byte>::Err(core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        PLAS_LOG_ERROR("[" + name_ + "][PciConfig] ReadConfig8 offset=" +
                       std::to_string(offset) + " failed: GetPciDev returned null");
        return core::Result<core::Byte>::Err(core::ErrorCode::kIOError);
    }
    auto val = pci_read_byte(dev.get(), offset);
    return core::Result<core::Byte>::Ok(val);
}

core::Result<core::Word> PciUtilsDevice::ReadConfig16(
    pci::Bdf bdf, pci::ConfigOffset offset) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<core::Word>::Err(core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        PLAS_LOG_ERROR("[" + name_ + "][PciConfig] ReadConfig16 offset=" +
                       std::to_string(offset) + " failed: GetPciDev returned null");
        return core::Result<core::Word>::Err(core::ErrorCode::kIOError);
    }
    auto val = pci_read_word(dev.get(), offset);
    return core::Result<core::Word>::Ok(val);
}

core::Result<core::DWord> PciUtilsDevice::ReadConfig32(
    pci::Bdf bdf, pci::ConfigOffset offset) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<core::DWord>::Err(
            core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        PLAS_LOG_ERROR("[" + name_ + "][PciConfig] ReadConfig32 offset=" +
                       std::to_string(offset) + " failed: GetPciDev returned null");
        return core::Result<core::DWord>::Err(core::ErrorCode::kIOError);
    }
    auto val = pci_read_long(dev.get(), offset);
    return core::Result<core::DWord>::Ok(val);
}

// ---------------------------------------------------------------------------
// PciConfig — typed writes
// ---------------------------------------------------------------------------

core::Result<void> PciUtilsDevice::WriteConfig8(pci::Bdf bdf,
                                                 pci::ConfigOffset offset,
                                                 core::Byte value) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        PLAS_LOG_ERROR("[" + name_ + "][PciConfig] WriteConfig8 offset=" +
                       std::to_string(offset) + " failed: GetPciDev returned null");
        return core::Result<void>::Err(core::ErrorCode::kIOError);
    }
    pci_write_byte(dev.get(), offset, value);
    return core::Result<void>::Ok();
}

core::Result<void> PciUtilsDevice::WriteConfig16(pci::Bdf bdf,
                                                  pci::ConfigOffset offset,
                                                  core::Word value) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        PLAS_LOG_ERROR("[" + name_ + "][PciConfig] WriteConfig16 offset=" +
                       std::to_string(offset) + " failed: GetPciDev returned null");
        return core::Result<void>::Err(core::ErrorCode::kIOError);
    }
    pci_write_word(dev.get(), offset, value);
    return core::Result<void>::Ok();
}

core::Result<void> PciUtilsDevice::WriteConfig32(pci::Bdf bdf,
                                                  pci::ConfigOffset offset,
                                                  core::DWord value) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        PLAS_LOG_ERROR("[" + name_ + "][PciConfig] WriteConfig32 offset=" +
                       std::to_string(offset) + " failed: GetPciDev returned null");
        return core::Result<void>::Err(core::ErrorCode::kIOError);
    }
    pci_write_long(dev.get(), offset, value);
    return core::Result<void>::Ok();
}

// ---------------------------------------------------------------------------
// PciConfig — capability walking
// ---------------------------------------------------------------------------

core::Result<std::optional<pci::ConfigOffset>>
PciUtilsDevice::FindCapability(pci::Bdf bdf, pci::CapabilityId id) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<std::optional<pci::ConfigOffset>>::Err(
            core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        return core::Result<std::optional<pci::ConfigOffset>>::Err(
            core::ErrorCode::kIOError);
    }

    // Check if device has capabilities (Status register bit 4).
    auto status = pci_read_word(dev.get(), 0x06);
    if (!(status & (1u << 4))) {
        return core::Result<std::optional<pci::ConfigOffset>>::Ok(
            std::nullopt);
    }

    uint8_t offset = pci_read_byte(dev.get(), 0x34) & 0xFC;
    constexpr int kMaxCaps = 48;  // guard against loops
    for (int i = 0; i < kMaxCaps && offset != 0; ++i) {
        auto cap_id = pci_read_byte(dev.get(), offset);
        if (cap_id == static_cast<uint8_t>(id)) {
            return core::Result<std::optional<pci::ConfigOffset>>::Ok(
                static_cast<pci::ConfigOffset>(offset));
        }
        offset = pci_read_byte(dev.get(), offset + 1) & 0xFC;
    }

    return core::Result<std::optional<pci::ConfigOffset>>::Ok(std::nullopt);
}

core::Result<std::optional<pci::ConfigOffset>>
PciUtilsDevice::FindExtCapability(pci::Bdf bdf, pci::ExtCapabilityId id) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<std::optional<pci::ConfigOffset>>::Err(
            core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        return core::Result<std::optional<pci::ConfigOffset>>::Err(
            core::ErrorCode::kIOError);
    }

    pci::ConfigOffset offset = 0x100;
    constexpr int kMaxExtCaps = 256;  // guard against loops
    for (int i = 0; i < kMaxExtCaps && offset != 0; ++i) {
        auto header = pci_read_long(dev.get(), offset);
        uint16_t cap_id = header & 0xFFFF;
        if (cap_id == static_cast<uint16_t>(id)) {
            return core::Result<std::optional<pci::ConfigOffset>>::Ok(offset);
        }
        uint16_t next = (header >> 20) & 0xFFC;
        if (next == 0 || next <= offset) {
            break;
        }
        offset = next;
    }

    return core::Result<std::optional<pci::ConfigOffset>>::Ok(std::nullopt);
}

// ---------------------------------------------------------------------------
// DOE helpers
// ---------------------------------------------------------------------------

core::Result<void> PciUtilsDevice::DoeAbort(pci_dev* dev,
                                             pci::ConfigOffset doe_offset) {
    pci_write_long(dev, doe_offset + doe_reg::kControl, doe_ctrl::kAbort);

    // Poll until abort completes (Busy clears).
    auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(doe_timeout_ms_);
    while (std::chrono::steady_clock::now() < deadline) {
        auto status = pci_read_long(dev, doe_offset + doe_reg::kStatus);
        if (!(status & doe_status::kBusy)) {
            return core::Result<void>::Ok();
        }
        std::this_thread::sleep_for(
            std::chrono::microseconds(doe_poll_interval_us_));
    }
    return core::Result<void>::Err(core::ErrorCode::kTimeout);
}

core::Result<bool> PciUtilsDevice::DoePollReady(
    pci_dev* dev, pci::ConfigOffset doe_offset) {
    auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(doe_timeout_ms_);
    while (std::chrono::steady_clock::now() < deadline) {
        auto status = pci_read_long(dev, doe_offset + doe_reg::kStatus);
        if (status & doe_status::kError) {
            return core::Result<bool>::Err(core::ErrorCode::kIOError);
        }
        if (status & doe_status::kReady) {
            return core::Result<bool>::Ok(true);
        }
        std::this_thread::sleep_for(
            std::chrono::microseconds(doe_poll_interval_us_));
    }
    return core::Result<bool>::Err(core::ErrorCode::kTimeout);
}

core::Result<void> PciUtilsDevice::DoeWriteMailbox(
    pci_dev* dev, pci::ConfigOffset doe_offset,
    const pci::DoePayload& payload) {
    for (auto dw : payload) {
        pci_write_long(dev, doe_offset + doe_reg::kWriteMailbox, dw);
    }
    return core::Result<void>::Ok();
}

core::Result<pci::DoePayload> PciUtilsDevice::DoeReadMailbox(
    pci_dev* dev, pci::ConfigOffset doe_offset) {
    pci::DoePayload result;

    // Read first two DWORDs (DOE header) to determine payload length.
    auto dw0 = pci_read_long(dev, doe_offset + doe_reg::kReadMailbox);
    result.push_back(dw0);
    // Reading the mailbox register advances the internal pointer.

    auto dw1 = pci_read_long(dev, doe_offset + doe_reg::kReadMailbox);
    result.push_back(dw1);

    // Length field: bits [17:0] of DW1. 0 means 2^18.
    uint32_t length = dw1 & 0x0003FFFF;
    if (length == 0) {
        length = (1u << 18);
    }

    // length includes the 2-DWord header, so remaining = length - 2.
    if (length > 2) {
        for (uint32_t i = 0; i < length - 2; ++i) {
            auto dw = pci_read_long(dev, doe_offset + doe_reg::kReadMailbox);
            result.push_back(dw);
        }
    }

    return core::Result<pci::DoePayload>::Ok(std::move(result));
}

// ---------------------------------------------------------------------------
// PciDoe — DoeDiscover
// ---------------------------------------------------------------------------

core::Result<std::vector<pci::DoeProtocolId>>
PciUtilsDevice::DoeDiscover(pci::Bdf bdf, pci::ConfigOffset doe_offset) {
    std::lock_guard<std::mutex> lock(doe_mutex_);

    if (state_ != DeviceState::kOpen) {
        return core::Result<std::vector<pci::DoeProtocolId>>::Err(
            core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        return core::Result<std::vector<pci::DoeProtocolId>>::Err(
            core::ErrorCode::kIOError);
    }

    std::vector<pci::DoeProtocolId> protocols;
    uint8_t discovery_index = 0;

    while (true) {
        // Build discovery request:
        //   DW0 (header): VendorID=0x0001, Type=0x00
        //   DW1 (header): Length=3 (2 header + 1 payload)
        //   DW2 (payload): discovery_index in bits [7:0]
        pci::DoePayload request;
        request.push_back(
            (static_cast<uint32_t>(pci::doe_type::kDoeDiscovery) << 16) |
            pci::doe_vendor::kPciSig);
        request.push_back(3);  // length in DWords
        request.push_back(static_cast<uint32_t>(discovery_index));

        // Check busy.
        auto status = pci_read_long(dev.get(),
                                    doe_offset + doe_reg::kStatus);
        if (status & doe_status::kBusy) {
            auto abort_res = DoeAbort(dev.get(), doe_offset);
            if (abort_res.IsError()) {
                return core::Result<std::vector<pci::DoeProtocolId>>::Err(
                    abort_res.Error());
            }
        }

        // Write request.
        auto wr = DoeWriteMailbox(dev.get(), doe_offset, request);
        if (wr.IsError()) {
            return core::Result<std::vector<pci::DoeProtocolId>>::Err(
                wr.Error());
        }

        // Set GO bit.
        pci_write_long(dev.get(), doe_offset + doe_reg::kControl,
                       doe_ctrl::kGo);

        // Poll for ready.
        auto ready = DoePollReady(dev.get(), doe_offset);
        if (ready.IsError()) {
            return core::Result<std::vector<pci::DoeProtocolId>>::Err(
                ready.Error());
        }

        // Read response.
        auto resp = DoeReadMailbox(dev.get(), doe_offset);
        if (resp.IsError()) {
            return core::Result<std::vector<pci::DoeProtocolId>>::Err(
                resp.Error());
        }

        auto& payload = resp.Value();
        // Response: DW0=header, DW1=length, DW2=discovery response
        if (payload.size() < 3) {
            return core::Result<std::vector<pci::DoeProtocolId>>::Err(
                core::ErrorCode::kDataLoss);
        }

        uint32_t disc_dw = payload[2];
        pci::DoeProtocolId proto;
        proto.vendor_id = static_cast<uint16_t>(disc_dw & 0xFFFF);
        proto.data_object_type =
            static_cast<uint8_t>((disc_dw >> 16) & 0xFF);
        uint8_t next_index = static_cast<uint8_t>((disc_dw >> 24) & 0xFF);

        protocols.push_back(proto);

        if (next_index == 0) {
            break;  // last entry
        }
        discovery_index = next_index;
    }

    return core::Result<std::vector<pci::DoeProtocolId>>::Ok(
        std::move(protocols));
}

// ---------------------------------------------------------------------------
// PciDoe — DoeExchange
// ---------------------------------------------------------------------------

core::Result<pci::DoePayload> PciUtilsDevice::DoeExchange(
    pci::Bdf bdf, pci::ConfigOffset doe_offset,
    pci::DoeProtocolId protocol, const pci::DoePayload& request) {
    std::lock_guard<std::mutex> lock(doe_mutex_);

    if (state_ != DeviceState::kOpen) {
        return core::Result<pci::DoePayload>::Err(
            core::ErrorCode::kNotInitialized);
    }
    auto dev = GetPciDev(bdf);
    if (!dev) {
        PLAS_LOG_ERROR("[" + name_ + "][PciDoe] DoeExchange failed: GetPciDev returned null");
        return core::Result<pci::DoePayload>::Err(core::ErrorCode::kIOError);
    }

    // Build full DOE data object: 2-DWord header + payload.
    pci::DoePayload full_request;

    // DW0: [15:0]=VendorID, [23:16]=DataObjectType
    uint32_t dw0 = (static_cast<uint32_t>(protocol.data_object_type) << 16) |
                   protocol.vendor_id;
    full_request.push_back(dw0);

    // DW1: [17:0]=Length (header + payload, in DWords)
    uint32_t length = static_cast<uint32_t>(2 + request.size());
    full_request.push_back(length);

    // Append user payload.
    full_request.insert(full_request.end(), request.begin(), request.end());

    // Check busy.
    auto status = pci_read_long(dev.get(), doe_offset + doe_reg::kStatus);
    if (status & doe_status::kBusy) {
        auto abort_res = DoeAbort(dev.get(), doe_offset);
        if (abort_res.IsError()) {
            return core::Result<pci::DoePayload>::Err(abort_res.Error());
        }
    }

    // Write request to mailbox.
    auto wr = DoeWriteMailbox(dev.get(), doe_offset, full_request);
    if (wr.IsError()) {
        return core::Result<pci::DoePayload>::Err(wr.Error());
    }

    // Set GO bit.
    pci_write_long(dev.get(), doe_offset + doe_reg::kControl, doe_ctrl::kGo);

    // Poll for response ready.
    auto ready = DoePollReady(dev.get(), doe_offset);
    if (ready.IsError()) {
        return core::Result<pci::DoePayload>::Err(ready.Error());
    }

    // Read response from mailbox.
    auto resp = DoeReadMailbox(dev.get(), doe_offset);
    if (resp.IsError()) {
        return core::Result<pci::DoePayload>::Err(resp.Error());
    }

    // Strip the 2-DWord header, return only payload.
    auto& full_response = resp.Value();
    if (full_response.size() < 2) {
        return core::Result<pci::DoePayload>::Err(
            core::ErrorCode::kDataLoss);
    }

    pci::DoePayload payload(full_response.begin() + 2,
                            full_response.end());
    return core::Result<pci::DoePayload>::Ok(std::move(payload));
}

// ---------------------------------------------------------------------------
// PciBar — BAR MMIO helpers
// ---------------------------------------------------------------------------

std::string FormatSysfsPath(uint16_t domain, uint8_t bus, uint8_t device,
                            uint8_t function) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "/sys/bus/pci/devices/%04x:%02x:%02x.%x",
                  domain, bus, device, function);
    return buf;
}

uint64_t PciUtilsDevice::GetBarSize(uint8_t bar_index) {
    auto sysfs = FormatSysfsPath(domain_, bus_, device_num_, function_);
    std::ifstream resource(sysfs + "/resource");
    if (!resource.is_open()) {
        return 0;
    }

    std::string line;
    for (uint8_t i = 0; i <= bar_index; ++i) {
        if (!std::getline(resource, line)) {
            return 0;
        }
    }

    // Each line: start_addr end_addr flags (hex)
    uint64_t start = 0, end = 0, flags = 0;
    std::istringstream iss(line);
    iss >> std::hex >> start >> end >> flags;
    if (end == 0 || end < start) {
        return 0;
    }
    return end - start + 1;
}

core::Result<PciUtilsDevice::MappedBar*>
PciUtilsDevice::EnsureBarMapped(uint8_t bar_index) {
    if (bar_index > 5) {
        return core::Result<MappedBar*>::Err(core::ErrorCode::kInvalidArgument);
    }

    std::lock_guard<std::mutex> lock(bar_mutex_);

    auto it = mapped_bars_.find(bar_index);
    if (it != mapped_bars_.end()) {
        return core::Result<MappedBar*>::Ok(&it->second);
    }

    uint64_t size = GetBarSize(bar_index);
    if (size == 0) {
        return core::Result<MappedBar*>::Err(core::ErrorCode::kNotFound);
    }

    auto sysfs = FormatSysfsPath(domain_, bus_, device_num_, function_);
    std::string resource_path = sysfs + "/resource" + std::to_string(bar_index);

    int fd = ::open(resource_path.c_str(), O_RDWR | O_SYNC);
    if (fd < 0) {
        PLAS_LOG_ERROR("PciUtilsDevice: cannot open " + resource_path);
        return core::Result<MappedBar*>::Err(core::ErrorCode::kPermissionDenied);
    }

    void* base = ::mmap(nullptr, static_cast<std::size_t>(size),
                        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        PLAS_LOG_ERROR("PciUtilsDevice: mmap failed for " + resource_path);
        ::close(fd);
        return core::Result<MappedBar*>::Err(core::ErrorCode::kIOError);
    }

    MappedBar bar;
    bar.fd = fd;
    bar.base = base;
    bar.size = size;
    auto [inserted, _] = mapped_bars_.emplace(bar_index, bar);
    return core::Result<MappedBar*>::Ok(&inserted->second);
}

void PciUtilsDevice::UnmapAllBars() {
    for (auto& [idx, bar] : mapped_bars_) {
        if (bar.base && bar.base != MAP_FAILED) {
            ::munmap(bar.base, static_cast<std::size_t>(bar.size));
        }
        if (bar.fd >= 0) {
            ::close(bar.fd);
        }
    }
    mapped_bars_.clear();
}

// ---------------------------------------------------------------------------
// PciBar — typed reads/writes
// ---------------------------------------------------------------------------

core::Result<core::DWord> PciUtilsDevice::BarRead32(
    pci::Bdf bdf, uint8_t bar_index, uint64_t offset) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<core::DWord>::Err(core::ErrorCode::kNotInitialized);
    }
    auto bar_result = EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
        PLAS_LOG_ERROR("[" + name_ + "][PciBar] BarRead32 bar=" +
                       std::to_string(bar_index) + " offset=" +
                       std::to_string(offset) + " failed: " +
                       make_error_code(bar_result.Error()).message());
        return core::Result<core::DWord>::Err(bar_result.Error());
    }
    auto* bar = bar_result.Value();
    if (offset + sizeof(core::DWord) > bar->size) {
        return core::Result<core::DWord>::Err(core::ErrorCode::kOutOfRange);
    }
    auto* ptr = reinterpret_cast<volatile uint32_t*>(
        static_cast<uint8_t*>(bar->base) + offset);
    return core::Result<core::DWord>::Ok(*ptr);
}

core::Result<core::QWord> PciUtilsDevice::BarRead64(
    pci::Bdf bdf, uint8_t bar_index, uint64_t offset) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<core::QWord>::Err(core::ErrorCode::kNotInitialized);
    }
    auto bar_result = EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
        PLAS_LOG_ERROR("[" + name_ + "][PciBar] BarRead64 bar=" +
                       std::to_string(bar_index) + " offset=" +
                       std::to_string(offset) + " failed: " +
                       make_error_code(bar_result.Error()).message());
        return core::Result<core::QWord>::Err(bar_result.Error());
    }
    auto* bar = bar_result.Value();
    if (offset + sizeof(core::QWord) > bar->size) {
        return core::Result<core::QWord>::Err(core::ErrorCode::kOutOfRange);
    }
    auto* ptr = reinterpret_cast<volatile uint64_t*>(
        static_cast<uint8_t*>(bar->base) + offset);
    return core::Result<core::QWord>::Ok(*ptr);
}

core::Result<void> PciUtilsDevice::BarWrite32(
    pci::Bdf bdf, uint8_t bar_index, uint64_t offset, core::DWord value) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }
    auto bar_result = EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
        PLAS_LOG_ERROR("[" + name_ + "][PciBar] BarWrite32 bar=" +
                       std::to_string(bar_index) + " offset=" +
                       std::to_string(offset) + " failed: " +
                       make_error_code(bar_result.Error()).message());
        return core::Result<void>::Err(bar_result.Error());
    }
    auto* bar = bar_result.Value();
    if (offset + sizeof(core::DWord) > bar->size) {
        return core::Result<void>::Err(core::ErrorCode::kOutOfRange);
    }
    auto* ptr = reinterpret_cast<volatile uint32_t*>(
        static_cast<uint8_t*>(bar->base) + offset);
    *ptr = value;
    return core::Result<void>::Ok();
}

core::Result<void> PciUtilsDevice::BarWrite64(
    pci::Bdf bdf, uint8_t bar_index, uint64_t offset, core::QWord value) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }
    auto bar_result = EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
        PLAS_LOG_ERROR("[" + name_ + "][PciBar] BarWrite64 bar=" +
                       std::to_string(bar_index) + " offset=" +
                       std::to_string(offset) + " failed: " +
                       make_error_code(bar_result.Error()).message());
        return core::Result<void>::Err(bar_result.Error());
    }
    auto* bar = bar_result.Value();
    if (offset + sizeof(core::QWord) > bar->size) {
        return core::Result<void>::Err(core::ErrorCode::kOutOfRange);
    }
    auto* ptr = reinterpret_cast<volatile uint64_t*>(
        static_cast<uint8_t*>(bar->base) + offset);
    *ptr = value;
    return core::Result<void>::Ok();
}

core::Result<void> PciUtilsDevice::BarReadBuffer(
    pci::Bdf bdf, uint8_t bar_index, uint64_t offset, void* buffer,
    std::size_t length) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }
    if (!buffer || length == 0) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }
    auto bar_result = EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
        return core::Result<void>::Err(bar_result.Error());
    }
    auto* bar = bar_result.Value();
    if (offset + length > bar->size) {
        return core::Result<void>::Err(core::ErrorCode::kOutOfRange);
    }
    auto* src = static_cast<uint8_t*>(bar->base) + offset;
    std::memcpy(buffer, src, length);
    return core::Result<void>::Ok();
}

core::Result<void> PciUtilsDevice::BarWriteBuffer(
    pci::Bdf bdf, uint8_t bar_index, uint64_t offset, const void* buffer,
    std::size_t length) {
    if (state_ != DeviceState::kOpen) {
        return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
    }
    if (!buffer || length == 0) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }
    auto bar_result = EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
        return core::Result<void>::Err(bar_result.Error());
    }
    auto* bar = bar_result.Value();
    if (offset + length > bar->size) {
        return core::Result<void>::Err(core::ErrorCode::kOutOfRange);
    }
    auto* dst = static_cast<uint8_t*>(bar->base) + offset;
    std::memcpy(dst, buffer, length);
    return core::Result<void>::Ok();
}

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------

void PciUtilsDevice::Register() {
    DeviceFactory::RegisterDriver(
        "pciutils", [](const config::DeviceEntry& entry) {
            return std::make_unique<PciUtilsDevice>(entry);
        });
}

}  // namespace plas::hal::driver
