#include "yaml_property_parser.h"

#include <fstream>

#include <yaml-cpp/yaml.h>

#include "plas/core/error.h"

namespace plas::config::detail {

namespace {

void FlattenYaml(const YAML::Node& node,
                 const std::string& prefix,
                 core::Properties& props) {
    for (auto it = node.begin(); it != node.end(); ++it) {
        std::string key = prefix.empty()
            ? it->first.as<std::string>()
            : prefix + "." + it->first.as<std::string>();
        const auto& val = it->second;

        if (val.IsMap()) {
            FlattenYaml(val, key, props);
        } else if (val.IsScalar()) {
            const std::string& scalar = val.Scalar();

            // Try boolean
            if (scalar == "true" || scalar == "false") {
                props.Set(key, val.as<bool>());
                continue;
            }

            // Try integer
            try {
                std::size_t pos = 0;
                int64_t int_val = std::stoll(scalar, &pos);
                if (pos == scalar.size()) {
                    props.Set(key, int_val);
                    continue;
                }
            } catch (...) {}

            // Try double
            try {
                std::size_t pos = 0;
                double dbl_val = std::stod(scalar, &pos);
                if (pos == scalar.size()) {
                    props.Set(key, dbl_val);
                    continue;
                }
            } catch (...) {}

            // Default to string
            props.Set(key, scalar);
        } else if (val.IsSequence()) {
            // Store arrays as JSON string representation
            YAML::Emitter emitter;
            emitter << YAML::Flow << val;
            props.Set(key, std::string(emitter.c_str()));
        }
        // null → skip
    }
}

}  // namespace

core::Result<void> PopulatePropertiesFromYaml(
    const std::string& path, core::Properties& props) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::BadFile&) {
        return core::Result<void>::Err(core::ErrorCode::kNotFound);
    } catch (const YAML::ParserException&) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (!root.IsMap()) {
        return core::Result<void>::Err(core::ErrorCode::kInvalidArgument);
    }

    FlattenYaml(root, "", props);
    return core::Result<void>::Ok();
}

core::Result<std::vector<std::string>> PopulateSessionsFromYaml(
    const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::BadFile&) {
        return core::Result<std::vector<std::string>>::Err(core::ErrorCode::kNotFound);
    } catch (const YAML::ParserException&) {
        return core::Result<std::vector<std::string>>::Err(core::ErrorCode::kInvalidArgument);
    }

    if (!root.IsMap()) {
        return core::Result<std::vector<std::string>>::Err(core::ErrorCode::kInvalidArgument);
    }

    std::vector<std::string> session_names;

    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it->second.IsMap()) {
            return core::Result<std::vector<std::string>>::Err(
                core::ErrorCode::kInvalidArgument);
        }

        std::string session_name = it->first.as<std::string>();
        auto& props = core::Properties::GetSession(session_name);
        FlattenYaml(it->second, "", props);
        session_names.push_back(std::move(session_name));
    }

    return core::Result<std::vector<std::string>>::Ok(std::move(session_names));
}

}  // namespace plas::config::detail
