#include "plas/core/properties.h"

namespace plas::core {

std::mutex& Properties::RegistryMutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<std::string, std::unique_ptr<Properties>>& Properties::Registry() {
    static std::map<std::string, std::unique_ptr<Properties>> registry;
    return registry;
}

Properties& Properties::GetSession(const std::string& name) {
    std::lock_guard lock(RegistryMutex());
    auto& reg = Registry();
    auto it = reg.find(name);
    if (it != reg.end()) {
        return *it->second;
    }
    auto [inserted, _] = reg.emplace(name, std::unique_ptr<Properties>(new Properties()));
    return *inserted->second;
}

void Properties::DestroySession(const std::string& name) {
    std::lock_guard lock(RegistryMutex());
    Registry().erase(name);
}

void Properties::DestroyAll() {
    std::lock_guard lock(RegistryMutex());
    Registry().clear();
}

bool Properties::HasSession(const std::string& name) {
    std::lock_guard lock(RegistryMutex());
    return Registry().count(name) > 0;
}

bool Properties::Has(const std::string& key) const {
    std::shared_lock lock(mutex_);
    return store_.count(key) > 0;
}

void Properties::Remove(const std::string& key) {
    std::unique_lock lock(mutex_);
    store_.erase(key);
}

void Properties::Clear() {
    std::unique_lock lock(mutex_);
    store_.clear();
}

size_t Properties::Size() const {
    std::shared_lock lock(mutex_);
    return store_.size();
}

std::vector<std::string> Properties::Keys() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(store_.size());
    for (const auto& [key, _] : store_) {
        keys.push_back(key);
    }
    return keys;
}

}  // namespace plas::core
