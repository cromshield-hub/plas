#include <gtest/gtest.h>

#include "plas/bootstrap/bootstrap.h"
#include "plas/config/property_manager.h"
#include "plas/core/error.h"
#include "plas/core/properties.h"
#include "plas/hal/device_manager.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/i2c.h"
#include "plas/hal/interface/power_control.h"

using plas::bootstrap::Bootstrap;
using plas::bootstrap::BootstrapConfig;
using plas::bootstrap::BootstrapResult;
using plas::bootstrap::DeviceFailure;
using plas::core::ErrorCode;
using plas::hal::Device;
using plas::hal::DeviceManager;
using plas::hal::DeviceState;
using plas::hal::I2c;
using plas::hal::PowerControl;

class BootstrapTest : public ::testing::Test {
protected:
    void SetUp() override {
        DeviceManager::GetInstance().Reset();
        plas::config::PropertyManager::GetInstance().Reset();
        plas::core::Properties::DestroyAll();
    }

    void TearDown() override {
        DeviceManager::GetInstance().Reset();
        plas::config::PropertyManager::GetInstance().Reset();
        plas::core::Properties::DestroyAll();
    }

    std::string FixturePath(const std::string& filename) {
        return "fixtures/" + filename;
    }
};

// ===========================================================================
// Construction / State
// ===========================================================================

TEST_F(BootstrapTest, DefaultConstruction) {
    Bootstrap bs;
    EXPECT_FALSE(bs.IsInitialized());
}

TEST_F(BootstrapTest, MoveConstruction) {
    Bootstrap bs1;
    Bootstrap bs2(std::move(bs1));
    EXPECT_FALSE(bs2.IsInitialized());
}

TEST_F(BootstrapTest, MoveAssignment) {
    Bootstrap bs1;
    Bootstrap bs2;
    bs2 = std::move(bs1);
    EXPECT_FALSE(bs2.IsInitialized());
}

TEST_F(BootstrapTest, DestructorCallsDeinit) {
    // Init, then destroy without explicit Deinit — devices should be cleaned up
    {
        Bootstrap bs;
        BootstrapConfig cfg;
        cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
        auto result = bs.Init(cfg);
        ASSERT_TRUE(result.IsOk()) << result.Error().message();
        EXPECT_GT(DeviceManager::GetInstance().DeviceCount(), 0u);
    }
    // After destruction, DeviceManager should be reset
    EXPECT_EQ(DeviceManager::GetInstance().DeviceCount(), 0u);
}

TEST_F(BootstrapTest, DoubleInitFails) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");

    auto r1 = bs.Init(cfg);
    ASSERT_TRUE(r1.IsOk()) << r1.Error().message();

    auto r2 = bs.Init(cfg);
    EXPECT_TRUE(r2.IsError());
    EXPECT_EQ(r2.Error(), plas::core::make_error_code(ErrorCode::kAlreadyOpen));
}

// ===========================================================================
// RegisterAllDrivers
// ===========================================================================

TEST_F(BootstrapTest, RegisterAllDriversSucceeds) {
    EXPECT_NO_FATAL_FAILURE(Bootstrap::RegisterAllDrivers());
}

TEST_F(BootstrapTest, RegisterAllDriversIdempotent) {
    Bootstrap::RegisterAllDrivers();
    EXPECT_NO_FATAL_FAILURE(Bootstrap::RegisterAllDrivers());
}

// ===========================================================================
// Init success cases
// ===========================================================================

TEST_F(BootstrapTest, InitMinimalConfig) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_TRUE(bs.IsInitialized());
    EXPECT_EQ(result.Value().devices_opened, 2u);
    EXPECT_EQ(result.Value().devices_failed, 0u);
    EXPECT_EQ(result.Value().devices_skipped, 0u);
}

TEST_F(BootstrapTest, InitWithKeyPath) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_grouped_config.yaml");
    cfg.device_config_key_path = "plas.devices";

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_EQ(result.Value().devices_opened, 2u);
}

