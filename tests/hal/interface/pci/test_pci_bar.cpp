#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <vector>

#include "plas/hal/interface/device.h"
#include "plas/hal/interface/pci/pci_bar.h"
#include "plas/core/error.h"

namespace plas::hal::pci {
namespace {

/// Mock device implementing Device + PciBar for testing the ABC.
class MockPciBarDevice : public Device, public PciBar {
public:
    // Device interface
    core::Result<void> Init() override { return core::Result<void>::Ok(); }
    core::Result<void> Open() override { return core::Result<void>::Ok(); }
    core::Result<void> Close() override { return core::Result<void>::Ok(); }
    core::Result<void> Reset() override { return core::Result<void>::Ok(); }
    DeviceState GetState() const override { return DeviceState::kOpen; }
    std::string GetName() const override { return "mock_pci_bar"; }
    std::string GetUri() const override { return "mock://0:0"; }

    // PciBar interface
    core::Result<core::DWord> BarRead32(Bdf bdf, uint8_t bar_index,
                                         uint64_t offset) override {
        last_bdf_ = bdf;
        last_bar_index_ = bar_index;
        last_offset_ = offset;
        if (error_) return core::Result<core::DWord>::Err(core::ErrorCode::kIOError);
        return core::Result<core::DWord>::Ok(read32_value_);
    }

    core::Result<core::QWord> BarRead64(Bdf bdf, uint8_t bar_index,
                                         uint64_t offset) override {
        last_bdf_ = bdf;
        last_bar_index_ = bar_index;
        last_offset_ = offset;
        if (error_) return core::Result<core::QWord>::Err(core::ErrorCode::kIOError);
        return core::Result<core::QWord>::Ok(read64_value_);
    }

    core::Result<void> BarWrite32(Bdf bdf, uint8_t bar_index, uint64_t offset,
                                   core::DWord value) override {
        last_bdf_ = bdf;
        last_bar_index_ = bar_index;
        last_offset_ = offset;
        last_write32_ = value;
        if (error_) return core::Result<void>::Err(core::ErrorCode::kIOError);
        return core::Result<void>::Ok();
    }

    core::Result<void> BarWrite64(Bdf bdf, uint8_t bar_index, uint64_t offset,
                                   core::QWord value) override {
        last_bdf_ = bdf;
        last_bar_index_ = bar_index;
        last_offset_ = offset;
        last_write64_ = value;
        if (error_) return core::Result<void>::Err(core::ErrorCode::kIOError);
        return core::Result<void>::Ok();
    }

    core::Result<void> BarReadBuffer(Bdf bdf, uint8_t bar_index,
                                      uint64_t offset, void* buffer,
                                      std::size_t length) override {
        last_bdf_ = bdf;
        last_bar_index_ = bar_index;
        last_offset_ = offset;
        last_length_ = length;
        if (error_) return core::Result<void>::Err(core::ErrorCode::kIOError);
        if (buffer && length <= read_buffer_.size()) {
            std::memcpy(buffer, read_buffer_.data(), length);
        }
        return core::Result<void>::Ok();
    }

    core::Result<void> BarWriteBuffer(Bdf bdf, uint8_t bar_index,
                                       uint64_t offset, const void* buffer,
                                       std::size_t length) override {
        last_bdf_ = bdf;
        last_bar_index_ = bar_index;
        last_offset_ = offset;
        last_length_ = length;
        if (error_) return core::Result<void>::Err(core::ErrorCode::kIOError);
        if (buffer && length > 0) {
            write_buffer_.resize(length);
            std::memcpy(write_buffer_.data(), buffer, length);
        }
        return core::Result<void>::Ok();
    }

    // Test helpers
    Bdf last_bdf_{};
    uint8_t last_bar_index_ = 0;
    uint64_t last_offset_ = 0;
    std::size_t last_length_ = 0;
    core::DWord last_write32_ = 0;
    core::QWord last_write64_ = 0;

