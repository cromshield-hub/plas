#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "plas/log/log_config.h"
#include "plas/log/log_level.h"
#include "plas/log/logger.h"

using plas::log::LogConfig;
using plas::log::LogLevel;
using plas::log::Logger;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_dir_ = "test_logs_" + std::to_string(::testing::UnitTest::GetInstance()
                                                      ->current_test_info()
                                                      ->line());
        std::filesystem::create_directories(log_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(log_dir_);
    }

    std::string log_dir_;
};

TEST_F(LoggerTest, InitAndLog) {
    LogConfig config;
    config.log_dir = log_dir_;
    config.file_prefix = "test";
    config.level = LogLevel::kTrace;
    config.console_enabled = false;

    auto& logger = Logger::GetInstance();
    logger.Init(config);
    logger.Info("Test message from LoggerTest");

    std::string log_file = logger.GetCurrentLogFile();
    EXPECT_FALSE(log_file.empty());
}

TEST_F(LoggerTest, LogLevelFiltering) {
    LogConfig config;
    config.log_dir = log_dir_;
    config.file_prefix = "level_test";
    config.level = LogLevel::kWarn;
    config.console_enabled = false;

    auto& logger = Logger::GetInstance();
    logger.Init(config);

    EXPECT_EQ(logger.GetLevel(), LogLevel::kWarn);

    logger.SetLevel(LogLevel::kDebug);
    EXPECT_EQ(logger.GetLevel(), LogLevel::kDebug);
}

TEST_F(LoggerTest, AllLogLevels) {
    LogConfig config;
    config.log_dir = log_dir_;
    config.file_prefix = "all_levels";
    config.level = LogLevel::kTrace;
    config.console_enabled = false;

    auto& logger = Logger::GetInstance();
    logger.Init(config);

    logger.Trace("trace message");
    logger.Debug("debug message");
    logger.Info("info message");
    logger.Warn("warn message");
    logger.Error("error message");
    logger.Critical("critical message");
}

TEST(LogLevelTest, ToString) {
    EXPECT_EQ(plas::log::ToString(LogLevel::kTrace), "Trace");
    EXPECT_EQ(plas::log::ToString(LogLevel::kDebug), "Debug");
    EXPECT_EQ(plas::log::ToString(LogLevel::kInfo), "Info");
    EXPECT_EQ(plas::log::ToString(LogLevel::kWarn), "Warn");
    EXPECT_EQ(plas::log::ToString(LogLevel::kError), "Error");
    EXPECT_EQ(plas::log::ToString(LogLevel::kCritical), "Critical");
    EXPECT_EQ(plas::log::ToString(LogLevel::kOff), "Off");
}
