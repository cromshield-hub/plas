#pragma once

#include <string>

#include "plas/hal/interface/device.h"
#include "plas/core/result.h"

namespace plas::hal::test {

class MockDevice : public Device {
public:
    explicit MockDevice(std::string name, std::string uri)
        : name_(std::move(name)), uri_(std::move(uri)), state_(DeviceState::kUninitialized) {}

    core::Result<void> Init() override {
        state_ = DeviceState::kInitialized;
        return core::Result<void>::Ok();
    }

    core::Result<void> Open() override {
        if (state_ != DeviceState::kInitialized && state_ != DeviceState::kClosed) {
            return core::Result<void>::Err(core::ErrorCode::kNotInitialized);
        }
        state_ = DeviceState::kOpen;
        return core::Result<void>::Ok();
    }

    core::Result<void> Close() override {
        if (state_ != DeviceState::kOpen) {
            return core::Result<void>::Err(core::ErrorCode::kAlreadyClosed);
        }
        state_ = DeviceState::kClosed;
        return core::Result<void>::Ok();
    }

    core::Result<void> Reset() override {
        state_ = DeviceState::kInitialized;
        return core::Result<void>::Ok();
    }

    DeviceState GetState() const override { return state_; }
    std::string GetName() const override { return name_; }
    std::string GetUri() const override { return uri_; }

private:
    std::string name_;
    std::string uri_;
    DeviceState state_;
};

}  // namespace plas::hal::test
