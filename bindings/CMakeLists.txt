pybind11_add_module(nxn_cube_py nxn_cube_py.cpp)
target_link_libraries(nxn_cube_py PRIVATE cube solver)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME python_bindings
    COMMAND "${Python3_EXECUTABLE}" "-m" "unittest" "-v" "test_bindings"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set_tests_properties(python_bindings PROPERTIES
    ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:nxn_cube_py>")
