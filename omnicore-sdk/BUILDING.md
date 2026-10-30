# OmniCore SDK — Build

## Canonical implementation

The canonical compiler implementation is C. The C++ programs and their existing
build scripts are retained as reference/legacy tools and are not used by this build.

## Requirements

- CMake 3.16 or newer
- A C11-capable compiler:
  - Windows: MSVC or MinGW-w64 GCC
  - Linux: GCC or Clang
  - macOS: Apple Clang or another C11-capable compiler

## Build and test

From the `omnicore-sdk` directory:

### Windows

```bat
build_c.bat
```

Or use CMake directly:

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Linux / macOS

```sh
sh ./build_c.sh
```

Or use CMake directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Output layout

The build places the C executable and code-generation shared library in
`build/bin/`. Runtime configuration and language files are copied there.
Target descriptors are copied to `build/targets/`, matching the current
runtime's relative target lookup.

- Windows: `build/bin/occ.exe` and `build/bin/codegen.dll`
- Linux: `build/bin/occ` and `build/bin/libcodegen.so`
- macOS: `build/bin/occ` and `build/bin/libcodegen.dylib`

Run OCC with `build/bin` as the working directory for this initial build
layout. Runtime path resolution will be made independent of the working
directory in a later reliability task.

## Source selection

The C frontend library contains:

- `parser/ast.c`
- `parser/lexer.c`
- `parser/parser.c`
- `parser/semantic.c`

The C OCC entry point is `main.c`. The old `parser/main.c` is deliberately
excluded because it defines a second `main()` and represents an earlier
standalone parser test entry point.

The C code generator is built as a separate shared library. The C++ sources
under `src/` and the existing `build_occ.bat`, `parser_build.bat`, and
`analyzer_build.bat` scripts are not part of this C build.

## Verification status

The build definitions and wrappers are committed, but they must not be
considered verified until the build and CTest commands have been run on the
corresponding host toolchain. This repository inspection environment does not
execute the user's local Windows/Linux/macOS build.
