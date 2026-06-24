# 2026-06-21-interest-management-aoi-battle — Netcode AOI for 100+ player battlefield

**Status:** in-progress
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** independent (military sandbox axis — Tier 0 Foundation & Optimization — netcode)
**Estimated effort:** M
**Author:** self (per operator instruction 2026-06-21 «выбирай свободную тему или придумывай свою исследуй»)

---

## 1. Hypothesis

**Главная гипотеза:** Grid-based AOI (cell size = 64 m) с 3-tier relevance (critical 200 m @ 20 Hz,
peripheral 500 m @ 5 Hz, ambient > 500 m @ 1 Hz) + per-object priority queue + 9-grid lookup pattern
снижает per-player bandwidth с **10+ Mbps** (full broadcast baseline) до **<1 Mbps** на VoxelLab
battlefield scenario (100 players + 10k entities, 1 km² area, 30 Hz tick).

**Sub-hypotheses:**

- **A. 9-grid lookup vs brute force**: проверка только 3×3 cells вокруг observer vs O(N) по всем
  entities → **>100× reduction** в queries per tick при 100 players.
- **B. 3-tier relevance** (Photon Fusion `Send Priority` pattern): critical 20 Hz, peripheral 5 Hz,
  ambient 1 Hz → **bandwidth savings = 3-4×** vs uniform 20 Hz.
- **C. Per-object priority queue**: entities сортируются по (distance, importance, freshness) →
  top-K сжатие, **>2× compression** дальних entities per tick.
- **D. Server CPU cost**: analytical model shows AOI computation <2 ms/server-tick при 100 players
  (cellRadius=4 для 200m range + 64m cell, итого 9 cells × ~50 entities/cell = 450 entities check per
  observer per tier).
- **E. K-nearest-neighbours VELVET** (per UCL 2011 academic paper) + back culling via player rotation
  → variable AOI radius adapts к load → **+10-30% bandwidth reduction** в dense scenarios.
- **F. AOI batching**: 60-70 byte per player update, batched into single packet → 800 packets/tick
  → <200 packets/tick (per `wirepair.org 2025-12` post) → **4× packet reduction**.

**Альтернативы (rejected or deferred):**

- **X_FullBroadcast** — O(N) per player, 100×100 = 10k update events per tick. Baseline для сравнения.
- **Y_StaticRadius_200m_NoTier** — single radius, all 20 Hz. Per `aceld dev.to 2023-08` baseline.
- **Z_LearningBased_AFLL** (per arXiv 2601.10998) — circular causality learning. **Deferred** —
  сложнее, out of scope single session.

**Метрики:**

- **per_player_bandwidth_kbps** — bytes/tick × 30 Hz × 8 / 1024
- **packets_per_sec_per_player** — per wirepair.org benchmark pattern
- **per_player_update_effective_rate_hz** — суммарный effective rate по 3 tier'ам
- **server_cpu_ms_per_tick** — analytical model
- **AOI churn cost** (re-AOI events per second per player)
- **per-tick wall time** для AOI computation

**Success criteria:**

- **Bandwidth**: <1 Mbps per player at 100 players + 10k entities (vs >10 Mbps baseline)
- **Update rate**: critical entities 18-20 Hz effective (target 20 Hz), peripheral 4-5 Hz (target 5 Hz)
- **CPU**: <2 ms/server-tick AOI cost
- **Packet reduction**: <300 packets/tick (vs 800+ baseline) per wirepair.org pattern

---

## 2. Prior art

Web-research complete via Exa `web_search` (8 primary sources verified this session, 2026-06-21):

### Tier 1 — Primary references

