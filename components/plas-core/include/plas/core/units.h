#pragma once

namespace plas::core {

class Voltage {
public:
    constexpr explicit Voltage(double volts) : value_(volts) {}

    constexpr double Value() const { return value_; }

    constexpr bool operator==(const Voltage& other) const { return value_ == other.value_; }
    constexpr bool operator!=(const Voltage& other) const { return value_ != other.value_; }
    constexpr bool operator<(const Voltage& other) const { return value_ < other.value_; }
    constexpr bool operator<=(const Voltage& other) const { return value_ <= other.value_; }
    constexpr bool operator>(const Voltage& other) const { return value_ > other.value_; }
    constexpr bool operator>=(const Voltage& other) const { return value_ >= other.value_; }

private:
    double value_;
};

class Current {
public:
    constexpr explicit Current(double amps) : value_(amps) {}

    constexpr double Value() const { return value_; }

    constexpr bool operator==(const Current& other) const { return value_ == other.value_; }
    constexpr bool operator!=(const Current& other) const { return value_ != other.value_; }
    constexpr bool operator<(const Current& other) const { return value_ < other.value_; }
    constexpr bool operator<=(const Current& other) const { return value_ <= other.value_; }
    constexpr bool operator>(const Current& other) const { return value_ > other.value_; }
    constexpr bool operator>=(const Current& other) const { return value_ >= other.value_; }

private:
    double value_;
};

class Frequency {
public:
    constexpr explicit Frequency(double hertz) : value_(hertz) {}

    constexpr double Value() const { return value_; }

    constexpr bool operator==(const Frequency& other) const { return value_ == other.value_; }
    constexpr bool operator!=(const Frequency& other) const { return value_ != other.value_; }
    constexpr bool operator<(const Frequency& other) const { return value_ < other.value_; }
    constexpr bool operator<=(const Frequency& other) const { return value_ <= other.value_; }
    constexpr bool operator>(const Frequency& other) const { return value_ > other.value_; }
    constexpr bool operator>=(const Frequency& other) const { return value_ >= other.value_; }

private:
    double value_;
};

}  // namespace plas::core
