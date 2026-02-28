#pragma once

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace plas::configspec {

enum class Severity { kError, kWarning };

struct ValidationIssue {
    Severity severity;
    std::string path;
    std::string message;
};

struct ValidationResult {
    bool valid = true;
    std::vector<ValidationIssue> issues;

    std::vector<ValidationIssue> Errors() const {
        std::vector<ValidationIssue> result;
        std::copy_if(issues.begin(), issues.end(), std::back_inserter(result),
                     [](const ValidationIssue& i) {
                         return i.severity == Severity::kError;
                     });
        return result;
    }

    std::vector<ValidationIssue> Warnings() const {
        std::vector<ValidationIssue> result;
        std::copy_if(issues.begin(), issues.end(), std::back_inserter(result),
                     [](const ValidationIssue& i) {
                         return i.severity == Severity::kWarning;
                     });
        return result;
    }

    std::string Summary() const {
        auto errors = Errors();
        auto warnings = Warnings();
        std::ostringstream os;
        os << (valid ? "valid" : "invalid") << ": " << errors.size()
           << " error(s), " << warnings.size() << " warning(s)";
        for (const auto& issue : issues) {
            os << "\n  ["
               << (issue.severity == Severity::kError ? "ERROR" : "WARN")
               << "] " << issue.path << ": " << issue.message;
        }
        return os.str();
    }
};

}  // namespace plas::configspec
