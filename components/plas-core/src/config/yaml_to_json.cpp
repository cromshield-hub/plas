#include "yaml_to_json.h"

#include <fstream>

#include <yaml-cpp/yaml.h>

#include "plas/core/error.h"

namespace plas::config::detail {

namespace {

nlohmann::json YamlNodeToJson(const YAML::Node& node) {
    switch (node.Type()) {
        case YAML::NodeType::Null:
            return nullptr;

        case YAML::NodeType::Scalar: {
            // Try bool first (YAML "true"/"false"/"yes"/"no" etc.)
            try {
                auto val = node.as<bool>();
                // Only accept explicit bool-like strings to avoid "0"/"1" matching
                auto raw = node.Scalar();
                if (raw == "true" || raw == "false" || raw == "True" || raw == "False" ||
                    raw == "TRUE" || raw == "FALSE" || raw == "yes" || raw == "no" ||
                    raw == "Yes" || raw == "No" || raw == "YES" || raw == "NO" ||
                    raw == "on" || raw == "off" || raw == "On" || raw == "Off" ||
                    raw == "ON" || raw == "OFF") {
                    return val;
                }
            } catch (...) {}

            // Try int64
            try {
                auto val = node.as<int64_t>();
                // Verify round-trip to avoid partial float matches
                if (std::to_string(val) == node.Scalar() || node.Scalar() == "0") {
                    return val;
                }
            } catch (...) {}

            // Try double
            try {
                auto val = node.as<double>();
                return val;
            } catch (...) {}

            // Fallback: string
            return node.as<std::string>();
        }

        case YAML::NodeType::Sequence: {
            auto arr = nlohmann::json::array();
            for (const auto& item : node) {
                arr.push_back(YamlNodeToJson(item));
            }
            return arr;
        }

        case YAML::NodeType::Map: {
            auto obj = nlohmann::json::object();
            for (const auto& pair : node) {
                obj[pair.first.as<std::string>()] = YamlNodeToJson(pair.second);
            }
            return obj;
        }

        default:
            return nullptr;
    }
}

}  // namespace

core::Result<nlohmann::json> LoadYamlFileAsJson(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::BadFile&) {
        return core::Result<nlohmann::json>::Err(core::ErrorCode::kNotFound);
    } catch (const YAML::ParserException&) {
        return core::Result<nlohmann::json>::Err(core::ErrorCode::kInvalidArgument);
    }

    return core::Result<nlohmann::json>::Ok(YamlNodeToJson(root));
}

}  // namespace plas::config::detail
