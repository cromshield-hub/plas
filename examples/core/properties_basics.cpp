/// @file properties_basics.cpp
/// @brief Example: Properties session CRUD and type conversion.
///
/// Demonstrates how to:
///  1. Create and access named sessions
///  2. Store and retrieve typed values (string, int, bool, double)
///  3. Update values by re-setting the same key
///  4. Convert between numeric types with GetAs<T>
///  5. Query keys, size, and session isolation

#include <cstdio>
#include <cstdint>
#include <string>

#include "plas/core/properties.h"

int main() {
    using plas::core::Properties;

    // --- 1. Create a session ---
    auto& session = Properties::GetSession("TestParam");
    std::printf("[*] Created session 'TestParam'\n");

    // --- 2. Set values of different types ---
    session.Set<std::string>("device_name", "aardvark0");
    session.Set<int64_t>("bitrate", 400000);
    session.Set<bool>("verbose", true);
    session.Set<double>("voltage", 3.3);

    std::printf("[*] Stored 4 key-value pairs\n");

    // --- 3. Read values back ---
    auto name = session.Get<std::string>("device_name");
    if (name.IsOk()) {
        std::printf("    device_name = %s\n", name.Value().c_str());
    }

    auto bitrate = session.Get<int64_t>("bitrate");
    if (bitrate.IsOk()) {
        std::printf("    bitrate     = %ld\n",
                    static_cast<long>(bitrate.Value()));
    }

    auto verbose = session.Get<bool>("verbose");
    if (verbose.IsOk()) {
        std::printf("    verbose     = %s\n",
                    verbose.Value() ? "true" : "false");
    }

    auto voltage = session.Get<double>("voltage");
    if (voltage.IsOk()) {
        std::printf("    voltage     = %.1f\n", voltage.Value());
    }

    // --- 4. Update an existing key ---
    session.Set<int64_t>("bitrate", 1000000);
    auto updated = session.Get<int64_t>("bitrate");
    if (updated.IsOk()) {
        std::printf("[*] Updated bitrate = %ld\n",
                    static_cast<long>(updated.Value()));
    }

    // --- 5. Type conversion with GetAs<T> ---
    //   bitrate was stored as int64_t; read it as uint32_t via safe cast
    auto as_u32 = session.GetAs<uint32_t>("bitrate");
    if (as_u32.IsOk()) {
        std::printf("[*] GetAs<uint32_t>(bitrate) = %u\n", as_u32.Value());
    }

    // --- 6. Query helpers ---
    std::printf("[*] Has 'bitrate'   : %s\n",
                session.Has("bitrate") ? "yes" : "no");
    std::printf("[*] Has 'missing'   : %s\n",
                session.Has("missing") ? "yes" : "no");
    std::printf("[*] Session size    : %zu\n", session.Size());

    std::printf("[*] Keys: ");
    for (const auto& key : session.Keys()) {
        std::printf("%s ", key.c_str());
    }
    std::printf("\n");

    // --- 7. Session isolation ---
    auto& other = Properties::GetSession("OtherSession");
    other.Set<int64_t>("counter", 42);

    std::printf("[*] OtherSession.counter = %ld\n",
                static_cast<long>(other.Get<int64_t>("counter").Value()));
    std::printf("[*] TestParam has 'counter': %s\n",
                session.Has("counter") ? "yes" : "no");

    // --- Cleanup ---
    Properties::DestroyAll();
    std::printf("[*] Done.\n");
    return 0;
}
