add_executable(config_load config_load.cpp)
target_link_libraries(config_load PRIVATE plas::config)

add_executable(config_node_tree config_node_tree.cpp)
target_link_libraries(config_node_tree PRIVATE plas::config)

add_executable(property_manager property_manager.cpp)
target_link_libraries(property_manager PRIVATE plas::config)

# Copy fixture files next to the built executables
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)
