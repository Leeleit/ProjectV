# Results — Terrain Traction Variation Benchmark

This document presents the performance metrics, scaling characteristics, and architectural findings of the terrain traction and wheel slip simulation.

## 1. Summary of Performance Data

The benchmark was executed on an **AMD Ryzen 7 5800X (Zen 3)** CPU. The times below represent the **mean execution time per wheel-step (in nanoseconds)**, averaged over 5 random seeds across 625 simulation ticks (10 seconds of simulated time at 60 Hz).

| Strategy | Jeep (64) | Truck (256) | Convoy (1024) | Regiment (4096) | Division (16384) | Average (All Scales) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **A: Constant Traction (AoS)** | 4.9 ns | 4.1 ns | 4.1 ns | 4.2 ns | 4.1 ns | **4.28 ns** |
| **B: Surface Lookup (AoS)** | 1.6 ns | 1.0 ns | 1.3 ns | 1.3 ns | 1.7 ns | **1.38 ns** |
| **C: Slip Linear (AoS)** | 5.3 ns | 4.3 ns | 4.2 ns | 4.4 ns | 4.4 ns | **4.54 ns** |
| **D: Slip Pacejka (AoS)** | 36.2 ns | 33.3 ns | 36.2 ns | 35.3 ns | 35.0 ns | **35.20 ns** |
| **E: Vectorized SoA (Pacejka)** | 27.1 ns | 26.7 ns | 29.2 ns | 27.9 ns | 27.9 ns | **27.76 ns** |

---

## 2. Key Analysis & Insights

### AoS vs. SoA Vectorization Trade-offs
1. **The Pacejka Transcendental Math Bottleneck:**
   Strategy E (SoA vectorized) achieves **27.76 ns** per wheel-step compared to Strategy D (AoS) at **35.20 ns**.
   - **Speedup:** **~1.27×** (a **21%** performance reduction).
   - Unlike basic memory-bound logic, the performance of the Pacejka Magic Formula is heavily dominated by transcendental math calls (`std::atan`, `std::sin`). Although the compiler (GCC 16) successfully vectorizes these operations on Zen 3 using SIMD math library extensions (AVX2 libmvec), the latency of division, two `atan` calls, and one `sin` call sets a hard execution floor.

2. **Linear Slip vs. Pacejka:**
   - Strategy C (Linear Slip) is extremely fast at **4.54 ns** per wheel-step, as it only requires simple clamp and min/max operations.
   - Strategy D (Pacejka) increases execution time to **35.20 ns** (~7.7× increase), due to the transcendental functions.
   - **Trade-off:** Linear slip is extremely cheap and suitable for distant/LOD vehicles, whereas Pacejka provides physically correct force curves (peak grip slip ratio around 0.1–0.15, followed by a sliding friction drop-off) and should be used for the player's vehicle and nearby vehicles.

3. **Flat Scaling:**
   The execution times remain completely flat across all scales (64 wheels to 16,384 wheels). A cohort of 16,384 wheels in SoA consumes:
   $$\text{Memory Size} = 16384 \text{ elements} \times 14 \text{ float/uint8 arrays} \approx 917 \text{ KB}$$
   This is well within the 32 MB L3 cache of the Ryzen 5800X, resulting in zero DRAM latency bottlenecking.

---

## 3. Conclusion & Mainline Impact

Dynamic traction lookup and tire slip calculations are incredibly cheap on modern hardware. Even with full non-linear Pacejka formula evaluations:
- Simulating a division of **16,384 wheels** (about 2,730 vehicles) takes only **0.45 ms** per tick on a single CPU core.
- For a realistic battlefield scenario with 200 vehicles (approx. 1,200 wheels), the simulation cost is **33 µs** (0.033 ms), representing **<0.7%** of a 5 ms physics budget.
- **LOD Recommendation:** We recommend a 2-tier LOD architecture:
  - **LOD0 (Nearby/Active):** Use **Strategy E (Vectorized SoA Pacejka)** for maximum physical realism (wheel spin, locking, drift stability).
  - **LOD1 (Distant/LOD):** Use **Strategy C (Linear Slip)** or **Strategy B (Surface Lookup)** to save CPU cycles.
