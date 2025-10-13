cmake_minimum_required(VERSION 3.16)
project(Daric VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------
# Module build options
# -----------------------------
option(BUILD_LOGGER "Build Logger module" ON)
option(BUILD_THREADPOOL "Build ThreadPool module" ON)
option(BUILD_NETWORKING "Build Networking module" ON)
option(BUILD_OBJECT_POOL "Build ObjectPool module" ON)

# -----------------------------
# Collect sources
# -----------------------------
set(DARIC_SOURCES "")

if(BUILD_LOGGER)
    list(APPEND DARIC_SOURCES src/logger.cpp)
endif()
if(BUILD_THREADPOOL)
    list(APPEND DARIC_SOURCES src/threadpool.cpp)
endif()

if(BUILD_NETWORKING)
    list(APPEND DARIC_SOURCES src/networking.cpp)
endif()

if(BUILD_OBJECT_POOL)
    list(APPEND DARIC_SOURCES src/thread_local_object_pool.cpp)
endif()

# Always include umbrella header
set(DARIC_INCLUDE include/daric/daric.h)

# -----------------------------
# Build Daric library
# -----------------------------
add_library(Daric STATIC ${DARIC_SOURCES})
target_include_directories(Daric PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Define macros for conditional compilation
if(BUILD_LOGGER)
    target_compile_definitions(Daric PUBLIC BUILD_LOGGER)
endif()

if(BUILD_THREADPOOL)
    target_compile_definitions(Daric PUBLIC BUILD_THREADPOOL)
endif()

if(BUILD_NETWORKING)
    target_compile_definitions(Daric PUBLIC BUILD_NETWORKING)
endif()

if(BUILD_OBJECT_POOL)
    target_compile_definitions(Daric PUBLIC BUILD_OBJECT_POOL)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(Daric PRIVATE /W4 /permissive-)
else()
    target_compile_options(Daric PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------
# Build tests/examples
# -----------------------------
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" ON)

if(BUILD_TESTS)
enable_testing()
add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
add_subdirectory(examples)
endif()


# -----------------------------
# Installation
# -----------------------------
install(TARGETS Daric
        EXPORT DaricTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
