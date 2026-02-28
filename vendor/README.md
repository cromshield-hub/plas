# Vendor SDKs

This directory contains proprietary vendor SDK headers and prebuilt libraries
used by PLAS driver implementations. Each SDK is optional — if not present,
the corresponding driver builds as a stub (lifecycle works, I/O returns
`kNotSupported`).

## Directory Layout

```
vendor/
├── <sdk_name>/
│   ├── include/            ← SDK headers (aardvark.h, libft4222.h, etc.)
│   ├── linux/
│   │   ├── x86_64/         ← .so / .a for Linux x86_64
│   │   └── aarch64/        ← .so / .a for Linux aarch64
│   └── windows/
│       └── x86_64/         ← .dll / .lib for Windows x64
```

## SDK Reference

| Directory    | Vendor       | Headers              | Library            | Compile Define       |
|-------------|-------------|----------------------|--------------------|-----------------------|
| `aardvark/` | Total Phase | `aardvark.h`         | `libaardvark`      | `PLAS_HAS_AARDVARK`  |
| `ft4222h/`  | FTDI        | `ftd2xx.h`, `libft4222.h` | `libft4222`   | `PLAS_HAS_FT4222H`   |
| `pmu3/`     | (internal)  | `pmu3.h`             | `libpmu3`          | `PLAS_HAS_PMU3`      |
| `pmu4/`     | (internal)  | `pmu4.h`             | `libpmu4`          | `PLAS_HAS_PMU4`      |

## How to Add an SDK

1. Copy the SDK header(s) into `vendor/<sdk>/include/`
2. Copy the prebuilt library into the correct platform/arch directory
3. Re-run CMake — the Find module will auto-detect and enable the driver

Example for Aardvark on Linux x86_64:
```
vendor/aardvark/include/aardvark.h
vendor/aardvark/linux/x86_64/libaardvark.so
```

## CMake Override

You can also point to an external SDK location using CMake variables:
```bash
cmake -B build -DAARDVARK_ROOT=/opt/aardvark-sdk ...
cmake -B build -DFT4222H_ROOT=/opt/ft4222h-sdk ...
```

Or via environment variables:
```bash
export AARDVARK_ROOT=/opt/aardvark-sdk
cmake -B build ...
```

The vendor/ directory is checked first, then the external hint, then system paths.

## Build Status Messages

During CMake configuration you will see status for each SDK:
```
-- Aardvark driver: enabled (SDK found)          # SDK present
-- Aardvark driver: stub only (SDK not found)     # SDK absent, stub built
-- FT4222H driver: stub only (SDK not found)
-- PMU3 driver: stub only (SDK not found)
-- PMU4 driver: stub only (SDK not found)
```
