#!/usr/bin/env bash
# Build script for `2026-06-21-voxel-chunk-streaming-pipeline` prototype.
# Per `agent/knowledge.md` Linux baseline = clang++ 22.1.6.
set -euo pipefail

PROTO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${PROTO_DIR}"

mkdir -p build

echo "[build] clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic"
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
  -o build/stream_bench stream_bench.cpp

echo "[build] done: build/stream_bench"
echo "[run]   ./build/stream_bench --warmup 10 --frames 1000 --output build/results.csv --verbose"
