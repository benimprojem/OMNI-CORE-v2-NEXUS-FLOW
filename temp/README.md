# Temporary archive

This directory preserves material moved out of the active source/build layout.
Nothing here is part of the canonical C compiler build.

- `legacy-cpp/src/`: preserved C++ parser, analyzer and old OCC entry point.
- `legacy-cpp/scripts/`: archived C++ build scripts, adjusted to their new paths.
- `legacy-c/`: obsolete standalone C parser entry point, excluded from CMake.
- `legacy-docs/`: historical C++ plan and old command notes.
- `legacy-tests/`: older standalone NexusFlow test inputs not used by current CTest.
- `build-artifacts/`: previously tracked EXE/DLL/static libraries/object files and generated OIR/assembly outputs. Kept for reference; not loaded by the active build.
- `duplicate-docs/`: byte-for-byte duplicate document copies. The active copies remain in their original canonical documentation locations.

The active C compiler source is now `omnicore-sdk/src/c/`. The CMake build uses that path. The separate `omnicore-sdk/src/oce/` editor project is intentionally left in place.
