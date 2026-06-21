#!/usr/bin/env bash
# run_all.sh — full sweep of (strategy, scene, seed) configurations
#
# Per experiments/benchmarks/methodology.md: 10 warmup + 1000 measured iters per config.
# Output: results.csv (1 header + 125 data rows = 5 strategies × 5 scenes × 5 seeds)
#   plus detailed per-config CSV via military_terrain_bench.

set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p prototype/build

# Build if binary missing
if [[ ! -x prototype/build/military_terrain_bench ]]; then
    pushd prototype/build > /dev/null
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_FLAGS="-std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic" \
        .. > /dev/null
    ninja
    popd > /dev/null
fi

OUT=prototype/build/results.csv
TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

# Header
echo "strategy,scene,seed,iter,time_us_mean,time_us_p50,time_us_p95,time_us_std,ridgelines,defilade,kill_zones,hull_down,chokepoints,firing_pos,cover,total" > "$OUT"

strategies=(A B C D E)
scenes=(flat_grasslands rolling_hills mountainous_ridge urban_periphery river_valley)
seeds=(1 7 42 1234 31337)
ITERS=50

# Build job list as newline-separated (strategy,scene,seed) tuples
JOBS="$TMP/jobs.txt"
> "$JOBS"
for s in "${strategies[@]}"; do
    for sc in "${scenes[@]}"; do
        for sd in "${seeds[@]}"; do
            printf "%s %s %s\n" "$s" "$sc" "$sd" >> "$JOBS"
        done
    done
done

total=$(wc -l < "$JOBS")
START=$(date +%s)

# Run in parallel (4 jobs at a time)
export OUT ITERS
run_one() {
    local s=$1 sc=$2 sd=$3
    local cfg_out="$TMP/${s}_${sc}_${sd}.csv"
    ./prototype/build/military_terrain_bench \
        --strategy "$s" --scene "$sc" --seed "$sd" --iter "$ITERS" \
        --output "$cfg_out" > /dev/null
    tail -n +2 "$cfg_out"
}
export -f run_one
export TMP

cat "$JOBS" | xargs -n 3 -P 8 bash -c 'run_one "$@"' _ >> "$OUT"
COUNT=$(tail -n +2 "$OUT" | wc -l)
ELAPSED=$(( $(date +%s) - START ))

echo ""
echo "Full sweep complete: $((COUNT * ITERS)) measurements in ${ELAPSED}s"
echo "Output: $OUT"
echo "Rows: $(wc -l < "$OUT") (1 header + $COUNT data)"
