/// @file config_load.cpp
/// @brief Example: Loading device configurations from JSON and YAML.
///
/// Demonstrates how to:
///  1. Load a flat JSON config (devices array)
///  2. Load a grouped YAML config via key_path
///  3. Load directly from a ConfigNode
///  4. Iterate devices and search by nickname

#include <cstdio>
#include <string>

#include "plas/config/config.h"
#include "plas/config/config_node.h"
#include "plas/config/device_entry.h"

static std::string FixturePath(const char* name) {
    return std::string("fixtures/") + name;
}

static void PrintDevices(const plas::config::Config& cfg) {
    for (const auto& dev : cfg.GetDevices()) {
        std::printf("    nickname=%-12s driver=%-10s uri=%s\n",
                    dev.nickname.c_str(), dev.driver.c_str(),
                    dev.uri.c_str());
        for (const auto& [k, v] : dev.args) {
            std::printf("      arg: %s = %s\n", k.c_str(), v.c_str());
        }
    }
}

int main() {
    using namespace plas::config;

    // --- 1. Flat JSON format ---
    std::printf("[*] Loading flat JSON config...\n");
    auto flat = Config::LoadFromFile(FixturePath("devices.json"));
    if (flat.IsError()) {
        std::printf("Error: %s\n", flat.Error().message().c_str());
        return 1;
    }
    PrintDevices(flat.Value());

    // --- 2. Grouped YAML with key_path ---
    std::printf("\n[*] Loading grouped YAML config (key_path: plas.devices)...\n");
    auto grouped = Config::LoadFromFile(
        FixturePath("devices_grouped.yaml"), "plas.devices");
    if (grouped.IsError()) {
        std::printf("Error: %s\n", grouped.Error().message().c_str());
        return 1;
    }
    PrintDevices(grouped.Value());

    // --- 3. Load from ConfigNode ---
    std::printf("\n[*] Loading from ConfigNode...\n");
    auto node = ConfigNode::LoadFromFile(FixturePath("devices_grouped.yaml"));
    if (node.IsError()) {
        std::printf("Error loading node: %s\n",
                    node.Error().message().c_str());
        return 1;
    }
    auto subtree = node.Value().GetSubtree("plas.devices");
    if (subtree.IsError()) {
        std::printf("Error getting subtree: %s\n",
                    subtree.Error().message().c_str());
        return 1;
    }
    auto from_node = Config::LoadFromNode(subtree.Value());
    if (from_node.IsError()) {
        std::printf("Error: %s\n", from_node.Error().message().c_str());
        return 1;
    }
    PrintDevices(from_node.Value());

    // --- 4. FindDevice ---
    std::printf("\n[*] FindDevice('aardvark0')...\n");
    auto found = flat.Value().FindDevice("aardvark0");
    if (found) {
        std::printf("    Found: driver=%s uri=%s\n",
                    found->driver.c_str(), found->uri.c_str());
    } else {
        std::printf("    Not found\n");
    }

    auto missing = flat.Value().FindDevice("nonexistent");
    std::printf("[*] FindDevice('nonexistent'): %s\n",
                missing ? "found" : "not found");

    std::printf("[*] Done.\n");
    return 0;
}
