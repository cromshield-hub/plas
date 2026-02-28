#pragma once

#include <string>
#include <vector>

#include "plas/core/properties.h"
#include "plas/core/result.h"

namespace plas::config::detail {

// Single session: populate a Properties session from YAML file root
core::Result<void> PopulatePropertiesFromYaml(
    const std::string& path, core::Properties& props);

// Multi session: each top-level key becomes a session name
core::Result<std::vector<std::string>> PopulateSessionsFromYaml(
    const std::string& path);

}  // namespace plas::config::detail
