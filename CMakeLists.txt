cmake_minimum_required(VERSION 3.16)
project(LowLatencyOrderBook
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "Low-latency limit order book and matching engine in C++"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimisation flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -march=native -DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Benchmark")
    add_compile_options(-O3 -march=native -DNDEBUG)
else()
    add_compile_options(-O0 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# --- Core Library ---
add_library(lob_core
    src/order_book.cpp
)
target_include_directories(lob_core PUBLIC include)

# --- Example executable ---
add_executable(lob_example examples/main.cpp)
target_link_libraries(lob_example PRIVATE lob_core)

# --- Tests (Google Test) ---
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(lob_tests
    tests/test_order_pool.cpp
    tests/test_price_level.cpp
    tests/test_order_book.cpp
    tests/test_matching_engine.cpp
)
target_link_libraries(lob_tests PRIVATE lob_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(lob_tests)

# --- Benchmark executable ---
add_executable(lob_bench bench/benchmark.cpp)
target_link_libraries(lob_bench PRIVATE lob_core)
target_compile_options(lob_bench PRIVATE -O3 -march=native -DNDEBUG)
