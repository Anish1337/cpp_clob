find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    include(FetchContent)
    FetchContent_Declare(googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)
endif()
add_executable(benchmarks benchmark_order_book.cpp benchmark_matching.cpp benchmark_allocator.cpp)
target_link_libraries(benchmarks PRIVATE lob_cpp benchmark::benchmark benchmark::benchmark_main)
