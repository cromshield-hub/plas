#include "plas/hal/interface/pci/pci_topology.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>

#include <dirent.h>
#include <sys/stat.h>

#include "plas/core/error.h"

namespace plas::hal::pci {

// --- PciAddress implementation ---

std::string PciAddress::ToString() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04x:%02x:%02x.%x", domain, bdf.bus,
                  bdf.device & 0x1F, bdf.function & 0x07);
    return buf;
}

core::Result<PciAddress> PciAddress::FromString(const std::string& str) {
    unsigned int domain_val = 0;
    unsigned int bus_val = 0;
    unsigned int dev_val = 0;
    unsigned int func_val = 0;

    if (std::sscanf(str.c_str(), "%x:%x:%x.%x", &domain_val, &bus_val,
                    &dev_val, &func_val) != 4) {
        return core::Result<PciAddress>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (domain_val > 0xFFFF || bus_val > 0xFF || dev_val > 0x1F ||
        func_val > 0x07) {
        return core::Result<PciAddress>::Err(core::ErrorCode::kOutOfRange);
    }

    PciAddress addr{};
    addr.domain = static_cast<uint16_t>(domain_val);
    addr.bdf.bus = static_cast<uint8_t>(bus_val);
    addr.bdf.device = static_cast<uint8_t>(dev_val);
    addr.bdf.function = static_cast<uint8_t>(func_val);
    return core::Result<PciAddress>::Ok(addr);
}

// --- PciTopology static member ---

std::string PciTopology::sysfs_root_ = "/sys";

// --- Private helpers ---

core::Result<std::string> PciTopology::ReadSysfsFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return core::Result<std::string>::Err(core::ErrorCode::kIOError);
    }
    std::string content;
    std::getline(file, content);
    if (file.bad()) {
        return core::Result<std::string>::Err(core::ErrorCode::kIOError);
    }
    return core::Result<std::string>::Ok(std::move(content));
}

core::Result<void> PciTopology::WriteSysfsFile(const std::string& path,
                                                const std::string& value) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return core::Result<void>::Err(core::ErrorCode::kIOError);
    }
    file << value;
    if (file.bad()) {
        return core::Result<void>::Err(core::ErrorCode::kIOError);
    }
    return core::Result<void>::Ok();
}

PciePortType PciTopology::ReadPortType(const std::string& sysfs_device_path) {
    // Read PCI config space binary from sysfs
    std::string config_path = sysfs_device_path + "/config";
    std::ifstream config(config_path, std::ios::binary);
    if (!config.is_open()) {
        return PciePortType::kUnknown;
    }

    // Check Status register (offset 0x06) for capabilities list bit
    config.seekg(0x06);
    uint16_t status = 0;
    config.read(reinterpret_cast<char*>(&status), sizeof(status));
    if (!config.good()) {
        return PciePortType::kUnknown;
    }

    // Bit 4 of Status register: Capabilities List
    if (!(status & (1 << 4))) {
        return PciePortType::kUnknown;
    }

    // Read capability pointer (offset 0x34)
    config.seekg(0x34);
    uint8_t cap_ptr = 0;
    config.read(reinterpret_cast<char*>(&cap_ptr), sizeof(cap_ptr));
    if (!config.good()) {
        return PciePortType::kUnknown;
    }

    // Walk capability chain looking for PCI Express (ID=0x10)
    int max_caps = 48;  // Safety limit
    while (cap_ptr >= 0x40 && max_caps-- > 0) {
        // Align to dword boundary
        cap_ptr &= 0xFC;

        config.seekg(cap_ptr);
        uint8_t cap_id = 0;
        config.read(reinterpret_cast<char*>(&cap_id), sizeof(cap_id));
        if (!config.good()) {
            return PciePortType::kUnknown;
        }

        if (cap_id == static_cast<uint8_t>(CapabilityId::kPciExpress)) {
            // PCIe Capabilities Register is at cap_ptr + 0x02
            config.seekg(cap_ptr + 0x02);
            uint16_t pcie_caps = 0;
            config.read(reinterpret_cast<char*>(&pcie_caps), sizeof(pcie_caps));
            if (!config.good()) {
                return PciePortType::kUnknown;
            }
            // Bits [7:4] = Device/Port Type
            uint8_t port_type = (pcie_caps >> 4) & 0x0F;
            switch (port_type) {
                case 0x00: return PciePortType::kEndpoint;
                case 0x01: return PciePortType::kLegacyEndpoint;
                case 0x04: return PciePortType::kRootPort;
                case 0x05: return PciePortType::kUpstreamPort;
                case 0x06: return PciePortType::kDownstreamPort;
                case 0x07: return PciePortType::kPcieToPciBridge;
                case 0x08: return PciePortType::kPciToPcieBridge;
                case 0x09: return PciePortType::kRcIntegratedEndpoint;
                case 0x0A: return PciePortType::kRcEventCollector;
                default: return PciePortType::kUnknown;
            }
        }

        // Next capability: offset + 1
        config.seekg(cap_ptr + 1);
        config.read(reinterpret_cast<char*>(&cap_ptr), sizeof(cap_ptr));
        if (!config.good()) {
            return PciePortType::kUnknown;
        }
    }

    return PciePortType::kUnknown;
}

