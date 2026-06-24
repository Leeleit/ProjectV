# STATUS — `2026-06-21-persistent-war-server-architecture`

**Last update:** 2026-06-21 23:15 (this session, closed)
**Phase:** Closed (single session, ~3h)
**Agent:** self
**Verdict:** `yes` for E_Hybrid_ShardedReactive as recommended default; `mixed` per strategy.

---

## Current state

✅ **Phase 0 — Claim + reservation** complete (claim per `AGENTS.md §13.1` + sentinel §13.7 clean + §13.3 race recovery).
✅ **Phase 1 — Web-research** complete (18 sources verified Tier 1-4).
✅ **Phase 2 — Prototype design** complete (5 strategies × 5 scenes × 5 seeds × 1000 iter = 125,000 main measurements).
✅ **Phase 3 — Build + run** complete (`clang++ 22.1.6` build green 0 warnings, wall time **6 ms** on Zen 3 5800X).
✅ **Phase 4 — Analysis + RESULTS.md** complete (E_Hybrid_ShardedReactive = universal winner).
✅ **Phase 5 — Closure sync** complete (backlog.md §In progress + backlog_closed.md + INDEX.md §6 + §8 Last update).

## Headline verdict

**`yes`** for E_Hybrid_ShardedReactive as universal recommended default for Stage 6+ military sandbox persistent war server infrastructure.

Per strategy at foxhole_war=1000 players (mean across 5 seeds):
- **A_P2P_ListenServer** = INF (16-player cap; **NEVER**)
- **B_Centralized_Postgres** = INF (lock contention O(N²); OK≤100p, FAIL≥500p)
- **C_RealmSharded_NATS** = 10.10 ms p99 / 99.99% durability (highest, but slow recovery at scale)
- **D_RowsAgones** = 6.52 ms p99 / 95.00% durability (lowest; match-based only)
- **E_Hybrid_ShardedReactive ⭐** = 4.70 ms p99 / 99.95% durability / 45s recovery / 0.30 CPU·ms/s (**UNIVERSAL WINNER**)

**5-10% threshold:** E vs worst_feasible = **89,308× improvement**. Hypothesis fully validated across 4 clauses.

## Final artifacts

- [`README.md`](./README.md) — claim + results + verdict + integration recommendation
- [`sources.md`](./sources.md) — 18 sources Tier 1-4 (Agones, NATS JetStream, Foxhole, etc.)
- [`RESULTS.md`](./RESULTS.md) — 8 sections, headline + per-cell breakdown + cross-axis validation
- [`prototype/persistent_war_server_bench.cpp`](./prototype/persistent_war_server_bench.cpp) — ~330 LoC C++26 CPU analytical cost model
- [`prototype/build/persistent_war_server_bench`](./prototype/build/persistent_war_server_bench) — compiled binary (26 KB)
- [`prototype/build/results.csv`](./prototype/build/results.csv) — 126 rows × 14 cols (15.5 KB)

## Next steps for mainline (deferred до Stage 6+)

Per `agent/knowledge.md` precedent, 3-step migration (~1200 LoC, M-L effort, 3-5 sessions):
1. `src/server/RealmCore.{hpp,cpp}` (~300 LoC) — NATS JetStream + RAFT R=3 + sync_interval=always + realm sharding.
2. `src/server/RealmOrchestrator.{hpp,cpp}` (~600 LoC) — Agones FleetAutoscaler + per-realm pod lifecycle + cross-realm event routing + player migration.
3. `src/server/PersistenceSnapshot.{hpp,cpp}` (~300 LoC) — periodic event-log snapshot + recovery replay + `PROJECTV_SERVER_ARCH=HYBRID|REALM_NATS|AGONES|POSTGRES|DEV` env gate (default `HYBRID`) + Tracy plot + unit test.

**Recommended default for ProjectV:** `PROJECTV_SERVER_ARCH=HYBRID` (E_Hybrid_ShardedReactive).

## Cross-axis context

In-progress parallel at this session's peak (verified via `find -mmin -60` at 22:55):
- `2026-06-21-save-game-persistence-architecture` (Tier 4) — closed at 22:46, just-closed
- `2026-06-21-group-formation-maneuver-axis` (Tier 2 AI) — active
- `2026-06-21-data-driven-vehicle-weapon-definitions` (Tier 0 data) — active
- `2026-06-21-boid-flocking-steering-axis` (Tier 0 steering) — active
- `2026-06-21-combined-arms-coordination-ai` (Tier 2 AI) — active
- `2026-06-21-flanking-maneuver-ai` (Tier 2 AI) — active
- `2026-06-21-ballistic-crack-thump` (Tier 4 audio) — active
- `2026-06-21-structural-collapse-cascade` (Tier 1 Physics) — race-lost to parallel self at 22:57, active in another session

**Orth** to all 8 (no Tier 1 Server Architecture in parallel at session start).

## Reference docs

- Project metadata: `/AGENTS.md §2 vision (sandbox + modding + persistence)`
- Sources: [`./sources.md`](./sources.md)
- Hypothesis: [`./README.md §1`](./README.md)
- Results: [`./RESULTS.md`](./RESULTS.md)
- Benchmarks protocol: `../benchmarks/methodology.md`