#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include "plas/backend/interface/device.h"
#include "plas/backend/interface/pci/pci_config.h"
#include "plas/core/error.h"

namespace plas::backend::pci {
namespace {

/// Mock device implementing Device + PciConfig for testing the ABC.
class MockPciConfigDevice : public Device, public PciConfig {
public:
    // Device interface
    core::Result<void> Init() override { return core::Result<void>::Ok(); }
    core::Result<void> Open() override { return core::Result<void>::Ok(); }
    core::Result<void> Close() override { return core::Result<void>::Ok(); }
    core::Result<void> Reset() override { return core::Result<void>::Ok(); }
    DeviceState GetState() const override { return DeviceState::kInitialized; }
    std::string GetName() const override { return "mock_pci_config"; }
    std::string GetUri() const override { return "mock://0:0"; }

    // PciConfig interface
    core::Result<core::Byte> ReadConfig8(Bdf bdf,
                                          ConfigOffset offset) override {
        last_bdf_ = bdf;
        last_offset_ = offset;
        return core::Result<core::Byte>::Ok(read8_value_);
    }

    core::Result<core::Word> ReadConfig16(Bdf bdf,
                                           ConfigOffset offset) override {
        last_bdf_ = bdf;
        last_offset_ = offset;
        return core::Result<core::Word>::Ok(read16_value_);
    }

    core::Result<core::DWord> ReadConfig32(Bdf bdf,
                                            ConfigOffset offset) override {
        last_bdf_ = bdf;
        last_offset_ = offset;
        return core::Result<core::DWord>::Ok(read32_value_);
    }

    core::Result<void> WriteConfig8(Bdf bdf, ConfigOffset offset,
                                    core::Byte value) override {
        last_bdf_ = bdf;
        last_offset_ = offset;
        written8_ = value;
        return core::Result<void>::Ok();
    }

    core::Result<void> WriteConfig16(Bdf bdf, ConfigOffset offset,
                                     core::Word value) override {
        last_bdf_ = bdf;
        last_offset_ = offset;
        written16_ = value;
        return core::Result<void>::Ok();
    }

    core::Result<void> WriteConfig32(Bdf bdf, ConfigOffset offset,
                                     core::DWord value) override {
        last_bdf_ = bdf;
        last_offset_ = offset;
        written32_ = value;
        return core::Result<void>::Ok();
    }

    core::Result<std::optional<ConfigOffset>> FindCapability(
        Bdf bdf, CapabilityId id) override {
        last_bdf_ = bdf;
        if (id == CapabilityId::kPciExpress) {
            return core::Result<std::optional<ConfigOffset>>::Ok(
                std::optional<ConfigOffset>(0x40));
        }
        return core::Result<std::optional<ConfigOffset>>::Ok(std::nullopt);
    }

    core::Result<std::optional<ConfigOffset>> FindExtCapability(
        Bdf bdf, ExtCapabilityId id) override {
        last_bdf_ = bdf;
        if (id == ExtCapabilityId::kDoe) {
            return core::Result<std::optional<ConfigOffset>>::Ok(
                std::optional<ConfigOffset>(0x150));
        }
        return core::Result<std::optional<ConfigOffset>>::Ok(std::nullopt);
    }

    // Test helpers
    Bdf last_bdf_{};
    ConfigOffset last_offset_ = 0;
    core::Byte read8_value_ = 0xAA;
    core::Word read16_value_ = 0xBBCC;
    core::DWord read32_value_ = 0xDDEEFF00;
    core::Byte written8_ = 0;
    core::Word written16_ = 0;
    core::DWord written32_ = 0;
};

// --- Dynamic cast capability check ---

TEST(PciConfigTest, DynamicCastFromDevice) {
    auto device = std::make_unique<MockPciConfigDevice>();
    auto* pci_config = dynamic_cast<PciConfig*>(device.get());
    EXPECT_NE(pci_config, nullptr)
        << "MockPciConfigDevice should implement PciConfig";
}

// --- ReadConfig typed methods ---

TEST(PciConfigTest, ReadConfig8) {
    MockPciConfigDevice device;
    device.read8_value_ = 0x42;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.ReadConfig8(bdf, 0x00);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0x42);
    EXPECT_EQ(device.last_bdf_, bdf);
    EXPECT_EQ(device.last_offset_, 0x00);
}

TEST(PciConfigTest, ReadConfig16) {
    MockPciConfigDevice device;
    device.read16_value_ = 0x8086;
    Bdf bdf{0x00, 0x02, 0x00};

    auto result = device.ReadConfig16(bdf, 0x00);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0x8086);
}

TEST(PciConfigTest, ReadConfig32) {
    MockPciConfigDevice device;
    device.read32_value_ = 0xDEADBEEF;
    Bdf bdf{0x01, 0x00, 0x00};

    auto result = device.ReadConfig32(bdf, 0x08);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0xDEADBEEF);
    EXPECT_EQ(device.last_offset_, 0x08);
}

// --- WriteConfig typed methods ---

TEST(PciConfigTest, WriteConfig8) {
    MockPciConfigDevice device;
    Bdf bdf{0x00, 0x03, 0x01};

    auto result = device.WriteConfig8(bdf, 0x04, 0x55);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.written8_, 0x55);
}

TEST(PciConfigTest, WriteConfig16) {
    MockPciConfigDevice device;
    Bdf bdf{0x00, 0x03, 0x01};

    auto result = device.WriteConfig16(bdf, 0x06, 0x1234);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.written16_, 0x1234);
}

TEST(PciConfigTest, WriteConfig32) {
    MockPciConfigDevice device;
    Bdf bdf{0x00, 0x03, 0x01};

    auto result = device.WriteConfig32(bdf, 0x10, 0xCAFEBABE);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.written32_, 0xCAFEBABE);
}

// --- FindCapability ---

TEST(PciConfigTest, FindCapabilityFound) {
    MockPciConfigDevice device;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.FindCapability(bdf, CapabilityId::kPciExpress);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(result.Value().has_value());
    EXPECT_EQ(result.Value().value(), 0x40);
}

TEST(PciConfigTest, FindCapabilityNotFound) {
    MockPciConfigDevice device;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.FindCapability(bdf, CapabilityId::kMsi);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().has_value());
}

// --- FindExtCapability ---

TEST(PciConfigTest, FindExtCapabilityFound) {
    MockPciConfigDevice device;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.FindExtCapability(bdf, ExtCapabilityId::kDoe);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(result.Value().has_value());
    EXPECT_EQ(result.Value().value(), 0x150);
}

TEST(PciConfigTest, FindExtCapabilityNotFound) {
    MockPciConfigDevice device;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.FindExtCapability(bdf, ExtCapabilityId::kAer);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().has_value());
}

}  // namespace
}  // namespace plas::backend::pci
