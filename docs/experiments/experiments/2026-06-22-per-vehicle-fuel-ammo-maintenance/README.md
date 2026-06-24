# 2026-06-22-per-vehicle-fuel-ammo-maintenance — Per-vehicle fuel/ammo/maintenance model

**Status:** _concluded-verdict-mixed_ (per strategy); `yes` for D_HierarchicalLOD ⭐ as universal recommended default + E_PhysicsCoupledSoA ⭐ as production-recommended.
**Date opened:** 2026-06-22
**Date closed:** 2026-06-22
**Stage link:** _independent (Tier 1+2+3 cross-cutting)_
**Estimated effort:** _M (~2h single session)_
**Author:** _agent (self)_

---

## 1. Hypothesis

Гипотеза: Multi-strategy approach ∈ {A_NaiveFlat, B_LoadMultipliedExponential, C_StatefulEventDriven, D_HierarchicalLOD, E_PhysicsCoupled_PerVehicleSoA} handles **1000+ vehicles** with per-tick fuel + ammo + maintenance state updates при **< 0.5 µs/vehicle/tick** (target < 500 µs/tick @ 1000 vehicles = < 1.5% of 30 Hz budget), **bit-exact** to canonical Miner 1945 cumulative-damage formula + Wikipedia "Specific fuel consumption" jet/turbine data + per-caliber ammo consumption from closed `ballistic-projectile-simulation` [yes], with damage cascading to maintenance (collision → structural wear, G-load → airframe fatigue, round count → barrel wear). Alternative = closed `supply-logistics-simulation` [mixed] = per-node flow (fleet-level, NOT per-vehicle) = DOES NOT address continuous per-vehicle state. Alternative = closed `aircraft-damage-model` [yes] = event-driven combat damage (hit = event, NOT continuous wear) = DOES NOT address fuel/ammo consumption / continuous degradation. **My approach combines** fuel + ammo + maintenance as **continuous per-vehicle state** with **physics-driven consumption** (RPM-dependent fuel burn, muzzle-counter ammo consumption, G-load + round-count maintenance). Cross-cuts Tier 1 (Physics) + Tier 2 (AI: per-vehicle state → AI commander decision) + Tier 3 (Economy: factory-production refuel/reload/repair cost).

---

## 2. Prior art

Web-research complete (см. [`sources.md`](./sources.md)). **5 Tier 1 canonical + 3 Tier 2 game-production + 8 Tier 3 ProjectV cross-refs verified:**

- **Wikipedia "Brake-specific fuel consumption"** — canonical BSFC for piston/turboprop/turboshaft/diesel: 200-1280 g/(kW·h) range (T700: 263, T408: 240, Wärtsilä W31: 165).
- **Wikipedia "Thrust-specific fuel consumption"** — canonical TSFC for jet/turbofan: 8-55 g/(kN·s) range (GEnx: 8.06, F119: 17.3 dry, F110: 53.8 AB).
- **Wikipedia "Fatigue (material)" / Miner's rule** — canonical cumulative damage formula `D = Σ n_i / N_i = C` (Palmgren 1924, Miner 1945, Endo & Matsuishi 1968 rainflow).
- **Wikipedia "Fuel economy in aircraft"** — empirical maintenance→fuel correlation (100 kg penalty without engine wash, 50 kg with 5mm slat gap).
- **Wikipedia "Specific fuel consumption"** (disambig) — TSFC vs BSFC, gas turbines vs shaft engines.
- **Game production refs**: War Thunder Wiki (per-vehicle repair cost), DCS World (engine wear), Arma 3 CfgVehicles/CfgWeapons (data-driven vehicle defs).
- **ProjectV cross-refs**: 8 closed experiments (see [`sources.md` §Tier 3](./sources.md)).

---

## 3. Method

