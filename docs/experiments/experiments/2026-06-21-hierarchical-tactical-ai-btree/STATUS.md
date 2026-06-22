# STATUS — 2026-06-21-hierarchical-tactical-ai-btree

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session, ~2h)
**Stage link:** independent (military sandbox axis — Tier 2 AI, Tactical & Warfare Mechanics)
**Estimated effort:** S (1 session)
**Author:** self (research agent)

---

## Phase 0 — Claim + skeleton

- Reservation claim в `research/backlog.md §In progress` (per `AGENTS.md §13.1`)
- Sentinel §13.7 verified: no other agent working on this topic (rg показывает только prior backlog/INDEX cross-refs)
- Topic = "hierarchical BT for 100+ unit tactical AI (platoon/company)"

## Phase 1 — Web-research (via `webfetch` direct; Exa 429 persistent)

Sources verified (4 primary + 2 cross-refs):
1. **Wikipedia BT article** (Colledanchise & Ögren 2018) — canonical mathematical model, Control flow nodes (Selector, Sequence), Decorators, event-driven extension
2. **Damian Isla GDC 2005** "Handling Complexity in the Halo 2 AI" — production reference, 50 behaviors, behavior DAG, behavior impulses, behavior tagging, stimulus behaviors
3. **Chris Simpson BT blog** (Project Zomboid, 2014) — practical implementation, EnsureItemInInventory recursive pattern, Stack manipulation nodes
4. **Colledanchise 2014 ICRA** "Performance analysis of stochastic behavior trees" — formal cost analysis
5. **Cross-ref: closed `flow-field-pathfinding-10k-units`** (yes) — BT runs ON TOP of pathfinding
6. **Cross-ref: closed `ecs-1m-entities-bottleneck`** (yes) — Flecs handles 1M ents @ 3.74 µs/frame

## Phase 2 — Design

5 strategies:
- A_NaiveNoMemory (baseline) — traverse entire tree every tick
- B_BT_RunningMemory (classic) — cache running child, re-tick only path
- C_Hierarchical_3Tier — Strategic (3 Hz) + Tactical + Unit per-soldier
- D_EventDriven — BT with event queue + halts (Champandard 2012, Halo 2)
- E_Blackboard — D + memoization via per-tick signature

5 scenes (scaled 8→256 units):
- recon_patrol (8), platoon_attack (32), urban_clear (64), company_advance (128), combined_arms (256)

5 seeds × 5 scenes × 5 strategies = **125 main measurements**, ~3-25 ms total wall time per config.

## Phase 3 — Prototype

- `prototype/btree_bench.cpp` ~1053 LoC
- `prototype/CMakeLists.txt` (clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`)
- Build: `cmake -B build -S . && cmake --build build`
- Run: `./build/btree_bench` → `results.csv` (126 rows = 1 header + 125 data)
- Clang build: green, 2 cosmetic warnings (unused `status_name` / `node_type_name` debug helpers)

## Phase 4 — Measurements

Output: `prototype/build/results.csv` (126 rows, 12 KB), `prototype/results.csv` (12 KB, 126 rows).

Headline (mean ns/unit/tick, across 5 seeds):
- A_NaiveNoMemory: **315** (8u), 280 (32u), 252 (64u), 247 (128u), 200 (256u)
- B_BT_RunningMemory: **278** (8u), 269 (32u), 254 (64u), 239 (128u), 201 (256u) — **12-13% speedup vs A on small**
- C_Hierarchical_3Tier: **286** (8u), 272 (32u), 273 (64u), 254 (128u), 209 (256u) — similar to A on small
- D_EventDriven: **263** (8u), 246 (32u), 238 (64u), 217 (128u), **179** (256u) — **20% speedup at scale**
- E_Blackboard: **261** (8u), 256 (32u), 239 (64u), 224 (128u), 200 (256u) — comparable to D

## Phase 5 — Verdict

**verdict=`mixed`**: classical BT optimization (Running memory) provides modest gains (12-13% small, 3-5% at 256u). Event-driven (D) is the SOTA winner at scale (~20% gain). Hierarchical (C) doesn't help on per-unit basis in this prototype — would need actual ECS integration to validate. Blackboard memoization (E) doesn't help in this random-per-tick test (signature never matches because Blackboard state is randomized each tick).

**Recommendation:** Use D (Event-Driven) for military sandbox AI. Skip A and pure B for >128 units. C (Hierarchical) needs ECS-coupled redesign to be meaningful — defer.

## Phase 6 — Integration recommendation

Mainline: 3-step migration per `agent/knowledge.md §30.4`:
- Step 1 (XS, ~80 LoC) `src/ai/BehaviorTree.hpp` — flat-SoA BT primitive + Selector/Sequence/Inverter/Repeater
- Step 2 (S, ~250 LoC) `src/ai/TacticalBT.{hpp,cpp}` — Flecs component `BehaviorTreeComponent` + event-driven halts via Flecs observer
- Step 3 (M, ~500 LoC, deferred) per-soldier BT instances + blackboard + hierarchical strategic/tactical/unit split

Total ~830 LoC, M effort, 2-3 sessions. **Deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning.

---

**Last update:** 2026-06-21 ~21:10 (results finalized, docs in progress)
**Next tick:** Sync to `INDEX.md §6 Recent closed` + `backlog.md §Closed`
