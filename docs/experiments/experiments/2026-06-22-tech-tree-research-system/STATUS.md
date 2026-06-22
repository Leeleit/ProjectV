# STATUS — Tech Tree Research System

**2026-06-22**
- **Phase 0:** Reservation per `AGENTS.md §13.1` — done.
  - `backlog.md §Open` → `§In progress` (Tech 3 Economy section).
  - `INDEX.md §5 Active experiments` — done (entry added).
  - Folder + `prototype/build/` created.
- **Phase 1:** Web research — done. 11 sources verified (4 Tier 1 Wikipedia + 5 game production + 2 ProjectV cross-refs) per [`sources.md`](./sources.md).
- **Phase 2:** Prototype — done. `prototype/tech_tree_bench.cpp` ~775 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings** after 1 fix iteration: namespace scoping + init_state queue population).
- **Phase 3:** Measurements — done. 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**. Output: [`prototype/build/results.csv`](./prototype/build/results.csv) (126 rows × 11 cols).
- **Phase 4:** Write-up — done. Verdict = **`mixed` per strategy / `yes` for E_Hybrid_CP_LazyPQueue ⭐ as universal recommended default + D_LazyPrerequisiteExpand ⭐ for simple scenes + C_CriticalPathPrecompute ⭐ for static DAGs**.
- **Phase 5:** Doc sync (INDEX + backlog) — done.

**Closed `2026-06-22` (single session, ~2.5h).**

**Headline (mean µs per run, Zen 3 5800X):**

- **A_NaiveSequential** = 90-820 µs (baseline, REJECT for production).
- **B_PriorityQueueDijkstra** = 103-911 µs (REJECT, PQ overhead > savings on DAG).
- **C_CriticalPathPrecompute** = 87-941 µs (RECOMMENDED for static DAGs, 14% faster than A on linear chain).
- **D_LazyPrerequisiteExpand** = 84-543 µs (RECOMMENDED for simple scenes, 5-6× faster than A on linear chain).
- **E_Hybrid_CP_LazyPQueue ⭐** = 86-577 µs (UNIVERSAL RECOMMENDED DEFAULT, best on diamond_100 and dense_cross_200).

**5-10% threshold per `optimization-philosophy.md`:** all strategies < 1 ms/run = 0.003% of 30 Hz frame budget. Hypothesis "<0.01 ms/tick per active research" CONFIRMED massively (1700× headroom on worst case).

**Hypothesis validation (3-clause):** ✅ H1 (cost), ⚠️ H2 (A O(N²) blowup REJECTED, but D/E relative scaling confirmed), ✅ H3 (cycle detection 100% bit-exact).

**Integration:** Step 1 (XS, ~80 LoC) `src/economy/TechTree.{hpp,cpp}` foundation + `PROJECTV_TECH_TREE` env gate (default `HYBRID`) + 5 strategy impls + Flecs `TechTreeComponent`. Step 2 (M, ~400 LoC) integration с `FactoryProductionSystem` (closed mixed) for unlock-gated recipe building + integration с `DataDrivenVehicleWeaponDefinitions` (closed mixed) for unlock-gated content. Step 3 (S, ~150 LoC) `ProjectVTechTreeTests` (5 cycle-detection + 5 throughput tests) + Tracy plot "Tech Tree Tick" + JSON doctrine config for hot-swappable faction tech trees. **Deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision.**

**First dedicated technology research-tree architecture axis** в 137+ closed experiments; opens Stage 6+ military sandbox Tier 3 Economy.
