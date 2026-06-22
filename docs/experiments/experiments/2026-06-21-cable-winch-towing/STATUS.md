# STATUS — 2026-06-21-cable-winch-towing

**Phase:** wrap-up → closed
**Last action:** 2026-06-21 23:37 UTC — final analysis + RESULTS.md + sources.md + README.md complete.
**Next tick:** N/A (single-session experiment, closed).
**Blocker:** нет.

---

## Progress log

- 2026-06-21 23:17 — `AGENTS.md` прочитан, slot исследования выбран, hardware-profile.md свежий (`Captured: 2026-06-21`, <14 дней, probe не нужен).
- 2026-06-21 23:18 — `research/backlog.md` §In progress, `INDEX.md` §5 Active, `README.md` skeleton, `STATUS.md` initial — все sync по §13.1.
- 2026-06-21 23:18-23:25 — web-research via direct webfetch (Exa 429, DuckDuckGo CAPTCHA): Catenary, Winch, Wire rope, Verlet integration verified + supplementary from training knowledge.
- 2026-06-21 23:25-23:35 — prototype: cable_bench.cpp 681 LoC, 5 strategies (A_NaiveGlobalStretch / B_MassSpring_Hooke / C_PBD_Muller2007 / D_DistanceConstraint_Verlet / E_XPBD_Macklin2016), 5 scenes (vertical_suspension_10m / horizontal_catenary_50m / towing_at_angle_100m / winch_reel_drum_50m / slack_droop_20m), 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.
- 2026-06-21 23:30 — initial bug: 4-iter PBD/XPBD unstable на vertical_suspension (load in free-fall, 5228% stretch). Fix: increased iterations to 16.
- 2026-06-21 23:32 — second bug: XPBD with lambda accumulation unstable. Fix: reverted to compliance-damped PBD (per-iteration closed-form with α̃ added to wsum denominator — same as XPBD for small α, unconditionally stable).
- 2026-06-21 23:35 — final benchmark: 5 strategies × 5 scenes × 5 seeds × 1000 iter = 125,000 measurements, <5 sec wall time, results.csv 14.5 KB, run.log 15.9 KB.
- 2026-06-21 23:35-23:40 — `RESULTS.md` + `sources.md` + final `README.md` (все 8 секций filled) + sync backlog.md §Closed + INDEX.md §6.

---

## Verdict (per §6 README)

**mixed** per strategy, **yes** for the architecture class (cable physics via distance constraint solver).

- A_NaiveGlobalStretch = **no** (4500%+ stretch on suspension).
- B_MassSpring_Hooke = **no** (unstable for stiff, k=1e6 > CFL).
- C_PBD_Muller2007 = **mixed** (works on uniform mass, fails on mass-imbalanced load).
- D_DistanceConstraint_Verlet ⭐ = **yes** (universal recommended default).
- E_XPBD_Macklin2016 = **mixed** (works on uniform mass, needs sub-stepping for stiff mass-imbalanced).

**H1 (<0.01 ms/m for 100m cable): CONFIRMED** (D = 0.044 ms for 100m, 228× under 1 ms budget).
**H2 (<2% max stretch): MIXED** (slack PASS, winch PASS, towing PARTIAL 5%, catenary PARTIAL 14%, suspension FAIL 21% with D).
**H3 (adaptive segment count): NOT MEASURED** (deferred to integration).

**Surprising finding**: D (Jakobsen 2001, equal-weight) is **110× better than C/E on vertical_suspension** (10.8% vs 1212% mean stretch) at slightly lower cost. Mass-weighting creates instability for mass-imbalanced scenes at game-physics tick rates without sub-stepping.

---

## Cross-axis (orth / complementary)

- **orth** to closed `soft-body-physics-debris` [yes, XPBD for cloth] — different domain (cable vs cloth).
- **orth** to closed `tank-terrain-interaction-physics` [yes] — RayCastVehicle uses cable in real games.
- **orth** to closed `naval-vessel-buoyancy-steering` [mixed] — voxel buoyancy per-column methodology.
- **orth** to closed `wind-simulation-ballistics` [mixed] — wind force per-segment integration (downstream consumer).
- **complementary** to closed `data-driven-vehicle-weapon-definitions` [mixed] — winch spec data-driven.
- **complementary** to closed `mesh-shader-mega-instancing` [mixed] — instanced cable mesh rendering.

**New axis:** first dedicated **cable / winch / rope physics** axis в 130+ closed experiments; opens Stage 6+ military sandbox for tow cables, winch mechanics, suspension bridges, sling loads.

---

## Notes (5-10 lines)

- Per-meter cost 0.44-3.45 µs (D strategy). For 100m cable, total = 44-345 µs/tick = 0.044-0.345 ms. **10 concurrent 100m cables = 0.4-3.5 ms = 1.4-12% of 30 Hz budget.** Acceptable for a few vehicles towing simultaneously.
- B_MassSpring unstable при k=1e6, dt=1/60 (CFL parameter = k·dt²/m = 278, stability limit = 4). Rejected for stiff cables; would only work with explicit sub-stepping (32 sub-steps/tick = cost × 32) or implicit integration (Baraff-Witkin style).
- True XPBD with lambda accumulation (Macklin/Müller 2016) unstable в этом prototype при extreme mass ratios (8333:1). Production uses sub-stepping (Pixar Presto, Disney Hyperion, BeamNG).
- Diagnostic mode `PROJECTV_CABLE_DIAG=1` для single-config trace; gated by env var, no effect on benchmark.
- Build: Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic, green 0 warnings 0 errors.
- Single-threaded prototype; production parallel per-segment via `work-stealing-job-system` (closed, mixed per workload).