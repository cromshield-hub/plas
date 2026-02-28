#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "plas/hal/interface/device.h"
#include "plas/hal/interface/i2c.h"
#include "plas/config/device_entry.h"

namespace plas::hal::driver {

class Ft4222hDevice : public Device, public I2c {
public:
    explicit Ft4222hDevice(const config::DeviceEntry& entry);
    ~Ft4222hDevice() override;

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
                              size_t length) override;
    core::Result<size_t> Write(core::Address addr, const core::Byte* data,
                               size_t length) override;
    core::Result<size_t> WriteRead(core::Address addr,
                                   const core::Byte* write_data,
                                   size_t write_len, core::Byte* read_data,
                                   size_t read_len) override;
    core::Result<void> SetBitrate(uint32_t bitrate) override;
    uint32_t GetBitrate() const override;

    /// Register this driver with the DeviceFactory.
    static void Register();

private:
    static bool ParseUri(const std::string& uri, uint16_t& master_idx,
                         uint16_t& slave_idx);

    core::Result<uint16_t> PollSlaveRx(size_t expected_len);

    std::string name_;
    std::string uri_;
    DeviceState state_;

    uint16_t master_idx_;
    uint16_t slave_idx_;

    void* master_handle_;
    void* slave_handle_;

    uint32_t bitrate_;
    uint8_t slave_addr_;
    uint32_t sys_clock_;
    uint32_t rx_timeout_ms_;
    uint32_t rx_poll_interval_us_;
    std::mutex i2c_mutex_;
};

}  // namespace plas::hal::driver
