#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CIRCLE_STDLIB="$ROOT/libs/circle-stdlib"

# Default to Raspberry Pi 2
PI_MODEL="${1:-2}"

echo "Building q3lite-circle"
echo "Using Raspberry Pi model: $PI_MODEL"

# Ensure submodules are present
echo "Checking submodules..."
git submodule update --init --recursive

echo
echo "=== Building circle-stdlib ==="

cd "$CIRCLE_STDLIB"

./configure --kernel-max-size=64 -r"$PI_MODEL"

make -j"$(nproc)"

echo
echo "=== Building Circle addons ==="

cd addon/linux
make -j"$(nproc)"

cd ../vc4
./makeall --nosample

cd interface
./makeall

echo
echo "=== Building q3lite kernel ==="

cd "$ROOT/src"

make -j"$(nproc)"

echo
echo "Build complete"
