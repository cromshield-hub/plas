#include <gtest/gtest.h>

#include "plas/config/device_entry.h"
#include "plas/core/error.h"
#include "plas/hal/driver/pciutils/pciutils_device.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/hal/interface/pci/pci_bar.h"
#include "plas/hal/interface/pci/pci_config.h"
#include "plas/hal/interface/pci/pci_doe.h"

namespace plas::hal::driver {
namespace {

// Helper to build a DeviceEntry for the pciutils driver.
config::DeviceEntry MakeEntry(
    const std::string& nickname, const std::string& uri,
    const std::map<std::string, std::string>& args = {}) {
    return config::DeviceEntry{nickname, uri, "pciutils", args};
}

// ---------------------------------------------------------------------------
// Factory registration
// ---------------------------------------------------------------------------

class PciUtilsFactoryTest : public ::testing::Test {
protected:
    void SetUp() override { PciUtilsDevice::Register(); }
};

TEST_F(PciUtilsFactoryTest, CreateFromConfig) {
    auto entry = MakeEntry("nvme0", "pciutils://0000:03:00.0");
    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_NE(result.Value(), nullptr);
}

TEST_F(PciUtilsFactoryTest, SupportsPciConfigInterface) {
    auto entry = MakeEntry("nvme0", "pciutils://0000:03:00.0");
    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());
    auto* pci_cfg = dynamic_cast<pci::PciConfig*>(result.Value().get());
    EXPECT_NE(pci_cfg, nullptr);
}

TEST_F(PciUtilsFactoryTest, SupportsPciDoeInterface) {
    auto entry = MakeEntry("nvme0", "pciutils://0000:03:00.0");
    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());
    auto* pci_doe = dynamic_cast<pci::PciDoe*>(result.Value().get());
    EXPECT_NE(pci_doe, nullptr);
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST(PciUtilsDeviceTest, InitialStateIsUninitialized) {
    auto entry = MakeEntry("dev0", "pciutils://0000:01:00.0");
    PciUtilsDevice device(entry);
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(PciUtilsDeviceTest, GetNameReturnsNickname) {
    auto entry = MakeEntry("my_device", "pciutils://0000:02:00.0");
    PciUtilsDevice device(entry);
    EXPECT_EQ(device.GetName(), "my_device");
}

TEST(PciUtilsDeviceTest, GetNicknameEqualsGetName) {
    auto entry = MakeEntry("my_device", "pciutils://0000:02:00.0");
    PciUtilsDevice device(entry);
    EXPECT_EQ(device.GetNickname(), device.GetName());
}

TEST(PciUtilsDeviceTest, GetDriverNameReturnsPciutils) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    EXPECT_EQ(device.GetDriverName(), "pciutils");
}

TEST(PciUtilsDeviceTest, GetDeviceReturnsSelfViaPciConfig) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    auto* pci_cfg = static_cast<pci::PciConfig*>(&device);
    EXPECT_EQ(pci_cfg->GetDevice(), static_cast<Device*>(&device));
}

TEST(PciUtilsDeviceTest, GetDeviceReturnsSelfViaPciBar) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    auto* pci_bar = static_cast<pci::PciBar*>(&device);
    EXPECT_EQ(pci_bar->GetDevice(), static_cast<Device*>(&device));
}

TEST(PciUtilsDeviceTest, PciConfigInterfaceName) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    auto* pci_cfg = static_cast<pci::PciConfig*>(&device);
    EXPECT_EQ(pci_cfg->InterfaceName(), "PciConfig");
}

TEST(PciUtilsDeviceTest, GetUriReturnsUri) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    EXPECT_EQ(device.GetUri(), "pciutils://0000:03:00.0");
}

// ---------------------------------------------------------------------------
// Init / URI parsing
// ---------------------------------------------------------------------------

TEST(PciUtilsDeviceTest, InitSucceedsWithValidUri) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetState(), DeviceState::kInitialized);
}

TEST(PciUtilsDeviceTest, InitFailsWithInvalidUri_NoPrefix) {
    auto entry = MakeEntry("dev0", "invalid://0000:03:00.0");
    PciUtilsDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kInvalidArgument));
}

TEST(PciUtilsDeviceTest, InitFailsWithInvalidUri_BadFormat) {
    auto entry = MakeEntry("dev0", "pciutils://badformat");
    PciUtilsDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(PciUtilsDeviceTest, InitFailsWithInvalidUri_DeviceTooLarge) {
    // Device > 0x1F
    auto entry = MakeEntry("dev0", "pciutils://0000:00:20.0");
    PciUtilsDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(PciUtilsDeviceTest, InitFailsWithInvalidUri_FunctionTooLarge) {
    // Function > 7
    auto entry = MakeEntry("dev0", "pciutils://0000:00:00.8");
    PciUtilsDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(PciUtilsDeviceTest, InitFailsWithEmptyUri) {
    auto entry = MakeEntry("dev0", "");
    PciUtilsDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

TEST(PciUtilsDeviceTest, ReadConfigBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    device.Init();
    pci::Bdf bdf{0x03, 0x00, 0x00};
    auto result = device.ReadConfig8(bdf, 0x00);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(PciUtilsDeviceTest, OpenBeforeInitFails) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    auto result = device.Open();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

// ---------------------------------------------------------------------------
// DOE args parsing
// ---------------------------------------------------------------------------

TEST(PciUtilsDeviceTest, DefaultDoeArgs) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0");
    PciUtilsDevice device(entry);
    // Defaults: timeout=1000, poll_interval=100
    // We can't directly inspect private members, but construction should work.
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(PciUtilsDeviceTest, CustomDoeArgs) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0",
                           {{"doe_timeout_ms", "5000"},
                            {"doe_poll_interval_us", "500"}});
    PciUtilsDevice device(entry);
    // Construction should succeed with custom args.
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(PciUtilsDeviceTest, InvalidDoeArgsIgnored) {
    auto entry = MakeEntry("dev0", "pciutils://0000:03:00.0",
                           {{"doe_timeout_ms", "not_a_number"}});
    PciUtilsDevice device(entry);
    // Should fall back to defaults without crashing.
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

// ---------------------------------------------------------------------------
// URI edge cases
// ---------------------------------------------------------------------------

TEST(PciUtilsDeviceTest, InitWithMaxDomain) {
    auto entry = MakeEntry("dev0", "pciutils://FFFF:FF:1F.7");
    PciUtilsDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetState(), DeviceState::kInitialized);
}

TEST(PciUtilsDeviceTest, InitWithZeroBdf) {
    auto entry = MakeEntry("dev0", "pciutils://0000:00:00.0");
    PciUtilsDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
}

}  // namespace
}  // namespace plas::hal::driver
