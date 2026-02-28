#pragma once

#include <memory>
#include <string>

#include "plas/config/config_format.h"
#include "plas/config/config_node.h"
#include "plas/config/device_entry.h"
#include "plas/core/result.h"
#include "plas/configspec/validation_result.h"

namespace plas::configspec {

enum class ValidationMode { kStrict, kWarning, kLenient };

class Validator {
public:
    explicit Validator(ValidationMode mode = ValidationMode::kStrict);
    ~Validator();

    Validator(Validator&&) noexcept;
    Validator& operator=(Validator&&) noexcept;

    Validator(const Validator&) = delete;
    Validator& operator=(const Validator&) = delete;

    core::Result<ValidationResult> ValidateConfigFile(
        const std::string& path,
        config::ConfigFormat fmt = config::ConfigFormat::kAuto) const;

    core::Result<ValidationResult> ValidateConfigString(
        const std::string& content, config::ConfigFormat fmt) const;

    core::Result<ValidationResult> ValidateConfigNode(
        const config::ConfigNode& node) const;

    core::Result<ValidationResult> ValidateDeviceEntry(
        const config::DeviceEntry& entry) const;

    ValidationMode GetMode() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace plas::configspec
