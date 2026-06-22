# STATUS — 2026-06-22-urban-combat-tactics-ai

**Phase:** Phase 4 (writeup + close) — DONE.
**Opened:** 2026-06-22.
**Closed:** 2026-06-22 (single session, ~2.5h from claim to close).
**Last action:** README.md §5/§6/§7 finalized with RESULTS.md data; sources.md with 12 verified sources; prototype/build/results.csv (126 rows) + summary_means.csv (6 rows) generated.
**Blocker:** нет.
**Verdict:** `mixed` per strategy; **`yes`** for **C_Graph_BFS_Interior ⭐ as universal recommended default** (8× ff reduction at 2.8× cost) + **E_CoverAwarePeek_DoorPriority ⭐ as safety-critical opt-in** (0 ff at 22× cost).
**Hypothesis (one-line):** <1 µs/room at 100-room scale (C/E) — **CONFIRMED massively** (max 51.8 ns/room, 19× under target).
**Files touched:**
- `README.md` (full 8-section template per `_TEMPLATE/README.md`)
- `STATUS.md` (this file)
- `sources.md` (8 primary + 4 supplementary verified)
- `RESULTS.md` (10-section headline + per-strategy verdicts + caveats + cross-axis)
- `prototype/urban_combat_bench.cpp` (~880 LoC standalone C++26 CPU)
- `prototype/build/urban_combat_bench` (binary, 103 KiB)
- `prototype/build/urban_combat_asan` (ASAN build, debug only)
- `prototype/build/results.csv` (126 rows = 5 strategies × 5 buildings × 5 seeds + header)
- `prototype/build/summary_means.csv` (6 rows = 1 header + 5 strategy means)
**Cross-axis:** orth to all in-progress parallel; complementary к closed `voxel-topology-analysis` [yes] + `cover-system-terrain-adaptive` [mixed] + `flanking-maneuver-ai` [mixed] + `hierarchical-tactical-ai-btree` [mixed] + `combined-arms-coordination-ai` [mixed] + `flow-field-pathfinding-10k-units` [yes] + `suppression-mechanics` [mixed] + `infantry-soldier-sim` [yes].
**Migration effort:** M (2-3 sessions), deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning.
**Reusable for:** `squad-fire-team-command` [m Tier 2] + `medical-evacuation-chain` [m Tier 2] + `fire-coordination-multiple-units` [m Tier 2] + `soldier-role-specialization` [m Tier 2] — all consume `UrbanCombatSystem::Update` output.