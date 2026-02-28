#include "plas/log/logger.h"

#include "backends/spdlog_backend.h"

namespace plas::log {

// ---------------------------------------------------------------------------
// Pimpl definition
// ---------------------------------------------------------------------------
struct Logger::Impl {
    SpdlogBackend backend;
    LogLevel level = LogLevel::kInfo;
    bool initialized = false;
};

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
Logger::Logger() : impl_(std::make_unique<Impl>()) {}

Logger::~Logger() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void Logger::Init(const LogConfig& config) {
    impl_->backend.Initialize(config);
    impl_->level = config.level;
    impl_->initialized = true;
}

void Logger::SetLevel(LogLevel level) {
    impl_->level = level;
    impl_->backend.SetLevel(level);
}

LogLevel Logger::GetLevel() const {
    return impl_->level;
}

std::string Logger::GetCurrentLogFile() const {
    return impl_->backend.GetCurrentLogFile();
}

void Logger::Log(LogLevel level, const std::string& msg) {
    if (!impl_->initialized) {
        return;
    }
    if (level < impl_->level) {
        return;
    }
    impl_->backend.Log(level, msg);
}

void Logger::Trace(const std::string& msg) {
    Log(LogLevel::kTrace, msg);
}

void Logger::Debug(const std::string& msg) {
    Log(LogLevel::kDebug, msg);
}

void Logger::Info(const std::string& msg) {
    Log(LogLevel::kInfo, msg);
}

void Logger::Warn(const std::string& msg) {
    Log(LogLevel::kWarn, msg);
}

void Logger::Error(const std::string& msg) {
    Log(LogLevel::kError, msg);
}

void Logger::Critical(const std::string& msg) {
    Log(LogLevel::kCritical, msg);
}

}  // namespace plas::log
