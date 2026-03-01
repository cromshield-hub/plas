#include <gtest/gtest.h>

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "plas/hal/interface/pci/pci_device.h"

namespace plas::hal::pci {
namespace {

// --- Helpers (shared pattern with test_pci_topology.cpp) ---

static void MkdirP(const std::string& path) {
    std::string cmd = "mkdir -p \"" + path + "\"";
    int ret = ::system(cmd.c_str());
    (void)ret;
}

static void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

static void WriteBinaryFile(const std::string& path,
                            const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
}

static std::string ReadFileContent(const std::string& path) {
    std::ifstream f(path);
    std::string content;
    std::getline(f, content);
    return content;
}

// Build a PCI config space binary blob.
// Supports:
//   - Header type at offset 0x0E
//   - PCIe capability with port type
//   - Additional standard capability chain entries
//   - Extended capability chain entries
static std::vector<uint8_t> BuildConfigBlob(
    uint8_t header_type, PciePortType port_type, bool has_pcie_cap = true,
    std::size_t size = 256) {
    std::vector<uint8_t> config(size, 0);

    config[0x0E] = header_type;

    if (has_pcie_cap) {
        // Status register at 0x06: capabilities list bit (bit 4)
        config[0x06] = 0x10;
        // Capability pointer at 0x34: point to 0x40
        config[0x34] = 0x40;
        // PCI Express cap at 0x40
        config[0x40] = 0x10;  // Cap ID = PCI Express
        config[0x41] = 0x00;  // Next cap = none
        uint16_t pcie_caps = static_cast<uint16_t>(
            (static_cast<uint8_t>(port_type) & 0x0F) << 4);
        config[0x42] = static_cast<uint8_t>(pcie_caps & 0xFF);
        config[0x43] = static_cast<uint8_t>((pcie_caps >> 8) & 0xFF);
    }

    return config;
}

// Build config blob with a capability chain: PCIe cap → extra cap
static std::vector<uint8_t> BuildConfigBlobWithCapChain(
    uint8_t header_type, PciePortType port_type,
    uint8_t extra_cap_id, uint8_t extra_cap_offset) {
    auto config = BuildConfigBlob(header_type, port_type, true, 256);
    // Link PCIe cap (at 0x40) → extra cap
    config[0x41] = extra_cap_offset;
    // Extra cap
    config[extra_cap_offset] = extra_cap_id;
    config[extra_cap_offset + 1] = 0x00;  // end of chain
    return config;
}

// Build config blob with extended capability chain (config space ≥ 4096 bytes)
static std::vector<uint8_t> BuildConfigBlobWithExtCap(
    uint8_t header_type, PciePortType port_type,
    uint16_t ext_cap_id, uint16_t ext_cap_offset = 0x100) {
    auto config = BuildConfigBlob(header_type, port_type, true, 4096);
    // Extended cap header at ext_cap_offset: [15:0]=cap_id, [19:16]=version,
    // [31:20]=next
    uint32_t ext_header = static_cast<uint32_t>(ext_cap_id) |
                          (1u << 16);  // version 1, next = 0
    std::memcpy(&config[ext_cap_offset], &ext_header, sizeof(ext_header));
    return config;
}

class PciDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        char tmpl[] = "/tmp/plas_test_pcidev_XXXXXX";
        char* dir = ::mkdtemp(tmpl);
        ASSERT_NE(dir, nullptr);
        sysfs_root_ = dir;
        PciTopology::SetSysfsRoot(sysfs_root_);
    }

    void TearDown() override {
        std::string cmd = "rm -rf \"" + sysfs_root_ + "\"";
        int ret = ::system(cmd.c_str());
        (void)ret;
        PciTopology::SetSysfsRoot("/sys");
    }

    // Create a fake sysfs device with the given config blob.
    void CreateFakeDevice(const std::vector<std::string>& topology_segments,
                          const std::vector<uint8_t>& config_blob) {
        ASSERT_FALSE(topology_segments.empty());

        std::string first = topology_segments[0];
        std::string domain_str = first.substr(0, 4);
        std::string bus_str = first.substr(5, 2);
        std::string real_path =
            sysfs_root_ + "/devices/pci" + domain_str + ":" + bus_str;
        for (const auto& seg : topology_segments) {
            real_path += "/" + seg;
        }
        MkdirP(real_path);

        WriteBinaryFile(real_path + "/config", config_blob);

        std::string link_dir = sysfs_root_ + "/bus/pci/devices";
        MkdirP(link_dir);

        const std::string& device_bdf = topology_segments.back();
        std::string link_path = link_dir + "/" + device_bdf;
        ::unlink(link_path.c_str());
        int rc = ::symlink(real_path.c_str(), link_path.c_str());
        (void)rc;
    }