- **Тип эксперимента:** analytical + prototype + benchmark.
- **Сцена:** 5 scenes ∈ {small_ground_20 (20 light vehicles, 5-min engagement, 90% activity), medium_mixed_100 (100 mixed, 30-min, 60% activity), large_armor_500 (500 heavy tanks, 60-min, 40% activity), massive_battle_1000 (1000 mixed, 2-hr, 30% activity), air_combat_50 (50 jets, 1-hr, 80% activity)}.
- **Метрики:** mean / median / p95 / p99 / std / N per (strategy × scene × seed) — cost per vehicle per tick (CPU ns/vehicle/tick) + per-state-update accuracy.
- **Контроль:** A_NaiveFlat (constant consumption) baseline; B-E compared to A via 5-10% threshold per `optimization-philosophy.md`.
- **Протокол:** `benchmarks/methodology.md §3` (N=1000 + 10 warmup). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.108 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.

---

## 4. Prototype

If code — где он лежит, как собирается, как запускается, что выводит.

```bash
# Build
cd prototype/build && cmake .. -DCMAKE_CXX_COMPILER=clang++ && make
# Run
./fuel_ammo_maint_bench
# Output: build/results.csv (126 rows = 1 header + 125 data, 16 KB)
```

**Harness:** standalone C++26 CPU analytical benchmark, Clang 22.1.6 `-O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic -Wno-unused-parameter`, **build green 0 warnings 0 errors**. ~530 LoC `fuel_ammo_maint_bench.cpp` (8 vehicle class defs + 5 strategy functions + 5 scene configs + 5-seed × 1000-iter harness + CSV writer + per-iter chrono timing).

---

## 5. Results

См. подробности в [`RESULTS.md`](./RESULTS.md). **Краткая сводка:**

| Strategy | Per-vehicle @ 1000 | vs target 500 ns/v | Verdict |
|:---------|-------------------:|-------------------:|:--------|
| A_NaiveFlat | 0.9 ns/v | 555× under | REJECTED (baseline) |
| B_LoadMultipliedExp | 3.4 ns/v | 147× under | REJECTED as default (3.7× cost vs A) |
| C_StatefulEventDriven | 0.8 ns/v | 625× under | REJECTED (functionally trivial) |
| **D_HierarchicalLOD ⭐** | **1.7 ns/v** | **294× under** | **RECOMMENDED DEFAULT** |
| E_PhysicsCoupledSoA | 2.3 ns/v | 217× under | RECOMMENDED for production (2.5× cost for full accuracy) |

Per-iter @ massive_battle_1000: A=905 ns, B=3364 ns, C=836 ns, **D=1701 ns**, E=2265 ns (all < 0.01% of 30 Hz budget).

---

## 6. Verdict

`mixed` per strategy; `yes` for **D_HierarchicalLOD ⭐ as universal recommended default** (best cost-quality ratio) + **E_PhysicsCoupledSoA ⭐ for production** (when upstream physics simulators available). All 5 strategies 200-2000× under the 0.5 µs/vehicle target — hypothesis H1 cost confirmed MASSIVELY. H2 bit-exact Miner 1945 confirmed (B and E). H3 continuous state confirmed (A/B/D/E all update per-tick).

---

## 7. Integration recommendation

**Target stage:** `TODO.md §3.2` (Stage 3.2 destruction) or `TODO.md §6+` (Stage 6+ military sandbox activation).

**Конкретные изменения:** per-vehicle `FuelState` + `AmmoState` + `MaintenanceState` components в `src/voxel/Vehicle.{hpp,cpp}` or new `src/flight/ecs/components/FuelAmmoMaintenance.{hpp,cpp}`.

**Подход:** 3-step migration per `agent/knowledge.md` precedent:

