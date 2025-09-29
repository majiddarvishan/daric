# -----------------------------
# Logger tests
# -----------------------------
if(BUILD_LOGGER)
    add_executable(test_logger test_logger.cpp)
    target_link_libraries(test_logger PRIVATE Daric)
    add_test(NAME LoggerTest COMMAND test_logger)
endif()

# -----------------------------
# Networking tests
# -----------------------------
# You can add networking tests similarly
if(BUILD_NETWORKING)
    add_executable(test_network test_network.cpp)
    target_link_libraries(test_network PRIVATE Daric)
    add_test(NAME NetworkTest COMMAND test_network)
endif()
