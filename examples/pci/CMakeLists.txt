add_executable(doe_exchange doe_exchange.cpp)
target_link_libraries(doe_exchange PRIVATE plas::hal_interface)

add_executable(topology_walk topology_walk.cpp)
target_link_libraries(topology_walk PRIVATE plas::hal_interface)
