include(GoogleTest)

add_executable(cube_tests
    cube_test.cpp
    scramble_test.cpp
    coords_test.cpp
    move_tables_test.cpp
    pruning_test.cpp
    ida_test.cpp
)
target_link_libraries(cube_tests PRIVATE cube solver GTest::gtest_main)
gtest_discover_tests(cube_tests)
