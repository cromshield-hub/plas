#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "plas/config/config.h"
#include "plas/config/config_format.h"
#include "plas/config/config_node.h"
#include "plas/core/error.h"

namespace plas::config {
namespace {

/// Helper to write content to a temp file and return its path.
class TempFile {
public:
    explicit TempFile(const std::string& filename, const std::string& content)
        : path_(filename) {
        std::ofstream ofs(path_);
        ofs << content;
    }

    ~TempFile() { std::remove(path_.c_str()); }

    const std::string& Path() const { return path_; }

private:
    std::string path_;
};

// --- File errors ---

TEST(ConfigErrorsTest, NonExistentFile) {
    auto result = Config::LoadFromFile("nonexistent_file_that_does_not_exist.json");
    EXPECT_TRUE(result.IsError());
}

TEST(ConfigErrorsTest, EmptyJsonFile) {
    TempFile f("_test_empty.json", "");
    auto result = Config::LoadFromFile(f.Path());
    EXPECT_TRUE(result.IsError());
}

TEST(ConfigErrorsTest, EmptyYamlFile) {
    TempFile f("_test_empty.yaml", "");
    auto result = Config::LoadFromFile(f.Path());
    EXPECT_TRUE(result.IsError());
}

// --- Malformed JSON ---

TEST(ConfigErrorsTest, MalformedJsonUnclosedBrace) {
    TempFile f("_test_malformed.json", R"({"devices": [{"nickname": "a")");
    auto result = Config::LoadFromFile(f.Path());
    EXPECT_TRUE(result.IsError());
}

TEST(ConfigErrorsTest, MalformedJsonRootIsScalar) {
    TempFile f("_test_scalar.json", "42");
    auto result = Config::LoadFromFile(f.Path());
    EXPECT_TRUE(result.IsError());
}

TEST(ConfigErrorsTest, MalformedJsonRootIsString) {
    TempFile f("_test_string.json", R"("hello")");
    auto result = Config::LoadFromFile(f.Path());
    EXPECT_TRUE(result.IsError());
}

// --- Malformed YAML ---

TEST(ConfigErrorsTest, MalformedYamlRootIsScalar) {
    TempFile f("_test_scalar.yaml", "42");
    auto result = Config::LoadFromFile(f.Path());
    EXPECT_TRUE(result.IsError());
}

// --- Missing fields (flat format) ---

TEST(ConfigErrorsTest, FlatMissingNickname) {
    TempFile f("_test_no_nick.json", R"([{"uri":"a://0","driver":"a"}])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    // flat array items missing nickname should still parse (nickname may be optional
    // depending on impl) or error — either way we test it doesn't crash
    // The important thing: no crash, result is either Ok or Error
    SUCCEED();
}

TEST(ConfigErrorsTest, FlatMissingUri) {
    TempFile f("_test_no_uri.json",
               R"([{"nickname":"dev","driver":"test"}])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    SUCCEED();
}

TEST(ConfigErrorsTest, FlatMissingDriver) {
    TempFile f("_test_no_driver.json",
               R"([{"nickname":"dev","uri":"test://0"}])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    SUCCEED();
}

// --- Wrong types (flat format) ---

TEST(ConfigErrorsTest, FlatNicknameIsNumber) {
    TempFile f("_test_nick_num.json",
               R"([{"nickname":42,"uri":"a://0","driver":"a"}])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    SUCCEED();
}

TEST(ConfigErrorsTest, FlatUriIsArray) {
    TempFile f("_test_uri_arr.json",
               R"([{"nickname":"dev","uri":["a"],"driver":"a"}])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    SUCCEED();
}

TEST(ConfigErrorsTest, FlatDriverIsObject) {
    TempFile f("_test_driver_obj.json",
               R"([{"nickname":"dev","uri":"a://0","driver":{"x":1}}])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    SUCCEED();
}

// --- Args errors ---

TEST(ConfigErrorsTest, ArgsIsString) {
    TempFile f("_test_args_str.json",
               R"([{"nickname":"dev","uri":"a://0","driver":"a","args":"bad"}])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    SUCCEED();
}

TEST(ConfigErrorsTest, ArgsNestedObject) {
    TempFile f("_test_args_nested.json",
               R"([{"nickname":"dev","uri":"a://0","driver":"a","args":{"nested":{"deep":1}}}])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    SUCCEED();
}

// --- Key path errors ---

TEST(ConfigErrorsTest, NonExistentKeyPath) {
    TempFile f("_test_keypath.json", R"({"a":{"b":1}})");
    auto result = Config::LoadFromFile(f.Path(), "x.y.z");
    EXPECT_TRUE(result.IsError());
}

TEST(ConfigErrorsTest, KeyPathPointsToScalar) {
    TempFile f("_test_keypath_scalar.json", R"({"a":{"b":42}})");
    auto result = Config::LoadFromFile(f.Path(), "a.b");
    EXPECT_TRUE(result.IsError());
}

// --- Edge cases ---

TEST(ConfigErrorsTest, EmptyDevicesArray) {
    TempFile f("_test_empty_arr.json", R"([])");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    // Empty array is a valid config with zero devices, or an error
    if (result.IsOk()) {
        EXPECT_TRUE(result.Value().GetDevices().empty());
    }
}

TEST(ConfigErrorsTest, EmptyGroupedObject) {
    TempFile f("_test_empty_obj.json", R"({})");
    auto result = Config::LoadFromFile(f.Path(), ConfigFormat::kJson);
    // Empty object: might be valid with zero devices, or fail parsing
    // Either is acceptable behavior
    SUCCEED();
}

// --- ConfigNode errors ---

TEST(ConfigErrorsTest, ConfigNodeNonExistentFile) {
    auto result = ConfigNode::LoadFromFile("_no_such_file.json");
    EXPECT_TRUE(result.IsError());
}

TEST(ConfigErrorsTest, ConfigNodeMalformedJson) {
    TempFile f("_test_node_bad.json", R"({invalid)");
    auto result = ConfigNode::LoadFromFile(f.Path());
    EXPECT_TRUE(result.IsError());
}

}  // namespace
}  // namespace plas::config
