#pragma once

#include <string>
#include <system_error>

namespace plas::core {

enum class ErrorCode : int {
    kSuccess = 0,
    kInvalidArgument,
    kNotFound,
    kTimeout,
    kBusy,
    kIOError,
    kNotSupported,
    kNotInitialized,
    kAlreadyOpen,
    kAlreadyClosed,
    kPermissionDenied,
    kOutOfMemory,
    kInternalError,
    kUnknown
};

class PlasErrorCategory : public std::error_category {
public:
    static const PlasErrorCategory& Instance();

    const char* name() const noexcept override;
    std::string message(int code) const override;

private:
    PlasErrorCategory() = default;
};

inline std::error_code make_error_code(ErrorCode code) {
    return {static_cast<int>(code), PlasErrorCategory::Instance()};
}

}  // namespace plas::core

namespace std {

template <>
struct is_error_code_enum<plas::core::ErrorCode> : true_type {};

}  // namespace std
