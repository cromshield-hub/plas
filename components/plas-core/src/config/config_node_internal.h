#pragma once

#include <nlohmann/json.hpp>

#include "plas/config/config_node.h"

namespace plas::config {

class ConfigNode::Impl {
public:
    explicit Impl(nlohmann::json data) : data_(std::move(data)) {}
    nlohmann::json data_;
};

namespace detail {

class ConfigNodeAccessor {
public:
    static const nlohmann::json& GetJson(const ConfigNode& node) {
        return node.impl_->data_;
    }

    static ConfigNode Make(nlohmann::json data) {
        ConfigNode node;
        node.impl_ = std::make_shared<ConfigNode::Impl>(std::move(data));
        return node;
    }
};

inline const nlohmann::json& GetNodeJson(const ConfigNode& node) {
    return ConfigNodeAccessor::GetJson(node);
}

inline ConfigNode MakeNode(nlohmann::json data) {
    return ConfigNodeAccessor::Make(std::move(data));
}

}  // namespace detail
}  // namespace plas::config
