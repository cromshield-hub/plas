#include "json_parser.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "device_parser.h"
#include "plas/core/error.h"

namespace plas::config::detail {

core::Result<std::vector<DeviceEntry>> ParseJsonConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kNotFound);
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::parse_error&) {
        return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (!root.contains("devices")) {
        return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
    }

    return ParseDeviceEntries(root["devices"]);
}

}  // namespace plas::config::detail
