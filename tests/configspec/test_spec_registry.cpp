#include <gtest/gtest.h>

#include <fstream>

#include "plas/configspec/spec_registry.h"

using plas::config::ConfigFormat;
using plas::configspec::SpecRegistry;

class SpecRegistryTest : public ::testing::Test {
protected:
    void SetUp() override { SpecRegistry::GetInstance().Reset(); }
    void TearDown() override { SpecRegistry::GetInstance().Reset(); }
};

TEST_F(SpecRegistryTest, InitiallyEmpty) {
    auto& reg = SpecRegistry::GetInstance();
    EXPECT_FALSE(reg.HasConfigSpec());
    EXPECT_TRUE(reg.RegisteredDrivers().empty());
}

TEST_F(SpecRegistryTest, RegisterBuiltinSpecs) {
    auto& reg = SpecRegistry::GetInstance();
    reg.RegisterBuiltinSpecs();

    EXPECT_TRUE(reg.HasConfigSpec());
    EXPECT_TRUE(reg.HasDriverSpec("aardvark"));
    EXPECT_TRUE(reg.HasDriverSpec("ft4222h"));
    EXPECT_TRUE(reg.HasDriverSpec("pciutils"));
    EXPECT_TRUE(reg.HasDriverSpec("pmu3"));
    EXPECT_TRUE(reg.HasDriverSpec("pmu4"));

    auto drivers = reg.RegisteredDrivers();
    EXPECT_GE(drivers.size(), 5u);
}

TEST_F(SpecRegistryTest, RegisterBuiltinSpecsIdempotent) {
    auto& reg = SpecRegistry::GetInstance();
    reg.RegisterBuiltinSpecs();
    auto count1 = reg.RegisteredDrivers().size();
    reg.RegisterBuiltinSpecs();
    auto count2 = reg.RegisteredDrivers().size();
    EXPECT_EQ(count1, count2);
}

TEST_F(SpecRegistryTest, RegisterDriverSpecJson) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.RegisterDriverSpec(
        "test_driver",
        R"({"type":"object","properties":{"x":{"type":"integer"}}})",
        ConfigFormat::kJson);
    EXPECT_TRUE(result.IsOk());
    EXPECT_TRUE(reg.HasDriverSpec("test_driver"));
}

TEST_F(SpecRegistryTest, RegisterDriverSpecYaml) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.RegisterDriverSpec(
        "yaml_driver", "type: object\nproperties:\n  x:\n    type: integer\n",
        ConfigFormat::kYaml);
    EXPECT_TRUE(result.IsOk());
    EXPECT_TRUE(reg.HasDriverSpec("yaml_driver"));
}

TEST_F(SpecRegistryTest, RegisterDriverSpecInvalidContent) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.RegisterDriverSpec("bad", "{invalid json", ConfigFormat::kJson);
    EXPECT_TRUE(result.IsError());
}

TEST_F(SpecRegistryTest, RegisterConfigSpec) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.RegisterConfigSpec(
        R"({"type":"object","properties":{"devices":{"type":"array"}}})",
        ConfigFormat::kJson);
    EXPECT_TRUE(result.IsOk());
    EXPECT_TRUE(reg.HasConfigSpec());
}

TEST_F(SpecRegistryTest, ExportDriverSpec) {
    auto& reg = SpecRegistry::GetInstance();
    reg.RegisterDriverSpec(
        "exp_driver",
        R"({"type":"object","properties":{"x":{"type":"integer"}}})",
        ConfigFormat::kJson);

    auto result = reg.ExportDriverSpec("exp_driver");
    ASSERT_TRUE(result.IsOk());
    EXPECT_NE(result.Value().find("integer"), std::string::npos);
}

TEST_F(SpecRegistryTest, ExportDriverSpecNotFound) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.ExportDriverSpec("nonexistent");
    EXPECT_TRUE(result.IsError());
}

TEST_F(SpecRegistryTest, Reset) {
    auto& reg = SpecRegistry::GetInstance();
    reg.RegisterBuiltinSpecs();
    EXPECT_TRUE(reg.HasConfigSpec());
    EXPECT_FALSE(reg.RegisteredDrivers().empty());

    reg.Reset();
    EXPECT_FALSE(reg.HasConfigSpec());
    EXPECT_TRUE(reg.RegisteredDrivers().empty());
}

TEST_F(SpecRegistryTest, OverrideDriverSpec) {
    auto& reg = SpecRegistry::GetInstance();
    reg.RegisterDriverSpec(
        "my_drv", R"({"type":"object","properties":{"a":{"type":"string"}}})",
        ConfigFormat::kJson);

    auto exp1 = reg.ExportDriverSpec("my_drv");
    ASSERT_TRUE(exp1.IsOk());
    EXPECT_NE(exp1.Value().find("string"), std::string::npos);

    // Override
    reg.RegisterDriverSpec(
        "my_drv", R"({"type":"object","properties":{"b":{"type":"integer"}}})",
        ConfigFormat::kJson);

    auto exp2 = reg.ExportDriverSpec("my_drv");
    ASSERT_TRUE(exp2.IsOk());
    EXPECT_NE(exp2.Value().find("integer"), std::string::npos);
}

TEST_F(SpecRegistryTest, LoadSpecsFromDirectory) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.LoadSpecsFromDirectory("fixtures/schemas");
    ASSERT_TRUE(result.IsOk());
    EXPECT_GE(result.Value(), 1u);
}

TEST_F(SpecRegistryTest, LoadSpecsFromDirectoryNotFound) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.LoadSpecsFromDirectory("/nonexistent/path");
    EXPECT_TRUE(result.IsError());
}

TEST_F(SpecRegistryTest, HasDriverSpecReturnsFalseForUnknown) {
    auto& reg = SpecRegistry::GetInstance();
    EXPECT_FALSE(reg.HasDriverSpec("unknown_driver"));
}

TEST_F(SpecRegistryTest, RegisterDriverSpecFromFile) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.RegisterDriverSpecFromFile(
        "aardvark", "fixtures/schemas/aardvark.schema.yaml");
    EXPECT_TRUE(result.IsOk());
    EXPECT_TRUE(reg.HasDriverSpec("aardvark"));
}

TEST_F(SpecRegistryTest, RegisterDriverSpecFromFileNotFound) {
    auto& reg = SpecRegistry::GetInstance();
    auto result = reg.RegisterDriverSpecFromFile(
        "bad", "/nonexistent/schema.yaml");
    EXPECT_TRUE(result.IsError());
}
