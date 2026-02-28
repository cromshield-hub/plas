#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "plas/core/properties.h"

using plas::core::ErrorCode;
using plas::core::Properties;
using plas::core::Result;

// Clean up all sessions between tests
class PropertiesTest : public ::testing::Test {
protected:
    void TearDown() override { Properties::DestroyAll(); }
};

// ========================== Session Management ==========================

TEST_F(PropertiesTest, GetSessionCreatesNew) {
    EXPECT_FALSE(Properties::HasSession("s1"));
    auto& s = Properties::GetSession("s1");
    EXPECT_TRUE(Properties::HasSession("s1"));
    EXPECT_EQ(s.Size(), 0u);
}

TEST_F(PropertiesTest, GetSessionReturnsSame) {
    auto& a = Properties::GetSession("s1");
    auto& b = Properties::GetSession("s1");
    EXPECT_EQ(&a, &b);
}

TEST_F(PropertiesTest, HasSessionReturnsFalseForMissing) {
    EXPECT_FALSE(Properties::HasSession("nonexistent"));
}

TEST_F(PropertiesTest, DestroySessionRemovesIt) {
    Properties::GetSession("s1");
    EXPECT_TRUE(Properties::HasSession("s1"));
    Properties::DestroySession("s1");
    EXPECT_FALSE(Properties::HasSession("s1"));
}

TEST_F(PropertiesTest, DestroySessionNonexistentIsNoOp) {
    Properties::DestroySession("ghost");  // should not throw
}

TEST_F(PropertiesTest, DestroyAllClearsAll) {
    Properties::GetSession("a");
    Properties::GetSession("b");
    Properties::DestroyAll();
    EXPECT_FALSE(Properties::HasSession("a"));
    EXPECT_FALSE(Properties::HasSession("b"));
}

TEST_F(PropertiesTest, SessionsAreIndependent) {
    auto& s1 = Properties::GetSession("s1");
    auto& s2 = Properties::GetSession("s2");
    s1.Set<int>("key", 1);
    s2.Set<int>("key", 2);
    EXPECT_EQ(s1.Get<int>("key").Value(), 1);
    EXPECT_EQ(s2.Get<int>("key").Value(), 2);
}

// ========================== Get / Set ==========================

TEST_F(PropertiesTest, SetAndGetInt) {
    auto& p = Properties::GetSession("test");
    p.Set<int>("x", 42);
    auto r = p.Get<int>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), 42);
}

TEST_F(PropertiesTest, SetAndGetString) {
    auto& p = Properties::GetSession("test");
    p.Set<std::string>("name", "hello");
    auto r = p.Get<std::string>("name");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), "hello");
}

TEST_F(PropertiesTest, SetAndGetDouble) {
    auto& p = Properties::GetSession("test");
    p.Set<double>("pi", 3.14);
    auto r = p.Get<double>("pi");
    EXPECT_TRUE(r.IsOk());
    EXPECT_DOUBLE_EQ(r.Value(), 3.14);
}

TEST_F(PropertiesTest, SetAndGetBool) {
    auto& p = Properties::GetSession("test");
    p.Set<bool>("flag", true);
    auto r = p.Get<bool>("flag");
    EXPECT_TRUE(r.IsOk());
    EXPECT_TRUE(r.Value());
}

TEST_F(PropertiesTest, SetAndGetUint64) {
    auto& p = Properties::GetSession("test");
    uint64_t big = 0xFFFFFFFFFFFFFFFFull;
    p.Set<uint64_t>("big", big);
    auto r = p.Get<uint64_t>("big");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), big);
}

TEST_F(PropertiesTest, SetOverwritesValue) {
    auto& p = Properties::GetSession("test");
    p.Set<int>("x", 1);
    p.Set<int>("x", 2);
    EXPECT_EQ(p.Get<int>("x").Value(), 2);
}

TEST_F(PropertiesTest, SetOverwritesWithDifferentType) {
    auto& p = Properties::GetSession("test");
    p.Set<int>("x", 1);
    p.Set<std::string>("x", "now a string");
    EXPECT_TRUE(p.Get<std::string>("x").IsOk());
    EXPECT_EQ(p.Get<std::string>("x").Value(), "now a string");
}

TEST_F(PropertiesTest, GetNotFoundReturnsError) {
    auto& p = Properties::GetSession("test");
    auto r = p.Get<int>("missing");
    EXPECT_TRUE(r.IsError());
    EXPECT_EQ(r.Error(), make_error_code(ErrorCode::kNotFound));
}

TEST_F(PropertiesTest, GetTypeMismatchReturnsError) {
    auto& p = Properties::GetSession("test");
    p.Set<int>("x", 42);
    auto r = p.Get<std::string>("x");
    EXPECT_TRUE(r.IsError());
    EXPECT_EQ(r.Error(), make_error_code(ErrorCode::kTypeMismatch));
}

