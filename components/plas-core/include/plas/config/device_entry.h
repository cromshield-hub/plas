#pragma once

#include <map>
#include <string>

namespace plas::config {

struct DeviceEntry {
    std::string nickname;
    std::string uri;
    std::string driver;
    std::map<std::string, std::string> args;
};

}  // namespace plas::config
