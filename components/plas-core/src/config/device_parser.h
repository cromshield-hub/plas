#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "plas/config/device_entry.h"
#include "plas/core/result.h"

namespace plas::config::detail {

core::Result<std::vector<DeviceEntry>> ParseDeviceEntries(
    const nlohmann::json& node);

}  // namespace plas::config::detail
