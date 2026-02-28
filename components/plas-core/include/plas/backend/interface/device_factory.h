#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "plas/backend/interface/device.h"
#include "plas/config/device_entry.h"
#include "plas/core/result.h"

namespace plas::backend {

class DeviceFactory {
public:
    using CreatorFunc =
        std::function<std::unique_ptr<Device>(const config::DeviceEntry&)>;

    static core::Result<std::unique_ptr<Device>> CreateFromConfig(
        const config::DeviceEntry& entry);

    static void RegisterDriver(const std::string& driver_name,
                               CreatorFunc creator);

private:
    static std::map<std::string, CreatorFunc>& GetRegistry();
};

}  // namespace plas::backend
