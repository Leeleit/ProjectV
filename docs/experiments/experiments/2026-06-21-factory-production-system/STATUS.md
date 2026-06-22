# STATUS — `2026-06-21-factory-production-system`

**Status:** `concluded-verdict-mixed` (per strategy; `yes` for E_ProductionLinePipeline + A_NaiveLinearScan as recommended defaults)
**Date opened:** `2026-06-21`
**Date closed:** `2026-06-21` (single session, ~3h end-to-end)
**Stage link:** `independent` (Stage 6+ military sandbox — Tier 3 Economy; cross-cuts Stage 4.x asset pipeline + Stage 6+ economy tier)
**Estimated effort:** M (single session, ~3h end-to-end)
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй» 2026-06-21)
**Verdict:** `mixed` per strategy; `yes` for **E_ProductionLinePipeline** + **A_NaiveLinearScan** as recommended defaults.

---

## Phase tracker

- [x] **Phase 0 (reservation per `AGENTS.md §13.1`):** DONE — `research/backlog.md §Open → §In progress` + `INDEX.md §5 Active` (sync at close) + folder `experiments/2026-06-21-factory-production-system/`.
  - **Anti-duplicate sentinel §13.7:** `rg "factory-production|factory.production"` → only `backlog.md` `[ ]` line; `ls experiments/2026-06-21-factory*` = ENOENT до claim; `INDEX.md §5` = no parallel reservation. ✓
  - **§13.7 Anti-duplicate confirmed clean.** No prior experiments cover factory production scheduling specifically (verified vs 130+ closed experiments in INDEX.md §6).
- [x] **Phase 1 (skeleton README + STATUS):** DONE — README.md (8 mandatory sections per `_TEMPLATE/README.md` skeleton) + STATUS.md (this file).
- [x] **Phase 2 (web-research):** DONE via direct `webfetch` (Exa HTTP 429 persistent this session per `agent/knowledge.md Part B §9` line 1424 fallback list). **6 primary + 3 secondary + 13 ProjectV cross-refs verified** в [`sources.md`](./sources.md).
- [x] **Phase 3 (prototype implementation):** DONE — `prototype/world_model.hpp` (~250 LoC) + `prototype/factory_bench.cpp` (~500 LoC) standalone C++26 CPU harness. 5 schedulers (A_NaiveLinearScan / B_PriorityBucketQueue / C_DependencyDAG_TopoSort / D_CriticalPathBatch / E_ProductionLinePipeline) + 5 scenes (single_item_uniform / mixed_product_uniform / multi_tier_dependencies / wartime_surge / economic_complex) + 16 item types with dependency chains.
- [x] **Phase 4 (build + run + measurements):** DONE — Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` build green 0 warnings. Wall time < 2 sec. 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements** (126 rows in `prototype/build/results.csv`, 18 KB).
- [x] **Phase 5 (RESULTS.md + sources.md + sync §13.5):** DONE — `RESULTS.md` written (8 sections, full per-strategy × per-scene breakdown, 4 critical findings, integration recommendation). `sources.md` written (6 primary + 3 secondary verified). Close-out sync per §13.5: `backlog.md §In progress → backlog_closed.md §Closed` (pending) + `INDEX.md §5 → §6 Recent closed` (pending).

---

## Current state (2026-06-21)

- **Claimed** `2026-06-21` by self per `AGENTS.md §13.1`.
- **Slug:** `2026-06-21-factory-production-system` (military sandbox axis — Tier 3 Economy).
- **Differentiation vs closed experiments:**
  - `supply-logistics-simulation` [mixed] = supply graph traversal (resource flow), NOT production queue scheduling.
  - `data-driven-vehicle-weapon-definitions` [mixed] = schema storage, downstream input to factory specs.
  - `component-vehicle-damage-model` [yes] = downstream consumer (factory produces → consumer wears down).
  - `lua-game-rules-scripting` [mixed] = mod-scripting hook dispatch, ORTH axis.
  - `lockstep-state-sync-hybrid-netcode` [mixed] = netcode transport, ORTH axis.
  - **0 of 130+ closed experiments cover factory production scheduling specifically.**
- **Cross-axis:** orth to in-progress parallel (`structural-collapse-cascade` Tier 1 Physics); complementary to closed `supply-logistics-simulation` + `data-driven-vehicle-weapon-definitions` + Tier 1 Physics + Tier 2 AI + Tier 1 Netcode.
- **Hypothesis (one-line):** правильная стратегия ∈ {A_NaiveLinearScan, B_PriorityBucketQueue, C_DependencyDAG_TopoSort, D_CriticalPathBatch, E_ProductionLinePipeline} даст **<0.05 ms/factory per tick** для 1000 factories + **≥95% throughput** vs theoretical max + **zero deadlock** в dependency cycles.

---

## Blocker

Нет.

---

## Next action

Phase 2 → Phase 3: web-research (sources.md) → prototype implementation (`prototype/factory_bench.cpp`).

---

## Decision log

- `2026-06-21`: claimed slug per `AGENTS.md §13.1` (factory-production-system was the highest-priority still-open Tier 3 Economy topic; sentinel §13.7 clean; orth to all in-progress parallel sessions + 130+ closed experiments).
- `2026-06-21`: skeleton README + STATUS created per `_TEMPLATE/README.md` 8-section format.