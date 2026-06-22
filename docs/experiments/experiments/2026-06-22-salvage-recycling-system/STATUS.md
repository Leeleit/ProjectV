# STATUS — salvage-recycling-system

**Status:** `concluded-verdict-yes`

---

## Phase log

| Date       | Phase | Event |
|:-----------|:------|:------|
| 2026-06-22 | 0     | Claimed & scaffolded. README.md, STATUS.md created. Reservation in backlog.md. INDEX.md §5 updated. |
| 2026-06-22 | 1     | Web research: Wikipedia Scrap / Vehicle recycling / Ship breaking. 3 primary sources collected. |
| 2026-06-22 | 2     | C++26 prototype written: `prototype/salvage_bench.cpp` (450+ LoC), `CMakeLists.txt`. |
| 2026-06-22 | 2     | Built with Clang 22.1.6, -O3 -march=native -std=c++26. |
| 2026-06-22 | 3     | Benchmark run: 125,000 measurements (5 strats × 5 scenes × 5 seeds × 1000 iter + 10 warmup each). |
| 2026-06-22 | 3     | RESULTS.md written with full analysis tables and verdict. |
| 2026-06-22 | 4     | STATUS.md updated (this file). Backlog & INDEX sync pending. |

## Pending

- [x] Sync backlog.md (close → backlog_closed.md)
- [x] Sync INDEX.md (move §5 → §6)
- [ ] Remove duplicate reservation or confirm slot

## Results summary

- All 5 strategies benchmarked across 5 scenes and 5 seeds.
- **Strategy C (DestructionMethodModifier)** recommended as primary — method-aware recovery at 1.3× baseline cost (~0.35 μs avg).
- **Strategy E (HybridSalvage)** recommended when time decay + team efficiency needed — 2.7× cost but still <2.2 μs for 200-wreck scene.
- **Absolute cost negligible** for ProjectV hot-path (salvage is per-event game logic, not per-frame rendering/physics).

**Verdict:** `concluded-verdict-yes` — salvage/recycling systems are feasible and low-cost. Strategy C should be the default implementation.