struct CustomType {
    int a;
    std::string b;
    bool operator==(const CustomType& o) const { return a == o.a && b == o.b; }
};

TEST_F(PropertiesTest, SetAndGetCustomType) {
    auto& p = Properties::GetSession("test");
    p.Set<CustomType>("obj", {10, "hello"});
    auto r = p.Get<CustomType>("obj");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), (CustomType{10, "hello"}));
}

// ========================== Has / Remove / Clear / Size / Keys ==========================

TEST_F(PropertiesTest, HasReturnsTrueForExistingKey) {
    auto& p = Properties::GetSession("test");
    p.Set<int>("x", 1);
    EXPECT_TRUE(p.Has("x"));
}

TEST_F(PropertiesTest, HasReturnsFalseForMissingKey) {
    auto& p = Properties::GetSession("test");
    EXPECT_FALSE(p.Has("missing"));
}

TEST_F(PropertiesTest, RemoveDeletesKey) {
    auto& p = Properties::GetSession("test");
    p.Set<int>("x", 1);
    p.Remove("x");
    EXPECT_FALSE(p.Has("x"));
}

TEST_F(PropertiesTest, RemoveNonexistentIsNoOp) {
    auto& p = Properties::GetSession("test");
    p.Remove("ghost");  // should not throw
}

TEST_F(PropertiesTest, ClearRemovesAll) {
    auto& p = Properties::GetSession("test");
    p.Set<int>("a", 1);
    p.Set<int>("b", 2);
    p.Clear();
    EXPECT_EQ(p.Size(), 0u);
    EXPECT_FALSE(p.Has("a"));
}

TEST_F(PropertiesTest, SizeReflectsEntries) {
    auto& p = Properties::GetSession("test");
    EXPECT_EQ(p.Size(), 0u);
    p.Set<int>("a", 1);
    EXPECT_EQ(p.Size(), 1u);
    p.Set<int>("b", 2);
    EXPECT_EQ(p.Size(), 2u);
    p.Remove("a");
    EXPECT_EQ(p.Size(), 1u);
}

TEST_F(PropertiesTest, KeysReturnsSortedList) {
    auto& p = Properties::GetSession("test");
    p.Set<int>("c", 3);
    p.Set<int>("a", 1);
    p.Set<int>("b", 2);
    auto keys = p.Keys();
    // std::map keeps keys sorted
    EXPECT_EQ(keys, (std::vector<std::string>{"a", "b", "c"}));
}

// ========================== GetAs Numeric Conversions ==========================

TEST_F(PropertiesTest, GetAsExactTypeMatch) {
    auto& p = Properties::GetSession("test");
    p.Set<int32_t>("x", 42);
    auto r = p.GetAs<int32_t>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), 42);
}

TEST_F(PropertiesTest, GetAsWideningIntConversion) {
    auto& p = Properties::GetSession("test");
    p.Set<int16_t>("x", 100);
    auto r = p.GetAs<int64_t>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), 100);
}

TEST_F(PropertiesTest, GetAsNarrowingIntConversionSuccess) {
    auto& p = Properties::GetSession("test");
    p.Set<int64_t>("x", 127);
    auto r = p.GetAs<int8_t>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), 127);
}

TEST_F(PropertiesTest, GetAsNarrowingIntConversionOverflow) {
    auto& p = Properties::GetSession("test");
    p.Set<int64_t>("x", 1000);
    auto r = p.GetAs<int8_t>("x");
    EXPECT_TRUE(r.IsError());
    EXPECT_EQ(r.Error(), make_error_code(ErrorCode::kTypeMismatch));
}

TEST_F(PropertiesTest, GetAsSignedToUnsignedSuccess) {
    auto& p = Properties::GetSession("test");
    p.Set<int32_t>("x", 42);
    auto r = p.GetAs<uint32_t>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), 42u);
}

TEST_F(PropertiesTest, GetAsNegativeToUnsignedFails) {
    auto& p = Properties::GetSession("test");
    p.Set<int32_t>("x", -1);
    auto r = p.GetAs<uint32_t>("x");
    EXPECT_TRUE(r.IsError());
    EXPECT_EQ(r.Error(), make_error_code(ErrorCode::kTypeMismatch));
}

TEST_F(PropertiesTest, GetAsUnsignedToSignedSuccess) {
    auto& p = Properties::GetSession("test");
    p.Set<uint32_t>("x", 100u);
    auto r = p.GetAs<int32_t>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), 100);
}

TEST_F(PropertiesTest, GetAsUnsignedToSignedOverflow) {
    auto& p = Properties::GetSession("test");
    p.Set<uint64_t>("x", uint64_t(0xFFFFFFFFFFFFFFFFull));
    auto r = p.GetAs<int32_t>("x");
    EXPECT_TRUE(r.IsError());
}

