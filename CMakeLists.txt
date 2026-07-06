cmake_minimum_required(VERSION 3.20)
project(nxn_cube_optimization LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.0
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_library(cube
    src/cube/cube.cpp
    src/cube/notation.cpp
    src/cube/scramble.cpp
)
target_include_directories(cube PUBLIC src)
target_compile_options(cube PRIVATE -Wall -Wextra -Wpedantic)

add_library(solver
    src/solver/coords.cpp
)
target_include_directories(solver PUBLIC src)
target_link_libraries(solver PUBLIC cube)
target_compile_options(solver PRIVATE -Wall -Wextra -Wpedantic)

add_executable(nxn_cube app/main.cpp)
target_link_libraries(nxn_cube PRIVATE cube)

enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)
