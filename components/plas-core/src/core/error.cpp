#include "plas/core/error.h"

namespace plas::core {

const PlasErrorCategory& PlasErrorCategory::Instance() {
    static const PlasErrorCategory instance;
    return instance;
}

const char* PlasErrorCategory::name() const noexcept {
    return "plas";
}

std::string PlasErrorCategory::message(int code) const {
    switch (static_cast<ErrorCode>(code)) {
        case ErrorCode::kSuccess:
            return "Success";
        case ErrorCode::kInvalidArgument:
            return "Invalid argument";
        case ErrorCode::kNotFound:
            return "Not found";
        case ErrorCode::kTimeout:
            return "Timeout";
        case ErrorCode::kBusy:
            return "Busy";
        case ErrorCode::kIOError:
            return "I/O error";
        case ErrorCode::kNotSupported:
            return "Not supported";
        case ErrorCode::kNotInitialized:
            return "Not initialized";
        case ErrorCode::kAlreadyOpen:
            return "Already open";
        case ErrorCode::kAlreadyClosed:
            return "Already closed";
        case ErrorCode::kPermissionDenied:
            return "Permission denied";
        case ErrorCode::kOutOfMemory:
            return "Out of memory";
        case ErrorCode::kTypeMismatch:
            return "Type mismatch";
        case ErrorCode::kOutOfRange:
            return "Out of range";
        case ErrorCode::kOverflow:
            return "Overflow";
        case ErrorCode::kResourceExhausted:
            return "Resource exhausted";
        case ErrorCode::kCancelled:
            return "Cancelled";
        case ErrorCode::kDataLoss:
            return "Data loss";
        case ErrorCode::kInternalError:
            return "Internal error";
        case ErrorCode::kUnknown:
            return "Unknown error";
        default:
            return "Unrecognized error code";
    }
}

}  // namespace plas::core
