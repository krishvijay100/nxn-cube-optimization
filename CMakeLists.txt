cmake_minimum_required(VERSION 3.20)
project(nxn_cube_optimization LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

# pybind11 for exposing the c++ solver to python (for fastapi service)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.13.6
)
FetchContent_MakeAvailable(pybind11)

add_library(cube
    src/cube/cube.cpp
    src/cube/notation.cpp
    src/cube/scramble.cpp
)
target_include_directories(cube PUBLIC src)
target_compile_options(cube PRIVATE -Wall -Wextra -Wpedantic)

add_library(cube_nxn
    src/cube_nxn/cube_nxn.cpp
)
target_include_directories(cube_nxn PUBLIC src)
target_compile_options(cube_nxn PRIVATE -Wall -Wextra -Wpedantic)

set(solver_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/solver/coords.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/solver/move_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/solver/pruning.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/solver/symmetry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/solver/ida.cpp
)

add_library(solver ${solver_sources})
target_include_directories(solver PUBLIC src)
target_link_libraries(solver PUBLIC cube)
target_compile_options(solver PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(cube_nxn PUBLIC cube solver)

add_executable(nxn_cube app/main.cpp)
target_link_libraries(nxn_cube PRIVATE cube)

# profiling harness — hot loop for attaching Instruments/perf
add_executable(profile_solve tools/profile_solve.cpp)
target_link_libraries(profile_solve PRIVATE cube solver)
# keep symbols in the release binary so the profiler can name functions
target_compile_options(profile_solve PRIVATE -g)

# phase-8 stage-1 demo: print a scramble + center-reduction sequence
add_executable(reduce_centers_demo tools/reduce_centers_demo.cpp)
target_link_libraries(reduce_centers_demo PRIVATE cube cube_nxn)

enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(bindings)
