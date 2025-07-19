cmake_minimum_required(VERSION 3.29)
project(TimestampOptimalHeap VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")


# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create interface library for the header-only TimestampOptimalHeap
add_library(TimestampOptimalHeap INTERFACE)

# Set include directories for the interface library
target_include_directories(TimestampOptimalHeap INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Set compile features for the interface library
target_compile_features(TimestampOptimalHeap INTERFACE cxx_std_17)

# Main executable
add_executable(main src/main.cpp
        src/WorkSetHeap.cpp)

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
