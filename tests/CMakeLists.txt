include(GoogleTest)

add_executable(cube_tests
    cube_test.cpp
)
target_link_libraries(cube_tests PRIVATE cube GTest::gtest_main)
gtest_discover_tests(cube_tests)
