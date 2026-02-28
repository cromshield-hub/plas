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
