#include <gtest/gtest.h>

#include "plas/core/units.h"

using plas::core::Current;
using plas::core::Frequency;
using plas::core::Voltage;

TEST(VoltageTest, ConstructAndGet) {
    constexpr Voltage v(3.3);
    EXPECT_DOUBLE_EQ(v.Value(), 3.3);
}

TEST(VoltageTest, Comparison) {
    constexpr Voltage v1(3.3);
    constexpr Voltage v2(5.0);
    constexpr Voltage v3(3.3);

    EXPECT_EQ(v1, v3);
    EXPECT_NE(v1, v2);
    EXPECT_LT(v1, v2);
    EXPECT_LE(v1, v3);
    EXPECT_GT(v2, v1);
    EXPECT_GE(v2, v1);
}

TEST(CurrentTest, ConstructAndGet) {
    constexpr Current c(1.5);
    EXPECT_DOUBLE_EQ(c.Value(), 1.5);
}

TEST(CurrentTest, Comparison) {
    constexpr Current c1(0.5);
    constexpr Current c2(2.0);

    EXPECT_LT(c1, c2);
    EXPECT_GT(c2, c1);
    EXPECT_NE(c1, c2);
}

TEST(FrequencyTest, ConstructAndGet) {
    constexpr Frequency f(100e6);
    EXPECT_DOUBLE_EQ(f.Value(), 100e6);
}

TEST(FrequencyTest, Comparison) {
    constexpr Frequency f1(100.0);
    constexpr Frequency f2(200.0);
    constexpr Frequency f3(100.0);

    EXPECT_EQ(f1, f3);
    EXPECT_LT(f1, f2);
    EXPECT_GE(f2, f1);
}

TEST(VoltageTest, ExplicitConstruction) {
    // Voltage v = 3.3;  // Should not compile — explicit constructor
    Voltage v(3.3);
    EXPECT_DOUBLE_EQ(v.Value(), 3.3);
}

TEST(UnitsTest, Constexpr) {
    constexpr Voltage v(1.8);
    constexpr Current c(0.1);
    constexpr Frequency f(400e3);

    static_assert(v.Value() == 1.8, "Voltage should be constexpr");
    static_assert(c.Value() == 0.1, "Current should be constexpr");
    static_assert(f.Value() == 400e3, "Frequency should be constexpr");
}
