# STATUS — 2026-06-20-work-stealing-job-system

**Phase:** in-progress (research start)
**Last action:** 2026-06-20 — claim per §13.1, `backlog.md §In progress` + `INDEX.md §5` обновлены, dir создан.
**Blocker:** нет.
**Next tick:** web-research (C++26 `std::execution` P2300, BS::thread_pool, Taskflow, TBB, libdispatch 2024-2026 SOTA).

---

## Progress log

- 2026-06-20 — claimed. Anti-duplicate sentinel clean (нет sluga в `experiments/<slug>/`; `meshing-algo-comparison`
  in-progress с disjoint scope). Reservation в `backlog.md §In progress` + `INDEX.md §5`. `agent/knowledge.md §29.0`
  line 887 = «Tier 4 R&D: `std::execution` — нужна Job System, отдельный slice» — direct prior art.

---

## Notes

Прямая связь с уже закрытыми сегодня-сессиями:

- `flecs-soa-vs-aos-bench` (closed) — ECS layout settled; this experiment дизайнит job-scheduling surface для ECS
  multi-threading.
- `async-compute-overhead-numbers` (closed) — async foundation на GPU; этот experiment — async foundation на CPU side.
- `simd-procedural-noise` (closed) — per-chunk CPU compute workload; этот experiment дизайнит dispatcher для batch таких
  workload'ов.

Hardware baseline per `docs/experiments/hardware-profile.md` §1: Zen 3 8C/16T, `amd-pstate-epp` governor `powersave`.
Измерения должны явно фиксировать governor (per `benchmarks/methodology.md` §2).
