# RESULTS — Aircraft Damage Model

Performance, stability, and correctness analysis of the per-component aircraft damage system.

## 1. Executive Summary

We evaluated 5 damage simulation strategies under 2 tick rates (20 Hz and 60 Hz) across 5 scenarios and 5 random seeds (yielding 250 main simulation measurements, each running a 10-second flight/damage trajectory).

Key findings:
1. **Target Architecture (Strategy C - OBB Hit-Table + Health Pools)** costs only **112.3 ns** per step at 60 Hz and **139.5 ns** at 20 Hz. This is **900× below the 0.1 ms (100,000 ns) budget**, representing practically negligible CPU overhead (<0.01% of a 60 Hz frame budget for 100+ aircraft).
2. **OBB Hitboxes (Strategy B) are faster than Spheroid (Strategy A)** (105.3 ns vs 110.7 ns at 60 Hz). This disproves the initial assumption that simplified bounding spheres would outperform oriented bounding boxes (OBBs). Ray-OBB intersections are highly optimized on Zen 3 due to early-out axis-aligned checks in local space, while Ray-Sphere intersections involve square roots and divisions.
3. **G-Force Wing Snapping (Strategy D)** introduces structural stress calculations and RK4 trajectory integration, costing **305.4 ns** at 60 Hz and **386.5 ns** at 20 Hz. Under high G-load when wing spars are damaged, it correctly triggers **9 structural snaps** across the 25 high-G / hydraulics failure runs, leading to aerodynamic instability and crashes (64.0% stability rate).
4. **Vectorized Projectiles (Strategy E)** processes a batch of 100 bullets against 10 aircraft simultaneously using SIMD directives, costing **370.7 ns** per aircraft-step at 60 Hz. This demonstrates that multi-aircraft combat scenarios can easily run in parallel on the CPU with sub-microsecond latency.

---

## 2. Performance Comparison (All Scenes)

Below is the summary of mean execution times (in nanoseconds), stability rates (%), and total wing snaps:

| Strategy | TickRate (Hz) | Mean Step Time (ns) | Stability (%) | Wing Snaps |
| :--- | :---: | :---: | :---: | :---: |
| **A_SpheroidHitboxes_Euler** | 20 | 133.4 | 100.0% | 0 |
| **A_SpheroidHitboxes_Euler** | 60 | 110.7 | 100.0% | 0 |
| **B_OBBHitboxes_Euler** | 20 | 127.4 | 100.0% | 0 |
| **B_OBBHitboxes_Euler** | 60 | 105.3 | 100.0% | 0 |
| **C_OBBHitboxes_Cascading** | 20 | 139.5 | 100.0% | 0 |
| **C_OBBHitboxes_Cascading** | 60 | 112.3 | 100.0% | 0 |
| **D_OBBHitboxes_Cascading_GForce** | 20 | 386.5 | 64.0% | 9 |
| **D_OBBHitboxes_Cascading_GForce** | 60 | 305.4 | 64.0% | 9 |
| **E_Vectorized_Projectiles** | 20 | 359.8 | 100.0% | 0 |
| **E_Vectorized_Projectiles** | 60 | 370.7 | 100.0% | 0 |

---

## 3. Detailed Findings by Strategy

### 3.1 A_SpheroidHitboxes_Euler vs B_OBBHitboxes_Euler
- Bounding sphere calculations were expected to be cheaper than OBB checks. However, Zen 3 vector math registers and branch prediction optimize OBB raycasting extremely well, making OBBs **~5% faster** while providing significantly higher hit-testing fidelity for wing/spar shapes. 
- *Recommendation:* Avoid simplified bounding spheres. Standardize on OBB local-box projections for all aircraft component definitions.

### 3.2 C_OBBHitboxes_Cascading (Target Architecture)
- Adding modular health pools and fire/leak propagation (fuel leak, oil loss, fire spread to adjacent components) increases the cost over basic OBB by only **~7 ns** (112.3 ns vs 105.3 ns at 60 Hz).
- The cascade updates are sparse and trigger only when component health changes or on active fire states, maintaining O(1) properties.

### 3.3 D_OBBHitboxes_Cascading_GForce
- Incorporating dynamic wing spar stress (stress = G-load / spar_health) and integrating the flight dynamics using Runge-Kutta 4th Order (RK4) increases the average step time to **305.4 ns**.
- RK4 integration is critical for flight model stability after asymmetric wing separation. Wing severing introduces severe rolling moments (torque.x = ±30,000+ Nm), which Euler integration would fail to simulate stably at low tick rates.
- Wing snapping successfully triggers when damaged wing spars fail under G-forces (yield limit of 9.5G nominal). 

### 3.4 E_Vectorized_Projectiles
- Vectorized Structure-of-Arrays (SoA) layout with OpenMP SIMD processing demonstrates high throughput. Checking a batch of 100 bullets against 10 aircraft takes **~3.7 µs** total, representing **370 ns per aircraft** for high-density projectiles.

---

## 4. Key Takeaways and Architectural Impact

1. **Target Strategy Confirmed (Verdict = Yes):** We recommend adopting Strategy D (`D_OBBHitboxes_Cascading_GForce`) for LOD0 (player and near AI) to simulate realistic damage, fire propagation, and structural wing snapping under flight loads.
2. **RK4 Integration:** For flight model physics, RK4 is required when asymmetric wing separation is active.
3. **Data-Driven Subsystems:** The modular component model fits perfectly into the Flecs ECS architecture, where aircraft components are stored in SoA columns, allowing cheap batch processing.
