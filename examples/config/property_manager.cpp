/// @file property_manager.cpp
/// @brief Example: PropertyManager for loading config files into sessions.
///
/// Demonstrates how to:
///  1. Load a YAML file as a single named session
///  2. Read typed values from the session
///  3. Add/update values at runtime via Set
///  4. Load a YAML file in multi-session mode

#include <cstdio>
#include <cstdint>
#include <string>

#include "plas/config/property_manager.h"
#include "plas/core/properties.h"

static std::string FixturePath(const char* name) {
    return std::string("fixtures/") + name;
}

int main() {
    using namespace plas::config;
    using plas::core::Properties;

    auto& pm = PropertyManager::GetInstance();

    // --- 1. Single-session mode ---
    std::printf("[*] Loading test_params.yaml as session 'TestParam'...\n");
    auto result = pm.LoadFromFile(FixturePath("test_params.yaml"), "TestParam");
    if (result.IsError()) {
        std::printf("Error: %s\n", result.Error().message().c_str());
        return 1;
    }

    // --- 2. Read values from session ---
    auto& session = pm.Session("TestParam");

    auto bitrate = session.Get<int64_t>("bitrate");
    if (bitrate.IsOk()) {
        std::printf("    bitrate    = %ld\n",
                    static_cast<long>(bitrate.Value()));
    }

    auto address = session.Get<int64_t>("address");
    if (address.IsOk()) {
        std::printf("    address    = %ld\n",
                    static_cast<long>(address.Value()));
    }

    auto ratio = session.Get<double>("ratio");
    if (ratio.IsOk()) {
        std::printf("    ratio      = %.2f\n", ratio.Value());
    }

    // Nested keys are flattened with dot notation
    auto retry = session.Get<int64_t>("advanced.retry_count");
    if (retry.IsOk()) {
        std::printf("    advanced.retry_count = %ld\n",
                    static_cast<long>(retry.Value()));
    }

    auto verbose = session.Get<bool>("advanced.verbose");
    if (verbose.IsOk()) {
        std::printf("    advanced.verbose     = %s\n",
                    verbose.Value() ? "true" : "false");
    }

    // --- 3. Runtime update ---
    std::printf("\n[*] Runtime: Set timeout_ms = 5000\n");
    session.Set<int64_t>("timeout_ms", 5000);
    auto updated = session.Get<int64_t>("timeout_ms");
    if (updated.IsOk()) {
        std::printf("    timeout_ms = %ld\n",
                    static_cast<long>(updated.Value()));
    }

    // --- 4. Multi-session mode ---
    std::printf("\n[*] Loading shared_config.yaml in multi-session mode...\n");
    auto multi = pm.LoadFromFile(FixturePath("shared_config.yaml"));
    if (multi.IsError()) {
        std::printf("Error: %s\n", multi.Error().message().c_str());
        return 1;
    }

    std::printf("    Sessions: ");
    for (const auto& name : pm.SessionNames()) {
        std::printf("%s ", name.c_str());
    }
    std::printf("\n");

    if (pm.HasSession("plas")) {
        auto& plas_session = pm.Session("plas");
        auto timeout = plas_session.Get<int64_t>("settings.timeout_ms");
        if (timeout.IsOk()) {
            std::printf("    plas.settings.timeout_ms = %ld\n",
                        static_cast<long>(timeout.Value()));
        }
    }

    // --- Cleanup ---
    pm.Reset();
    Properties::DestroyAll();
    std::printf("[*] Done.\n");
    return 0;
}
