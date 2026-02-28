#pragma once

#include <string>

namespace plas::log {

enum class LogLevel {
    kTrace,
    kDebug,
    kInfo,
    kWarn,
    kError,
    kCritical,
    kOff
};

inline std::string ToString(LogLevel level) {
    switch (level) {
        case LogLevel::kTrace:
            return "Trace";
        case LogLevel::kDebug:
            return "Debug";
        case LogLevel::kInfo:
            return "Info";
        case LogLevel::kWarn:
            return "Warn";
        case LogLevel::kError:
            return "Error";
        case LogLevel::kCritical:
            return "Critical";
        case LogLevel::kOff:
            return "Off";
        default:
            return "Unknown";
    }
}

}  // namespace plas::log
