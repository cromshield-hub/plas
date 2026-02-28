#include <gtest/gtest.h>

#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include "plas/hal/interface/pci/pci_topology.h"

namespace plas::hal::pci {
namespace {

// Helper to create directories recursively
static void MkdirP(const std::string& path) {
    std::string cmd = "mkdir -p \"" + path + "\"";
    int ret = ::system(cmd.c_str());
    (void)ret;
}

// Helper to write content to a file
static void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

// Helper to write binary content to a file
static void WriteBinaryFile(const std::string& path,
                            const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
}

// Helper to read content from a file
static std::string ReadFileContent(const std::string& path) {
    std::ifstream f(path);
    std::string content;
    std::getline(f, content);
    return content;
}

// Build a minimal PCI config space binary blob with:
//  - Status register (offset 0x06) with capabilities list bit set
//  - Header type (offset 0x0E)
//  - Capability pointer (offset 0x34)
//  - PCI Express capability at the pointed offset
static std::vector<uint8_t> BuildConfigBlob(uint8_t header_type,
                                             PciePortType port_type,
                                             bool has_pcie_cap = true) {
    // Create 256-byte config space (minimum)
    std::vector<uint8_t> config(256, 0);

    // Header type at offset 0x0E
    config[0x0E] = header_type;

    if (has_pcie_cap) {
        // Status register at offset 0x06: set capabilities list bit (bit 4)
        config[0x06] = 0x10;

        // Capability pointer at offset 0x34: point to 0x40
        config[0x34] = 0x40;

        // PCI Express capability at 0x40:
        //   +0x00: Capability ID = 0x10 (PCI Express)
        //   +0x01: Next pointer = 0x00 (end of chain)
        //   +0x02: PCIe Capabilities Register (bits [7:4] = port type)
        config[0x40] = 0x10;  // Cap ID = PCI Express
        config[0x41] = 0x00;  // Next cap = none
        uint16_t pcie_caps = static_cast<uint16_t>(
            (static_cast<uint8_t>(port_type) & 0x0F) << 4);
        config[0x42] = static_cast<uint8_t>(pcie_caps & 0xFF);
        config[0x43] = static_cast<uint8_t>((pcie_caps >> 8) & 0xFF);
    }

    return config;
}

class PciTopologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unique temp directory for fake sysfs
        char tmpl[] = "/tmp/plas_test_sysfs_XXXXXX";
        char* dir = ::mkdtemp(tmpl);
        ASSERT_NE(dir, nullptr);
        sysfs_root_ = dir;
        PciTopology::SetSysfsRoot(sysfs_root_);
    }

    void TearDown() override {
        // Clean up temp directory
        std::string cmd = "rm -rf \"" + sysfs_root_ + "\"";
        int ret = ::system(cmd.c_str());
        (void)ret;
        PciTopology::SetSysfsRoot("/sys");
    }

    // Create a fake sysfs device entry with proper topology symlink.
    //
    // topology_segments: list of DDDD:BB:DD.F strings forming the path
    //   from root complex to device. The last element is the target device.
    //
    // Example: {"0000:00:01.0", "0000:01:00.0"} creates:
    //   /tmp/.../devices/pci0000:00/0000:00:01.0/0000:01:00.0  (real path)
    //   /tmp/.../bus/pci/devices/0000:01:00.0 → symlink to real path
    void CreateFakeDevice(const std::vector<std::string>& topology_segments,
                          uint8_t header_type = 0x00,
                          PciePortType port_type = PciePortType::kEndpoint,
                          bool has_pcie_cap = true) {
        ASSERT_FALSE(topology_segments.empty());

        // Extract domain from first segment
        std::string first = topology_segments[0];
        std::string domain_str = first.substr(0, 4);

        // Build real device path:
        // sysfs_root/devices/pciDDDD:BB/seg0/seg1/.../segN
        std::string bus_str = first.substr(5, 2);
        std::string real_path =
            sysfs_root_ + "/devices/pci" + domain_str + ":" + bus_str;
        for (const auto& seg : topology_segments) {
            real_path += "/" + seg;
        }
        MkdirP(real_path);

        // Write config binary
        auto config = BuildConfigBlob(header_type, port_type, has_pcie_cap);
        WriteBinaryFile(real_path + "/config", config);

        // Create symlink:
        // sysfs_root/bus/pci/devices/DDDD:BB:DD.F → real_path
        std::string link_dir = sysfs_root_ + "/bus/pci/devices";
        MkdirP(link_dir);

        const std::string& device_bdf = topology_segments.back();
        std::string link_path = link_dir + "/" + device_bdf;

        // Remove existing symlink if any
        ::unlink(link_path.c_str());
        int rc = ::symlink(real_path.c_str(), link_path.c_str());
        (void)rc;
    }

