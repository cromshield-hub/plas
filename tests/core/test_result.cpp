#include <gtest/gtest.h>

#include <string>

#include "plas/core/result.h"

using plas::core::ErrorCode;
using plas::core::Result;

TEST(ResultTest, OkHoldsValue) {
    auto result = Result<int>::Ok(42);
    EXPECT_TRUE(result.IsOk());
    EXPECT_FALSE(result.IsError());
    EXPECT_EQ(result.Value(), 42);
}

TEST(ResultTest, ErrHoldsErrorCode) {
    auto result = Result<int>::Err(ErrorCode::kTimeout);
    EXPECT_FALSE(result.IsOk());
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(), make_error_code(ErrorCode::kTimeout));
}

TEST(ResultTest, ErrFromStdErrorCode) {
    std::error_code ec = ErrorCode::kNotFound;
    auto result = Result<int>::Err(ec);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(), ec);
}

TEST(ResultTest, OkWithString) {
    auto result = Result<std::string>::Ok("hello");
    EXPECT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), "hello");
}

TEST(ResultTest, ConstAccess) {
    const auto result = Result<int>::Ok(99);
    EXPECT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 99);
}

TEST(ResultTest, MoveValue) {
    auto result = Result<std::string>::Ok("moved");
    std::string val = std::move(result).Value();
    EXPECT_EQ(val, "moved");
}

TEST(ResultVoidTest, OkVoid) {
    auto result = Result<void>::Ok();
    EXPECT_TRUE(result.IsOk());
    EXPECT_FALSE(result.IsError());
}

TEST(ResultVoidTest, ErrVoid) {
    auto result = Result<void>::Err(ErrorCode::kIOError);
    EXPECT_FALSE(result.IsOk());
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(), make_error_code(ErrorCode::kIOError));
}

TEST(ResultVoidTest, ErrVoidFromStdErrorCode) {
    std::error_code ec = ErrorCode::kInternalError;
    auto result = Result<void>::Err(ec);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.Error(), ec);
}
