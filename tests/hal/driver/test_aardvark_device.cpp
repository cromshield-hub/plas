#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

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

TEST(AardvarkDeviceTest, GetNicknameEqualsGetName) {
    auto entry = MakeEntry("my_device", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetNickname(), device.GetName());
}

TEST(AardvarkDeviceTest, GetDriverNameReturnsAardvark) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    EXPECT_EQ(device.GetDriverName(), "aardvark");
}

TEST(AardvarkDeviceTest, GetDeviceReturnsSelf) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    auto* i2c = static_cast<I2c*>(&device);
    EXPECT_EQ(i2c->GetDevice(), static_cast<Device*>(&device));
}

TEST(AardvarkDeviceTest, I2cInterfaceName) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    auto* i2c = static_cast<I2c*>(&device);
    EXPECT_EQ(i2c->InterfaceName(), "I2c");
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

// ---------------------------------------------------------------------------
// stop=false parameter (stub mode — no SDK, verifies no crash + correct error)
// ---------------------------------------------------------------------------

TEST(AardvarkDeviceTest, WriteWithStopFalseNotCrash) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    uint8_t buf[1] = {0};
    auto result = device.Write(0x50, buf, 1, /*stop=*/false);
    EXPECT_EQ(result.Error(), core::ErrorCode::kNotInitialized);
}

TEST(AardvarkDeviceTest, ReadWithStopFalseNotCrash) {
    auto entry = MakeEntry("dev0", "aardvark://0:0x50");
    AardvarkDevice device(entry);
    uint8_t buf[1] = {0};
    auto result = device.Read(0x50, buf, 1, /*stop=*/false);
    EXPECT_EQ(result.Error(), core::ErrorCode::kNotInitialized);
}

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

// ---------------------------------------------------------------------------
// Shared bus tests
// ---------------------------------------------------------------------------

class AardvarkSharedBusTest : public ::testing::Test {
protected:
    void TearDown() override { AardvarkDevice::ResetBusRegistry(); }
};

TEST_F(AardvarkSharedBusTest, TwoInstancesSamePortBothOpen) {
    // Two devices on the same port share the bus state; both Open must succeed.
    AardvarkDevice dev1(MakeEntry("dev1", "aardvark://0:0x50"));
    AardvarkDevice dev2(MakeEntry("dev2", "aardvark://0:0x51"));
    ASSERT_TRUE(dev1.Init().IsOk());
    ASSERT_TRUE(dev2.Init().IsOk());
    EXPECT_TRUE(dev1.Open().IsOk());
    EXPECT_TRUE(dev2.Open().IsOk());
    EXPECT_EQ(dev1.GetState(), DeviceState::kOpen);
    EXPECT_EQ(dev2.GetState(), DeviceState::kOpen);
    dev2.Close();
    dev1.Close();
}

TEST_F(AardvarkSharedBusTest, DifferentPortsHaveIndependentBusState) {
    // Devices on different ports must not share bus state.
    AardvarkDevice dev1(MakeEntry("dev1", "aardvark://0:0x50"));
    AardvarkDevice dev2(MakeEntry("dev2", "aardvark://1:0x50"));
    ASSERT_TRUE(dev1.Init().IsOk());
    ASSERT_TRUE(dev2.Init().IsOk());
    ASSERT_TRUE(dev1.Open().IsOk());
    ASSERT_TRUE(dev2.Open().IsOk());
    EXPECT_EQ(dev1.GetState(), DeviceState::kOpen);
    EXPECT_EQ(dev2.GetState(), DeviceState::kOpen);
    dev1.Close();
    dev2.Close();
}

TEST_F(AardvarkSharedBusTest, ReopenAfterLastClose) {
    // After the last reference on a port closes, a new Open creates a fresh
    // bus state on the same port.
    {
        AardvarkDevice dev1(MakeEntry("dev1", "aardvark://0:0x50"));
        ASSERT_TRUE(dev1.Init().IsOk());
        ASSERT_TRUE(dev1.Open().IsOk());
        ASSERT_TRUE(dev1.Close().IsOk());
    }
    AardvarkDevice dev2(MakeEntry("dev2", "aardvark://0:0x51"));
    ASSERT_TRUE(dev2.Init().IsOk());
    EXPECT_TRUE(dev2.Open().IsOk());
    EXPECT_EQ(dev2.GetState(), DeviceState::kOpen);
    dev2.Close();
}

