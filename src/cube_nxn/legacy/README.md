# Legacy 4x4 reduction

This directory preserves the original specialized 4x4 reduction path for
regression tests, comparison benchmarks, and historical reference.

Production solving does not link this library. Cubes from 4x4 through 7x7 use
the general-N reduction path in `reduction_general.cpp`; 3x3 cubes continue to
use the Kociemba solver directly.

Build the legacy artifact explicitly with:

```sh
cmake --build build --target cube_n4_legacy
```

The C++ test target also links it so the historical implementation remains
executable and verified.
