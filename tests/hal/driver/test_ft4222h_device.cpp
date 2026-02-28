#include <gtest/gtest.h>

#include "plas/config/device_entry.h"
#include "plas/core/error.h"
#include "plas/hal/driver/ft4222h/ft4222h_device.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/hal/interface/i2c.h"

namespace plas::hal::driver {
namespace {

// Helper to build a DeviceEntry for the ft4222h driver.
config::DeviceEntry MakeEntry(
    const std::string& nickname, const std::string& uri,
    const std::map<std::string, std::string>& args = {}) {
    return config::DeviceEntry{nickname, uri, "ft4222h", args};
}

// ---------------------------------------------------------------------------
// Factory registration
// ---------------------------------------------------------------------------

class Ft4222hFactoryTest : public ::testing::Test {
protected:
    void SetUp() override { Ft4222hDevice::Register(); }
};

TEST_F(Ft4222hFactoryTest, CreateFromConfig) {
    auto entry = MakeEntry("ft4222h_0", "ft4222h://0:1");
    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_NE(result.Value(), nullptr);
}

TEST_F(Ft4222hFactoryTest, SupportsI2cInterface) {
    auto entry = MakeEntry("ft4222h_0", "ft4222h://0:1");
    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());
    auto* i2c = dynamic_cast<I2c*>(result.Value().get());
    EXPECT_NE(i2c, nullptr);
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST(Ft4222hDeviceTest, InitialStateIsUninitialized) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(Ft4222hDeviceTest, GetNameReturnsNickname) {
    auto entry = MakeEntry("my_device", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetName(), "my_device");
}

TEST(Ft4222hDeviceTest, GetUriReturnsUri) {
    auto entry = MakeEntry("dev0", "ft4222h://2:3");
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetUri(), "ft4222h://2:3");
}

// ---------------------------------------------------------------------------
// Init / URI parsing
// ---------------------------------------------------------------------------

TEST(Ft4222hDeviceTest, InitSucceedsWithValidUri) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetState(), DeviceState::kInitialized);
}

TEST(Ft4222hDeviceTest, InitFailsWithInvalidPrefix) {
    auto entry = MakeEntry("dev0", "invalid://0:1");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kInvalidArgument));
}

TEST(Ft4222hDeviceTest, InitFailsWithBadFormat) {
    auto entry = MakeEntry("dev0", "ft4222h://badformat");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(Ft4222hDeviceTest, InitFailsWithMissingSlave) {
    // Master only, no colon+slave
    auto entry = MakeEntry("dev0", "ft4222h://0");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(Ft4222hDeviceTest, InitFailsWithEmptyUri) {
    auto entry = MakeEntry("dev0", "");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(Ft4222hDeviceTest, InitFailsWithSameIndices) {
    // master_idx == slave_idx → invalid
    auto entry = MakeEntry("dev0", "ft4222h://0:0");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kInvalidArgument));
}

TEST(Ft4222hDeviceTest, InitSucceedsWithLargeIndices) {
    auto entry = MakeEntry("dev0", "ft4222h://100:200");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
}

TEST(Ft4222hDeviceTest, InitFailsWithNonNumericMaster) {
    auto entry = MakeEntry("dev0", "ft4222h://abc:1");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(Ft4222hDeviceTest, InitFailsWithNonNumericSlave) {
    auto entry = MakeEntry("dev0", "ft4222h://0:xyz");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(Ft4222hDeviceTest, InitFailsWithNegativeIndex) {
    // strtoul("-1") wraps to ULONG_MAX > 65535
    auto entry = MakeEntry("dev0", "ft4222h://-1:0");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

TEST(Ft4222hDeviceTest, InitSucceedsWithMaxIndex) {
    auto entry = MakeEntry("dev0", "ft4222h://65535:0");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    ASSERT_TRUE(result.IsOk());
}

TEST(Ft4222hDeviceTest, InitFailsWithOverflowIndex) {
    // 65536 > 65535
    auto entry = MakeEntry("dev0", "ft4222h://65536:0");
    Ft4222hDevice device(entry);
    auto result = device.Init();
    EXPECT_TRUE(result.IsError());
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

TEST(Ft4222hDeviceTest, OpenBeforeInitFails) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    auto result = device.Open();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(Ft4222hDeviceTest, ReadBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    device.Init();
    core::Byte buf[4];
    auto result = device.Read(0x50, buf, sizeof(buf));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(Ft4222hDeviceTest, WriteBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    device.Init();
    core::Byte buf[4] = {0x01, 0x02, 0x03, 0x04};
    auto result = device.Write(0x50, buf, sizeof(buf));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(Ft4222hDeviceTest, WriteReadBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    device.Init();
    core::Byte wbuf[2] = {0x00, 0x01};
    core::Byte rbuf[4];
    auto result = device.WriteRead(0x50, wbuf, sizeof(wbuf), rbuf,
                                   sizeof(rbuf));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kNotInitialized));
}

TEST(Ft4222hDeviceTest, CloseBeforeOpenFails) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    device.Init();
    auto result = device.Close();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kAlreadyClosed));
}

// ---------------------------------------------------------------------------
// Config args
// ---------------------------------------------------------------------------

TEST(Ft4222hDeviceTest, DefaultBitrate) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetBitrate(), 400000u);
}

