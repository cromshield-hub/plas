#include "plas/hal/interface/device_factory.h"

#include "plas/core/error.h"

namespace plas::hal {

core::Result<std::unique_ptr<Device>> DeviceFactory::CreateFromConfig(
    const config::DeviceEntry& entry) {
    auto& registry = GetRegistry();
    auto it = registry.find(entry.driver);
    if (it == registry.end()) {
        return core::Result<std::unique_ptr<Device>>::Err(
            core::ErrorCode::kNotFound);
    }

    auto device = it->second(entry);
    if (!device) {
        return core::Result<std::unique_ptr<Device>>::Err(
            core::ErrorCode::kInternalError);
    }

    return core::Result<std::unique_ptr<Device>>::Ok(std::move(device));
}

void DeviceFactory::RegisterDriver(const std::string& driver_name,
                                   CreatorFunc creator) {
    GetRegistry()[driver_name] = std::move(creator);
}

std::map<std::string, DeviceFactory::CreatorFunc>&
DeviceFactory::GetRegistry() {
    static std::map<std::string, CreatorFunc> registry;
    return registry;
}

}  // namespace plas::hal
