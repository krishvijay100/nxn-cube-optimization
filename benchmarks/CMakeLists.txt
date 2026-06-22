add_executable(cube_benchmarks
    cube_bench.cpp
)
target_link_libraries(cube_benchmarks PRIVATE cube benchmark::benchmark benchmark::benchmark_main)
