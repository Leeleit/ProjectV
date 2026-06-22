# STATUS — 2026-06-21-vegetation-destruction-interaction

**Phase:** *concluded-verdict-yes*
**Last action:** 2026-06-21 — closed, headline numbers collected.
**Next tick:** deferred до Stage 3.2 (incremental Jolt physics / voxel destruction / debris).
**Blocker:** нет

---

## Progress log

- 2026-06-21 — opened and reserved (per `AGENTS.md §13.1`).
- 2026-06-21 — single-session closure. Prototype: `prototype/vegetation_destruction_bench.cpp`
  ~770 LoC, build green (5 cosmetic warnings). Standalone C++26 CPU harness. 5 strategies × 5
  scenes × 5 seeds × 8 mutations × 50 iter = **50,000 main measurements**, wall time ~1.6 sec на
  dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Headline:
  **A_NaiveGlobalBFS = 8.4 µs, B_HierarchicalDSU = 28.4 µs, D_LightweightStressTopple = 71.4 µs
  per destruction event**. All strategies <100 µs (target from `backlog.md`). D adds the
  physically realistic topple behaviour that pure geometric CC misses.

---

## Notes

Эксперимент закрыт `yes`. Геометрические стратегии (A, B, C, E) 100% точны. D
(`LightweightStressTopple`) добавляет реалистичную модель повала (1 повреждённый воксель ствола
→ topple всей кроны), снижая геометрическую точность до 99.9%, но это by design.

Рекомендация для mainline: использовать **B_HierarchicalDSU + D_LightweightStressTopple**
композицию. Stage 3.2 (incremental Jolt physics / voxel destruction / debris). Ориентировочно
2 sessions, ~400 LoC.

Caveat: per-tree toppling spawn cost (50-200 µs/rigid body via Jolt) **10× detection cost** —
not measured in this prototype, требует отдельного Stage 3.2 hot-path verification.

См. полный отчёт в [README.md](./README.md) + [RESULTS.md](./RESULTS.md) +
`prototype/build/results.csv`.