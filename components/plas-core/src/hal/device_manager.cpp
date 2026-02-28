#include "plas/hal/device_manager.h"

#include "plas/core/error.h"

namespace plas::hal {

DeviceManager& DeviceManager::GetInstance() {
    static DeviceManager instance;
    return instance;
}

core::Result<void> DeviceManager::LoadFromConfig(
    const std::string& path, config::ConfigFormat fmt) {
    auto config_result = config::Config::LoadFromFile(path, fmt);
    if (config_result.IsError()) {
        return core::Result<void>::Err(config_result.Error());
    }

    return LoadFromEntries(config_result.Value().GetDevices());
}

core::Result<void> DeviceManager::LoadFromConfig(
    const std::string& path, const std::string& key_path,
    config::ConfigFormat fmt) {
    auto config_result = config::Config::LoadFromFile(path, key_path, fmt);
    if (config_result.IsError()) {
        return core::Result<void>::Err(config_result.Error());
    }

    return LoadFromEntries(config_result.Value().GetDevices());
}

core::Result<void> DeviceManager::LoadFromTree(const config::ConfigNode& node) {
    auto config_result = config::Config::LoadFromNode(node);
    if (config_result.IsError()) {
        return core::Result<void>::Err(config_result.Error());
    }

    return LoadFromEntries(config_result.Value().GetDevices());
}

core::Result<void> DeviceManager::LoadFromEntries(
    const std::vector<config::DeviceEntry>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& entry : entries) {
        if (devices_.count(entry.nickname) > 0) {
            return core::Result<void>::Err(core::ErrorCode::kAlreadyOpen);
        }

        auto result = DeviceFactory::CreateFromConfig(entry);
        if (result.IsError()) {
            return core::Result<void>::Err(result.Error());
        }

        devices_[entry.nickname] = std::move(result).Value();
    }

    return core::Result<void>::Ok();
}

Device* DeviceManager::GetDevice(const std::string& nickname) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(nickname);
    if (it == devices_.end()) return nullptr;
    return it->second.get();
}

std::vector<std::string> DeviceManager::DeviceNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(devices_.size());
    for (const auto& [name, _] : devices_) {
        names.push_back(name);
    }
    return names;
}

bool DeviceManager::HasDevice(const std::string& nickname) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.count(nickname) > 0;
}

std::size_t DeviceManager::DeviceCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.size();
}

void DeviceManager::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, device] : devices_) {
        if (device->GetState() == DeviceState::kOpen) {
            device->Close();
        }
    }
    devices_.clear();
}

}  // namespace plas::hal
