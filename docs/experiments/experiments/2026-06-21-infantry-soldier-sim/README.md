# 2026-06-21-infantry-soldier-sim — High-Fidelity Infantry Soldier Simulation

**Status:** `concluded-verdict-yes`
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** independent (military sandbox Tier 1 Core Engine Systems: Physics/AI)
**Estimated effort:** M
**Author:** agent (self)

---

## 1. Hypothesis

Detailed infantry soldier simulation (including a 6-state movement state machine, loadout gear weight fatigue, heart rate integration, a 7-compartment limb injury model, and medical treatment logic) can be executed at **<0.5 µs per soldier** on CPU. Storing agent states in a Structure-of-Arrays (SoA) layout aligned with Flecs ECS queries enables simulating **10,000+ active soldiers within a 5 ms frame budget** (under 10% of a 30 Hz/60 Hz frame), representing negligible overhead for large-scale battle scenarios.

**Key claims:**
1. **Movement State Machine + Stamina Fatigue** (idle, walk, run, sprint, crouch, prone) under loadout gear weight can be updated in **<100 ns** per soldier.
2. **Escape from Tarkov-style limb HP model** (7 distinct compartments, status effects like bleeding/fractures, damage distribution from blacked-out limbs) costs **<150 ns** per soldier.
3. **Medical treatment system** (evaluating injuries, applying bandages/splints/medkits/painkillers) costs **<150 ns** per soldier.
4. **Vectorized SoA layout** (Strategy E) achieves a **2–4× speedup** over Object-Oriented (AoS) layouts due to CPU L1/L2 cache locality and SIMD alignment.

---

## 2. Prior art

See [`sources.md`](./sources.md) for full literature. Focus areas:
- **Arma 3 Stamina System:** Stamina consumption scaled by loadout weight (kg), linking heart rate directly to weapon sway.
- **Escape from Tarkov Health Model:** 7-limb compartment HP, blacked-out limbs distributing excess damage to remaining parts via multipliers, fractures, light/heavy bleeding.
- **Flecs ECS SoA Layout:** cache-friendly data alignment for high-density entities.

---

## 3. Method

- **Type:** prototype + benchmark (standalone C++26 CPU simulation).
- **Strategies (5):**
  - `A_Baseline_Simple` — simple state machine, single global HP pool, no loadout impact.
  - `B_Stamina_Loadout` — state machine + stamina pool + gear weight fatigue + heart rate & weapon sway.
  - `C_Stamina_LimbDamage` — B + 7-limb HP pools + bleeding/fractures + blacked-out limb damage distribution.
  - `D_Stamina_LimbDamage_Medical` — C + medical treatment simulation (applying bandages, splints, medkits, painkillers).
  - `E_Vectorized_ECS_Simulation` — D implemented in an optimized Structure-of-Arrays (SoA) layout aligned for compiler auto-vectorization.
- **Scenes (5 scales):**
  - `skirmish` — 64 soldiers
  - `company` — 256 soldiers
  - `battalion` — 1024 soldiers
  - `brigade` — 4096 soldiers
  - `division` — 16384 soldiers
- **Metrics:** mean/median/p95 time per step (µs), per-soldier update cost (ns), bleeding recovery rate, active casualties.
- **Protocol:** 5 strategies × 5 scales × 5 seeds × 1000 iter = **125,000 main measurements**, wall time < 1 sec on Zen 3 5800X.

---

## 4. Prototype

Location: `prototype/infantry_soldier_bench.cpp`.

```bash
cd prototype && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make
./infantry_soldier_bench
```

Output: `build/results.csv`.

---

## 5. Results

Detailed benchmark results have been recorded in [`RESULTS.md`](./RESULTS.md).
- **AoS Baseline vs SoA Vectorized:** The Structure-of-Arrays (SoA) layout (Strategy E) runs the entire stamina, loadout, limb injury, and medical treatment simulation in **~15.9 ns** per soldier-step. This is a **2.0× speedup** over the Object-Oriented (AoS) layout (Strategy D, **~31.5 ns**).
- **SoA Cache Locality:** Strategy E (SoA) is even faster than the simplistic AoS baseline (Strategy A, **~17.0 ns**), showing that cache layout is more critical than operation complexity.
- **Feasibility:** Update overhead is flat from 64 to 16,384 soldiers, staying within the CPU L2/L3 cache boundary. 10,000 active soldiers can be simulated in **0.16 ms**, representing negligible overhead (<1% of a 60 Hz frame).

---

## 6. Verdict

**Verdict: YES.** High-fidelity infantry soldier simulation is highly feasible and extremely fast if implemented using a Structure-of-Arrays (SoA) format. It should be standard for all high-density entity simulations.

---

## 7. Integration recommendation

We recommend full integration using Flecs ECS. Follow the 3-step migration:
- **Step 1 (XS, ~80 LoC):** Define `SoldierState`, `StaminaComponent`, and `LimbHealthComponent` structs compatible with Flecs ECS. Use separate components to keep queries cache-optimal.
- **Step 2 (M, ~350 LoC):** Implement the `InfantrySimulationSystem` running state machine transitions, stamina drain/recovery under load, limb damage propagation, and medical treatment. Ensure compiler auto-vectorization is not blocked by non-contiguous memory allocations.
- **Step 3 (S, ~120 LoC):** Hook up to the vehicle component system (soldier egress/ingress) and flight models (parachuting/g-loads), add Tracy profiling plots and unit tests.

---

## 8. Sources

_See `sources.md`._
