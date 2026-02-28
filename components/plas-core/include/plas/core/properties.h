#pragma once

#include <any>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "plas/core/error.h"
#include "plas/core/result.h"

namespace plas::core {

class Properties {
public:
    // --- Session management (static, thread-safe) ---
    static Properties& GetSession(const std::string& name);
    static void DestroySession(const std::string& name);
    static void DestroyAll();
    static bool HasSession(const std::string& name);

    // --- Key-Value access (per-session, thread-safe) ---
    template <typename T>
    void Set(const std::string& key, T value);

    template <typename T>
    Result<T> Get(const std::string& key) const;

    template <typename To>
    Result<To> GetAs(const std::string& key) const;

    bool Has(const std::string& key) const;
    void Remove(const std::string& key);
    void Clear();
    size_t Size() const;
    std::vector<std::string> Keys() const;

    Properties(const Properties&) = delete;
    Properties& operator=(const Properties&) = delete;

private:
    Properties() = default;

    // --- Session registry ---
    static std::mutex& RegistryMutex();
    static std::map<std::string, std::unique_ptr<Properties>>& Registry();

    // --- Per-session storage ---
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::any> store_;
};

// --- SafeNumericCast: range-checked numeric conversion ---
namespace detail {

template <typename To, typename From>
constexpr bool SafeNumericCast(From from, To& out) {
    static_assert(std::is_arithmetic_v<To> && std::is_arithmetic_v<From>);

    // bool → numeric: always safe (0 or 1)
    if constexpr (std::is_same_v<From, bool>) {
        out = static_cast<To>(from);
        return true;
    }
    // numeric → bool
    else if constexpr (std::is_same_v<To, bool>) {
        out = (from != From{0});
        return true;
    }
    // float/double → integral: check range
    else if constexpr (std::is_floating_point_v<From> && std::is_integral_v<To>) {
        if (from < static_cast<From>(std::numeric_limits<To>::min()) ||
            from > static_cast<From>(std::numeric_limits<To>::max())) {
            return false;
        }
        out = static_cast<To>(from);
        return true;
    }
    // integral → float/double: always representable (may lose precision)
    else if constexpr (std::is_integral_v<From> && std::is_floating_point_v<To>) {
        out = static_cast<To>(from);
        return true;
    }
    // float ↔ double
    else if constexpr (std::is_floating_point_v<From> && std::is_floating_point_v<To>) {
        if constexpr (sizeof(To) < sizeof(From)) {
            if (from < static_cast<From>(std::numeric_limits<To>::lowest()) ||
                from > static_cast<From>(std::numeric_limits<To>::max())) {
                return false;
            }
        }
        out = static_cast<To>(from);
        return true;
    }
    // signed → unsigned
    else if constexpr (std::is_signed_v<From> && std::is_unsigned_v<To>) {
        if (from < 0) return false;
        auto ufrom = static_cast<std::make_unsigned_t<From>>(from);
        if (ufrom > std::numeric_limits<To>::max()) return false;
        out = static_cast<To>(ufrom);
        return true;
    }
    // unsigned → signed
    else if constexpr (std::is_unsigned_v<From> && std::is_signed_v<To>) {
        if (from > static_cast<std::make_unsigned_t<To>>(std::numeric_limits<To>::max())) {
            return false;
        }
        out = static_cast<To>(from);
        return true;
    }
    // same signedness, integral → integral
    else {
        if constexpr (sizeof(From) > sizeof(To)) {
            if (from < static_cast<From>(std::numeric_limits<To>::min()) ||
                from > static_cast<From>(std::numeric_limits<To>::max())) {
                return false;
            }
        }
        out = static_cast<To>(from);
        return true;
    }
}

// Try to extract `from` as type `From`, then safe-cast to `To`.
template <typename To, typename From>
bool TryCastFrom(const std::any& from, To& out) {
    auto* p = std::any_cast<From>(&from);
    if (!p) return false;
    return SafeNumericCast<To>(*p, out);
}

// Try all numeric source types to cast to `To`.
template <typename To>
bool TryNumericCast(const std::any& from, To& out) {
    return TryCastFrom<To, bool>(from, out) ||
           TryCastFrom<To, int8_t>(from, out) ||
           TryCastFrom<To, uint8_t>(from, out) ||
           TryCastFrom<To, int16_t>(from, out) ||
           TryCastFrom<To, uint16_t>(from, out) ||
           TryCastFrom<To, int32_t>(from, out) ||
           TryCastFrom<To, uint32_t>(from, out) ||
           TryCastFrom<To, int64_t>(from, out) ||
           TryCastFrom<To, uint64_t>(from, out) ||
           TryCastFrom<To, float>(from, out) ||
           TryCastFrom<To, double>(from, out);
}

}  // namespace detail

// --- Template implementations ---

template <typename T>
void Properties::Set(const std::string& key, T value) {
    std::unique_lock lock(mutex_);
    store_.insert_or_assign(key, std::any(std::move(value)));
}

template <typename T>
Result<T> Properties::Get(const std::string& key) const {
    std::shared_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) {
        return Result<T>::Err(ErrorCode::kNotFound);
    }
    auto* val = std::any_cast<T>(&it->second);
    if (!val) {
        return Result<T>::Err(ErrorCode::kTypeMismatch);
    }
    return Result<T>::Ok(*val);
}

template <typename To>
Result<To> Properties::GetAs(const std::string& key) const {
    static_assert(std::is_arithmetic_v<To>,
                  "GetAs<To> requires To to be an arithmetic type");

    std::shared_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) {
        return Result<To>::Err(ErrorCode::kNotFound);
    }

    // Exact type match
    if (auto* val = std::any_cast<To>(&it->second)) {
        return Result<To>::Ok(*val);
    }

    // Try numeric conversion from all known source types
    To out{};
    if (detail::TryNumericCast<To>(it->second, out)) {
        return Result<To>::Ok(out);
    }

    // Stored value is not a numeric type
    return Result<To>::Err(ErrorCode::kTypeMismatch);
}

}  // namespace plas::core
