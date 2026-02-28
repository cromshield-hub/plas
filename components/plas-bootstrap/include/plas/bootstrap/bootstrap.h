#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "plas/config/config_format.h"
#include "plas/config/config_node.h"
#include "plas/core/error.h"
#include "plas/core/result.h"
#include "plas/hal/device_manager.h"
#include "plas/log/log_config.h"

namespace plas::config {
class PropertyManager;
}  // namespace plas::config

namespace plas::bootstrap {

struct BootstrapConfig {
    std::string device_config_path;
    std::string device_config_key_path;
    config::ConfigFormat device_config_format = config::ConfigFormat::kAuto;
    std::optional<config::ConfigNode> device_config_node;

    std::optional<log::LogConfig> log_config;

    std::string properties_config_path;
    config::ConfigFormat properties_config_format = config::ConfigFormat::kAuto;

    bool auto_open_devices = true;
    bool skip_unknown_drivers = true;
    bool skip_device_failures = true;
};

struct DeviceFailure {
    std::string nickname;
    std::string uri;
    std::string driver;
    std::error_code error;
    std::string phase;   // "create", "init", "open"
    std::string detail;  // human-readable context (driver-specific)
};

struct BootstrapResult {
    std::size_t devices_opened = 0;
    std::size_t devices_failed = 0;
    std::size_t devices_skipped = 0;
    std::vector<DeviceFailure> failures;
};

class Bootstrap {
public:
    Bootstrap();
    ~Bootstrap();

    Bootstrap(Bootstrap&& other) noexcept;
    Bootstrap& operator=(Bootstrap&& other) noexcept;

    Bootstrap(const Bootstrap&) = delete;
    Bootstrap& operator=(const Bootstrap&) = delete;

    /// Register all available drivers (hides #ifdef guards internally).
    static void RegisterAllDrivers();

    /// Validate URI format: "driver://bus:identifier".
    static bool ValidateUri(const std::string& uri);

    /// Run the full initialization sequence.
    core::Result<BootstrapResult> Init(const BootstrapConfig& config);

    /// Tear down in reverse order (idempotent).
    void Deinit();

    bool IsInitialized() const;

    hal::DeviceManager* GetDeviceManager();
    config::PropertyManager* GetPropertyManager();

    hal::Device* GetDevice(const std::string& nickname);
    hal::Device* GetDeviceByUri(const std::string& uri);

    template <typename T>
    T* GetInterface(const std::string& nickname);

    template <typename T>
    std::vector<std::pair<std::string, T*>> GetDevicesByInterface();

    std::vector<std::string> DeviceNames() const;
    const std::vector<DeviceFailure>& GetFailures() const;

    /// Return a formatted summary of all loaded devices (for debugging).
    std::string DumpDevices() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// --- Template implementation ---

template <typename T>
T* Bootstrap::GetInterface(const std::string& nickname) {
    auto* device = GetDevice(nickname);
    if (!device) return nullptr;
    return dynamic_cast<T*>(device);
}

template <typename T>
std::vector<std::pair<std::string, T*>> Bootstrap::GetDevicesByInterface() {
    return hal::DeviceManager::GetInstance().GetDevicesByInterface<T>();
}

}  // namespace plas::bootstrap
