#include "spdlog_backend.h"

#include <vector>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace plas::log {

SpdlogBackend::SpdlogBackend() = default;

SpdlogBackend::~SpdlogBackend() {
    if (logger_) {
        spdlog::drop(logger_->name());
    }
}

void SpdlogBackend::Initialize(const LogConfig& config) {
    // Build the full log file path: <log_dir>/<file_prefix>.log
    log_file_path_ = config.log_dir + "/" + config.file_prefix + ".log";

    std::vector<spdlog::sink_ptr> sinks;

    // Rotating file sink
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_file_path_, config.max_file_size, config.max_files);
    sinks.push_back(file_sink);

    // Optional stdout color sink
    if (config.console_enabled) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(console_sink);
    }

    // Create the multi-sink logger
    logger_ = std::make_shared<spdlog::logger>(
        "plas", sinks.begin(), sinks.end());
    logger_->set_level(ToSpdlogLevel(config.level));
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    logger_->flush_on(spdlog::level::warn);

    spdlog::register_logger(logger_);
}

void SpdlogBackend::Log(LogLevel level, const std::string& msg) {
    if (logger_) {
        logger_->log(ToSpdlogLevel(level), msg);
    }
}

void SpdlogBackend::SetLevel(LogLevel level) {
    if (logger_) {
        logger_->set_level(ToSpdlogLevel(level));
    }
}

std::string SpdlogBackend::GetCurrentLogFile() const {
    return log_file_path_;
}

spdlog::level::level_enum SpdlogBackend::ToSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::kTrace:
            return spdlog::level::trace;
        case LogLevel::kDebug:
            return spdlog::level::debug;
        case LogLevel::kInfo:
            return spdlog::level::info;
        case LogLevel::kWarn:
            return spdlog::level::warn;
        case LogLevel::kError:
            return spdlog::level::err;
        case LogLevel::kCritical:
            return spdlog::level::critical;
        case LogLevel::kOff:
            return spdlog::level::off;
        default:
            return spdlog::level::info;
    }
}

}  // namespace plas::log
