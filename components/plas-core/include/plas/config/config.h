#pragma once

#include <optional>
#include <string>
#include <vector>

#include "plas/config/config_format.h"
#include "plas/config/config_node.h"
#include "plas/config/device_entry.h"
#include "plas/core/result.h"

namespace plas::config {

class Config {
public:
    static core::Result<Config> LoadFromFile(const std::string& path,
                                             ConfigFormat fmt = ConfigFormat::kAuto);

    static core::Result<Config> LoadFromFile(const std::string& path,
                                             const std::string& key_path,
                                             ConfigFormat fmt = ConfigFormat::kAuto);

    static core::Result<Config> LoadFromNode(const ConfigNode& node);

    const std::vector<DeviceEntry>& GetDevices() const;

    std::optional<DeviceEntry> FindDevice(const std::string& nickname) const;

private:
    static ConfigFormat DetectFormat(const std::string& path);

    std::vector<DeviceEntry> devices_;
};

}  // namespace plas::config
