#include <iostream>
#include "cube/cube.h"

int main() {
    cube::CubeState c = cube::solved_cube();
    std::cout << "Created solved cube. is_solved = "
              << (cube::is_solved(c) ? "true" : "false") << "\n";
    return 0;
}
