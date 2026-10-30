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

OCC now resolves `occ.ocf`, language files, target descriptors and the
code-generation shared library relative to the executable location. It can
therefore be launched from a different working directory. Source input and
generated `.oir` / `.s` output paths remain based on the source path supplied
by the caller.

## Intermediate file note

The current C compiler writes an internal AST serialization stream to files with
the `.oir` extension so the separately loaded code generator can read it.
This is not yet the canonical, stable OIR format described in the roadmap, and
it is not compatible with the C++ OIR2 format. The AST stream layout is still
being validated; regenerate previously saved `.oir` files when changing
compiler revisions rather than treating them as a stable interchange format.

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

## Current CTest coverage

The current CTest suite contains ten checks:

1. Reject truncated, invalid and oversized AST streams; round-trip every AST node tag and every serialized field using byte-equality; verify AST_EXTERN_DECL and AST_LITERAL regressions, AST_UNPACK/AST_NEXUS_HALT, a 700-node sibling list, and parser_save_ir failure propagation.
2. Validate all five current target descriptors, reject malformed/duplicate/unknown/oversized fields, and distinguish valid target descriptions from backend support.
3. OCC help output.
4. Reject an unknown CLI option.
5. Reject a missing source file.
6. Accept `--target` before the source path.
7. Reject `--target` without a value.
8. Reject multiple source paths.
9. Compile `tests/test1.nxf` through the C frontend, semantic analysis, AST serialization/deserialization and shared code generator.
10. Copy OCC to a separate runtime folder with a Turkish config whose language key appears after section headers; verify the Turkish undefined-variable diagnostic is loaded.

## Sanitizer build (GCC/Clang)

AddressSanitizer and UndefinedBehaviorSanitizer can be enabled without changing
the normal build:

```sh
cmake -S omnicore-sdk -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DOCC_ENABLE_SANITIZERS=ON
cmake --build build-sanitized --config Debug --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-sanitized -C Debug --output-on-failure
```

The option currently supports GCC and Clang toolchains; MSVC is intentionally
rejected rather than silently ignoring sanitizer flags. GitHub Actions runs
this additional job on Ubuntu.

## Verification status

The latest GitHub Actions matrix completed successfully on Windows, Ubuntu and
macOS. CMake configure, C build and all ten CTest checks passed on each runner.
This verifies the committed clean-checkout CI path; it does not claim that a
separate local build has been run on the user's machine.
