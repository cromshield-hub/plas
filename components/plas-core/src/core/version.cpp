#include "plas/core/version.h"

#ifndef PLAS_VERSION_MAJOR
#define PLAS_VERSION_MAJOR 0
#endif

#ifndef PLAS_VERSION_MINOR
#define PLAS_VERSION_MINOR 0
#endif

#ifndef PLAS_VERSION_PATCH
#define PLAS_VERSION_PATCH 0
#endif

namespace plas::core {

Version GetVersion() {
    return {PLAS_VERSION_MAJOR, PLAS_VERSION_MINOR, PLAS_VERSION_PATCH};
}

std::string GetVersionString() {
    return std::to_string(PLAS_VERSION_MAJOR) + "." +
           std::to_string(PLAS_VERSION_MINOR) + "." +
           std::to_string(PLAS_VERSION_PATCH);
}

}  // namespace plas::core
