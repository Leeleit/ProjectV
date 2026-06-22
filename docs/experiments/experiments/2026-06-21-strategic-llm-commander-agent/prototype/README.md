# `2026-06-21-strategic-llm-commander-agent` — Prototype

## Build

```bash
cd prototype
mkdir -p build && cd build
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
        -o strategic_llm_bench ../strategic_llm_bench.cpp
```

## Run

```bash
./strategic_llm_bench
```

## Output

- `prototype/build/results.csv` — 125 data rows + 1 header (5 strategies × 5 scenes × 5 seeds).
- `prototype/build/summary_means.csv` — 5 data rows + 1 header (per-strategy mean across all configs).
- stderr — per-config live progress with mean/median/p95/p99/std latency + quality percentiles.

## Files

- `strategic_llm_bench.cpp` (~450 LoC) — self-contained C++26 CPU-only analytical model.
  - 5 strategies (A_Heuristic / B_RAG / C_Hierarchical / D_ReAct / E_PureTactical).
  - 5 scenes (early_war_breakthrough / mid_war_2front / late_war_attrition / naval_invasion / defensive_counterattack).
  - 5 seeds × 10 warmup + 1000 main = 125,000 main measurements.
  - Mock-LLM cost/quality calibrated to IFPV 19.4% improvement + ReAct 34% ALFWorld + CICERO top-10% human-level + HoI4 production baseline.
  - Deterministic SplitMix64 PRNG + state hash for full reproducibility.
  - Per `benchmarks/methodology.md` §3 protocol (10 warmup + 1000 main).
