#include "plas/bootstrap/bootstrap.h"

#include <sstream>
#include <utility>

#include "plas/config/config.h"
#include "plas/config/property_manager.h"
#include "plas/core/properties.h"
#include "plas/hal/interface/device_factory.h"
#include "plas/hal/interface/i2c.h"
#include "plas/hal/interface/i3c.h"
#include "plas/hal/interface/power_control.h"
#include "plas/hal/interface/serial.h"
#include "plas/hal/interface/ssd_gpio.h"
#include "plas/hal/interface/uart.h"
#include "plas/hal/interface/pci/pci_bar.h"
#include "plas/hal/interface/pci/pci_config.h"
#include "plas/hal/interface/pci/pci_doe.h"
#include "plas/hal/interface/pci/cxl.h"
#include "plas/hal/interface/pci/cxl_mailbox.h"
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
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Build a detail string for a DeviceFailure from error_code.
std::string MakeDetail(const std::string& phase, std::error_code ec) {
    std::string msg = phase + " failed: " + ec.message();
    if (ec.category().name() != std::string("plas")) {
        msg += " [" + std::string(ec.category().name()) + "]";
    }
    return msg;
}

/// Convert DeviceState enum to readable string.
const char* StateToString(hal::DeviceState state) {
    switch (state) {
        case hal::DeviceState::kUninitialized: return "uninitialized";
        case hal::DeviceState::kInitialized:   return "initialized";
        case hal::DeviceState::kOpen:           return "open";
        case hal::DeviceState::kClosed:         return "closed";
        case hal::DeviceState::kError:          return "error";
    }
    return "unknown";
}

/// Probe all known HAL interface types and return comma-separated list.
std::string ProbeInterfaces(hal::Device* dev) {
    std::string result;
    auto append = [&](const char* name) {
        if (!result.empty()) result += ", ";
        result += name;
    };

    if (dynamic_cast<hal::I2c*>(dev))              append("I2c");
    if (dynamic_cast<hal::I3c*>(dev))              append("I3c");
    if (dynamic_cast<hal::PowerControl*>(dev))     append("PowerControl");
    if (dynamic_cast<hal::Serial*>(dev))           append("Serial");
    if (dynamic_cast<hal::Uart*>(dev))             append("Uart");
    if (dynamic_cast<hal::SsdGpio*>(dev))          append("SsdGpio");
    if (dynamic_cast<hal::pci::PciConfig*>(dev))   append("PciConfig");
    if (dynamic_cast<hal::pci::PciDoe*>(dev))      append("PciDoe");
    if (dynamic_cast<hal::pci::PciBar*>(dev))      append("PciBar");
    if (dynamic_cast<hal::pci::Cxl*>(dev))         append("Cxl");
    if (dynamic_cast<hal::pci::CxlMailbox*>(dev))  append("CxlMailbox");

    return result.empty() ? "(none)" : result;
}

}  // namespace

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
// ValidateUri
// ---------------------------------------------------------------------------

bool Bootstrap::ValidateUri(const std::string& uri) {
    // Expected format: driver://bus:identifier
    auto scheme_end = uri.find("://");
    if (scheme_end == std::string::npos || scheme_end == 0) {
        return false;
    }

    auto authority = uri.substr(scheme_end + 3);
    if (authority.empty()) {
        return false;
    }

    // Must contain at least one colon separating bus and identifier
    auto colon = authority.find(':');
    if (colon == std::string::npos || colon == 0 ||
        colon == authority.size() - 1) {
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

core::Result<BootstrapResult> Bootstrap::Init(const BootstrapConfig& cfg) {
    if (impl_->initialized) {
        return core::Result<BootstrapResult>::Err(
            core::ErrorCode::kAlreadyOpen);
    }

    if (cfg.device_config_path.empty() && !cfg.device_config_node.has_value()) {
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
    core::Result<config::Config> config_result = [&]() -> core::Result<config::Config> {
        if (cfg.device_config_node.has_value()) {
            return config::Config::LoadFromNode(cfg.device_config_node.value());
        }
        if (!cfg.device_config_key_path.empty()) {
            return config::Config::LoadFromFile(cfg.device_config_path,
                                                cfg.device_config_key_path,
                                                cfg.device_config_format);
        }
        return config::Config::LoadFromFile(cfg.device_config_path,
                                            cfg.device_config_format);
    }();

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
        // 5a. URI format validation (early detection)
        if (!ValidateUri(entry.uri)) {
            auto ec = core::make_error_code(core::ErrorCode::kInvalidArgument);
            std::string detail = "invalid URI format: \"" + entry.uri +
                                 "\" (expected driver://bus:identifier)";
            if (cfg.skip_device_failures) {
                ++result.devices_failed;
                impl_->failures.push_back(
                    {entry.nickname, entry.uri, entry.driver,
                     ec, "create", detail});
                continue;
            }
            dm.Reset();
            if (impl_->properties_loaded) {
                config::PropertyManager::GetInstance().Reset();
                core::Properties::DestroyAll();
                impl_->properties_loaded = false;
            }
            return core::Result<BootstrapResult>::Err(ec);
        }

        auto create = hal::DeviceFactory::CreateFromConfig(entry);
        if (create.IsError()) {
            if (create.Error() ==
                    core::make_error_code(core::ErrorCode::kNotFound) &&
                cfg.skip_unknown_drivers) {
                ++result.devices_skipped;
                impl_->failures.push_back(
                    {entry.nickname, entry.uri, entry.driver,
                     create.Error(), "create",
                     MakeDetail("create", create.Error()) +
                         " (driver \"" + entry.driver + "\" not registered)"});
                continue;
            }
            if (cfg.skip_device_failures) {
                ++result.devices_failed;
                impl_->failures.push_back(
                    {entry.nickname, entry.uri, entry.driver,
                     create.Error(), "create",
                     MakeDetail("create", create.Error())});
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
                     add.Error(), "create",
                     MakeDetail("create", add.Error())});
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
                         init.Error(), "init",
                         MakeDetail("init", init.Error())});
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
                         open.Error(), "open",
                         MakeDetail("open", open.Error())});
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

hal::Device* Bootstrap::GetDeviceByUri(const std::string& uri) {
    return hal::DeviceManager::GetInstance().GetDeviceByUri(uri);
}

std::vector<std::string> Bootstrap::DeviceNames() const {
    return hal::DeviceManager::GetInstance().DeviceNames();
}

const std::vector<DeviceFailure>& Bootstrap::GetFailures() const {
    return impl_->failures;
}

// ---------------------------------------------------------------------------
// DumpDevices
// ---------------------------------------------------------------------------

std::string Bootstrap::DumpDevices() const {
    auto& dm = hal::DeviceManager::GetInstance();
    auto names = dm.DeviceNames();

    std::ostringstream os;
    os << "Devices (" << names.size() << "):\n";

    for (const auto& name : names) {
        auto* dev = dm.GetDevice(name);
        if (!dev) continue;

        os << "  " << name << "\n"
           << "    uri:        " << dev->GetUri() << "\n"
           << "    driver:     " << dev->GetName() << "\n"
           << "    state:      " << StateToString(dev->GetState()) << "\n"
           << "    interfaces: " << ProbeInterfaces(dev) << "\n";
    }

    if (!impl_->failures.empty()) {
        os << "Failures (" << impl_->failures.size() << "):\n";
        for (const auto& f : impl_->failures) {
            os << "  " << f.nickname << " [" << f.phase << "]: "
               << f.error.message();
            if (!f.detail.empty()) {
                os << " -- " << f.detail;
            }
            os << "\n";
        }
    }

    return os.str();
}

}  // namespace plas::bootstrap