    // Create all devices along a topology path with proper config blobs.
    // port_types[i] corresponds to topology_segments[i].
    void CreateTopology(
        const std::vector<std::string>& topology_segments,
        const std::vector<std::pair<uint8_t, PciePortType>>& device_types) {
        ASSERT_EQ(topology_segments.size(), device_types.size());

        for (size_t i = 0; i < topology_segments.size(); ++i) {
            // Create device with path from root to this point
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

// ===== PciAddress Tests =====

TEST(PciAddressTest, ToStringBasic) {
    PciAddress addr{0x0000, {0x03, 0x00, 0x00}};
    EXPECT_EQ(addr.ToString(), "0000:03:00.0");
}

TEST(PciAddressTest, ToStringWithDomain) {
    PciAddress addr{0x00AB, {0xFF, 0x1F, 0x07}};
    EXPECT_EQ(addr.ToString(), "00ab:ff:1f.7");
}

TEST(PciAddressTest, FromStringBasic) {
    auto result = PciAddress::FromString("0000:03:00.0");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().domain, 0x0000);
    EXPECT_EQ(result.Value().bdf.bus, 0x03);
    EXPECT_EQ(result.Value().bdf.device, 0x00);
    EXPECT_EQ(result.Value().bdf.function, 0x00);
}

TEST(PciAddressTest, FromStringWithDomain) {
    auto result = PciAddress::FromString("00ab:ff:1f.7");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().domain, 0x00AB);
    EXPECT_EQ(result.Value().bdf.bus, 0xFF);
    EXPECT_EQ(result.Value().bdf.device, 0x1F);
    EXPECT_EQ(result.Value().bdf.function, 0x07);
}

TEST(PciAddressTest, FromStringUpperCase) {
    auto result = PciAddress::FromString("00AB:FF:1F.7");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().domain, 0x00AB);
}

TEST(PciAddressTest, Roundtrip) {
    PciAddress original{0x0000, {0x41, 0x00, 0x00}};
    auto parsed = PciAddress::FromString(original.ToString());
    ASSERT_TRUE(parsed.IsOk());
    EXPECT_EQ(parsed.Value(), original);
}

TEST(PciAddressTest, FromStringInvalid) {
    EXPECT_TRUE(PciAddress::FromString("").IsError());
    EXPECT_TRUE(PciAddress::FromString("invalid").IsError());
    EXPECT_TRUE(PciAddress::FromString("0000:03:00").IsError());
    EXPECT_TRUE(PciAddress::FromString("03:00.0").IsError());
}

TEST(PciAddressTest, FromStringOutOfRange) {
    // Device > 0x1F
    EXPECT_TRUE(PciAddress::FromString("0000:00:20.0").IsError());
    // Function > 0x07
    EXPECT_TRUE(PciAddress::FromString("0000:00:00.8").IsError());
}

TEST(PciAddressTest, Equality) {
    PciAddress a{0x0000, {0x03, 0x00, 0x00}};
    PciAddress b{0x0000, {0x03, 0x00, 0x00}};
    EXPECT_EQ(a, b);
}

TEST(PciAddressTest, InequalityDomain) {
    PciAddress a{0x0000, {0x03, 0x00, 0x00}};
    PciAddress b{0x0001, {0x03, 0x00, 0x00}};
    EXPECT_NE(a, b);
}

TEST(PciAddressTest, InequalityBdf) {
    PciAddress a{0x0000, {0x03, 0x00, 0x00}};
    PciAddress b{0x0000, {0x04, 0x00, 0x00}};
    EXPECT_NE(a, b);
}

// ===== PciePortType Tests =====

TEST(PciePortTypeTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kEndpoint), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kLegacyEndpoint), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kRootPort), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kUpstreamPort), 0x05);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kDownstreamPort), 0x06);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kPcieToPciBridge), 0x07);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kPciToPcieBridge), 0x08);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kRcIntegratedEndpoint), 0x09);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kRcEventCollector), 0x0A);
    EXPECT_EQ(static_cast<uint8_t>(PciePortType::kUnknown), 0xFF);
}

// ===== PciTopology Tests =====

TEST_F(PciTopologyTest, GetSysfsPath) {
    PciAddress addr{0x0000, {0x03, 0x00, 0x00}};
    std::string expected = sysfs_root_ + "/bus/pci/devices/0000:03:00.0";
    EXPECT_EQ(PciTopology::GetSysfsPath(addr), expected);
}

