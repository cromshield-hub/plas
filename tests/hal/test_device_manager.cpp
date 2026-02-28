#include <gtest/gtest.h>

#include "plas/hal/device_manager.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/i2c.h"
#include "plas/hal/interface/power_control.h"
#include "plas/hal/interface/ssd_gpio.h"
#include "plas/config/device_entry.h"
#include "plas/core/error.h"

// Include driver headers for registration
#include "plas/hal/driver/aardvark/aardvark_device.h"
#include "plas/hal/driver/ft4222h/ft4222h_device.h"
#include "plas/hal/driver/pmu3/pmu3_device.h"
#include "plas/hal/driver/pmu4/pmu4_device.h"

using plas::hal::DeviceManager;
using plas::hal::DeviceState;
using plas::hal::I2c;
using plas::hal::PowerControl;
using plas::hal::SsdGpio;
using plas::config::DeviceEntry;

namespace {

struct DriverRegistrar {
    DriverRegistrar() {
        plas::hal::driver::AardvarkDevice::Register();
        plas::hal::driver::Ft4222hDevice::Register();
        plas::hal::driver::Pmu3Device::Register();
        plas::hal::driver::Pmu4Device::Register();
    }
};

static DriverRegistrar registrar;

}  // namespace

class DeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        DeviceManager::GetInstance().Reset();
    }

    void TearDown() override {
        DeviceManager::GetInstance().Reset();
    }

    std::string FixturePath(const std::string& filename) {
        return "fixtures/" + filename;
    }
};

// --- Singleton tests ---

TEST_F(DeviceManagerTest, GetInstanceReturnsSameReference) {
    auto& inst1 = DeviceManager::GetInstance();
    auto& inst2 = DeviceManager::GetInstance();
    EXPECT_EQ(&inst1, &inst2);
}

TEST_F(DeviceManagerTest, InitialStateEmpty) {
    EXPECT_EQ(DeviceManager::GetInstance().DeviceCount(), 0u);
    EXPECT_TRUE(DeviceManager::GetInstance().DeviceNames().empty());
}

// --- LoadFromConfig tests ---

TEST_F(DeviceManagerTest, LoadFromConfigJson) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_EQ(mgr.DeviceCount(), 2u);
}

TEST_F(DeviceManagerTest, LoadFromConfigYaml) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.yaml"));
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_EQ(mgr.DeviceCount(), 2u);
}

TEST_F(DeviceManagerTest, LoadFromConfigDeviceNames) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());

    auto names = mgr.DeviceNames();
    ASSERT_EQ(names.size(), 2u);
    // std::map is sorted, so aardvark0 < pmu3_main
    EXPECT_EQ(names[0], "aardvark0");
    EXPECT_EQ(names[1], "pmu3_main");
}

TEST_F(DeviceManagerTest, LoadFromConfigDeviceCount) {
    auto& mgr = DeviceManager::GetInstance();
    EXPECT_EQ(mgr.DeviceCount(), 0u);

    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(mgr.DeviceCount(), 2u);
}

// --- LoadFromEntries tests ---

TEST_F(DeviceManagerTest, LoadFromEntries) {
    auto& mgr = DeviceManager::GetInstance();

    std::vector<DeviceEntry> entries;
    entries.push_back({"aardvark0", "aardvark://0:0x50", "aardvark", {}});
    entries.push_back({"pmu3_main", "pmu3://usb:PMU3-001", "pmu3", {}});

    auto result = mgr.LoadFromEntries(entries);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();
    EXPECT_EQ(mgr.DeviceCount(), 2u);
    EXPECT_TRUE(mgr.HasDevice("aardvark0"));
    EXPECT_TRUE(mgr.HasDevice("pmu3_main"));
}

TEST_F(DeviceManagerTest, LoadFromEntriesEmpty) {
    auto& mgr = DeviceManager::GetInstance();
    std::vector<DeviceEntry> entries;

    auto result = mgr.LoadFromEntries(entries);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(mgr.DeviceCount(), 0u);
}

// --- GetDevice tests ---

TEST_F(DeviceManagerTest, GetDeviceFound) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());

    auto* device = mgr.GetDevice("aardvark0");
    ASSERT_NE(device, nullptr);
}

TEST_F(DeviceManagerTest, GetDeviceNotFound) {
    auto& mgr = DeviceManager::GetInstance();
    auto* device = mgr.GetDevice("nonexistent");
    EXPECT_EQ(device, nullptr);
}

TEST_F(DeviceManagerTest, GetDeviceProperties) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());

    auto* device = mgr.GetDevice("aardvark0");
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->GetName(), "aardvark0");
    EXPECT_EQ(device->GetUri(), "aardvark://0:0x50");
}

// --- GetInterface tests ---

TEST_F(DeviceManagerTest, GetInterfaceI2c) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());

    auto* i2c = mgr.GetInterface<I2c>("aardvark0");
    EXPECT_NE(i2c, nullptr);
}

TEST_F(DeviceManagerTest, GetInterfaceNotSupported) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());

    // Aardvark doesn't implement PowerControl
    auto* power = mgr.GetInterface<PowerControl>("aardvark0");
    EXPECT_EQ(power, nullptr);
}

TEST_F(DeviceManagerTest, GetInterfaceNonexistentDevice) {
    auto& mgr = DeviceManager::GetInstance();
    auto* i2c = mgr.GetInterface<I2c>("nonexistent");
    EXPECT_EQ(i2c, nullptr);
}

// --- Error cases ---

TEST_F(DeviceManagerTest, LoadFromConfigFileNotFound) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig("nonexistent_file.json");
    EXPECT_TRUE(result.IsError());
}

TEST_F(DeviceManagerTest, LoadFromConfigUnknownDriver) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(
        FixturePath("device_manager_unknown_driver.json"));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              plas::core::make_error_code(plas::core::ErrorCode::kNotFound));
}

TEST_F(DeviceManagerTest, LoadFromConfigDuplicateNickname) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(
        FixturePath("device_manager_duplicate.json"));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(),
              plas::core::make_error_code(plas::core::ErrorCode::kAlreadyOpen));
}

// --- Reset tests ---

TEST_F(DeviceManagerTest, ResetClearsDevices) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(mgr.DeviceCount(), 2u);

    mgr.Reset();
    EXPECT_EQ(mgr.DeviceCount(), 0u);
}

TEST_F(DeviceManagerTest, ResetDevicesNoLongerAccessible) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());

    mgr.Reset();
    EXPECT_EQ(mgr.GetDevice("aardvark0"), nullptr);
    EXPECT_EQ(mgr.GetDevice("pmu3_main"), nullptr);
}

// --- Lifecycle test ---

TEST_F(DeviceManagerTest, DeviceLifecycleInitOpenClose) {
    auto& mgr = DeviceManager::GetInstance();
    auto result = mgr.LoadFromConfig(FixturePath("device_manager_test.json"));
    ASSERT_TRUE(result.IsOk());

    auto* device = mgr.GetDevice("aardvark0");
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->GetState(), DeviceState::kUninitialized);

    EXPECT_TRUE(device->Init().IsOk());
    EXPECT_EQ(device->GetState(), DeviceState::kInitialized);

    EXPECT_TRUE(device->Open().IsOk());
    EXPECT_EQ(device->GetState(), DeviceState::kOpen);

    EXPECT_TRUE(device->Close().IsOk());
    EXPECT_EQ(device->GetState(), DeviceState::kClosed);
}
