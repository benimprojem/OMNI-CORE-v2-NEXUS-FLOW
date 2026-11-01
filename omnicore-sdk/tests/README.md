# OCC SDK Tests

## OIR v2 Reader/Writer structural tests

The standalone test `oir_reader_tests.cpp` checks:

- Writer → binary file → Reader round-trip.
- Header version rejection.
- Truncated fixed-width instruction rejection.
- Offset beyond end-of-file rejection.
- Target field without an in-field NUL terminator rejection.
- No partially parsed symbol/instruction arrays are exposed after a failed load.

Run from the `omnicore-sdk/` directory with a C++17 compiler:

```bash
g++ -std=c++17 -Wall -Wextra tests/oir_reader_tests.cpp -o tests/oir_reader_tests
./tests/oir_reader_tests
```

On Windows with MinGW:

```bat
g++ -std=c++17 -Wall -Wextra tests/oir_reader_tests.cpp -o tests/oir_reader_tests.exe
tests\oir_reader_tests.exe
```

These are source-level regression tests. They are not represented as passing until executed in a build environment.
