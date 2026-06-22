# Results — Infantry Soldier Simulation Benchmark

This document presents the timing results, cache analysis, and architectural conclusions derived from the standalone CPU benchmark for the infantry soldier simulation.

## 1. Summary of Performance Data

The benchmark was executed on a **Zen 3 (AMD Ryzen 7 5800X)** CPU. The times below represent the **mean execution time per soldier-step (in nanoseconds)**, averaged over 5 random seeds (each run consisting of 100 simulation ticks representing 10 seconds of simulated time at 10 Hz).

| Strategy | Skirmish (64) | Company (256) | Battalion (1024) | Brigade (4096) | Division (16384) | Average (All Scales) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **A: Baseline (Simple AoS)** | 17.5 ns | 17.5 ns | 17.2 ns | 16.2 ns | 16.7 ns | **17.02 ns** |
| **B: Stamina + Loadout (AoS)** | 19.4 ns | 20.2 ns | 21.4 ns | 19.5 ns | 20.2 ns | **20.14 ns** |
| **C: Stamina + Limb HP (AoS)** | 25.9 ns | 28.6 ns | 28.5 ns | 26.2 ns | 26.3 ns | **27.10 ns** |
| **D: Stamina + Limb HP + Medical (AoS)** | 31.7 ns | 30.8 ns | 33.2 ns | 30.5 ns | 31.5 ns | **31.54 ns** |
| **E: Vectorized (SoA)** | 16.0 ns | 15.8 ns | 14.9 ns | 16.4 ns | 16.2 ns | **15.86 ns** |

---

## 2. Key Analysis & Insights

### SoA vs. AoS Cache Locality & SIMD Vectorization
1. **The SoA Advantage (Strategy E vs. Strategy D):**
   Strategy E implements the exact same detailed physical simulation as Strategy D (stamina fatigue, loadout weight, 7-limb HP pools, bleeding, fracture propagation, and prioritization medical aid checks). However, by converting the data layout from **Array-of-Structures (AoS)** to **Structure-of-Arrays (SoA)** and using `#pragma omp simd`, the compiler was able to fully vectorize the loops.
   - Strategy D (AoS): **31.54 ns** per soldier-step.
   - Strategy E (SoA): **15.86 ns** per soldier-step.
   - **Speedup:** **~1.99× (2.0×)** improvement.

2. **SoA Outperforming Simple Baseline (Strategy E vs. Strategy A):**
   Strategy E is actually **faster** than Strategy A (the baseline AoS update that only does simple state machine transitions and a single global HP check).
   - Strategy A (AoS): **17.02 ns** per soldier-step.
   - Strategy E (SoA): **15.86 ns** per soldier-step.
   - This proves that cache locality and SIMD alignment are more significant factors than the raw complexity of the mathematical updates for CPU-bound simulations.

3. **Incremental Cost of Features (AoS):**
   - Adding **Stamina + Loadout** to the baseline (A → B) increases step time by **~3.1 ns** (+18%).
   - Adding **7-Compartment Limb Damage** (B → C) increases step time by **~7.0 ns** (+35%), due to branching loop logic over the 7 limbs, and checking bleed/damage distribution.
   - Adding **Medical Treatment Logic** (C → D) increases step time by **~4.4 ns** (+16%), due to priority checks (bandages → splints → medkits → painkillers).

---

## 3. Scale Scaling Analysis

The update cost per soldier-step remains remarkably flat across all scales (from 64 up to 16,384 soldiers).
- For **Strategy E (SoA)**, the update time fluctuates between **14.9 ns** (at 1024 entities) and **16.4 ns** (at 4096 entities).
- For **Strategy D (AoS)**, it ranges from **30.5 ns** to **33.2 ns**.

This flat curve indicates that the simulation fits comfortably within the CPU cache hierarchy. A group of 16,384 soldiers in SoA format takes only about:
$$\text{Memory Size} = 16384 \times \left(1 + 4\times 6 + 1 + (4 \times 4 \times 7)\right) \approx 2.3 \text{ MB}$$
This easily fits into the 32 MB L3 cache of the Ryzen 5800X, preventing any DRAM bottlenecking.

---

## 4. Conclusion & Mainline Impact

Implementing detailed infantry simulation with full limb-damage, medical treatment, and stamina fatigue is extremely feasible. At **15.86 ns** per soldier-step:
- Simulating **10,000 active soldiers** takes **0.158 ms** (158 microseconds) per tick.
- This represents only **3.1%** of a 5 ms frame budget, and **0.9%** of a 60 Hz frame budget (16.6 ms).
- We recommend using **Flecs ECS** query systems with SoA components (`StaminaComponent`, `LimbHealthComponent`, `MovementStateComponent`) to achieve this high-performance layout natively.
