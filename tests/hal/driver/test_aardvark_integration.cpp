#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "plas/config/device_entry.h"
#include "plas/hal/driver/aardvark/aardvark_device.h"
#include "plas/hal/interface/i2c.h"

namespace plas::hal::driver {
namespace {

/// Integration tests that require a real Aardvark adapter.
///
/// Set PLAS_TEST_AARDVARK_PORT=port:address to enable.
/// Example: PLAS_TEST_AARDVARK_PORT=0:0x50

class AardvarkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* env = std::getenv("PLAS_TEST_AARDVARK_PORT");
        if (!env || std::string(env).empty()) {
            GTEST_SKIP() << "PLAS_TEST_AARDVARK_PORT not set; skipping "
                            "integration tests";
        }
        port_str_ = env;
        uri_ = "aardvark://" + port_str_;

        // Parse address for I2C operations
        auto colon = port_str_.find(':');
        if (colon != std::string::npos && colon + 1 < port_str_.size()) {
            char* end = nullptr;
            unsigned long val =
                std::strtoul(port_str_.substr(colon + 1).c_str(), &end, 0);
            addr_ = static_cast<uint16_t>(val);
        }

        config::DeviceEntry entry{"integ_dev", uri_, "aardvark", {}};
        device_ = std::make_unique<AardvarkDevice>(entry);

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
    uint16_t addr_ = 0x50;
    std::unique_ptr<AardvarkDevice> device_;
};

TEST_F(AardvarkIntegrationTest, ReadFromDevice) {
    core::Byte buf[4] = {};
    auto result = device_->Read(addr_, buf, sizeof(buf));
    ASSERT_TRUE(result.IsOk()) << "Read failed: " << result.Error().message();
    EXPECT_GT(result.Value(), 0u);
}

TEST_F(AardvarkIntegrationTest, WriteToDevice) {
    core::Byte buf[2] = {0x00, 0x01};
    auto result = device_->Write(addr_, buf, sizeof(buf));
    ASSERT_TRUE(result.IsOk()) << "Write failed: " << result.Error().message();
    EXPECT_GT(result.Value(), 0u);
}

TEST_F(AardvarkIntegrationTest, WriteReadTransaction) {
    core::Byte wbuf[1] = {0x00};
    core::Byte rbuf[4] = {};
    auto result = device_->WriteRead(addr_, wbuf, sizeof(wbuf), rbuf,
                                     sizeof(rbuf));
    ASSERT_TRUE(result.IsOk())
        << "WriteRead failed: " << result.Error().message();
    EXPECT_GT(result.Value(), 0u);
}

TEST_F(AardvarkIntegrationTest, SetBitrateOnLive) {
    auto result = device_->SetBitrate(400000);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device_->GetBitrate(), 400000u);
}

TEST_F(AardvarkIntegrationTest, CloseAndReopen) {
    auto close = device_->Close();
    ASSERT_TRUE(close.IsOk());
    EXPECT_EQ(device_->GetState(), DeviceState::kClosed);

    // Re-init and re-open
    auto init = device_->Init();
    ASSERT_TRUE(init.IsOk());
    auto open = device_->Open();
    ASSERT_TRUE(open.IsOk());
    EXPECT_EQ(device_->GetState(), DeviceState::kOpen);
}

TEST_F(AardvarkIntegrationTest, DeviceInfo) {
    EXPECT_EQ(device_->GetName(), "integ_dev");
    EXPECT_EQ(device_->GetUri(), uri_);
    EXPECT_EQ(device_->GetState(), DeviceState::kOpen);
}

}  // namespace
}  // namespace plas::hal::driver
