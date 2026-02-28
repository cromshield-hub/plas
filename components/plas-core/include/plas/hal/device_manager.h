#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "plas/config/config.h"
#include "plas/config/config_format.h"
#include "plas/config/device_entry.h"
#include "plas/core/result.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/device_factory.h"

namespace plas::hal {

class DeviceManager {
public:
    static DeviceManager& GetInstance();

    core::Result<void> LoadFromConfig(
        const std::string& path,
        config::ConfigFormat fmt = config::ConfigFormat::kAuto);

    core::Result<void> LoadFromEntries(
        const std::vector<config::DeviceEntry>& entries);

    Device* GetDevice(const std::string& nickname);

    template <typename T>
    T* GetInterface(const std::string& nickname);

    std::vector<std::string> DeviceNames() const;
    bool HasDevice(const std::string& nickname) const;
    std::size_t DeviceCount() const;

    void Reset();

    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

private:
    DeviceManager() = default;
    ~DeviceManager() = default;

    std::map<std::string, std::unique_ptr<Device>> devices_;
    mutable std::mutex mutex_;
};

// --- Template implementation ---

template <typename T>
T* DeviceManager::GetInterface(const std::string& nickname) {
    auto* device = GetDevice(nickname);
    if (!device) return nullptr;
    return dynamic_cast<T*>(device);
}

}  // namespace plas::hal
