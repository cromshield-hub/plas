#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "plas/log/log_config.h"
#include "plas/log/log_level.h"

namespace plas::log {

class SpdlogBackend {
public:
    SpdlogBackend();
    ~SpdlogBackend();

    /// Create spdlog sinks and logger from the given configuration.
    void Initialize(const LogConfig& config);

    /// Forward a log message to spdlog.
    void Log(LogLevel level, const std::string& msg);

    /// Update the minimum log level on the spdlog logger.
    void SetLevel(LogLevel level);

    /// Return the path to the current (base) log file.
    std::string GetCurrentLogFile() const;

private:
    /// Convert plas::log::LogLevel to spdlog::level::level_enum.
    static spdlog::level::level_enum ToSpdlogLevel(LogLevel level);

    std::shared_ptr<spdlog::logger> logger_;
    std::string log_file_path_;
};

}  // namespace plas::log
