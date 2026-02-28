# PLAS — Platform Library Across Systems

## Project Overview
C++17 library providing unified interfaces for hardware backends (I2C, I3C, Serial, UART, Power Control, SSD GPIO) with driver implementations for Aardvark, FT4222H, PMU3, and PMU4 devices.

## Build
```bash
mkdir build && cd build
cmake .. -DPLAS_BUILD_TESTS=ON
cmake --build . -j$(nproc)
ctest --output-on-failure
```

## Coding Conventions
- **Files/Folders**: `snake_case`
- **Classes/Functions**: `CamelCase`
- **Variables**: `snake_case`, member variables end with `_`
- **Include guard**: `#pragma once`
- **Standard**: C++17, no exceptions (use `Result<T>`)

## Namespace Structure
- `plas::core` — types, error codes, units, byte buffer
- `plas::log` — logger (spdlog backend, compile-time selection)
- `plas::config` — JSON/YAML config parsing
- `plas::backend` — device interfaces (I2c, PowerControl, SsdGpio, etc.)
- `plas::backend::pci` — PCI domain types and interfaces (Bdf, PciConfig, PciDoe)
- `plas::backend::driver` — driver implementations (AardvarkDevice, Pmu3Device, etc.)

## CMake Targets
| Target | Dependencies | Private Deps |
|--------|-------------|-------------|
| `plas_core` | (none) | |
| `plas_log` | `plas_core` | spdlog |
| `plas_config` | `plas_core` | nlohmann_json, yaml-cpp |
| `plas_backend_interface` | `plas_core`, `plas_log` | |
| `plas_backend_driver` | `plas_backend_interface`, `plas_config`, `plas_log` | |

## Key Design Decisions
- **Error handling**: `std::error_code` + `Result<T>` (no exceptions)
- **Log backend**: Compile-time selection via pimpl (spdlog default)
- **Config parsers**: PRIVATE linked, no third-party types in public API
- **Multi-interface drivers**: Multiple inheritance from pure virtual ABCs
- **Capability check**: `dynamic_cast` on Device pointer
- **URI scheme**: `driver://bus:identifier`

## Project Structure
```
plas/
├── components/
│   └── plas-core/              ← all 5 library targets
│       ├── CMakeLists.txt
│       ├── include/plas/       ← public headers
│       │   ├── core/
│       │   ├── log/
│       │   ├── config/
│       │   └── backend/
│       │       ├── interface/
│       │       │   └── pci/
│       │       └── driver/
│       └── src/
│           ├── core/
│           ├── log/
│           ├── config/
│           └── backend/
│               ├── interface/
│               └── driver/
├── cmake/
├── tests/
├── examples/
├── apps/
└── packaging/
```

## Adding a New Driver
1. Create header in `components/plas-core/include/plas/backend/driver/<name>/<name>_device.h`
2. Inherit from `Device` + relevant interface ABCs
3. Implement stub in `components/plas-core/src/backend/driver/<name>/<name>_device.cpp`
4. Add static `Register()` method that calls `DeviceFactory::RegisterDriver()`
5. Add source to `components/plas-core/CMakeLists.txt` (plas_backend_driver target)
6. Add tests in `tests/backend/`
