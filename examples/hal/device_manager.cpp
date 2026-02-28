/// @file device_manager.cpp
/// @brief Example: DeviceManager with driver registration and config loading.
///
/// Demonstrates how to:
///  1. Register drivers with the factory
///  2. Load devices from a grouped YAML config
///  3. Query and retrieve devices by nickname
///  4. Cast to interface types (dynamic_cast via GetInterface)
///  5. Device lifecycle (Init -> Open -> Close)
///  6. Load from ConfigNode tree

#include <cstdio>
#include <string>

#include "plas/config/config_node.h"
#include "plas/hal/device_manager.h"
#include "plas/hal/interface/device.h"
#include "plas/hal/interface/i2c.h"
#include "plas/hal/interface/power_control.h"
#include "plas/hal/driver/aardvark/aardvark_device.h"
#include "plas/hal/driver/ft4222h/ft4222h_device.h"
#include "plas/hal/driver/pmu3/pmu3_device.h"
#include "plas/hal/driver/pmu4/pmu4_device.h"

static std::string FixturePath(const char* name) {
    return std::string("fixtures/") + name;
}

int main() {
    using namespace plas::hal;
    using namespace plas::config;

    // --- 1. Register all drivers ---
    driver::AardvarkDevice::Register();
    driver::Ft4222hDevice::Register();
    driver::Pmu3Device::Register();
    driver::Pmu4Device::Register();
    std::printf("[*] Registered 4 drivers\n");

    auto& dm = DeviceManager::GetInstance();

    // --- 2. Load from grouped YAML config ---
    std::printf("\n[*] Loading devices from grouped YAML (key_path: devices)...\n");
    auto result = dm.LoadFromConfig(
        FixturePath("devices_grouped.yaml"), "plas.devices");
    if (result.IsError()) {
        std::printf("Error: %s\n", result.Error().message().c_str());
        return 1;
    }
    std::printf("    Loaded %zu device(s)\n", dm.DeviceCount());

    // --- 3. List all devices ---
    std::printf("\n[*] Registered devices:\n");
    for (const auto& name : dm.DeviceNames()) {
        auto* dev = dm.GetDevice(name);
        if (dev) {
            std::printf("    %-12s uri=%s  state=%d\n",
                        name.c_str(), dev->GetUri().c_str(),
                        static_cast<int>(dev->GetState()));
        }
    }

    // --- 4. Interface casting ---
    std::printf("\n[*] Interface check:\n");
    auto* i2c = dm.GetInterface<I2c>("aardvark0");
    std::printf("    aardvark0 -> I2c:          %s\n",
                i2c ? "supported" : "not supported");

    auto* pwr = dm.GetInterface<PowerControl>("pmu3_main");
    std::printf("    pmu3_main -> PowerControl:  %s\n",
                pwr ? "supported" : "not supported");

    auto* bad = dm.GetInterface<I2c>("pmu3_main");
    std::printf("    pmu3_main -> I2c:           %s\n",
                bad ? "supported" : "not supported");

    // --- 5. Device lifecycle ---
    auto* device = dm.GetDevice("aardvark0");
    if (device) {
        std::printf("\n[*] Device lifecycle for 'aardvark0':\n");

        auto init = device->Init();
        std::printf("    Init:  %s\n", init.IsOk() ? "ok" : "error");

        auto open = device->Open();
        std::printf("    Open:  %s\n", open.IsOk() ? "ok" : "error");

        std::printf("    State: %d\n", static_cast<int>(device->GetState()));

        auto close = device->Close();
        std::printf("    Close: %s\n", close.IsOk() ? "ok" : "error");
    }

    // --- 6. Reset and reload from ConfigNode ---
    dm.Reset();
    std::printf("\n[*] Reset. Loading from ConfigNode...\n");

    auto node = ConfigNode::LoadFromFile(
        FixturePath("devices_grouped.yaml"));
    if (node.IsError()) {
        std::printf("Error: %s\n", node.Error().message().c_str());
        return 1;
    }
    auto subtree = node.Value().GetSubtree("plas.devices");
    if (subtree.IsError()) {
        std::printf("Error: %s\n", subtree.Error().message().c_str());
        return 1;
    }
    auto tree_result = dm.LoadFromTree(subtree.Value());
    if (tree_result.IsError()) {
        std::printf("Error: %s\n", tree_result.Error().message().c_str());
        return 1;
    }
    std::printf("    Loaded %zu device(s) from tree\n", dm.DeviceCount());

    // --- Cleanup ---
    dm.Reset();
    std::printf("[*] Done.\n");
    return 0;
}
