#include "plas/config/property_manager.h"

#include <algorithm>

#include "plas/core/error.h"

#include "json_property_parser.h"
#include "yaml_property_parser.h"

namespace plas::config {

PropertyManager& PropertyManager::GetInstance() {
    static PropertyManager instance;
    return instance;
}

core::Result<void> PropertyManager::LoadFromFile(
    const std::string& path, ConfigFormat fmt) {
    if (fmt == ConfigFormat::kAuto) {
        fmt = DetectFormat(path);
    }

    core::Result<std::vector<std::string>> result =
        core::Result<std::vector<std::string>>::Err(core::ErrorCode::kInvalidArgument);

    switch (fmt) {
        case ConfigFormat::kJson:
            result = detail::PopulateSessionsFromJson(path);
            break;
        case ConfigFormat::kYaml:
            result = detail::PopulateSessionsFromYaml(path);
            break;
        default:
            return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (result.IsError()) {
        return core::Result<void>::Err(result.Error());
    }

    std::lock_guard lock(mutex_);
    auto& names = result.Value();
    for (auto& name : names) {
        if (std::find(managed_sessions_.begin(), managed_sessions_.end(), name) ==
            managed_sessions_.end()) {
            managed_sessions_.push_back(std::move(name));
        }
    }

    return core::Result<void>::Ok();
}

core::Result<void> PropertyManager::LoadFromFile(
    const std::string& path,
    const std::string& session_name,
    ConfigFormat fmt) {
    if (fmt == ConfigFormat::kAuto) {
        fmt = DetectFormat(path);
    }

    auto& props = core::Properties::GetSession(session_name);

    core::Result<void> result =
        core::Result<void>::Err(core::ErrorCode::kInvalidArgument);

    switch (fmt) {
        case ConfigFormat::kJson:
            result = detail::PopulatePropertiesFromJson(path, props);
            break;
        case ConfigFormat::kYaml:
            result = detail::PopulatePropertiesFromYaml(path, props);
            break;
        default:
            return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (result.IsError()) {
        return core::Result<void>::Err(result.Error());
    }

    std::lock_guard lock(mutex_);
    if (std::find(managed_sessions_.begin(), managed_sessions_.end(), session_name) ==
        managed_sessions_.end()) {
        managed_sessions_.push_back(session_name);
    }

    return core::Result<void>::Ok();
}

core::Properties& PropertyManager::Session(const std::string& name) {
    return core::Properties::GetSession(name);
}

bool PropertyManager::HasSession(const std::string& name) const {
    std::lock_guard lock(mutex_);
    return std::find(managed_sessions_.begin(), managed_sessions_.end(), name) !=
           managed_sessions_.end();
}

std::vector<std::string> PropertyManager::SessionNames() const {
    std::lock_guard lock(mutex_);
    return managed_sessions_;
}

void PropertyManager::Reset() {
    std::lock_guard lock(mutex_);
    for (const auto& name : managed_sessions_) {
        core::Properties::DestroySession(name);
    }
    managed_sessions_.clear();
}

ConfigFormat PropertyManager::DetectFormat(const std::string& path) {
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
