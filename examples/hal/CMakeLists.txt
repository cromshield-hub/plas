add_executable(device_manager device_manager.cpp)
target_link_libraries(device_manager PRIVATE plas::hal_interface plas::hal_driver)

# Copy fixture files next to the built executable
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../config/fixtures/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)
