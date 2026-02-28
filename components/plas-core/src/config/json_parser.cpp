#include "json_parser.h"

#include <fstream>

#include <nlohmann/json.hpp>

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

    if (!root.contains("devices") || !root["devices"].is_array()) {
        return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
    }

    std::vector<DeviceEntry> devices;

    for (const auto& item : root["devices"]) {
        DeviceEntry entry;

        if (!item.contains("nickname") || !item["nickname"].is_string()) {
            return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
        }
        entry.nickname = item["nickname"].get<std::string>();

        if (!item.contains("uri") || !item["uri"].is_string()) {
            return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
        }
        entry.uri = item["uri"].get<std::string>();

        if (!item.contains("driver") || !item["driver"].is_string()) {
            return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
        }
        entry.driver = item["driver"].get<std::string>();

        if (item.contains("args") && item["args"].is_object()) {
            for (auto it = item["args"].begin(); it != item["args"].end(); ++it) {
                if (!it.value().is_string()) {
                    return core::Result<std::vector<DeviceEntry>>::Err(
                        core::ErrorCode::kInvalidArgument);
                }
                entry.args[it.key()] = it.value().get<std::string>();
            }
        }

        devices.push_back(std::move(entry));
    }

    return core::Result<std::vector<DeviceEntry>>::Ok(std::move(devices));
}

}  // namespace plas::config::detail
