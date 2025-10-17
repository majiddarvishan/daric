# -----------------------------
# Logger tests
# -----------------------------
# if(BUILD_LOGGER)
#     add_executable(test_logger test_logger.cpp)
#     target_link_libraries(test_logger PRIVATE daric)
#     add_test(NAME LoggerTest COMMAND test_logger)
# endif()

# -----------------------------
# Networking tests
# -----------------------------
# You can add networking tests similarly
# if(BUILD_NETWORKING)
#     add_executable(test_network test_network.cpp)
#     target_link_libraries(test_network PRIVATE daric)
#     add_test(NAME NetworkTest COMMAND test_network)
# endif()

# -----------------------------
# ObjectPool test
# -----------------------------
if(BUILD_OBJECT_POOL)
    # add_compile_options(-O1 -g -fsanitize=thread,address -fno-omit-frame-pointer)
    add_compile_options(-O1 -g -fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)

    add_executable(pool_tsan_test pool_tsan_test.cpp)

    option(ENABLE_PROMETHEUS_METRICS "Enable Prometheus metrics" ON)

    if(ENABLE_PROMETHEUS_METRICS)
        message(STATUS "ENABLE_PROMETHEUS_METRICS is ON - adding definition")
        target_compile_definitions(pool_tsan_test PRIVATE ENABLE_PROMETHEUS_METRICS)
        target_link_libraries(pool_tsan_test
            PRIVATE
            daric
            prometheus-cpp::pull
            pthread
        )
    else()
        message(STATUS "ENABLE_PROMETHEUS_METRICS is OFF")
        target_link_libraries(pool_tsan_test
            PRIVATE
            daric
            pthread
        )
    endif()
endif()
