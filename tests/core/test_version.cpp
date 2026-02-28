#include <gtest/gtest.h>

#include <string>

#include "plas/core/version.h"

namespace plas::core {
namespace {

TEST(VersionTest, GetVersionReturnsValidStruct) {
    auto v = GetVersion();
    // Fields should be accessible (compilation test + basic sanity)
    EXPECT_GE(v.major, 0u);
    EXPECT_GE(v.minor, 0u);
    EXPECT_GE(v.patch, 0u);
}

TEST(VersionTest, GetVersionStringNotEmpty) {
    auto s = GetVersionString();
    EXPECT_FALSE(s.empty());
}

TEST(VersionTest, GetVersionStringContainsTwoDots) {
    auto s = GetVersionString();
    auto count = std::count(s.begin(), s.end(), '.');
    EXPECT_EQ(count, 2) << "Version string '" << s << "' should have format M.m.p";
}

TEST(VersionTest, GetVersionStringConsistentWithStruct) {
    auto v = GetVersion();
    auto s = GetVersionString();

    std::string expected = std::to_string(v.major) + "." +
                           std::to_string(v.minor) + "." +
                           std::to_string(v.patch);
    EXPECT_EQ(s, expected);
}

TEST(VersionTest, VersionStructIsCopyable) {
    auto v = GetVersion();
    Version copy = v;
    EXPECT_EQ(copy.major, v.major);
    EXPECT_EQ(copy.minor, v.minor);
    EXPECT_EQ(copy.patch, v.patch);
}

TEST(VersionTest, GetVersionCalledTwiceIsSame) {
    auto v1 = GetVersion();
    auto v2 = GetVersion();
    EXPECT_EQ(v1.major, v2.major);
    EXPECT_EQ(v1.minor, v2.minor);
    EXPECT_EQ(v1.patch, v2.patch);
}

TEST(VersionTest, GetVersionStringCalledTwiceIsSame) {
    auto s1 = GetVersionString();
    auto s2 = GetVersionString();
    EXPECT_EQ(s1, s2);
}

TEST(VersionTest, VersionStringStartsWithMajor) {
    auto v = GetVersion();
    auto s = GetVersionString();
    auto prefix = std::to_string(v.major) + ".";
    EXPECT_EQ(s.substr(0, prefix.size()), prefix);
}

}  // namespace
}  // namespace plas::core
