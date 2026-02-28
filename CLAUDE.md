# PLAS — Platform Library Across Systems

## Project Overview
C++17 library providing unified HAL (Hardware Abstraction Layer) interfaces (I2C, I3C, Serial, UART, Power Control, SSD GPIO, PCI Config/DOE/BAR, CXL DVSEC/Mailbox) with driver implementations for Aardvark, FT4222H, PMU3, PMU4, and PciUtils devices.

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
- `plas::hal::pci` — PCI/CXL domain types and interfaces (Bdf, PciAddress, PciConfig, PciDoe, PciBar, PciTopology, Cxl, CxlMailbox)
- `plas::hal::driver` — driver implementations (AardvarkDevice, Pmu3Device, PciUtilsDevice, etc.)
- `plas::configspec` — JSON Schema (draft-07) based config spec validation (SpecRegistry, Validator)
- `plas::bootstrap` — application initialization helper (Bootstrap class)

## CMake Targets
| Target | Dependencies | Private Deps |
|--------|-------------|-------------|
| `plas_core` | (none) | |
| `plas_log` | `plas_core` | spdlog |
| `plas_config` | `plas_core` | nlohmann_json, yaml-cpp |
| `plas_hal_interface` | `plas_core`, `plas_log`, `plas_config` | |
| `plas_hal_driver` | `plas_hal_interface`, `plas_config`, `plas_log` | Aardvark SDK, FT4222H SDK, PMU3 SDK, PMU4 SDK, libpci — all optional |
| `plas_configspec` | `plas_config` | nlohmann_json, json-schema-validator, yaml-cpp |
| `plas_bootstrap` | `plas_hal_driver`, `plas_configspec` | |

## Key Design Decisions
- **Error handling**: `std::error_code` + `Result<T>` (no exceptions)
- **Log backend**: Compile-time selection via pimpl (spdlog default)
- **Config parsers**: PRIVATE linked, no third-party types in public API
- **Multi-interface drivers**: Multiple inheritance from pure virtual ABCs
- **Capability check**: `dynamic_cast` on Device pointer
- **URI scheme**: `driver://bus:identifier` (aardvark: `aardvark://port:address`, ft4222h: `ft4222h://master_idx:slave_idx`, pciutils: `pciutils://DDDD:BB:DD.F`)
- **Optional drivers**: Conditional build via CMake Find modules; missing SDK → driver compiles as stub (`#ifdef PLAS_HAS_*`)
- **Vendor SDKs**: Bundled in `vendor/` directory (headers + prebuilt libs per platform/arch); searched first, then system paths

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
│   ├── plas-drivers/           ← driver implementations
│   │   ├── CMakeLists.txt
│   │   ├── include/plas/hal/driver/
│   │   └── src/hal/driver/
│   ├── plas-configspec/         ← JSON Schema config spec validation
│   │   ├── CMakeLists.txt
│   │   ├── include/plas/configspec/
│   │   ├── src/configspec/
│   │   └── schemas/            ← builtin *.schema.yaml files
│   └── plas-bootstrap/         ← application initialization helper
│       ├── CMakeLists.txt
│       ├── include/plas/bootstrap/
│       └── src/bootstrap/
├── vendor/                    ← proprietary SDK headers + prebuilt libs
│   ├── aardvark/
│   │   ├── include/           ← aardvark.h
│   │   ├── linux/{x86_64,aarch64}/
│   │   └── windows/x86_64/
│   ├── ft4222h/               ← (same layout)
│   ├── pmu3/
│   └── pmu4/
├── cmake/                     ← FindAardvark, FindFT4222H, FindPMU3, FindPMU4
├── tests/
├── examples/
│   ├── core/                  ← Properties session CRUD
│   ├── log/                   ← Logger init, macros, level control
│   ├── config/                ← Config load, ConfigNode tree, PropertyManager
│   │   └── fixtures/          ← JSON/YAML fixture files
│   ├── hal/                   ← DeviceManager, driver registration
│   ├── pci/                   ← DOE discovery & exchange, topology walk
│   └── master/                ← End-to-end Bootstrap demo
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
| `master/` | `master_example` | `plas::bootstrap` | End-to-end Bootstrap: config→devices→I2C+PCI I/O |