bool PciTopology::ReadIsBridge(const std::string& sysfs_device_path) {
    std::string config_path = sysfs_device_path + "/config";
    std::ifstream config(config_path, std::ios::binary);
    if (!config.is_open()) {
        return false;
    }

    // Header Type register at offset 0x0E
    config.seekg(0x0E);
    uint8_t header_type = 0;
    config.read(reinterpret_cast<char*>(&header_type), sizeof(header_type));
    if (!config.good()) {
        return false;
    }

    // Bits [6:0] = header type (mask out multi-function bit 7)
    return (header_type & 0x7F) == 0x01;
}

core::Result<std::vector<PciAddress>> PciTopology::ParseTopologyPath(
    const std::string& real_path) {
    // Pattern: DDDD:BB:DD.F
    static const std::regex bdf_pattern(
        "[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\\.[0-7]",
        std::regex::icase);

    std::vector<PciAddress> addresses;

    // Split path by '/'
    std::istringstream stream(real_path);
    std::string segment;
    while (std::getline(stream, segment, '/')) {
        if (segment.empty()) continue;
        if (std::regex_match(segment, bdf_pattern)) {
            auto result = PciAddress::FromString(segment);
            if (result.IsOk()) {
                addresses.push_back(result.Value());
            }
        }
    }

    if (addresses.empty()) {
        return core::Result<std::vector<PciAddress>>::Err(
            core::ErrorCode::kNotFound);
    }

    return core::Result<std::vector<PciAddress>>::Ok(std::move(addresses));
}

// --- Public API ---

void PciTopology::SetSysfsRoot(const std::string& root) {
    sysfs_root_ = root;
}

const std::string& PciTopology::GetSysfsRoot() {
    return sysfs_root_;
}

std::string PciTopology::GetSysfsPath(const PciAddress& addr) {
    return sysfs_root_ + "/bus/pci/devices/" + addr.ToString();
}

bool PciTopology::DeviceExists(const PciAddress& addr) {
    struct stat st;
    return ::stat(GetSysfsPath(addr).c_str(), &st) == 0;
}

core::Result<PciDeviceNode> PciTopology::GetDeviceInfo(
    const PciAddress& addr) {
    std::string sysfs_path = GetSysfsPath(addr);
    struct stat st;
    if (::stat(sysfs_path.c_str(), &st) != 0) {
        return core::Result<PciDeviceNode>::Err(core::ErrorCode::kNotFound);
    }

    PciDeviceNode node{};
    node.address = addr;
    node.sysfs_path = sysfs_path;
    node.port_type = ReadPortType(sysfs_path);
    node.is_bridge = ReadIsBridge(sysfs_path);
    return core::Result<PciDeviceNode>::Ok(std::move(node));
}

core::Result<std::optional<PciAddress>> PciTopology::FindParent(
    const PciAddress& addr) {
    std::string sysfs_path = GetSysfsPath(addr);

    // Resolve symlink to get the real device path
    char resolved[PATH_MAX];
    if (::realpath(sysfs_path.c_str(), resolved) == nullptr) {
        return core::Result<std::optional<PciAddress>>::Err(
            core::ErrorCode::kNotFound);
    }

    auto parse_result = ParseTopologyPath(resolved);
    if (parse_result.IsError()) {
        return core::Result<std::optional<PciAddress>>::Err(
            parse_result.Error());
    }

    const auto& path_addrs = parse_result.Value();
    // The last element should be our device; the one before it is the parent
    if (path_addrs.size() < 2) {
        // No parent — this device is directly under the root complex
        return core::Result<std::optional<PciAddress>>::Ok(std::nullopt);
    }

    // Second-to-last is the parent
    return core::Result<std::optional<PciAddress>>::Ok(
        path_addrs[path_addrs.size() - 2]);
}

