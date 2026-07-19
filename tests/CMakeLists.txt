include(GoogleTest)

add_executable(cube_tests
    cube_test.cpp
    scramble_test.cpp
    coords_test.cpp
    move_tables_test.cpp
    pruning_test.cpp
    symmetry_test.cpp
    sym_pruning_test.cpp
    ida_test.cpp
    ida_thread_safety_test.cpp
    ida_parallel_test.cpp
    cube_nxn_test.cpp
)
target_link_libraries(cube_tests PRIVATE cube cube_nxn solver GTest::gtest_main)
gtest_discover_tests(cube_tests)
