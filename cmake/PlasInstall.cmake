# Install rules for the plas project

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers
install(DIRECTORY components/plas-core/include/plas
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Install plas library targets
install(TARGETS
        plas_core
        plas_log
        plas_config
        plas_backend_interface
        plas_backend_driver
        plas_compiler_settings
    EXPORT PlasTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Also install the FetchContent-provided dependency targets so that
# the export set can reference them.  Each FetchContent project uses
# a separate export set to keep things tidy.
install(TARGETS spdlog
    EXPORT PlasDepSpdlogTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT PlasDepSpdlogTargets
    FILE PlasSpdlogTargets.cmake
    NAMESPACE spdlog::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plas
)

install(TARGETS nlohmann_json
    EXPORT PlasDepJsonTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT PlasDepJsonTargets
    FILE PlasJsonTargets.cmake
    NAMESPACE nlohmann_json::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plas
)

install(TARGETS yaml-cpp
    EXPORT PlasDepYamlTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT PlasDepYamlTargets
    FILE PlasYamlTargets.cmake
    NAMESPACE yaml-cpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plas
)

# Export plas targets
install(EXPORT PlasTargets
    FILE PlasTargets.cmake
    NAMESPACE plas::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plas
)

# Package config
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PlasConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/PlasConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plas
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PlasConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PlasConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PlasConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plas
)

# VERSION file
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/plas
)