    // Convenience: create with auto-generated config blob.
    void CreateFakeDevice(const std::vector<std::string>& topology_segments,
                          uint8_t header_type = 0x00,
                          PciePortType port_type = PciePortType::kEndpoint,
                          bool has_pcie_cap = true) {
        auto config = BuildConfigBlob(header_type, port_type, has_pcie_cap);
        CreateFakeDevice(topology_segments, config);
    }

    // Create a fake BAR resource file and resource<N> file for mmap.
    void CreateFakeBar(const std::string& device_bdf,
                       uint8_t bar_index, uint64_t size) {
        // Resolve real path from symlink
        std::string link_path =
            sysfs_root_ + "/bus/pci/devices/" + device_bdf;
        char resolved[PATH_MAX];
        ASSERT_NE(::realpath(link_path.c_str(), resolved), nullptr);
        std::string real_path(resolved);

        // Write resource file (one line per BAR, format: start end flags)
        std::string resource_path = real_path + "/resource";
        std::ofstream resource(resource_path);
        for (uint8_t i = 0; i <= 5; ++i) {
            if (i == bar_index) {
                // start=0, end=size-1, flags=0
                char line[128];
                std::snprintf(line, sizeof(line),
                              "0x%016llx 0x%016llx 0x%016x",
                              0ULL, static_cast<unsigned long long>(size - 1),
                              0x00040200);
                resource << line << "\n";
            } else {
                resource << "0x0000000000000000 0x0000000000000000 "
                            "0x0000000000000000\n";
            }
        }
        resource.close();

        // Create resourceN file with the right size (truncate to fill with
        // zeros). We use ftruncate on a real file so mmap works.
        std::string res_file =
            real_path + "/resource" + std::to_string(bar_index);
        int fd = ::open(res_file.c_str(), O_CREAT | O_RDWR, 0644);
        ASSERT_GE(fd, 0);
        ASSERT_EQ(::ftruncate(fd, static_cast<off_t>(size)), 0);
        ::close(fd);
    }

    void CreateTopology(
        const std::vector<std::string>& topology_segments,
        const std::vector<std::pair<uint8_t, PciePortType>>& device_types) {
        ASSERT_EQ(topology_segments.size(), device_types.size());
        for (size_t i = 0; i < topology_segments.size(); ++i) {
            std::vector<std::string> path_to_device(
                topology_segments.begin(),
                topology_segments.begin() +
                    static_cast<std::ptrdiff_t>(i + 1));
            CreateFakeDevice(path_to_device, device_types[i].first,
                             device_types[i].second);
        }
    }

    std::string sysfs_root_;
};

// ===== Open Tests =====

TEST_F(PciDeviceTest, OpenValidAddress) {
    CreateFakeDevice({"0000:00:01.0", "0000:03:00.0"}, 0x00,
                     PciePortType::kEndpoint);

    PciAddress addr{0x0000, {0x03, 0x00, 0x00}};
    auto result = PciDevice::Open(addr);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().Address(), addr);
}

TEST_F(PciDeviceTest, OpenFromString) {
    CreateFakeDevice({"0000:00:01.0", "0000:03:00.0"}, 0x00,
                     PciePortType::kEndpoint);

    auto result = PciDevice::Open("0000:03:00.0");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().AddressString(), "0000:03:00.0");
}

TEST_F(PciDeviceTest, OpenNonexistent) {
    auto result = PciDevice::Open("0000:99:00.0");
    EXPECT_TRUE(result.IsError());
}

TEST_F(PciDeviceTest, OpenInvalidString) {
    auto result = PciDevice::Open("invalid");
    EXPECT_TRUE(result.IsError());
}

// ===== DeviceInfo Tests =====

TEST_F(PciDeviceTest, DeviceInfoEndpoint) {
    CreateFakeDevice({"0000:00:01.0", "0000:03:00.0"}, 0x00,
                     PciePortType::kEndpoint);

    auto dev = PciDevice::Open("0000:03:00.0");
    ASSERT_TRUE(dev.IsOk());
    EXPECT_EQ(dev.Value().PortType(), PciePortType::kEndpoint);
    EXPECT_FALSE(dev.Value().IsBridge());
    EXPECT_FALSE(dev.Value().SysfsPath().empty());
}