## Bootstrap (`plas::bootstrap`)
- **Class**: `Bootstrap` — single-call application initialization (replaces manual driver registration + config parsing + device lifecycle boilerplate)
- **Header**: `components/plas-bootstrap/include/plas/bootstrap/bootstrap.h`
- **Source**: `components/plas-bootstrap/src/bootstrap/bootstrap.cpp`
- **Target**: `plas_bootstrap` (PUBLIC dep: `plas::hal_driver`, `plas::configspec` — transitively includes hal_interface, config, log, core)
- **Namespace**: `plas::bootstrap`
- **Types**:
  - `BootstrapConfig` — device_config_path, device_config_key_path, device_config_node (optional ConfigNode), log_config, properties_config_path, auto_open_devices, skip_unknown_drivers, skip_device_failures, validation_mode (kLenient default), spec_dir
  - `DeviceFailure` — nickname, uri, driver, error, phase ("create"/"init"/"open"/"validate"), detail (human-readable context)
  - `BootstrapResult` — devices_opened, devices_failed, devices_skipped, failures vector
- **API**:
  - `static RegisterAllDrivers()` — registers all available drivers (hides `#ifdef PLAS_HAS_*` guards)
  - `static ValidateUri(uri) → bool` — validates `driver://bus:identifier` format
  - `Init(BootstrapConfig) → Result<BootstrapResult>` — full init sequence: register drivers → logger → properties → config parse → URI validate → device create/init/open
  - `Deinit()` — reverse teardown (idempotent, also called by destructor)
  - `GetDevice(nickname)`, `GetDeviceByUri(uri)` — device lookup by nickname or URI
  - `GetInterface<T>(nickname)` — single device interface cast
  - `GetDevicesByInterface<T>()` — returns `vector<pair<nickname, T*>>` of all devices supporting interface T
  - `DeviceNames()`, `GetFailures()` — query accessors
  - `DumpDevices() → string` — formatted summary of all devices (nickname, URI, driver, state, interfaces) and failures for debugging
- **Init sequence**: RegisterAllDrivers → Logger::Init → PropertyManager::LoadFromFile → Config::LoadFromNode or Config::LoadFromFile → **ConfigSpec validation (opt-in)** → per-device ValidateUri + DeviceFactory::CreateFromConfig + DeviceManager::AddDevice → per-device Init+Open
- **Config spec validation**: When `validation_mode` is not `kLenient`, registers builtin specs + loads `spec_dir`, then validates each DeviceEntry args against driver spec. `kWarning` logs and continues; `kStrict` with `skip_device_failures` skips device; `kStrict` without skip rolls back and fails
- **In-memory config**: Set `device_config_node` (ConfigNode) to skip file I/O; node takes precedence over `device_config_path` when both are set
- **URI validation**: Validates `driver://bus:identifier` format before device creation — catches malformed URIs at "create" phase with descriptive detail message
- **Graceful degradation**: `skip_unknown_drivers` skips unregistered drivers, `skip_device_failures` skips individual device failures — both report via `BootstrapResult::failures` with detail strings
- **Rollback**: Hard failure mid-init rolls back already-initialized subsystems in reverse order
- **Pimpl**: Implementation hidden behind `struct Impl` (same pattern as Logger)
- **Unit tests**: 69 tests in `tests/bootstrap/test_bootstrap.cpp`

## ConfigSpec (`plas::configspec`)
- **Purpose**: JSON Schema (draft-07) based validation for device config files and driver args — schema files can be dropped in without code changes
- **Headers**: `components/plas-configspec/include/plas/configspec/{validation_result.h, spec_registry.h, validator.h}`
- **Sources**: `components/plas-configspec/src/configspec/{spec_registry.cpp, validator.cpp, builtin_schemas.cpp.in}`
- **Target**: `plas_configspec` (alias: `plas::configspec`), PUBLIC dep: `plas::config`, PRIVATE dep: nlohmann_json, json-schema-validator, yaml-cpp
- **Namespace**: `plas::configspec`
- **Types**:
  - `Severity` enum — kError, kWarning
  - `ValidationIssue` — severity, path (JSON pointer), message
  - `ValidationResult` — valid flag, issues vector, Errors()/Warnings()/Summary() (header-only)
  - `ValidationMode` enum — kStrict (reject), kWarning (log+continue), kLenient (no-op)
