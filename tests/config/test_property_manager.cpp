#include <gtest/gtest.h>

#include <string>

#include "plas/config/property_manager.h"
#include "plas/core/error.h"
#include "plas/core/properties.h"

using plas::config::ConfigFormat;
using plas::config::PropertyManager;
using plas::core::ErrorCode;
using plas::core::Properties;

class PropertyManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        PropertyManager::GetInstance().Reset();
        Properties::DestroyAll();
    }

    void TearDown() override {
        PropertyManager::GetInstance().Reset();
        Properties::DestroyAll();
    }

    std::string FixturePath(const std::string& filename) {
        return "fixtures/" + filename;
    }
};

// --- Singleton tests ---

TEST_F(PropertyManagerTest, GetInstanceReturnsSameReference) {
    auto& a = PropertyManager::GetInstance();
    auto& b = PropertyManager::GetInstance();
    EXPECT_EQ(&a, &b);
}

TEST_F(PropertyManagerTest, InitialStateEmpty) {
    auto& mgr = PropertyManager::GetInstance();
    EXPECT_TRUE(mgr.SessionNames().empty());
}

TEST_F(PropertyManagerTest, ResetClearsSessions) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(
        FixturePath("multi_session.yaml"));
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_FALSE(mgr.SessionNames().empty());

    mgr.Reset();
    EXPECT_TRUE(mgr.SessionNames().empty());
    EXPECT_FALSE(Properties::HasSession("global"));
}

// --- Multi-session YAML tests ---

TEST_F(PropertyManagerTest, MultiSessionYamlCreatesSessions) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(FixturePath("multi_session.yaml"));
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    EXPECT_TRUE(mgr.HasSession("global"));
    EXPECT_TRUE(mgr.HasSession("device_config"));
    EXPECT_EQ(mgr.SessionNames().size(), 2u);
}

TEST_F(PropertyManagerTest, MultiSessionYamlStringType) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(FixturePath("multi_session.yaml"));
    ASSERT_TRUE(result.IsOk());

    auto& global = mgr.Session("global");
    auto app = global.Get<std::string>("app_name");
    ASSERT_TRUE(app.IsOk());
    EXPECT_EQ(app.Value(), "my_app");
}

TEST_F(PropertyManagerTest, MultiSessionYamlIntType) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("multi_session.yaml"));

    auto& dev = mgr.Session("device_config");
    auto bitrate = dev.Get<int64_t>("bitrate");
    ASSERT_TRUE(bitrate.IsOk());
    EXPECT_EQ(bitrate.Value(), 400000);
}

TEST_F(PropertyManagerTest, MultiSessionYamlBoolType) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("multi_session.yaml"));

    auto& global = mgr.Session("global");
    auto debug = global.Get<bool>("debug");
    ASSERT_TRUE(debug.IsOk());
    EXPECT_TRUE(debug.Value());
}

TEST_F(PropertyManagerTest, MultiSessionYamlNestedFlatten) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("multi_session.yaml"));

    auto& dev = mgr.Session("device_config");
    auto retry = dev.Get<int64_t>("advanced.retry_count");
    ASSERT_TRUE(retry.IsOk());
    EXPECT_EQ(retry.Value(), 3);

    auto verbose = dev.Get<bool>("advanced.verbose");
    ASSERT_TRUE(verbose.IsOk());
    EXPECT_TRUE(verbose.Value());
}

// --- Multi-session JSON tests ---

TEST_F(PropertyManagerTest, MultiSessionJsonCreatesSessions) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(FixturePath("multi_session.json"));
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    EXPECT_TRUE(mgr.HasSession("global"));
    EXPECT_TRUE(mgr.HasSession("device_config"));
}

TEST_F(PropertyManagerTest, MultiSessionJsonStringType) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("multi_session.json"));

    auto& global = mgr.Session("global");
    auto app = global.Get<std::string>("app_name");
    ASSERT_TRUE(app.IsOk());
    EXPECT_EQ(app.Value(), "my_app");
}

TEST_F(PropertyManagerTest, MultiSessionJsonIntType) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("multi_session.json"));

    auto& dev = mgr.Session("device_config");
    auto bitrate = dev.Get<int64_t>("bitrate");
    ASSERT_TRUE(bitrate.IsOk());
    EXPECT_EQ(bitrate.Value(), 400000);
}

TEST_F(PropertyManagerTest, MultiSessionJsonBoolType) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("multi_session.json"));

    auto& global = mgr.Session("global");
    auto debug = global.Get<bool>("debug");
    ASSERT_TRUE(debug.IsOk());
    EXPECT_TRUE(debug.Value());
}

TEST_F(PropertyManagerTest, MultiSessionJsonNestedFlatten) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("multi_session.json"));

    auto& dev = mgr.Session("device_config");
    auto retry = dev.Get<int64_t>("advanced.retry_count");
    ASSERT_TRUE(retry.IsOk());
    EXPECT_EQ(retry.Value(), 3);
}

// --- Single-session tests ---

TEST_F(PropertyManagerTest, SingleSessionYaml) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(
        FixturePath("single_session.yaml"), "my_device");
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    EXPECT_TRUE(mgr.HasSession("my_device"));
    auto& props = mgr.Session("my_device");

    auto bitrate = props.Get<int64_t>("bitrate");
    ASSERT_TRUE(bitrate.IsOk());
    EXPECT_EQ(bitrate.Value(), 400000);

    auto retry = props.Get<int64_t>("advanced.retry_count");
    ASSERT_TRUE(retry.IsOk());
    EXPECT_EQ(retry.Value(), 3);
}

