include(GoogleTest)

# Core tests
add_executable(test_core_error core/test_error.cpp)
target_link_libraries(test_core_error PRIVATE plas::core GTest::gtest_main)
gtest_discover_tests(test_core_error)

add_executable(test_core_result core/test_result.cpp)
target_link_libraries(test_core_result PRIVATE plas::core GTest::gtest_main)
gtest_discover_tests(test_core_result)

add_executable(test_core_units core/test_units.cpp)
target_link_libraries(test_core_units PRIVATE plas::core GTest::gtest_main)
gtest_discover_tests(test_core_units)

add_executable(test_core_properties core/test_properties.cpp)
target_link_libraries(test_core_properties PRIVATE plas::core GTest::gtest_main)
gtest_discover_tests(test_core_properties)

add_executable(test_core_byte_buffer core/test_byte_buffer.cpp)
target_link_libraries(test_core_byte_buffer PRIVATE plas::core GTest::gtest_main)
gtest_discover_tests(test_core_byte_buffer)

add_executable(test_core_version core/test_version.cpp)
target_link_libraries(test_core_version PRIVATE plas::core GTest::gtest_main)
gtest_discover_tests(test_core_version)

# Log tests
add_executable(test_logger log/test_logger.cpp)
target_link_libraries(test_logger PRIVATE plas::log GTest::gtest_main)
gtest_discover_tests(test_logger)

# Config tests
add_executable(test_config_json config/test_config_json.cpp)
target_link_libraries(test_config_json PRIVATE plas::config GTest::gtest_main)
gtest_discover_tests(test_config_json)

add_executable(test_config_yaml config/test_config_yaml.cpp)
target_link_libraries(test_config_yaml PRIVATE plas::config GTest::gtest_main)
gtest_discover_tests(test_config_yaml)

add_executable(test_property_manager config/test_property_manager.cpp)
target_link_libraries(test_property_manager PRIVATE plas::config GTest::gtest_main)
gtest_discover_tests(test_property_manager)

add_executable(test_config_node config/test_config_node.cpp)
target_link_libraries(test_config_node PRIVATE plas::config GTest::gtest_main)
gtest_discover_tests(test_config_node)

add_executable(test_config_errors config/test_config_errors.cpp)
target_link_libraries(test_config_errors PRIVATE plas::config GTest::gtest_main)
gtest_discover_tests(test_config_errors)

# Copy test fixtures to build dir
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config/fixtures/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/hal/fixtures/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)

# HAL tests
add_executable(test_device_manager hal/test_device_manager.cpp)
target_link_libraries(test_device_manager
    PRIVATE plas::hal_interface plas::hal_driver GTest::gtest_main)
gtest_discover_tests(test_device_manager)

add_executable(test_device_factory hal/interface/test_device_factory.cpp)
target_link_libraries(test_device_factory
    PRIVATE plas::hal_interface plas::hal_driver GTest::gtest_main)
gtest_discover_tests(test_device_factory)

add_executable(test_device_lifecycle hal/interface/test_device_lifecycle.cpp)
target_link_libraries(test_device_lifecycle
    PRIVATE plas::hal_interface GTest::gtest_main)
target_include_directories(test_device_lifecycle
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/hal/interface)
gtest_discover_tests(test_device_lifecycle)

# PCI interface tests
add_executable(test_pci_types hal/interface/pci/test_pci_types.cpp)
target_link_libraries(test_pci_types
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_pci_types)

add_executable(test_pci_config hal/interface/pci/test_pci_config.cpp)
target_link_libraries(test_pci_config
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_pci_config)

add_executable(test_pci_doe hal/interface/pci/test_pci_doe.cpp)
target_link_libraries(test_pci_doe
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_pci_doe)

add_executable(test_pci_bar hal/interface/pci/test_pci_bar.cpp)
target_link_libraries(test_pci_bar
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_pci_bar)

add_executable(test_pci_topology hal/interface/pci/test_pci_topology.cpp)
target_link_libraries(test_pci_topology
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_pci_topology)

