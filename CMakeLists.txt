cmake_minimum_required(VERSION 3.20)
project(lob_cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/slab_allocator.cpp
    src/order_book.cpp
    src/matching_engine.cpp
)

# Library
add_library(lob_cpp STATIC ${SOURCES})

# Tests
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Benchmarks
option(BUILD_BENCHMARKS "Build Google Benchmark executables" OFF)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Examples
add_subdirectory(examples)


# Standalone workload for Linux perf; no benchmark/test dependency is needed.
option(BUILD_PROFILING "Build deterministic profiling workload with frame pointers" OFF)
if(BUILD_PROFILING)
    add_executable(clob_profile profiling/matching_workload.cpp)
    target_link_libraries(clob_profile PRIVATE lob_cpp)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lob_cpp PRIVATE -fno-omit-frame-pointer)
        target_compile_options(clob_profile PRIVATE -fno-omit-frame-pointer)
    endif()
endif()
