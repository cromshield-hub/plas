# PLAS — Platform Library Across Systems

## Project Overview
C++17 library providing unified HAL (Hardware Abstraction Layer) interfaces (I2C, I3C, Serial, UART, Power Control, SSD GPIO, PCI Config/DOE) with driver implementations for Aardvark, FT4222H, PMU3, PMU4, and PciUtils devices.

## Build
```bash
cmake -B build -DPLAS_BUILD_TESTS=ON -DPLAS_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

## Coding Conventions
- **Files/Folders**: `snake_case`
- **Classes/Functions**: `CamelCase`
- **Variables**: `snake_case`, member variables end with `_`
- **Include guard**: `#pragma once`
- **Standard**: C++17, no exceptions (use `Result<T>`)

## Namespace Structure
- `plas::core` — types, error codes, units, byte buffer, Properties (session key-value store)
- `plas::log` — logger (spdlog backend, compile-time selection)
- `plas::config` — JSON/YAML config parsing, PropertyManager (config→Properties session mapping)
- `plas::hal` — device interfaces (I2c, PowerControl, SsdGpio, etc.)
- `plas::hal::pci` — PCI domain types and interfaces (Bdf, PciAddress, PciConfig, PciDoe, PciTopology)
- `plas::hal::driver` — driver implementations (AardvarkDevice, Pmu3Device, PciUtilsDevice, etc.)

## CMake Targets
| Target | Dependencies | Private Deps |
|--------|-------------|-------------|
| `plas_core` | (none) | |
| `plas_log` | `plas_core` | spdlog |
| `plas_config` | `plas_core` | nlohmann_json, yaml-cpp |
| `plas_hal_interface` | `plas_core`, `plas_log`, `plas_config` | |
| `plas_hal_driver` | `plas_hal_interface`, `plas_config`, `plas_log` | libpci (optional, for pciutils driver) |

## Key Design Decisions
- **Error handling**: `std::error_code` + `Result<T>` (no exceptions)
- **Log backend**: Compile-time selection via pimpl (spdlog default)
- **Config parsers**: PRIVATE linked, no third-party types in public API
- **Multi-interface drivers**: Multiple inheritance from pure virtual ABCs
- **Capability check**: `dynamic_cast` on Device pointer
- **URI scheme**: `driver://bus:identifier` (pciutils: `pciutils://DDDD:BB:DD.F`)
- **Optional drivers**: Conditional build via `pkg_check_modules`; missing system libs → driver skipped

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
│   ├── core/                  ← Properties session CRUD
│   ├── log/                   ← Logger init, macros, level control
│   ├── config/                ← Config load, ConfigNode tree, PropertyManager
│   │   └── fixtures/          ← JSON/YAML fixture files
│   ├── hal/                   ← DeviceManager, driver registration
│   └── pci/                   ← DOE discovery & exchange, topology walk
├── apps/
└── packaging/
```

## Examples (`-DPLAS_BUILD_EXAMPLES=ON`)
| Category | Example | Target | Description |
|----------|---------|--------|-------------|
| `core/` | `properties_basics` | `plas::core` | Session CRUD, typed Get/Set, GetAs conversion |
| `log/` | `logger_basics` | `plas::log` | LogConfig, PLAS_LOG_* macros, runtime SetLevel |
| `config/` | `config_load` | `plas::config` | Flat JSON, grouped YAML, LoadFromNode, FindDevice |
| `config/` | `config_node_tree` | `plas::config` | Subtree navigation, type queries, chaining |
| `config/` | `property_manager` | `plas::config` | Single/multi-session load, runtime update |
| `hal/` | `device_manager` | `plas::hal_interface`, `plas::hal_driver` | Driver registration, interface casting, lifecycle |
| `pci/` | `doe_exchange` | `plas::hal_interface` | PCI DOE discovery + data exchange (stub device) |
| `pci/` | `topology_walk` | `plas::hal_interface` | sysfs PCI topology traversal (real hardware) |

## PCI Topology (sysfs-based)
- **Class**: `PciTopology` — static utility class for PCI topology traversal and device management
- **Header**: `components/plas-core/include/plas/hal/interface/pci/pci_topology.h`
- **Source**: `components/plas-core/src/hal/interface/pci/pci_topology.cpp`
- **Target**: `plas_hal_interface` (no extra dependencies — pure sysfs file I/O)
- **New types** (in `types.h`):
  - `PciAddress{uint16_t domain; Bdf bdf;}` — full PCI address with `ToString()`/`FromString()`
  - `PciePortType` enum — endpoint, root port, upstream/downstream port, bridges, etc.
  - `PciDeviceNode` struct — address + port type + bridge flag + sysfs path
- **API**:
  - `GetSysfsPath`, `DeviceExists` — sysfs path resolution
  - `GetDeviceInfo` — read port type and bridge flag from config space binary
  - `FindParent` — resolve symlink via `realpath()` and parse topology path segments
  - `FindRootPort` — walk up to find root port
  - `GetPathToRoot` — full path from device to root (device first, root last)
  - `RemoveDevice` / `RescanBridge` / `RescanAll` — sysfs remove/rescan writes
  - `SetSysfsRoot` — override for unit testing with fake sysfs
- **Config space parsing**: reads binary `/sys/bus/pci/devices/DDDD:BB:DD.F/config` for header type (offset 0x0E) and PCIe capability chain walking (cap ID 0x10, port type bits [7:4])
- **Unit tests**: 22 tests with fake sysfs directory structure (`test_pci_topology.cpp`)
- **Integration tests**: Gated by `PLAS_TEST_PCI_TOPOLOGY_BDF` env var (`test_pci_topology_integration.cpp`)

## PciUtils Driver (optional, requires `libpci-dev`)
- **Class**: `PciUtilsDevice` — implements `Device`, `PciConfig`, `PciDoe`
- **Driver name**: `"pciutils"` (config: `driver: pciutils`)
- **URI**: `pciutils://DDDD:BB:DD.F` (domain:bus:device.function)
- **Build flag**: `PLAS_WITH_PCIUTILS=ON` (default), auto-detected via `pkg_check_modules(libpci)`
- **Compile define**: `PLAS_HAS_PCIUTILS=1` when enabled
- **Config args**: `doe_timeout_ms` (default 1000), `doe_poll_interval_us` (default 100)
- **DOE**: Full mailbox handshake (Write→GO→Poll Ready→Read) with abort/error recovery
- **Integration tests**: Gated by `PLAS_TEST_PCIUTILS_BDF` env var (e.g., `0000:03:00.0`)

## Adding a New Driver
1. Create header in `components/plas-drivers/include/plas/hal/driver/<name>/<name>_device.h`
2. Inherit from `Device` + relevant interface ABCs
3. Implement stub in `components/plas-drivers/src/hal/driver/<name>/<name>_device.cpp`
4. Add static `Register()` method that calls `DeviceFactory::RegisterDriver()`
5. Add source to `components/plas-drivers/CMakeLists.txt` (plas_hal_driver target)
6. Add tests in `tests/hal/`
