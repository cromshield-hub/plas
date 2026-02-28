# FindPMU3.cmake — Locate the PMU3 SDK
#
# Search order:
#   1. vendor/pmu3/ in the project source tree (platform/arch auto-detected)
#   2. PMU3_ROOT CMake variable or environment variable
#   3. System paths
#
# Defines:
#   PMU3_FOUND            — TRUE if headers and the library were found
#   PMU3::PMU3            — Imported target

if(TARGET PMU3::PMU3)
    return()
endif()

# --- Platform / architecture detection ---
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_pmu3_platform "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_pmu3_platform "windows")
else()
    set(_pmu3_platform "")
endif()

set(_pmu3_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(_pmu3_arch STREQUAL "AMD64")
    set(_pmu3_arch "x86_64")
endif()

# --- Collect search hints (vendor/ first, then user override) ---
set(_pmu3_vendor_dir "${CMAKE_SOURCE_DIR}/vendor/pmu3")

set(_pmu3_hints "")
list(APPEND _pmu3_hints "${_pmu3_vendor_dir}")
if(PMU3_ROOT)
    list(APPEND _pmu3_hints "${PMU3_ROOT}")
endif()
if(DEFINED ENV{PMU3_ROOT})
    list(APPEND _pmu3_hints "$ENV{PMU3_ROOT}")
endif()

set(_pmu3_lib_suffixes
    "${_pmu3_platform}/${_pmu3_arch}"
    lib lib64
)

find_path(PMU3_INCLUDE_DIR
    NAMES pmu3.h
    HINTS ${_pmu3_hints}
    PATH_SUFFIXES include
)

find_library(PMU3_LIBRARY
    NAMES pmu3
    HINTS ${_pmu3_hints}
    PATH_SUFFIXES ${_pmu3_lib_suffixes}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PMU3
    REQUIRED_VARS PMU3_LIBRARY PMU3_INCLUDE_DIR
)

if(PMU3_FOUND AND NOT TARGET PMU3::PMU3)
    add_library(PMU3::PMU3 UNKNOWN IMPORTED)
    set_target_properties(PMU3::PMU3 PROPERTIES
        IMPORTED_LOCATION "${PMU3_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PMU3_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(PMU3_INCLUDE_DIR PMU3_LIBRARY)
