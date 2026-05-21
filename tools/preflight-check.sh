#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

BUILD_DIR=${MERCURYCHAT_PREFLIGHT_BUILD_DIR:-build-preflight}
BUILD_TYPE=${MERCURYCHAT_PREFLIGHT_BUILD_TYPE:-RelWithDebInfo}

printf '%s\n' '==> Configuring Mercury Chat test build'
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DMERCURYCHAT_USE_HAMLIB=OFF

printf '%s\n' '==> Building Mercury Chat'
cmake --build "$BUILD_DIR" --parallel

printf '%s\n' '==> Running Mercury Chat tests'
ctest --test-dir "$BUILD_DIR" --output-on-failure
