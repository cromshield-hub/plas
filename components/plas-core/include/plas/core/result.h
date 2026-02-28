#pragma once

#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

#include "plas/core/error.h"

namespace plas::core {

template <typename T>
class Result {
public:
    static Result Ok(T value) {
        return Result(std::variant<T, std::error_code>(std::in_place_index<0>, std::move(value)));
    }

    static Result Err(std::error_code error) {
        return Result(std::variant<T, std::error_code>(std::in_place_index<1>, error));
    }

    static Result Err(ErrorCode code) {
        return Err(make_error_code(code));
    }

    bool IsOk() const {
        return std::holds_alternative<T>(storage_);
    }

    bool IsError() const {
        return std::holds_alternative<std::error_code>(storage_);
    }

    T& Value() & {
        return std::get<T>(storage_);
    }

    const T& Value() const& {
        return std::get<T>(storage_);
    }

    T&& Value() && {
        return std::get<T>(std::move(storage_));
    }

    std::error_code Error() const {
        return std::get<std::error_code>(storage_);
    }

private:
    explicit Result(std::variant<T, std::error_code> storage)
        : storage_(std::move(storage)) {}

    std::variant<T, std::error_code> storage_;
};

template <>
class Result<void> {
public:
    static Result Ok() {
        Result result;
        result.storage_ = std::monostate{};
        return result;
    }

    static Result Err(std::error_code error) {
        Result result;
        result.storage_ = error;
        return result;
    }

    static Result Err(ErrorCode code) {
        return Err(make_error_code(code));
    }

    bool IsOk() const {
        return std::holds_alternative<std::monostate>(storage_);
    }

    bool IsError() const {
        return std::holds_alternative<std::error_code>(storage_);
    }

    std::error_code Error() const {
        return std::get<std::error_code>(storage_);
    }

private:
    Result() = default;

    std::variant<std::monostate, std::error_code> storage_;
};

}  // namespace plas::core
