# PLAS 사용 가이드

## 개요

**PLAS** (Platform Library Across Systems)는 I2C, I3C, Serial, UART, 전원 제어, SSD GPIO, PCI Config/DOE, CXL 등 다양한 하드웨어 인터페이스를 통합 추상화하는 C++17 라이브러리입니다. 드라이버별 구현 차이를 숨기고, 하나의 config 파일로 여러 장치를 자동 초기화하여 애플리케이션 코드를 단순화합니다.

---

## 빠른 시작

### 1. 빌드

```bash
cmake -B build -DPLAS_BUILD_TESTS=ON -DPLAS_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

### 2. 설정 파일 작성 (YAML)

```yaml
plas:
  devices:
    aardvark:
      - nickname: eeprom
        uri: "aardvark://0:0x50"
        bitrate: 400000

    pciutils:
      - nickname: nvme0
        uri: "pciutils://0000:03:00.0"
```

### 3. 코드 작성

```cpp
#include "plas/bootstrap/bootstrap.h"
#include "plas/hal/interface/i2c.h"

int main() {
    using namespace plas::bootstrap;
    using namespace plas::hal;

    // 1) 초기화 — 드라이버 등록, config 파싱, 디바이스 생성/열기를 한 번에 수행
    BootstrapConfig cfg;
    cfg.device_config_path = "config.yaml";
    cfg.device_config_key_path = "plas.devices";

    Bootstrap bs;
    auto result = bs.Init(cfg);
    if (result.IsError()) {
        // 초기화 실패
        return 1;
    }

    // 2) 디바이스 사용
    auto* i2c = bs.GetInterface<I2c>("eeprom");
    if (i2c) {
        uint8_t buf[16];
        auto rd = i2c->Read(0x50, buf, sizeof(buf));
        if (rd.IsOk()) {
            // rd.Value() = 읽은 바이트 수
        }
    }

    // 3) 정리 — 소멸자에서 자동 호출되므로 명시적 호출은 선택사항
    bs.Deinit();
    return 0;
}
```

---

## 설정 파일 형식

PLAS는 JSON과 YAML 두 가지 형식을 지원하며, 확장자(`.json`/`.yaml`/`.yml`)로 자동 감지합니다.

### Grouped 형식 (권장)

드라이버 이름을 키로 사용하여 그룹화합니다. `nickname`을 생략하면 `{driver}_{index}` 형태로 자동 생성됩니다.

```yaml
plas:
  devices:
    aardvark:
      - nickname: eeprom
        uri: "aardvark://0:0x50"
        bitrate: 400000
        pullup: true
      - nickname: sensor
        uri: "aardvark://1:0x48"

    pciutils:
      - nickname: nvme0
        uri: "pciutils://0000:03:00.0"
        doe_timeout_ms: 2000
```

### Flat 형식 (레거시)

모든 디바이스를 하나의 배열에 나열합니다. `driver` 필드가 필수입니다.

```json
[
  {
    "nickname": "eeprom",
    "driver": "aardvark",
    "uri": "aardvark://0:0x50",
    "args": { "bitrate": "400000" }
  }
]
```

### key_path

공유 설정 파일에서 디바이스 목록이 중첩된 위치를 지정합니다. 점(`.`)으로 구분합니다.

```cpp
cfg.device_config_key_path = "plas.devices";   // YAML의 plas: → devices: 를 가리킴
```

### 드라이버별 설정 인수

| 드라이버 | 인수 | 기본값 | 설명 |
|----------|------|--------|------|
| `aardvark` | `bitrate` | 100000 | I2C 비트레이트 (Hz) |
| | `pullup` | true | 내부 풀업 저항 |
| | `bus_timeout_ms` | 200 | 버스 타임아웃 (ms) |
| `ft4222h` | `bitrate` | 400000 | I2C 비트레이트 (Hz) |
| | `slave_addr` | 0x40 | 슬레이브 주소 (7비트) |
| | `sys_clock` | 60 | 시스템 클럭 (MHz: 60/24/48/80) |
| | `rx_timeout_ms` | 1000 | 수신 타임아웃 (ms) |
| | `rx_poll_interval_us` | 100 | 수신 폴링 간격 (us) |
| `pciutils` | `doe_timeout_ms` | 1000 | DOE 메일박스 타임아웃 (ms) |
| | `doe_poll_interval_us` | 100 | DOE 폴링 간격 (us) |

---

## URI 형식

모든 디바이스는 `driver://bus:identifier` 형식의 URI로 식별합니다.