TEST_F(BootstrapTest, InitWithProperties) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    cfg.properties_config_path = FixturePath("bootstrap_properties_config.yaml");

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    auto* pm = bs.GetPropertyManager();
    ASSERT_NE(pm, nullptr);
    EXPECT_TRUE(pm->HasSession("session_a"));
    EXPECT_TRUE(pm->HasSession("session_b"));
}

TEST_F(BootstrapTest, InitAutoOpenFalse) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    cfg.auto_open_devices = false;

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    // Devices are created but NOT init/opened
    EXPECT_EQ(result.Value().devices_opened, 2u);  // counted as loaded

    auto* dev = bs.GetDevice("aardvark0");
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->GetState(), DeviceState::kUninitialized);
}

TEST_F(BootstrapTest, InitAutoOpenTrue) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    cfg.auto_open_devices = true;

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    auto* dev = bs.GetDevice("aardvark0");
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->GetState(), DeviceState::kOpen);
}

TEST_F(BootstrapTest, InitDevicesAreOpenedAfterInit) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    for (const auto& name : bs.DeviceNames()) {
        auto* dev = bs.GetDevice(name);
        ASSERT_NE(dev, nullptr);
        EXPECT_EQ(dev->GetState(), DeviceState::kOpen);
    }
}

TEST_F(BootstrapTest, InitWithLogConfig) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    cfg.log_config = plas::log::LogConfig{};
    cfg.log_config->level = plas::log::LogLevel::kDebug;
    cfg.log_config->console_enabled = false;

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
}

// ===========================================================================
// Graceful degradation
// ===========================================================================

TEST_F(BootstrapTest, SkipUnknownDrivers) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_unknown_driver_config.yaml");
    cfg.skip_unknown_drivers = true;

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_EQ(result.Value().devices_opened, 2u);    // 2 aardvarks
    EXPECT_EQ(result.Value().devices_skipped, 1u);    // 1 unknown
    EXPECT_EQ(result.Value().failures.size(), 1u);
    EXPECT_EQ(result.Value().failures[0].nickname, "fake0");
    EXPECT_EQ(result.Value().failures[0].driver, "nonexistent_driver");
    EXPECT_EQ(result.Value().failures[0].phase, "create");
}

TEST_F(BootstrapTest, StrictModeUnknownDriverFails) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_unknown_driver_config.yaml");
    cfg.skip_unknown_drivers = false;
    cfg.skip_device_failures = false;

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              plas::core::make_error_code(ErrorCode::kNotFound));
}

TEST_F(BootstrapTest, SkipDeviceFailureOnCreate) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_unknown_driver_config.yaml");
    cfg.skip_unknown_drivers = false;  // treat unknown as device failure
    cfg.skip_device_failures = true;

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_EQ(result.Value().devices_opened, 2u);
    EXPECT_EQ(result.Value().devices_failed, 1u);
}

TEST_F(BootstrapTest, SkipDeviceFailureOnInit) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_invalid_uri_config.yaml");
    cfg.skip_device_failures = true;

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    // bad_aardvark has invalid URI format, Init should fail
    EXPECT_EQ(result.Value().devices_failed, 1u);
    EXPECT_EQ(result.Value().failures.size(), 1u);
    EXPECT_EQ(result.Value().failures[0].nickname, "bad_aardvark");
    EXPECT_EQ(result.Value().failures[0].phase, "init");
}

TEST_F(BootstrapTest, StrictModeDeviceFailureOnInit) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_invalid_uri_config.yaml");
    cfg.skip_device_failures = false;

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
}

// ===========================================================================
// Error cases
// ===========================================================================

TEST_F(BootstrapTest, InitEmptyPathFails) {
    Bootstrap bs;
    BootstrapConfig cfg;
    // device_config_path is empty

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              plas::core::make_error_code(ErrorCode::kInvalidArgument));
}

