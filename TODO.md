# PLAS — TODO & Backlog

> 협업용 요구사항/진행현황 문서. 신규 요구사항은 `## Backlog`에 추가.

---

## Project Goal

C++17 기반 **하드웨어 백엔드 통합 라이브러리**. I2C, I3C, Serial, UART, Power Control, SSD GPIO, PCI 등 다양한 버스/프로토콜에 대해 **인터페이스(ABC)** 를 정의하고, 디바이스 드라이버가 이를 구현하는 구조.

**핵심 원칙:**
- 예외 없음 — `Result<T>` 기반 에러 처리
- 서드파티 타입 비노출 — PRIVATE 링크 (spdlog, nlohmann_json, yaml-cpp)
- 런타임 인터페이스 확인 — `dynamic_cast` on `Device*`
- 자기 등록 팩토리 — `DeviceFactory::RegisterDriver()`

---

## Current Status

### Infrastructure (완료)

| 모듈 | 상태 | 비고 |
|------|------|------|
| `plas_core` (types, error, result, units, byte_buffer, version, properties) | **Done** | Properties: 세션 기반 key-value 스토어, SafeNumericCast |
| `plas_log` (logger, spdlog backend, pimpl) | **Done** | 싱글톤, PLAS_LOG_* 매크로 |
| `plas_config` (JSON/YAML auto-detect, ConfigNode, PropertyManager) | **Done** | 확장자 기반 포맷 감지, ConfigNode 트리 탐색, config→Properties 세션 매핑 |
| `plas_hal_interface` (Device, DeviceFactory, DeviceManager) | **Done** | ABC + 팩토리 패턴 + config→인스턴스 레지스트리, grouped config 지원 |
| CMake / FetchContent / install | **Done** | PlasInstall.cmake 포함 |
| Monorepo 구조 (`components/`) | **Done** | plas-core (인프라+인터페이스), plas-drivers (드라이버), plas-bootstrap (초기화 헬퍼) |
| Vendor SDK 동봉 (`vendor/`) | **Done** | 플랫폼/아키텍처별 SDK 번들링, CMake Find 모듈 자동 탐색 |
| `plas_bootstrap` (Bootstrap 초기화 헬퍼) | **Done** | 단일 Init/Deinit으로 드라이버 등록→설정→디바이스 생성/열기 자동화, graceful degradation |

### Interfaces (ABC 정의)

| 인터페이스 | 헤더 | 상태 |
|------------|------|------|
| I2c | `interface/i2c.h` | **Done** |
| I3c | `interface/i3c.h` | **Done** |
| Serial | `interface/serial.h` | **Done** |
| Uart | `interface/uart.h` | **Done** |
| PowerControl | `interface/power_control.h` | **Done** |
| SsdGpio | `interface/ssd_gpio.h` | **Done** |
| PciConfig | `interface/pci/pci_config.h` | **Done** |
| PciDoe | `interface/pci/pci_doe.h` | **Done** |
| PCI types (Bdf, CapabilityId, etc.) | `interface/pci/types.h` | **Done** |

### Drivers (구현체)

| 드라이버 | 구현 인터페이스 | 상태 |
|----------|----------------|------|
| AardvarkDevice | Device, I2c | **Done** — SDK 있으면 실제 I2C, 없으면 stub (33 unit + 6 integration tests) |
| Ft4222hDevice | Device, I2c | **Done** — SDK 있으면 실제 I2C master+slave, 없으면 stub (33 unit + 7 integration tests) |
| PciUtilsDevice | Device, PciConfig, PciDoe | **Done** — libpci 있으면 실제 PCI config R/W + DOE, 없으면 빌드 제외 (14 unit + integration tests) |
| Pmu3Device | Device, PowerControl, SsdGpio | **Stub** — lifecycle만 동작, 기능 메서드 kNotSupported |
| Pmu4Device | Device, PowerControl, SsdGpio | **Stub** — lifecycle만 동작, 기능 메서드 kNotSupported |

### Examples

