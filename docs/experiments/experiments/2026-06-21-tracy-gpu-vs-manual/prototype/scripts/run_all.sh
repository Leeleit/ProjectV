#!/usr/bin/env bash
# Run all 4 configs × 3 workload scales, aggregate into results.csv.
# Plus drift test (Issue #663) for config B at high passes (15).
#
# Usage: cd prototype && bash scripts/run_all.sh [--no-drift]
# Output: prototype/results.csv + prototype/results_drift.csv

set -euo pipefail

cd "$(dirname "$0")/.."

OUT="results.csv"
TMP="$(mktemp -d)"
RUN_DRIFT=true
for arg in "$@"; do
    case "$arg" in
        --no-drift) RUN_DRIFT=false ;;
    esac
done
echo "config,passes,mean_ms,median_ms,p95_ms,p99_ms,stddev_ms,min_ms,max_ms" > "$OUT"

# Pin to single core for main thread (dev host: 5800X = 16 threads).
TASKSET="taskset -c 2"

# Workload scales per §3: low=3, mid=8, high=15.
WORKLOADS=(3 8 15)

# Configs per §3: A baseline, B Tracy GPU all, C manual only, D hybrid.
CONFIGS=(A B C D)

# Frames per config per workload (per benchmarks/methodology.md §3).
WARMUP=60
FRAMES=1000

# Build with Tracy support if possible.
if [[ -d "../../../../../external/tracy" && -d "../../../../../external/volk" ]]; then
    echo "Building both default and Tracy-enabled variants..."
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
    cmake --build build -j 2>&1 | tail -5
    BIN_DEFAULT="build/bin/tracy_gpu_vs_manual"
    BIN_TRACY="build/bin/tracy_gpu_vs_manual_tracy"
else
    echo "Building only default (Tracy / volk not found at expected paths)..."
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
    cmake --build build -j 2>&1 | tail -5
    BIN_DEFAULT="build/bin/tracy_gpu_vs_manual"
    BIN_TRACY=""
fi

run_config() {
    local cfg="$1"
    local passes="$2"
    local bin="$3"
    local out_csv="$TMP/${cfg}_p${passes}.csv"
    local frames="${4:-$FRAMES}"
    local drift="${5:-}"  # pass --drift-test to enable drift CSV output

    if [[ -z "$bin" ]]; then
        echo "  Skipping config $cfg (binary not built)"
        return
    fi

    echo "Running config=$cfg passes=$passes frames=$frames${drift:+ DRIFT} bin=$(basename $bin)..."
    $TASKSET "$bin" \
        --config="$cfg" \
        --passes="$passes" \
        --warmup="$WARMUP" \
        --frames="$frames" \
        $drift \
        --out="$out_csv" 2>&1 | grep -E "mean=|GPU:|Drift verdict|drift window" || true

    # Extract mean/p95/p99 from CSV (last line per frame has aggregated wall_ms).
    python3 - "$cfg" "$passes" "$out_csv" >> "$OUT" <<'PYEOF'
import csv, sys, statistics
cfg, passes, path = sys.argv[1], int(sys.argv[2]), sys.argv[3]
with open(path) as f:
    rdr = csv.DictReader(f)
    samples = [float(row["wall_ms"]) for row in rdr]
if not samples:
    print(f"{cfg},{passes},,,,,,", flush=True)
    sys.exit(0)
samples.sort()
n = len(samples)
mean = sum(samples) / n
median = samples[n // 2]
p95 = samples[int(n * 0.95)]
p99 = samples[int(n * 0.99)]
stddev = statistics.pstdev(samples) if n > 1 else 0.0
print(f"{cfg},{passes},{mean:.4f},{median:.4f},{p95:.4f},{p99:.4f},{stddev:.4f},{samples[0]:.4f},{samples[-1]:.4f}", flush=True)
PYEOF
}

for passes in "${WORKLOADS[@]}"; do
    for cfg in "${CONFIGS[@]}"; do
        if [[ "$cfg" == "A" ]]; then
            run_config "$cfg" "$passes" "$BIN_DEFAULT"
        elif [[ "$cfg" == "B" || "$cfg" == "D" ]]; then
            run_config "$cfg" "$passes" "$BIN_TRACY"
        else
            run_config "$cfg" "$passes" "$BIN_DEFAULT"
        fi
    done
done

# Drift test (Issue #663 verification): 10K frames for config A baseline + config B
# Tracy GPU all at high passes (15) + config D hybrid at high passes (15).
# Per-1K-window mean reported in results_drift.csv to detect calibration drift.
if [[ "$RUN_DRIFT" == "true" ]]; then
    echo
    echo "=== DRIFT TEST (Issue #663 verification, 10K frames) ==="
    for cfg in A B D; do
        if [[ "$cfg" == "A" ]]; then
            run_config "$cfg" 15 "$BIN_DEFAULT" 10000 --drift-test
        else
            run_config "$cfg" 15 "$BIN_TRACY" 10000 --drift-test
        fi
    done
fi

echo
echo "Done. Results: $(pwd)/$OUT"
# Copy drift CSVs from /tmp to results dir.
for f in $TMP/*_drift.csv; do
    if [[ -f "$f" ]]; then
        cp "$f" "$(pwd)/$(basename $f)"
    fi
done
if compgen -G "$(pwd)/*_drift.csv" > /dev/null; then
    echo "Drift:     $(pwd)/*_drift.csv"
fi
echo
column -ts, "$OUT"
