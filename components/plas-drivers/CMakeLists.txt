# plas-drivers component — driver implementations
# Target: plas::hal_driver

add_library(plas_hal_driver
    src/hal/driver/aardvark/aardvark_device.cpp
    src/hal/driver/ft4222h/ft4222h_device.cpp
    src/hal/driver/pmu3/pmu3_device.cpp
    src/hal/driver/pmu4/pmu4_device.cpp
)
add_library(plas::hal_driver ALIAS plas_hal_driver)

target_include_directories(plas_hal_driver
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(plas_hal_driver
    PUBLIC plas::hal_interface plas::config plas::log
)

# ---------------------------------------------------------------------------
# Optional Aardvark SDK (proprietary, no pkg-config)
# ---------------------------------------------------------------------------
option(PLAS_WITH_AARDVARK "Build Aardvark driver with real SDK (requires libaardvark)" ON)
if(PLAS_WITH_AARDVARK)
    find_package(Aardvark QUIET)
endif()

if(Aardvark_FOUND)
    target_link_libraries(plas_hal_driver PRIVATE Aardvark::Aardvark)
    target_compile_definitions(plas_hal_driver PUBLIC PLAS_HAS_AARDVARK=1)
    set(PLAS_HAS_AARDVARK TRUE PARENT_SCOPE)
    message(STATUS "Aardvark driver: enabled (SDK found)")
else()
    set(PLAS_HAS_AARDVARK FALSE PARENT_SCOPE)
    message(STATUS "Aardvark driver: stub only (SDK not found)")
endif()

# ---------------------------------------------------------------------------
# Optional FT4222H SDK (FTDI, proprietary)
# ---------------------------------------------------------------------------
option(PLAS_WITH_FT4222H "Build FT4222H driver with real SDK (requires libft4222)" ON)
if(PLAS_WITH_FT4222H)
    find_package(FT4222H QUIET)
endif()

if(FT4222H_FOUND)
    target_link_libraries(plas_hal_driver PRIVATE FT4222H::FT4222H)
    target_compile_definitions(plas_hal_driver PUBLIC PLAS_HAS_FT4222H=1)
    set(PLAS_HAS_FT4222H TRUE PARENT_SCOPE)
    message(STATUS "FT4222H driver: enabled (SDK found)")
else()
    set(PLAS_HAS_FT4222H FALSE PARENT_SCOPE)
    message(STATUS "FT4222H driver: stub only (SDK not found)")
endif()

# ---------------------------------------------------------------------------
# Optional PMU3 SDK (proprietary)
# ---------------------------------------------------------------------------
option(PLAS_WITH_PMU3 "Build PMU3 driver with real SDK (requires libpmu3)" ON)
if(PLAS_WITH_PMU3)
    find_package(PMU3 QUIET)
endif()

if(PMU3_FOUND)
    target_link_libraries(plas_hal_driver PRIVATE PMU3::PMU3)
    target_compile_definitions(plas_hal_driver PUBLIC PLAS_HAS_PMU3=1)
    set(PLAS_HAS_PMU3 TRUE PARENT_SCOPE)
    message(STATUS "PMU3 driver: enabled (SDK found)")
else()
    set(PLAS_HAS_PMU3 FALSE PARENT_SCOPE)
    message(STATUS "PMU3 driver: stub only (SDK not found)")
endif()

# ---------------------------------------------------------------------------
# Optional PMU4 SDK (proprietary)
# ---------------------------------------------------------------------------
option(PLAS_WITH_PMU4 "Build PMU4 driver with real SDK (requires libpmu4)" ON)
if(PLAS_WITH_PMU4)
    find_package(PMU4 QUIET)
endif()

if(PMU4_FOUND)
    target_link_libraries(plas_hal_driver PRIVATE PMU4::PMU4)
    target_compile_definitions(plas_hal_driver PUBLIC PLAS_HAS_PMU4=1)
    set(PLAS_HAS_PMU4 TRUE PARENT_SCOPE)
    message(STATUS "PMU4 driver: enabled (SDK found)")
else()
    set(PLAS_HAS_PMU4 FALSE PARENT_SCOPE)
    message(STATUS "PMU4 driver: stub only (SDK not found)")
endif()

# ---------------------------------------------------------------------------
# Optional pciutils (libpci) driver
# ---------------------------------------------------------------------------
option(PLAS_WITH_PCIUTILS "Build pciutils PCI driver (requires libpci)" ON)
if(PLAS_WITH_PCIUTILS)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PCIUTILS IMPORTED_TARGET libpci)
    endif()
endif()

if(PCIUTILS_FOUND)
    target_sources(plas_hal_driver PRIVATE
        src/hal/driver/pciutils/pciutils_device.cpp
    )
    target_link_libraries(plas_hal_driver PRIVATE PkgConfig::PCIUTILS)
    target_compile_definitions(plas_hal_driver PUBLIC PLAS_HAS_PCIUTILS=1)
    set(PLAS_HAS_PCIUTILS TRUE PARENT_SCOPE)
    message(STATUS "pciutils driver: enabled (libpci found)")
else()
    set(PLAS_HAS_PCIUTILS FALSE PARENT_SCOPE)
    message(STATUS "pciutils driver: disabled (libpci not found)")
endif()
