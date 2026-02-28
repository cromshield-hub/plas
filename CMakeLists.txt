cmake_minimum_required(VERSION 3.16)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PLAS_VERSION_STRING)
string(STRIP "${PLAS_VERSION_STRING}" PLAS_VERSION_STRING)

project(plas
    VERSION ${PLAS_VERSION_STRING}
    LANGUAGES CXX
    DESCRIPTION "Platform Library Across Systems"
)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a separate build directory.")
endif()

# Options
option(PLAS_BUILD_TESTS "Build unit tests" OFF)
option(PLAS_BUILD_APPS "Build applications" OFF)
option(PLAS_BUILD_EXAMPLES "Build examples" OFF)
option(PLAS_INSTALL "Generate install targets" ON)

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PlasCompilerSettings)
include(PlasDependencies)

# Library modules
add_subdirectory(components/plas-core)
add_subdirectory(components/plas-drivers)
add_subdirectory(components/plas-bootstrap)

# Tests
if(PLAS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Applications (future)
if(PLAS_BUILD_APPS)
    add_subdirectory(apps)
endif()

# Examples
if(PLAS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
if(PLAS_INSTALL)
    include(PlasInstall)
endif()
