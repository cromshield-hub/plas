#include "plas/bootstrap/bootstrap.h"

#include <utility>

#include "plas/config/config.h"
#include "plas/config/property_manager.h"
#include "plas/core/properties.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/log/logger.h"

#include "plas/hal/driver/aardvark/aardvark_device.h"
#include "plas/hal/driver/ft4222h/ft4222h_device.h"
#include "plas/hal/driver/pmu3/pmu3_device.h"
#include "plas/hal/driver/pmu4/pmu4_device.h"
#ifdef PLAS_HAS_PCIUTILS
#include "plas/hal/driver/pciutils/pciutils_device.h"
#endif

namespace plas::bootstrap {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct Bootstrap::Impl {
    bool initialized = false;
    bool logger_initialized = false;
    bool properties_loaded = false;
    std::vector<DeviceFailure> failures;
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Bootstrap::Bootstrap() : impl_(std::make_unique<Impl>()) {}

Bootstrap::~Bootstrap() {
    if (impl_ && impl_->initialized) {
        Deinit();
    }
}

Bootstrap::Bootstrap(Bootstrap&& other) noexcept = default;
Bootstrap& Bootstrap::operator=(Bootstrap&& other) noexcept = default;

// ---------------------------------------------------------------------------
// RegisterAllDrivers
// ---------------------------------------------------------------------------

void Bootstrap::RegisterAllDrivers() {
    hal::driver::AardvarkDevice::Register();
    hal::driver::Ft4222hDevice::Register();
    hal::driver::Pmu3Device::Register();
    hal::driver::Pmu4Device::Register();
#ifdef PLAS_HAS_PCIUTILS
    hal::driver::PciUtilsDevice::Register();
#endif
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

core::Result<BootstrapResult> Bootstrap::Init(const BootstrapConfig& cfg) {
    if (impl_->initialized) {
        return core::Result<BootstrapResult>::Err(
            core::ErrorCode::kAlreadyOpen);
    }

    if (cfg.device_config_path.empty()) {
        return core::Result<BootstrapResult>::Err(
            core::ErrorCode::kInvalidArgument);
    }

    // 1. Register drivers
    RegisterAllDrivers();

    // 2. Logger init (optional)
    if (cfg.log_config.has_value()) {
        log::Logger::GetInstance().Init(cfg.log_config.value());
        impl_->logger_initialized = true;
    }

    // 3. Properties load (optional)
    if (!cfg.properties_config_path.empty()) {
        auto& pm = config::PropertyManager::GetInstance();
        auto prop_result = pm.LoadFromFile(cfg.properties_config_path,
                                            cfg.properties_config_format);
        if (prop_result.IsError()) {
            // Rollback: logger stays (process lifetime)
            return core::Result<BootstrapResult>::Err(prop_result.Error());
        }
        impl_->properties_loaded = true;
    }

    // 4. Parse device config
    core::Result<config::Config> config_result =
        cfg.device_config_key_path.empty()
            ? config::Config::LoadFromFile(cfg.device_config_path,
                                           cfg.device_config_format)
            : config::Config::LoadFromFile(cfg.device_config_path,
                                           cfg.device_config_key_path,
                                           cfg.device_config_format);

    if (config_result.IsError()) {
        // Rollback properties if loaded
        if (impl_->properties_loaded) {
            config::PropertyManager::GetInstance().Reset();
            core::Properties::DestroyAll();
            impl_->properties_loaded = false;
        }
        return core::Result<BootstrapResult>::Err(config_result.Error());
    }

    const auto& entries = config_result.Value().GetDevices();
    auto& dm = hal::DeviceManager::GetInstance();
    BootstrapResult result;
    impl_->failures.clear();

    // 5. Create + add devices individually
    for (const auto& entry : entries) {
        auto create = hal::DeviceFactory::CreateFromConfig(entry);
        if (create.IsError()) {
            if (create.Error() ==
                    core::make_error_code(core::ErrorCode::kNotFound) &&
                cfg.skip_unknown_drivers) {
                ++result.devices_skipped;
                impl_->failures.push_back(
                    {entry.nickname, entry.uri, entry.driver,
                     create.Error(), "create"});
                continue;
            }
            if (cfg.skip_device_failures) {
                ++result.devices_failed;
                impl_->failures.push_back(
                    {entry.nickname, entry.uri, entry.driver,
                     create.Error(), "create"});
                continue;
            }
            // Hard failure — rollback
            dm.Reset();
            if (impl_->properties_loaded) {
                config::PropertyManager::GetInstance().Reset();
                core::Properties::DestroyAll();
                impl_->properties_loaded = false;
            }
            return core::Result<BootstrapResult>::Err(create.Error());
        }

        auto add = dm.AddDevice(entry.nickname, std::move(create).Value());
        if (add.IsError()) {
            if (cfg.skip_device_failures) {
                ++result.devices_failed;
                impl_->failures.push_back(
                    {entry.nickname, entry.uri, entry.driver,
                     add.Error(), "create"});
                continue;
            }
            dm.Reset();
            if (impl_->properties_loaded) {
                config::PropertyManager::GetInstance().Reset();
                core::Properties::DestroyAll();
                impl_->properties_loaded = false;
            }
            return core::Result<BootstrapResult>::Err(add.Error());
        }
    }

    // 6. Init + Open devices (optional)
    if (cfg.auto_open_devices) {
        for (const auto& name : dm.DeviceNames()) {
            auto* dev = dm.GetDevice(name);
            if (!dev) continue;

            auto init = dev->Init();
            if (init.IsError()) {
                if (cfg.skip_device_failures) {
                    ++result.devices_failed;
                    impl_->failures.push_back(
                        {name, dev->GetUri(), dev->GetName(),
                         init.Error(), "init"});
                    continue;
                }
                dm.Reset();
                if (impl_->properties_loaded) {
                    config::PropertyManager::GetInstance().Reset();
                    core::Properties::DestroyAll();
                    impl_->properties_loaded = false;
                }
                return core::Result<BootstrapResult>::Err(init.Error());
            }

            auto open = dev->Open();
            if (open.IsError()) {
                if (cfg.skip_device_failures) {
                    ++result.devices_failed;
                    impl_->failures.push_back(
                        {name, dev->GetUri(), dev->GetName(),
                         open.Error(), "open"});
                    continue;
                }
                dm.Reset();
                if (impl_->properties_loaded) {
                    config::PropertyManager::GetInstance().Reset();
                    core::Properties::DestroyAll();
                    impl_->properties_loaded = false;
                }
                return core::Result<BootstrapResult>::Err(open.Error());
            }

            ++result.devices_opened;
        }
    } else {
        // Count successfully created devices as "opened" (loaded, not opened)
        result.devices_opened = dm.DeviceCount();
    }

    result.failures = impl_->failures;
    impl_->initialized = true;
    return core::Result<BootstrapResult>::Ok(std::move(result));
}

// ---------------------------------------------------------------------------
// Deinit
// ---------------------------------------------------------------------------

void Bootstrap::Deinit() {
    if (!impl_ || !impl_->initialized) return;

    // 1. Close + clear devices
    hal::DeviceManager::GetInstance().Reset();

    // 2. Properties cleanup
    if (impl_->properties_loaded) {
        config::PropertyManager::GetInstance().Reset();
        core::Properties::DestroyAll();
        impl_->properties_loaded = false;
    }

    // Logger: no teardown (process lifetime)

    impl_->failures.clear();
    impl_->initialized = false;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

bool Bootstrap::IsInitialized() const { return impl_->initialized; }

hal::DeviceManager* Bootstrap::GetDeviceManager() {
    return &hal::DeviceManager::GetInstance();
}

config::PropertyManager* Bootstrap::GetPropertyManager() {
    return &config::PropertyManager::GetInstance();
}

hal::Device* Bootstrap::GetDevice(const std::string& nickname) {
    return hal::DeviceManager::GetInstance().GetDevice(nickname);
}

std::vector<std::string> Bootstrap::DeviceNames() const {
    return hal::DeviceManager::GetInstance().DeviceNames();
}

const std::vector<DeviceFailure>& Bootstrap::GetFailures() const {
    return impl_->failures;
}

}  // namespace plas::bootstrap