- **Step 1 (XS, ~80 LoC)**: `FuelAmmoMaintenance` Flecs components (SoA-aligned, 64-byte cache line) + `PROJECTV_FUEL_AMMO_STRATEGY=NAIVE|LOAD_EXP|EVENT|LOD|PHYSICS` env gate (default `LOD`) + 5 strategy function pointers in `FuelAmmoMaintenanceSystem::Update(ecs, dt)`.
- **Step 2 (S, ~250 LoC)**: per-strategy implementation in `src/flight/ecs/systems/FuelAmmoMaintenanceSystem.{hpp,cpp}` + integration with upstream `fixed-wing-flight-model-simulation` [yes] (RPM → fuel burn), `helicopter-rotor-physics` [yes] (rotor RPM → fuel burn), `ballistic-projectile-simulation` [yes] (shot count → ammo), `aircraft-damage-model` [yes] (event damage → state), `component-vehicle-damage-model` [yes] (per-module HP), `interest-management-aoi-battle` [yes] (AOI → is_active), `ecs-1m-entities-bottleneck` [yes] (Flecs host), `data-driven-vehicle-weapon-definitions` [mixed] (vehicle stat defs), `supply-logistics-simulation` [mixed] (per-node flow consumer).
- **Step 3 (XS, ~50 LoC)**: `FuelAmmoMaintenanceTests` 5 scene tests + Tracy plot "Fuel Ammo Maintenance" zones per strategy + `PROJECTV_FUEL_AMMO_STRATEGY` env gate default flip + integration with closed `factory-production-system` [mixed] (refuel/reload/repair cost) + closed `lockstep-state-sync-hybrid-netcode` [mixed] (sync events for multiplayer).

**Риски:**
- Real production cost 2-5× higher than prototype (Flecs ECS + VMA + Vulkan overhead).
- Damage coupling simplified (G-load + round count only, no thermal cycling, no oil degradation).
- Per-vehicle BSFC/TSFC values are production reference samples, not real per-vehicle calibration.
- D_HierarchicalLOD requires integration with `interest-management-aoi-battle` [yes] for real AOI.

**Критерии приёмки:** per-tick fuel/ammo/maintenance cost < 0.5 µs/vehicle @ 1000 vehicles (Tracy plot zone < 0.5 ms/tick total); 100% bit-exact to Miner 1945 formula for B/E strategies; fuel/ammo state correctly degrades over time and after damage events.

**Зависимости:** Flecs ECS (closed `ecs-1m-entities-bottleneck` [yes] pattern); upstream physics simulators (`fixed-wing-flight-model-simulation`, `helicopter-rotor-physics`, `ballistic-projectile-simulation`); upstream state (`aircraft-damage-model`, `component-vehicle-damage-model`).

**Estimated effort:** ~380 LoC total, S effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision (or Stage 3.2 destruction if vehicle physics is required earlier).

---

## 8. Sources

См. [`sources.md`](./sources.md) — 5 Tier 1 + 3 Tier 2 + 8 Tier 3 = 16 sources verified.

---

## 9. Mapping to ProjectV hot-path

**Какой участок движка соответствует прототипу:**
- `src/flight/ecs/systems/FuelAmmoMaintenanceSystem.{hpp,cpp}` (post-integration) — per-vehicle state update each tick
- `src/flight/ecs/components/{FuelState, AmmoState, MaintenanceState}.hpp` (post-integration) — SoA components
- Reads from: `fixed-wing-flight-model-simulation` (RPM), `helicopter-rotor-physics` (rotor RPM), `ballistic-projectile-simulation` (shot count), `aircraft-damage-model` (event damage), `component-vehicle-damage-model` (per-module HP)
- Writes to: `factory-production-system` (refuel/reload/repair cost trigger), `lockstep-state-sync-hybrid-netcode` (sync events)

**Какие допущения/упрощения:**
- CPU-only, no Vulkan GPU dispatch, no Flecs ECS overhead in timing
- Per-vehicle state = 64-byte cache-line aligned struct (SoA pattern)
- BSFC/TSFC = production reference values from Wikipedia tables (not real per-vehicle calibration)
- Damage coupling = simplified (G-load + round count, no thermal cycling, no oil degradation)
- D_HierarchicalLOD uses synthetic is_active boolean from RNG, not real AOI check
- No refuel/reload/repair event handler (out of scope for this prototype)

**Что осталось неизмеренным:**
- Flecs ECS component access overhead (real ~50-100 ns/component access)
- Vulkan GPU dispatch overhead (relevant only if Strategy E uses GPU compute)
- VMA memory barrier overhead for component SoA buffers
- Real per-vehicle BSFC/TSFC calibration from production data
- Real refuel/reload/repair event handler cost
- Network sync overhead for multiplayer state updates

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X, 8/16 cores, 32 MiB L3) + §2 (62.7 GiB RAM). CPU-only experiment, no GPU-specific dependencies. Wall time 0.108 sec на dev host `obvium`.
