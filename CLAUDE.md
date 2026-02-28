# PLAS — Platform Library Across Systems

## Project Overview
C++17 library providing unified HAL (Hardware Abstraction Layer) interfaces (I2C, I3C, Serial, UART, Power Control, SSD GPIO) with driver implementations for Aardvark, FT4222H, PMU3, and PMU4 devices.

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
- `plas::hal` — device interfaces (I2c, PowerControl, SsdGpio, etc.)
- `plas::hal::pci` — PCI domain types and interfaces (Bdf, PciConfig, PciDoe)
- `plas::hal::driver` — driver implementations (AardvarkDevice, Pmu3Device, etc.)

## CMake Targets
| Target | Dependencies | Private Deps |
|--------|-------------|-------------|
| `plas_core` | (none) | |
| `plas_log` | `plas_core` | spdlog |
| `plas_config` | `plas_core` | nlohmann_json, yaml-cpp |
| `plas_hal_interface` | `plas_core`, `plas_log` | |
| `plas_hal_driver` | `plas_hal_interface`, `plas_config`, `plas_log` | |

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
│   ├── plas-core/              ← core library targets
│   │   ├── CMakeLists.txt
│   │   ├── include/plas/       ← public headers
│   │   │   ├── core/
│   │   │   ├── log/
│   │   │   ├── config/
│   │   │   └── hal/
│   │   │       └── interface/
│   │   │           └── pci/
│   │   └── src/
│   │       ├── core/
│   │       ├── log/
│   │       ├── config/
│   │       └── hal/
│   │           └── interface/
│   └── plas-drivers/           ← driver implementations
│       ├── CMakeLists.txt
│       ├── include/plas/hal/driver/
│       └── src/hal/driver/
├── cmake/
├── tests/
├── examples/
├── apps/
└── packaging/
```

## Adding a New Driver
1. Create header in `components/plas-drivers/include/plas/hal/driver/<name>/<name>_device.h`
2. Inherit from `Device` + relevant interface ABCs
3. Implement stub in `components/plas-drivers/src/hal/driver/<name>/<name>_device.cpp`
4. Add static `Register()` method that calls `DeviceFactory::RegisterDriver()`
5. Add source to `components/plas-drivers/CMakeLists.txt` (plas_hal_driver target)
6. Add tests in `tests/hal/`
