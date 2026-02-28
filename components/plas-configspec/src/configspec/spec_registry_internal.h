#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "plas/configspec/spec_registry.h"

namespace plas::configspec::detail {

class SpecRegistryAccessor {
public:
    static SpecRegistry::Impl* GetImpl(const SpecRegistry& registry) {
        return registry.impl_.get();
    }
};

const nlohmann::json* GetDriverSpecJson(const SpecRegistry& registry,
                                         const std::string& name);

const nlohmann::json* GetConfigSpecJson(const SpecRegistry& registry);

}  // namespace plas::configspec::detail
