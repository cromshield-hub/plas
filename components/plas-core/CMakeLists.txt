# plas-core component — all library targets
# Targets: plas::core, plas::log, plas::config, plas::backend_interface, plas::backend_driver

# ---------- plas_core ----------
add_library(plas_core
    src/core/error.cpp
    src/core/version.cpp
    src/core/byte_buffer.cpp
)
add_library(plas::core ALIAS plas_core)

target_include_directories(plas_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(plas_core
    PUBLIC plas::compiler_settings
)

target_compile_definitions(plas_core
    PRIVATE
        PLAS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        PLAS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        PLAS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# ---------- plas_log ----------
add_library(plas_log
    src/log/logger.cpp
    src/log/backends/spdlog_backend.cpp
)
add_library(plas::log ALIAS plas_log)

target_include_directories(plas_log
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(plas_log
    PUBLIC plas::core
    PRIVATE spdlog::spdlog
)

# ---------- plas_config ----------
add_library(plas_config
    src/config/config.cpp
    src/config/json_parser.cpp
    src/config/yaml_parser.cpp
)
add_library(plas::config ALIAS plas_config)

target_include_directories(plas_config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config
)

target_link_libraries(plas_config
    PUBLIC plas::core
    PRIVATE nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp
)

# ---------- plas_backend_interface ----------
add_library(plas_backend_interface
    src/backend/interface/device_factory.cpp
)
add_library(plas::backend_interface ALIAS plas_backend_interface)

target_include_directories(plas_backend_interface
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(plas_backend_interface
    PUBLIC plas::core plas::log
)

# ---------- plas_backend_driver ----------
add_library(plas_backend_driver
    src/backend/driver/aardvark/aardvark_device.cpp
    src/backend/driver/ft4222h/ft4222h_device.cpp
    src/backend/driver/pmu3/pmu3_device.cpp
    src/backend/driver/pmu4/pmu4_device.cpp
)
add_library(plas::backend_driver ALIAS plas_backend_driver)

target_include_directories(plas_backend_driver
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(plas_backend_driver
    PUBLIC plas::backend_interface plas::config plas::log
)
