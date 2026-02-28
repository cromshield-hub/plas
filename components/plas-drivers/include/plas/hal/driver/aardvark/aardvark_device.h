#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "plas/hal/interface/device.h"
#include "plas/hal/interface/i2c.h"
#include "plas/config/device_entry.h"

namespace plas::hal::driver {

struct AardvarkBusState;  // defined in aardvark_device.cpp

class AardvarkDevice : public Device, public I2c {
public:
    explicit AardvarkDevice(const config::DeviceEntry& entry);
    ~AardvarkDevice() override;

    // Device interface
    core::Result<void> Init() override;
    core::Result<void> Open() override;
    core::Result<void> Close() override;
    core::Result<void> Reset() override;
    DeviceState GetState() const override;
    std::string GetName() const override;
    std::string GetUri() const override;

    // I2c interface
    core::Result<size_t> Read(core::Address addr, core::Byte* data,
                              size_t length, bool stop = true) override;
    core::Result<size_t> Write(core::Address addr, const core::Byte* data,
                               size_t length, bool stop = true) override;
    core::Result<size_t> WriteRead(core::Address addr,
                                   const core::Byte* write_data,
                                   size_t write_len, core::Byte* read_data,
                                   size_t read_len) override;
    core::Result<void> SetBitrate(uint32_t bitrate) override;
    uint32_t GetBitrate() const override;

    /// Register this driver with the DeviceFactory.
    static void Register();

    /// Reset the shared bus registry (for test isolation only).
    static void ResetBusRegistry();

private:
    static bool ParseUri(const std::string& uri, uint16_t& port,
                         uint16_t& addr);

    std::string name_;
    std::string uri_;
    DeviceState state_;
    uint32_t bitrate_;
    uint16_t port_;
    uint16_t default_addr_;
    bool pullup_enabled_;
    uint16_t bus_timeout_ms_;
    std::shared_ptr<AardvarkBusState> bus_state_;  // valid after Open()
};

}  // namespace plas::hal::driver