- **SpecRegistry** (singleton):
  - `RegisterBuiltinSpecs()` — loads 6 builtin specs from CMake-generated C++ arrays (idempotent)
  - `RegisterDriverSpec(name, content, fmt)` / `RegisterDriverSpecFromFile()` — runtime registration
  - `RegisterConfigSpec(content, fmt)` — register whole-config schema
  - `HasDriverSpec(name)`, `HasConfigSpec()`, `RegisteredDrivers()` — query
  - `ExportDriverSpec(name) → Result<string>` — JSON dump for tooling
  - `LoadSpecsFromDirectory(dir) → Result<size_t>` — glob `*.schema.yaml`/`*.schema.json`/`*.spec.yaml`/`*.spec.json`
  - `Reset()` — clear all (for testing)
- **Validator**:
  - `ValidateConfigFile(path, fmt)`, `ValidateConfigString(content, fmt)`, `ValidateConfigNode(node)` — whole-config validation against config spec
  - `ValidateDeviceEntry(entry)` — validates `args` map against driver spec (string→typed JSON conversion)
  - kLenient mode returns valid immediately (no-op)
- **Builtin schemas** (6 files in `schemas/`): `device_config.schema.yaml`, `aardvark.schema.yaml`, `ft4222h.schema.yaml`, `pciutils.schema.yaml`, `pmu3.schema.yaml`, `pmu4.schema.yaml`
- **CMake code generation**: `file(GLOB schemas/*.schema.yaml)` → raw string literals in `builtin_specs.cpp` via `configure_file()`
- **YAML-to-JSON**: Replicated from plas-core's private `yaml_to_json.cpp` (~50 lines, intentional duplication to avoid dependency coupling)
- **Unit tests**: 43 tests — `test_validation_result.cpp` (6), `test_spec_registry.cpp` (16), `test_validator.cpp` (21)
- **Adding a new driver spec**: Drop `<driver>.schema.yaml` in `components/plas-configspec/schemas/` → `cmake -B build` → auto-embedded, no code changes

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

## Vendor SDK Management
- **Location**: `vendor/<sdk>/include/` (headers) + `vendor/<sdk>/<platform>/<arch>/` (prebuilt libs)
- **Platforms**: `linux/x86_64`, `linux/aarch64`, `windows/x86_64`
- **CMake Find modules**: `cmake/Find{Aardvark,FT4222H,PMU3,PMU4}.cmake`
- **Search order**: `vendor/` → `*_ROOT` CMake variable only (`NO_DEFAULT_PATH` — no env vars, no system paths)
- **Build options**: `PLAS_WITH_<SDK>=ON` (default), auto-detected; `PLAS_HAS_<SDK>=1` compile define when found
- **Behavior when absent**: Driver compiles as stub — lifecycle works, I/O returns `kNotSupported`

| SDK | Find Module | Header | Library | Define | Vendor files |
|-----|------------|--------|---------|--------|--------------|
| Aardvark | `FindAardvark` | `aardvark.h` | `libaardvark` | `PLAS_HAS_AARDVARK` | manual — login required at totalphase.com |
| FT4222H | `FindFT4222H` | `libft4222.h` | `libft4222` + `libftd2xx` | `PLAS_HAS_FT4222H` | bundled — FT4222H v1.4.4.232, D2XX v1.4.34 |
| PMU3 | `FindPMU3` | `pmu3.h` | `libpmu3` | `PLAS_HAS_PMU3` | manual |
| PMU4 | `FindPMU4` | `pmu4.h` | `libpmu4` | `PLAS_HAS_PMU4` | manual |

## Aardvark Driver (optional, requires Aardvark SDK)
- **Class**: `AardvarkDevice` — implements `Device`, `I2c`
- **Driver name**: `"aardvark"` (config: `driver: aardvark`)
- **URI**: `aardvark://port:address` (port: decimal 0–65535, address: 7-bit I2C 0x00–0x7F)
- **Build flag**: `PLAS_WITH_AARDVARK=ON` (default), auto-detected via `FindAardvark.cmake`
- **Compile define**: `PLAS_HAS_AARDVARK=1` when enabled
- **Config args**: `bitrate` (Hz, default 100000), `pullup` (true/false, default true), `bus_timeout_ms` (default 200)
- **State machine**: kUninitialized → Init (URI validation) → kInitialized → Open (aa_open + configure) → kOpen → Close (aa_close) → kClosed
- **I2C ops**: `aa_i2c_read`, `aa_i2c_write`, `aa_i2c_write_read` — mutex-serialized, length ≤ 0xFFFF; `stop=false` passes `AA_I2C_NO_STOP` flag (Repeated START support)
- **Error mapping**: SDK error codes → `core::ErrorCode` (kIOError, kTimeout, kNotSupported, kDataLoss)
- **Unit tests**: 35 tests in `test_aardvark_device.cpp` (always built, no SDK required)
- **Integration tests**: Gated by `PLAS_TEST_AARDVARK_PORT` env var (e.g., `0:0x50`)