| 드라이버 | URI 형식 | 예시 |
|----------|----------|------|
| `aardvark` | `aardvark://port:address` | `aardvark://0:0x50` |
| `ft4222h` | `ft4222h://master_idx:slave_idx` | `ft4222h://0:1` |
| `pciutils` | `pciutils://DDDD:BB:DD.F` | `pciutils://0000:03:00.0` |
| `pmu3` | `pmu3://bus:id` | `pmu3://0:0` |
| `pmu4` | `pmu4://bus:id` | `pmu4://0:0` |

`Bootstrap::ValidateUri(uri)` 로 URI 형식을 사전 검증할 수 있습니다.

---

## 디바이스 접근

### 닉네임으로 조회

```cpp
auto* device = bs.GetDevice("eeprom");          // Device* (nullptr if 없음)
```

### URI로 조회

```cpp
auto* device = bs.GetDeviceByUri("aardvark://0:0x50");
```

### 인터페이스 캐스팅

```cpp
auto* i2c = bs.GetInterface<I2c>("eeprom");     // I2c* (nullptr if 미지원)
```

### 특정 인터페이스를 지원하는 모든 디바이스 조회

```cpp
auto i2c_devices = bs.GetDevicesByInterface<I2c>();
// vector<pair<string, I2c*>> — (닉네임, 인터페이스 포인터) 쌍
for (auto& [name, iface] : i2c_devices) {
    // name = "eeprom", iface = I2c*
}
```

---

## HAL 인터페이스

PLAS가 제공하는 순수 가상 인터페이스(ABC) 목록입니다. 드라이버가 `Device`와 함께 해당 인터페이스를 다중 상속하여 구현합니다. `dynamic_cast`로 지원 여부를 런타임에 확인합니다.

| 인터페이스 | 네임스페이스 | 주요 메서드 | 용도 |
|------------|-------------|------------|------|
| `I2c` | `plas::hal` | Read, Write, WriteRead, SetBitrate | I2C 버스 통신 |
| `I3c` | `plas::hal` | Read, Write, SendCcc, SetFrequency | I3C 버스 통신 |
| `Serial` | `plas::hal` | Read, Write, SetBaudRate, Flush | 시리얼 포트 |
| `Uart` | `plas::hal` | Read, Write, SetBaudRate, SetParity | UART 통신 |
| `PowerControl` | `plas::hal` | SetVoltage, GetVoltage, PowerOn/Off | 전원 제어 |
| `SsdGpio` | `plas::hal` | SetPerst, SetClkReq, SetDualPort | SSD GPIO 제어 |
| `PciConfig` | `plas::hal::pci` | ReadConfig8/16/32, FindCapability | PCI 설정 공간 접근 |
| `PciDoe` | `plas::hal::pci` | DoeDiscover, DoeExchange | PCI DOE 프로토콜 |
| `Cxl` | `plas::hal::pci` | EnumerateCxlDvsecs, GetCxlDeviceType | CXL DVSEC 탐색 |
| `CxlMailbox` | `plas::hal::pci` | ExecuteCommand, IsReady | CXL 메일박스 명령 |

### 드라이버-인터페이스 매핑

| 드라이버 | 구현 인터페이스 | SDK 필요 | 상태 |
|----------|----------------|---------|------|
| `AardvarkDevice` | Device, I2c | Aardvark SDK | 완전 구현 |
| `Ft4222hDevice` | Device, I2c | FT4222H + D2XX SDK | 완전 구현 |
| `PciUtilsDevice` | Device, PciConfig, PciDoe | libpci-dev | 완전 구현 |
| `Pmu3Device` | Device, PowerControl, SsdGpio | PMU3 SDK | 스텁 (kNotSupported) |
| `Pmu4Device` | Device, PowerControl, SsdGpio | PMU4 SDK | 스텁 (kNotSupported) |

> SDK가 없어도 드라이버는 빌드됩니다. I/O 메서드만 `kNotSupported`를 반환합니다.

---

## 에러 처리

PLAS는 예외를 사용하지 않습니다. 모든 실패 가능한 연산은 `Result<T>`를 반환합니다.

### Result\<T\> 패턴

```cpp
auto result = i2c->Read(0x50, buf, 16);
if (result.IsOk()) {
    size_t bytes_read = result.Value();
} else {
    std::error_code ec = result.Error();
    // ec.message() → "I/O error", "Timeout" 등
}
```

`Result<void>`는 값 없이 성공/실패만 나타냅니다:

```cpp
auto result = device->Open();
if (result.IsError()) {
    // 실패 처리
}
```

### 에러 코드

`plas::core::ErrorCode` 열거형:

