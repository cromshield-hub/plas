#include <gtest/gtest.h>

#include "plas/backend/interface/pci/types.h"

namespace plas::backend::pci {
namespace {

// --- Bdf Pack / FromPacked ---

TEST(BdfTest, PackBasic) {
    Bdf bdf{0x01, 0x02, 0x03};
    // bus=0x01 << 8 | device=0x02 << 3 | function=0x03
    EXPECT_EQ(bdf.Pack(), 0x0113);
}

TEST(BdfTest, PackZero) {
    Bdf bdf{0, 0, 0};
    EXPECT_EQ(bdf.Pack(), 0x0000);
}

TEST(BdfTest, PackMaxValues) {
    Bdf bdf{0xFF, 0x1F, 0x07};
    EXPECT_EQ(bdf.Pack(), 0xFFFF);
}

TEST(BdfTest, FromPackedBasic) {
    auto bdf = Bdf::FromPacked(0x0113);
    EXPECT_EQ(bdf.bus, 0x01);
    EXPECT_EQ(bdf.device, 0x02);
    EXPECT_EQ(bdf.function, 0x03);
}

TEST(BdfTest, FromPackedZero) {
    auto bdf = Bdf::FromPacked(0x0000);
    EXPECT_EQ(bdf.bus, 0);
    EXPECT_EQ(bdf.device, 0);
    EXPECT_EQ(bdf.function, 0);
}

TEST(BdfTest, FromPackedMax) {
    auto bdf = Bdf::FromPacked(0xFFFF);
    EXPECT_EQ(bdf.bus, 0xFF);
    EXPECT_EQ(bdf.device, 0x1F);
    EXPECT_EQ(bdf.function, 0x07);
}

TEST(BdfTest, PackFromPackedRoundtrip) {
    Bdf original{0xAB, 0x0C, 0x05};
    auto packed = original.Pack();
    auto restored = Bdf::FromPacked(packed);
    EXPECT_EQ(original, restored);
}

TEST(BdfTest, PackMasksDeviceBits) {
    // device has only 5 valid bits; upper bits should be masked
    Bdf bdf{0x00, 0xFF, 0x00};  // device=0xFF but only 0x1F valid
    auto restored = Bdf::FromPacked(bdf.Pack());
    EXPECT_EQ(restored.device, 0x1F);
}

TEST(BdfTest, PackMasksFunctionBits) {
    // function has only 3 valid bits; upper bits should be masked
    Bdf bdf{0x00, 0x00, 0xFF};  // function=0xFF but only 0x07 valid
    auto restored = Bdf::FromPacked(bdf.Pack());
    EXPECT_EQ(restored.function, 0x07);
}

// --- Bdf comparison operators ---

TEST(BdfTest, EqualityEqual) {
    Bdf a{1, 2, 3};
    Bdf b{1, 2, 3};
    EXPECT_EQ(a, b);
}

TEST(BdfTest, EqualityDifferentBus) {
    Bdf a{1, 2, 3};
    Bdf b{2, 2, 3};
    EXPECT_NE(a, b);
}

TEST(BdfTest, EqualityDifferentDevice) {
    Bdf a{1, 2, 3};
    Bdf b{1, 3, 3};
    EXPECT_NE(a, b);
}

TEST(BdfTest, EqualityDifferentFunction) {
    Bdf a{1, 2, 3};
    Bdf b{1, 2, 4};
    EXPECT_NE(a, b);
}

// --- DoeProtocolId comparison ---

TEST(DoeProtocolIdTest, Equal) {
    DoeProtocolId a{doe_vendor::kPciSig, doe_type::kDoeDiscovery};
    DoeProtocolId b{doe_vendor::kPciSig, doe_type::kDoeDiscovery};
    EXPECT_EQ(a, b);
}

TEST(DoeProtocolIdTest, NotEqual) {
    DoeProtocolId a{doe_vendor::kPciSig, doe_type::kDoeDiscovery};
    DoeProtocolId b{doe_vendor::kPciSig, doe_type::kCma};
    EXPECT_NE(a, b);
}

// --- Constexpr validation ---

TEST(BdfTest, ConstexprPack) {
    constexpr Bdf bdf{0x00, 0x1F, 0x07};
    constexpr auto packed = bdf.Pack();
    static_assert(packed == 0x00FF, "Pack should work at compile time");
    EXPECT_EQ(packed, 0x00FF);
}

TEST(BdfTest, ConstexprFromPacked) {
    constexpr auto bdf = Bdf::FromPacked(0x00FF);
    static_assert(bdf.bus == 0x00, "FromPacked bus");
    static_assert(bdf.device == 0x1F, "FromPacked device");
    static_assert(bdf.function == 0x07, "FromPacked function");
    EXPECT_EQ(bdf.device, 0x1F);
}

}  // namespace
}  // namespace plas::backend::pci
