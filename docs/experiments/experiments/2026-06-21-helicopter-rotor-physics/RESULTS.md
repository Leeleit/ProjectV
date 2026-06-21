# RESULTS — Helicopter rotor physics simulation benchmark results

Benchmark executed on dev host `obvium` (Zen 3 CPU, GCC 16.1.1, C++26, `-O3 -march=native`).

---

## 1. Executive Summary

- **Strategy D (4-Blade BET + Flapping + RK4)** is the high-fidelity reference. With the removal of the redundant damping term (which caused stiff numerical explosions), Strategy D achieves **96.0% stability at 60 Hz** with a mean step time of **1.34 µs** (well below the 100 µs target).
- **Strategy A (Momentum Theory LOD)** is extremely fast: **80.4 ns** (0.08 µs) per step, achieving **80.0% stability** across all scenarios. Its trajectory error compared to Strategy D is ~63 meters over a 10-second run (due to lack of lateral aerodynamic coupling).
- **20 Hz Tick Rate is Stiffly Unstable for Flapping**: At 20 Hz, Strategy D and E (Vectorized) have **0% stability** because the coning/flapping dynamics oscillate at ~5 Hz (nominal 300 RPM rotor speed). With only 4 samples per cycle, the explicit RK4 solver explodes.
- **Autopilot Sensitivity**: In hover stability scenarios, the phase delay of coning/flapping (90-degree gyroscopic precession lag) requires lower PD gains (`-0.15` roll/pitch feedback instead of `-0.5`) to prevent pilot-induced oscillations (PIO) and crashes on the high-fidelity models (B, C, D, E).
- **Vectorized (SoA) Performance**: Strategy E (SoA batching of 10 helicopters) shows a per-aircraft cost of **1.43 µs**, which is slightly higher than Strategy D (1.34 µs) due to the overhead of gathering/scattering states in our synthetic CPU test loop, but still demonstrates that 100+ helicopters can be simulated in <0.15 ms on a single thread.

---

## 2. Numerical Performance Table

Average step times and stability percentages over 5 seeds and 5 scenarios (250 runs total):

| Strategy | Tick Rate (Hz) | Mean Step Time (ns) | Step Time (µs) | Stability (%) | Mean Traj Error vs D@200Hz (m) |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **A_MomentumTheory_LOD** | 20 | 81.1 | 0.08 | 80.0% | 63.7 |
| **A_MomentumTheory_LOD** | 60 | 80.4 | 0.08 | 80.0% | 62.9 |
| **B_BladeElement_2Blades** | 20 | 242.2 | 0.24 | 40.0% | 41.8 |
| **B_BladeElement_2Blades** | 60 | 218.4 | 0.22 | 60.0% | 49.2 |
| **C_BladeElement_4Blades** | 20 | 359.5 | 0.36 | 40.0% | 48.1 |
| **C_BladeElement_4Blades** | 60 | 311.2 | 0.31 | 60.0% | 47.7 |
| **D_BladeElement_4Blades_Flapping** | 20 | 1537.4 | 1.54 | 0.0% | 999999.9 (Exploded) |
| **D_BladeElement_4Blades_Flapping** | 60 | 1339.4 | 1.34 | **96.0%** | **6.4** |
| **E_Vectorized_Helicopters** | 20 | 1445.2 | 1.45 | 0.0% | 999999.9 (Exploded) |
| **E_Vectorized_Helicopters** | 60 | 1430.5 | 1.43 | **96.0%** | **6.4** |

---

## 3. Scenario-Specific Observations (at 60 Hz)

### 3.1 Hover Stability (`hover_stability`)
- **Momentum Theory (A)**: Stable (100%), but oscillates slightly because it lacks lateral aerodynamics.
- **Blade Element (B, C, D, E)**: Fully stable (100% for B and C; 80% for D and E). For D, seed 3 was unstable due to extreme gusty wind (30 knots) causing control feedback divergence.
- **Physics Lag**: Unlike Strategy A, the blade element models exhibit significant rotor thrust tilt delay (90-degree phase shift). Autopilot gains must be reduced to avoid PIO.

### 3.2 Forward Flight (`forward_flight`)
- **Rigid Blade Models (B, C)**: Instantly pitch down, roll laterally (due to asymmetrical lift from advancing vs retreating blades) and dive into the ground. Stability = 0%.
- **Flapping Models (D, E)**: Blade flapping naturally compensates for the advancing/retreating lift asymmetry. As a result, the aircraft experiences translational lift and stable pitch behavior, remaining fully stable (100%) and matching the reference path with very low error (~3.4 m).

### 3.3 Vortex Ring State (`vortex_ring_state`)
- All models at 60 Hz are stable (did not crash below -100m).
- The high-fidelity model (D) captures the **VRS lift penalty** (dropping to 50% lift during rapid vertical descent), resulting in a sudden 30-meter altitude drop before regaining control in cleaner air.

### 3.4 Autorotation (`autorotation`)
- All models at 60 Hz are stable (100%).
- Rotor RPM drops to ~12 rad/s after engine failure, then stabilizes and recovers to ~22 rad/s due to updraft airflow. The helicopter glides safely with low descent rate.

---

## 4. Key Engineering Takeaways

1. **Explicit integration limit**: If dynamic coning and flapping are simulated, the minimum physics rate must be $\ge 50$ Hz. At 20 Hz, the coning equations are stiffly unstable.
2. **Autopilot coupling**: Aerodynamic cross-coupling (yaw torque from main rotor, roll-pitch gyroscopic precession) is naturally emergent in Strategy D. This means the flight controller must be tuned specifically for each LOD tier.
3. **CPU cost is negligible**: With a step cost of 1.34 µs for the highest LOD, there is no need to write a complex GPU compute-shader solver for player-controlled helicopters. The CPU-based flight model is cheap enough to run on the main physics thread.