add_executable(test_pci_device hal/interface/pci/test_pci_device.cpp)
target_link_libraries(test_pci_device
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_pci_device)

add_executable(test_pci_topology_integration
    hal/interface/pci/test_pci_topology_integration.cpp)
target_link_libraries(test_pci_topology_integration
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_pci_topology_integration)

# CXL interface tests
add_executable(test_cxl_types hal/interface/pci/test_cxl_types.cpp)
target_link_libraries(test_cxl_types
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_cxl_types)

add_executable(test_cxl hal/interface/pci/test_cxl.cpp)
target_link_libraries(test_cxl
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_cxl)

add_executable(test_cxl_mailbox hal/interface/pci/test_cxl_mailbox.cpp)
target_link_libraries(test_cxl_mailbox
    PRIVATE plas::hal_interface GTest::gtest_main)
gtest_discover_tests(test_cxl_mailbox)

# Aardvark driver tests (unit tests always, integration gated by SDK)
add_executable(test_aardvark_device hal/driver/test_aardvark_device.cpp)
target_link_libraries(test_aardvark_device
    PRIVATE plas::hal_interface plas::hal_driver GTest::gtest_main)
gtest_discover_tests(test_aardvark_device)

if(PLAS_HAS_AARDVARK)
    add_executable(test_aardvark_integration hal/driver/test_aardvark_integration.cpp)
    target_link_libraries(test_aardvark_integration
        PRIVATE plas::hal_interface plas::hal_driver GTest::gtest_main)
    gtest_discover_tests(test_aardvark_integration)
endif()

# FT4222H driver tests (unit tests always, integration gated by SDK)
add_executable(test_ft4222h_device hal/driver/test_ft4222h_device.cpp)
target_link_libraries(test_ft4222h_device
    PRIVATE plas::hal_interface plas::hal_driver GTest::gtest_main)
gtest_discover_tests(test_ft4222h_device)

if(PLAS_HAS_FT4222H)
    add_executable(test_ft4222h_integration hal/driver/test_ft4222h_integration.cpp)
    target_link_libraries(test_ft4222h_integration
        PRIVATE plas::hal_interface plas::hal_driver GTest::gtest_main)
    gtest_discover_tests(test_ft4222h_integration)
endif()

# Bootstrap tests
add_executable(test_bootstrap bootstrap/test_bootstrap.cpp)
target_link_libraries(test_bootstrap
    PRIVATE plas::bootstrap GTest::gtest_main)
gtest_discover_tests(test_bootstrap)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bootstrap/fixtures/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)

# ConfigSpec tests
add_executable(test_validation_result configspec/test_validation_result.cpp)
target_link_libraries(test_validation_result
    PRIVATE plas::configspec GTest::gtest_main)
gtest_discover_tests(test_validation_result)

add_executable(test_spec_registry configspec/test_spec_registry.cpp)
target_link_libraries(test_spec_registry
    PRIVATE plas::configspec GTest::gtest_main)
gtest_discover_tests(test_spec_registry)

add_executable(test_validator configspec/test_validator.cpp)
target_link_libraries(test_validator
    PRIVATE plas::configspec GTest::gtest_main)
gtest_discover_tests(test_validator)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/configspec/fixtures/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)

# Copy spec source files for LoadSpecsFromDirectory tests
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../components/plas-configspec/schemas/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/schemas/)

# PciUtils driver tests (conditional on libpci availability)
if(PLAS_HAS_PCIUTILS)
    add_executable(test_pciutils_device hal/driver/test_pciutils_device.cpp)
    target_link_libraries(test_pciutils_device
        PRIVATE plas::hal_interface plas::hal_driver GTest::gtest_main)
    gtest_discover_tests(test_pciutils_device)

    add_executable(test_pciutils_integration hal/driver/test_pciutils_integration.cpp)
    target_link_libraries(test_pciutils_integration
        PRIVATE plas::hal_interface plas::hal_driver GTest::gtest_main)
    gtest_discover_tests(test_pciutils_integration)
endif()
