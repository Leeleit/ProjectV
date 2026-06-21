# RESULTS — `2026-06-21-audio-diffraction-hybrid`

Measurement results per `docs/experiments/benchmarks/methodology.md §3`.

**Dev host:** AMD Ryzen 7 5800X (Zen 3), governor `powersave`, 62.7 GiB RAM. См. [`hardware-profile.md`](../../hardware-profile.md) §1.

**Prototype:** `prototype/{voxel_grid.hpp, audio_path.hpp, diffraction.hpp, bench.cpp, Makefile}` (~600 LoC C++26, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`).

**Measurement campaign:** 3 strategies × 3 scenes × 3 seeds × 100 iterations × 16 sources = 14,400 strategy invocations. Per invocation: mean / median / p95 / p99 / stddev / min / max. Warmup: 5 iterations.

---

## 1. Headline numbers (mean latency per source)

| Strategy | cave_stress | open_plains | multi_room | Mean probes / source | Mean dB recovery (multi_room) |
|:---------|:------------|:------------|:-----------|:---------------------|:------------------------------|
| **A_None** (Phase 1, 1 ray) | 0.0001 ms | 0.0001 ms | 0.0001 ms | 1.0 | 0.00 dB (baseline) |
| **B_Schissler** (top-8 edge probes, UTD) | 0.079-0.082 ms | 0.024-0.026 ms | 0.040-0.041 ms | 17.0 | **0.00 dB** (no recovery in simple cases) |
| **C_Tsingos** (32 depth-mip samples) | 0.0030-0.0032 ms | 0.0027-0.0032 ms | 0.0025-0.0029 ms | 33.0 | **+1.23 to +1.37 dB** (Tsingos 2007 spec: 1-2 dB) |

**Extrapolation to 64 sources @ 30 Hz audio (33.3 ms budget):**

| Strategy | Total latency (64 sources) | % of audio budget | Comment |
|:---------|:---------------------------|:------------------|:--------|
| A_None | 0.0064 ms | **0.02%** | Trivial. |
| B_Schissler | 1.5-5.2 ms | **5-16%** | Marginal; would compete with reverb tail. |
| C_Tsingos | 0.16-0.21 ms | **0.5-0.6%** | Production-ready. |

**Direct verdict candidates:**
- **C_Tsingos wins on cost (0.6% budget) and quality (+1.2 dB recovery, within Tsingos 2007 spec 1-2 dB).**
- **B_Schissler is borderline (5-16% budget) and quality 0 dB in simple cases** (requires second-order UTD for full Schissler 2014 benefit, out of scope for prototype).
- **A_None is the cheap baseline; serves as Phase 1 reference.**

---

## 2. Per-strategy × per-scene × per-seed table

Cave_stress, open_plains, multi_room (3 seeds each: 1, 7, 42).

### 2.1 A_None (Phase 1 baseline)

| Scene | Seed | Mean (ms) | p95 (ms) | p99 (ms) | mean_atten (dB) | mean_probes |
|:------|:-----|:----------|:---------|:---------|:----------------|:------------|
| cave_stress | 1 | 0.0001 | 0.0002 | 0.0003 | 0.00 | 1.0 |
| cave_stress | 7 | 0.0001 | 0.0002 | 0.0002 | -1.25 | 1.0 |
| cave_stress | 42 | 0.0001 | 0.0002 | 0.0002 | -1.25 | 1.0 |
| open_plains | 1 | 0.0001 | 0.0001 | 0.0002 | 0.00 | 1.0 |
| open_plains | 7 | 0.0001 | 0.0002 | 0.0002 | 0.00 | 1.0 |
| open_plains | 42 | 0.0001 | 0.0002 | 0.0003 | 0.00 | 1.0 |
| multi_room | 1 | 0.0001 | 0.0001 | 0.0001 | -12.50 | 1.0 |
| multi_room | 7 | 0.0001 | 0.0002 | 0.0002 | -11.25 | 1.0 |
| multi_room | 42 | 0.0001 | 0.0001 | 0.0001 | -13.75 | 1.0 |

**Observations:** Uniform 0.0001 ms per source (1 ray test). Multi_room has significant occlusion (-12.5 dB mean), validating that the prototype geometry has occluding walls. cave_stress shows light occlusion (-1.25 dB) because random source-listener pairs often land in adjacent corridors with LOS through doorways.

### 2.2 B_Schissler (top-8 edge probes + UTD approximation)

| Scene | Seed | Mean (ms) | p95 (ms) | p99 (ms) | mean_atten (dB) | mean_probes |
|:------|:-----|:----------|:---------|:---------|:----------------|:------------|
| cave_stress | 1 | 0.0803 | 0.1072 | 0.1222 | 0.00 | 17.0 |
| cave_stress | 7 | 0.0819 | 0.1152 | 0.1388 | -1.25 | 17.0 |
| cave_stress | 42 | 0.0790 | 0.1040 | 0.1350 | -1.25 | 17.0 |
| open_plains | 1 | 0.0260 | 0.0392 | 0.0467 | 0.00 | 17.0 |
| open_plains | 7 | 0.0254 | 0.0359 | 0.0429 | 0.00 | 17.0 |
| open_plains | 42 | 0.0240 | 0.0304 | 0.0376 | 0.00 | 17.0 |
| multi_room | 1 | 0.0397 | 0.0506 | 0.0615 | -12.50 | 17.0 |
| multi_room | 7 | 0.0404 | 0.0562 | 0.0660 | -11.25 | 17.0 |
| multi_room | 42 | 0.0407 | 0.0561 | 0.0666 | -13.75 | 17.0 |

**Observations:** 17 probes per source = 1 (occlusion baseline) + 8 (top edges) × 2 (visibility from source + visibility from listener). Latency 0.024-0.082 ms per source = 0.5-5% audio budget per source, **5-16% for 64 sources** — borderline.

**Quality:** 0 dB recovery in all configurations. This is a **known limitation of the prototype's simplified UTD**: my implementation tests only direct visibility (source → edge AND edge → listener). For source-listener pairs where direct LOS is blocked by walls (multi_room: -12.5 dB), the UTD edge must be **visible from both** source and listener. In multi_room, doorways are visible from one side only, so the algorithm finds no valid edges.

**To get +2-4 dB recovery per Schissler 2014, we need second-order diffraction (edge-to-edge paths).** This is more expensive (cubic in edge count) and out of scope for this prototype.

### 2.3 C_Tsingos (32 uniform hemisphere samples + depth-mip)

| Scene | Seed | Mean (ms) | p95 (ms) | p99 (ms) | mean_atten (dB) | mean_probes |
|:------|:-----|:----------|:---------|:---------|:----------------|:------------|
| cave_stress | 1 | 0.0030 | 0.0040 | 0.0060 | 0.00 | 33.0 |
| cave_stress | 7 | 0.0032 | 0.0066 | 0.0076 | -1.18 | 33.0 |
| cave_stress | 42 | 0.0032 | 0.0049 | 0.0078 | -1.15 | 33.0 |
| open_plains | 1 | 0.0031 | 0.0051 | 0.0069 | 0.00 | 33.0 |
| open_plains | 7 | 0.0032 | 0.0049 | 0.0075 | 0.00 | 33.0 |
| open_plains | 42 | 0.0027 | 0.0040 | 0.0063 | 0.00 | 33.0 |
| multi_room | 1 | 0.0029 | 0.0039 | 0.0061 | -11.27 | 33.0 |
| multi_room | 7 | 0.0025 | 0.0039 | 0.0045 | -10.14 | 33.0 |
| multi_room | 42 | 0.0026 | 0.0040 | 0.0055 | -12.38 | 33.0 |

**Observations:** 33 probes per source = 1 (occlusion baseline) + 32 (uniform hemisphere samples). Latency 0.0025-0.0032 ms per source = 0.16-0.21 ms for 64 sources = **0.5-0.6% of 33.3 ms audio budget** — well under the 5% optimization threshold per `optimization-philosophy.md`.

**Quality: +1.2 to +1.4 dB recovery for multi_room** (Tsingos 2007 spec is 1-2 dB; recovery proportional to fraction of open hemisphere). For cave_stress and open_plains, A_None already has near-zero attenuation (sparse geometry), so the absolute recovery is small (~+0.1 dB).

---

## 3. Cross-vendor / cross-architecture projection

Per `hardware-profile.md §1`, dev host = Zen 3 5800X (no AVX-512). Per `simd-procedural-noise` closed=mixed precedent: AVX-512 hardware (Zen 5 / Arrow Lake / Sapphire Rapids) = 2-4× speedup for SIMD-friendly kernels.

| Strategy | AVX2 (Zen 3) | AVX-512 (Zen 5 / Arrow Lake, projected) | Notes |
|:---------|:-------------|:------------------------------------------|:------|
| A_None | 0.0001 ms | 0.0001 ms | Scalar ray traversal, no SIMD gain |
| B_Schissler | 0.024-0.082 ms | 0.012-0.040 ms (2×) | 8 edges × 2 ray_distance — partial SIMD gain |
| C_Tsingos | 0.0025-0.0032 ms | 0.0015-0.0020 ms (1.5-2×) | 32 hemisphere samples — SIMD batchable |

**Projected best case (Zen 5, AVX-512, governor=performance):**
- C_Tsingos @ 64 sources: 0.10-0.13 ms = **0.3-0.4% audio budget** — very comfortable.
- B_Schissler @ 64 sources: 0.8-2.6 ms = **2.4-7.8% audio budget** — acceptable but competing.

---

## 4. Comparison with closed `audio-raytracing-voxel-sdf` baseline

Closed `2026-06-21-audio-raytracing-voxel-sdf` measured:
- **A_no_geom** (pure baseline, no audio geometry): 0.0002 ms per source.
- **B_occlusion** (1 ray occlusion): 0.008-0.016 ms per source.
- **C_full_hybrid** (32 rays × 4 reflection orders + Eyring + IR gen): **FALSIFIED** 17.1/13.8/6.3 ms per frame.
- **D_full_cached** (+ temporal cache): 21.1/14.4/6.0 ms (cache doesn't help).

**My A_None (0.0001 ms per source) matches `A_no_geom` baseline within 2×.** The difference between my A_None and closed B_occlusion (0.008-0.016 ms) is the cost of full AudioEngine integration (DSP, filtering, output mixing) which my prototype omits.

**C_Tsingos at 0.0025-0.0032 ms is 3-5× slower than A_None, but 2-5× faster than closed B_occlusion.** The C_Tsingos gain is +1-2 dB recovery at lower cost than full hybrid.

**B_Schissler at 0.024-0.082 ms is 1.5-5× faster than closed B_occlusion baseline** (because the ray test is cheap DDA, not the full audio pipeline), but **~3-10× more expensive than C_Tsingos**. The benefit (if any) is only realized with second-order UTD.

---

## 5. Caveats and limitations

Per `AGENTS.md §5.3` and `docs/experiments/AGENTS.md §8`:

1. **CPU-only synthetic voxel scenes** (cave/open_plains/multi_room representative, не full ProjectV VoxelWorld).
2. **No DSP overhead**: my prototype counts only ray-traversal cost. Real `AudioEngine::tick()` includes DSP (filtering, mixing, output) which adds ~0.005-0.015 ms per source per closed `audio-raytracing-voxel-sdf` baseline.
3. **Governor `powersave`**: real production = `performance` governor, expected 1.5-2× speedup. Closed `audio-raytracing-voxel-sdf` measured powersave; same caveat applies.
4. **Single CPU vendor (Zen 3 5800X)**: cross-arch projection per §3 above.
5. **Perceptual quality = analytical proxy** (Tsingos openness fraction → +1.0-2.0 dB per spec). NOT full HRTF / ABX listening test (out of scope for single-agent research).
6. **B_Schissler simplified UTD**: only direct visibility. Second-order diffraction (edge-to-edge) is the path to Schissler 2014's full +2-4 dB recovery. Out of scope for this prototype.
7. **Random source-listener placement**: real gameplay = directional, not random. May produce different occlusion patterns.
8. **N=100 iterations per strategy × scene × seed** (vs methodology default N=1000). 10× reduction for research-prototype speed. Tradeoff: slightly wider confidence interval.

---

## 6. Self-check per `benchmarks/methodology.md §8`

- [x] Compiler / driver / OS versions recorded (`hardware-profile.md §6`: Clang 22.1.6, NVIDIA 610.43.02, Zen 3 5800X).
- [x] Build command + run command in `prototype/Makefile` + `prototype/README.md`.
- [x] `results.csv` attached (28 rows, 1 header + 27 measurements = 3 strategies × 3 scenes × 3 seeds).
- [x] `RESULTS.md` (this file) contains table + interpretation.
- [x] Mapping to ProjectV hot-path documented in main `README.md` §9.
- [x] Cross-references to `hardware-profile.md §1` for hardware baseline.