TEST_F(PciDeviceTest, DeviceInfoBridge) {
    CreateFakeDevice({"0000:00:01.0"}, 0x01, PciePortType::kRootPort);

    auto dev = PciDevice::Open("0000:00:01.0");
    ASSERT_TRUE(dev.IsOk());
    EXPECT_EQ(dev.Value().PortType(), PciePortType::kRootPort);
    EXPECT_TRUE(dev.Value().IsBridge());
}

// ===== Config Read Tests =====

TEST_F(PciDeviceTest, ReadConfig8) {
    // Put a known value at offset 0x0E (header type)
    auto config = BuildConfigBlob(0x01, PciePortType::kRootPort);
    CreateFakeDevice({"0000:00:01.0"}, config);

    auto dev = PciDevice::Open("0000:00:01.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().ReadConfig8(0x0E);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0x01);
}

TEST_F(PciDeviceTest, ReadConfig16) {
    // Status register at 0x06 has 0x10 (cap bit)
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().ReadConfig16(0x06);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value() & 0x10, 0x10);
}

TEST_F(PciDeviceTest, ReadConfig32) {
    // Read 32 bits spanning cap pointer area
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint);
    // Write a known pattern at offset 0x00 (vendor + device ID)
    config[0x00] = 0x86;
    config[0x01] = 0x80;
    config[0x02] = 0x00;
    config[0x03] = 0xA0;
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().ReadConfig32(0x00);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0xA0008086u);
}

// ===== Config Write Tests =====

TEST_F(PciDeviceTest, WriteConfig8ReadBack) {
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint, true, 256);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    // Write to a safe offset (e.g., 0x3C = interrupt line)
    auto wr = dev.Value().WriteConfig8(0x3C, 0x0A);
    ASSERT_TRUE(wr.IsOk());

    auto rd = dev.Value().ReadConfig8(0x3C);
    ASSERT_TRUE(rd.IsOk());
    EXPECT_EQ(rd.Value(), 0x0A);
}

TEST_F(PciDeviceTest, WriteConfig16ReadBack) {
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint, true, 256);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto wr = dev.Value().WriteConfig16(0x04, 0x0147);
    ASSERT_TRUE(wr.IsOk());

    auto rd = dev.Value().ReadConfig16(0x04);
    ASSERT_TRUE(rd.IsOk());
    EXPECT_EQ(rd.Value(), 0x0147);
}

TEST_F(PciDeviceTest, WriteConfig32ReadBack) {
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint, true, 256);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto wr = dev.Value().WriteConfig32(0x10, 0xDEADBEEF);
    ASSERT_TRUE(wr.IsOk());

    auto rd = dev.Value().ReadConfig32(0x10);
    ASSERT_TRUE(rd.IsOk());
    EXPECT_EQ(rd.Value(), 0xDEADBEEF);
}

// ===== Capability Walking Tests =====

TEST_F(PciDeviceTest, FindCapabilityPciExpress) {
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().FindCapability(CapabilityId::kPciExpress);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(result.Value().has_value());
    EXPECT_EQ(result.Value().value(), 0x40);
}

TEST_F(PciDeviceTest, FindCapabilityNotPresent) {
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().FindCapability(CapabilityId::kMsi);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().has_value());
}

TEST_F(PciDeviceTest, FindCapabilityChain) {
    // PCIe cap at 0x40, then MSI at 0x50
    auto config = BuildConfigBlobWithCapChain(
        0x00, PciePortType::kEndpoint,
        static_cast<uint8_t>(CapabilityId::kMsi), 0x50);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().FindCapability(CapabilityId::kMsi);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(result.Value().has_value());
    EXPECT_EQ(result.Value().value(), 0x50);
}

TEST_F(PciDeviceTest, FindCapabilityNoCaps) {
    // No capabilities (status bit not set)
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint, false);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().FindCapability(CapabilityId::kPciExpress);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().has_value());
}

TEST_F(PciDeviceTest, FindExtCapability) {
    auto config = BuildConfigBlobWithExtCap(
        0x00, PciePortType::kEndpoint,
        static_cast<uint16_t>(ExtCapabilityId::kDoe), 0x100);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().FindExtCapability(ExtCapabilityId::kDoe);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(result.Value().has_value());
    EXPECT_EQ(result.Value().value(), 0x100);
}