## FT4222H Driver (optional, requires FT4222H + D2XX SDK)
- **Class**: `Ft4222hDevice` — implements `Device`, `I2c`
- **Driver name**: `"ft4222h"` (config: `driver: ft4222h`)
- **URI**: `ft4222h://master_idx:slave_idx` (decimal USB device indices, must differ)
- **Architecture**: Dual-chip master+slave — Master (TX) sends I2C commands, Slave (RX) receives responses via polling
- **Build flag**: `PLAS_WITH_FT4222H=ON` (default), auto-detected via `FindFT4222H.cmake`
- **Compile define**: `PLAS_HAS_FT4222H=1` when enabled
- **SDK dependency**: FT4222H SDK + D2XX (ftd2xx) — both searched by FindFT4222H.cmake
- **Config args**: `bitrate` (Hz, default 400000), `slave_addr` (7-bit, default 0x40), `sys_clock` (60/24/48/80 MHz, default 60), `rx_timeout_ms` (default 1000), `rx_poll_interval_us` (default 100)
- **State machine**: kUninitialized → Init (URI validation) → kInitialized → Open (FT_Open both + FT4222_SetClock + I2CMaster_Init + I2CSlave_Init + SetAddress, rollback on failure) → kOpen → Close (UnInitialize + FT_Close both) → kClosed
- **I2C ops**: Write via master (`FT4222_I2CMaster_WriteEx` with `START_AND_STOP` or `START` flag), Read via slave polling (`PollSlaveRx` + `FT4222_I2CSlave_Read`; `stop` param accepted but DUT-controlled), WriteRead = `WriteEx(START)` + slave poll + slave read — mutex-serialized, length ≤ 0xFFFF
- **PollSlaveRx**: Deadline-based polling of `FT4222_I2CSlave_GetRxStatus` with configurable timeout/interval
- **Error mapping**: `MapFtStatus(FT_STATUS)` + `MapFt4222Status(FT4222_STATUS)` → `core::ErrorCode`
- **Unit tests**: 35 tests in `test_ft4222h_device.cpp` (always built, no SDK required)
- **Integration tests**: Gated by `PLAS_TEST_FT4222H_PORT` env var (e.g., `0:1`)

## PciUtils Driver (optional, requires `libpci-dev`)
- **Class**: `PciUtilsDevice` — implements `Device`, `PciConfig`, `PciDoe`, `PciBar`
- **Driver name**: `"pciutils"` (config: `driver: pciutils`)
- **URI**: `pciutils://DDDD:BB:DD.F` (domain:bus:device.function)
- **Build flag**: `PLAS_WITH_PCIUTILS=ON` (default), auto-detected via `pkg_check_modules(libpci)`
- **Compile define**: `PLAS_HAS_PCIUTILS=1` when enabled
- **Config args**: `doe_timeout_ms` (default 1000), `doe_poll_interval_us` (default 100)
- **DOE**: Full mailbox handshake (Write→GO→Poll Ready→Read) with abort/error recovery
- **BAR MMIO**: Lazy mmap of sysfs `resourceN` files; cached per bar_index; `O_RDWR | O_SYNC | MAP_SHARED`; auto-unmapped on Close/destruction
- **Integration tests**: Gated by `PLAS_TEST_PCIUTILS_BDF` env var (e.g., `0000:03:00.0`)

