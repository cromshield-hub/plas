#pragma once

#include <memory>
#include <string>

#include "plas/log/log_config.h"
#include "plas/log/log_level.h"

namespace plas::log {

class Logger {
public:
    static Logger& GetInstance();

    /// Initialize the logger with the given configuration.
    /// Must be called before any logging occurs.
    void Init(const LogConfig& config);

    /// Set the minimum log level.
    void SetLevel(LogLevel level);

    /// Get the current minimum log level.
    LogLevel GetLevel() const;

    /// Return the path to the current log file.
    std::string GetCurrentLogFile() const;

    /// Core logging method. Messages below the current level are discarded.
    void Log(LogLevel level, const std::string& msg);

    /// Convenience methods for each log level.
    void Trace(const std::string& msg);
    void Debug(const std::string& msg);
    void Info(const std::string& msg);
    void Warn(const std::string& msg);
    void Error(const std::string& msg);
    void Critical(const std::string& msg);

    // Not copyable or movable (singleton).
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

private:
    Logger();
    ~Logger();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace plas::log

// ---------------------------------------------------------------------------
// Convenience macros
// ---------------------------------------------------------------------------
#define PLAS_LOG_TRACE(msg)    ::plas::log::Logger::GetInstance().Trace(msg)
#define PLAS_LOG_DEBUG(msg)    ::plas::log::Logger::GetInstance().Debug(msg)
#define PLAS_LOG_INFO(msg)     ::plas::log::Logger::GetInstance().Info(msg)
#define PLAS_LOG_WARN(msg)     ::plas::log::Logger::GetInstance().Warn(msg)
#define PLAS_LOG_ERROR(msg)    ::plas::log::Logger::GetInstance().Error(msg)
#define PLAS_LOG_CRITICAL(msg) ::plas::log::Logger::GetInstance().Critical(msg)