core::Result<std::vector<PciAddress>> PciTopology::FindChildren(
    const PciAddress& bridge_addr) {
    std::string sysfs_path = GetSysfsPath(bridge_addr);

    // Check that the device exists
    struct stat st;
    if (::stat(sysfs_path.c_str(), &st) != 0) {
        return core::Result<std::vector<PciAddress>>::Err(
            core::ErrorCode::kNotFound);
    }

    // Pattern: DDDD:BB:DD.F
    static const std::regex bdf_pattern(
        "[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\\.[0-7]",
        std::regex::icase);

    std::vector<PciAddress> children;

    DIR* dir = ::opendir(sysfs_path.c_str());
    if (dir == nullptr) {
        // Device exists but directory can't be opened — return empty
        return core::Result<std::vector<PciAddress>>::Ok(std::move(children));
    }

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (std::regex_match(name, bdf_pattern)) {
            auto result = PciAddress::FromString(name);
            if (result.IsOk()) {
                children.push_back(result.Value());
            }
        }
    }
    ::closedir(dir);

    return core::Result<std::vector<PciAddress>>::Ok(std::move(children));
}

core::Result<PciAddress> PciTopology::FindRootPort(const PciAddress& addr) {
    std::string sysfs_path = GetSysfsPath(addr);

    char resolved[PATH_MAX];
    if (::realpath(sysfs_path.c_str(), resolved) == nullptr) {
        return core::Result<PciAddress>::Err(core::ErrorCode::kNotFound);
    }

    auto parse_result = ParseTopologyPath(resolved);
    if (parse_result.IsError()) {
        return core::Result<PciAddress>::Err(parse_result.Error());
    }

    const auto& path_addrs = parse_result.Value();
    if (path_addrs.size() < 2) {
        // Device itself might be directly under root complex
        return core::Result<PciAddress>::Err(core::ErrorCode::kNotFound);
    }

    // Walk from the root end to find the first root port
    for (size_t i = 0; i < path_addrs.size() - 1; ++i) {
        std::string dev_sysfs = GetSysfsPath(path_addrs[i]);
        if (ReadPortType(dev_sysfs) == PciePortType::kRootPort) {
            return core::Result<PciAddress>::Ok(path_addrs[i]);
        }
    }

    // If no explicit root port type found, return the first device in the path
    // (topmost bridge under root complex)
    return core::Result<PciAddress>::Ok(path_addrs[0]);
}

core::Result<std::vector<PciDeviceNode>> PciTopology::GetPathToRoot(
    const PciAddress& addr) {
    std::string sysfs_path = GetSysfsPath(addr);

    char resolved[PATH_MAX];
    if (::realpath(sysfs_path.c_str(), resolved) == nullptr) {
        return core::Result<std::vector<PciDeviceNode>>::Err(
            core::ErrorCode::kNotFound);
    }

    auto parse_result = ParseTopologyPath(resolved);
    if (parse_result.IsError()) {
        return core::Result<std::vector<PciDeviceNode>>::Err(
            parse_result.Error());
    }

    const auto& path_addrs = parse_result.Value();
    // Build node list from device (last) to root (first)
    std::vector<PciDeviceNode> nodes;
    nodes.reserve(path_addrs.size());

    for (auto it = path_addrs.rbegin(); it != path_addrs.rend(); ++it) {
        std::string dev_sysfs = GetSysfsPath(*it);
        PciDeviceNode node{};
        node.address = *it;
        node.sysfs_path = dev_sysfs;
        node.port_type = ReadPortType(dev_sysfs);
        node.is_bridge = ReadIsBridge(dev_sysfs);
        nodes.push_back(std::move(node));
    }

    return core::Result<std::vector<PciDeviceNode>>::Ok(std::move(nodes));
}

core::Result<void> PciTopology::RemoveDevice(const PciAddress& addr) {
    std::string remove_path = GetSysfsPath(addr) + "/remove";
    return WriteSysfsFile(remove_path, "1");
}

core::Result<void> PciTopology::RescanBridge(const PciAddress& bridge_addr) {
    std::string rescan_path = GetSysfsPath(bridge_addr) + "/rescan";
    return WriteSysfsFile(rescan_path, "1");
}

core::Result<void> PciTopology::RescanAll() {
    std::string rescan_path = sysfs_root_ + "/bus/pci/rescan";
    return WriteSysfsFile(rescan_path, "1");
}

}  // namespace plas::hal::pci
