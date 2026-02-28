#include <gtest/gtest.h>

#include <filesystem>

#include "plas/config/config.h"
#include "plas/config/config_format.h"
#include "plas/config/config_node.h"
#include "plas/config/device_entry.h"
#include "plas/core/error.h"

using plas::config::Config;
using plas::config::ConfigFormat;

class ConfigYamlTest : public ::testing::Test {
protected:
    std::string FixturePath(const std::string& filename) {
        return "fixtures/" + filename;
    }
};

TEST_F(ConfigYamlTest, LoadValidYaml) {
    auto result = Config::LoadFromFile(FixturePath("test_config.yaml"));
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    const auto& config = result.Value();
    const auto& devices = config.GetDevices();
    ASSERT_EQ(devices.size(), 2u);

    EXPECT_EQ(devices[0].nickname, "aardvark0");
    EXPECT_EQ(devices[0].uri, "aardvark://0:0x50");
    EXPECT_EQ(devices[0].driver, "aardvark");
    EXPECT_EQ(devices[0].args.at("bitrate"), "400000");

    EXPECT_EQ(devices[1].nickname, "pmu3_main");
    EXPECT_EQ(devices[1].driver, "pmu3");
}

TEST_F(ConfigYamlTest, ExplicitFormat) {
    auto result = Config::LoadFromFile(FixturePath("test_config.yaml"), ConfigFormat::kYaml);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
}

TEST_F(ConfigYamlTest, FindDevice) {
    auto result = Config::LoadFromFile(FixturePath("test_config.yaml"));
    ASSERT_TRUE(result.IsOk());

    auto& config = result.Value();
    auto device = config.FindDevice("pmu3_main");
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->driver, "pmu3");
    EXPECT_EQ(device->args.at("channel"), "0");
}

TEST_F(ConfigYamlTest, LoadNonexistentFile) {
    auto result = Config::LoadFromFile("nonexistent.yaml");
    EXPECT_TRUE(result.IsError());
}

TEST_F(ConfigYamlTest, ConsistentWithJson) {
    auto json_result = Config::LoadFromFile(FixturePath("test_config.json"));
    auto yaml_result = Config::LoadFromFile(FixturePath("test_config.yaml"));
    ASSERT_TRUE(json_result.IsOk());
    ASSERT_TRUE(yaml_result.IsOk());

    const auto& json_devices = json_result.Value().GetDevices();
    const auto& yaml_devices = yaml_result.Value().GetDevices();
    ASSERT_EQ(json_devices.size(), yaml_devices.size());

    for (size_t i = 0; i < json_devices.size(); ++i) {
        EXPECT_EQ(json_devices[i].nickname, yaml_devices[i].nickname);
        EXPECT_EQ(json_devices[i].uri, yaml_devices[i].uri);
        EXPECT_EQ(json_devices[i].driver, yaml_devices[i].driver);
        EXPECT_EQ(json_devices[i].args, yaml_devices[i].args);
    }
}

// --- Grouped format tests ---

TEST_F(ConfigYamlTest, LoadFromNodeGrouped) {
    auto tree = plas::config::ConfigNode::LoadFromFile(
        FixturePath("grouped_config.yaml"));
    ASSERT_TRUE(tree.IsOk()) << tree.Error().message();

    auto config_result = Config::LoadFromNode(tree.Value());
    ASSERT_TRUE(config_result.IsOk()) << config_result.Error().message();

    const auto& devices = config_result.Value().GetDevices();
    ASSERT_EQ(devices.size(), 3u);

    EXPECT_EQ(devices[0].nickname, "aardvark0");
    EXPECT_EQ(devices[0].driver, "aardvark");
    EXPECT_EQ(devices[0].args.at("bitrate"), "400000");

    EXPECT_EQ(devices[1].nickname, "aardvark_1");
    EXPECT_EQ(devices[1].driver, "aardvark");

    EXPECT_EQ(devices[2].nickname, "pmu3_main");
    EXPECT_EQ(devices[2].driver, "pmu3");
}

TEST_F(ConfigYamlTest, LoadFromFileWithKeyPath) {
    auto result = Config::LoadFromFile(
        FixturePath("nested_config.yaml"), "plas.devices");
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    const auto& devices = result.Value().GetDevices();
    ASSERT_EQ(devices.size(), 2u);
    EXPECT_EQ(devices[0].nickname, "aardvark0");
    EXPECT_EQ(devices[1].nickname, "pmu3_main");
}

TEST_F(ConfigYamlTest, GroupedJsonYamlConsistency) {
    auto json_tree = plas::config::ConfigNode::LoadFromFile(
        FixturePath("grouped_config.json"));
    auto yaml_tree = plas::config::ConfigNode::LoadFromFile(
        FixturePath("grouped_config.yaml"));
    ASSERT_TRUE(json_tree.IsOk());
    ASSERT_TRUE(yaml_tree.IsOk());

    auto json_cfg = Config::LoadFromNode(json_tree.Value());
    auto yaml_cfg = Config::LoadFromNode(yaml_tree.Value());
    ASSERT_TRUE(json_cfg.IsOk());
    ASSERT_TRUE(yaml_cfg.IsOk());

    const auto& jd = json_cfg.Value().GetDevices();
    const auto& yd = yaml_cfg.Value().GetDevices();
    ASSERT_EQ(jd.size(), yd.size());

    for (size_t i = 0; i < jd.size(); ++i) {
        EXPECT_EQ(jd[i].nickname, yd[i].nickname);
        EXPECT_EQ(jd[i].uri, yd[i].uri);
        EXPECT_EQ(jd[i].driver, yd[i].driver);
        EXPECT_EQ(jd[i].args, yd[i].args);
    }
}