TEST_F(PciDeviceTest, FindExtCapabilityNotPresent) {
    // Config space too small for extended caps (only 256 bytes)
    auto config = BuildConfigBlob(0x00, PciePortType::kEndpoint, true, 256);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, config);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    // Read at 0x100 will fail (file too short), so ReadConfig32 returns error
    // or reads zeros. Either way, ext cap won't be found.
    auto result = dev.Value().FindExtCapability(ExtCapabilityId::kDoe);
    // Result depends on pread behavior at EOF — either error or nullopt
    if (result.IsOk()) {
        EXPECT_FALSE(result.Value().has_value());
    }
}

// ===== BAR MMIO Tests =====

TEST_F(PciDeviceTest, BarRead32Write32) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);
    CreateFakeBar("0000:01:00.0", 0, 4096);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto wr = dev.Value().BarWrite32(0, 0x100, 0xCAFEBABE);
    ASSERT_TRUE(wr.IsOk());

    auto rd = dev.Value().BarRead32(0, 0x100);
    ASSERT_TRUE(rd.IsOk());
    EXPECT_EQ(rd.Value(), 0xCAFEBABE);
}

TEST_F(PciDeviceTest, BarRead64Write64) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);
    CreateFakeBar("0000:01:00.0", 0, 4096);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto wr = dev.Value().BarWrite64(0, 0x200, 0x0123456789ABCDEFULL);
    ASSERT_TRUE(wr.IsOk());

    auto rd = dev.Value().BarRead64(0, 0x200);
    ASSERT_TRUE(rd.IsOk());
    EXPECT_EQ(rd.Value(), 0x0123456789ABCDEFULL);
}

TEST_F(PciDeviceTest, BarReadWriteBuffer) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);
    CreateFakeBar("0000:01:00.0", 0, 4096);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    uint8_t write_buf[16] = {0, 1, 2, 3, 4, 5, 6, 7,
                             8, 9, 10, 11, 12, 13, 14, 15};
    auto wr = dev.Value().BarWriteBuffer(0, 0x300, write_buf, sizeof(write_buf));
    ASSERT_TRUE(wr.IsOk());

    uint8_t read_buf[16] = {};
    auto rd = dev.Value().BarReadBuffer(0, 0x300, read_buf, sizeof(read_buf));
    ASSERT_TRUE(rd.IsOk());
    EXPECT_EQ(std::memcmp(write_buf, read_buf, sizeof(write_buf)), 0);
}

TEST_F(PciDeviceTest, BarOutOfRange) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);
    CreateFakeBar("0000:01:00.0", 0, 4096);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    // Read at offset that exceeds BAR size
    auto rd = dev.Value().BarRead32(0, 4096);
    EXPECT_TRUE(rd.IsError());
}

TEST_F(PciDeviceTest, BarInvalidIndex) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto rd = dev.Value().BarRead32(6, 0);
    EXPECT_TRUE(rd.IsError());
}

TEST_F(PciDeviceTest, BarBufferNullptr) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);
    CreateFakeBar("0000:01:00.0", 0, 4096);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto rd = dev.Value().BarReadBuffer(0, 0, nullptr, 16);
    EXPECT_TRUE(rd.IsError());
}

// ===== Topology Tests =====

TEST_F(PciDeviceTest, FindParent) {
    CreateTopology(
        {"0000:00:01.0", "0000:01:00.0"},
        {{0x01, PciePortType::kRootPort}, {0x00, PciePortType::kEndpoint}});

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto parent = dev.Value().FindParent();
    ASSERT_TRUE(parent.IsOk());
    ASSERT_TRUE(parent.Value().has_value());
    EXPECT_EQ(parent.Value()->AddressString(), "0000:00:01.0");
    EXPECT_EQ(parent.Value()->PortType(), PciePortType::kRootPort);
}

TEST_F(PciDeviceTest, FindParentNone) {
    CreateFakeDevice({"0000:00:01.0"}, 0x01, PciePortType::kRootPort);

    auto dev = PciDevice::Open("0000:00:01.0");
    ASSERT_TRUE(dev.IsOk());

    auto parent = dev.Value().FindParent();
    ASSERT_TRUE(parent.IsOk());
    EXPECT_FALSE(parent.Value().has_value());
}

TEST_F(PciDeviceTest, FindChildrenMultiple) {
    CreateFakeDevice({"0000:00:01.0"}, 0x01, PciePortType::kRootPort);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.1"}, 0x00,
                     PciePortType::kEndpoint);

    auto dev = PciDevice::Open("0000:00:01.0");
    ASSERT_TRUE(dev.IsOk());

    auto children = dev.Value().FindChildren();
    ASSERT_TRUE(children.IsOk());
    ASSERT_EQ(children.Value().size(), 2u);

    // Sort for deterministic comparison
    auto& c = children.Value();
    std::sort(c.begin(), c.end(),
              [](const PciDevice& a, const PciDevice& b) {
                  return a.AddressString() < b.AddressString();
              });
    EXPECT_EQ(c[0].AddressString(), "0000:01:00.0");
    EXPECT_EQ(c[1].AddressString(), "0000:01:00.1");
}

