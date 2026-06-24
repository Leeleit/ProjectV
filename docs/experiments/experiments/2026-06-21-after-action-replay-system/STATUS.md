# STATUS — `2026-06-21-after-action-replay-system`

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session, ~2h end-to-end)
**Stage link:** independent (cross-cutting Tier 0 Foundation & Optimization — military sandbox axis — replay/determinism primitive; prerequisite for `lockstep-state-sync-hybrid-netcode` h Tier 1 + `lockstep-deterministic-multiplayer` open + `after-action-report` Tier 4 + esports observer).
**Estimated effort:** S-M (single session, ~2h end-to-end)
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй» 2026-06-21)

---

## Log

- `2026-06-21` — claimed from `backlog.md §Open` (line ~193) per `AGENTS.md §13.1`; anti-duplicate sentinel
  §13.7 confirmed clean:
  - `rg "after.action.replay|replay.system|deterministic.replay|deterministic.lockstep|recording.rate"` over
    `docs/experiments/` → только cross-refs в `INDEX.md` + `backlog.md` + `flow-field-pathfinding-10k-units/README`
    (зависимость) + `procedural-military-terrain-gen/README` (косвенно) + `ballistic-projectile-simulation/README`
    (упомянуто как goal). **Нет dedicated `after-action-replay-system` folder**, нет closed experiments на эту тему.
  - `ls experiments/2026-06-21-after-action-replay-system/` — отсутствует до этой сессии.
  - In-progress параллельные (НЕ трогаю): `2026-06-21-water-surface-rendering` (Stage 5.x),
    `2026-06-21-voxel-grass-foliage-rendering-pipeline` (Stage 4.1+5.x). Topic orth orth: replay = cross-cutting
    Tier 0, не Stage 5.x, не Stage 4.1.
- `2026-06-21` — Phase 1 (Web research) complete via direct `webfetch` to canonical URLs (Glenn Fiedler
  Gaffer On Games x3 + Wikipedia C&C Remastered). Exa `web_search` HTTP 429 persistent per
  the web_search fallback chain + DuckDuckGo bot challenge + Google bot challenge. 17 sources
  verified (S1-S17 в `sources.md`).
- `2026-06-21` — Phase 2-3 (prototype design + build + run) complete. 5 scenes × 3 seeds × 4 strategies +
  5 K-sweep variants. 75 measurements, wall time 36.8 sec.
- `2026-06-21` — Phase 4 (writeup) complete: `README.md` + `RESULTS.md` + `sources.md` +
  `prototype/replay_bench.cpp` ~700 LoC + `prototype/build/replay_bench` + `prototype/build/results.csv` (76 rows).
- `2026-06-21` — Phase 5 (close) per `AGENTS.md §6` DoD: all 8 sections of `README.md` filled, `STATUS.md`
  reflects verdict=mixed, `INDEX.md` updated (§5 → §6), `backlog.md` synced (§In progress → §Closed).

---

## State (final)

- **Slug:** `2026-06-21-after-action-replay-system` (military sandbox axis — Tier 0 Foundation & Optimization — cross-cutting replay primitive).
- **Priority:** h (Tier 0 — Foundation; cross-cutting prerequisite для Tier 1 netcode + Tier 4 UI/replay).
- **Verdict:** **`mixed`** (hypothesis partially validated).
- **Agent:** self.
- **Headline:** **C_InputPlusCheckpoint with K=60 (2 s @ 30 Hz) = recommended default** для 1k+ entities
  (−81% bandwidth vs A, ~100 ms cold-seek, bit-exact, low record overhead). **A_FullState wins for ≤100
  entities**. **B_InputOnly = long-term archival** (smallest, slow seek). **D_DeltaEncoded is non-deterministic
  в текущем прототипе** (rng state не в delta record; fix trivial 8 B/tick но D остаётся niche).
- **Mainline recommendation:** 3-step migration per `agent/knowledge.md` precedent (~400 LoC, S effort,
  1-2 sessions, deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x
  planning decision).

---

## Cross-axis summary (final)

- **Orth** к in-progress `water-surface-rendering` (Stage 5.x), `voxel-grass-foliage-rendering-pipeline`
  (Stage 4.1+5.x).
- **Complementary** к closed `multi-resolution-collision-broadphase` [closed yes, JPH foundation, must be
  deterministic для replay] + `flow-field-pathfinding-10k-units` [closed yes, 3.74 µs/frame cycle, must be
  deterministic] + `interest-management-aoi-battle` [closed mixed, AOI = state subset] +
  `ballistic-projectile-simulation` [closed yes, projectile sim must be deterministic] +
  `recon-intel-fog-of-war` [closed yes, intel state snapshot-able] + `tank-terrain-interaction-physics`
  [closed yes, suspension must be deterministic] + `ecs-1m-entities-bottleneck` [closed yes, 1M+ entity
  registry state] + `cover-system-terrain-adaptive` [closed mixed, cover point state].
- **Prerequisite** для open `lockstep-state-sync-hybrid-netcode` h Tier 1 + `lockstep-deterministic-multiplayer`
  l + `after-action-report` m Tier 4 + `observer-spectator-free-camera` m Tier 4 + `spectator-esports-camera`
  m Tier 4.
- **New axis:** first dedicated **replay system** axis в 100+ closed experiments; opens Tier 4
  spectator/esports / persistence layer.
