include(cmake/CPM.cmake)

# -----------------------------
# Logger example
# -----------------------------
if(BUILD_LOGGER)
    add_executable(example_logger example_logger.cpp)
    target_link_libraries(example_logger PRIVATE Daric)
endif()

# -----------------------------
# Networking example
# -----------------------------
if(BUILD_NETWORKING)
    add_executable(example_network example_network.cpp)
    target_link_libraries(example_network PRIVATE Daric)
endif()

# -----------------------------
# ObjectPool example
# -----------------------------
if(BUILD_OBJECT_POOL)
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

    add_executable(example_object_pool example_object_pool.cpp)
    target_link_libraries(example_object_pool
                PRIVATE
                Daric

                prometheus-cpp::pull
)
endif()
