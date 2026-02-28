# FindAardvark.cmake — Locate the Total Phase Aardvark SDK
#
# Search order:
#   1. vendor/aardvark/ in the project source tree (platform/arch auto-detected)
#   2. AARDVARK_ROOT CMake variable
#
# Defines:
#   Aardvark_FOUND        — TRUE if aardvark.h and the library were found
#   Aardvark::Aardvark    — Imported target

if(TARGET Aardvark::Aardvark)
    return()
endif()

# --- Platform / architecture detection ---
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_aardvark_platform "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_aardvark_platform "windows")
else()
    set(_aardvark_platform "")
endif()

set(_aardvark_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(_aardvark_arch STREQUAL "AMD64")
    set(_aardvark_arch "x86_64")
endif()

# --- Collect search hints (vendor/ first, then user override) ---
set(_aardvark_vendor_dir "${CMAKE_SOURCE_DIR}/vendor/aardvark")

set(_aardvark_hints "")
list(APPEND _aardvark_hints "${_aardvark_vendor_dir}")
if(AARDVARK_ROOT)
    list(APPEND _aardvark_hints "${AARDVARK_ROOT}")
endif()

# Library path suffixes: vendor layout + standard layouts
set(_aardvark_lib_suffixes
    "${_aardvark_platform}/${_aardvark_arch}"
    lib lib64
)

find_path(AARDVARK_INCLUDE_DIR
    NAMES aardvark.h
    HINTS ${_aardvark_hints}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
)

find_library(AARDVARK_LIBRARY
    NAMES aardvark
    HINTS ${_aardvark_hints}
    PATH_SUFFIXES ${_aardvark_lib_suffixes}
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Aardvark
    REQUIRED_VARS AARDVARK_LIBRARY AARDVARK_INCLUDE_DIR
)

if(Aardvark_FOUND AND NOT TARGET Aardvark::Aardvark)
    add_library(Aardvark::Aardvark UNKNOWN IMPORTED)
    set_target_properties(Aardvark::Aardvark PROPERTIES
        IMPORTED_LOCATION "${AARDVARK_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AARDVARK_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(AARDVARK_INCLUDE_DIR AARDVARK_LIBRARY)
