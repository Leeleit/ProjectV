#!/bin/bash
set -euo pipefail

cd /workspace

# Ensure a clean CI build: the host may have a local build tree with different paths.
rm -rf /workspace/build/linux-clang-debug-ci
rm -rf /workspace/install/linux-clang-debug-ci

echo "=== Configure linux-clang-debug-ci ==="
cmake --preset linux-clang-debug-ci

echo "=== Build linux-clang-debug-ci-build ==="
cmake --build --preset linux-clang-debug-ci-build

echo "=== Test linux-clang-debug-ci-tests ==="
ctest --preset linux-clang-debug-ci-tests
