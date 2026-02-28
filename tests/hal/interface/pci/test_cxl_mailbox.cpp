#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "plas/core/error.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/pci/cxl_mailbox.h"

namespace plas::hal::pci {
namespace {

/// Mock device implementing Device + CxlMailbox for testing the ABC.
class MockCxlMailboxDevice : public Device, public CxlMailbox {
public:
    // Device interface
    core::Result<void> Init() override { return core::Result<void>::Ok(); }
    core::Result<void> Open() override { return core::Result<void>::Ok(); }
    core::Result<void> Close() override { return core::Result<void>::Ok(); }
    core::Result<void> Reset() override { return core::Result<void>::Ok(); }
    DeviceState GetState() const override { return DeviceState::kInitialized; }
    std::string GetName() const override { return "mock_cxl_mailbox"; }
    std::string GetUri() const override { return "mock://0:0"; }

    // CxlMailbox interface
    core::Result<CxlMailboxResult> ExecuteCommand(
        Bdf bdf, CxlMailboxOpcode opcode,
        const CxlMailboxPayload& payload) override {
        last_bdf_ = bdf;
        last_opcode_ = static_cast<uint16_t>(opcode);
        last_payload_ = payload;
        if (execute_error_) {
            return core::Result<CxlMailboxResult>::Err(
                core::ErrorCode::kIOError);
        }
        return core::Result<CxlMailboxResult>::Ok(execute_result_);
    }

    core::Result<CxlMailboxResult> ExecuteCommand(
        Bdf bdf, uint16_t raw_opcode,
        const CxlMailboxPayload& payload) override {
        last_bdf_ = bdf;
        last_opcode_ = raw_opcode;
        last_payload_ = payload;
        if (execute_error_) {
            return core::Result<CxlMailboxResult>::Err(
                core::ErrorCode::kIOError);
        }
        return core::Result<CxlMailboxResult>::Ok(execute_result_);
    }

    core::Result<uint32_t> GetPayloadSize(Bdf bdf) override {
        last_bdf_ = bdf;
        return core::Result<uint32_t>::Ok(payload_size_);
    }

    core::Result<bool> IsReady(Bdf bdf) override {
        last_bdf_ = bdf;
        return core::Result<bool>::Ok(is_ready_);
    }

    core::Result<CxlMailboxResult> GetBackgroundCmdStatus(Bdf bdf) override {
        last_bdf_ = bdf;
        if (bg_status_error_) {
            return core::Result<CxlMailboxResult>::Err(
                core::ErrorCode::kIOError);
        }
        return core::Result<CxlMailboxResult>::Ok(bg_status_result_);
    }

    // Test helpers
    Bdf last_bdf_{};
    uint16_t last_opcode_ = 0;
    CxlMailboxPayload last_payload_;

    CxlMailboxResult execute_result_{CxlMailboxReturnCode::kSuccess, {}};
    uint32_t payload_size_ = 256;
    bool is_ready_ = true;
    CxlMailboxResult bg_status_result_{CxlMailboxReturnCode::kSuccess, {}};

    bool execute_error_ = false;
    bool bg_status_error_ = false;
};

// --- Dynamic cast ---

TEST(CxlMailboxTest, DynamicCastFromDevice) {
    auto device = std::make_unique<MockCxlMailboxDevice>();
    auto* mailbox = dynamic_cast<CxlMailbox*>(device.get());
    EXPECT_NE(mailbox, nullptr)
        << "MockCxlMailboxDevice should implement CxlMailbox";
}

// --- ExecuteCommand (typed opcode) ---

TEST(CxlMailboxTest, ExecuteCommandSuccess) {
    MockCxlMailboxDevice device;
    device.execute_result_ = {
        CxlMailboxReturnCode::kSuccess, {0x01, 0x02, 0x03}};
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.ExecuteCommand(
        bdf, CxlMailboxOpcode::kIdentify, {});
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().return_code, CxlMailboxReturnCode::kSuccess);
    ASSERT_EQ(result.Value().payload.size(), 3u);
    EXPECT_EQ(result.Value().payload[0], 0x01);
}

TEST(CxlMailboxTest, ExecuteCommandPassesBdfAndOpcode) {
    MockCxlMailboxDevice device;
    Bdf bdf{0x03, 0x0A, 0x02};

    device.ExecuteCommand(bdf, CxlMailboxOpcode::kGetHealthInfo, {0xAA});
    EXPECT_EQ(device.last_bdf_, bdf);
    EXPECT_EQ(device.last_opcode_,
              static_cast<uint16_t>(CxlMailboxOpcode::kGetHealthInfo));
    ASSERT_EQ(device.last_payload_.size(), 1u);
    EXPECT_EQ(device.last_payload_[0], 0xAA);
}

TEST(CxlMailboxTest, ExecuteCommandError) {
    MockCxlMailboxDevice device;
    device.execute_error_ = true;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.ExecuteCommand(
        bdf, CxlMailboxOpcode::kIdentify, {});
    ASSERT_TRUE(result.IsError());
}

TEST(CxlMailboxTest, ExecuteCommandReturnCodeNonSuccess) {
    MockCxlMailboxDevice device;
    device.execute_result_ = {CxlMailboxReturnCode::kInvalidInput, {}};
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.ExecuteCommand(
        bdf, CxlMailboxOpcode::kIdentify, {});
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().return_code, CxlMailboxReturnCode::kInvalidInput);
}

// --- ExecuteCommand (raw opcode) ---

TEST(CxlMailboxTest, ExecuteRawOpcodeSuccess) {
    MockCxlMailboxDevice device;
    device.execute_result_ = {CxlMailboxReturnCode::kSuccess, {0xFF}};
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.ExecuteCommand(bdf, uint16_t{0x9999}, {0x01});
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.last_opcode_, 0x9999);
    EXPECT_EQ(result.Value().payload.size(), 1u);
}

TEST(CxlMailboxTest, ExecuteRawOpcodeError) {
    MockCxlMailboxDevice device;
    device.execute_error_ = true;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.ExecuteCommand(bdf, uint16_t{0x9999}, {});
    ASSERT_TRUE(result.IsError());
}

// --- GetPayloadSize ---

TEST(CxlMailboxTest, GetPayloadSize) {
    MockCxlMailboxDevice device;
    device.payload_size_ = 1024;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.GetPayloadSize(bdf);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 1024u);
}

// --- IsReady ---

TEST(CxlMailboxTest, IsReadyTrue) {
    MockCxlMailboxDevice device;
    device.is_ready_ = true;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.IsReady(bdf);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value());
}

TEST(CxlMailboxTest, IsReadyFalse) {
    MockCxlMailboxDevice device;
    device.is_ready_ = false;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.IsReady(bdf);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value());
}

// --- GetBackgroundCmdStatus ---

TEST(CxlMailboxTest, GetBackgroundCmdStatusSuccess) {
    MockCxlMailboxDevice device;
    device.bg_status_result_ = {
        CxlMailboxReturnCode::kBackgroundCmdStarted, {0x50}};
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.GetBackgroundCmdStatus(bdf);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().return_code,
              CxlMailboxReturnCode::kBackgroundCmdStarted);
}

TEST(CxlMailboxTest, GetBackgroundCmdStatusError) {
    MockCxlMailboxDevice device;
    device.bg_status_error_ = true;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.GetBackgroundCmdStatus(bdf);
    ASSERT_TRUE(result.IsError());
}

}  // namespace
}  // namespace plas::hal::pci
