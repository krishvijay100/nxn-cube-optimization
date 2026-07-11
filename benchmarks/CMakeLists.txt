add_executable(cube_benchmarks
    cube_bench.cpp
    solve_bench.cpp
)
target_link_libraries(cube_benchmarks PRIVATE cube solver benchmark::benchmark benchmark::benchmark_main)
