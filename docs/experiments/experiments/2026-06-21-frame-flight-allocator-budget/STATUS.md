# STATUS — 2026-06-21-frame-flight-allocator-budget

**Phase:** concluded (verdict=mixed)
**Started:** 2026-06-21
**Closed:** 2026-06-21 (single session)
**Last action:** 2026-06-21 — prototype built + run + 5 strategies measured + stress pass + analysis written + integration recommendation drafted.

## Verdict
**`mixed`** — hypothesis (H1-H4) **подтверждена частично**.

- ✅ **(H1) Hard cap saves OOM crashes**: Strategy D в stress test = 21 clean failures.
- ✅ **(H2) WITHIN_BUDGET cost ≈ 0**: 34.7 vs 35.5 µs mean (within noise).
- ✅ **(H4) Per-frame observability через `vmaGetHeapBudgets`**: 6161-6175 MiB budget observable.
- ❌ **(H3) Per-frame ring buffer does NOT stabilise p99 latency at current scale**: Strategy E p99 = 113 µs vs A's 67 µs.

## Sync obligations (per `AGENTS.md §13.5`)
- `backlog.md`: §Open → §In progress (✅ 2026-06-21) → §Closed (✅ 2026-06-21)
- `INDEX.md`: §5 Active → §6 Recent closed (✅ 2026-06-21)
- `README.md`: status `in-progress` → `concluded-verdict-mixed` (✅ 2026-06-21)

## Active claims (CLOSED)
- `research/backlog.md §Closed` — `2026-06-21-frame-flight-allocator-budget` (m, Stage 6.2 tech-debt, closed 2026-06-21)
- `INDEX.md §6 Recent closed` — entry added

## Files produced this session

| File                                                                                                | LoC  | Purpose                                          |
|:----------------------------------------------------------------------------------------------------|:-----|:-------------------------------------------------|
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/README.md`                   | ~430 | Full experiment writeup (8 sections per template) |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/STATUS.md`                   | ~30  | This file                                         |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/sources.md`                  | (linked from README §8)                              |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype/main.cpp`          | ~125 | Entry point + strategy harness                     |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype/harness.hpp`       | ~170 | Vulkan 1.4 base setup + VMA allocator             |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype/strategies.hpp`   | ~470 | 5 strategy classes (A/B/C/D/E) + stress pass       |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype/benchmark.hpp`     | ~75  | Stats { mean, median, p95, p99, stddev }          |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype/CMakeLists.txt`     | ~50  | Build config                                       |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype/README.md`        | ~50  | Build + run instructions                          |
| `docs/experiments/experiments/2026-06-21-frame-flight-allocator-budget/prototype/build/results.csv`| ~10  | Machine-readable measurements                    |

## Hot-path recap
Standalone Vulkan 1.4 harness comparing 5 allocator strategies (default VMA / budget-
tracked VMA / per-frame linear pool / per-frame + hard cap / pre-created ring buffer).
Production-realistic finding: **default VMA + `WITHIN_BUDGET` flag = best ROI** for
ProjectV's current scale. Pre-created ring buffer = production-grade pattern for
Stage 4.3 / 5.2 re-evaluation trigger.

## Cross-refs
- `agent/workspace.md` — mainline session running parallel (mesh shader 2x part 6); no scope conflict.
- `agent/knowledge.md` — 3-step migration precedent (Step 1 budget + Step 2 hard cap + Step 3 pre-created ring buffer).
- `hardware-profile.md §3` — RTX 3060 Ti 8 GiB / 5.06 GiB driver budget; dev host validation baseline.
- 6 prior experiments — `bindless-descriptor-overhead`, `dec-pipelines-async-compute`,
  `nanovdb-on-gpu`, `clustered-forward-mass-lights`, `vct-vs-rt-cutoff`, `rt-shadows-vs-csm`
  — все упоминают growing transient SSBO pressure; this experiment quantifies the
  cross-cutting allocator solution.