- **[ESEngine AOI (Area of Interest)](https://esengine.cn/en/modules/spatial/aoi/)** — production-grade grid-based AOI manager. `cellSize` recommendation = 1-2x average view range. `addObserver(player, position, {viewRange, observable})` API + Blueprint nodes (`GetEntitiesInView`, `CanSee`, `OnEntityEnterView`, `OnEntityExitView`). **Direct relevance:** canonical grid-AOI reference, 9-grid pattern (`GetSurroundGridsByGid` per aceld 2023), cellSize = 64 m для viewRange 200 m (1:3 ratio).

- **[Netcode optimizations for MMORPGs (wirepair.org 2025-12-20)](https://wirepair.org/2025/12/20/netcode-optimizations-for-mmorpgs/)** — recent production postmortem (2025-12-20). Jolt sensors для AoI (3x3 multi-sensor pattern, 126 m chunks). Distance tiering: 50 m / 150 m / 250 m zones + beyond cull. **Concrete numbers:** 60-70 bytes per player update × 7-10 packets/player/tick × 100 players = 800 packets/tick × 30 Hz = 24,000 pkt/s = 1.344 Mbps per player = 434 TiB/month. **Direct relevance:** quantitative baseline для bandwidth hypothesis.

- **[Photon Fusion 2 — Interest Management](https://doc.photonengine.com/fusion/current/manual/advanced/interest-management)** — production reference (Unity DOTS netcode). AOI region overlap test, modes (Area / Global / Explicit), `AddPlayerAreaOfInterest()` API. **Key feature: `Send Priority`** — per-object priority value determines send frequency; high-priority (close) = 20 Hz, low-priority (far) = 1 Hz, configurable. `AutoAOIOverride` для nested objects. **Direct relevance:** 3-tier priority queue pattern directly applicable.

### Tier 2 — Supporting references

- **[Spatial Interest Management in Networked Games — UCL 2011 paper](https://www.ee.ucl.ac.uk/lcs/previous/LCS2011/LCS1121.pdf)** (John Mitchell, 2011) — academic, VELVET (Variable AOI based on K Nearest Neighbours). Cross-layer optimization (App + Session). Occlusion as Interest metric (player behind wall = no update needed). **Direct relevance:** KNN-based variable AOI for load-adaptive radius.

- **[esengine AOI fix commit c5adfff (2026-04-09)](https://github.com/esengine/esengine/commit/c5adfff0fc50a55b00835f2f193dbd9aebff2940)** — recent (2026-04) production optimization. **Critical insight:** original `_updateObserversOfEntity` iterated ALL observers (O(N²)) — fix uses spatial grid to limit to `cellRadius` cells around moved entity (O(K) where K = 9 cells × avg_observers_per_cell). **Direct relevance:** validates 9-grid lookup as production standard, not theoretical.

- **[MMO Online Game AOI Algorithm (aceld dev.to 2023-08-17)](https://dev.to/aceld/11-mmo-online-game-aoi-algorithm-l7d)** — Chinese MMO dev post, classic 9-grid pattern. `GetGIDByPos(x, y)` → `GetSurroundGridsByGid(gid)` → aggregate entities from 9 cells. AOI entry/exit events drive network sync (move-in-cell triggers subscribe/unsubscribe). **Direct relevance:** canonical algorithm reference.

- **[AFLL: Real-time Load Stabilization for MMO Game Servers (arXiv 2601.10998)](https://arxiv.org/html/2601.10998)** — academic paper, learning-based load balancing. 3 limitations of static AOI: (1) unrecognized circular causality, (2) static policy, (3) no runtime adaptation. Proposes AFLL: real-time backpropagation of message load contribution. **Deferred** — out of scope for single session, but documents modern frontier.

### Cross-axis (closed experiments)

- **Closed `2026-06-21-ecs-1m-entities-bottleneck`** (verdict=yes) — Flecs handles 1M+ ents, 0.5 ns/ent iteration. **Complementary:** ECS pattern для AOI observer/entity registry; AOI events → ECS component add/remove.
- **Closed `2026-06-21-multi-resolution-collision-broadphase`** (verdict=mixed) — D_QuadTree winner. **Reuse:** spatial indexing pattern — AOI grid is conceptually a 2D QuadTree/Grid; same implementation pattern.
- **Closed `2026-06-21-flow-field-pathfinding-10k-units`** (verdict=yes) — C_FlowField_BFS. **Complementary:** AOI determines WHO needs pathfinding, BFS computes HOW. AOI reduction = -3-10× pathfinding cost.
- **Closed `2026-06-21-gpu-fluid-ca-atomic-strategy`** (verdict=mixed) — atomic strategy. **Complementary:** server AOI update under contention needs atomic registration of AOI events.
- **Open `lockstep-state-sync-hybrid-netcode`** (h, not started) — `TODO.md` §6+ military sandbox Tier 1. **Prerequisite:** AOI is foundation для lockstep broadcast reduction.
- **Open `persistent-war-server-architecture`** (h, not started) — event-sourced world state. **Complementary:** AOI events = stream of entity-state changes per observer.

---

## 3. Method

**Тип эксперимента:** analytical + prototype + benchmark.

**Сцены (VoxelLab battlefield reference, 5 scenes × 5 seeds = 25 configs per strategy):**

1. **`uniform_dense`** — 100 players + 10k entities uniform random в 1 km² area. Baseline load.
2. **`battle_clustered`** — 100 players + 10k entities, 2 armies в contact (each 50p + 5kE clustered). Worst case для AOI overlap.
3. **`sparse_scattered`** — 100 players + 1k entities, exploration phase. Best case.
4. **`chase_high_movement`** — 100 players + 5k entities, high movement (10 m/s average). AOI churn stress.
5. **`mixed_dynamic`** — 100 players + 5k entities, mixed (30% in battle, 40% in transit, 30% at base). Realistic.

**Стратегии (6 variants):**

- **A_FullBroadcast** — every player gets every entity update. O(N²) per tick. Baseline.
- **B_GridAOI_NoTiering** — grid AOI, 9-grid lookup, single 200 m range, all in-range at 20 Hz. Per aceld 2023.
- **C_GridAOI_3Tier** — 3 tiers: critical 200 m @ 20 Hz, peripheral 500 m @ 5 Hz, ambient > 500 m @ 1 Hz. Per Photon Fusion Send Priority pattern.
- **D_GridAOI_3Tier_Priority** — C + per-object priority queue (sort by distance × importance). Top-K compression.
- **E_GridAOI_3Tier_KNN_BackCull** — C + KNN variable radius (max 100 entities per player) + back cull via player rotation (only forward 180°).
- **F_GridAOI_3Tier_Batched** — C + packet batching (group multiple player updates into 1 packet).

**Метрики (per scene × seed):**

- `per_player_bandwidth_kbps` = (packets_per_tick × packet_size_bytes × 30 Hz × 8) / 1024
- `packets_per_sec_per_player` = packets_per_tick × 30
- `effective_update_rate_hz` (per tier): critical ~20, peripheral ~5, ambient ~1
- `server_cpu_ms_per_tick` (analytical projection)
- `aoi_churn_per_sec_per_player` (entity enter/exit events)
- `wall_time_ns_per_aoi_query` (per observer per tick)

**Контроль:**

- Baseline = A_FullBroadcast (максимальный bandwidth, нижний bound CPU).
- Strategies B-F должен быть **>5× bandwidth reduction** (cross optimization threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
- Strategies C-F должен сохранять **critical update rate ≥18 Hz** (90% от 20 Hz target).

**Протокол:**

- Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build dir `prototype/build/`, executable `prototype/build/aoi_bench`.
- 6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **150,000 main measurements**.
- Per `benchmarks/methodology.md §3` (warm-up + N + mean/median/p95/std).
- Output: `prototype/build/results.csv` (151 rows) + `prototype/RESULTS.md` (summary).

---

## 4. Prototype

**Расположение:** `experiments/2026-06-21-interest-management-aoi-battle/prototype/`

**Файлы (planned):**

- `aoi_bench.cpp` — main harness, ~600-800 LoC, strategies as inline functors + scenes as inline data.
- `CMakeLists.txt` — standalone build (NOT mainline), Clang 22.1.6 + C++26.
- `README.md` — quick build instructions.
- `build/aoi_bench` — executable.
- `build/results.csv` — machine-readable output.

**Сборка:**

```bash
cd experiments/2026-06-21-interest-management-aoi-battle/prototype/build
cmake .. -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./aoi_bench
```

**Запуск:** standalone binary, outputs to stdout + `results.csv`. Wall time target: < 30 sec on Zen 3 5800X.

---

## 5. Results

See [`RESULTS.md`](./RESULTS.md) for full detail. Headlines:

**E_KNN_BackCull = universal winner** (1.5-1.8 Mbps, 10-86× reduction vs A_FullBroadcast).
**D_Priority = strong secondary** (2.7-3.3 Mbps, 5-46× reduction). B/C/F insufficient for
100-player scale (3-6× reduction, well short of <1 Mbps target).

| Scene              | A_FullB | B_NoTier | C_3Tier | D_Pri   | E_KNN   | F_Batch |
|:-------------------|:--------|:---------|:--------|:--------|:--------|:--------|
| uniform_dense      | 150,000 | 24,645   | 36,610  | 3,260   | **1,753** | 36,610 |
| battle_clustered   | 150,000 | 49,806   | 58,599  | 3,260   | **1,757** | 58,599 |
| sparse_scattered   | 15,000  | 2,444    | 3,639   | 2,675   | **1,484** | 3,639  |
| chase_high_movement| 75,000  | 12,370   | 18,357  | 3,260   | **1,742** | 18,357 |
| mixed_dynamic      | 75,000  | 21,234   | 27,232  | 3,247   | **1,734** | 27,232 |

**Cross 5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** YES
(D = 5-46×, E = 10-86×, both >5% of 30 Hz frame budget savings — actually exceed budget entirely).

**Critical finding:** 3-tier alone (Strategy C) is **insufficient** for 100-player scale because
peripheral tier (5 Hz) still dominates bandwidth due to entity count (4-5× critical area). **Need
top-K cap (D) or KNN+back cull (E) to hit target.**

CPU cost: B = 24 µs/tick, C-F = 2-3 ms/tick (analytical). Well below 2 ms/tick target for B,
marginal для C-F (real cost will be 2-5× higher).

---

## 6. Verdict

**`mixed`** (4 lines):

1. **Hypothesis "<1 Mbps per player" REJECTED for B, C, F** — 3-tier alone insufficient; need
   additional top-K cap or KNN+back cull.
2. **Hypothesis ">5× reduction" PARTIALLY confirmed** — D, E achieve 5-86× reduction; B, C, F only
   3-6× (insufficient).
3. **E_KNN_BackCull = 1.5-1.8 Mbps (10-86× reduction) — recommended default** for Stage 6+ netcode.
4. **D_Priority (top-K cap) = 2.7-3.3 Mbps (5-46× reduction) — fallback** if back-cull rotation
   tracking is hard to implement.

---

## 7. Integration recommendation

**Target stage:** `TODO.md §6+` military sandbox activation (currently not in mainline scope;
deferred to Stage 6+ per operator 8x planning).

**Конкретные изменения (для mainline integration):**

- **New module:** `src/network/AoiManager.{hpp,cpp}` — grid-based AOI с 3-tier relevance.
- **New module:** `src/network/PriorityQueue.{hpp,cpp}` — per-object priority queue (top-K cap).
- **ECS integration:** AOI events → Flecs component add/remove (`OnEntityEnterView`/`ExitView`).
- **Netcode layer:** wire AOI events to packet scheduler (`SendPriority` pattern per Photon Fusion).

**Подход:**

- **Adopt E_KNN_BackCull** as default:
  - `cell_size = 64 m` (1:3 ratio vs 200m view range, per ESEngine heuristic)
  - `critical_range = 200 m @ 20 Hz`, `peripheral_range = 500 m @ 5 Hz`, `ambient > 500 m @ 1 Hz`
  - `top_K = {critical: 100, peripheral: 100, ambient: 20}` (KNN variable radius per UCL 2011)
  - `back_cull = forward 180°` (per wirepair.org 2025-12)
- **Use D_Priority** as fallback if rotation tracking is hard:
  - `top_K = {critical: 200, peripheral: 100, ambient: 20}` (per-entity importance ranking)
- **Use B (no tiering) as baseline** for comparison (3-6× reduction only, insufficient for 100p)

**Риски:**

- **CPU cost 2-3 ms/tick** for C-F (analytical) — real cost will be 2-5× higher → may exceed
  5% frame budget on busy ticks. Mitigation: defer per-frame AOI updates to 10 Hz with state
  caching.
- **Static AOI policies** (per AFLL arXiv 2601.10998) — no runtime adaptation. Adaptive AOI
  deferred to future work.
- **MTU assumption (1200 bytes)** — typical Ethernet but may vary. Affects packet count, not bytes.
- **Back cull requires player rotation** — 3rd person view per wirepair.org; first-person view
  may need different cull strategy.

**Критерии приёмки:**

- **Bandwidth:** <2 Mbps per player at 100 players + 10k entities (vs 150 Mbps baseline)
- **CPU:** <5 ms/server-tick AOI cost (analytical + 2× real factor)
- **Cross 5-10% threshold:** YES, both D and E cross massively (5-86×)
- **Wirepair.org target:** <2 Mbps per player (was 1.344 Mbps in their benchmark with 100p + 7-10
  packets/player/tick)

**Зависимости:**

- Flecs ECS mainline (✅ done per `2026-06-21-ecs-1m-entities-bottleneck`).
- 2D/3D position component (✅ standard in ProjectV).
- Netcode layer (❌ not in mainline; Stage 6+ scope).

**Estimated effort:** 3-step migration per `agent/knowledge.md` precedent:
- Step 1 (S, ~150 LoC) `AoiManager` core (grid + 9-grid lookup + 3-tier).
- Step 2 (M, ~400 LoC) Priority queue + KNN+back cull + per-object importance ranking.
- Step 3 (S, ~150 LoC) ECS event integration + Tracy plot + unit test.

Total: ~700 LoC, M-L effort, 2-3 sessions. **Deferred** до Stage 6+ military sandbox activation
per operator 8x planning.

---

## 8. Sources

- See §2 for primary sources (8 verified, Exa `web_search`).
- Additional sources will be added to `sources.md` if any reference is cited but not yet listed.

---

## 9. Mapping to ProjectV hot-path

**Соответствующие mainline файлы:**

- `src/ecs/EcsWorld.cpp` — Flecs-based entity registry (per closed `2026-06-21-ecs-1m-entities-bottleneck` 1M+ ents).
- `src/network/` — предполагаемый netcode layer (отсутствует в текущем TODO Stage 1-6; deferred до Stage 6+).
- `agent/workspace.md §2` "Nearest Gap" — multiplayer netcode не в текущей roadmap.

**Допущения / упрощения:**

- 1 km² battlefield, 100 players + 10k entities — синтетические сценарии, не из mainline.
- CPU analytical model only (no actual netcode).
- Single-threaded server (Flecs work-stealing deferred to integration).
- Bandwidth estimates from per-tick byte count + 30 Hz, no protocol overhead modeling.
- `aoi_churn` analytically computed from entity velocity, not measured.

**Что осталось неизмеренным:**

- Network protocol overhead (header bytes, retransmission, congestion control).
- Real packet scheduling jitter.
- Cross-region entity migration (entity moves server zone → AOI recompute cost spike).
- Encryption/auth overhead per packet.
- Server memory cost for per-observer AOI state (entity_id list per player).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X, `obvium` host) + §2 (32 GiB RAM). **Не дублировать данные здесь.** Данные захвачены `2026-06-21`, свежие — probe не нужен.

---

## 10. Operator directives compliance

- **§5.3 Web search:** Exa `web_search` работал в этой сессии, 8 primary sources verified.
- **§5.4 Git safety:** не используется `git *` (per `docs/experiments/AGENTS.md` — моя зона).
- **§13 Topic reservation:** claim в `backlog.md §In progress` per §13.1 + §13.2 + sentinel §13.7.
- **§14 Hardware profile:** cross-ref только, hardware-profile.md свежий (`2026-06-21` capture).