#include <gtest/gtest.h>

#include "plas/config/device_entry.h"
#include "plas/configspec/spec_registry.h"
#include "plas/configspec/validator.h"

using plas::config::ConfigFormat;
using plas::config::DeviceEntry;
using plas::configspec::SpecRegistry;
using plas::configspec::ValidationMode;
using plas::configspec::Validator;

class ValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& reg = SpecRegistry::GetInstance();
        reg.Reset();
        reg.RegisterBuiltinSpecs();
    }
    void TearDown() override { SpecRegistry::GetInstance().Reset(); }
};

// --- Mode ---

TEST_F(ValidatorTest, DefaultModeIsStrict) {
    Validator v;
    EXPECT_EQ(v.GetMode(), ValidationMode::kStrict);
}

TEST_F(ValidatorTest, ExplicitMode) {
    Validator v(ValidationMode::kWarning);
    EXPECT_EQ(v.GetMode(), ValidationMode::kWarning);
}

// --- Lenient mode ---

TEST_F(ValidatorTest, LenientModeAlwaysValid) {
    Validator v(ValidationMode::kLenient);

    DeviceEntry entry{"dev", "aardvark://0:0x50", "aardvark",
                      {{"nonexistent_arg", "42"}}};
    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid);
}

// --- DeviceEntry validation ---

TEST_F(ValidatorTest, ValidAardvarkEntry) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "dev0", "aardvark://0:0x50", "aardvark",
        {{"bitrate", "100000"}, {"pullup", "true"}, {"bus_timeout_ms", "200"}}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid) << result.Value().Summary();
}

TEST_F(ValidatorTest, InvalidAardvarkUnknownArg) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "dev0", "aardvark://0:0x50", "aardvark",
        {{"bitrate", "100000"}, {"nonexistent_arg", "42"}}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().valid);
    EXPECT_GE(result.Value().Errors().size(), 1u);
}

TEST_F(ValidatorTest, InvalidAardvarkTypeMismatch) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "dev0", "aardvark://0:0x50", "aardvark",
        {{"bitrate", "not_a_number"}}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().valid);
}

TEST_F(ValidatorTest, ValidPciutilsEntry) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "pci0", "pciutils://0000:03:00.0", "pciutils",
        {{"doe_timeout_ms", "2000"}, {"doe_poll_interval_us", "100"}}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid) << result.Value().Summary();
}

TEST_F(ValidatorTest, ValidFt4222hEntry) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "ft0", "ft4222h://0:1", "ft4222h",
        {{"bitrate", "400000"}, {"sys_clock", "60"}}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid) << result.Value().Summary();
}

TEST_F(ValidatorTest, InvalidFt4222hSysClock) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "ft0", "ft4222h://0:1", "ft4222h",
        {{"sys_clock", "99"}}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().valid);
}

TEST_F(ValidatorTest, Pmu3AdditionalPropertiesAllowed) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "pmu0", "pmu3://0:0", "pmu3",
        {{"any_key", "any_value"}}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid) << result.Value().Summary();
}

TEST_F(ValidatorTest, UnknownDriverNoSpecPassesThrough) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "unk", "unknown://0:0", "unknown_driver",
        {{"anything", "goes"}}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid);
}

TEST_F(ValidatorTest, EmptyArgsValid) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{"dev0", "aardvark://0:0x50", "aardvark", {}};

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid) << result.Value().Summary();
}

// --- Config string validation ---

TEST_F(ValidatorTest, ValidConfigStringJson) {
    Validator v(ValidationMode::kStrict);
    auto result = v.ValidateConfigString(
        R"({"devices":[{"nickname":"d","uri":"aardvark://0:0x50","driver":"aardvark"}]})",
        ConfigFormat::kJson);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid) << result.Value().Summary();
}

TEST_F(ValidatorTest, InvalidConfigStringMissingDevices) {
    Validator v(ValidationMode::kStrict);
    auto result = v.ValidateConfigString(R"({"other":"field"})",
                                          ConfigFormat::kJson);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().valid);
}

// --- Config file validation ---

TEST_F(ValidatorTest, ValidConfigFile) {
    Validator v(ValidationMode::kStrict);
    auto result = v.ValidateConfigFile("fixtures/valid_flat.yaml");
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid) << result.Value().Summary();
}

TEST_F(ValidatorTest, ConfigFileNotFound) {
    Validator v(ValidationMode::kStrict);
    auto result = v.ValidateConfigFile("/nonexistent/config.yaml");
    EXPECT_TRUE(result.IsError());
}

// --- ConfigNode validation ---

TEST_F(ValidatorTest, ValidConfigNode) {
    Validator v(ValidationMode::kStrict);
    auto node_result = plas::config::ConfigNode::LoadFromFile(
        "fixtures/valid_flat.yaml");
    ASSERT_TRUE(node_result.IsOk());

    auto result = v.ValidateConfigNode(node_result.Value());
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid) << result.Value().Summary();
}

TEST_F(ValidatorTest, LenientConfigNodeAlwaysValid) {
    Validator v(ValidationMode::kLenient);
    auto node_result = plas::config::ConfigNode::LoadFromFile(
        "fixtures/valid_flat.yaml");
    ASSERT_TRUE(node_result.IsOk());

    auto result = v.ValidateConfigNode(node_result.Value());
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().valid);
}

TEST_F(ValidatorTest, MoveConstruction) {
    Validator v1(ValidationMode::kWarning);
    Validator v2(std::move(v1));
    EXPECT_EQ(v2.GetMode(), ValidationMode::kWarning);
}

TEST_F(ValidatorTest, InvalidAardvarkBitrateRange) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "dev0", "aardvark://0:0x50", "aardvark",
        {{"bitrate", "0"}}};  // minimum is 1

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().valid);
}

TEST_F(ValidatorTest, InvalidBusTimeoutRange) {
    Validator v(ValidationMode::kStrict);
    DeviceEntry entry{
        "dev0", "aardvark://0:0x50", "aardvark",
        {{"bus_timeout_ms", "70000"}}};  // max is 65535

    auto result = v.ValidateDeviceEntry(entry);
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(result.Value().valid);
}
