#pragma once

#include <string>
#include <vector>

#include "plas/config/device_entry.h"
#include "plas/core/result.h"

namespace plas::config::detail {

core::Result<std::vector<DeviceEntry>> ParseYamlConfig(const std::string& path);

}  // namespace plas::config::detail
