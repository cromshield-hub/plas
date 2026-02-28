#include "device_parser.h"

#include "plas/core/error.h"

namespace plas::config::detail {

namespace {

std::string JsonScalarToString(const nlohmann::json& val) {
    if (val.is_string()) return val.get<std::string>();
    if (val.is_boolean()) return val.get<bool>() ? "true" : "false";
    if (val.is_number_integer()) return std::to_string(val.get<int64_t>());
    if (val.is_number_unsigned()) return std::to_string(val.get<uint64_t>());
    if (val.is_number_float()) return std::to_string(val.get<double>());
    return {};
}

core::Result<std::vector<DeviceEntry>> ParseFlatDevices(
    const nlohmann::json& arr) {
    std::vector<DeviceEntry> devices;

    for (const auto& item : arr) {
        DeviceEntry entry;

        if (!item.contains("nickname") || !item["nickname"].is_string()) {
            return core::Result<std::vector<DeviceEntry>>::Err(
                core::ErrorCode::kInvalidArgument);
        }
        entry.nickname = item["nickname"].get<std::string>();

        if (!item.contains("uri") || !item["uri"].is_string()) {
            return core::Result<std::vector<DeviceEntry>>::Err(
                core::ErrorCode::kInvalidArgument);
        }
        entry.uri = item["uri"].get<std::string>();

        if (!item.contains("driver") || !item["driver"].is_string()) {
            return core::Result<std::vector<DeviceEntry>>::Err(
                core::ErrorCode::kInvalidArgument);
        }
        entry.driver = item["driver"].get<std::string>();

        if (item.contains("args") && item["args"].is_object()) {
            for (auto it = item["args"].begin(); it != item["args"].end(); ++it) {
                if (it.value().is_null()) continue;
                if (it.value().is_object() || it.value().is_array()) {
                    return core::Result<std::vector<DeviceEntry>>::Err(
                        core::ErrorCode::kInvalidArgument);
                }
                entry.args[it.key()] = JsonScalarToString(it.value());
            }
        }

        devices.push_back(std::move(entry));
    }

    return core::Result<std::vector<DeviceEntry>>::Ok(std::move(devices));
}

core::Result<std::vector<DeviceEntry>> ParseGroupedDevices(
    const nlohmann::json& obj) {
    std::vector<DeviceEntry> devices;

    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const std::string& driver_name = it.key();
        const auto& group_array = it.value();

        if (!group_array.is_array()) {
            return core::Result<std::vector<DeviceEntry>>::Err(
                core::ErrorCode::kInvalidArgument);
        }

        int index = 0;
        for (const auto& item : group_array) {
            if (!item.is_object()) {
                return core::Result<std::vector<DeviceEntry>>::Err(
                    core::ErrorCode::kInvalidArgument);
            }

            DeviceEntry entry;
            entry.driver = driver_name;

            if (!item.contains("uri") || !item["uri"].is_string()) {
                return core::Result<std::vector<DeviceEntry>>::Err(
                    core::ErrorCode::kInvalidArgument);
            }
            entry.uri = item["uri"].get<std::string>();

            if (item.contains("nickname") && item["nickname"].is_string()) {
                entry.nickname = item["nickname"].get<std::string>();
            } else {
                entry.nickname = driver_name + "_" + std::to_string(index);
            }

            for (auto arg_it = item.begin(); arg_it != item.end(); ++arg_it) {
                if (arg_it.key() == "nickname" || arg_it.key() == "uri") {
                    continue;
                }
                if (arg_it.value().is_null()) {
                    continue;
                }
                if (arg_it.value().is_object() || arg_it.value().is_array()) {
                    return core::Result<std::vector<DeviceEntry>>::Err(
                        core::ErrorCode::kInvalidArgument);
                }
                entry.args[arg_it.key()] = JsonScalarToString(arg_it.value());
            }

            devices.push_back(std::move(entry));
            ++index;
        }
    }

    return core::Result<std::vector<DeviceEntry>>::Ok(std::move(devices));
}

}  // namespace

core::Result<std::vector<DeviceEntry>> ParseDeviceEntries(
    const nlohmann::json& node) {
    if (node.is_array()) {
        return ParseFlatDevices(node);
    }
    if (node.is_object()) {
        return ParseGroupedDevices(node);
    }
    return core::Result<std::vector<DeviceEntry>>::Err(
        core::ErrorCode::kInvalidArgument);
}

}  // namespace plas::config::detail
