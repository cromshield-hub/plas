#include "plas/hal/interface/pci/pci_device.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "plas/core/error.h"

namespace plas::hal::pci {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct MappedBar {
    int fd = -1;
    void* base = nullptr;
    uint64_t size = 0;
};

struct PciDevice::Impl {
    PciDeviceNode info;
    int config_fd = -1;
    std::unordered_map<uint8_t, MappedBar> mapped_bars;

    explicit Impl(PciDeviceNode&& node) : info(std::move(node)) {}

    ~Impl() {
        UnmapAllBars();
        CloseConfigFd();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // -- config fd --

    core::Result<void> EnsureConfigFd() {
        if (config_fd >= 0) {
            return core::Result<void>::Ok();
        }
        std::string config_path = info.sysfs_path + "/config";
        config_fd = ::open(config_path.c_str(), O_RDWR | O_SYNC);
        if (config_fd < 0) {
            // Retry read-only for non-root users
            config_fd = ::open(config_path.c_str(), O_RDONLY);
            if (config_fd < 0) {
                return core::Result<void>::Err(
                    core::ErrorCode::kPermissionDenied);
            }
        }
        return core::Result<void>::Ok();
    }

    void CloseConfigFd() {
        if (config_fd >= 0) {
            ::close(config_fd);
            config_fd = -1;
        }
    }

    // -- BAR mmap --

    uint64_t GetBarSize(uint8_t bar_index) {
        std::ifstream resource(info.sysfs_path + "/resource");
        if (!resource.is_open()) {
            return 0;
        }
        std::string line;
        for (uint8_t i = 0; i <= bar_index; ++i) {
            if (!std::getline(resource, line)) {
                return 0;
            }
        }
        uint64_t start = 0, end = 0, flags = 0;
        std::istringstream iss(line);
        iss >> std::hex >> start >> end >> flags;
        if (end == 0 || end < start) {
            return 0;
        }
        return end - start + 1;
    }

    core::Result<MappedBar*> EnsureBarMapped(uint8_t bar_index) {
        if (bar_index > 5) {
            return core::Result<MappedBar*>::Err(
                core::ErrorCode::kInvalidArgument);
        }
        auto it = mapped_bars.find(bar_index);
        if (it != mapped_bars.end()) {
            return core::Result<MappedBar*>::Ok(&it->second);
        }

        uint64_t size = GetBarSize(bar_index);
        if (size == 0) {
            return core::Result<MappedBar*>::Err(core::ErrorCode::kNotFound);
        }

        std::string resource_path =
            info.sysfs_path + "/resource" + std::to_string(bar_index);
        int fd = ::open(resource_path.c_str(), O_RDWR | O_SYNC);
        if (fd < 0) {
            return core::Result<MappedBar*>::Err(
                core::ErrorCode::kPermissionDenied);
        }

        void* base = ::mmap(nullptr, static_cast<std::size_t>(size),
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (base == MAP_FAILED) {
            ::close(fd);
            return core::Result<MappedBar*>::Err(core::ErrorCode::kIOError);
        }

        MappedBar bar;
        bar.fd = fd;
        bar.base = base;
        bar.size = size;
        auto [inserted, _] = mapped_bars.emplace(bar_index, bar);
        return core::Result<MappedBar*>::Ok(&inserted->second);
    }

    void UnmapAllBars() {
        for (auto& [idx, bar] : mapped_bars) {
            if (bar.base && bar.base != MAP_FAILED) {
                ::munmap(bar.base, static_cast<std::size_t>(bar.size));
            }
            if (bar.fd >= 0) {
                ::close(bar.fd);
            }
        }
        mapped_bars.clear();
    }
};

// ---------------------------------------------------------------------------
// PciDevice — construction / move / destruction
// ---------------------------------------------------------------------------

PciDevice::PciDevice(PciDeviceNode&& info)
    : impl_(std::make_unique<Impl>(std::move(info))) {}

PciDevice::~PciDevice() = default;

PciDevice::PciDevice(PciDevice&& other) noexcept = default;

PciDevice& PciDevice::operator=(PciDevice&& other) noexcept = default;

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

core::Result<PciDevice> PciDevice::Open(const PciAddress& addr) {
    auto info_result = PciTopology::GetDeviceInfo(addr);
    if (info_result.IsError()) {
        return core::Result<PciDevice>::Err(info_result.Error());
    }
    return core::Result<PciDevice>::Ok(
        PciDevice(std::move(info_result.Value())));
}

core::Result<PciDevice> PciDevice::Open(const std::string& addr_str) {
    auto addr_result = PciAddress::FromString(addr_str);
    if (addr_result.IsError()) {
        return core::Result<PciDevice>::Err(addr_result.Error());
    }
    return Open(addr_result.Value());
}

// ---------------------------------------------------------------------------
// Device Info
// ---------------------------------------------------------------------------

const PciAddress& PciDevice::Address() const {
    return impl_->info.address;
}

std::string PciDevice::AddressString() const {
    return impl_->info.address.ToString();
}

PciePortType PciDevice::PortType() const {
    return impl_->info.port_type;
}

bool PciDevice::IsBridge() const {
    return impl_->info.is_bridge;
}

const std::string& PciDevice::SysfsPath() const {
    return impl_->info.sysfs_path;
}

// ---------------------------------------------------------------------------
// Config Space — reads
// ---------------------------------------------------------------------------

core::Result<core::Byte> PciDevice::ReadConfig8(ConfigOffset offset) {
    auto fd_result = impl_->EnsureConfigFd();
    if (fd_result.IsError()) {
        return core::Result<core::Byte>::Err(fd_result.Error());
    }
    core::Byte val = 0;
    auto n = ::pread(impl_->config_fd, &val, sizeof(val), offset);
    if (n != sizeof(val)) {
        return core::Result<core::Byte>::Err(core::ErrorCode::kIOError);
    }
    return core::Result<core::Byte>::Ok(val);
}

core::Result<core::Word> PciDevice::ReadConfig16(ConfigOffset offset) {
    auto fd_result = impl_->EnsureConfigFd();
    if (fd_result.IsError()) {
        return core::Result<core::Word>::Err(fd_result.Error());
    }
    core::Word val = 0;
    auto n = ::pread(impl_->config_fd, &val, sizeof(val), offset);
    if (n != sizeof(val)) {
        return core::Result<core::Word>::Err(core::ErrorCode::kIOError);
    }
    return core::Result<core::Word>::Ok(val);
}

core::Result<core::DWord> PciDevice::ReadConfig32(ConfigOffset offset) {
    auto fd_result = impl_->EnsureConfigFd();
    if (fd_result.IsError()) {
        return core::Result<core::DWord>::Err(fd_result.Error());
    }
    core::DWord val = 0;
    auto n = ::pread(impl_->config_fd, &val, sizeof(val), offset);
    if (n != sizeof(val)) {
        return core::Result<core::DWord>::Err(core::ErrorCode::kIOError);
    }
    return core::Result<core::DWord>::Ok(val);
}

// ---------------------------------------------------------------------------
// Config Space — writes
// ---------------------------------------------------------------------------

core::Result<void> PciDevice::WriteConfig8(ConfigOffset offset,
                                           core::Byte value) {
    auto fd_result = impl_->EnsureConfigFd();
    if (fd_result.IsError()) {
        return core::Result<void>::Err(fd_result.Error());
    }
    auto n = ::pwrite(impl_->config_fd, &value, sizeof(value), offset);
    if (n != sizeof(value)) {
        return core::Result<void>::Err(core::ErrorCode::kIOError);
    }
    return core::Result<void>::Ok();
}

core::Result<void> PciDevice::WriteConfig16(ConfigOffset offset,
                                            core::Word value) {
    auto fd_result = impl_->EnsureConfigFd();
    if (fd_result.IsError()) {
        return core::Result<void>::Err(fd_result.Error());
    }
    auto n = ::pwrite(impl_->config_fd, &value, sizeof(value), offset);
    if (n != sizeof(value)) {
        return core::Result<void>::Err(core::ErrorCode::kIOError);
    }
    return core::Result<void>::Ok();
}

core::Result<void> PciDevice::WriteConfig32(ConfigOffset offset,
                                            core::DWord value) {
    auto fd_result = impl_->EnsureConfigFd();
    if (fd_result.IsError()) {
        return core::Result<void>::Err(fd_result.Error());
    }
    auto n = ::pwrite(impl_->config_fd, &value, sizeof(value), offset);
    if (n != sizeof(value)) {
        return core::Result<void>::Err(core::ErrorCode::kIOError);
    }
    return core::Result<void>::Ok();
}

// ---------------------------------------------------------------------------
// Capability walking
// ---------------------------------------------------------------------------

core::Result<std::optional<ConfigOffset>> PciDevice::FindCapability(
    CapabilityId id) {
    // Check Status register bit 4 (capabilities list)
    auto status_result = ReadConfig16(0x06);
    if (status_result.IsError()) {
        return core::Result<std::optional<ConfigOffset>>::Err(
            status_result.Error());
    }
    if (!(status_result.Value() & (1u << 4))) {
        return core::Result<std::optional<ConfigOffset>>::Ok(std::nullopt);
    }

    // Read capability pointer
    auto cap_ptr_result = ReadConfig8(0x34);
    if (cap_ptr_result.IsError()) {
        return core::Result<std::optional<ConfigOffset>>::Err(
            cap_ptr_result.Error());
    }
    uint8_t offset = cap_ptr_result.Value() & 0xFC;

    constexpr int kMaxCaps = 48;
    for (int i = 0; i < kMaxCaps && offset != 0; ++i) {
        auto cap_id_result = ReadConfig8(offset);
        if (cap_id_result.IsError()) {
            return core::Result<std::optional<ConfigOffset>>::Err(
                cap_id_result.Error());
        }
        if (cap_id_result.Value() == static_cast<uint8_t>(id)) {
            return core::Result<std::optional<ConfigOffset>>::Ok(
                static_cast<ConfigOffset>(offset));
        }
        auto next_result = ReadConfig8(offset + 1);
        if (next_result.IsError()) {
            return core::Result<std::optional<ConfigOffset>>::Err(
                next_result.Error());
        }
        offset = next_result.Value() & 0xFC;
    }

    return core::Result<std::optional<ConfigOffset>>::Ok(std::nullopt);
}

core::Result<std::optional<ConfigOffset>> PciDevice::FindExtCapability(
    ExtCapabilityId id) {
    ConfigOffset offset = 0x100;
    constexpr int kMaxExtCaps = 256;

    for (int i = 0; i < kMaxExtCaps && offset != 0; ++i) {
        auto header_result = ReadConfig32(offset);
        if (header_result.IsError()) {
            return core::Result<std::optional<ConfigOffset>>::Err(
                header_result.Error());
        }
        auto header = header_result.Value();
        uint16_t cap_id = header & 0xFFFF;
        if (cap_id == static_cast<uint16_t>(id)) {
            return core::Result<std::optional<ConfigOffset>>::Ok(offset);
        }
        uint16_t next = (header >> 20) & 0xFFC;
        if (next == 0 || next <= offset) {
            break;
        }
        offset = next;
    }

    return core::Result<std::optional<ConfigOffset>>::Ok(std::nullopt);
}

// ---------------------------------------------------------------------------
// BAR MMIO
// ---------------------------------------------------------------------------

core::Result<core::DWord> PciDevice::BarRead32(uint8_t bar_index,
                                               uint64_t offset) {
    auto bar_result = impl_->EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
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

core::Result<core::QWord> PciDevice::BarRead64(uint8_t bar_index,
                                               uint64_t offset) {
    auto bar_result = impl_->EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
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

core::Result<void> PciDevice::BarWrite32(uint8_t bar_index, uint64_t offset,
                                         core::DWord value) {
    auto bar_result = impl_->EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
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

core::Result<void> PciDevice::BarWrite64(uint8_t bar_index, uint64_t offset,
                                         core::QWord value) {
    auto bar_result = impl_->EnsureBarMapped(bar_index);
    if (bar_result.IsError()) {
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

core::Result<void> PciDevice::BarReadBuffer(uint8_t bar_index, uint64_t offset,
                                            void* buffer,
                                            std::size_t length) {
    if (!buffer || length == 0) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }
    auto bar_result = impl_->EnsureBarMapped(bar_index);
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

core::Result<void> PciDevice::BarWriteBuffer(uint8_t bar_index,
                                             uint64_t offset,
                                             const void* buffer,
                                             std::size_t length) {
    if (!buffer || length == 0) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }
    auto bar_result = impl_->EnsureBarMapped(bar_index);
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
// Topology
// ---------------------------------------------------------------------------

core::Result<std::optional<PciDevice>> PciDevice::FindParent() {
    auto parent_result = PciTopology::FindParent(impl_->info.address);
    if (parent_result.IsError()) {
        return core::Result<std::optional<PciDevice>>::Err(
            parent_result.Error());
    }
    if (!parent_result.Value().has_value()) {
        return core::Result<std::optional<PciDevice>>::Ok(std::nullopt);
    }
    auto dev_result = PciDevice::Open(parent_result.Value().value());
    if (dev_result.IsError()) {
        return core::Result<std::optional<PciDevice>>::Err(
            dev_result.Error());
    }
    return core::Result<std::optional<PciDevice>>::Ok(
        std::move(dev_result.Value()));
}

core::Result<std::vector<PciDevice>> PciDevice::FindChildren() {
    auto children_result = PciTopology::FindChildren(impl_->info.address);
    if (children_result.IsError()) {
        return core::Result<std::vector<PciDevice>>::Err(
            children_result.Error());
    }
    std::vector<PciDevice> devices;
    for (const auto& addr : children_result.Value()) {
        auto dev_result = PciDevice::Open(addr);
        if (dev_result.IsError()) {
            return core::Result<std::vector<PciDevice>>::Err(
                dev_result.Error());
        }
        devices.push_back(std::move(dev_result.Value()));
    }
    return core::Result<std::vector<PciDevice>>::Ok(std::move(devices));
}

core::Result<PciDevice> PciDevice::FindRootPort() {
    auto root_result = PciTopology::FindRootPort(impl_->info.address);
    if (root_result.IsError()) {
        return core::Result<PciDevice>::Err(root_result.Error());
    }
    return PciDevice::Open(root_result.Value());
}

core::Result<std::vector<PciDeviceNode>> PciDevice::GetPathToRoot() {
    return PciTopology::GetPathToRoot(impl_->info.address);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

core::Result<void> PciDevice::Remove() {
    return PciTopology::RemoveDevice(impl_->info.address);
}

core::Result<void> PciDevice::Rescan() {
    return PciTopology::RescanBridge(impl_->info.address);
}

}  // namespace plas::hal::pci
