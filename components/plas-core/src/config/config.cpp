#include "plas/config/config.h"

#include <algorithm>

#include "plas/core/error.h"

#include "json_parser.h"
#include "yaml_parser.h"

namespace plas::config {

core::Result<Config> Config::LoadFromFile(const std::string& path, ConfigFormat fmt) {
    if (fmt == ConfigFormat::kAuto) {
        fmt = DetectFormat(path);
    }

    core::Result<std::vector<DeviceEntry>> parse_result =
        core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);

    switch (fmt) {
        case ConfigFormat::kJson:
            parse_result = detail::ParseJsonConfig(path);
            break;
        case ConfigFormat::kYaml:
            parse_result = detail::ParseYamlConfig(path);
            break;
        default:
            return core::Result<Config>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (parse_result.IsError()) {
        return core::Result<Config>::Err(parse_result.Error());
    }

    Config config;
    config.devices_ = std::move(parse_result).Value();
    return core::Result<Config>::Ok(std::move(config));
}

const std::vector<DeviceEntry>& Config::GetDevices() const {
    return devices_;
}

std::optional<DeviceEntry> Config::FindDevice(const std::string& nickname) const {
    auto it = std::find_if(devices_.begin(), devices_.end(),
                           [&nickname](const DeviceEntry& entry) {
                               return entry.nickname == nickname;
                           });
    if (it != devices_.end()) {
        return *it;
    }
    return std::nullopt;
}

ConfigFormat Config::DetectFormat(const std::string& path) {
    auto dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) {
        return ConfigFormat::kAuto;
    }

    std::string ext = path.substr(dot_pos);
    if (ext == ".json") {
        return ConfigFormat::kJson;
    }
    if (ext == ".yaml" || ext == ".yml") {
        return ConfigFormat::kYaml;
    }

    return ConfigFormat::kAuto;
}

}  // namespace plas::config
