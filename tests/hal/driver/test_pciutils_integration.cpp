#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "plas/config/device_entry.h"
#include "plas/hal/driver/pciutils/pciutils_device.h"
#include "plas/hal/interface/pci/pci_config.h"
#include "plas/hal/interface/pci/pci_doe.h"
#include "plas/hal/interface/pci/types.h"

namespace plas::hal::driver {
namespace {

/// Integration tests that require a real PCI device.
///
/// Set PLAS_TEST_PCIUTILS_BDF=DDDD:BB:DD.F to enable.
/// Example: PLAS_TEST_PCIUTILS_BDF=0000:03:00.0

class PciUtilsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* env = std::getenv("PLAS_TEST_PCIUTILS_BDF");
        if (!env || std::string(env).empty()) {
            GTEST_SKIP() << "PLAS_TEST_PCIUTILS_BDF not set; skipping "
                            "integration tests";
        }
        bdf_str_ = env;
        uri_ = "pciutils://" + bdf_str_;

        config::DeviceEntry entry{"integ_dev", uri_, "pciutils", {}};
        device_ = std::make_unique<PciUtilsDevice>(entry);

        auto init = device_->Init();
        ASSERT_TRUE(init.IsOk()) << "Init failed";

        auto open = device_->Open();
        ASSERT_TRUE(open.IsOk()) << "Open failed";

        // Parse BDF for use in test methods.
        unsigned d, b, dev, f;
        ASSERT_EQ(std::sscanf(bdf_str_.c_str(), "%x:%x:%x.%x",
                              &d, &b, &dev, &f),
                  4);
        bdf_.bus = static_cast<uint8_t>(b);
        bdf_.device = static_cast<uint8_t>(dev);
        bdf_.function = static_cast<uint8_t>(f);
    }

    void TearDown() override {
        if (device_ && device_->GetState() == DeviceState::kOpen) {
            device_->Close();
        }
    }

    std::string bdf_str_;
    std::string uri_;
    pci::Bdf bdf_{};
    std::unique_ptr<PciUtilsDevice> device_;
};

TEST_F(PciUtilsIntegrationTest, ReadVendorAndDeviceId) {
    auto vendor = device_->ReadConfig16(bdf_, 0x00);
    ASSERT_TRUE(vendor.IsOk());
    EXPECT_NE(vendor.Value(), 0x0000);
    EXPECT_NE(vendor.Value(), 0xFFFF);

    auto device_id = device_->ReadConfig16(bdf_, 0x02);
    ASSERT_TRUE(device_id.IsOk());
    EXPECT_NE(device_id.Value(), 0xFFFF);

    std::printf("  Vendor=0x%04X  Device=0x%04X\n", vendor.Value(),
                device_id.Value());
}

TEST_F(PciUtilsIntegrationTest, ReadConfig32_ClassCode) {
    auto rev_class = device_->ReadConfig32(bdf_, 0x08);
    ASSERT_TRUE(rev_class.IsOk());
    uint8_t class_code = static_cast<uint8_t>((rev_class.Value() >> 24) & 0xFF);
    std::printf("  Class=0x%02X\n", class_code);
}

TEST_F(PciUtilsIntegrationTest, FindCapability_PciExpress) {
    auto cap = device_->FindCapability(bdf_, pci::CapabilityId::kPciExpress);
    ASSERT_TRUE(cap.IsOk());
    if (cap.Value().has_value()) {
        std::printf("  PCI Express capability at offset 0x%03X\n",
                    cap.Value().value());
    } else {
        std::printf("  No PCI Express capability found\n");
    }
}

TEST_F(PciUtilsIntegrationTest, FindExtCapability_Aer) {
    auto cap = device_->FindExtCapability(bdf_, pci::ExtCapabilityId::kAer);
    ASSERT_TRUE(cap.IsOk());
    if (cap.Value().has_value()) {
        std::printf("  AER extended capability at offset 0x%03X\n",
                    cap.Value().value());
    } else {
        std::printf("  No AER extended capability found\n");
    }
}

TEST_F(PciUtilsIntegrationTest, FindExtCapability_Doe) {
    auto cap = device_->FindExtCapability(bdf_, pci::ExtCapabilityId::kDoe);
    ASSERT_TRUE(cap.IsOk());
    if (cap.Value().has_value()) {
        std::printf("  DOE extended capability at offset 0x%03X\n",
                    cap.Value().value());
    } else {
        std::printf("  No DOE extended capability found (skipping DOE tests)\n");
    }
}

TEST_F(PciUtilsIntegrationTest, DoeDiscover) {
    auto cap = device_->FindExtCapability(bdf_, pci::ExtCapabilityId::kDoe);
    ASSERT_TRUE(cap.IsOk());
    if (!cap.Value().has_value()) {
        GTEST_SKIP() << "Device has no DOE capability";
    }

    auto doe_offset = cap.Value().value();
    auto protocols = device_->DoeDiscover(bdf_, doe_offset);
    ASSERT_TRUE(protocols.IsOk())
        << "DoeDiscover failed: " << protocols.Error().message();

    std::printf("  DOE protocols discovered: %zu\n",
                protocols.Value().size());
    for (const auto& p : protocols.Value()) {
        std::printf("    VendorID=0x%04X  Type=0x%02X\n", p.vendor_id,
                    p.data_object_type);
    }
    EXPECT_FALSE(protocols.Value().empty());
}

}  // namespace
}  // namespace plas::hal::driver
