# plas-core component — core library targets
# Targets: plas::core, plas::log, plas::config, plas::hal_interface

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

# ---------- plas_hal_interface ----------
add_library(plas_hal_interface
    src/hal/interface/device_factory.cpp
)
add_library(plas::hal_interface ALIAS plas_hal_interface)

target_include_directories(plas_hal_interface
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(plas_hal_interface
    PUBLIC plas::core plas::log
)
