/// @file config_node_tree.cpp
/// @brief Example: ConfigNode subtree navigation and type queries.
///
/// Demonstrates how to:
///  1. Load a shared config file as a ConfigNode
///  2. Navigate to a subtree via dot-path (GetSubtree)
///  3. Query node types (IsMap, IsArray, IsScalar, IsNull)
///  4. Chain subtree navigation

#include <cstdio>
#include <string>

#include "plas/config/config_node.h"

static std::string FixturePath(const char* name) {
    return std::string("fixtures/") + name;
}

static const char* NodeType(const plas::config::ConfigNode& node) {
    if (node.IsMap()) return "Map";
    if (node.IsArray()) return "Array";
    if (node.IsScalar()) return "Scalar";
    if (node.IsNull()) return "Null";
    return "Unknown";
}

int main() {
    using namespace plas::config;

    // --- 1. Load shared config ---
    std::printf("[*] Loading shared_config.yaml...\n");
    auto root = ConfigNode::LoadFromFile(FixturePath("shared_config.yaml"));
    if (root.IsError()) {
        std::printf("Error: %s\n", root.Error().message().c_str());
        return 1;
    }
    std::printf("    Root type: %s\n", NodeType(root.Value()));

    // --- 2. Navigate to plas.devices ---
    std::printf("\n[*] GetSubtree('plas.devices')...\n");
    auto devices = root.Value().GetSubtree("plas.devices");
    if (devices.IsError()) {
        std::printf("Error: %s\n", devices.Error().message().c_str());
        return 1;
    }
    std::printf("    plas.devices type: %s\n", NodeType(devices.Value()));

    // --- 3. Navigate to plas.settings ---
    std::printf("\n[*] GetSubtree('plas.settings')...\n");
    auto settings = root.Value().GetSubtree("plas.settings");
    if (settings.IsError()) {
        std::printf("Error: %s\n", settings.Error().message().c_str());
        return 1;
    }
    std::printf("    plas.settings type: %s\n", NodeType(settings.Value()));

    // --- 4. Navigate to libnvme.buses ---
    std::printf("\n[*] GetSubtree('libnvme.buses')...\n");
    auto buses = root.Value().GetSubtree("libnvme.buses");
    if (buses.IsError()) {
        std::printf("Error: %s\n", buses.Error().message().c_str());
        return 1;
    }
    std::printf("    libnvme.buses type: %s\n", NodeType(buses.Value()));

    // --- 5. Chained subtree (subtree of subtree) ---
    std::printf("\n[*] Chained: GetSubtree('plas') -> GetSubtree('devices')...\n");
    auto plas = root.Value().GetSubtree("plas");
    if (plas.IsOk()) {
        auto chained = plas.Value().GetSubtree("devices");
        if (chained.IsOk()) {
            std::printf("    Chained result type: %s\n",
                        NodeType(chained.Value()));
        }
    }

    // --- 6. Missing key ---
    std::printf("\n[*] GetSubtree('nonexistent')...\n");
    auto missing = root.Value().GetSubtree("nonexistent");
    std::printf("    Result: %s\n", missing.IsError() ? "error (expected)" : "ok");

    std::printf("[*] Done.\n");
    return 0;
}