    core::DWord read32_value_ = 0;
    core::QWord read64_value_ = 0;
    std::vector<uint8_t> read_buffer_;
    std::vector<uint8_t> write_buffer_;
    bool error_ = false;
};

// --- Dynamic cast capability check ---

TEST(PciBarTest, DynamicCastFromDevice) {
    auto device = std::make_unique<MockPciBarDevice>();
    auto* pci_bar = dynamic_cast<PciBar*>(device.get());
    EXPECT_NE(pci_bar, nullptr)
        << "MockPciBarDevice should implement PciBar";
}

// --- BarRead32 ---

TEST(PciBarTest, BarRead32Success) {
    MockPciBarDevice device;
    device.read32_value_ = 0xDEADBEEF;
    Bdf bdf{0x03, 0x00, 0x00};

    auto result = device.BarRead32(bdf, 0, 0x1000);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0xDEADBEEFu);
}

TEST(PciBarTest, BarRead32PassesArgs) {
    MockPciBarDevice device;
    Bdf bdf{0x05, 0x0A, 0x02};

    device.BarRead32(bdf, 3, 0x2000);
    EXPECT_EQ(device.last_bdf_, bdf);
    EXPECT_EQ(device.last_bar_index_, 3u);
    EXPECT_EQ(device.last_offset_, 0x2000u);
}

TEST(PciBarTest, BarRead32Error) {
    MockPciBarDevice device;
    device.error_ = true;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.BarRead32(bdf, 0, 0);
    EXPECT_TRUE(result.IsError());
}

// --- BarRead64 ---

TEST(PciBarTest, BarRead64Success) {
    MockPciBarDevice device;
    device.read64_value_ = 0x0123456789ABCDEFull;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.BarRead64(bdf, 0, 0x00);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0x0123456789ABCDEFull);
}

// --- BarWrite32 ---

TEST(PciBarTest, BarWrite32Success) {
    MockPciBarDevice device;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.BarWrite32(bdf, 1, 0x14, 0xCAFEBABE);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.last_write32_, 0xCAFEBABEu);
    EXPECT_EQ(device.last_bar_index_, 1u);
    EXPECT_EQ(device.last_offset_, 0x14u);
}

TEST(PciBarTest, BarWrite32Error) {
    MockPciBarDevice device;
    device.error_ = true;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.BarWrite32(bdf, 0, 0, 0);
    EXPECT_TRUE(result.IsError());
}

// --- BarWrite64 ---

TEST(PciBarTest, BarWrite64Success) {
    MockPciBarDevice device;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.BarWrite64(bdf, 2, 0x08, 0xFEDCBA9876543210ull);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.last_write64_, 0xFEDCBA9876543210ull);
}

// --- BarReadBuffer ---

TEST(PciBarTest, BarReadBufferSuccess) {
    MockPciBarDevice device;
    device.read_buffer_ = {0x11, 0x22, 0x33, 0x44};
    Bdf bdf{0x00, 0x1F, 0x00};

    uint8_t buf[4] = {};
    auto result = device.BarReadBuffer(bdf, 0, 0x100, buf, 4);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(buf[0], 0x11);
    EXPECT_EQ(buf[1], 0x22);
    EXPECT_EQ(buf[2], 0x33);
    EXPECT_EQ(buf[3], 0x44);
}

TEST(PciBarTest, BarReadBufferPassesArgs) {
    MockPciBarDevice device;
    device.read_buffer_.resize(16, 0);
    Bdf bdf{0x0A, 0x02, 0x01};

    uint8_t buf[16] = {};
    device.BarReadBuffer(bdf, 4, 0x500, buf, 16);
    EXPECT_EQ(device.last_bdf_, bdf);
    EXPECT_EQ(device.last_bar_index_, 4u);
    EXPECT_EQ(device.last_offset_, 0x500u);
    EXPECT_EQ(device.last_length_, 16u);
}

// --- BarWriteBuffer ---

TEST(PciBarTest, BarWriteBufferSuccess) {
    MockPciBarDevice device;
    Bdf bdf{0x00, 0x1F, 0x00};

    uint8_t buf[] = {0xAA, 0xBB, 0xCC, 0xDD};
    auto result = device.BarWriteBuffer(bdf, 0, 0x200, buf, 4);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(device.write_buffer_.size(), 4u);
    EXPECT_EQ(device.write_buffer_[0], 0xAA);
    EXPECT_EQ(device.write_buffer_[3], 0xDD);
}

TEST(PciBarTest, BarWriteBufferError) {
    MockPciBarDevice device;
    device.error_ = true;
    Bdf bdf{0x00, 0x1F, 0x00};

    uint8_t buf[] = {0x01};
    auto result = device.BarWriteBuffer(bdf, 0, 0, buf, 1);
    EXPECT_TRUE(result.IsError());
}

// --- Various bar indices ---

TEST(PciBarTest, BarIndexRange) {
    MockPciBarDevice device;
    device.read32_value_ = 0x42;
    Bdf bdf{0x00, 0x00, 0x00};

    // BAR index 5 is the maximum valid
    auto result = device.BarRead32(bdf, 5, 0);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(device.last_bar_index_, 5u);
}

}  // namespace
}  // namespace plas::hal::pci
