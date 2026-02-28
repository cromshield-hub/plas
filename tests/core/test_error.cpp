#include <gtest/gtest.h>

#include <system_error>

#include "plas/core/error.h"

using plas::core::ErrorCode;
using plas::core::PlasErrorCategory;
using plas::core::make_error_code;

TEST(ErrorCodeTest, SuccessIsZero) {
    EXPECT_EQ(static_cast<int>(ErrorCode::kSuccess), 0);
}

TEST(ErrorCodeTest, ImplicitConversionToErrorCode) {
    std::error_code ec = ErrorCode::kTimeout;
    EXPECT_TRUE(ec.operator bool());
    EXPECT_EQ(ec.value(), static_cast<int>(ErrorCode::kTimeout));
    EXPECT_EQ(&ec.category(), &PlasErrorCategory::Instance());
}

TEST(ErrorCodeTest, SuccessIsNotAnError) {
    std::error_code ec = ErrorCode::kSuccess;
    EXPECT_FALSE(ec.operator bool());
}

TEST(ErrorCodeTest, CategoryName) {
    EXPECT_STREQ(PlasErrorCategory::Instance().name(), "plas");
}

TEST(ErrorCodeTest, AllCodesHaveMessages) {
    const auto& cat = PlasErrorCategory::Instance();
    for (int i = 0; i <= static_cast<int>(ErrorCode::kUnknown); ++i) {
        std::string msg = cat.message(i);
        EXPECT_FALSE(msg.empty()) << "ErrorCode " << i << " has empty message";
    }
}

TEST(ErrorCodeTest, MakeErrorCode) {
    auto ec = make_error_code(ErrorCode::kIOError);
    EXPECT_EQ(ec.value(), static_cast<int>(ErrorCode::kIOError));
    EXPECT_EQ(&ec.category(), &PlasErrorCategory::Instance());
}

TEST(ErrorCodeTest, DifferentCodesAreNotEqual) {
    std::error_code ec1 = ErrorCode::kTimeout;
    std::error_code ec2 = ErrorCode::kBusy;
    EXPECT_NE(ec1, ec2);
}

TEST(ErrorCodeTest, SameCodesAreEqual) {
    std::error_code ec1 = ErrorCode::kNotFound;
    std::error_code ec2 = ErrorCode::kNotFound;
    EXPECT_EQ(ec1, ec2);
}
