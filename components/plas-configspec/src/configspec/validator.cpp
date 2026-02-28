#include "plas/configspec/validator.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <yaml-cpp/yaml.h>

#include "plas/core/error.h"
#include "plas/configspec/spec_registry.h"
#include "spec_registry_internal.h"

namespace plas::configspec {

// --- YAML-to-JSON helper (same as spec_registry.cpp) ---

namespace {

nlohmann::json YamlNodeToJson(const YAML::Node& node) {
    switch (node.Type()) {
        case YAML::NodeType::Null:
            return nullptr;

        case YAML::NodeType::Scalar: {
            try {
                auto val = node.as<bool>();
                auto raw = node.Scalar();
                if (raw == "true" || raw == "false" || raw == "True" ||
                    raw == "False" || raw == "TRUE" || raw == "FALSE" ||
                    raw == "yes" || raw == "no" || raw == "Yes" ||
                    raw == "No" || raw == "YES" || raw == "NO" ||
                    raw == "on" || raw == "off" || raw == "On" ||
                    raw == "Off" || raw == "ON" || raw == "OFF") {
                    return val;
                }
            } catch (...) {
            }

            try {
                auto val = node.as<int64_t>();
                if (std::to_string(val) == node.Scalar() ||
                    node.Scalar() == "0") {
                    return val;
                }
            } catch (...) {
            }

            try {
                auto val = node.as<double>();
                return val;
            } catch (...) {
            }

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

nlohmann::json ParseContent(const std::string& content,
                             config::ConfigFormat fmt) {
    if (fmt == config::ConfigFormat::kYaml) {
        YAML::Node root = YAML::Load(content);
        return YamlNodeToJson(root);
    }
    return nlohmann::json::parse(content);
}

config::ConfigFormat DetectFormat(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return config::ConfigFormat::kAuto;
    std::string ext = path.substr(dot);
    if (ext == ".yaml" || ext == ".yml") return config::ConfigFormat::kYaml;
    if (ext == ".json") return config::ConfigFormat::kJson;
    return config::ConfigFormat::kAuto;
}

// Custom error handler that collects issues instead of throwing
class PlasErrorHandler : public nlohmann::json_schema::basic_error_handler {
public:
    void error(const nlohmann::json::json_pointer& pointer,
               const nlohmann::json& instance,
               const std::string& message) override {
        nlohmann::json_schema::basic_error_handler::error(pointer, instance,
                                                           message);
        issues.push_back(
            {Severity::kError, pointer.to_string(), message});
    }

    std::vector<ValidationIssue> issues;
};

// Try to convert string args to typed JSON values
nlohmann::json ArgsToTypedJson(
    const std::map<std::string, std::string>& args) {
    auto obj = nlohmann::json::object();
    for (const auto& [key, value] : args) {
        // Try bool
        if (value == "true" || value == "True" || value == "TRUE" ||
            value == "yes" || value == "Yes" || value == "YES") {
            obj[key] = true;
            continue;
        }
        if (value == "false" || value == "False" || value == "FALSE" ||
            value == "no" || value == "No" || value == "NO") {
            obj[key] = false;
            continue;
        }

        // Try integer
        try {
            std::size_t pos = 0;
            long long iv = std::stoll(value, &pos);
            if (pos == value.size()) {
                obj[key] = iv;
                continue;
            }
        } catch (...) {
        }

        // Try double
        try {
            std::size_t pos = 0;
            double dv = std::stod(value, &pos);
            if (pos == value.size()) {
                obj[key] = dv;
                continue;
            }
        } catch (...) {
        }

        // Fallback: string
        obj[key] = value;
    }
    return obj;
}

ValidationResult RunValidation(const nlohmann::json& spec,
                                const nlohmann::json& data) {
    ValidationResult result;
    try {
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(spec);

        PlasErrorHandler handler;
        validator.validate(data, handler);

        result.issues = std::move(handler.issues);
        result.valid = result.Errors().empty();
    } catch (const std::exception& e) {
        result.valid = false;
        result.issues.push_back(
            {Severity::kError, "/", std::string("spec error: ") + e.what()});
    }
    return result;
}

}  // namespace

// --- Impl ---

struct Validator::Impl {
    ValidationMode mode;
};

// --- Lifecycle ---

Validator::Validator(ValidationMode mode) : impl_(std::make_unique<Impl>()) {
    impl_->mode = mode;
}

Validator::~Validator() = default;
Validator::Validator(Validator&&) noexcept = default;
Validator& Validator::operator=(Validator&&) noexcept = default;

// --- API ---

ValidationMode Validator::GetMode() const { return impl_->mode; }

core::Result<ValidationResult> Validator::ValidateConfigFile(
    const std::string& path, config::ConfigFormat fmt) const {
    if (impl_->mode == ValidationMode::kLenient) {
        return core::Result<ValidationResult>::Ok(ValidationResult{});
    }

    if (fmt == config::ConfigFormat::kAuto) {
        fmt = DetectFormat(path);
        if (fmt == config::ConfigFormat::kAuto) {
            fmt = config::ConfigFormat::kJson;
        }
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return core::Result<ValidationResult>::Err(core::ErrorCode::kNotFound);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ValidateConfigString(ss.str(), fmt);
}

core::Result<ValidationResult> Validator::ValidateConfigString(
    const std::string& content, config::ConfigFormat fmt) const {
    if (impl_->mode == ValidationMode::kLenient) {
        return core::Result<ValidationResult>::Ok(ValidationResult{});
    }

    nlohmann::json data;
    try {
        data = ParseContent(content, fmt);
    } catch (...) {
        return core::Result<ValidationResult>::Err(
            core::ErrorCode::kInvalidArgument);
    }

    auto* spec =
        detail::GetConfigSpecJson(SpecRegistry::GetInstance());
    if (!spec) {
        // No config spec registered — pass through
        return core::Result<ValidationResult>::Ok(ValidationResult{});
    }

    return core::Result<ValidationResult>::Ok(RunValidation(*spec, data));
}

core::Result<ValidationResult> Validator::ValidateConfigNode(
    const config::ConfigNode& node) const {
    if (impl_->mode == ValidationMode::kLenient) {
        return core::Result<ValidationResult>::Ok(ValidationResult{});
    }

    return ValidateConfigString(node.Dump(), config::ConfigFormat::kJson);
}

core::Result<ValidationResult> Validator::ValidateDeviceEntry(
    const config::DeviceEntry& entry) const {
    if (impl_->mode == ValidationMode::kLenient) {
        return core::Result<ValidationResult>::Ok(ValidationResult{});
    }

    auto* spec = detail::GetDriverSpecJson(
        SpecRegistry::GetInstance(), entry.driver);
    if (!spec) {
        // No spec for this driver — pass through
        return core::Result<ValidationResult>::Ok(ValidationResult{});
    }

    auto args_json = ArgsToTypedJson(entry.args);
    return core::Result<ValidationResult>::Ok(
        RunValidation(*spec, args_json));
}

}  // namespace plas::configspec
