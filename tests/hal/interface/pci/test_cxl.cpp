#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

#include "plas/core/error.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/pci/cxl.h"

namespace plas::hal::pci {
namespace {

/// Mock device implementing Device + Cxl for testing the ABC.
class MockCxlDevice : public Device, public Cxl {
public:
    // Device interface
    core::Result<void> Init() override { return core::Result<void>::Ok(); }
    core::Result<void> Open() override { return core::Result<void>::Ok(); }
    core::Result<void> Close() override { return core::Result<void>::Ok(); }
    core::Result<void> Reset() override { return core::Result<void>::Ok(); }
    DeviceState GetState() const override { return DeviceState::kInitialized; }
    std::string GetName() const override { return "mock_cxl"; }
    std::string GetUri() const override { return "mock://0:0"; }

    // Cxl interface
    core::Result<std::vector<DvsecHeader>> EnumerateCxlDvsecs(
        Bdf bdf) override {
        last_bdf_ = bdf;
        if (enumerate_error_) {
            return core::Result<std::vector<DvsecHeader>>::Err(
                core::ErrorCode::kIOError);
        }
        return core::Result<std::vector<DvsecHeader>>::Ok(dvsec_headers_);
    }

    core::Result<std::optional<DvsecHeader>> FindCxlDvsec(
        Bdf bdf, CxlDvsecId dvsec_id) override {
        last_bdf_ = bdf;
        for (const auto& h : dvsec_headers_) {
            if (h.dvsec_id == dvsec_id) {
                return core::Result<std::optional<DvsecHeader>>::Ok(h);
            }
        }
        return core::Result<std::optional<DvsecHeader>>::Ok(std::nullopt);
    }

    core::Result<CxlDeviceType> GetCxlDeviceType(Bdf bdf) override {
        last_bdf_ = bdf;
        return core::Result<CxlDeviceType>::Ok(device_type_);
    }

    core::Result<std::vector<RegisterBlockEntry>> GetRegisterBlocks(
        Bdf bdf) override {
        last_bdf_ = bdf;
        return core::Result<std::vector<RegisterBlockEntry>>::Ok(
            register_blocks_);
    }

    core::Result<core::DWord> ReadDvsecRegister(
        Bdf bdf, ConfigOffset dvsec_offset, uint16_t reg_offset) override {
        last_bdf_ = bdf;
        last_dvsec_offset_ = dvsec_offset;
        last_reg_offset_ = reg_offset;
        if (read_error_) {
            return core::Result<core::DWord>::Err(core::ErrorCode::kIOError);
        }
        return core::Result<core::DWord>::Ok(read_value_);
    }

    core::Result<void> WriteDvsecRegister(
        Bdf bdf, ConfigOffset dvsec_offset, uint16_t reg_offset,
        core::DWord value) override {
        last_bdf_ = bdf;
        last_dvsec_offset_ = dvsec_offset;
        last_reg_offset_ = reg_offset;
        last_write_value_ = value;
        if (write_error_) {
            return core::Result<void>::Err(core::ErrorCode::kIOError);
        }
        return core::Result<void>::Ok();
    }

    // Test helpers
    Bdf last_bdf_{};
    ConfigOffset last_dvsec_offset_ = 0;
    uint16_t last_reg_offset_ = 0;
    core::DWord last_write_value_ = 0;

    std::vector<DvsecHeader> dvsec_headers_;
    CxlDeviceType device_type_ = CxlDeviceType::kUnknown;
    std::vector<RegisterBlockEntry> register_blocks_;
    core::DWord read_value_ = 0;

    bool enumerate_error_ = false;
    bool read_error_ = false;
    bool write_error_ = false;
};

// --- Dynamic cast ---

TEST(CxlTest, DynamicCastFromDevice) {
    auto device = std::make_unique<MockCxlDevice>();
    auto* cxl = dynamic_cast<Cxl*>(device.get());
    EXPECT_NE(cxl, nullptr) << "MockCxlDevice should implement Cxl";
}

TEST(CxlTest, DynamicCastNullForNonCxl) {
    class PlainDevice : public Device {
    public:
        core::Result<void> Init() override { return core::Result<void>::Ok(); }
        core::Result<void> Open() override { return core::Result<void>::Ok(); }
        core::Result<void> Close() override { return core::Result<void>::Ok(); }
        core::Result<void> Reset() override { return core::Result<void>::Ok(); }
        DeviceState GetState() const override { return DeviceState::kInitialized; }
        std::string GetName() const override { return "plain"; }
        std::string GetUri() const override { return "plain://0"; }
    };
    auto device = std::make_unique<PlainDevice>();
    auto* cxl = dynamic_cast<Cxl*>(device.get());
    EXPECT_EQ(cxl, nullptr);
}

// --- EnumerateCxlDvsecs ---

TEST(CxlTest, EnumerateEmpty) {
    MockCxlDevice device;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.EnumerateCxlDvsecs(bdf);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().empty());
}

TEST(CxlTest, EnumerateMultipleDvsecs) {
    MockCxlDevice device;
    device.dvsec_headers_ = {
        {0x100, cxl::kCxlVendorId, 1, CxlDvsecId::kCxlDevice, 0x3C},
        {0x200, cxl::kCxlVendorId, 1, CxlDvsecId::kRegisterLocator, 0x24},
    };
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.EnumerateCxlDvsecs(bdf);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value().size(), 2u);
    EXPECT_EQ(result.Value()[0].dvsec_id, CxlDvsecId::kCxlDevice);
    EXPECT_EQ(result.Value()[1].dvsec_id, CxlDvsecId::kRegisterLocator);
}

