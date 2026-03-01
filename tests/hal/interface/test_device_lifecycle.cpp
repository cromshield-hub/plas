#include <gtest/gtest.h>

#include "mock_device.h"
#include "plas/core/error.h"
#include "plas/hal/interface/i2c.h"

namespace plas::hal {
namespace {

using test::MockDevice;

// --- Initial state ---

TEST(DeviceLifecycleTest, InitialStateIsUninitialized) {
    MockDevice dev("test", "test://0");
    EXPECT_EQ(dev.GetState(), DeviceState::kUninitialized);
}

// --- Valid transitions ---

TEST(DeviceLifecycleTest, InitTransition) {
    MockDevice dev("test", "test://0");
    auto result = dev.Init();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kInitialized);
}

TEST(DeviceLifecycleTest, InitOpenClose) {
    MockDevice dev("test", "test://0");
    ASSERT_TRUE(dev.Init().IsOk());
    ASSERT_TRUE(dev.Open().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kOpen);
    ASSERT_TRUE(dev.Close().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kClosed);
}

TEST(DeviceLifecycleTest, CloseAndReopen) {
    MockDevice dev("test", "test://0");
    ASSERT_TRUE(dev.Init().IsOk());
    ASSERT_TRUE(dev.Open().IsOk());
    ASSERT_TRUE(dev.Close().IsOk());
    // Re-open from Closed state should succeed
    ASSERT_TRUE(dev.Open().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kOpen);
}

TEST(DeviceLifecycleTest, ResetThenOpen) {
    MockDevice dev("test", "test://0");
    auto result = dev.Reset();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kInitialized);
    ASSERT_TRUE(dev.Open().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kOpen);
}

// --- Invalid Open ---

TEST(DeviceLifecycleTest, OpenFromUninitializedFails) {
    MockDevice dev("test", "test://0");
    auto result = dev.Open();
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(), make_error_code(core::ErrorCode::kNotInitialized));
}

// --- Invalid Close ---

TEST(DeviceLifecycleTest, CloseFromUninitializedFails) {
    MockDevice dev("test", "test://0");
    auto result = dev.Close();
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              make_error_code(core::ErrorCode::kAlreadyClosed));
}

TEST(DeviceLifecycleTest, CloseFromInitializedFails) {
    MockDevice dev("test", "test://0");
    ASSERT_TRUE(dev.Init().IsOk());
    auto result = dev.Close();
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              make_error_code(core::ErrorCode::kAlreadyClosed));
}

TEST(DeviceLifecycleTest, DoubleCloseFails) {
    MockDevice dev("test", "test://0");
    ASSERT_TRUE(dev.Init().IsOk());
    ASSERT_TRUE(dev.Open().IsOk());
    ASSERT_TRUE(dev.Close().IsOk());
    auto result = dev.Close();
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              make_error_code(core::ErrorCode::kAlreadyClosed));
}

// --- Reset ---

TEST(DeviceLifecycleTest, ResetFromOpenGoesToInitialized) {
    MockDevice dev("test", "test://0");
    ASSERT_TRUE(dev.Init().IsOk());
    ASSERT_TRUE(dev.Open().IsOk());
    auto result = dev.Reset();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kInitialized);
}

TEST(DeviceLifecycleTest, ResetFromUninitializedGoesToInitialized) {
    MockDevice dev("test", "test://0");
    auto result = dev.Reset();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kInitialized);
}

// --- Name and URI ---

TEST(DeviceLifecycleTest, GetNameReturnsConstructorValue) {
    MockDevice dev("my_device", "test://1:0x50");
    EXPECT_EQ(dev.GetName(), "my_device");
}

TEST(DeviceLifecycleTest, GetUriReturnsConstructorValue) {
    MockDevice dev("my_device", "test://1:0x50");
    EXPECT_EQ(dev.GetUri(), "test://1:0x50");
}

TEST(DeviceLifecycleTest, GetNicknameEqualsGetName) {
    MockDevice dev("nickname_test", "test://0:0");
    EXPECT_EQ(dev.GetNickname(), dev.GetName());
    EXPECT_EQ(dev.GetNickname(), "nickname_test");
}

TEST(DeviceLifecycleTest, GetDriverNameReturnsMock) {
    MockDevice dev("test", "test://0");
    EXPECT_EQ(dev.GetDriverName(), "mock");
}

// --- Full lifecycle cycle ---

TEST(DeviceLifecycleTest, FullCycle) {
    MockDevice dev("test", "test://0");

    // Init → Open → Close
    ASSERT_TRUE(dev.Init().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kInitialized);
    ASSERT_TRUE(dev.Open().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kOpen);
    ASSERT_TRUE(dev.Close().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kClosed);

    // Re-open → Close
    ASSERT_TRUE(dev.Open().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kOpen);
    ASSERT_TRUE(dev.Close().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kClosed);

    // Reset → Open → Close
    ASSERT_TRUE(dev.Reset().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kInitialized);
    ASSERT_TRUE(dev.Open().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kOpen);
    ASSERT_TRUE(dev.Close().IsOk());
    EXPECT_EQ(dev.GetState(), DeviceState::kClosed);
}

}  // namespace
}  // namespace plas::hal
