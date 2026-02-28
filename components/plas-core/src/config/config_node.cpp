#include "plas/config/config_node.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "config_node_internal.h"
#include "plas/core/error.h"
#include "yaml_to_json.h"

namespace plas::config {

// --- Special members ---

ConfigNode::ConfigNode() : impl_(std::make_shared<Impl>(nullptr)) {}
ConfigNode::~ConfigNode() = default;
ConfigNode::ConfigNode(const ConfigNode&) = default;
ConfigNode& ConfigNode::operator=(const ConfigNode&) = default;
ConfigNode::ConfigNode(ConfigNode&&) noexcept = default;
ConfigNode& ConfigNode::operator=(ConfigNode&&) noexcept = default;

// --- Static factory ---

static ConfigFormat DetectFormat(const std::string& path) {
    auto dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) {
        return ConfigFormat::kAuto;
    }
    std::string ext = path.substr(dot_pos);
    if (ext == ".json") return ConfigFormat::kJson;
    if (ext == ".yaml" || ext == ".yml") return ConfigFormat::kYaml;
    return ConfigFormat::kAuto;
}

core::Result<ConfigNode> ConfigNode::LoadFromFile(
    const std::string& path, ConfigFormat fmt) {
    if (fmt == ConfigFormat::kAuto) {
        fmt = DetectFormat(path);
    }

    if (fmt == ConfigFormat::kYaml) {
        auto result = detail::LoadYamlFileAsJson(path);
        if (result.IsError()) {
            return core::Result<ConfigNode>::Err(result.Error());
        }
        return core::Result<ConfigNode>::Ok(
            detail::MakeNode(std::move(result).Value()));
    }

    if (fmt == ConfigFormat::kJson) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return core::Result<ConfigNode>::Err(core::ErrorCode::kNotFound);
        }
        nlohmann::json data;
        try {
            file >> data;
        } catch (const nlohmann::json::parse_error&) {
            return core::Result<ConfigNode>::Err(core::ErrorCode::kInvalidArgument);
        }
        return core::Result<ConfigNode>::Ok(detail::MakeNode(std::move(data)));
    }

    return core::Result<ConfigNode>::Err(core::ErrorCode::kInvalidArgument);
}

// --- GetSubtree ---

core::Result<ConfigNode> ConfigNode::GetSubtree(const std::string& key_path) const {
    const nlohmann::json* current = &impl_->data_;

    std::istringstream stream(key_path);
    std::string key;
    while (std::getline(stream, key, '.')) {
        if (key.empty()) continue;
        if (!current->is_object() || !current->contains(key)) {
            return core::Result<ConfigNode>::Err(core::ErrorCode::kNotFound);
        }
        current = &(*current)[key];
    }

    return core::Result<ConfigNode>::Ok(detail::MakeNode(*current));
}

// --- Type queries ---

bool ConfigNode::IsMap() const {
    return impl_->data_.is_object();
}

bool ConfigNode::IsArray() const {
    return impl_->data_.is_array();
}

bool ConfigNode::IsScalar() const {
    return impl_->data_.is_primitive() && !impl_->data_.is_null();
}

bool ConfigNode::IsNull() const {
    return impl_->data_.is_null();
}

std::string ConfigNode::Dump() const {
    return impl_->data_.dump();
}

}  // namespace plas::config
