#include "yaml_parser.h"

#include "device_parser.h"
#include "plas/core/error.h"
#include "yaml_to_json.h"

namespace plas::config::detail {

core::Result<std::vector<DeviceEntry>> ParseYamlConfig(const std::string& path) {
    auto json_result = LoadYamlFileAsJson(path);
    if (json_result.IsError()) {
        return core::Result<std::vector<DeviceEntry>>::Err(json_result.Error());
    }

    auto& root = json_result.Value();

    if (!root.contains("devices")) {
        return core::Result<std::vector<DeviceEntry>>::Err(core::ErrorCode::kInvalidArgument);
    }

    return ParseDeviceEntries(root["devices"]);
}

}  // namespace plas::config::detail
