#include <gtest/gtest.h>

#include <filesystem>

#include "plas/config/config.h"
#include "plas/config/config_format.h"
#include "plas/config/config_node.h"
#include "plas/config/device_entry.h"
#include "plas/core/error.h"

using plas::config::Config;
using plas::config::ConfigFormat;

class ConfigJsonTest : public ::testing::Test {
protected:
    std::string FixturePath(const std::string& filename) {
        return "fixtures/" + filename;
    }
};

TEST_F(ConfigJsonTest, LoadValidJson) {
    auto result = Config::LoadFromFile(FixturePath("test_config.json"));
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

TEST_F(ConfigJsonTest, ExplicitFormat) {
    auto result = Config::LoadFromFile(FixturePath("test_config.json"), ConfigFormat::kJson);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
}

TEST_F(ConfigJsonTest, FindDevice) {
    auto result = Config::LoadFromFile(FixturePath("test_config.json"));
    ASSERT_TRUE(result.IsOk());

    auto& config = result.Value();
    auto device = config.FindDevice("aardvark0");
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->driver, "aardvark");

    auto not_found = config.FindDevice("nonexistent");
    EXPECT_FALSE(not_found.has_value());
}

TEST_F(ConfigJsonTest, LoadNonexistentFile) {
    auto result = Config::LoadFromFile("nonexistent.json");
    EXPECT_TRUE(result.IsError());
}

TEST_F(ConfigJsonTest, UnsupportedFormat) {
    auto result = Config::LoadFromFile("config.toml");
    EXPECT_TRUE(result.IsError());
}

// --- Grouped format tests ---

TEST_F(ConfigJsonTest, LoadFromNodeGrouped) {
    auto tree = plas::config::ConfigNode::LoadFromFile(
        FixturePath("grouped_config.json"));
    ASSERT_TRUE(tree.IsOk()) << tree.Error().message();

    auto config_result = Config::LoadFromNode(tree.Value());
    ASSERT_TRUE(config_result.IsOk()) << config_result.Error().message();

    const auto& devices = config_result.Value().GetDevices();
    ASSERT_EQ(devices.size(), 3u);

    // aardvark group: aardvark0 (explicit), aardvark_1 (auto-generated)
    EXPECT_EQ(devices[0].nickname, "aardvark0");
    EXPECT_EQ(devices[0].driver, "aardvark");
    EXPECT_EQ(devices[0].uri, "aardvark://0:0x50");
    EXPECT_EQ(devices[0].args.at("bitrate"), "400000");

    EXPECT_EQ(devices[1].nickname, "aardvark_1");
    EXPECT_EQ(devices[1].driver, "aardvark");
    EXPECT_EQ(devices[1].uri, "aardvark://1:0x51");

    // pmu3 group
    EXPECT_EQ(devices[2].nickname, "pmu3_main");
    EXPECT_EQ(devices[2].driver, "pmu3");
}

TEST_F(ConfigJsonTest, LoadFromNodeFlatArray) {
    // Existing flat format should also work via LoadFromNode
    auto tree = plas::config::ConfigNode::LoadFromFile(
        FixturePath("test_config.json"));
    ASSERT_TRUE(tree.IsOk());

    auto devices_node = tree.Value().GetSubtree("devices");
    ASSERT_TRUE(devices_node.IsOk());

    auto config_result = Config::LoadFromNode(devices_node.Value());
    ASSERT_TRUE(config_result.IsOk()) << config_result.Error().message();

    const auto& devices = config_result.Value().GetDevices();
    ASSERT_EQ(devices.size(), 2u);
    EXPECT_EQ(devices[0].nickname, "aardvark0");
    EXPECT_EQ(devices[1].nickname, "pmu3_main");
}

TEST_F(ConfigJsonTest, LoadFromFileWithKeyPath) {
    auto result = Config::LoadFromFile(
        FixturePath("nested_config.json"), "plas.devices");
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    const auto& devices = result.Value().GetDevices();
    ASSERT_EQ(devices.size(), 2u);
    EXPECT_EQ(devices[0].nickname, "aardvark0");
    EXPECT_EQ(devices[0].driver, "aardvark");
    EXPECT_EQ(devices[1].nickname, "pmu3_main");
    EXPECT_EQ(devices[1].driver, "pmu3");
}

TEST_F(ConfigJsonTest, GroupedAutoNickname) {
    auto tree = plas::config::ConfigNode::LoadFromFile(
        FixturePath("grouped_config.json"));
    ASSERT_TRUE(tree.IsOk());

    auto config_result = Config::LoadFromNode(tree.Value());
    ASSERT_TRUE(config_result.IsOk());

    // Second aardvark entry has no nickname → auto-generated "aardvark_1"
    auto device = config_result.Value().FindDevice("aardvark_1");
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->driver, "aardvark");
    EXPECT_EQ(device->uri, "aardvark://1:0x51");
}

TEST_F(ConfigJsonTest, GroupedScalarArgsConversion) {
    auto tree = plas::config::ConfigNode::LoadFromFile(
        FixturePath("grouped_config.json"));
    ASSERT_TRUE(tree.IsOk());

    auto config_result = Config::LoadFromNode(tree.Value());
    ASSERT_TRUE(config_result.IsOk());

    // bitrate: 400000 (integer) → "400000" (string)
    auto device = config_result.Value().FindDevice("aardvark0");
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->args.at("bitrate"), "400000");
}

TEST_F(ConfigJsonTest, GroupedUriMissing) {
    // Create a grouped config with missing uri to test error
    auto tree = plas::config::ConfigNode::LoadFromFile(
        FixturePath("grouped_config.json"));
    ASSERT_TRUE(tree.IsOk());
    // Passing a non-array/non-object should fail
    auto subtree = tree.Value().GetSubtree("aardvark");
    ASSERT_TRUE(subtree.IsOk());
    // This is an array of device entries, parsed as flat — should fail because
    // flat entries require "driver" field
    auto config_result = Config::LoadFromNode(subtree.Value());
    EXPECT_TRUE(config_result.IsError());
}
