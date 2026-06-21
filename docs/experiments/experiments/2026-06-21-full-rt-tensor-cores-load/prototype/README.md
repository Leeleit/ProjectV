# Prototype — full-rt-tensor-cores-load

**Status:** closed `2026-06-21` verdict=`mixed` (per `STATUS.md`).

## Files

- `cycle_budget.cpp` ~620 LoC (Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG, **0 warnings**).
- `build/cycle_budget` (binary, 90 KB).
- `build/results.csv` (490 rows × 20 cols, 161 KB — 490,000 main measurements).
- `run.log` (3.5 KB).
- `README.md` (this file).

## Build (reproducible per `AGENTS.md §1` ad-hoc `clang++` workflow)

```bash
cd /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-21-full-rt-tensor-cores-load/prototype
mkdir -p build && cd build
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic ../cycle_budget.cpp -o cycle_budget
./cycle_budget --candidates=all --workloads=all --seeds=5 --iter=1000 --csv=results.csv
```

## Cross-refs

- [`../README.md`](../README.md) — full hypothesis + method + results + integration recommendation.
- [`../RESULTS.md`](../RESULTS.md) — 193 lines, full numerical synthesis.
- [`../sources.md`](../sources.md) — 33 sources, 4-tier, ~140 lines.
- [`../STATUS.md`](../STATUS.md) — closure sync notes.
- `../../research/backlog.md §Closed` — closure entry.
- `../../../hardware-profile.md §1/§3/§4` — Zen 3 5800X + RTX 3060 Ti GA104 + Vulkan 1.4.341 + RT/tensor extensions.
- `../../../benchmarks/methodology.md §3` — measurement protocol.
