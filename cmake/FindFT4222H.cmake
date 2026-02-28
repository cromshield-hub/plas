# FindFT4222H.cmake — Locate the FTDI FT4222H SDK + D2XX dependency
#
# Search order:
#   1. vendor/ft4222h/ in the project source tree (platform/arch auto-detected)
#   2. FT4222H_ROOT CMake variable or environment variable
#   3. System paths
#
# Defines:
#   FT4222H_FOUND         — TRUE if headers and the library were found
#   FT4222H::FT4222H      — Imported target (links D2XX automatically)

if(TARGET FT4222H::FT4222H)
    return()
endif()

# --- Platform / architecture detection ---
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_ft4222h_platform "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_ft4222h_platform "windows")
else()
    set(_ft4222h_platform "")
endif()

set(_ft4222h_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(_ft4222h_arch STREQUAL "AMD64")
    set(_ft4222h_arch "x86_64")
endif()

# --- Collect search hints (vendor/ first, then user override) ---
set(_ft4222h_vendor_dir "${CMAKE_SOURCE_DIR}/vendor/ft4222h")

set(_ft4222h_hints "")
list(APPEND _ft4222h_hints "${_ft4222h_vendor_dir}")
if(FT4222H_ROOT)
    list(APPEND _ft4222h_hints "${FT4222H_ROOT}")
endif()
if(DEFINED ENV{FT4222H_ROOT})
    list(APPEND _ft4222h_hints "$ENV{FT4222H_ROOT}")
endif()

set(_ft4222h_lib_suffixes
    "${_ft4222h_platform}/${_ft4222h_arch}"
    lib lib64
)

# --- FT4222H header + library ---
find_path(FT4222H_INCLUDE_DIR
    NAMES libft4222.h
    HINTS ${_ft4222h_hints}
    PATH_SUFFIXES include
)

find_library(FT4222H_LIBRARY
    NAMES ft4222
    HINTS ${_ft4222h_hints}
    PATH_SUFFIXES ${_ft4222h_lib_suffixes}
)

# --- D2XX (ftd2xx) dependency — FT4222H requires it ---
find_path(FTD2XX_INCLUDE_DIR
    NAMES ftd2xx.h
    HINTS ${_ft4222h_hints}
    PATH_SUFFIXES include
)

find_library(FTD2XX_LIBRARY
    NAMES ftd2xx
    HINTS ${_ft4222h_hints}
    PATH_SUFFIXES ${_ft4222h_lib_suffixes}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FT4222H
    REQUIRED_VARS FT4222H_LIBRARY FT4222H_INCLUDE_DIR
                  FTD2XX_LIBRARY FTD2XX_INCLUDE_DIR
)

if(FT4222H_FOUND AND NOT TARGET FT4222H::FT4222H)
    add_library(FT4222H::FT4222H UNKNOWN IMPORTED)
    set_target_properties(FT4222H::FT4222H PROPERTIES
        IMPORTED_LOCATION "${FT4222H_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${FT4222H_INCLUDE_DIR};${FTD2XX_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${FTD2XX_LIBRARY}"
    )
endif()

mark_as_advanced(FT4222H_INCLUDE_DIR FT4222H_LIBRARY FTD2XX_INCLUDE_DIR FTD2XX_LIBRARY)
