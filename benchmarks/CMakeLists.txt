add_executable(cube_benchmarks
    cube_bench.cpp
    solve_bench.cpp
)
target_link_libraries(cube_benchmarks PRIVATE cube solver benchmark::benchmark benchmark::benchmark_main)

add_library(solver_counted STATIC ${solver_sources})
target_include_directories(solver_counted PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(solver_counted PUBLIC ENABLE_NODE_COUNTS)
target_link_libraries(solver_counted PUBLIC cube)

add_executable(cube_benchmarks_counted
    cube_bench.cpp
    solve_bench.cpp
)
target_link_libraries(cube_benchmarks_counted PRIVATE cube solver_counted benchmark::benchmark benchmark::benchmark_main)