TEST_F(BootstrapTest, InitFileNotFoundFails) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = "nonexistent_file.yaml";

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
}

TEST_F(BootstrapTest, InitMalformedConfigFails) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_malformed.yaml");

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
}

TEST_F(BootstrapTest, InitBadPropertiesPathRollsBack) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    cfg.properties_config_path = "nonexistent_properties.yaml";

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
    EXPECT_FALSE(bs.IsInitialized());
}

TEST_F(BootstrapTest, ConfigParseFailureRollsBackProperties) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_malformed.yaml");
    cfg.properties_config_path = FixturePath("bootstrap_properties_config.yaml");

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
    EXPECT_FALSE(bs.IsInitialized());
    // Properties should be rolled back
    EXPECT_FALSE(
        plas::config::PropertyManager::GetInstance().HasSession("session_a"));
}

TEST_F(BootstrapTest, StrictModeRollsBackDevices) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_unknown_driver_config.yaml");
    cfg.skip_unknown_drivers = false;
    cfg.skip_device_failures = false;

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
    // Devices should be rolled back
    EXPECT_EQ(DeviceManager::GetInstance().DeviceCount(), 0u);
}

// ===========================================================================
// Accessors
// ===========================================================================

TEST_F(BootstrapTest, GetDeviceManager) {
    Bootstrap bs;
    auto* dm = bs.GetDeviceManager();
    ASSERT_NE(dm, nullptr);
    EXPECT_EQ(dm, &DeviceManager::GetInstance());
}

TEST_F(BootstrapTest, GetPropertyManager) {
    Bootstrap bs;
    auto* pm = bs.GetPropertyManager();
    ASSERT_NE(pm, nullptr);
}

TEST_F(BootstrapTest, GetDeviceAfterInit) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto* dev = bs.GetDevice("aardvark0");
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->GetName(), "aardvark0");
}

TEST_F(BootstrapTest, GetDeviceNotFound) {
    Bootstrap bs;
    auto* dev = bs.GetDevice("nonexistent");
    EXPECT_EQ(dev, nullptr);
}

TEST_F(BootstrapTest, GetInterfaceI2c) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto* i2c = bs.GetInterface<I2c>("aardvark0");
    EXPECT_NE(i2c, nullptr);
}

TEST_F(BootstrapTest, GetInterfaceUnsupported) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto* power = bs.GetInterface<PowerControl>("aardvark0");
    EXPECT_EQ(power, nullptr);
}

TEST_F(BootstrapTest, GetInterfaceNonexistentDevice) {
    Bootstrap bs;
    auto* i2c = bs.GetInterface<I2c>("nonexistent");
    EXPECT_EQ(i2c, nullptr);
}

TEST_F(BootstrapTest, DeviceNames) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto names = bs.DeviceNames();
    ASSERT_EQ(names.size(), 2u);
    // std::map sorted: aardvark0 < aardvark1
    EXPECT_EQ(names[0], "aardvark0");
    EXPECT_EQ(names[1], "aardvark1");
}

TEST_F(BootstrapTest, GetFailuresEmpty) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    EXPECT_TRUE(bs.GetFailures().empty());
}

TEST_F(BootstrapTest, GetFailuresWithSkipped) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_unknown_driver_config.yaml");
    cfg.skip_unknown_drivers = true;
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    const auto& failures = bs.GetFailures();
    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].nickname, "fake0");
    EXPECT_EQ(failures[0].driver, "nonexistent_driver");
}

// ===========================================================================
// Deinit
// ===========================================================================

TEST_F(BootstrapTest, DeinitClearsDevices) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());
    EXPECT_GT(DeviceManager::GetInstance().DeviceCount(), 0u);

    bs.Deinit();
    EXPECT_FALSE(bs.IsInitialized());
    EXPECT_EQ(DeviceManager::GetInstance().DeviceCount(), 0u);
}

