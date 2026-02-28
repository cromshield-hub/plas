#include "json_property_parser.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "plas/core/error.h"

namespace plas::config::detail {

namespace {

void FlattenJson(const nlohmann::json& node,
                 const std::string& prefix,
                 core::Properties& props) {
    for (auto it = node.begin(); it != node.end(); ++it) {
        std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
        const auto& val = it.value();

        if (val.is_object()) {
            FlattenJson(val, key, props);
        } else if (val.is_boolean()) {
            props.Set(key, val.get<bool>());
        } else if (val.is_number_integer()) {
            props.Set(key, val.get<int64_t>());
        } else if (val.is_number_float()) {
            props.Set(key, val.get<double>());
        } else if (val.is_string()) {
            props.Set(key, val.get<std::string>());
        } else if (val.is_array()) {
            props.Set(key, val.dump());
        }
        // null → skip
    }
}

}  // namespace

core::Result<void> PopulatePropertiesFromJson(
    const std::string& path, core::Properties& props) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return core::Result<void>::Err(core::ErrorCode::kNotFound);
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::parse_error&) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (!root.is_object()) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    FlattenJson(root, "", props);
    return core::Result<void>::Ok();
}

core::Result<std::vector<std::string>> PopulateSessionsFromJson(
    const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return core::Result<std::vector<std::string>>::Err(core::ErrorCode::kNotFound);
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::parse_error&) {
        return core::Result<std::vector<std::string>>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (!root.is_object()) {
        return core::Result<std::vector<std::string>>::Err(core::ErrorCode::kInvalidArgument);
    }

    std::vector<std::string> session_names;

    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it.value().is_object()) {
            return core::Result<std::vector<std::string>>::Err(
                core::ErrorCode::kInvalidArgument);
        }

        const std::string& session_name = it.key();
        auto& props = core::Properties::GetSession(session_name);
        FlattenJson(it.value(), "", props);
        session_names.push_back(session_name);
    }

    return core::Result<std::vector<std::string>>::Ok(std::move(session_names));
}

}  // namespace plas::config::detail
