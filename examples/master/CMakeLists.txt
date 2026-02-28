add_executable(master_example master_example.cpp)
target_link_libraries(master_example PRIVATE plas::bootstrap)

# Copy fixture files next to the built executable
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)
