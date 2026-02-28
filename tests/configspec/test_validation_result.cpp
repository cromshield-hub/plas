#include <gtest/gtest.h>

#include "plas/configspec/validation_result.h"

using plas::configspec::Severity;
using plas::configspec::ValidationIssue;
using plas::configspec::ValidationResult;

TEST(ValidationResult, DefaultIsValid) {
    ValidationResult vr;
    EXPECT_TRUE(vr.valid);
    EXPECT_TRUE(vr.issues.empty());
}

TEST(ValidationResult, ErrorsFilterOnlyErrors) {
    ValidationResult vr;
    vr.issues.push_back({Severity::kError, "/a", "err1"});
    vr.issues.push_back({Severity::kWarning, "/b", "warn1"});
    vr.issues.push_back({Severity::kError, "/c", "err2"});

    auto errors = vr.Errors();
    EXPECT_EQ(errors.size(), 2u);
    EXPECT_EQ(errors[0].path, "/a");
    EXPECT_EQ(errors[1].path, "/c");
}

TEST(ValidationResult, WarningsFilterOnlyWarnings) {
    ValidationResult vr;
    vr.issues.push_back({Severity::kError, "/a", "err1"});
    vr.issues.push_back({Severity::kWarning, "/b", "warn1"});

    auto warnings = vr.Warnings();
    EXPECT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0].path, "/b");
}

TEST(ValidationResult, SummaryValid) {
    ValidationResult vr;
    vr.valid = true;
    auto summary = vr.Summary();
    EXPECT_NE(summary.find("valid"), std::string::npos);
    EXPECT_NE(summary.find("0 error(s)"), std::string::npos);
    EXPECT_NE(summary.find("0 warning(s)"), std::string::npos);
}

TEST(ValidationResult, SummaryInvalid) {
    ValidationResult vr;
    vr.valid = false;
    vr.issues.push_back({Severity::kError, "/foo", "type mismatch"});
    auto summary = vr.Summary();
    EXPECT_NE(summary.find("invalid"), std::string::npos);
    EXPECT_NE(summary.find("1 error(s)"), std::string::npos);
    EXPECT_NE(summary.find("[ERROR]"), std::string::npos);
    EXPECT_NE(summary.find("/foo"), std::string::npos);
    EXPECT_NE(summary.find("type mismatch"), std::string::npos);
}

TEST(ValidationResult, SummaryContainsWarningTag) {
    ValidationResult vr;
    vr.valid = true;
    vr.issues.push_back({Severity::kWarning, "/bar", "deprecated field"});
    auto summary = vr.Summary();
    EXPECT_NE(summary.find("[WARN]"), std::string::npos);
    EXPECT_NE(summary.find("/bar"), std::string::npos);
}