| 카테고리 | 예제 | 비고 |
|----------|------|------|
| `examples/core/` | `properties_basics` | 세션 CRUD, 타입 변환, 세션 격리 |
| `examples/log/` | `logger_basics` | LogConfig 초기화, 매크로, 런타임 레벨 변경 |
| `examples/config/` | `config_load` | flat JSON, grouped YAML, LoadFromNode, FindDevice |
| `examples/config/` | `config_node_tree` | 서브트리 탐색, 타입 쿼리, 체이닝 |
| `examples/config/` | `property_manager` | single/multi-session 로드, 런타임 업데이트 |
| `examples/hal/` | `device_manager` | 드라이버 등록, 인터페이스 캐스팅, lifecycle |
| `examples/pci/` | `doe_exchange` | PCI DOE discovery + data exchange (stub) |
| `examples/pci/` | `topology_walk` | sysfs PCI 토폴로지 탐색 (실제 하드웨어) |
| `examples/master/` | `master_example` | Bootstrap 기반 end-to-end 데모 (config→devices→I2C+PCI) |

### Tests

| 카테고리 | 테스트 수 | 상태 |
|----------|----------|------|
| Core (error, result, units) | 25 | **Pass** |
| Core (byte_buffer) | 17 | **Pass** |
| Core (version) | 8 | **Pass** |
| Core (properties) | 44 | **Pass** |
| Log | 4 | **Pass** |
| Config (JSON, YAML) | 19 | **Pass** |
| Config (ConfigNode) | 12 | **Pass** |
| Config (PropertyManager) | 31 | **Pass** |
| Config (errors) | 21 | **Pass** |
| HAL (device_factory) | 11 | **Pass** |
| HAL (device_manager) | 30 | **Pass** |
| HAL (device_lifecycle) | 14 | **Pass** |
| PCI (types, config, doe) | 47 | **Pass** |
| PCI (topology) | 17 | **Pass** |
| PCI (topology integration) | 6 | **Skip** (env-gated) |
| CXL (types, cxl, mailbox) | 40 | **Pass** |
| HAL (aardvark unit) | 31 | **Pass** |
| HAL (aardvark integration) | — | **Skip** (env-gated, `PLAS_TEST_AARDVARK_PORT`) |
| HAL (ft4222h unit) | 31 | **Pass** |
| HAL (ft4222h integration) | — | **Skip** (env-gated, `PLAS_TEST_FT4222H_PORT`) |
| HAL (pciutils unit) | — | **Skip** (libpci 필요) |
| HAL (pciutils integration) | — | **Skip** (env-gated, `PLAS_TEST_PCIUTILS_BDF`) |
| Bootstrap | 64 | **Pass** |
| **합계** | **466** | **All Pass** (466 run, integration skipped) |

---

## Backlog

> 우선순위: P0 (즉시) > P1 (다음) > P2 (추후) > P3 (아이디어)

### P1 — 런타임 레지스트리

- [x] `DeviceManager` — Config→Device 인스턴스 레지스트리 (2026-02-28)
  - Meyer's singleton, LoadFromConfig/LoadFromEntries, nickname 기반 GetDevice/GetInterface<T>
  - `plas_hal_interface`에 배치, `plas::config` PUBLIC 의존 추가
  - 20개 테스트 (싱글턴, 로드, 조회, 인터페이스 캐스팅, 에러, Reset, lifecycle)
- [x] `ConfigNode` + Grouped Device Config (2026-02-28)
  - ConfigNode: pimpl 기반 generic config tree, `plas_config` 타겟에 배치
  - dot-separated key path 탐색 (`GetSubtree("plas.devices")`) — 공유 config에서 서브트리 추출
  - Grouped device format: map key = driver name, auto-nickname `{driver}_{index}`, scalar args → string 변환
  - Config 확장: `LoadFromNode(ConfigNode)`, `LoadFromFile(path, key_path)`
  - DeviceManager 확장: `LoadFromTree(ConfigNode)`, `LoadFromConfig(path, key_path)`
  - YAML→JSON 내부 변환, 통합 device_parser (flat+grouped)
  - 25개 신규 테스트 (ConfigNode 11, Config grouped 9, DeviceManager tree/key_path 4, error 1)

### P1 — PCI/CXL 확장

