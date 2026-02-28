#include <gtest/gtest.h>

#include "plas/hal/interface/pci/cxl_types.h"

namespace plas::hal::pci {
namespace {

// --- CXL vendor ID ---

TEST(CxlTypesTest, CxlVendorId) {
    EXPECT_EQ(cxl::kCxlVendorId, 0x1E98);
}

// --- CxlDvsecId enum values ---

TEST(CxlTypesTest, DvsecIdValues) {
    EXPECT_EQ(static_cast<uint16_t>(CxlDvsecId::kCxlDevice), 0x0000);
    EXPECT_EQ(static_cast<uint16_t>(CxlDvsecId::kNonCxlFunction), 0x0002);
    EXPECT_EQ(static_cast<uint16_t>(CxlDvsecId::kCxlExtensionsPort), 0x0003);
    EXPECT_EQ(static_cast<uint16_t>(CxlDvsecId::kGpfPort), 0x0004);
    EXPECT_EQ(static_cast<uint16_t>(CxlDvsecId::kGpfDevice), 0x0005);
    EXPECT_EQ(static_cast<uint16_t>(CxlDvsecId::kFlexBusPort), 0x0007);
    EXPECT_EQ(static_cast<uint16_t>(CxlDvsecId::kRegisterLocator), 0x0008);
    EXPECT_EQ(static_cast<uint16_t>(CxlDvsecId::kMld), 0x0009);
}

// --- CxlDeviceType enum values ---

TEST(CxlTypesTest, DeviceTypeValues) {
    EXPECT_EQ(static_cast<uint8_t>(CxlDeviceType::kType1), 1);
    EXPECT_EQ(static_cast<uint8_t>(CxlDeviceType::kType2), 2);
    EXPECT_EQ(static_cast<uint8_t>(CxlDeviceType::kType3), 3);
    EXPECT_EQ(static_cast<uint8_t>(CxlDeviceType::kUnknown), 0xFF);
}

// --- DvsecHeader struct ---

TEST(CxlTypesTest, DvsecHeaderEquality) {
    DvsecHeader a{0x100, cxl::kCxlVendorId, 1, CxlDvsecId::kCxlDevice, 0x3C};
    DvsecHeader b{0x100, cxl::kCxlVendorId, 1, CxlDvsecId::kCxlDevice, 0x3C};
    EXPECT_EQ(a, b);
}

TEST(CxlTypesTest, DvsecHeaderInequality) {
    DvsecHeader a{0x100, cxl::kCxlVendorId, 1, CxlDvsecId::kCxlDevice, 0x3C};
    DvsecHeader b{0x200, cxl::kCxlVendorId, 1, CxlDvsecId::kCxlDevice, 0x3C};
    EXPECT_NE(a, b);
}

// --- CxlRegisterBlockId enum ---

TEST(CxlTypesTest, RegisterBlockIdValues) {
    EXPECT_EQ(static_cast<uint8_t>(CxlRegisterBlockId::kEmpty), 0);
    EXPECT_EQ(static_cast<uint8_t>(CxlRegisterBlockId::kComponentRegister), 1);
    EXPECT_EQ(static_cast<uint8_t>(CxlRegisterBlockId::kBarVirtualized), 2);
    EXPECT_EQ(static_cast<uint8_t>(CxlRegisterBlockId::kCxlDeviceRegister), 3);
}

// --- RegisterBlockEntry struct ---

TEST(CxlTypesTest, RegisterBlockEntryEquality) {
    RegisterBlockEntry a{CxlRegisterBlockId::kComponentRegister, 0, 0x1000};
    RegisterBlockEntry b{CxlRegisterBlockId::kComponentRegister, 0, 0x1000};
    EXPECT_EQ(a, b);
}

TEST(CxlTypesTest, RegisterBlockEntryInequality) {
    RegisterBlockEntry a{CxlRegisterBlockId::kComponentRegister, 0, 0x1000};
    RegisterBlockEntry b{CxlRegisterBlockId::kCxlDeviceRegister, 0, 0x1000};
    EXPECT_NE(a, b);
}

// --- CxlMailboxOpcode enum ---

TEST(CxlTypesTest, MailboxOpcodeValues) {
    EXPECT_EQ(static_cast<uint16_t>(CxlMailboxOpcode::kIdentify), 0x0001);
    EXPECT_EQ(static_cast<uint16_t>(CxlMailboxOpcode::kGetHealthInfo), 0x4200);
    EXPECT_EQ(static_cast<uint16_t>(CxlMailboxOpcode::kSanitize), 0x4400);
}

// --- CxlMailboxReturnCode enum ---

TEST(CxlTypesTest, MailboxReturnCodeValues) {
    EXPECT_EQ(static_cast<uint16_t>(CxlMailboxReturnCode::kSuccess), 0x0000);
    EXPECT_EQ(static_cast<uint16_t>(CxlMailboxReturnCode::kBackgroundCmdStarted), 0x0001);
    EXPECT_EQ(static_cast<uint16_t>(CxlMailboxReturnCode::kInvalidInput), 0x0002);
    EXPECT_EQ(static_cast<uint16_t>(CxlMailboxReturnCode::kUnsupported), 0x0003);
}

// --- IDE-KM types ---

TEST(CxlTypesTest, IdeKmDoeType) {
    EXPECT_EQ(doe_type::kIdeKm, 0x02);
}

TEST(CxlTypesTest, IdeStreamIdEquality) {
    IdeStreamId a{1, 0, 0, 0};
    IdeStreamId b{1, 0, 0, 0};
    EXPECT_EQ(a, b);
}

TEST(CxlTypesTest, IdeStreamIdInequality) {
    IdeStreamId a{1, 0, 0, 0};
    IdeStreamId b{2, 0, 0, 0};
    EXPECT_NE(a, b);

    IdeStreamId c{1, 1, 0, 0};
    EXPECT_NE(a, c);

    IdeStreamId d{1, 0, 1, 0};
    EXPECT_NE(a, d);

    IdeStreamId e{1, 0, 0, 1};
    EXPECT_NE(a, e);
}

}  // namespace
}  // namespace plas::hal::pci