## CXL Interface (header-only ABCs)
- **Headers**: `components/plas-core/include/plas/hal/interface/pci/cxl_types.h`, `cxl.h`, `cxl_mailbox.h`
- **Target**: `plas_hal_interface` (header-only, no new source files)
- **Types** (`cxl_types.h`):
  - `cxl::kCxlVendorId = 0x1E98`
  - `CxlDvsecId` enum — CXL DVSEC IDs (kCxlDevice, kRegisterLocator, kFlexBusPort, etc.)
  - `CxlDeviceType` enum — kType1, kType2, kType3, kUnknown
  - `DvsecHeader` struct — offset, vendor_id, dvsec_revision, dvsec_id, dvsec_length
  - `CxlRegisterBlockId` enum, `RegisterBlockEntry` struct — Register Locator DVSEC entries
  - `CxlMailboxOpcode` enum — Identify, GetHealthInfo, Sanitize, FW ops, etc.
  - `CxlMailboxReturnCode` enum — Success, InvalidInput, Unsupported, etc.
  - `CxlMailboxPayload` (`vector<uint8_t>`), `CxlMailboxResult` struct
  - IDE-KM types: `doe_type::kIdeKm`, `IdeKmMessageType` enum, `IdeStreamId` struct
- **Cxl ABC** (`cxl.h`): EnumerateCxlDvsecs, FindCxlDvsec, GetCxlDeviceType, GetRegisterBlocks, ReadDvsecRegister, WriteDvsecRegister
- **CxlMailbox ABC** (`cxl_mailbox.h`): ExecuteCommand (typed + raw opcode), GetPayloadSize, IsReady, GetBackgroundCmdStatus
- **Tests**: `test_cxl_types.cpp` (12), `test_cxl.cpp` (15), `test_cxl_mailbox.cpp` (12) — mock device pattern

## PciBar Interface (header-only ABC)
- **Header**: `components/plas-core/include/plas/hal/interface/pci/pci_bar.h`
- **Target**: `plas_hal_interface` (header-only, no source file)
- **API**: `BarRead32`, `BarRead64`, `BarWrite32`, `BarWrite64`, `BarReadBuffer`, `BarWriteBuffer`
- **Parameters**: `Bdf bdf, uint8_t bar_index (0–5), uint64_t offset`
- **Implementations**: `PciUtilsDevice` (sysfs resource mmap)
- **Tests**: `test_pci_bar.cpp` (13 tests) — mock device pattern

## I3c Interface (header-only ABC)
- **Header**: `components/plas-core/include/plas/hal/interface/i3c.h`
- **Target**: `plas_hal_interface` (header-only, no source file)
- **Namespace**: `plas::hal`
- **API**:
  - `Read(addr, data, len, stop=true)` / `Write(addr, data, len, stop=true)` — `stop=false` suppresses STOP condition for Repeated START sequences
  - `SendBroadcastCcc(ccc_id, data, len)` — broadcast CCC to all bus devices, no response (renamed from `SendCcc`)
  - `SendDirectCcc(ccc_id, addr, data, len)` — direct CCC SET to a specific device, no response
  - `RecvDirectCcc(ccc_id, addr, data, len)` — direct CCC GET from a specific device
  - `SetFrequency(freq)` — set I3C bus frequency
- **Note**: No driver implementation yet; interface defined for future driver authors

## Adding a New Driver
1. Create header in `components/plas-drivers/include/plas/hal/driver/<name>/<name>_device.h`
2. Inherit from `Device` + relevant interface ABCs
3. Implement in `components/plas-drivers/src/hal/driver/<name>/<name>_device.cpp` with `#ifdef PLAS_HAS_<NAME>` for SDK calls (stub fallback when SDK absent)
4. Add static `Register()` method that calls `DeviceFactory::RegisterDriver()`
5. Add source to `components/plas-drivers/CMakeLists.txt` (plas_hal_driver target)
6. If proprietary SDK required:
   - Create `cmake/Find<Name>.cmake` (search `vendor/` → `*_ROOT` CMake variable, `NO_DEFAULT_PATH`)
   - Add `vendor/<name>/{include, linux/x86_64, windows/x86_64}` directories
   - Add conditional block in `components/plas-drivers/CMakeLists.txt` (`PLAS_WITH_<NAME>` option + find + link + define)
7. Add `Register()` call to `Bootstrap::RegisterAllDrivers()` in `components/plas-bootstrap/src/bootstrap/bootstrap.cpp` (guarded by `#ifdef PLAS_HAS_<NAME>` if SDK-dependent)
8. Add `<name>.schema.yaml` in `components/plas-configspec/schemas/` for config args validation (auto-embedded at build time)
9. Add tests in `tests/hal/` (unit tests always built, integration tests gated by `PLAS_HAS_<NAME>`)
