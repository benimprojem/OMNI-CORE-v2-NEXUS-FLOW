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

The C OCC entry point is `main.c`. The old `parser/main.c` is preserved at `../temp/legacy-c/obsolete-parser-main.c`;
it defines a second `main()` and is deliberately excluded from the active build.

The C code generator is built as a separate shared library. The C++ compiler sources are preserved under `../../temp/legacy-cpp/src/`;
their legacy build scripts are under `../../temp/legacy-cpp/scripts/` and are
not part of this C build.

## Current CTest coverage

The current CTest suite contains eighteen checks:

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
11. Tokenize declaration prefixes, literals and core Nexus Flow operators; verify token positions across lines.
12. Parse a valid source and reject invalid syntax; verify a partial AST remains owned by the parser context for cleanup.
13. Run semantic analysis on a valid source and a source with an undefined identifier; verify the expected diagnostic code.
14. Serialize and read an OIR v1 container; verify round-trip fields and reject truncated streams, bad magic, inconsistent total/section lengths, unknown required sections, missing payloads, invalid no-artifact state and undersized output buffers.
15. Encode and decode SOURCE_UNIT schema 1.0; verify path/source byte round-trip, exact lengths, valid UTF-8 and rejection of malformed UTF-8, embedded NUL, truncation, unsupported source ID and undersized buffers.
16. Verify all 160 frontend token kinds map bidirectionally to stable OIR token IDs; reject unknown enum values, unknown IDs and null output pointers.
17. Collect job-owned diagnostics with owned message/hint storage; verify severity counts, source spans, NOTE-only non-blocking behavior, strict warning/error output gate and fail-closed invalid/allocation states.
18. Create per-job CompilationContext; verify copied options, legal lifecycle transitions, source diagnostics continuing without stopping the job, and final-output eligibility for clean/warning/failed/cancelled jobs.

## Sanitizer build (GCC/Clang)

AddressSanitizer and UndefinedBehaviorSanitizer can be enabled without changing
the normal build:

```sh
cmake -S omnicore-sdk -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DOCC_ENABLE_SANITIZERS=ON
cmake --build build-sanitized --config Debug --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-sanitized -C Debug --output-on-failure
```

Valgrind can be used on Linux after installing it:

```sh
cmake -S omnicore-sdk -B build-valgrind -DCMAKE_BUILD_TYPE=Debug -DMEMORYCHECK_COMMAND=/usr/bin/valgrind "-DMEMORYCHECK_COMMAND_OPTIONS=--leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --track-origins=yes --error-exitcode=99"
cmake --build build-valgrind --config Debug --parallel
ctest --test-dir build-valgrind -T memcheck --output-on-failure
```

The option currently supports GCC and Clang toolchains; MSVC is intentionally
rejected rather than silently ignoring sanitizer flags. GitHub Actions runs
this additional job on Ubuntu.

## Verification status

GitHub Actions workflow run 41145426190 completed successfully:
CMake configure, build and all eighteen CTest checks passed on Windows, Ubuntu
and macOS. Ubuntu GCC ASan/UBSan and Valgrind memcheck also passed all eighteen
tests. This verifies the committed clean-checkout CI path; it does not claim
a separate local build on the user's machine.
