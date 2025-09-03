#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="build"
GEN="Ninja"
TYPE="${TYPE:-Release}"
ARCH="${ARCH:-86}"

if command -v nvcc >/dev/null 2>&1; then
  cmake -S . -B "$BUILD_DIR" -G "$GEN" \
    -DCMAKE_BUILD_TYPE="$TYPE" \
    -DUSE_CUDA=ON \
    -DCMAKE_CUDA_ARCHITECTURES="${ARCH}"
else
  echo "nvcc not found; building CPU-only"
  cmake -S . -B "$BUILD_DIR" -G "$GEN" \
    -DCMAKE_BUILD_TYPE="$TYPE" \
    -DUSE_CUDA=OFF
fi

cmake --build "$BUILD_DIR" -j

echo "== Build done =="
echo "Binary: $BUILD_DIR/bin/thtminer"