TEST(CxlTest, EnumerateError) {
    MockCxlDevice device;
    device.enumerate_error_ = true;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.EnumerateCxlDvsecs(bdf);
    ASSERT_TRUE(result.IsError());
}

TEST(CxlTest, EnumeratePassesBdf) {
    MockCxlDevice device;
    Bdf bdf{0x03, 0x0A, 0x02};

    device.EnumerateCxlDvsecs(bdf);
    EXPECT_EQ(device.last_bdf_, bdf);
}

// --- FindCxlDvsec ---

TEST(CxlTest, FindDvsecFound) {
    MockCxlDevice device;
    device.dvsec_headers_ = {
        {0x100, cxl::kCxlVendorId, 1, CxlDvsecId::kCxlDevice, 0x3C},
        {0x200, cxl::kCxlVendorId, 1, CxlDvsecId::kRegisterLocator, 0x24},
    };
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.FindCxlDvsec(bdf, CxlDvsecId::kRegisterLocator);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(result.Value().has_value());
    EXPECT_EQ(result.Value()->offset, 0x200);
}

TEST(CxlTest, FindDvsecNotFound) {
    MockCxlDevice device;
    device.dvsec_headers_ = {
        {0x100, cxl::kCxlVendorId, 1, CxlDvsecId::kCxlDevice, 0x3C},
    };
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.FindCxlDvsec(bdf, CxlDvsecId::kMld);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().has_value());
}

// --- GetCxlDeviceType ---

TEST(CxlTest, GetDeviceTypeType3) {
    MockCxlDevice device;
    device.device_type_ = CxlDeviceType::kType3;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.GetCxlDeviceType(bdf);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), CxlDeviceType::kType3);
}

// --- GetRegisterBlocks ---

TEST(CxlTest, GetRegisterBlocksEmpty) {
    MockCxlDevice device;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.GetRegisterBlocks(bdf);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().empty());
}

TEST(CxlTest, GetRegisterBlocksMultiple) {
    MockCxlDevice device;
    device.register_blocks_ = {
        {CxlRegisterBlockId::kComponentRegister, 0, 0x10000},
        {CxlRegisterBlockId::kCxlDeviceRegister, 2, 0x20000},
    };
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.GetRegisterBlocks(bdf);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value().size(), 2u);
    EXPECT_EQ(result.Value()[0].block_id, CxlRegisterBlockId::kComponentRegister);
    EXPECT_EQ(result.Value()[1].bar_index, 2);
}

// --- ReadDvsecRegister / WriteDvsecRegister ---

TEST(CxlTest, ReadDvsecRegisterSuccess) {
    MockCxlDevice device;
    device.read_value_ = 0xDEADBEEF;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.ReadDvsecRegister(bdf, 0x100, 0x04);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0xDEADBEEFu);
    EXPECT_EQ(device.last_dvsec_offset_, 0x100);
    EXPECT_EQ(device.last_reg_offset_, 0x04);
}

TEST(CxlTest, ReadDvsecRegisterError) {
    MockCxlDevice device;
    device.read_error_ = true;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.ReadDvsecRegister(bdf, 0x100, 0x04);
    ASSERT_TRUE(result.IsError());
}

TEST(CxlTest, WriteDvsecRegisterSuccess) {
    MockCxlDevice device;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.WriteDvsecRegister(bdf, 0x100, 0x08, 0xCAFEBABE);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.last_write_value_, 0xCAFEBABEu);
}

TEST(CxlTest, WriteDvsecRegisterError) {
    MockCxlDevice device;
    device.write_error_ = true;
    Bdf bdf{0x00, 0x03, 0x00};

    auto result = device.WriteDvsecRegister(bdf, 0x100, 0x08, 0x00);
    ASSERT_TRUE(result.IsError());
}

}  // namespace
}  // namespace plas::hal::pci
