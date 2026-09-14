cmake_minimum_required(VERSION 3.20)

# Find or download Catch2
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests
    test_order_book.cpp
    test_matching_engine.cpp
    test_allocator.cpp
)

target_link_libraries(tests PRIVATE lob_cpp Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)

