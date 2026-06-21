# 2026-06-21-aircraft-damage-model — Per-Component Aircraft Damage System

**Status:** `in-progress`
**Date opened:** 2026-06-21
**Date closed:** _N/A_
**Stage link:** independent (new game axis — military sandbox Tier 1 Core Engine Systems: Physics)
**Estimated effort:** M
**Author:** agent (self)

---

## 1. Hypothesis

Per-component hit-table (precomputed 3D mask per aircraft type, mapping projectile hit position to specific components: engine, wings, tail, control surfaces, fuel tanks, hydraulics, cockpit, oil cooler) + per-component health pool costs **<1 µs/projectile hit**. Cascading failure (fuel leak → fire → wing separation; engine damage → oil leak → seizure) triggered by health thresholds at **<0.1 µs/cascade-update**. GPU particle proxy for fire/smoke (engine fire, hydraulic spray, fuel vapor) at **<0.2 ms per aircraft** (proxy cost from closed `mesh-shader-mega-instancing` [mixed, C_AmplificationShaderOnly pattern]).

**Key claims:**
1. **Hit-table (5D lookup: x, y, z, aircraft_id, projectile_caliber → component_id)** precomputed at aircraft load = O(1) per hit, **<50 ns/hit**.
2. **Per-component health pool** with cascading thresholds (component-specific failure modes per datamined WT scripts) = **<0.1 µs/update per component**, <1 µs total per hit.
3. **Visual fire/smoke GPU particle proxy** (per `mesh-shader-mega-instancing` 2.5× amplification per 1k particles) at <0.2 ms/aircraft.

**Alternatives rejected:**
- **Full 6-DOF with per-mesh breakable FEM** (libuipc, Bullet softbody): 100-1000× too slow for realtime (100+ aircraft in scenario).
- **Per-voxel damage grid** (minecraft-style): wasteful (most aircraft voxels are undamageable skin), and breaks at curved aerofoil surfaces.
- **Single global HP** (arcade): doesn't differentiate engine fire vs control surface jam vs wing separation — player can't prioritize fire vs structural.

---

## 2. Prior art

Web-research pending (Phase 2). Target sources:
- War Thunder datamine (`gszabi99/War-Thunder-Datamine`): production `blck_dmg` config + per-component armor + fuel/control_surface modules.
- War Thunder Wiki: damage model (since 2019 Jacob DeMarre basis for AP penetration + per-component HP for HE/HEAT).
- DCS World aircraft damage: modular per-system (engine / flight controls / hydraulics / fuel / electric).
- IL-2 Sturmovik: per-component damage state (engine cylinders, oil cooler, control cables).
- "Game Engine Architecture" (Jason Gregory, 3rd ed. 2018, Ch. 14) — component-based damage patterns.
- Glenn Fiedler "Deterministic Lockstep" (Gaffer On Games 2014) — required for damage cascades in lockstep.
- Closed `2026-06-21-component-vehicle-damage-model` (yes) — ground vehicle precedent.

---

## 3. Method

- **Type:** prototype + benchmark (standalone C++26 CPU analytical cost model).
- **Strategies (5):**
  - `A_Indestructible` — baseline, no damage tracking, projectile just deletes (current arcade default).
  - `B_GlobalHP` — single HP per aircraft, <100 ns/hit but no component info.
  - `C_HitTable_HealthPool` — precomputed 3D hit-table + per-component HP, target architecture.
  - `D_HitTable_HealthPool_Cascade` — C + cascading failure (fire spread, fuel leak, oil loss).
  - `E_FullFEMAnalytical` — analytical proxy for full 6-DOF breakable FEM cost (rejected, for comparison only).
- **Scenes (5):** aircraft_density × 5:
  - `patrol` — 4 aircraft
  - `dogfight` — 16 aircraft (8v8)
  - `squadron` — 64 aircraft (32v32)
  - `large_battle` — 256 aircraft
  - `bombing_run` — 512 aircraft + AAA flak
- **Projectile types (6):** 7.7mm, 12.7mm, 20mm, 30mm, 50mm, 88mm HE (per WT datamine `ammunition` table).
- **Aircraft templates (3):** generic fighter (6 components), bomber (12 components), heavy bomber (20 components).
- **Metrics:** mean/median/p95 time per tick (µs), per-projectile cost (ns), per-aircraft cost (µs), cascade event count.
- **Control:** A as speed baseline; E as feasibility reference (rejected for production).
- **Protocol:** 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time target < 2 sec on Zen 3 5800X per `hardware-profile.md §1`.

---

## 4. Prototype

Location: `prototype/aircraft_damage_bench.cpp` (~600 LoC planned).

```bash
cd prototype && mkdir -p build && cd build
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic ../aircraft_damage_bench.cpp -o aircraft_damage_bench
./aircraft_damage_bench [iter=1000] [warmup=10] [seed=42]
```

Output: `build/results.csv` (125,001 rows: header + 125,000 measurements).
Build: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, expected green.

---

## 5. Results

_Pending Phase 4-5._

---

## 6. Verdict

_Pending Phase 5._

---

## 7. Integration recommendation

_Pending Phase 5._ Will follow `agent/knowledge.md §30.4` 3-step migration precedent:
- Step 1 (XS, ~80 LoC) `src/physics/AircraftDamage.{hpp,cpp}` + HitTable struct + ProjectileHit dispatch.
- Step 2 (M, ~300 LoC) per-component HP + cascading failure + integration with `BallisticProjectile` (closed yes) and `FixedWingFlightModel` (closed yes).
- Step 3 (S, ~100 LoC) `PROJECTV_AIRCRAFT_DAMAGE=NONE|HIT_TABLE|FULL_FEM` env gate + Tracy plot "Aircraft Damage Tick" + `ProjectVAircraftDamageTests` unit test.

---

## 8. Sources

_See `sources.md` (Phase 2 pending)._

---

## 9. Mapping to ProjectV hot-path

The prototype models the **aircraft-damage tick** hot path: per-frame update of active projectile hits + per-component HP update + cascading failure evaluation.

- **Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X) + §3 (RTX 3060 Ti, 8 GiB VRAM) + §4 (Vulkan 1.4.341). Data captured 2026-06-21, dev host `obvium`.
- **Unmeasured:**
  - Real GPU particle dispatch overhead (analytical proxy only).
  - Flecs ECS component registration overhead.
  - Network serialization of damage state for lockstep multiplayer (closed `lockstep-state-sync-hybrid-netcode` mixed validates).
  - Voxel chunk intersection for damage voxel destruction (closed `explosion-crater-terrain-deformation` yes precedent).
  - Cross-vendor GPU performance.
- **Production dominated cost:** at 512 aircraft × 16 projectiles/tick, **cascade evaluation** (D strategy) is the only non-trivial cost; hit-table lookup is essentially free (O(1) array index).
