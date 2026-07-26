// expose notation-string APIs so callers never touch c++ state internals

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>   // for std::vector <-> python list auto-conversion

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "cube_nxn/cube_nxn.h"

namespace py = pybind11;

namespace {

void validate_n(int n) {
    if (n < 3 || n > 7) {
        throw std::invalid_argument("cube size must be in [3, 7]");
    }
}

std::vector<cube_nxn::Move> notation_to_moves(
    int n, const std::vector<std::string>& tokens,
    bool allow_fixed_center_moves = false) {
    std::vector<cube_nxn::Move> out;
    out.reserve(tokens.size());
    for (const auto& t : tokens) {
        auto m = cube_nxn::parse_move(t);
        if (!m) throw std::invalid_argument("unrecognized move: '" + t + "'");
        if (m->inner_depth >= n - 1) {
            throw std::invalid_argument(
                "move exceeds " + std::to_string(n) + "x" +
                std::to_string(n) + " depth: '" + t + "'");
        }
        const int middle_depth = n / 2;
        if (!allow_fixed_center_moves && n % 2 == 1 &&
            m->outer_depth <= middle_depth &&
            m->inner_depth >= middle_depth) {
            throw std::invalid_argument(
                "move changes the fixed-center frame on " +
                std::to_string(n) + "x" + std::to_string(n) +
                ": '" + t + "'");
        }
        out.push_back(*m);
    }
    return out;
}

std::vector<std::string> moves_to_notation(
    const std::vector<cube_nxn::Move>& moves) {
    std::vector<std::string> out;
    out.reserve(moves.size());
    for (const auto& move : moves) {
        out.emplace_back(cube_nxn::format_move(move));
    }
    return out;
}

std::vector<std::string> py_random_scramble(
    int length, uint64_t seed, int n) {
    validate_n(n);
    if (length < 0) throw std::invalid_argument("length must be >= 0");
    return moves_to_notation(cube_nxn::random_scramble(n, length, seed));
}

std::vector<std::string> py_solve(
    const std::vector<std::string>& scramble_tokens, int n) {
    validate_n(n);
    const auto scramble = notation_to_moves(n, scramble_tokens);
    cube_nxn::NxNCube cube(n);
    for (const auto& move : scramble) cube_nxn::apply_move(cube, move);

    cube_nxn::SolveResult result;
    {
        py::gil_scoped_release release;
        result = cube_nxn::solve_nxn(cube);
    }
    if (!result.ok || !cube.is_solved()) {
        throw std::runtime_error("NxN solver failed to produce a verified solution");
    }
    return moves_to_notation(result.moves);
}

bool py_verify(const std::vector<std::string>& scramble_tokens,
               const std::vector<std::string>& solution_tokens,
               int n) {
    validate_n(n);
    const auto scramble = notation_to_moves(n, scramble_tokens);
    const auto solution = notation_to_moves(n, solution_tokens, true);
    cube_nxn::NxNCube cube(n);
    for (const auto& move : scramble) cube_nxn::apply_move(cube, move);
    for (const auto& move : solution) cube_nxn::apply_move(cube, move);
    return cube.is_solved();
}

}  // namespace

PYBIND11_MODULE(nxn_cube_py, m) {
    m.doc() = "native C++ 3x3 through 7x7 Rubik's cube solver bindings";

    m.def("random_scramble", &py_random_scramble,
          py::arg("length"), py::arg("seed"), py::arg("n") = 3,
          "generate a deterministic random scramble of the given length");

    m.def("solve", &py_solve,
          py::arg("scramble"), py::arg("n") = 3,
          "solve an NxN scramble and return the solution moves");

    m.def("verify", &py_verify,
          py::arg("scramble"), py::arg("solution"), py::arg("n") = 3,
          "return true iff scramble plus solution solves the selected cube");
}
