#include <gtest/gtest.h>

#include "plas/config/device_entry.h"
#include "plas/core/error.h"
#include "plas/hal/driver/aardvark/aardvark_device.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/hal/interface/i2c.h"

namespace plas::hal::driver {
namespace {

// Helper to build a DeviceEntry for the aardvark driver.
config::DeviceEntry MakeEntry(
    const std::string& nickname, const std::string& uri,
    const std::map<std::string, std::string>& args = {}) {
    return config::DeviceEntry{nickname, uri, "aardvark", args};
}

// ---------------------------------------------------------------------------
// Factory registration
// ---------------------------------------------------------------------------

class AardvarkFactoryTest : public ::testing::Test {
protected:
    void SetUp() override { AardvarkDevice::Register(); }
};

TEST_F(AardvarkFactoryTest, CreateFromConfig) {
    auto entry = MakeEntry("aardvark0", "aardvark://0:0x50");
    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_NE(result.Value(), nullptr);
}

TEST_F(AardvarkFactoryTest, SupportsI2cInterface) {
    auto entry = MakeEntry("aardvark0", "aardvark://0:0x50");
    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());
    auto* i2c = dynamic_cast<I2c*>(result.Value().get());
    EXPECT_NE(i2c, nullptr);
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST(AardvarkDeviceTest, InitialStateIsUninitialized) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(AardvarkDeviceTest, GetNameReturnsNickname) {
    auto entry = MakeEntry("my_device", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetName(), "my_device");
}

TEST(AardvarkDeviceTest, GetUriReturnsUri) {
    auto entry = MakeEntry("dev0", "aardvark://1:0x68");
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetUri(), "aardvark://1:0x68");
}

// ---------------------------------------------------------------------------
// Init / URI parsing
// ---------------------------------------------------------------------------

TEST(AardvarkDeviceTest, InitSucceedsWithValidUri) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetState(), DeviceState::kInitialized);
}

TEST(AardvarkDeviceTest, InitFailsWithInvalidPrefix) {
    auto entry = MakeEntry("dev0", "invalid://0:0x50");
    AardvarkDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kInvalidArgument));
}

TEST(AardvarkDeviceTest, InitFailsWithBadFormat) {
    auto entry = MakeEntry("dev0", "aardvark://badformat");
    AardvarkDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(AardvarkDeviceTest, InitFailsWithMissingAddr) {
    // Port only, no colon+address
    auto entry = MakeEntry("dev0", "aardvark://0");
    AardvarkDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(AardvarkDeviceTest, InitFailsWithEmptyUri) {
    auto entry = MakeEntry("dev0", "");
    AardvarkDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(AardvarkDeviceTest, InitSucceedsWithHexAddr) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x68");
    AardvarkDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
}

TEST(AardvarkDeviceTest, InitSucceedsWithDecimalAddr) {
    // 80 decimal = 0x50, valid
    auto entry = MakeEntry("dev0", "aardvark://0:80");
    AardvarkDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
}

TEST(AardvarkDeviceTest, InitFailsWithAddrOverflow) {
    // 0x80 = 128 > 0x7F, out of 7-bit I2C address range
    auto entry = MakeEntry("dev0", "aardvark://0:0x80");
    AardvarkDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(AardvarkDeviceTest, InitFailsWithNegativePort) {
    // strtoul wraps negatives, but the string starts with '-' which is invalid
    // for an unsigned parse — strtoul accepts it but wraps, so we rely on
    // the value being > 65535 after wrap. Actually strtoul("-1"...) returns
    // ULONG_MAX which > 65535.
    auto entry = MakeEntry("dev0", "aardvark://-1:0x50");
    AardvarkDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

TEST(AardvarkDeviceTest, OpenBeforeInitFails) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    auto result = device.Open();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(AardvarkDeviceTest, ReadBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    device.Init();
    core::Byte buf[4];
    auto result = device.Read(0x50, buf, sizeof(buf));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(AardvarkDeviceTest, WriteBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    device.Init();
    core::Byte buf[4] = {0x01, 0x02, 0x03, 0x04};
    auto result = device.Write(0x50, buf, sizeof(buf));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(AardvarkDeviceTest, WriteReadBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    device.Init();
    core::Byte wbuf[2] = {0x00, 0x01};
    core::Byte rbuf[4];
    auto result = device.WriteRead(0x50, wbuf, sizeof(wbuf), rbuf,
                                   sizeof(rbuf));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(AardvarkDeviceTest, CloseBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    device.Init();
    auto result = device.Close();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kAlreadyClosed));
}

// ---------------------------------------------------------------------------
// Config args
// ---------------------------------------------------------------------------

TEST(AardvarkDeviceTest, DefaultArgs) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetBitrate(), 100000u);
}

TEST(AardvarkDeviceTest, CustomBitrate) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50", {{"bitrate", "400000"}});
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetBitrate(), 400000u);
}

TEST(AardvarkDeviceTest, CustomPullup) {
    // Pullup is internal, just verify construction doesn't crash
    auto entry = MakeEntry("dev0", "aardvark://0:0x50",
                           {{"pullup", "false"}});
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(AardvarkDeviceTest, CustomBusTimeout) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50",
                           {{"bus_timeout_ms", "500"}});
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(AardvarkDeviceTest, InvalidArgsIgnored) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50",
                           {{"bitrate", "not_a_number"},
                            {"pullup", "maybe"},
                            {"bus_timeout_ms", "xyz"}});
    AardvarkDevice device(entry);
    // Should fall back to defaults without crashing
    EXPECT_EQ(device.GetBitrate(), 100000u);
}

// ---------------------------------------------------------------------------
// Bitrate
// ---------------------------------------------------------------------------

TEST(AardvarkDeviceTest, SetBitrateCaches) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    auto result = device.SetBitrate(400000);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetBitrate(), 400000u);
}

TEST(AardvarkDeviceTest, SetBitrateZeroFails) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    auto result = device.SetBitrate(0);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kInvalidArgument));
}

TEST(AardvarkDeviceTest, GetBitrateDefault) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetBitrate(), 100000u);
}

// ---------------------------------------------------------------------------
// URI edge cases
// ---------------------------------------------------------------------------

TEST(AardvarkDeviceTest, InitWithMaxPort) {
    auto entry = MakeEntry("dev0", "aardvark://65535:0x50");
    AardvarkDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
}

TEST(AardvarkDeviceTest, InitWithZeroAddr) {
    // 0x00 is the general-call address, but valid syntactically
    auto entry = MakeEntry("dev0", "aardvark://0:0x00");
    AardvarkDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
}

TEST(AardvarkDeviceTest, InitWithMaxAddr) {
    // 0x7F is the max 7-bit I2C address
    auto entry = MakeEntry("dev0", "aardvark://0:0x7F");
    AardvarkDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
}

TEST(AardvarkDeviceTest, InitFailsWithPortOverflow) {
    // 65536 > 65535
    auto entry = MakeEntry("dev0", "aardvark://65536:0x50");
    AardvarkDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

TEST(AardvarkDeviceTest, ResetFromUninitialized) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    // Reset from kUninitialized should go through Init
    auto result = device.Reset();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetState(), DeviceState::kInitialized);
}

TEST(AardvarkDeviceTest, ResetFromInitialized) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    device.Init();
    auto result = device.Reset();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetState(), DeviceState::kInitialized);
}

}  // namespace
}  // namespace plas::hal::driver
