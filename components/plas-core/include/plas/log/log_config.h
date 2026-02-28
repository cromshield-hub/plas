#pragma once

#include <cstddef>
#include <string>

#include "plas/log/log_level.h"

namespace plas::log {

struct LogConfig {
    std::string log_dir = "logs";
    std::string file_prefix = "plas";
    std::size_t max_file_size = 10 * 1024 * 1024;  // 10 MB
    std::size_t max_files = 5;
    LogLevel level = LogLevel::kInfo;
    bool console_enabled = true;
};

}  // namespace plas::log
