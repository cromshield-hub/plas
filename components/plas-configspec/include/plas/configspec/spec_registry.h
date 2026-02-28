#pragma once

#include <memory>
#include <string>
#include <vector>

#include "plas/config/config_format.h"
#include "plas/core/result.h"

namespace plas::configspec {

namespace detail {
class SpecRegistryAccessor;
}  // namespace detail

class SpecRegistry {
public:
    static SpecRegistry& GetInstance();

    void RegisterBuiltinSpecs();

    core::Result<void> RegisterDriverSpec(const std::string& name,
                                          const std::string& content,
                                          config::ConfigFormat fmt);
    core::Result<void> RegisterDriverSpecFromFile(const std::string& name,
                                                   const std::string& path,
                                                   config::ConfigFormat fmt = config::ConfigFormat::kAuto);
    core::Result<void> RegisterConfigSpec(const std::string& content,
                                          config::ConfigFormat fmt);

    bool HasDriverSpec(const std::string& name) const;
    bool HasConfigSpec() const;
    std::vector<std::string> RegisteredDrivers() const;

    core::Result<std::string> ExportDriverSpec(const std::string& name) const;

    core::Result<std::size_t> LoadSpecsFromDirectory(const std::string& dir);

    void Reset();

private:
    friend class detail::SpecRegistryAccessor;

    SpecRegistry();
    ~SpecRegistry();

    SpecRegistry(const SpecRegistry&) = delete;
    SpecRegistry& operator=(const SpecRegistry&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace plas::configspec
