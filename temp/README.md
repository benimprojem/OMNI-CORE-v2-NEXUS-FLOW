# Temporary archive

This directory contains only C-related legacy material and generated C build snapshots that are excluded from the active build.

- `legacy-c/`: obsolete standalone C parser entry point, excluded from CMake.
- `legacy-tests/`: older standalone NexusFlow test inputs not used by current CTest.
- `build-artifacts/c-snapshot/`: historical C build outputs and generated test artifacts; not loaded by the active build.
- `build-artifacts/oce/`: historical editor object output.
- `duplicate-docs/`: duplicate document copies retained for reference; active copies remain in their canonical locations.

The active compiler implementation, build system and tests are C-only and live under `omnicore-sdk/src/c/` and the C SDK build files. The independent editor project under `omnicore-sdk/src/oce/` is not part of the compiler build.
