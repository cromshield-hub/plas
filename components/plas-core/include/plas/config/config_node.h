#pragma once

#include <memory>
#include <string>

#include "plas/config/config_format.h"
#include "plas/core/result.h"

namespace plas::config {

namespace detail {
class ConfigNodeAccessor;
}  // namespace detail

class ConfigNode {
public:
    static core::Result<ConfigNode> LoadFromFile(
        const std::string& path,
        ConfigFormat fmt = ConfigFormat::kAuto);

    core::Result<ConfigNode> GetSubtree(const std::string& key_path) const;

    bool IsMap() const;
    bool IsArray() const;
    bool IsScalar() const;
    bool IsNull() const;

    std::string Dump() const;

    ConfigNode();
    ~ConfigNode();
    ConfigNode(const ConfigNode&);
    ConfigNode& operator=(const ConfigNode&);
    ConfigNode(ConfigNode&&) noexcept;
    ConfigNode& operator=(ConfigNode&&) noexcept;

private:
    friend class detail::ConfigNodeAccessor;

    class Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace plas::config
