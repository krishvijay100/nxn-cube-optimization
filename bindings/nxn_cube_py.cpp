// expose three functions that talk in move-notation strings so callers never touch the CubeState/Move enum internals

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>   // for std::vector <-> python list auto-conversion

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "cube/cube.h"
#include "cube/notation.h"
#include "cube/scramble.h"
#include "solver/ida.h"

namespace py = pybind11;

namespace {

std::vector<cube::Move> notation_to_moves(const std::vector<std::string>& tokens) {
    std::vector<cube::Move> out;
    out.reserve(tokens.size());
    for (const auto& t : tokens) {
        auto m = cube::parse_move(t);
        if (!m) throw std::invalid_argument("unrecognized move: '" + t + "'");
        out.push_back(*m);
    }
    return out;
}

std::vector<std::string> moves_to_notation(const std::vector<cube::Move>& moves) {
    std::vector<std::string> out;
    out.reserve(moves.size());
    for (auto m : moves) out.emplace_back(cube::to_string(m));
    return out;
}

std::vector<std::string> py_random_scramble(int length, uint64_t seed) {
    if (length < 0) throw std::invalid_argument("length must be >= 0");
    return moves_to_notation(cube::random_scramble(length, seed));
}

std::vector<std::string> py_solve(const std::vector<std::string>& scramble_tokens) {
    auto scramble = notation_to_moves(scramble_tokens);
    cube::CubeState c = cube::solved_cube();
    for (auto m : scramble) cube::apply_move(c, m);
    return moves_to_notation(cube::solver::solve(c));
}

// used for the python round-trip correctness test
bool py_verify(const std::vector<std::string>& scramble_tokens,
               const std::vector<std::string>& solution_tokens) {
    auto scramble = notation_to_moves(scramble_tokens);
    auto solution = notation_to_moves(solution_tokens);
    cube::CubeState c = cube::solved_cube();
    for (auto m : scramble) cube::apply_move(c, m);
    for (auto m : solution) cube::apply_move(c, m);
    return cube::is_solved(c);
}

}  // namespace

PYBIND11_MODULE(nxn_cube_py, m) {
    m.doc() = "native c++ 3x3 rubik's cube solver bindings";

    m.def("random_scramble", &py_random_scramble,
          py::arg("length"), py::arg("seed"),
          "generate a deterministic random scramble of the given length");

    m.def("solve", &py_solve,
          py::arg("scramble"),
          "solve a 3x3 given as a list of move-notation strings; returns the solution moves");

    m.def("verify", &py_verify,
          py::arg("scramble"), py::arg("solution"),
          "return true iff applying scramble+solution to a solved cube leaves it solved");
}