TEST_F(BootstrapTest, DeinitIdempotent) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    bs.Deinit();
    EXPECT_FALSE(bs.IsInitialized());
    // Second Deinit should be safe
    EXPECT_NO_FATAL_FAILURE(bs.Deinit());
}

TEST_F(BootstrapTest, DeinitWithoutInitIsNoop) {
    Bootstrap bs;
    EXPECT_NO_FATAL_FAILURE(bs.Deinit());
}

TEST_F(BootstrapTest, DeinitCleansUpProperties) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    cfg.properties_config_path = FixturePath("bootstrap_properties_config.yaml");

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(
        plas::config::PropertyManager::GetInstance().HasSession("session_a"));

    bs.Deinit();
    EXPECT_FALSE(
        plas::config::PropertyManager::GetInstance().HasSession("session_a"));
}

TEST_F(BootstrapTest, CanReinitAfterDeinit) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");

    auto r1 = bs.Init(cfg);
    ASSERT_TRUE(r1.IsOk());
    bs.Deinit();

    auto r2 = bs.Init(cfg);
    ASSERT_TRUE(r2.IsOk()) << r2.Error().message();
    EXPECT_TRUE(bs.IsInitialized());
    EXPECT_EQ(r2.Value().devices_opened, 2u);
}

// ===========================================================================
// GetDeviceByUri
// ===========================================================================

TEST_F(BootstrapTest, GetDeviceByUriFound) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto* dev = bs.GetDeviceByUri("aardvark://0:0x50");
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->GetUri(), "aardvark://0:0x50");
}

TEST_F(BootstrapTest, GetDeviceByUriNotFound) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto* dev = bs.GetDeviceByUri("aardvark://99:0xFF");
    EXPECT_EQ(dev, nullptr);
}

TEST_F(BootstrapTest, GetDeviceByUriBeforeInit) {
    Bootstrap bs;
    auto* dev = bs.GetDeviceByUri("aardvark://0:0x50");
    EXPECT_EQ(dev, nullptr);
}

// ===========================================================================
// GetDevicesByInterface
// ===========================================================================

TEST_F(BootstrapTest, GetDevicesByInterfaceI2c) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto i2c_devices = bs.GetDevicesByInterface<I2c>();
    ASSERT_EQ(i2c_devices.size(), 2u);  // both aardvarks support I2c
    // Map is sorted, so aardvark0 first
    EXPECT_EQ(i2c_devices[0].first, "aardvark0");
    EXPECT_NE(i2c_devices[0].second, nullptr);
    EXPECT_EQ(i2c_devices[1].first, "aardvark1");
    EXPECT_NE(i2c_devices[1].second, nullptr);
}

TEST_F(BootstrapTest, GetDevicesByInterfaceNoneMatch) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto power_devices = bs.GetDevicesByInterface<PowerControl>();
    EXPECT_TRUE(power_devices.empty());
}

TEST_F(BootstrapTest, GetDevicesByInterfaceBeforeInit) {
    Bootstrap bs;
    auto i2c_devices = bs.GetDevicesByInterface<I2c>();
    EXPECT_TRUE(i2c_devices.empty());
}

// ===========================================================================
// DumpDevices
// ===========================================================================

TEST_F(BootstrapTest, DumpDevicesAfterInit) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_basic_config.yaml");
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto dump = bs.DumpDevices();
    EXPECT_FALSE(dump.empty());
    EXPECT_NE(dump.find("Devices (2)"), std::string::npos);
    EXPECT_NE(dump.find("aardvark0"), std::string::npos);
    EXPECT_NE(dump.find("aardvark1"), std::string::npos);
    EXPECT_NE(dump.find("aardvark://0:0x50"), std::string::npos);
    EXPECT_NE(dump.find("I2c"), std::string::npos);
    EXPECT_NE(dump.find("open"), std::string::npos);
}