TEST_F(PropertyManagerTest, SingleSessionJson) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(
        FixturePath("single_session.json"), "my_device");
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    EXPECT_TRUE(mgr.HasSession("my_device"));
    auto& props = mgr.Session("my_device");

    auto bitrate = props.Get<int64_t>("bitrate");
    ASSERT_TRUE(bitrate.IsOk());
    EXPECT_EQ(bitrate.Value(), 400000);
}

TEST_F(PropertyManagerTest, SingleSessionDoubleType) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("single_session.json"), "my_device");

    auto& props = mgr.Session("my_device");
    auto ratio = props.Get<double>("ratio");
    ASSERT_TRUE(ratio.IsOk());
    EXPECT_DOUBLE_EQ(ratio.Value(), 3.14);
}

TEST_F(PropertyManagerTest, SingleSessionSessionName) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("single_session.yaml"), "custom_name");

    EXPECT_TRUE(mgr.HasSession("custom_name"));
    EXPECT_FALSE(mgr.HasSession("other"));
    auto names = mgr.SessionNames();
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "custom_name");
}

// --- Type validation tests ---

TEST_F(PropertyManagerTest, TypesIntGetAsConversion) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("types_test.json"));

    auto& props = mgr.Session("types");
    // int64_t stored, GetAs<int> should work via SafeNumericCast
    auto val = props.GetAs<int>("int_val");
    ASSERT_TRUE(val.IsOk());
    EXPECT_EQ(val.Value(), 42);
}

TEST_F(PropertyManagerTest, TypesBoolExact) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("types_test.json"));

    auto& props = mgr.Session("types");
    auto t = props.Get<bool>("bool_true");
    ASSERT_TRUE(t.IsOk());
    EXPECT_TRUE(t.Value());

    auto f = props.Get<bool>("bool_false");
    ASSERT_TRUE(f.IsOk());
    EXPECT_FALSE(f.Value());
}

TEST_F(PropertyManagerTest, TypesStringExact) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("types_test.json"));

    auto& props = mgr.Session("types");
    auto val = props.Get<std::string>("str_val");
    ASSERT_TRUE(val.IsOk());
    EXPECT_EQ(val.Value(), "hello");
}

TEST_F(PropertyManagerTest, TypesNestedDotNotation) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("types_test.json"));

    auto& props = mgr.Session("types");
    auto val = props.Get<std::string>("nested.deep");
    ASSERT_TRUE(val.IsOk());
    EXPECT_EQ(val.Value(), "value");
}

TEST_F(PropertyManagerTest, TypesNullSkipped) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("types_test.json"));

    auto& props = mgr.Session("types");
    EXPECT_FALSE(props.Has("null_val"));
}

TEST_F(PropertyManagerTest, TypesArrayStoredAsString) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("types_test.json"));

    auto& props = mgr.Session("types");
    auto val = props.Get<std::string>("array_val");
    ASSERT_TRUE(val.IsOk());
    EXPECT_EQ(val.Value(), "[1,2,3]");
}

// --- YAML-specific type tests ---

TEST_F(PropertyManagerTest, YamlTypesPreserved) {
    auto& mgr = PropertyManager::GetInstance();
    mgr.LoadFromFile(FixturePath("types_test.yaml"));

    auto& props = mgr.Session("types");

    auto int_val = props.Get<int64_t>("int_val");
    ASSERT_TRUE(int_val.IsOk());
    EXPECT_EQ(int_val.Value(), 42);

    auto neg = props.Get<int64_t>("neg_int");
    ASSERT_TRUE(neg.IsOk());
    EXPECT_EQ(neg.Value(), -10);

    auto fval = props.Get<double>("float_val");
    ASSERT_TRUE(fval.IsOk());
    EXPECT_DOUBLE_EQ(fval.Value(), 3.14);

    auto bval = props.Get<bool>("bool_true");
    ASSERT_TRUE(bval.IsOk());
    EXPECT_TRUE(bval.Value());

    auto sval = props.Get<std::string>("str_val");
    ASSERT_TRUE(sval.IsOk());
    EXPECT_EQ(sval.Value(), "hello");
}

// --- Error cases ---

TEST_F(PropertyManagerTest, FileNotFound) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile("nonexistent.yaml");
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(), plas::core::make_error_code(ErrorCode::kNotFound));
}

TEST_F(PropertyManagerTest, InvalidYamlParsing) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(FixturePath("invalid.yaml"));
    ASSERT_TRUE(result.IsError());
}

TEST_F(PropertyManagerTest, InvalidJsonParsing) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(FixturePath("invalid.json"));
    ASSERT_TRUE(result.IsError());
}

TEST_F(PropertyManagerTest, MultiSessionScalarTopLevelFails) {
    auto& mgr = PropertyManager::GetInstance();
    // scalar_top.yaml has scalar values at top level, not objects
    auto result = mgr.LoadFromFile(FixturePath("scalar_top.yaml"));
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              plas::core::make_error_code(ErrorCode::kInvalidArgument));
}

TEST_F(PropertyManagerTest, EmptyJsonMultiSession) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(FixturePath("empty.json"));
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(mgr.SessionNames().empty());
}

TEST_F(PropertyManagerTest, EmptyYamlFails) {
    auto& mgr = PropertyManager::GetInstance();
    auto result = mgr.LoadFromFile(FixturePath("empty.yaml"));
    ASSERT_TRUE(result.IsError());
}

TEST_F(PropertyManagerTest, AutoFormatDetection) {
    auto& mgr = PropertyManager::GetInstance();

    auto r1 = mgr.LoadFromFile(FixturePath("single_session.yaml"), "s1");
    ASSERT_TRUE(r1.IsOk());

    auto r2 = mgr.LoadFromFile(FixturePath("single_session.json"), "s2");
    ASSERT_TRUE(r2.IsOk());

    EXPECT_TRUE(mgr.HasSession("s1"));
    EXPECT_TRUE(mgr.HasSession("s2"));
}
