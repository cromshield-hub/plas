#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "plas/config/config_format.h"
#include "plas/core/properties.h"
#include "plas/core/result.h"

namespace plas::config {

class PropertyManager {
public:
    static PropertyManager& GetInstance();

    // Multi-session mode: each top-level key becomes a session name
    core::Result<void> LoadFromFile(
        const std::string& path,
        ConfigFormat fmt = ConfigFormat::kAuto);

    // Single-session mode: entire file maps to one named session
    core::Result<void> LoadFromFile(
        const std::string& path,
        const std::string& session_name,
        ConfigFormat fmt = ConfigFormat::kAuto);

    // Session access (delegates to Properties)
    core::Properties& Session(const std::string& name);
    bool HasSession(const std::string& name) const;
    std::vector<std::string> SessionNames() const;

    // Destroy all sessions managed by this manager
    void Reset();

    PropertyManager(const PropertyManager&) = delete;
    PropertyManager& operator=(const PropertyManager&) = delete;

private:
    PropertyManager() = default;
    ~PropertyManager() = default;

    static ConfigFormat DetectFormat(const std::string& path);

    std::vector<std::string> managed_sessions_;
    mutable std::mutex mutex_;
};

}  // namespace plas::config
