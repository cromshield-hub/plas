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
