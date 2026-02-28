# FindPMU4.cmake — Locate the PMU4 SDK
#
# Search order:
#   1. vendor/pmu4/ in the project source tree (platform/arch auto-detected)
#   2. PMU4_ROOT CMake variable
#
# Defines:
#   PMU4_FOUND            — TRUE if headers and the library were found
#   PMU4::PMU4            — Imported target

if(TARGET PMU4::PMU4)
    return()
endif()

# --- Platform / architecture detection ---
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_pmu4_platform "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_pmu4_platform "windows")
else()
    set(_pmu4_platform "")
endif()

set(_pmu4_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(_pmu4_arch STREQUAL "AMD64")
    set(_pmu4_arch "x86_64")
endif()

# --- Collect search hints (vendor/ first, then user override) ---
set(_pmu4_vendor_dir "${CMAKE_SOURCE_DIR}/vendor/pmu4")

set(_pmu4_hints "")
list(APPEND _pmu4_hints "${_pmu4_vendor_dir}")
if(PMU4_ROOT)
    list(APPEND _pmu4_hints "${PMU4_ROOT}")
endif()
set(_pmu4_lib_suffixes
    "${_pmu4_platform}/${_pmu4_arch}"
    lib lib64
)

find_path(PMU4_INCLUDE_DIR
    NAMES pmu4.h
    HINTS ${_pmu4_hints}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
)

find_library(PMU4_LIBRARY
    NAMES pmu4
    HINTS ${_pmu4_hints}
    PATH_SUFFIXES ${_pmu4_lib_suffixes}
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PMU4
    REQUIRED_VARS PMU4_LIBRARY PMU4_INCLUDE_DIR
)

if(PMU4_FOUND AND NOT TARGET PMU4::PMU4)
    add_library(PMU4::PMU4 UNKNOWN IMPORTED)
    set_target_properties(PMU4::PMU4 PROPERTIES
        IMPORTED_LOCATION "${PMU4_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PMU4_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(PMU4_INCLUDE_DIR PMU4_LIBRARY)
