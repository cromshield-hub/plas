# plas-bootstrap component — application initialization helper
# Target: plas::bootstrap

add_library(plas_bootstrap
    src/bootstrap/bootstrap.cpp
)
add_library(plas::bootstrap ALIAS plas_bootstrap)

target_include_directories(plas_bootstrap
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(plas_bootstrap PUBLIC plas::hal_driver)
