# PLAS API 레퍼런스

> 모듈별 공개 API 시그니처를 정리한 문서입니다.
> 헤더 경로는 `components/plas-core/include/plas/` 기준입니다 (드라이버: `components/plas-drivers/include/plas/`, 부트스트랩: `components/plas-bootstrap/include/plas/`).

---

## 목차

1. [Core](#1-core)
2. [Log](#2-log)
3. [Config](#3-config)
4. [HAL — 디바이스 관리](#4-hal--디바이스-관리)
5. [HAL — 인터페이스 ABC](#5-hal--인터페이스-abc)
6. [PCI / CXL](#6-pci--cxl)
7. [드라이버](#7-드라이버)
8. [Bootstrap](#8-bootstrap)

---

## 1. Core

### 타입 별칭 — `plas::core` (`core/types.h`)

```cpp
using Byte    = uint8_t;
using Word    = uint16_t;
using DWord   = uint32_t;
using QWord   = uint64_t;
using Address = uint32_t;
```

### 단위 타입 — `plas::core` (`core/units.h`)

```cpp
class Voltage {
    explicit Voltage(double volts);
    double Value() const;
    // 비교 연산자: ==, !=, <, <=, >, >=
};

class Current {
    explicit Current(double amps);
    double Value() const;
};

class Frequency {
    explicit Frequency(double hertz);
    double Value() const;
};
```

### ErrorCode — `plas::core` (`core/error.h`)

```cpp
enum class ErrorCode : int {
    kSuccess = 0,
    kInvalidArgument, kNotFound, kTimeout, kBusy, kIOError,
    kNotSupported, kNotInitialized, kAlreadyOpen, kAlreadyClosed,
    kPermissionDenied, kOutOfMemory, kTypeMismatch, kOutOfRange,
    kOverflow, kResourceExhausted, kCancelled, kDataLoss,
    kInternalError, kUnknown
};

std::error_code make_error_code(ErrorCode code);
```

`std::is_error_code_enum<ErrorCode>` 특수화가 제공되므로 `std::error_code`로 암시적 변환이 가능합니다.

### Result\<T\> — `plas::core` (`core/result.h`)

예외 없이 성공/실패를 나타내는 타입입니다.

```cpp
template <typename T>
class Result {
    static Result Ok(T value);
    static Result Err(std::error_code error);
    static Result Err(ErrorCode code);

    bool IsOk() const;
    bool IsError() const;
    T& Value();
    const T& Value() const;
    std::error_code Error() const;
};

// void 특수화
template <>
class Result<void> {
    static Result Ok();
    static Result Err(std::error_code error);
    static Result Err(ErrorCode code);

    bool IsOk() const;
    bool IsError() const;
    std::error_code Error() const;
};
```

### ByteBuffer — `plas::core` (`core/byte_buffer.h`)

가변 크기 바이트 버퍼입니다.

```cpp
class ByteBuffer {
    ByteBuffer();
    explicit ByteBuffer(size_t size);
    ByteBuffer(std::initializer_list<uint8_t> init);
    ByteBuffer(const uint8_t* data, size_t size);

    const uint8_t* Data() const;
    uint8_t* Data();
    size_t Size() const;
    bool Empty() const;

    void Clear();
    void Resize(size_t new_size);
    void Append(const uint8_t* data, size_t size);

    uint8_t& operator[](size_t index);
    const uint8_t& operator[](size_t index) const;
};
```

### Version — `plas::core` (`core/version.h`)

```cpp
struct Version {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
};

Version GetVersion();
std::string GetVersionString();
```

### Properties — `plas::core` (`core/properties.h`)

세션 기반 키-값 저장소입니다. 스레드 안전합니다.

```cpp
class Properties {
    // 세션 관리 (static)
    static Properties& GetSession(const std::string& name);
    static void DestroySession(const std::string& name);
    static void DestroyAll();
    static bool HasSession(const std::string& name);

    // 키-값 접근
    template <typename T> void Set(const std::string& key, T value);
    template <typename T> Result<T> Get(const std::string& key) const;
    template <typename To> Result<To> GetAs(const std::string& key) const;  // 숫자 변환 (범위 검사)

    bool Has(const std::string& key) const;
    void Remove(const std::string& key);
    void Clear();
    size_t Size() const;
    std::vector<std::string> Keys() const;
};
```

`GetAs<To>`는 저장된 숫자 타입(int8~64, uint8~64, float, double, bool)에서 대상 타입으로 범위 검사 후 변환합니다.

---

## 2. Log

### LogLevel — `plas::log` (`log/log_level.h`)

```cpp
enum class LogLevel {
    kTrace, kDebug, kInfo, kWarn, kError, kCritical, kOff
};

std::string ToString(LogLevel level);
```

### LogConfig — `plas::log` (`log/log_config.h`)

```cpp
struct LogConfig {
    std::string log_dir       = "logs";
    std::string file_prefix   = "plas";
    std::size_t max_file_size = 10 * 1024 * 1024;  // 10 MB
    std::size_t max_files     = 5;
    LogLevel    level         = LogLevel::kInfo;
    bool        console_enabled = true;
};
```

### Logger — `plas::log` (`log/logger.h`)

싱글톤 로거입니다. 백엔드는 spdlog (pimpl 패턴, 컴파일 타임 선택).

```cpp
class Logger {
    static Logger& GetInstance();

    void Init(const LogConfig& config);
    void SetLevel(LogLevel level);
    LogLevel GetLevel() const;
    std::string GetCurrentLogFile() const;

    void Log(LogLevel level, const std::string& msg);
    void Trace(const std::string& msg);
    void Debug(const std::string& msg);
    void Info(const std::string& msg);
    void Warn(const std::string& msg);
    void Error(const std::string& msg);
    void Critical(const std::string& msg);
};
```

### 로그 매크로

```cpp
PLAS_LOG_TRACE(msg)
PLAS_LOG_DEBUG(msg)
PLAS_LOG_INFO(msg)
PLAS_LOG_WARN(msg)
PLAS_LOG_ERROR(msg)
PLAS_LOG_CRITICAL(msg)
```

---

## 3. Config

### ConfigFormat — `plas::config` (`config/config_format.h`)

```cpp
enum class ConfigFormat { kJson, kYaml, kAuto };
```

`kAuto`는 파일 확장자(`.json`, `.yaml`, `.yml`)로 자동 감지합니다.

### DeviceEntry — `plas::config` (`config/device_entry.h`)

설정 파일에서 파싱된 디바이스 한 개의 정보입니다.

```cpp
struct DeviceEntry {
    std::string nickname;
    std::string uri;
    std::string driver;
    std::map<std::string, std::string> args;
};
```

### Config — `plas::config` (`config/config.h`)

디바이스 설정을 파싱합니다. flat 배열과 grouped 맵 형식 모두 지원합니다.

```cpp
class Config {
    static Result<Config> LoadFromFile(const std::string& path,
                                       ConfigFormat fmt = ConfigFormat::kAuto);
    static Result<Config> LoadFromFile(const std::string& path,
                                       const std::string& key_path,
                                       ConfigFormat fmt = ConfigFormat::kAuto);
    static Result<Config> LoadFromNode(const ConfigNode& node);

    const std::vector<DeviceEntry>& GetDevices() const;
    std::optional<DeviceEntry> FindDevice(const std::string& nickname) const;
};
```

### ConfigNode — `plas::config` (`config/config_node.h`)

설정 파일의 트리 구조를 탐색합니다.

```cpp
class ConfigNode {
    static Result<ConfigNode> LoadFromFile(const std::string& path,
                                            ConfigFormat fmt = ConfigFormat::kAuto);

    Result<ConfigNode> GetSubtree(const std::string& key_path) const;  // "a.b.c" 형태

    bool IsMap() const;
    bool IsArray() const;
    bool IsScalar() const;
    bool IsNull() const;
};
```

### PropertyManager — `plas::config` (`config/property_manager.h`)

YAML/JSON 파일에서 Properties 세션을 자동 로드합니다.

```cpp
class PropertyManager {
    static PropertyManager& GetInstance();

    // 멀티 세션: 최상위 키 = 세션 이름
    Result<void> LoadFromFile(const std::string& path,
                               ConfigFormat fmt = ConfigFormat::kAuto);
    // 싱글 세션: 파일 전체 → 하나의 세션
    Result<void> LoadFromFile(const std::string& path,
                               const std::string& session_name,
                               ConfigFormat fmt = ConfigFormat::kAuto);

    Properties& Session(const std::string& name);
    bool HasSession(const std::string& name) const;
    std::vector<std::string> SessionNames() const;
    void Reset();
};
```

---

## 4. HAL — 디바이스 관리

### DeviceState — `plas::hal` (`hal/interface/device.h`)

```cpp
enum class DeviceState {
    kUninitialized, kInitialized, kOpen, kClosed, kError
};
```

### Device — `plas::hal` (`hal/interface/device.h`)

모든 디바이스의 기본 ABC입니다. 라이프사이클(Init→Open→Close)과 식별자를 정의합니다.

```cpp
class Device {
    virtual ~Device() = default;

    virtual Result<void> Init() = 0;
    virtual Result<void> Open() = 0;
    virtual Result<void> Close() = 0;
    virtual Result<void> Reset() = 0;

    virtual DeviceState GetState() const = 0;
    virtual std::string GetName() const = 0;
    virtual std::string GetUri() const = 0;
};
```

### DeviceFactory — `plas::hal` (`hal/interface/device_factory.h`)

드라이버 자기 등록 팩토리입니다.

```cpp
class DeviceFactory {
    using CreatorFunc = std::function<std::unique_ptr<Device>(const DeviceEntry&)>;

    static Result<std::unique_ptr<Device>> CreateFromConfig(const DeviceEntry& entry);
    static void RegisterDriver(const std::string& driver_name, CreatorFunc creator);
};
```

### DeviceManager — `plas::hal` (`hal/device_manager.h`)

디바이스 인스턴스 레지스트리입니다 (Meyer's 싱글톤, 스레드 안전).

```cpp
class DeviceManager {
    static DeviceManager& GetInstance();

    // 설정 로드
    Result<void> LoadFromConfig(const std::string& path,
                                 ConfigFormat fmt = ConfigFormat::kAuto);
    Result<void> LoadFromConfig(const std::string& path,
                                 const std::string& key_path,
                                 ConfigFormat fmt = ConfigFormat::kAuto);
    Result<void> LoadFromTree(const ConfigNode& node);
    Result<void> LoadFromEntries(const std::vector<DeviceEntry>& entries);
    Result<void> AddDevice(const std::string& nickname, std::unique_ptr<Device> device);

    // 조회
    Device* GetDevice(const std::string& nickname);
    Device* GetDeviceByUri(const std::string& uri);
    template <typename T> T* GetInterface(const std::string& nickname);
    template <typename T> std::vector<std::pair<std::string, T*>> GetDevicesByInterface();

    // 쿼리
    std::vector<std::string> DeviceNames() const;
    bool HasDevice(const std::string& nickname) const;
    std::size_t DeviceCount() const;

    void Reset();  // 모든 디바이스 Close + 제거
};
```

---

## 5. HAL — 인터페이스 ABC

### I2c — `plas::hal` (`hal/interface/i2c.h`)

```cpp
class I2c {
    virtual Result<size_t> Read(Address addr, Byte* data, size_t length) = 0;
    virtual Result<size_t> Write(Address addr, const Byte* data, size_t length) = 0;
    virtual Result<size_t> WriteRead(Address addr,
                                      const Byte* write_data, size_t write_len,
                                      Byte* read_data, size_t read_len) = 0;
    virtual Result<void> SetBitrate(uint32_t bitrate) = 0;
    virtual uint32_t GetBitrate() const = 0;
};
```

### I3c — `plas::hal` (`hal/interface/i3c.h`)

```cpp
class I3c {
    virtual Result<size_t> Read(Address addr, Byte* data, size_t length) = 0;
    virtual Result<size_t> Write(Address addr, const Byte* data, size_t length) = 0;
    virtual Result<void> SendCcc(uint8_t ccc_id, const Byte* data, size_t length) = 0;
    virtual Result<void> SetFrequency(Frequency freq) = 0;
};
```

### Serial — `plas::hal` (`hal/interface/serial.h`)

```cpp
class Serial {
    virtual Result<size_t> Read(Byte* data, size_t length) = 0;
    virtual Result<size_t> Write(const Byte* data, size_t length) = 0;
    virtual Result<void> SetBaudRate(uint32_t baud_rate) = 0;
    virtual uint32_t GetBaudRate() const = 0;
    virtual Result<void> Flush() = 0;
};
```

### Uart — `plas::hal` (`hal/interface/uart.h`)

```cpp
enum class Parity { kNone, kOdd, kEven };

class Uart {
    virtual Result<size_t> Read(Byte* data, size_t length) = 0;
    virtual Result<size_t> Write(const Byte* data, size_t length) = 0;
    virtual Result<void> SetBaudRate(uint32_t baud_rate) = 0;
    virtual uint32_t GetBaudRate() const = 0;
    virtual Result<void> SetParity(Parity parity) = 0;
};
```

### PowerControl — `plas::hal` (`hal/interface/power_control.h`)

```cpp
class PowerControl {
    virtual Result<void> SetVoltage(Voltage voltage) = 0;
    virtual Result<Voltage> GetVoltage() = 0;
    virtual Result<void> SetCurrent(Current current) = 0;
    virtual Result<Current> GetCurrent() = 0;
    virtual Result<void> PowerOn() = 0;
    virtual Result<void> PowerOff() = 0;
    virtual Result<bool> IsPowerOn() = 0;
};
```

### SsdGpio — `plas::hal` (`hal/interface/ssd_gpio.h`)

```cpp
class SsdGpio {
    virtual Result<void> SetPerst(bool active) = 0;
    virtual Result<bool> GetPerst() = 0;
    virtual Result<void> SetClkReq(bool active) = 0;
    virtual Result<bool> GetClkReq() = 0;
    virtual Result<void> SetDualPort(bool enable) = 0;
    virtual Result<bool> GetDualPort() = 0;
};
```

---

## 6. PCI / CXL

### PCI 타입 — `plas::hal::pci` (`hal/interface/pci/types.h`)

```cpp
struct Bdf {
    uint8_t bus;
    uint8_t device;    // 5비트 유효
    uint8_t function;  // 3비트 유효

    constexpr uint16_t Pack() const;
    static constexpr Bdf FromPacked(uint16_t packed);
};

using ConfigOffset = uint16_t;  // 0x000–0xFFF

struct PciAddress {
    uint16_t domain;
    Bdf bdf;

    std::string ToString() const;                    // "DDDD:BB:DD.F"
    static Result<PciAddress> FromString(const std::string& str);
};

enum class CapabilityId : uint8_t {
    kPowerManagement = 0x01, kAgp = 0x02, kVpd = 0x03,
    kMsi = 0x05, kPciExpress = 0x10, kMsix = 0x11
};

enum class ExtCapabilityId : uint16_t {
    kAer = 0x0001, kSriov = 0x0010, kDvsec = 0x0023, kDoe = 0x002E
};

struct DoeProtocolId {
    uint16_t vendor_id;
    uint8_t data_object_type;
};

using DoePayload = std::vector<DWord>;

enum class PciePortType : uint8_t {
    kEndpoint = 0x00, kLegacyEndpoint = 0x01,
    kRootPort = 0x04, kUpstreamPort = 0x05, kDownstreamPort = 0x06,
    kPcieToPciBridge = 0x07, kPciToPcieBridge = 0x08,
    kRcIntegratedEndpoint = 0x09, kRcEventCollector = 0x0A,
    kUnknown = 0xFF
};
```

### PciConfig — `plas::hal::pci` (`hal/interface/pci/pci_config.h`)

PCI 설정 공간 읽기/쓰기 인터페이스입니다.

```cpp
class PciConfig {
    // 타입별 읽기
    virtual Result<Byte>  ReadConfig8(Bdf bdf, ConfigOffset offset) = 0;
    virtual Result<Word>  ReadConfig16(Bdf bdf, ConfigOffset offset) = 0;
    virtual Result<DWord> ReadConfig32(Bdf bdf, ConfigOffset offset) = 0;

    // 타입별 쓰기
    virtual Result<void> WriteConfig8(Bdf bdf, ConfigOffset offset, Byte value) = 0;
    virtual Result<void> WriteConfig16(Bdf bdf, ConfigOffset offset, Word value) = 0;
    virtual Result<void> WriteConfig32(Bdf bdf, ConfigOffset offset, DWord value) = 0;

    // 캐퍼빌리티 탐색
    virtual Result<std::optional<ConfigOffset>> FindCapability(Bdf bdf, CapabilityId id) = 0;
    virtual Result<std::optional<ConfigOffset>> FindExtCapability(Bdf bdf, ExtCapabilityId id) = 0;
};
```

### PciDoe — `plas::hal::pci` (`hal/interface/pci/pci_doe.h`)

PCI DOE (Data Object Exchange) 프로토콜 인터페이스입니다.

```cpp
class PciDoe {
    virtual Result<std::vector<DoeProtocolId>> DoeDiscover(
        Bdf bdf, ConfigOffset doe_offset) = 0;

    virtual Result<DoePayload> DoeExchange(
        Bdf bdf, ConfigOffset doe_offset,
        DoeProtocolId protocol, const DoePayload& request) = 0;
};
```

### PciTopology — `plas::hal::pci` (`hal/interface/pci/pci_topology.h`)

sysfs 기반 PCI 토폴로지 탐색 유틸리티입니다 (모든 메서드가 static).

```cpp
struct PciDeviceNode {
    PciAddress address;
    PciePortType port_type;
    bool is_bridge;
    std::string sysfs_path;
};

class PciTopology {
    static std::string GetSysfsPath(const PciAddress& addr);
    static bool DeviceExists(const PciAddress& addr);
    static Result<PciDeviceNode> GetDeviceInfo(const PciAddress& addr);

    static Result<std::optional<PciAddress>> FindParent(const PciAddress& addr);
    static Result<PciAddress> FindRootPort(const PciAddress& addr);
    static Result<std::vector<PciDeviceNode>> GetPathToRoot(const PciAddress& addr);

    static Result<void> RemoveDevice(const PciAddress& addr);
    static Result<void> RescanBridge(const PciAddress& bridge_addr);
    static Result<void> RescanAll();

    static void SetSysfsRoot(const std::string& root);  // 테스트용
};
```

### CXL 타입 — `plas::hal::pci` (`hal/interface/pci/cxl_types.h`)

```cpp
namespace cxl {
constexpr uint16_t kCxlVendorId = 0x1E98;
}

enum class CxlDvsecId : uint16_t {
    kCxlDevice = 0x0000,   kNonCxlFunction = 0x0002,
    kCxlExtensionsPort = 0x0003, kGpfPort = 0x0004,
    kGpfDevice = 0x0005,   kFlexBusPort = 0x0007,
    kRegisterLocator = 0x0008, kMld = 0x0009
};

enum class CxlDeviceType : uint8_t {
    kType1 = 1, kType2 = 2, kType3 = 3, kUnknown = 0xFF
};

struct DvsecHeader {
    uint16_t offset;
    uint16_t vendor_id;
    uint16_t dvsec_revision;
    CxlDvsecId dvsec_id;
    uint16_t dvsec_length;
};

enum class CxlRegisterBlockId : uint8_t {
    kEmpty = 0, kComponentRegister = 1,
    kBarVirtualized = 2, kCxlDeviceRegister = 3
};

struct RegisterBlockEntry {
    CxlRegisterBlockId block_id;
    uint8_t bar_index;
    uint64_t offset;
};

enum class CxlMailboxOpcode : uint16_t {
    kIdentify = 0x0001,
    kGetFwInfo = 0x0200, kTransferFw = 0x0201, kActivateFw = 0x0202,
    kGetTimestamp = 0x0300, kSetTimestamp = 0x0301,
    kGetSupportedLogs = 0x0400, kGetLog = 0x0401,
    kGetSupportedFeatures = 0x0500, kGetFeature = 0x0501, kSetFeature = 0x0502,
    kIdentifyMemoryDevice = 0x4000,
    kGetHealthInfo = 0x4200, kGetAlertConfig = 0x4201, kSetAlertConfig = 0x4202,
    kGetPoisonList = 0x4300, kInjectPoison = 0x4301, kClearPoison = 0x4302,
    kSanitize = 0x4400, kSecureErase = 0x4401,
    kGetScanMediaCapabilities = 0x4302
};

enum class CxlMailboxReturnCode : uint16_t {
    kSuccess = 0x0000, kBackgroundCmdStarted = 0x0001,
    kInvalidInput = 0x0002, kUnsupported = 0x0003,
    kInternalError = 0x0004, kRetryRequired = 0x0005,
    kBusy = 0x0006, kMediaDisabled = 0x0007,
    kFwTransferInProgress = 0x0008, kFwTransferOutOfOrder = 0x0009,
    kFwAuthenticationFailed = 0x000A, kInvalidSlot = 0x000B,
    kActivationFailedFwRolled = 0x000C, kActivationFailedColdReset = 0x000D,
    kInvalidHandle = 0x000E, kInvalidPhysicalAddress = 0x000F,
    kPayloadLengthError = 0x0012
};

using CxlMailboxPayload = std::vector<uint8_t>;

struct CxlMailboxResult {
    CxlMailboxReturnCode return_code;
    CxlMailboxPayload payload;
};

// IDE-KM 타입
namespace doe_type {
constexpr uint8_t kIdeKm = 0x02;
}

enum class IdeKmMessageType : uint8_t {
    kQuery = 0x00, kQueryResp = 0x01,
    kKeyProg = 0x02, kKeyProgAck = 0x03,
    kKeySetGo = 0x04, kKeySetGoAck = 0x05,
    kKeySetStop = 0x06, kKeySetStopAck = 0x07
};

struct IdeStreamId {
    uint8_t stream_id;
    uint8_t key_set;
    uint8_t direction;   // 0 = TX, 1 = RX
    uint8_t sub_stream;
};
```

### Cxl — `plas::hal::pci` (`hal/interface/pci/cxl.h`)

CXL DVSEC 탐색 인터페이스입니다.

```cpp
class Cxl {
    virtual Result<std::vector<DvsecHeader>> EnumerateCxlDvsecs(Bdf bdf) = 0;
    virtual Result<std::optional<DvsecHeader>> FindCxlDvsec(Bdf bdf, CxlDvsecId dvsec_id) = 0;
    virtual Result<CxlDeviceType> GetCxlDeviceType(Bdf bdf) = 0;
    virtual Result<std::vector<RegisterBlockEntry>> GetRegisterBlocks(Bdf bdf) = 0;
    virtual Result<DWord> ReadDvsecRegister(Bdf bdf, ConfigOffset dvsec_offset, uint16_t reg_offset) = 0;
    virtual Result<void> WriteDvsecRegister(Bdf bdf, ConfigOffset dvsec_offset, uint16_t reg_offset, DWord value) = 0;
};
```

### CxlMailbox — `plas::hal::pci` (`hal/interface/pci/cxl_mailbox.h`)

CXL 메일박스 명령 실행 인터페이스입니다.

```cpp
class CxlMailbox {
    virtual Result<CxlMailboxResult> ExecuteCommand(
        Bdf bdf, CxlMailboxOpcode opcode, const CxlMailboxPayload& payload) = 0;
    virtual Result<CxlMailboxResult> ExecuteCommand(
        Bdf bdf, uint16_t raw_opcode, const CxlMailboxPayload& payload) = 0;

    virtual Result<uint32_t> GetPayloadSize(Bdf bdf) = 0;
    virtual Result<bool> IsReady(Bdf bdf) = 0;
    virtual Result<CxlMailboxResult> GetBackgroundCmdStatus(Bdf bdf) = 0;
};
```

---

## 7. 드라이버

모든 드라이버는 `plas::hal::driver` 네임스페이스에 있으며, `Device` + 해당 인터페이스 ABC를 다중 상속합니다. 각 드라이버에는 팩토리 자기 등록을 위한 `static void Register()` 메서드가 있습니다.

### AardvarkDevice (`hal/driver/aardvark/aardvark_device.h`)

I2C 어댑터 드라이버입니다 (Total Phase Aardvark).

```cpp
class AardvarkDevice : public Device, public I2c {
    explicit AardvarkDevice(const config::DeviceEntry& entry);
    static void Register();   // 드라이버 이름: "aardvark"
};
```

| 항목 | 값 |
|------|-----|
| 드라이버 이름 | `aardvark` |
| URI 형식 | `aardvark://port:address` (port: 0–65535, address: 7비트 I2C 0x00–0x7F) |
| SDK 필요 | Aardvark SDK (`PLAS_HAS_AARDVARK`) |
| 구현 인터페이스 | `Device`, `I2c` |
| 설정 인수 | `bitrate` (기본 100000), `pullup` (기본 true), `bus_timeout_ms` (기본 200) |

### Ft4222hDevice (`hal/driver/ft4222h/ft4222h_device.h`)

듀얼 칩 I2C 어댑터 드라이버입니다 (FTDI FT4222H).

```cpp
class Ft4222hDevice : public Device, public I2c {
    explicit Ft4222hDevice(const config::DeviceEntry& entry);
    static void Register();   // 드라이버 이름: "ft4222h"
};
```

| 항목 | 값 |
|------|-----|
| 드라이버 이름 | `ft4222h` |
| URI 형식 | `ft4222h://master_idx:slave_idx` (USB 디바이스 인덱스, 서로 달라야 함) |
| SDK 필요 | FT4222H SDK + D2XX (`PLAS_HAS_FT4222H`) |
| 구현 인터페이스 | `Device`, `I2c` |
| 설정 인수 | `bitrate` (기본 400000), `slave_addr` (기본 0x40), `sys_clock` (기본 60 MHz), `rx_timeout_ms` (기본 1000), `rx_poll_interval_us` (기본 100) |

### PciUtilsDevice (`hal/driver/pciutils/pciutils_device.h`)

libpci 기반 PCI config/DOE 드라이버입니다.

```cpp
class PciUtilsDevice : public Device, public pci::PciConfig, public pci::PciDoe {
    explicit PciUtilsDevice(const config::DeviceEntry& entry);
    static void Register();   // 드라이버 이름: "pciutils"
};
```

| 항목 | 값 |
|------|-----|
| 드라이버 이름 | `pciutils` |
| URI 형식 | `pciutils://DDDD:BB:DD.F` (도메인:버스:디바이스.기능) |
| SDK 필요 | libpci-dev (`PLAS_HAS_PCIUTILS`) |
| 구현 인터페이스 | `Device`, `PciConfig`, `PciDoe` |
| 설정 인수 | `doe_timeout_ms` (기본 1000), `doe_poll_interval_us` (기본 100) |

### Pmu3Device (`hal/driver/pmu3/pmu3_device.h`)

PMU3 전원/GPIO 드라이버입니다 (현재 스텁).

```cpp
class Pmu3Device : public Device, public PowerControl, public SsdGpio {
    explicit Pmu3Device(const config::DeviceEntry& entry);
    static void Register();   // 드라이버 이름: "pmu3"
};
```

| 항목 | 값 |
|------|-----|
| 드라이버 이름 | `pmu3` |
| URI 형식 | `pmu3://bus:id` |
| SDK 필요 | PMU3 SDK (`PLAS_HAS_PMU3`) |
| 구현 인터페이스 | `Device`, `PowerControl`, `SsdGpio` |
| 상태 | 스텁 — lifecycle만 동작, 기능 메서드 `kNotSupported` 반환 |

### Pmu4Device (`hal/driver/pmu4/pmu4_device.h`)

PMU4 전원/GPIO 드라이버입니다 (현재 스텁).

```cpp
class Pmu4Device : public Device, public PowerControl, public SsdGpio {
    explicit Pmu4Device(const config::DeviceEntry& entry);
    static void Register();   // 드라이버 이름: "pmu4"
};
```

| 항목 | 값 |
|------|-----|
| 드라이버 이름 | `pmu4` |
| URI 형식 | `pmu4://bus:id` |
| SDK 필요 | PMU4 SDK (`PLAS_HAS_PMU4`) |
| 구현 인터페이스 | `Device`, `PowerControl`, `SsdGpio` |
| 상태 | 스텁 — lifecycle만 동작, 기능 메서드 `kNotSupported` 반환 |

---

## 8. Bootstrap

### BootstrapConfig — `plas::bootstrap` (`bootstrap/bootstrap.h`)

```cpp
struct BootstrapConfig {
    std::string device_config_path;          // 디바이스 설정 파일 경로
    std::string device_config_key_path;      // 설정 내 서브트리 경로 (예: "plas.devices")
    ConfigFormat device_config_format = ConfigFormat::kAuto;

    std::optional<LogConfig> log_config;     // 로거 설정 (생략 시 초기화 안 함)

    std::string properties_config_path;      // Properties 파일 경로 (생략 시 로드 안 함)
    ConfigFormat properties_config_format = ConfigFormat::kAuto;

    bool auto_open_devices    = true;   // Init 후 자동 Open
    bool skip_unknown_drivers = true;   // 미등록 드라이버 건너뛰기
    bool skip_device_failures = true;   // 개별 디바이스 실패 건너뛰기
};
```

### DeviceFailure — `plas::bootstrap` (`bootstrap/bootstrap.h`)

```cpp
struct DeviceFailure {
    std::string nickname;        // 디바이스 닉네임
    std::string uri;             // URI
    std::string driver;          // 드라이버 이름
    std::error_code error;       // 에러 코드
    std::string phase;           // 실패 단계: "create", "init", "open"
    std::string detail;          // 사람이 읽을 수 있는 상세 설명
};
```

### BootstrapResult — `plas::bootstrap` (`bootstrap/bootstrap.h`)

```cpp
struct BootstrapResult {
    std::size_t devices_opened  = 0;   // 성공적으로 열린 수
    std::size_t devices_failed  = 0;   // 실패한 수
    std::size_t devices_skipped = 0;   // 건너뛴 수
    std::vector<DeviceFailure> failures;
};
```

### Bootstrap — `plas::bootstrap` (`bootstrap/bootstrap.h`)

단일 `Init()` 호출로 전체 앱 초기화를 수행합니다. move-only, RAII.

```cpp
class Bootstrap {
    Bootstrap();
    ~Bootstrap();   // Deinit() 자동 호출
    Bootstrap(Bootstrap&&) noexcept;
    Bootstrap& operator=(Bootstrap&&) noexcept;

    // 정적 유틸리티
    static void RegisterAllDrivers();                 // 사용 가능한 모든 드라이버 등록
    static bool ValidateUri(const std::string& uri);  // URI 형식 검증

    // 초기화 / 종료
    Result<BootstrapResult> Init(const BootstrapConfig& config);
    void Deinit();                                     // 역순 정리 (idempotent)
    bool IsInitialized() const;

    // 내부 매니저 접근
    DeviceManager* GetDeviceManager();
    PropertyManager* GetPropertyManager();

    // 디바이스 조회
    Device* GetDevice(const std::string& nickname);
    Device* GetDeviceByUri(const std::string& uri);
    template <typename T> T* GetInterface(const std::string& nickname);
    template <typename T> std::vector<std::pair<std::string, T*>> GetDevicesByInterface();

    // 쿼리
    std::vector<std::string> DeviceNames() const;
    const std::vector<DeviceFailure>& GetFailures() const;

    // 디버깅
    std::string DumpDevices() const;   // 로드된 디바이스 요약 문자열
};
```

**Init 시퀀스** (순서대로):
1. `RegisterAllDrivers()` — 사용 가능한 드라이버 등록
2. `Logger::Init()` — 로거 초기화 (설정된 경우)
3. `PropertyManager::LoadFromFile()` — Properties 로드 (설정된 경우)
4. `Config::LoadFromFile()` — 디바이스 설정 파싱
5. 디바이스별: `ValidateUri()` → `DeviceFactory::CreateFromConfig()` → `DeviceManager::AddDevice()` → `Init()` → `Open()`

**실패 처리**:
- `skip_unknown_drivers = true` → 미등록 드라이버는 건너뛰고 `failures`에 기록
- `skip_device_failures = true` → 개별 디바이스 실패는 건너뛰고 `failures`에 기록
- 위 옵션이 `false`이면 첫 번째 실패 시 롤백 후 에러 반환
