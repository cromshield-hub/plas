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
