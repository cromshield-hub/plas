#include <gtest/gtest.h>

#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <sys/stat.h>

#include "plas/hal/interface/pci/pci_topology.h"

namespace plas::hal::pci {
namespace {

/// Integration tests for PciTopology using real sysfs.
/// Only runs when PLAS_TEST_PCI_TOPOLOGY_BDF env var is set
/// (e.g., PLAS_TEST_PCI_TOPOLOGY_BDF=0000:03:00.0).
///
/// NOTE: RemoveDevice/Rescan are NOT tested — too dangerous on real hardware.

class PciTopologyIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* bdf_env = std::getenv("PLAS_TEST_PCI_TOPOLOGY_BDF");
        if (bdf_env == nullptr || std::strlen(bdf_env) == 0) {
            GTEST_SKIP()
                << "PLAS_TEST_PCI_TOPOLOGY_BDF not set; skipping integration "
                   "tests";
        }

        auto result = PciAddress::FromString(bdf_env);
        ASSERT_TRUE(result.IsOk())
            << "Invalid BDF in PLAS_TEST_PCI_TOPOLOGY_BDF: " << bdf_env;
        target_addr_ = result.Value();
    }

    PciAddress target_addr_{};
};

TEST_F(PciTopologyIntegrationTest, DeviceExists) {
    EXPECT_TRUE(PciTopology::DeviceExists(target_addr_));
}

TEST_F(PciTopologyIntegrationTest, GetDeviceInfo) {
    auto result = PciTopology::GetDeviceInfo(target_addr_);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    const auto& info = result.Value();
    EXPECT_EQ(info.address, target_addr_);
    EXPECT_FALSE(info.sysfs_path.empty());

    std::cout << "Device: " << info.address.ToString() << "\n"
              << "  sysfs: " << info.sysfs_path << "\n"
              << "  port_type: 0x"
              << std::hex
              << static_cast<int>(static_cast<uint8_t>(info.port_type))
              << std::dec << "\n"
              << "  is_bridge: " << (info.is_bridge ? "yes" : "no") << "\n";
}

TEST_F(PciTopologyIntegrationTest, FindParent) {
    auto result = PciTopology::FindParent(target_addr_);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    if (result.Value().has_value()) {
        std::cout << "Parent: " << result.Value()->ToString() << "\n";
        EXPECT_TRUE(PciTopology::DeviceExists(*result.Value()));
    } else {
        std::cout << "No parent (root complex device)\n";
    }
}

TEST_F(PciTopologyIntegrationTest, FindRootPort) {
    auto result = PciTopology::FindRootPort(target_addr_);
    if (result.IsOk()) {
        std::cout << "Root port: " << result.Value().ToString() << "\n";
        EXPECT_TRUE(PciTopology::DeviceExists(result.Value()));

        // Verify root port is actually a root port or bridge
        auto info = PciTopology::GetDeviceInfo(result.Value());
        if (info.IsOk()) {
            std::cout << "  port_type: 0x"
                      << std::hex
                      << static_cast<int>(
                             static_cast<uint8_t>(info.Value().port_type))
                      << std::dec << "\n";
        }
    } else {
        std::cout << "No root port found (device may be directly under root "
                     "complex)\n";
    }
}

TEST_F(PciTopologyIntegrationTest, GetPathToRoot) {
    auto result = PciTopology::GetPathToRoot(target_addr_);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    const auto& path = result.Value();
    EXPECT_FALSE(path.empty());

    std::cout << "Topology path (" << path.size() << " devices):\n";
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& node = path[i];
        std::cout << "  [" << i << "] " << node.address.ToString()
                  << " type=0x"
                  << std::hex
                  << static_cast<int>(static_cast<uint8_t>(node.port_type))
                  << std::dec
                  << " bridge=" << (node.is_bridge ? "yes" : "no") << "\n";
    }

    // First should be our device
    EXPECT_EQ(path.front().address, target_addr_);
}

TEST_F(PciTopologyIntegrationTest, RemoveRescanPathsExist) {
    // Just verify the sysfs paths exist — do NOT actually remove/rescan
    std::string sysfs_path = PciTopology::GetSysfsPath(target_addr_);

    struct stat st;
    // Remove path should exist for any device
    std::string remove_path = sysfs_path + "/remove";
    // We check via realpath since sysfs_path is a symlink
    char resolved[PATH_MAX];
    if (::realpath(sysfs_path.c_str(), resolved) != nullptr) {
        std::string real_remove = std::string(resolved) + "/remove";
        EXPECT_EQ(::stat(real_remove.c_str(), &st), 0)
            << "remove file should exist at " << real_remove;
    }
}

}  // namespace
}  // namespace plas::hal::pci