TEST_F(AardvarkSharedBusTest, FirstCloseKeepsBusStateAlive) {
    // Closing one instance must not affect the other still-open instance.
    AardvarkDevice dev1(MakeEntry("dev1", "aardvark://0:0x50"));
    AardvarkDevice dev2(MakeEntry("dev2", "aardvark://0:0x51"));
    ASSERT_TRUE(dev1.Init().IsOk());
    ASSERT_TRUE(dev2.Init().IsOk());
    ASSERT_TRUE(dev1.Open().IsOk());
    ASSERT_TRUE(dev2.Open().IsOk());
    EXPECT_TRUE(dev1.Close().IsOk());
    EXPECT_EQ(dev1.GetState(), DeviceState::kClosed);
    EXPECT_EQ(dev2.GetState(), DeviceState::kOpen);
    dev2.Close();
}

TEST_F(AardvarkSharedBusTest, DestructorClosesOpenDevice) {
    // The destructor must release the shared bus ref so a subsequent Open on
    // the same port can succeed.
    {
        AardvarkDevice dev1(MakeEntry("dev1", "aardvark://0:0x50"));
        ASSERT_TRUE(dev1.Init().IsOk());
        ASSERT_TRUE(dev1.Open().IsOk());
        // dev1 destructs here, calls Close()
    }
    AardvarkDevice dev2(MakeEntry("dev2", "aardvark://0:0x51"));
    ASSERT_TRUE(dev2.Init().IsOk());
    EXPECT_TRUE(dev2.Open().IsOk());
    EXPECT_EQ(dev2.GetState(), DeviceState::kOpen);
    dev2.Close();
}

TEST_F(AardvarkSharedBusTest, ResetEmptyRegistryIsNoop) {
    EXPECT_NO_FATAL_FAILURE(AardvarkDevice::ResetBusRegistry());
}

TEST_F(AardvarkSharedBusTest, SetBitrateUpdatesSharedBus) {
    // Each instance tracks its own local bitrate_ independently.
    AardvarkDevice dev1(
        MakeEntry("dev1", "aardvark://0:0x50", {{"bitrate", "100000"}}));
    AardvarkDevice dev2(
        MakeEntry("dev2", "aardvark://0:0x51", {{"bitrate", "400000"}}));
    ASSERT_TRUE(dev1.Init().IsOk());
    ASSERT_TRUE(dev2.Init().IsOk());
    ASSERT_TRUE(dev1.Open().IsOk());
    ASSERT_TRUE(dev2.Open().IsOk());

    EXPECT_EQ(dev1.GetBitrate(), 100000u);
    EXPECT_EQ(dev2.GetBitrate(), 400000u);

    ASSERT_TRUE(dev1.SetBitrate(200000).IsOk());
    EXPECT_EQ(dev1.GetBitrate(), 200000u);
    EXPECT_EQ(dev2.GetBitrate(), 400000u);  // unchanged

    dev1.Close();
    dev2.Close();
}

TEST_F(AardvarkSharedBusTest, ConcurrentOpenSamePort) {
    // 8 threads simultaneously Opening devices on port 0 must not corrupt the
    // registry (no crash, all succeed via shared bus state).
    const int kThreads = 8;
    std::vector<std::unique_ptr<AardvarkDevice>> devices;
    devices.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        std::string nickname = "dev" + std::to_string(i);
        // Decimal addresses 80–87 are all valid 7-bit I2C addresses (≤127).
        std::string uri =
            "aardvark://0:" + std::to_string(static_cast<int>(0x50) + i);
        devices.push_back(
            std::make_unique<AardvarkDevice>(MakeEntry(nickname, uri)));
        ASSERT_TRUE(devices.back()->Init().IsOk());
    }

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&devices, &success_count, i]() {
            if (devices[static_cast<size_t>(i)]->Open().IsOk()) {
                ++success_count;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), kThreads);

    for (auto& dev : devices) {
        if (dev->GetState() == DeviceState::kOpen) {
            dev->Close();
        }
    }
}

}  // namespace
}  // namespace plas::hal::driver
