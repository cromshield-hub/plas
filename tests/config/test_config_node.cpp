#include <gtest/gtest.h>

#include "plas/config/config_node.h"
#include "plas/config/config_format.h"
#include "plas/core/error.h"

using plas::config::ConfigNode;
using plas::config::ConfigFormat;

class ConfigNodeTest : public ::testing::Test {
protected:
    std::string FixturePath(const std::string& filename) {
        return "fixtures/" + filename;
    }
};

// --- LoadFromFile tests ---

TEST_F(ConfigNodeTest, LoadFromFileJson) {
    auto result = ConfigNode::LoadFromFile(FixturePath("nested_config.json"));
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_TRUE(result.Value().IsMap());
}

TEST_F(ConfigNodeTest, LoadFromFileYaml) {
    auto result = ConfigNode::LoadFromFile(FixturePath("nested_config.yaml"));
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_TRUE(result.Value().IsMap());
}

TEST_F(ConfigNodeTest, LoadFromFileAutoDetect) {
    auto json_result = ConfigNode::LoadFromFile(FixturePath("nested_config.json"));
    auto yaml_result = ConfigNode::LoadFromFile(FixturePath("nested_config.yaml"));
    ASSERT_TRUE(json_result.IsOk());
    ASSERT_TRUE(yaml_result.IsOk());
    // Both should be maps
    EXPECT_TRUE(json_result.Value().IsMap());
    EXPECT_TRUE(yaml_result.Value().IsMap());
}

TEST_F(ConfigNodeTest, LoadFromFileNotFound) {
    auto result = ConfigNode::LoadFromFile("nonexistent.json");
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              plas::core::make_error_code(plas::core::ErrorCode::kNotFound));
}

// --- GetSubtree tests ---

TEST_F(ConfigNodeTest, GetSubtreeSingleKey) {
    auto result = ConfigNode::LoadFromFile(FixturePath("nested_config.json"));
    ASSERT_TRUE(result.IsOk());

    auto subtree = result.Value().GetSubtree("plas");
    ASSERT_TRUE(subtree.IsOk()) << subtree.Error().message();
    EXPECT_TRUE(subtree.Value().IsMap());
}

TEST_F(ConfigNodeTest, GetSubtreeDotSeparated) {
    auto result = ConfigNode::LoadFromFile(FixturePath("nested_config.json"));
    ASSERT_TRUE(result.IsOk());

    auto subtree = result.Value().GetSubtree("plas.devices");
    ASSERT_TRUE(subtree.IsOk()) << subtree.Error().message();
    EXPECT_TRUE(subtree.Value().IsMap());
}

TEST_F(ConfigNodeTest, GetSubtreeNotFound) {
    auto result = ConfigNode::LoadFromFile(FixturePath("nested_config.json"));
    ASSERT_TRUE(result.IsOk());

    auto subtree = result.Value().GetSubtree("nonexistent.path");
    ASSERT_TRUE(subtree.IsError());
    EXPECT_EQ(subtree.Error(),
              plas::core::make_error_code(plas::core::ErrorCode::kNotFound));
}

TEST_F(ConfigNodeTest, GetSubtreeChaining) {
    auto result = ConfigNode::LoadFromFile(FixturePath("nested_config.json"));
    ASSERT_TRUE(result.IsOk());

    auto plas_node = result.Value().GetSubtree("plas");
    ASSERT_TRUE(plas_node.IsOk());

    auto devices_node = plas_node.Value().GetSubtree("devices");
    ASSERT_TRUE(devices_node.IsOk());
    EXPECT_TRUE(devices_node.Value().IsMap());
}

// --- Type query tests ---

TEST_F(ConfigNodeTest, IsMapOnObject) {
    auto result = ConfigNode::LoadFromFile(FixturePath("nested_config.json"));
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().IsMap());
    EXPECT_FALSE(result.Value().IsArray());
    EXPECT_FALSE(result.Value().IsScalar());
    EXPECT_FALSE(result.Value().IsNull());
}

TEST_F(ConfigNodeTest, IsArrayOnArray) {
    auto result = ConfigNode::LoadFromFile(FixturePath("nested_config.json"));
    ASSERT_TRUE(result.IsOk());

    auto arr = result.Value().GetSubtree("plas.devices.aardvark");
    ASSERT_TRUE(arr.IsOk());
    EXPECT_TRUE(arr.Value().IsArray());
    EXPECT_FALSE(arr.Value().IsMap());
    EXPECT_FALSE(arr.Value().IsScalar());
    EXPECT_FALSE(arr.Value().IsNull());
}

TEST_F(ConfigNodeTest, IsNullOnDefaultNode) {
    ConfigNode node;
    EXPECT_TRUE(node.IsNull());
    EXPECT_FALSE(node.IsMap());
    EXPECT_FALSE(node.IsArray());
    EXPECT_FALSE(node.IsScalar());
}

// --- YAML type preservation ---

TEST_F(ConfigNodeTest, YamlTypePreservation) {
    auto result = ConfigNode::LoadFromFile(FixturePath("grouped_config.yaml"));
    ASSERT_TRUE(result.IsOk());

    // The root should be a map with driver groups
    EXPECT_TRUE(result.Value().IsMap());

    auto aardvark = result.Value().GetSubtree("aardvark");
    ASSERT_TRUE(aardvark.IsOk());
    EXPECT_TRUE(aardvark.Value().IsArray());
}