| 코드 | 의미 |
|------|------|
| `kSuccess` | 성공 |
| `kInvalidArgument` | 잘못된 인수 |
| `kNotFound` | 찾을 수 없음 |
| `kTimeout` | 시간 초과 |
| `kBusy` | 장치 사용 중 |
| `kIOError` | I/O 오류 |
| `kNotSupported` | 지원하지 않는 기능 |
| `kNotInitialized` | 초기화되지 않음 |
| `kAlreadyOpen` | 이미 열림 |
| `kAlreadyClosed` | 이미 닫힘 |
| `kPermissionDenied` | 권한 부족 |
| `kOutOfMemory` | 메모리 부족 |
| `kTypeMismatch` | 타입 불일치 |
| `kOutOfRange` | 범위 초과 |
| `kOverflow` | 오버플로 |
| `kResourceExhausted` | 리소스 소진 |
| `kCancelled` | 취소됨 |
| `kDataLoss` | 데이터 손실 |
| `kInternalError` | 내부 오류 |
| `kUnknown` | 알 수 없는 오류 |

### DeviceFailure — 디바이스 초기화 실패 상세 정보

Bootstrap 초기화 중 개별 디바이스 실패 시 `DeviceFailure` 구조체에 상세 정보가 기록됩니다:

```cpp
auto result = bs.Init(cfg);
for (const auto& f : bs.GetFailures()) {
    // f.nickname  — 디바이스 닉네임
    // f.uri       — URI
    // f.driver    — 드라이버 이름
    // f.error     — std::error_code
    // f.phase     — 실패 단계: "create", "init", "open"
    // f.detail    — 사람이 읽을 수 있는 상세 설명
}
```

---

## 디버깅

### DumpDevices()

로드된 모든 디바이스의 상태를 한 번에 확인합니다:

```cpp
std::printf("%s", bs.DumpDevices().c_str());
```

출력 예시:
```
--- Loaded Devices ---
  eeprom        aardvark://0:0x50       driver=aardvark  state=Open  [I2c]
  nvme0         pciutils://0000:03:00.0 driver=pciutils  state=Open  [PciConfig, PciDoe]
--- Failures ---
  (none)
```

### ValidateUri()

URI 형식을 사전 검증합니다 (실제 연결 없이 형식만 확인):

```cpp
bool ok = Bootstrap::ValidateUri("aardvark://0:0x50");       // true
bool bad = Bootstrap::ValidateUri("bad_uri_no_scheme");      // false
```

### Bootstrap 초기화 결과 확인

```cpp
auto result = bs.Init(cfg);
if (result.IsOk()) {
    auto& br = result.Value();
    // br.devices_opened  — 성공적으로 열린 디바이스 수
    // br.devices_failed  — 실패한 디바이스 수
    // br.devices_skipped — 건너뛴 디바이스 수 (드라이버 미등록 등)
}
```

---

## 고급 사용법

### Bootstrap 없이 DeviceManager 직접 사용

```cpp
#include "plas/hal/device_manager.h"
#include "plas/hal/interface/device_factory.h"

// 1) 드라이버 등록
AardvarkDevice::Register();

// 2) 설정 파일로 디바이스 로드
auto& dm = plas::hal::DeviceManager::GetInstance();
auto result = dm.LoadFromConfig("config.yaml", "plas.devices");

// 3) 사용
auto* i2c = dm.GetInterface<plas::hal::I2c>("eeprom");
```

DeviceManager는 Meyer's 싱글톤이며, `Reset()`으로 모든 디바이스를 닫고 제거할 수 있습니다.

### Properties 세션

키-값 저장소로, 세션별로 분리된 런타임 설정을 관리합니다:

```cpp
#include "plas/core/properties.h"

auto& session = plas::core::Properties::GetSession("my_session");
session.Set<int>("timeout_ms", 5000);
session.Set<std::string>("label", "test");

auto val = session.Get<int>("timeout_ms");    // Result<int>
auto as_double = session.GetAs<double>("timeout_ms");  // 숫자 변환 (범위 검사 포함)
```

PropertyManager를 사용하면 YAML/JSON 파일에서 자동으로 세션을 로드할 수 있습니다:

```cpp
#include "plas/config/property_manager.h"

auto& pm = plas::config::PropertyManager::GetInstance();
pm.LoadFromFile("properties.yaml");  // 최상위 키 = 세션 이름
```

### 로거 설정

```cpp
#include "plas/log/logger.h"

plas::log::LogConfig log_cfg;
log_cfg.level = plas::log::LogLevel::kDebug;
log_cfg.console_enabled = true;

plas::log::Logger::GetInstance().Init(log_cfg);

PLAS_LOG_INFO("초기화 완료");
PLAS_LOG_ERROR("문제 발생");
```

Bootstrap 사용 시에는 `BootstrapConfig::log_config`에 설정하면 자동으로 초기화됩니다.

### ConfigNode 트리 탐색

설정 파일의 서브트리를 직접 탐색할 때 사용합니다:

