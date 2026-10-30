#!/usr/bin/env sh
set -eu

SDK_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="$SDK_DIR/build"

cmake -S "$SDK_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release
ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure
