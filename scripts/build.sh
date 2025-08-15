#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="build"
GEN="Ninja"
TYPE="${TYPE:-Release}"

# Можно передать ARCH=89 ./scripts/build.sh
ARCH="${ARCH:-86}"

export CC=/usr/bin/gcc-12
export CXX=/usr/bin/g++-12

cmake -S . -B "$BUILD_DIR" -G "$GEN" \
  -DCMAKE_BUILD_TYPE="$TYPE" \
  -DCMAKE_C_COMPILER=/usr/bin/gcc-12 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-12 \
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-12 \
  -DCMAKE_CUDA_ARCHITECTURES="${ARCH}"

cmake --build "$BUILD_DIR" -j
echo "== Build done =="
echo "Binary: $BUILD_DIR/bin/thtminer"