```cpp
#include "plas/config/config_node.h"

auto node = plas::config::ConfigNode::LoadFromFile("config.yaml");
if (node.IsOk()) {
    auto subtree = node.Value().GetSubtree("plas.devices");
    // subtree.Value().IsMap(), IsArray(), IsScalar(), IsNull()
}
```

### PCI 토폴로지 탐색

sysfs 기반으로 PCI 디바이스 토폴로지를 탐색합니다 (실제 리눅스 환경 필요):

```cpp
#include "plas/hal/interface/pci/pci_topology.h"

using namespace plas::hal::pci;

PciAddress addr{0x0000, {0x03, 0x00, 0x00}};

if (PciTopology::DeviceExists(addr)) {
    auto path = PciTopology::GetPathToRoot(addr);
    if (path.IsOk()) {
        for (auto& node : path.Value()) {
            // node.address, node.port_type, node.is_bridge
        }
    }
}
```

---

## Graceful Degradation

Bootstrap은 개별 디바이스 실패를 허용하는 옵션을 제공합니다:

```cpp
BootstrapConfig cfg;
cfg.skip_unknown_drivers = true;   // 미등록 드라이버 건너뛰기
cfg.skip_device_failures = true;   // 개별 디바이스 실패 건너뛰기
cfg.auto_open_devices = true;      // Init 후 자동 Open (기본값: true)
```

두 옵션 모두 `true`로 설정하면, 실패한 디바이스는 `BootstrapResult::failures` 벡터에 기록되고 나머지 디바이스는 정상 동작합니다. `false`로 설정하면 첫 번째 실패 시 전체 Init이 에러를 반환합니다.

---

## 새 드라이버 추가 체크리스트

1. **헤더** 생성: `components/plas-drivers/include/plas/hal/driver/<name>/<name>_device.h`
   - `Device` + 해당 인터페이스 ABC를 다중 상속
2. **소스** 구현: `components/plas-drivers/src/hal/driver/<name>/<name>_device.cpp`
   - SDK 호출부는 `#ifdef PLAS_HAS_<NAME>` 으로 감싸기 (SDK 없으면 stub)
3. **Register()** 정적 메서드 추가 — `DeviceFactory::RegisterDriver("name", creator)` 호출
4. **CMakeLists.txt**: `components/plas-drivers/CMakeLists.txt`에 소스 추가
5. **SDK가 필요한 경우**:
   - `cmake/Find<Name>.cmake` 생성 (vendor/ → `*_ROOT` → 시스템 순서로 탐색)
   - `vendor/<name>/` 디렉토리에 헤더와 라이브러리 배치
   - CMakeLists.txt에 조건부 블록 추가 (`PLAS_WITH_<NAME>` 옵션 + 링크 + 디파인)
6. **Bootstrap 등록**: `bootstrap.cpp`의 `RegisterAllDrivers()`에 `Register()` 호출 추가
7. **테스트** 작성: `tests/hal/`에 유닛 테스트 (SDK 없이 항상 빌드) + 통합 테스트 (환경 변수 게이트)

---

## CMake 타겟 의존성

```
plas_core
  └─ plas_log          (PRIVATE: spdlog)
  └─ plas_config       (PRIVATE: nlohmann_json, yaml-cpp)
      └─ plas_hal_interface
          └─ plas_hal_driver   (PRIVATE: 벤더 SDK, 선택적)
              └─ plas_bootstrap
```

애플리케이션에서는 `plas_bootstrap`만 링크하면 모든 하위 타겟이 전이적으로 포함됩니다:

```cmake
target_link_libraries(my_app PRIVATE plas::bootstrap)
```

---

## 예제

빌드 시 `-DPLAS_BUILD_EXAMPLES=ON`을 추가하면 다음 예제가 함께 빌드됩니다:

| 예제 | 설명 | 빌드 타겟 |
|------|------|----------|
| `properties_basics` | Properties 세션 CRUD, 타입 변환 | `plas::core` |
| `logger_basics` | 로거 초기화, 매크로, 레벨 제어 | `plas::log` |
| `config_load` | JSON/YAML 로드, FindDevice | `plas::config` |
| `config_node_tree` | 서브트리 탐색, 타입 쿼리 | `plas::config` |
| `property_manager` | 파일에서 세션 로드, 런타임 업데이트 | `plas::config` |
| `device_manager` | 드라이버 등록, 인터페이스 캐스팅 | `plas::hal_interface` + `plas::hal_driver` |
| `doe_exchange` | PCI DOE 프로토콜 탐색/교환 (stub) | `plas::hal_interface` |
| `topology_walk` | sysfs PCI 토폴로지 탐색 | `plas::hal_interface` |
| `master_example` | Bootstrap 엔드투엔드 데모 | `plas::bootstrap` |
