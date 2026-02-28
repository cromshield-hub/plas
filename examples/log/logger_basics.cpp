/// @file logger_basics.cpp
/// @brief Example: Logger initialization, level control, and macro usage.
///
/// Demonstrates how to:
///  1. Configure LogConfig (console-only, no file output)
///  2. Initialize the singleton Logger
///  3. Use PLAS_LOG_* macros at various levels
///  4. Change the log level at runtime to filter messages

#include <cstdio>

#include "plas/log/log_config.h"
#include "plas/log/log_level.h"
#include "plas/log/logger.h"

int main() {
    using namespace plas::log;

    // --- 1. Configure logger (console only) ---
    LogConfig config;
    config.console_enabled = true;
    config.level = LogLevel::kDebug;
    config.max_files = 0;  // disable file output

    Logger::GetInstance().Init(config);
    std::printf("[*] Logger initialized (level: %s)\n",
                ToString(Logger::GetInstance().GetLevel()).c_str());

    // --- 2. Log at various levels ---
    PLAS_LOG_DEBUG("This is a DEBUG message");
    PLAS_LOG_INFO("Device initialized successfully");
    PLAS_LOG_WARN("Temperature approaching threshold");
    PLAS_LOG_ERROR("Failed to read register 0x1F");

    // --- 3. Change level at runtime ---
    std::printf("\n[*] Setting level to WARN\n");
    Logger::GetInstance().SetLevel(LogLevel::kWarn);

    PLAS_LOG_DEBUG("This DEBUG message should NOT appear");
    PLAS_LOG_INFO("This INFO message should NOT appear");
    PLAS_LOG_WARN("This WARN message SHOULD appear");
    PLAS_LOG_ERROR("This ERROR message SHOULD appear");

    // --- 4. Restore level ---
    Logger::GetInstance().SetLevel(LogLevel::kInfo);
    PLAS_LOG_INFO("Level restored to INFO");

    std::printf("[*] Done.\n");
    return 0;
}
