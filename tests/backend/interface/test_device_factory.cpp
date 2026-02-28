#include <gtest/gtest.h>

#include <memory>

#include "plas/backend/interface/device.h"
#include "plas/backend/interface/device_factory.h"
#include "plas/backend/interface/i2c.h"
#include "plas/backend/interface/power_control.h"
#include "plas/backend/interface/ssd_gpio.h"
#include "plas/config/device_entry.h"
#include "plas/core/error.h"

// Include driver headers to trigger registration
#include "plas/backend/driver/aardvark/aardvark_device.h"
#include "plas/backend/driver/ft4222h/ft4222h_device.h"
#include "plas/backend/driver/pmu3/pmu3_device.h"
#include "plas/backend/driver/pmu4/pmu4_device.h"

using plas::backend::Device;
using plas::backend::DeviceFactory;
using plas::backend::DeviceState;
using plas::backend::I2c;
using plas::backend::PowerControl;
using plas::backend::SsdGpio;
using plas::config::DeviceEntry;

namespace {

// Ensure driver registration happens
struct DriverRegistrar {
    DriverRegistrar() {
        plas::backend::driver::AardvarkDevice::Register();
        plas::backend::driver::Ft4222hDevice::Register();
        plas::backend::driver::Pmu3Device::Register();
        plas::backend::driver::Pmu4Device::Register();
    }
};

static DriverRegistrar registrar;

}  // namespace

TEST(DeviceFactoryTest, CreateAardvark) {
    DeviceEntry entry;
    entry.nickname = "aardvark0";
    entry.uri = "aardvark://0:0x50";
    entry.driver = "aardvark";

    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    auto& device = result.Value();
    EXPECT_EQ(device->GetName(), "aardvark0");
    EXPECT_EQ(device->GetUri(), "aardvark://0:0x50");
    EXPECT_EQ(device->GetState(), DeviceState::kUninitialized);
}

TEST(DeviceFactoryTest, AardvarkSupportsI2c) {
    DeviceEntry entry;
    entry.nickname = "aardvark0";
    entry.uri = "aardvark://0:0x50";
    entry.driver = "aardvark";

    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());

    auto* i2c = dynamic_cast<I2c*>(result.Value().get());
    EXPECT_NE(i2c, nullptr) << "AardvarkDevice should implement I2c";
}

TEST(DeviceFactoryTest, CreatePmu3) {
    DeviceEntry entry;
    entry.nickname = "pmu3_main";
    entry.uri = "pmu3://usb:PMU3-001";
    entry.driver = "pmu3";

    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    auto& device = result.Value();
    EXPECT_EQ(device->GetName(), "pmu3_main");

    auto* power = dynamic_cast<PowerControl*>(device.get());
    EXPECT_NE(power, nullptr) << "Pmu3Device should implement PowerControl";

    auto* gpio = dynamic_cast<SsdGpio*>(device.get());
    EXPECT_NE(gpio, nullptr) << "Pmu3Device should implement SsdGpio";
}

TEST(DeviceFactoryTest, CreatePmu4) {
    DeviceEntry entry;
    entry.nickname = "pmu4_main";
    entry.uri = "pmu4://usb:PMU4-001";
    entry.driver = "pmu4";

    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    auto* power = dynamic_cast<PowerControl*>(result.Value().get());
    EXPECT_NE(power, nullptr);
}

TEST(DeviceFactoryTest, CreateFt4222h) {
    DeviceEntry entry;
    entry.nickname = "ft4222_0";
    entry.uri = "ft4222h://0:0x50";
    entry.driver = "ft4222h";

    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk()) << result.Error().message();

    auto* i2c = dynamic_cast<I2c*>(result.Value().get());
    EXPECT_NE(i2c, nullptr);
}

TEST(DeviceFactoryTest, UnknownDriverFails) {
    DeviceEntry entry;
    entry.nickname = "unknown";
    entry.uri = "unknown://0";
    entry.driver = "nonexistent_driver";

    auto result = DeviceFactory::CreateFromConfig(entry);
    EXPECT_TRUE(result.IsError());
}

TEST(DeviceFactoryTest, DeviceLifecycle) {
    DeviceEntry entry;
    entry.nickname = "lifecycle_test";
    entry.uri = "aardvark://0:0x50";
    entry.driver = "aardvark";

    auto result = DeviceFactory::CreateFromConfig(entry);
    ASSERT_TRUE(result.IsOk());

    auto& device = result.Value();
    EXPECT_EQ(device->GetState(), DeviceState::kUninitialized);

    EXPECT_TRUE(device->Init().IsOk());
    EXPECT_EQ(device->GetState(), DeviceState::kInitialized);

    EXPECT_TRUE(device->Open().IsOk());
    EXPECT_EQ(device->GetState(), DeviceState::kOpen);

    EXPECT_TRUE(device->Close().IsOk());
    EXPECT_EQ(device->GetState(), DeviceState::kClosed);
}