TEST(Ft4222hDeviceTest, CustomBitrate) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1", {{"bitrate", "100000"}});
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetBitrate(), 100000u);
}

TEST(Ft4222hDeviceTest, CustomSlaveAddr) {
    // slave_addr is internal, verify construction doesn't crash
    auto entry = MakeEntry("dev0", "ft4222h://0:1", {{"slave_addr", "0x50"}});
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(Ft4222hDeviceTest, CustomSysClock) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1", {{"sys_clock", "48"}});
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(Ft4222hDeviceTest, CustomRxTimeout) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1",
                           {{"rx_timeout_ms", "5000"}});
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetState(), DeviceState::kUninitialized);
}

TEST(Ft4222hDeviceTest, InvalidArgsIgnored) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1",
                           {{"bitrate", "not_a_number"},
                            {"slave_addr", "invalid"},
                            {"sys_clock", "99"},
                            {"rx_timeout_ms", "xyz"}});
    Ft4222hDevice device(entry);
    // Should fall back to defaults without crashing
    EXPECT_EQ(device.GetBitrate(), 400000u);
}

// ---------------------------------------------------------------------------
// Bitrate
// ---------------------------------------------------------------------------

TEST(Ft4222hDeviceTest, SetBitrateCaches) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    auto result = device.SetBitrate(100000);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetBitrate(), 100000u);
}

TEST(Ft4222hDeviceTest, SetBitrateZeroFails) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    auto result = device.SetBitrate(0);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              core::make_error_code(core::ErrorCode::kInvalidArgument));
}

TEST(Ft4222hDeviceTest, GetBitrateDefault) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    EXPECT_EQ(device.GetBitrate(), 400000u);
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// stop=false parameter (stub mode — no SDK, verifies no crash + correct error)
// ---------------------------------------------------------------------------

TEST(Ft4222hDeviceTest, WriteWithStopFalseNotCrash) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    uint8_t buf[1] = {0};
    auto result = device.Write(0x40, buf, 1, /*stop=*/false);
    EXPECT_EQ(result.Error(), core::ErrorCode::kNotInitialized);
}

TEST(Ft4222hDeviceTest, ReadWithStopFalseNotCrash) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    uint8_t buf[1] = {0};
    auto result = device.Read(0x40, buf, 1, /*stop=*/false);
    EXPECT_EQ(result.Error(), core::ErrorCode::kNotInitialized);
}

TEST(Ft4222hDeviceTest, ResetFromUninitialized) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    auto result = device.Reset();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetState(), DeviceState::kInitialized);
}

TEST(Ft4222hDeviceTest, ResetFromInitialized) {
    auto entry = MakeEntry("dev0", "ft4222h://0:1");
    Ft4222hDevice device(entry);
    device.Init();
    auto result = device.Reset();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.GetState(), DeviceState::kInitialized);
}

}  // namespace
}  // namespace plas::hal::driver
