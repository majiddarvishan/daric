include(cmake/CPM.cmake)

CPMAddPackage(
  NAME prometheus_cpp
  VERSION 1.3.0
  GIT_REPOSITORY "https://github.com/jupp0r/prometheus-cpp.git"
  GIT_TAG v1.3.0
  OPTIONS
    "BUILD_SHARED_LIBS ON"
    "ENABLE_TESTING OFF"
    "OVERRIDE_CXX_STANDARD_FLAGS OFF"
    "GENERATE_PKGCONFIG OFF"
    "USE_THIRDPARTY_LIBRARIES ON"
)

# -----------------------------
# Logger example
# -----------------------------
if(BUILD_LOGGER)
    add_executable(example_logger example_logger.cpp)
    target_link_libraries(example_logger PRIVATE daric)
endif()

# -----------------------------
# Networking example
# -----------------------------
if(BUILD_NETWORKING)
    add_executable(example_network example_network.cpp)
    target_link_libraries(example_network PRIVATE daric)
endif()

# -----------------------------
# ObjectPool example
# -----------------------------
if(BUILD_OBJECT_POOL)
    add_executable(example_object_pool example_object_pool.cpp)

    option(ENABLE_PROMETHEUS_METRICS "Enable Prometheus metrics" ON)

    if(ENABLE_PROMETHEUS_METRICS)
        message(STATUS "ENABLE_PROMETHEUS_METRICS is ON - adding definition")
        target_compile_definitions(example_object_pool PRIVATE ENABLE_PROMETHEUS_METRICS)
        target_link_libraries(example_object_pool
            PRIVATE
            daric
            prometheus-cpp::pull
        )
    else()
        message(STATUS "ENABLE_PROMETHEUS_METRICS is OFF")
        target_link_libraries(example_object_pool PRIVATE daric)
    endif()
endif()