TEST_F(PciDeviceTest, FindChildrenEmpty) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto children = dev.Value().FindChildren();
    ASSERT_TRUE(children.IsOk());
    EXPECT_TRUE(children.Value().empty());
}

TEST_F(PciDeviceTest, FindRootPort) {
    CreateTopology(
        {"0000:3a:00.0", "0000:3b:00.0", "0000:3c:08.0", "0000:41:00.0"},
        {{0x01, PciePortType::kRootPort},
         {0x01, PciePortType::kUpstreamPort},
         {0x01, PciePortType::kDownstreamPort},
         {0x00, PciePortType::kEndpoint}});

    auto dev = PciDevice::Open("0000:41:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto root = dev.Value().FindRootPort();
    ASSERT_TRUE(root.IsOk());
    EXPECT_EQ(root.Value().AddressString(), "0000:3a:00.0");
    EXPECT_EQ(root.Value().PortType(), PciePortType::kRootPort);
}

TEST_F(PciDeviceTest, GetPathToRoot) {
    CreateTopology(
        {"0000:3a:00.0", "0000:3b:00.0", "0000:41:00.0"},
        {{0x01, PciePortType::kRootPort},
         {0x01, PciePortType::kUpstreamPort},
         {0x00, PciePortType::kEndpoint}});

    auto dev = PciDevice::Open("0000:41:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto path = dev.Value().GetPathToRoot();
    ASSERT_TRUE(path.IsOk());
    ASSERT_EQ(path.Value().size(), 3u);
    EXPECT_EQ(path.Value()[0].address,
              (PciAddress{0x0000, {0x41, 0x00, 0x00}}));
    EXPECT_EQ(path.Value()[2].address,
              (PciAddress{0x0000, {0x3a, 0x00, 0x00}}));
}

// ===== Lifecycle Tests =====

TEST_F(PciDeviceTest, Remove) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"});

    // Create the remove file at real path
    std::string link = sysfs_root_ + "/bus/pci/devices/0000:01:00.0";
    char resolved[PATH_MAX];
    ASSERT_NE(::realpath(link.c_str(), resolved), nullptr);
    WriteFile(std::string(resolved) + "/remove", "");

    auto dev = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().Remove();
    ASSERT_TRUE(result.IsOk());

    EXPECT_EQ(ReadFileContent(std::string(resolved) + "/remove"), "1");
}

TEST_F(PciDeviceTest, Rescan) {
    CreateFakeDevice({"0000:00:01.0"}, 0x01, PciePortType::kRootPort);

    std::string link = sysfs_root_ + "/bus/pci/devices/0000:00:01.0";
    char resolved[PATH_MAX];
    ASSERT_NE(::realpath(link.c_str(), resolved), nullptr);
    WriteFile(std::string(resolved) + "/rescan", "");

    auto dev = PciDevice::Open("0000:00:01.0");
    ASSERT_TRUE(dev.IsOk());

    auto result = dev.Value().Rescan();
    ASSERT_TRUE(result.IsOk());

    EXPECT_EQ(ReadFileContent(std::string(resolved) + "/rescan"), "1");
}

// ===== Move Semantics Tests =====

TEST_F(PciDeviceTest, MoveConstruction) {
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);

    auto result = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(result.IsOk());

    PciDevice moved(std::move(result.Value()));
    EXPECT_EQ(moved.AddressString(), "0000:01:00.0");
    EXPECT_EQ(moved.PortType(), PciePortType::kEndpoint);
}

TEST_F(PciDeviceTest, MoveAssignment) {
    CreateFakeDevice({"0000:00:01.0"}, 0x01, PciePortType::kRootPort);
    CreateFakeDevice({"0000:00:01.0", "0000:01:00.0"}, 0x00,
                     PciePortType::kEndpoint);

    auto dev1 = PciDevice::Open("0000:00:01.0");
    auto dev2 = PciDevice::Open("0000:01:00.0");
    ASSERT_TRUE(dev1.IsOk());
    ASSERT_TRUE(dev2.IsOk());

    dev1.Value() = std::move(dev2.Value());
    EXPECT_EQ(dev1.Value().AddressString(), "0000:01:00.0");
}

}  // namespace
}  // namespace plas::hal::pci
