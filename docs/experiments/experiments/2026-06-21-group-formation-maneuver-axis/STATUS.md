# 2026-06-21-group-formation-maneuver-axis — Group Formation Movement & Slot Allocation

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** independent (Stage 6+ military sandbox — Tier 2 AI/Tactical/Warfare Mechanics)
**Estimated effort:** M
**Author:** self (agent)
**Priority:** m
**Cross-axis:** orth to all closed Tier 2 AI (BT/cover/suppression/flanking/combined-arms); complementary to
`flow-field-pathfinding-10k-units` [yes, per-unit movement] + `flanking-maneuver-ai` [mixed, single maneuver,
NOT formation shape].

---

## Phase log

- `2026-06-21 22:30` — Phase 0 init. Reservation зафиксирован в `research/backlog.md §In progress`. Sentinel §13.7
  clean (папки не было, никто параллельно не работает). Web-search (Exa) HTTP 429 → fallback на Startpage + direct
  webfetch (per the web_search fallback chain).
- `2026-06-21 22:42` — Phase 1 web-research complete: **9 primary + 6 secondary = 15 verified sources** в
  [`sources.md`](./sources.md) (Reynolds 1987/1999, van den Berg ORCA 2008, Isla Halo 2 2005, Game AI Pro
  Ch.22, Wikipedia SupCom/HoI4/Military organization, DTIC Swarming PDF, OpenSteer library, V-RVO arXiv 2021,
  Tactical formation Wikipedia, Army University Press). Tier-1 sources верифицированы через direct `webfetch`
  (Reynolds canonical red3d.com, Wikipedia primary articles, ResearchGate paper mirrors).
- `2026-06-21 22:48` — Phase 2 prototype scaffold: 6 strategies (A_Naive_AStar / B_VirtualAnchor_SlotGrid /
  C_HierarchicalAnchor / D_PotentialField_Reynolds / E_ORCA_CollisionAvoidance / F_Hybrid_B_E) × 5 scenes
  (open_plains / forest_scattered / urban_grid / hill_terrain / defensive_line) × 4 unit counts (32, 64,
  128, 256) × 5 seeds × 100 iter = **60,000 main measurements**.
- `2026-06-21 22:55` — Phase 3 build: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra
  -Wpedantic` (2 cosmetic warnings: unused `n` param в `wedgeSlot` + unused `local_count` в `runHybrid`).
- `2026-06-21 23:00` — Phase 4 run: wall time **23.95 sec** на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output: `prototype/build/results.csv` (60,001 rows = 1 header + 60,000 data,
  4.4 MB) + `prototype/build/summary_means.csv` (120 rows = 6 strategies × 5 scenes × 4 unit_counts, 10.8 KB).
- `2026-06-21 23:10` — Phase 5 analysis + RESULTS.md written. **Verdict=mixed:** F_Hybrid_B_E recommended
  as universal default (best cohesion, 0.034% of 30Hz frame for N=256), B_VirtualAnchor for cost-sensitive
  scenarios, A and E rejected.
- `2026-06-21 23:20` — Phase 6 close experiment, sync `INDEX.md` §5+§6 + `backlog.md` §In progress → §Closed.

## Final state

- **Verdict:** `mixed` per strategy; `yes` for F_Hybrid_B_E (universal default) + B_VirtualAnchor (cost-optimal).
- **Headline metrics:** B = 229-296 ns/u (cost winner), F = 443-1322 ns/u (cohesion winner, 1.4-4.5× cost of B).
- **Hypothesis validation:** 3 of 4 confirmed massively, 1 rejected (E_ORCA fallback for tight scenarios).
- **Files:** README.md + STATUS.md + sources.md (15 verified sources) + RESULTS.md +
  prototype/formation_bench.cpp (691 LoC) + prototype/CMakeLists.txt +
  prototype/build/{formation_bench, results.csv (4.4 MB), summary_means.csv (10.8 KB)}.

## Cross-references

- README.md §2 prior art: 9 primary + 6 secondary verified sources.
- README.md §5 results: pivot table + per-strategy analysis.
- README.md §7 integration: 3-step migration per `agent/knowledge.md` precedent (~400 LoC, S effort).
- RESULTS.md §1-9: full analysis, per-scene breakdown, hypothesis validation, caveats.
- INDEX.md §5 → §6 sync на closing.
- backlog.md §In progress → backlog_closed.md §Closed sync на closing.
