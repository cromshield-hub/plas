#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "plas/config/device_entry.h"
#include "plas/hal/driver/ft4222h/ft4222h_device.h"
#include "plas/hal/interface/i2c.h"

namespace plas::hal::driver {
namespace {

/// Integration tests that require two real FT4222H adapters (master + slave).
///
/// Set PLAS_TEST_FT4222H_PORT=master_idx:slave_idx to enable.
/// Example: PLAS_TEST_FT4222H_PORT=0:1

class Ft4222hIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* env = std::getenv("PLAS_TEST_FT4222H_PORT");
        if (!env || std::string(env).empty()) {
            GTEST_SKIP() << "PLAS_TEST_FT4222H_PORT not set; skipping "
                            "integration tests";
        }
        port_str_ = env;
        uri_ = "ft4222h://" + port_str_;

        config::DeviceEntry entry{"integ_dev", uri_, "ft4222h", {}};
        device_ = std::make_unique<Ft4222hDevice>(entry);

        auto init = device_->Init();
        ASSERT_TRUE(init.IsOk()) << "Init failed";

        auto open = device_->Open();
        ASSERT_TRUE(open.IsOk()) << "Open failed";
    }

    void TearDown() override {
        if (device_ && device_->GetState() == DeviceState::kOpen) {
            device_->Close();
        }
    }

    std::string port_str_;
    std::string uri_;
    std::unique_ptr<Ft4222hDevice> device_;
};

TEST_F(Ft4222hIntegrationTest, WriteToDevice) {
    core::Byte buf[2] = {0x00, 0x01};
    auto result = device_->Write(0x50, buf, sizeof(buf));
    ASSERT_TRUE(result.IsOk()) << "Write failed: " << result.Error().message();
    EXPECT_GT(result.Value(), 0u);
}

TEST_F(Ft4222hIntegrationTest, ReadFromDevice) {
    core::Byte buf[4] = {};
    auto result = device_->Read(0x50, buf, sizeof(buf));
    ASSERT_TRUE(result.IsOk()) << "Read failed: " << result.Error().message();
    EXPECT_GT(result.Value(), 0u);
}

TEST_F(Ft4222hIntegrationTest, WriteReadTransaction) {
    core::Byte wbuf[1] = {0x00};
    core::Byte rbuf[4] = {};
    auto result = device_->WriteRead(0x50, wbuf, sizeof(wbuf), rbuf,
                                     sizeof(rbuf));
    ASSERT_TRUE(result.IsOk())
        << "WriteRead failed: " << result.Error().message();
    EXPECT_GT(result.Value(), 0u);
}

TEST_F(Ft4222hIntegrationTest, SetBitrateOnLive) {
    auto result = device_->SetBitrate(100000);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device_->GetBitrate(), 100000u);
}

TEST_F(Ft4222hIntegrationTest, CloseAndReopen) {
    auto close = device_->Close();
    ASSERT_TRUE(close.IsOk());
    EXPECT_EQ(device_->GetState(), DeviceState::kClosed);

    auto init = device_->Init();
    ASSERT_TRUE(init.IsOk());
    auto open = device_->Open();
    ASSERT_TRUE(open.IsOk());
    EXPECT_EQ(device_->GetState(), DeviceState::kOpen);
}

TEST_F(Ft4222hIntegrationTest, DeviceInfo) {
    EXPECT_EQ(device_->GetName(), "integ_dev");
    EXPECT_EQ(device_->GetUri(), uri_);
    EXPECT_EQ(device_->GetState(), DeviceState::kOpen);
}

TEST_F(Ft4222hIntegrationTest, MultipleWriteReads) {
    for (int i = 0; i < 3; ++i) {
        core::Byte wbuf[2] = {static_cast<core::Byte>(i), 0x00};
        core::Byte rbuf[4] = {};
        auto result = device_->WriteRead(0x50, wbuf, sizeof(wbuf), rbuf,
                                         sizeof(rbuf));
        ASSERT_TRUE(result.IsOk())
            << "WriteRead iteration " << i
            << " failed: " << result.Error().message();
    }
}

}  // namespace
}  // namespace plas::hal::driver
