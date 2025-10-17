cmake_minimum_required(VERSION 3.16)
project(daric
        VERSION 0.0.1
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# -----------------------------
# Module build options
# -----------------------------
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_LOGGER "Build Logger module" ON)
option(BUILD_THREADPOOL "Build ThreadPool module" ON)
option(BUILD_NETWORKING "Build Networking module" ON)
option(BUILD_OBJECT_POOL "Build ObjectPool module" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

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

# -----------------------------
# Build daric library
# -----------------------------
if(NOT DARIC_SOURCES)
    # Create interface library if no sources (header-only mode)
    message(STATUS "Building daric as header-only library")
    add_library(daric INTERFACE)
    target_include_directories(daric INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Define macros for conditional compilation
    if(BUILD_LOGGER)
        target_compile_definitions(daric INTERFACE BUILD_LOGGER)
    endif()
    if(BUILD_THREADPOOL)
        target_compile_definitions(daric INTERFACE BUILD_THREADPOOL)
    endif()
    if(BUILD_NETWORKING)
        target_compile_definitions(daric INTERFACE BUILD_NETWORKING)
    endif()
    if(BUILD_OBJECT_POOL)
        target_compile_definitions(daric INTERFACE BUILD_OBJECT_POOL)
    endif()
else()
    # Build normal library
    add_library(daric ${DARIC_SOURCES})

    target_include_directories(daric PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Define macros for conditional compilation
    if(BUILD_LOGGER)
        target_compile_definitions(daric PUBLIC BUILD_LOGGER)
    endif()
    if(BUILD_THREADPOOL)
        target_compile_definitions(daric PUBLIC BUILD_THREADPOOL)
    endif()
    if(BUILD_NETWORKING)
        target_compile_definitions(daric PUBLIC BUILD_NETWORKING)
    endif()
    if(BUILD_OBJECT_POOL)
        target_compile_definitions(daric PUBLIC BUILD_OBJECT_POOL)
    endif()

    # Platform-specific linking
    if(BUILD_NETWORKING)
        if(WIN32)
            target_link_libraries(daric PRIVATE ws2_32 wsock32)
        elseif(UNIX AND NOT APPLE)
            target_link_libraries(daric PRIVATE pthread)
        endif()
    endif()

    if(BUILD_THREADPOOL)
        if(UNIX)
            target_link_libraries(daric PRIVATE pthread)
        endif()
    endif()

    # Compiler warnings
    if(MSVC)
        target_compile_options(daric PRIVATE /W4 /permissive-)
    else()
        target_compile_options(daric PRIVATE
            -Wall -Wextra -Wpedantic
            $<$<CONFIG:Debug>:-g -O0>
            $<$<CONFIG:Release>:-O3>
        )
    endif()
endif()

# Add namespace alias for better practice
add_library(daric::daric ALIAS daric)

# Set library properties
set_target_properties(daric PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME daric
)

# -----------------------------
# Build tests/examples
# -----------------------------
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
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library
install(TARGETS daric
    EXPORT daricTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install CMake targets
install(EXPORT daricTargets
    FILE daricTargets.cmake
    NAMESPACE daric::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/daric
)

# Create package config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/daricConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/daricConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/daric
)

# Create package version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/daricConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Install package config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/daricConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/daricConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/daric
)

# Export from build tree (optional, for developers)
export(EXPORT daricTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/daricTargets.cmake"
    NAMESPACE daric::
)

# Print configuration summary
message(STATUS "")
message(STATUS "daric Configuration:")
message(STATUS "  Version:         ${PROJECT_VERSION}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libs:     ${BUILD_SHARED_LIBS}")
message(STATUS "  Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Modules:")
message(STATUS "  Logger:          ${BUILD_LOGGER}")
message(STATUS "  ThreadPool:      ${BUILD_THREADPOOL}")
message(STATUS "  Networking:      ${BUILD_NETWORKING}")
message(STATUS "  ObjectPool:      ${BUILD_OBJECT_POOL}")
message(STATUS "")
message(STATUS "Build options:")
message(STATUS "  Tests:           ${BUILD_TESTS}")
message(STATUS "  Examples:        ${BUILD_EXAMPLES}")
message(STATUS "")