add_executable(example_logger example_logger.cpp)
target_link_libraries(example_logger PRIVATE Daric)

add_executable(example_network example_network.cpp)
target_link_libraries(example_network PRIVATE Daric)