TEST_F(BootstrapTest, DumpDevicesIncludesFailures) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_unknown_driver_config.yaml");
    cfg.skip_unknown_drivers = true;
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    auto dump = bs.DumpDevices();
    EXPECT_NE(dump.find("Failures (1)"), std::string::npos);
    EXPECT_NE(dump.find("fake0"), std::string::npos);
}

TEST_F(BootstrapTest, DumpDevicesEmpty) {
    Bootstrap bs;
    auto dump = bs.DumpDevices();
    EXPECT_NE(dump.find("Devices (0)"), std::string::npos);
}

// ===========================================================================
// ValidateUri
// ===========================================================================

TEST_F(BootstrapTest, ValidateUriValid) {
    EXPECT_TRUE(Bootstrap::ValidateUri("aardvark://0:0x50"));
    EXPECT_TRUE(Bootstrap::ValidateUri("ft4222h://0:1"));
    EXPECT_TRUE(Bootstrap::ValidateUri("pciutils://0000:03:00.0"));
    EXPECT_TRUE(Bootstrap::ValidateUri("pmu3://usb:PMU3-001"));
}

TEST_F(BootstrapTest, ValidateUriMissingScheme) {
    EXPECT_FALSE(Bootstrap::ValidateUri("0:0x50"));
    EXPECT_FALSE(Bootstrap::ValidateUri("://0:0x50"));
}

TEST_F(BootstrapTest, ValidateUriMissingColon) {
    EXPECT_FALSE(Bootstrap::ValidateUri("aardvark://invalid_uri_format"));
}

TEST_F(BootstrapTest, ValidateUriEmptyParts) {
    EXPECT_FALSE(Bootstrap::ValidateUri("aardvark://:0x50"));
    EXPECT_FALSE(Bootstrap::ValidateUri("aardvark://0:"));
    EXPECT_FALSE(Bootstrap::ValidateUri(""));
}

TEST_F(BootstrapTest, ValidateUriEmptyAuthority) {
    EXPECT_FALSE(Bootstrap::ValidateUri("aardvark://"));
}

// ===========================================================================
// URI validation during Init
// ===========================================================================

TEST_F(BootstrapTest, InitUriValidationCatchesBadFormat) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_bad_uri_format_config.yaml");
    cfg.skip_device_failures = true;

    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_EQ(result.Value().devices_failed, 1u);
    EXPECT_EQ(result.Value().failures.size(), 1u);
    EXPECT_EQ(result.Value().failures[0].nickname, "bad_format");
    EXPECT_EQ(result.Value().failures[0].phase, "create");
    EXPECT_NE(result.Value().failures[0].detail.find("invalid URI format"),
              std::string::npos);
}

TEST_F(BootstrapTest, InitUriValidationStrictModeFails) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_bad_uri_format_config.yaml");
    cfg.skip_device_failures = false;

    auto result = bs.Init(cfg);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              plas::core::make_error_code(ErrorCode::kInvalidArgument));
}

// ===========================================================================
// DeviceFailure detail field
// ===========================================================================

TEST_F(BootstrapTest, FailureDetailOnUnknownDriver) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_unknown_driver_config.yaml");
    cfg.skip_unknown_drivers = true;
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    const auto& failures = bs.GetFailures();
    ASSERT_EQ(failures.size(), 1u);
    EXPECT_FALSE(failures[0].detail.empty());
    EXPECT_NE(failures[0].detail.find("not registered"), std::string::npos);
}

TEST_F(BootstrapTest, FailureDetailOnInitError) {
    Bootstrap bs;
    BootstrapConfig cfg;
    cfg.device_config_path = FixturePath("bootstrap_invalid_uri_config.yaml");
    cfg.skip_device_failures = true;
    auto result = bs.Init(cfg);
    ASSERT_TRUE(result.IsOk());

    const auto& failures = bs.GetFailures();
    ASSERT_EQ(failures.size(), 1u);
    EXPECT_FALSE(failures[0].detail.empty());
    EXPECT_NE(failures[0].detail.find("init failed"), std::string::npos);
}