- [x] `interface/pci/types.h` — Bdf, ConfigOffset, CapabilityId, ExtCapabilityId, DoeProtocolId, DoePayload (2026-02-28)
- [x] `interface/pci/pci_config.h` — PciConfig ABC: ReadConfig8/16/32, WriteConfig8/16/32, FindCapability, FindExtCapability (2026-02-28)
- [x] `interface/pci/pci_doe.h` — PciDoe ABC: DoeDiscover, DoeExchange (2026-02-28)
- [x] PCI 테스트 35개 (types, config, doe) (2026-02-28)
- [x] `PciTopology` — sysfs 기반 PCI 토폴로지 탐색 및 Remove/Rescan (2026-02-28)
  - `PciAddress` (domain + Bdf), `PciePortType` enum, `PciDeviceNode` struct
  - `PciTopology` static class: FindParent, FindRootPort, GetPathToRoot, RemoveDevice, RescanBridge, RescanAll
  - sysfs symlink → realpath 파싱으로 토폴로지 추출, config binary에서 PCIe cap 읽기
  - `SetSysfsRoot()` 로 fake sysfs 기반 unit test 22개 + integration test 6개 (env-gated)
  - `topology_walk` 예제 추가
- [x] `interface/pci/cxl_types.h`, `cxl.h`, `cxl_mailbox.h` — CXL 인터페이스 ABC (2026-02-28)
  - CXL DVSEC types/enums, Cxl ABC (6 methods), CxlMailbox ABC (5 methods)
  - IDE-KM types (타입만, ABC 없음), 40 tests (types 13 + cxl 15 + mailbox 12)
  - 추후 use case 확보 시 driver 구현체 마무리
- [x] PciUtils PCI 드라이버 구현 (2026-02-28)
  - `pciutils://DDDD:BB:DD.F`, libpci 기반 PCI config R/W + DOE mailbox 전체 핸드셰이크
  - 14 unit tests + integration tests (PLAS_TEST_PCIUTILS_BDF env-gated)

> **Note**: MCTP over PCIe VDM (`mctp.h`) — sideband transport 결정 보류. CXL 2.0은 DOE/mailbox 기반, MCTP는 별도 sideband (SMBus/I3C/PCIe VDM) 선택 후 진행.

### P1 — 앱 초기화

- [x] `Bootstrap` 클래스 (`plas_bootstrap` 컴포넌트) (2026-02-28)
  - `BootstrapConfig`로 설정, 단일 `Init()` 호출로 드라이버 등록→로거→프로퍼티→config 파싱→디바이스 생성/init/open 자동화
  - `RegisterAllDrivers()` — `#ifdef` 가드 내부 캡슐화
  - Graceful degradation: `skip_unknown_drivers`, `skip_device_failures` → `BootstrapResult`에 실패 정보 보고
  - 실패 시 롤백, `Deinit()` idempotent (소멸자에서 자동 호출)
  - `DeviceManager::AddDevice()` 메서드 추가 (개별 디바이스 추가 지원)
  - Pimpl 패턴, move-only, RAII
  - 40 tests, `master_example` 리팩토링 (349→213줄)
- [x] Bootstrap DX 개선 — 5가지 Quick Win (2026-02-28)
  - `GetDeviceByUri(uri)` — URI 기반 디바이스 검색 (DeviceManager + Bootstrap)
  - `GetDevicesByInterface<T>()` — 인터페이스 타입별 디바이스 필터링 (`vector<pair<nickname, T*>>`)
  - `DumpDevices()` — 로드된 디바이스 설정 덤프 (nickname, URI, driver, state, 지원 인터페이스)
  - URI 포맷 검증 — Init 시 `driver://bus:identifier` 포맷 조기 검증, 상세 에러 메시지
  - `DeviceFailure::detail` — 드라이버별 상세 에러 컨텍스트 (generic error code 보완)
  - DeviceManager에도 `GetDeviceByUri()`, `GetDevicesByInterface<T>()` 추가
  - 24 신규 테스트 (Bootstrap 18 + DeviceManager 6), 총 466 테스트

### P1 — Examples

- [x] 카테고리별 예제 정리 (2026-02-28)
  - `examples/` 하위 core, log, config, hal, pci, master 폴더로 분류
  - 기존 doe_exchange.cpp → `examples/pci/`로 이동
  - 모듈별 8개 예제 + 4개 fixture 파일 추가