TEST_F(PciTopologyTest, DeviceExistsTrue) {
    CreateFakeDevice({"0000:00:01.0", "0000:03:00.0"});

    PciAddress addr{0x0000, {0x03, 0x00, 0x00}};
    EXPECT_TRUE(PciTopology::DeviceExists(addr));
}

TEST_F(PciTopologyTest, DeviceExistsFalse) {
    PciAddress addr{0x0000, {0x99, 0x00, 0x00}};
    EXPECT_FALSE(PciTopology::DeviceExists(addr));
}

TEST_F(PciTopologyTest, GetDeviceInfoEndpoint) {
    CreateFakeDevice({"0000:00:01.0", "0000:03:00.0"}, 0x00,
                     PciePortType::kEndpoint);

    PciAddress addr{0x0000, {0x03, 0x00, 0x00}};
    auto result = PciTopology::GetDeviceInfo(addr);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().address, addr);
    EXPECT_EQ(result.Value().port_type, PciePortType::kEndpoint);
    EXPECT_FALSE(result.Value().is_bridge);
}

TEST_F(PciTopologyTest, GetDeviceInfoBridge) {
    CreateFakeDevice({"0000:00:01.0"}, 0x01, PciePortType::kRootPort);

    PciAddress addr{0x0000, {0x00, 0x01, 0x00}};
    auto result = PciTopology::GetDeviceInfo(addr);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().port_type, PciePortType::kRootPort);
    EXPECT_TRUE(result.Value().is_bridge);
}

TEST_F(PciTopologyTest, GetDeviceInfoNotFound) {
    PciAddress addr{0x0000, {0x99, 0x00, 0x00}};
    auto result = PciTopology::GetDeviceInfo(addr);
    EXPECT_TRUE(result.IsError());
}

TEST_F(PciTopologyTest, FindParentSimple) {
    // Topology: root_port → endpoint
    CreateFakeDevice({"0000:00:01.0", "0000:03:00.0"});

    PciAddress endpoint{0x0000, {0x03, 0x00, 0x00}};
    auto result = PciTopology::FindParent(endpoint);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(result.Value().has_value());

    PciAddress expected_parent{0x0000, {0x00, 0x01, 0x00}};
    EXPECT_EQ(result.Value().value(), expected_parent);
}

TEST_F(PciTopologyTest, FindParentRootDevice) {
    // Device directly under root complex — no parent
    CreateFakeDevice({"0000:00:01.0"});

    PciAddress root_device{0x0000, {0x00, 0x01, 0x00}};
    auto result = PciTopology::FindParent(root_device);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().has_value());
}

TEST_F(PciTopologyTest, FindParentNotFound) {
    PciAddress nonexistent{0x0000, {0x99, 0x00, 0x00}};
    auto result = PciTopology::FindParent(nonexistent);
    EXPECT_TRUE(result.IsError());
}

TEST_F(PciTopologyTest, FindRootPortMultiLevel) {
    // Topology: root_port → upstream → downstream → endpoint
    std::vector<std::string> segments = {"0000:3a:00.0", "0000:3b:00.0",
                                         "0000:3c:08.0", "0000:41:00.0"};
    std::vector<std::pair<uint8_t, PciePortType>> types = {
        {0x01, PciePortType::kRootPort},
        {0x01, PciePortType::kUpstreamPort},
        {0x01, PciePortType::kDownstreamPort},
        {0x00, PciePortType::kEndpoint},
    };
    CreateTopology(segments, types);

    PciAddress endpoint{0x0000, {0x41, 0x00, 0x00}};
    auto result = PciTopology::FindRootPort(endpoint);
    ASSERT_TRUE(result.IsOk());

    PciAddress expected_root{0x0000, {0x3a, 0x00, 0x00}};
    EXPECT_EQ(result.Value(), expected_root);
}

TEST_F(PciTopologyTest, GetPathToRootMultiLevel) {
    std::vector<std::string> segments = {"0000:3a:00.0", "0000:3b:00.0",
                                         "0000:3c:08.0", "0000:41:00.0"};
    std::vector<std::pair<uint8_t, PciePortType>> types = {
        {0x01, PciePortType::kRootPort},
        {0x01, PciePortType::kUpstreamPort},
        {0x01, PciePortType::kDownstreamPort},
        {0x00, PciePortType::kEndpoint},
    };
    CreateTopology(segments, types);

    PciAddress endpoint{0x0000, {0x41, 0x00, 0x00}};
    auto result = PciTopology::GetPathToRoot(endpoint);
    ASSERT_TRUE(result.IsOk());

    const auto& path = result.Value();
    ASSERT_EQ(path.size(), 4u);

    // Device first, root last
    EXPECT_EQ(path[0].address, (PciAddress{0x0000, {0x41, 0x00, 0x00}}));
    EXPECT_EQ(path[0].port_type, PciePortType::kEndpoint);

    EXPECT_EQ(path[1].address, (PciAddress{0x0000, {0x3c, 0x08, 0x00}}));
    EXPECT_EQ(path[1].port_type, PciePortType::kDownstreamPort);

    EXPECT_EQ(path[2].address, (PciAddress{0x0000, {0x3b, 0x00, 0x00}}));
    EXPECT_EQ(path[2].port_type, PciePortType::kUpstreamPort);

    EXPECT_EQ(path[3].address, (PciAddress{0x0000, {0x3a, 0x00, 0x00}}));
    EXPECT_EQ(path[3].port_type, PciePortType::kRootPort);
}

