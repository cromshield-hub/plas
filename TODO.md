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
| `plas_config` (JSON/YAML auto-detect, PropertyManager) | **Done** | 확장자 기반 포맷 감지, config→Properties 세션 매핑 |
| `plas_hal_interface` (Device, DeviceFactory) | **Done** | ABC + 팩토리 패턴 |
| CMake / FetchContent / install | **Done** | PlasInstall.cmake 포함 |
| Monorepo 구조 (`components/`) | **Done** | plas-core (인프라+인터페이스), plas-drivers (드라이버) |

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
| AardvarkDevice | Device, I2c | **Stub** — lifecycle만 동작, I2C R/W는 kNotSupported |
| Ft4222hDevice | Device, I2c | **Stub** — lifecycle만 동작, I2C R/W는 kNotSupported |
| Pmu3Device | Device, PowerControl, SsdGpio | **Stub** — lifecycle만 동작, 기능 메서드 kNotSupported |
| Pmu4Device | Device, PowerControl, SsdGpio | **Stub** — lifecycle만 동작, 기능 메서드 kNotSupported |

### Tests

| 카테고리 | 테스트 수 | 상태 |
|----------|----------|------|
| Core (error, result, units) | 25 | **Pass** |
| Core (properties) | 44 | **Pass** |
| Log | 4 | **Pass** |
| Config (JSON, YAML) | 10 | **Pass** |
| Config (PropertyManager) | 31 | **Pass** |
| HAL (device_factory) | 7 | **Pass** |
| PCI (types, config, doe) | 35 | **Pass** |
| **합계** | **156** | **All Pass** |

---

## Backlog

> 우선순위: P0 (즉시) > P1 (다음) > P2 (추후) > P3 (아이디어)

### P1 — PCI/CXL 확장

- [x] `interface/pci/types.h` — Bdf, ConfigOffset, CapabilityId, ExtCapabilityId, DoeProtocolId, DoePayload (2026-02-28)
- [x] `interface/pci/pci_config.h` — PciConfig ABC: ReadConfig8/16/32, WriteConfig8/16/32, FindCapability, FindExtCapability (2026-02-28)
- [x] `interface/pci/pci_doe.h` — PciDoe ABC: DoeDiscover, DoeExchange (2026-02-28)
- [x] PCI 테스트 35개 (types, config, doe) (2026-02-28)
- [ ] `interface/pci/cxl.h` — CXL 인터페이스 ABC (CXL IDE-KM, CXL.mem 등)
  - PciDoe를 composition으로 사용, DVSEC 기반 식별
- [ ] `interface/pci/mctp.h` — MCTP over PCIe VDM 인터페이스 ABC
- [ ] PCI 드라이버 구현체 (대상 하드웨어 미정)

### P2 — 드라이버 실제 구현

- [ ] Aardvark I2C 실제 구현 (libAardvark 연동)
- [ ] FT4222H I2C 실제 구현 (libft4222 연동)
- [ ] PMU3 PowerControl/SsdGpio 실제 구현
- [ ] PMU4 PowerControl/SsdGpio 실제 구현
- [ ] I3C/Serial/UART 드라이버 (대상 하드웨어 미정)

### P2 — 테스트 보강

- [ ] ByteBuffer 단위 테스트
- [ ] Version 단위 테스트
- [ ] Device lifecycle 에러 케이스 (이중 Open, 미초기화 Open 등)
- [ ] Config 에러 케이스 (malformed JSON/YAML)

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
