# STATUS — 2026-06-20-work-stealing-job-system

**Phase:** concluded-verdict-mixed (closed same session)
**Last action:** 2026-06-20 — experiment closed, verdict=`mixed`. README.md + RESULTS.md + sources.md + prototype/
ready. `backlog.md §In progress → §Closed`, `INDEX.md §5 → §6` (next tick).
**Blocker:** нет.
**Next tick:** по запросу оператора (negative result, no follow-up planned).

---

## Progress log

- 2026-06-20 — claimed per §13.1, `backlog.md §In progress` + `INDEX.md §5` обновлены, dir создан.
- 2026-06-20 — web-research: 4 batch queries (P2300/BS/Taskflow/oneTBB), 25 sources.
- 2026-06-20 — prototype scaffold: workload + simple pool + BS wrapper + stats + bench (6 files, ~750 LoC incl. vendored
  BS_thread_pool.hpp 2373 lines).
- 2026-06-20 — smoke test caught `pool_simple` segfault (`std::vector<bool>` race + push вне mutex), rewritten with
  `int tasks_in_flight_` under same mutex + atomic check pattern. Smoke OK after fix.
- 2026-06-20 — full benchmark: 30 iters × 24 configs = 720 measurements, ~80 sec. All configs deterministic, results.csv
  valid.
- 2026-06-20 — writeup: README.md (8 sections + 2 cross-refs) + RESULTS.md (5 tables + 9 sections analysis) +
  sources.md (25 refs).
- 2026-06-20 — closing sync: this STATUS update + backlog.md §In progress → §Closed + INDEX.md §5 → §6.

---

## Notes

**Ключевой негативный finding:** **serial dispatcher — sweet spot для ProjectV mainline.** Work-stealing pool (BS::
thread_pool) проигрывает simple pool'у для small tasks, simple pool проигрывает serial для small workloads. SMT (16
threads) = counter-productive для cache-fitting workloads (L3 32 MiB << 64 MiB для 16384 chunks × 4 KiB).

**Hypothesis NOT confirmed:** P2300 `std::execution` как mainline default — НЕ рекомендуется. P2300 = framework, не
pool. Real pool (BS/Taskflow/TBB) — все проигрывают serial для ProjectV primary workloads. Std::execution sender-chain
overhead предположительно ещё хуже (не измерен — callout as follow-up).

**Per-stage verdict:**

- Stage 4.1 (background world gen) = **serial CPU + parallel GPU** (already mainline approach).
- Stage 3.1 (Fluid CA bookkeeping) = **serial CPU** (already mainline).
- Stage 6.1 (ECS multi-thread) = **TBD, separate experiment needed**.
- Stage 4.3 (lift draw distance 128+ chunks) = **re-evaluate at Stage 4.3 start** (current measurement suggests serial
  still wins up to 16384 chunks).

**Cross-vendor caveat:** single-vendor (Zen 3 5800X). Intel desktop (no HT) = different scaling. EPYC NUMA = different
scaling. Arm big.LITTLE = different scaling. Not measured.

**Hardware baseline:** Zen 3 8C/16T, L3 32 MiB, governor `powersave` (не `performance` — поправлено в writeup). CPU
boost ~5.0 GHz idle.

**Future SOTA tracking:** P2300R10 published 2024-06; P3826R3 fix 2026-01; C++26 publication expected 2026-2027. Per
`bigcpp.com` 2026-05-25: GCC 15+ / Clang 20+ partial. libc++ integration pending. **Re-evaluate when Clang 23+ + libc++
stable.**

**Anti-pattern locked in:** «add work-stealing pool = best default for parallel code» — common wisdom, NOT measured in
this workload. Per `agent/knowledge.md` SIMD-frustum-cull priority — аналогично: per-entity ECS bookkeeping =
small task. Don't add pool без measurement.

**Re-evaluation triggers:**

- Stage 6.1 Step 6 NUMA-aware allocation (`TODO.md §6.1`).
- Stage 4.3 lift draw distance (128+ chunks, total data likely > L3 at extreme cases).
- Perlin/SVDAG real workload (3-5× compute vs my synthetic).
- AVX-512 hardware arrival (Zen 5, Arrow Lake).
- Real ProjectV ECS `ecs_progress` overhead (not yet measured).
- `stdexec::static_thread_pool` direct measurement when Clang 23+ + libc++ stable.
