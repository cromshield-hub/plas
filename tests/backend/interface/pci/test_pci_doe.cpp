#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "plas/backend/interface/device.h"
#include "plas/backend/interface/pci/pci_doe.h"
#include "plas/core/error.h"

namespace plas::backend::pci {
namespace {

/// Mock device implementing Device + PciDoe for testing the ABC.
class MockPciDoeDevice : public Device, public PciDoe {
public:
    // Device interface
    core::Result<void> Init() override { return core::Result<void>::Ok(); }
    core::Result<void> Open() override { return core::Result<void>::Ok(); }
    core::Result<void> Close() override { return core::Result<void>::Ok(); }
    core::Result<void> Reset() override { return core::Result<void>::Ok(); }
    DeviceState GetState() const override { return DeviceState::kInitialized; }
    std::string GetName() const override { return "mock_pci_doe"; }
    std::string GetUri() const override { return "mock://0:0"; }

    // PciDoe interface
    core::Result<std::vector<DoeProtocolId>> DoeDiscover(
        Bdf bdf, ConfigOffset doe_offset) override {
        last_bdf_ = bdf;
        last_doe_offset_ = doe_offset;
        return core::Result<std::vector<DoeProtocolId>>::Ok(
            discover_protocols_);
    }

    core::Result<DoePayload> DoeExchange(Bdf bdf, ConfigOffset doe_offset,
                                          DoeProtocolId protocol,
                                          const DoePayload& request) override {
        last_bdf_ = bdf;
        last_doe_offset_ = doe_offset;
        last_protocol_ = protocol;
        last_request_ = request;
        if (exchange_error_) {
            return core::Result<DoePayload>::Err(
                core::ErrorCode::kIOError);
        }
        return core::Result<DoePayload>::Ok(exchange_response_);
    }

    // Test helpers
    Bdf last_bdf_{};
    ConfigOffset last_doe_offset_ = 0;
    DoeProtocolId last_protocol_{0, 0};
    DoePayload last_request_;

    std::vector<DoeProtocolId> discover_protocols_;
    DoePayload exchange_response_;
    bool exchange_error_ = false;
};

// --- Dynamic cast capability check ---

TEST(PciDoeTest, DynamicCastFromDevice) {
    auto device = std::make_unique<MockPciDoeDevice>();
    auto* pci_doe = dynamic_cast<PciDoe*>(device.get());
    EXPECT_NE(pci_doe, nullptr)
        << "MockPciDoeDevice should implement PciDoe";
}

// --- DoeDiscover ---

TEST(PciDoeTest, DoeDiscoverEmpty) {
    MockPciDoeDevice device;
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.DoeDiscover(bdf, 0x150);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().empty());
}

TEST(PciDoeTest, DoeDiscoverMultipleProtocols) {
    MockPciDoeDevice device;
    device.discover_protocols_ = {
        {doe_vendor::kPciSig, doe_type::kDoeDiscovery},
        {doe_vendor::kPciSig, doe_type::kCma},
    };
    Bdf bdf{0x00, 0x1F, 0x00};

    auto result = device.DoeDiscover(bdf, 0x150);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value().size(), 2u);
    EXPECT_EQ(result.Value()[0].vendor_id, doe_vendor::kPciSig);
    EXPECT_EQ(result.Value()[0].data_object_type, doe_type::kDoeDiscovery);
    EXPECT_EQ(result.Value()[1].data_object_type, doe_type::kCma);
}

TEST(PciDoeTest, DoeDiscoverPassesBdfAndOffset) {
    MockPciDoeDevice device;
    Bdf bdf{0x03, 0x0A, 0x02};

    device.DoeDiscover(bdf, 0x200);
    EXPECT_EQ(device.last_bdf_, bdf);
    EXPECT_EQ(device.last_doe_offset_, 0x200);
}

// --- DoeExchange ---

TEST(PciDoeTest, DoeExchangeSuccess) {
    MockPciDoeDevice device;
    device.exchange_response_ = {0x11223344, 0x55667788};

    Bdf bdf{0x00, 0x1F, 0x00};
    DoeProtocolId protocol{doe_vendor::kPciSig, doe_type::kCma};
    DoePayload request = {0xAABBCCDD};

    auto result = device.DoeExchange(bdf, 0x150, protocol, request);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value().size(), 2u);
    EXPECT_EQ(result.Value()[0], 0x11223344u);
    EXPECT_EQ(result.Value()[1], 0x55667788u);
}

TEST(PciDoeTest, DoeExchangePassesProtocolAndRequest) {
    MockPciDoeDevice device;
    DoeProtocolId protocol{doe_vendor::kPciSig, doe_type::kCma};
    DoePayload request = {0x01, 0x02, 0x03};
    Bdf bdf{0x00, 0x1F, 0x00};

    device.DoeExchange(bdf, 0x150, protocol, request);
    EXPECT_EQ(device.last_protocol_, protocol);
    ASSERT_EQ(device.last_request_.size(), 3u);
    EXPECT_EQ(device.last_request_[0], 0x01u);
}

TEST(PciDoeTest, DoeExchangeError) {
    MockPciDoeDevice device;
    device.exchange_error_ = true;

    Bdf bdf{0x00, 0x1F, 0x00};
    DoeProtocolId protocol{doe_vendor::kPciSig, doe_type::kCma};
    DoePayload request = {0x01};

    auto result = device.DoeExchange(bdf, 0x150, protocol, request);
    ASSERT_TRUE(result.IsError());
}

}  // namespace
}  // namespace plas::backend::pci
