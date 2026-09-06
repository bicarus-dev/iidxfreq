#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
BUILD_TYPE="${BUILD_TYPE:-Release}"
TOOLCHAIN_64="${TOOLCHAIN_64:-$(pwd)/cmake/mingw64.cmake}"
case "$BUILD_TYPE" in
    Release) OUTPUT="$(pwd)/bin/iidxfreq" ;;
    Debug) OUTPUT="$(pwd)/bin/iidxfreq/debug" ;;
    *) echo "Unsupported build type: $BUILD_TYPE" >&2; exit 1 ;;
esac

export CCACHE_DIR="${CCACHE_DIR:-$(pwd)/.ccache}"
export CMAKE_C_COMPILER_LAUNCHER=ccache
export CMAKE_CXX_COMPILER_LAUNCHER=ccache
BUILDDIR="${BUILD_ROOT:-$(pwd)}/cmake-build-${BUILD_TYPE,,}-mingw64"

cmake -S . -B "$BUILDDIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_64" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$OUTPUT"
cmake --build "$BUILDDIR" --target iidxfreq --parallel "$(nproc)"
echo "MinGW build complete: $OUTPUT"