TEST_F(PropertiesTest, GetAsDoubleToInt) {
    auto& p = Properties::GetSession("test");
    p.Set<double>("x", 3.14);
    auto r = p.GetAs<int32_t>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), 3);
}

TEST_F(PropertiesTest, GetAsDoubleToIntOverflow) {
    auto& p = Properties::GetSession("test");
    p.Set<double>("x", 1e18);
    auto r = p.GetAs<int32_t>("x");
    EXPECT_TRUE(r.IsError());
}

TEST_F(PropertiesTest, GetAsIntToDouble) {
    auto& p = Properties::GetSession("test");
    p.Set<int32_t>("x", 42);
    auto r = p.GetAs<double>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_DOUBLE_EQ(r.Value(), 42.0);
}

TEST_F(PropertiesTest, GetAsBoolToInt) {
    auto& p = Properties::GetSession("test");
    p.Set<bool>("flag", true);
    auto r = p.GetAs<int32_t>("flag");
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value(), 1);
}

TEST_F(PropertiesTest, GetAsIntToBool) {
    auto& p = Properties::GetSession("test");
    p.Set<int32_t>("x", 0);
    auto r = p.GetAs<bool>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_FALSE(r.Value());
}

TEST_F(PropertiesTest, GetAsNonNumericTypeFails) {
    auto& p = Properties::GetSession("test");
    p.Set<std::string>("x", "hello");
    auto r = p.GetAs<int32_t>("x");
    EXPECT_TRUE(r.IsError());
    EXPECT_EQ(r.Error(), make_error_code(ErrorCode::kTypeMismatch));
}

TEST_F(PropertiesTest, GetAsNotFoundKey) {
    auto& p = Properties::GetSession("test");
    auto r = p.GetAs<int32_t>("missing");
    EXPECT_TRUE(r.IsError());
    EXPECT_EQ(r.Error(), make_error_code(ErrorCode::kNotFound));
}

TEST_F(PropertiesTest, GetAsPlasTypeAliases) {
    auto& p = Properties::GetSession("test");
    p.Set<uint8_t>("byte", 0xFF);
    p.Set<uint16_t>("word", 0x1234);
    p.Set<uint32_t>("dword", 0xDEADBEEF);
    p.Set<uint64_t>("qword", 0x123456789ABCDEFull);

    // Byte → uint16_t
    auto r1 = p.GetAs<uint16_t>("byte");
    EXPECT_TRUE(r1.IsOk());
    EXPECT_EQ(r1.Value(), 0xFF);

    // DWord → uint64_t
    auto r2 = p.GetAs<uint64_t>("dword");
    EXPECT_TRUE(r2.IsOk());
    EXPECT_EQ(r2.Value(), 0xDEADBEEF);
}

TEST_F(PropertiesTest, GetAsFloatToDouble) {
    auto& p = Properties::GetSession("test");
    p.Set<float>("x", 1.5f);
    auto r = p.GetAs<double>("x");
    EXPECT_TRUE(r.IsOk());
    EXPECT_DOUBLE_EQ(r.Value(), 1.5);
}

// ========================== Thread Safety ==========================

TEST_F(PropertiesTest, ConcurrentSetAndGet) {
    auto& p = Properties::GetSession("concurrent");
    constexpr int kIterations = 1000;

    std::thread writer([&] {
        for (int i = 0; i < kIterations; ++i) {
            p.Set<int>("counter", i);
        }
    });

    std::thread reader([&] {
        for (int i = 0; i < kIterations; ++i) {
            auto r = p.Get<int>("counter");
            // Either not found (not yet set) or valid int
            if (r.IsOk()) {
                EXPECT_GE(r.Value(), 0);
                EXPECT_LT(r.Value(), kIterations);
            }
        }
    });

    writer.join();
    reader.join();
}

TEST_F(PropertiesTest, ConcurrentSessionAccess) {
    constexpr int kThreads = 8;
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([t] {
            std::string name = "session_" + std::to_string(t);
            auto& p = Properties::GetSession(name);
            p.Set<int>("id", t);
            EXPECT_EQ(p.Get<int>("id").Value(), t);
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    for (int t = 0; t < kThreads; ++t) {
        std::string name = "session_" + std::to_string(t);
        EXPECT_TRUE(Properties::HasSession(name));
    }
}

TEST_F(PropertiesTest, ConcurrentReads) {
    auto& p = Properties::GetSession("reads");
    p.Set<int>("value", 42);

    constexpr int kThreads = 8;
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 1000; ++i) {
                auto r = p.Get<int>("value");
                EXPECT_TRUE(r.IsOk());
                EXPECT_EQ(r.Value(), 42);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
}
