#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "plas/core/result.h"

namespace plas::config::detail {

core::Result<nlohmann::json> LoadYamlFileAsJson(const std::string& path);

}  // namespace plas::config::detail
