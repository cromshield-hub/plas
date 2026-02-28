#pragma once

#include <cstdint>
#include <string>

#include "plas/backend/interface/device.h"
#include "plas/backend/interface/i2c.h"
#include "plas/config/device_entry.h"

namespace plas::backend::driver {

class Ft4222hDevice : public Device, public I2c {
public:
    explicit Ft4222hDevice(const config::DeviceEntry& entry);
    ~Ft4222hDevice() override = default;

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
    std::string name_;
    std::string uri_;
    DeviceState state_;
    uint32_t bitrate_;
    int handle_;
};

}  // namespace plas::backend::driver
