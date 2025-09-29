cmake_minimum_required(VERSION 3.16)
project(Daric VERSION 0.1.0 LANGUAGES CXX)

# Require C++17 (can upgrade later)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options for building tests/examples
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" ON)

# Add library
add_library(Daric STATIC
    src/logger.cpp
    src/threadpool.cpp
    src/networking.cpp
)

# Include directories for consumers
target_include_directories(Daric PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional: compiler warnings
if (MSVC)
    target_compile_options(Daric PRIVATE /W4 /permissive-)
else()
    target_compile_options(Daric PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Build tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Build examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation (optional)
install(TARGETS Daric
        EXPORT DaricTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
