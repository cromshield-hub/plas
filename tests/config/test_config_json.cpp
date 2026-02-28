#include <gtest/gtest.h>

#include <filesystem>

#include "plas/config/config.h"
#include "plas/config/config_format.h"
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