TEST_F(PciTopologyTest, RemoveDevice) {
    CreateFakeDevice({"0000:00:01.0", "0000:03:00.0"});

    // Create the remove file so we can write to it
    std::string remove_path =
        sysfs_root_ + "/bus/pci/devices/0000:03:00.0";
    char resolved[PATH_MAX];
    char* rp = ::realpath(remove_path.c_str(), resolved);
    ASSERT_NE(rp, nullptr);
    WriteFile(std::string(resolved) + "/remove", "");

    PciAddress addr{0x0000, {0x03, 0x00, 0x00}};
    // RemoveDevice writes to the sysfs_path/remove which is the symlink
    // We need to create the remove file at the real path
    auto result = PciTopology::RemoveDevice(addr);
    ASSERT_TRUE(result.IsOk());

    // Verify "1" was written
    std::string content = ReadFileContent(std::string(resolved) + "/remove");
    EXPECT_EQ(content, "1");
}

TEST_F(PciTopologyTest, RescanBridge) {
    CreateFakeDevice({"0000:00:01.0"}, 0x01, PciePortType::kRootPort);

    // Create the rescan file
    std::string sysfs_path =
        sysfs_root_ + "/bus/pci/devices/0000:00:01.0";
    char resolved[PATH_MAX];
    char* rp = ::realpath(sysfs_path.c_str(), resolved);
    ASSERT_NE(rp, nullptr);
    WriteFile(std::string(resolved) + "/rescan", "");

    PciAddress bridge{0x0000, {0x00, 0x01, 0x00}};
    auto result = PciTopology::RescanBridge(bridge);
    ASSERT_TRUE(result.IsOk());

    std::string content = ReadFileContent(std::string(resolved) + "/rescan");
    EXPECT_EQ(content, "1");
}

TEST_F(PciTopologyTest, RescanAll) {
    // Create the global rescan file
    std::string rescan_dir = sysfs_root_ + "/bus/pci";
    MkdirP(rescan_dir);
    WriteFile(rescan_dir + "/rescan", "");

    auto result = PciTopology::RescanAll();
    ASSERT_TRUE(result.IsOk());

    std::string content = ReadFileContent(rescan_dir + "/rescan");
    EXPECT_EQ(content, "1");
}

TEST_F(PciTopologyTest, RemoveDeviceNotFound) {
    PciAddress addr{0x0000, {0x99, 0x00, 0x00}};
    auto result = PciTopology::RemoveDevice(addr);
    EXPECT_TRUE(result.IsError());
}

TEST_F(PciTopologyTest, GetDeviceInfoNoPcieCap) {
    // Device without PCI Express capability
    CreateFakeDevice({"0000:00:01.0", "0000:03:00.0"}, 0x00,
                     PciePortType::kEndpoint, false);

    PciAddress addr{0x0000, {0x03, 0x00, 0x00}};
    auto result = PciTopology::GetDeviceInfo(addr);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().port_type, PciePortType::kUnknown);
    EXPECT_FALSE(result.Value().is_bridge);
}

TEST_F(PciTopologyTest, FindRootPortDirectChild) {
    // Only root_port → endpoint (no switch)
    std::vector<std::string> segments = {"0000:00:01.0", "0000:01:00.0"};
    std::vector<std::pair<uint8_t, PciePortType>> types = {
        {0x01, PciePortType::kRootPort},
        {0x00, PciePortType::kEndpoint},
    };
    CreateTopology(segments, types);

    PciAddress endpoint{0x0000, {0x01, 0x00, 0x00}};
    auto result = PciTopology::FindRootPort(endpoint);
    ASSERT_TRUE(result.IsOk());

    PciAddress expected_root{0x0000, {0x00, 0x01, 0x00}};
    EXPECT_EQ(result.Value(), expected_root);
}

}  // namespace
}  // namespace plas::hal::pci
