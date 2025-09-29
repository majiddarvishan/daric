add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE Daric)
add_test(NAME LoggerTest COMMAND test_logger)
