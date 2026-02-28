#pragma once

#include <cstdint>
#include <string>

namespace plas::core {

struct Version {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
};

Version GetVersion();
std::string GetVersionString();

}  // namespace plas::core