- [x] `master_example` Bootstrap 리팩토링 (2026-02-28)
  - 기존 수동 드라이버 등록/config 파싱/lifecycle 코드를 Bootstrap::Init() 한 줄로 대체
  - 349줄 → 213줄로 축소, I2C+PCI config+topology 데모 유지

### P2 — 드라이버 실제 구현

- [x] Aardvark I2C 실제 구현 (2026-02-28)
  - URI 파싱 (`aardvark://port:address`), 엄격한 state machine, config args (bitrate/pullup/bus_timeout_ms)
  - `#ifdef PLAS_HAS_AARDVARK` 조건부 SDK 호출 (aa_open/aa_i2c_read/write/write_read), 에러 매핑
  - 33 unit tests (SDK 없이 동작) + 6 integration tests (PLAS_TEST_AARDVARK_PORT env-gated)
- [x] Vendor SDK 동봉 인프라 구축 (2026-02-28)
  - `vendor/<sdk>/{include, linux/{x86_64,aarch64}, windows/x86_64}` 디렉토리 구조
  - `cmake/Find{Aardvark,FT4222H,PMU3,PMU4}.cmake` — vendor/ 우선 탐색 → *_ROOT → 시스템
  - `PLAS_WITH_<SDK>` 옵션 + `PLAS_HAS_<SDK>` 컴파일 정의, 모든 드라이버 항상 빌드 (SDK 조건부 링크)
- [x] FT4222H I2C 실제 구현 (2026-02-28)
  - Dual-chip master+slave 아키텍처 (`ft4222h://master_idx:slave_idx`), FT4222H SDK + D2XX 의존
  - Master (TX) I2CMaster_Write, Slave (RX) polling-based read, 설정 args (bitrate/slave_addr/sys_clock/rx_timeout_ms/rx_poll_interval_us)
  - 33 unit tests + 7 integration tests (PLAS_TEST_FT4222H_PORT env-gated)
- [ ] PMU3 PowerControl/SsdGpio 실제 구현
- [ ] PMU4 PowerControl/SsdGpio 실제 구현
- [ ] I3C/Serial/UART 드라이버 (대상 하드웨어 미정)

### P2 — 테스트 보강

- [x] ByteBuffer 단위 테스트 (2026-02-28) — 17 tests
- [x] Version 단위 테스트 (2026-02-28) — 8 tests
- [x] Device lifecycle 에러 케이스 (2026-02-28) — 16 tests (이중 Open, 미초기화 Open, 이중 Close, Reset, 풀사이클 등)
- [x] Config 에러 케이스 (2026-02-28) — 21 tests (malformed JSON/YAML, missing fields, key path errors)

### P3 — 아이디어

- [ ] 비동기 I/O (std::future 또는 callback)
- [ ] Device hot-plug 이벤트
- [ ] CLI 도구 (디바이스 스캔, config 검증)
- [ ] Python 바인딩 (pybind11)
- [ ] CI/CD 파이프라인 (GitHub Actions)

---

## How to Add Requirements

이 파일의 `## Backlog` 섹션에 아래 형식으로 추가:

```markdown
### P1 — 카테고리명

- [ ] 요구사항 제목
  - 상세 설명, 제약 조건, 참고 사항
```

완료 시 `- [ ]` → `- [x]`로 변경하고 날짜 기록:

```markdown
- [x] 요구사항 제목 (2026-02-28)
```

### 사용 시나리오 ###
최종 build는: Testcase (app) -> libnvme (inhouse library, nvme 제어용) -> plas
실행시 ./app config.json
config.json에는 아래 정보가 있지
 - nvme의 bus 정보 (여러개 연결 가능)
 - hw 정보와 uri 등 (각자 여러개 달릴 수 있지)

프로그래이 실행되면 config.json의 내용을 바탕으로 어디선가 nvme와 hw, 사용 가능한 인터페이스 등의 정보를 가지고 있고, libnvme나 testcase가 접근할 수 있어야하고, 거기서 정보를 바탕으로 interface를 열어서 사용하는거지.