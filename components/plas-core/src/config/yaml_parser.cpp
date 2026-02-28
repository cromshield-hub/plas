#include "yaml_parser.h"

#include <fstream>

#include <yaml-cpp/yaml.h>

#include "plas/core/error.h"

namespace plas::config::detail {

core::Result<std::vector<DeviceEntry>> ParseYamlConfig(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::BadFile&) {
        return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kNotFound);
    } catch (const YAML::ParserException&) {
        return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (!root["devices"] || !root["devices"].IsSequence()) {
        return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
    }

    std::vector<DeviceEntry> devices;

    for (const auto& item : root["devices"]) {
        DeviceEntry entry;

        if (!item["nickname"] || !item["nickname"].IsScalar()) {
            return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
        }
        entry.nickname = item["nickname"].as<std::string>();

        if (!item["uri"] || !item["uri"].IsScalar()) {
            return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
        }
        entry.uri = item["uri"].as<std::string>();

        if (!item["driver"] || !item["driver"].IsScalar()) {
            return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
        }
        entry.driver = item["driver"].as<std::string>();

        if (item["args"] && item["args"].IsMap()) {
            for (const auto& pair : item["args"]) {
                if (!pair.second.IsScalar()) {
                    return core::Result<std::vector<DeviceEntry>>::Err(
                        core::ErrorCode::kInvalidArgument);
                }
                entry.args[pair.first.as<std::string>()] = pair.second.as<std::string>();
            }
        }

        devices.push_back(std::move(entry));
    }

    return core::Result<std::vector<DeviceEntry>>::Ok(std::move(devices));
}

}  // namespace plas::config::detail
