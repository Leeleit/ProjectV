# 2026-06-22-procedural-voxel-material-audio — Procedural voxel material interaction audio synthesis

**Status:** in-progress
**Date opened:** 2026-06-22
**Stage link:** independent (cross-cuts Stage 3.x interaction → Stage 6+ immersion → Stage 5.x audio polish)
**Estimated effort:** M (single session: web-research + standalone C++26 CPU prototype + measurements + writeup)
**Author:** research-агент `docs/experiments/`

---

## 1. Hypothesis

**Главная гипотеза (3-clause):**

> **H1 (cost):** 5-стратегийное сравнение синтеза material interaction sounds (footstep, block place/break, tool impact, item drop) per-material per-event даст **<5 µs per synthesized interaction** (for 44.1 kHz, 0.5 s audio buffer) для non-baseline strategies (C/D/E); 1000 interactions/sec = 5 ms/frame = 15% of 30 Hz budget — manageable for gameplay-critical interactions.
>
> **H2 (quality):** non-baseline strategies (C/D/E) достигают **PSNR ≥30 dB** vs sample-based reference (B) для 10+ voxel material classes (stone, dirt, grass, wood, metal, gravel, sand, snow, glass, water), with perceptually distinguishable material signatures.
>
> **H3 (architecture):** **E_Hybrid_ModalGranular ⭐ = universal recommended default** — modal synthesis (phISAM) for rigid materials (stone, metal, wood, glass) + granular synthesis (phISEM) for aggregate/fluid materials (gravel, sand, snow, water, dirt) + filtered noise burst for friction/transient; C_ModalSynthesis as rigid-only fallback; D_GranularSynthesis as aggregate-only fallback; B_SampleBased as optional audiophile fallback.

**Что проверяю:** какой стратегии синтеза (no-audio / sample-playback / pure-modal / pure-granular / hybrid-modal+granular) достаточно для real-time voxel material interaction audio с per-material acoustic signature, velocity-sensitive amplitude, randomized variation, при 10+ material classes и CPU budget <15% of 30 Hz frame.

**Альтернативы, которые отвергаю:**

- **A_NoAudio (baseline)** — silent, для замера overhead reference.
- **GPU audio DSP** — overkill для material interaction synthesis (CPU sufficient для 1000 events/sec); deferred.
- **Full FEM physical simulation** — за пределами scope; modal synthesis с 4-12 modes — production pattern per garjan/FoleyAutomatic.
- **ML-based synthesis** (SonicGauss, DeepModal, DDSP) — требует GPU inference, обученных моделей, недетерминирован; deferred до Stage 7+.

---

## 2. Prior art

Web-research via `web_search` (working this session). См. [`sources.md`](./sources.md) для полного списка. Ключевые источники:

- **FoleyAutomatic** (van den Doel et al. SIGGRAPH 2001) — canonical: modal models driven by contact forces. Impact, rolling, sliding. "audio-force" sampled at audio rate from physics simulation running at video rate.
- **Cook PhISM** (ICMC 1996) — PhISAM (modal/resonant) + PhISEM (granular/stochastic). Foundational framework for all physically-informed synthesis.
- **Turchet footstep synthesis** (DAFx 2010, Applied Acoustics 2016) — complete footstep engine: solid surfaces → modal synthesis; aggregate (gravel, snow, sand) → PhiSM stochastic; liquid → dedicated model.
- **garjan** (Rust crate, 2026) — production modal synthesis engine: 4-12 resonant modes/material, 10 impact materials, 8 footstep terrains. Performance: Impact (Metal) = 1.4 ms for 1s audio (710× real-time). SIMD-friendly SoA layout, priority-based polyphony.
- **pbrAudioShaders** (Malcom3D 2025) — Hertzian contact + modal synthesis + material properties (Young's modulus, density, damping).
- **IRCAM Modalys** — material property tables for 50+ materials (density, Young's modulus, Poisson ratio, loss factor).
- **O'Brien, Cook, Essl** (SIGGRAPH 2001) — FEM + modal synthesis for deformable body impact sounds.

---

## 3. Method

### 3.1 Strategies

| Strategy | Description | Reference | Expected cost |
|:---------|:------------|:----------|:-------------|
| **A_NoAudio** | Baseline silence; measure function-call overhead | — | ~0 µs |
| **B_SampleBased** | Pre-recorded WAV per material class (0.5 s, 44.1 kHz, 16-bit mono = ~44 kB/material × 10 = 440 kB); play with linear gain + pitch variation | Game industry standard | ~0.5 µs (memcpy + gain) |
| **C_ModalSynthesis** | PhISAM-inspired: bank of N=8 damped sinusoidal oscillators per material; frequencies/decays from material properties (density, Young's modulus); velocity → amplitude; random detune ±5% for variation | van den Doel 2001, Cook 1996, garjan | ~3-5 µs (8 resonators × exp/sin) |
| **D_GranularSynthesis** | PhISEM-inspired: stochastic grain cloud per material class; Poisson-distributed micro-impacts (λ=50-500/s); grain = exponential decay noise burst (2-10 ms); density/size from material granularity | Cook 1996 PhISEM, Turchet 2010 | ~2-4 µs (Poisson + grain gen) |
| **E_Hybrid_ModalGranular ⭐** | Material-class dispatcher: rigid → C_Modal (stone, metal, wood, glass); aggregate → D_Granular (gravel, sand, snow, dirt); liquid → filtered noise burst (water); fluid → granular + tonal (mud, wet gravel) | Cook 1996 hybrid, Turchet 2016 holistic | ~3-5 µs (dispatcher overhead + per-type dispatch) |

### 3.2 Voxel material classes and acoustic parameters

| Material | Density (kg/m³) | Young's modulus (GPa) | Loss factor η | Synthesis strategy | Modal freqs (Hz) | Grain density λ (/s) |
|:---------|:---------------:|:---------------------:|:-------------:|:-----------------:|:-----------------:|:--------------------:|
| Stone | 2700 | 50 | 0.01 | C_Modal | 200, 450, 800, 1200, 1800, 2500, 3500, 5000 | — |
| Dirt | 1500 | 0.01 | 0.10 | D_Granular | — | 300 |
| Grass | 1200 | 0.005 | 0.15 | D_Granular | — | 200 |
| Wood | 700 | 12 | 0.02 | C_Modal | 150, 350, 600, 900, 1300, 1800, 2500, 3500 | — |
| Metal | 7800 | 200 | 0.005 | C_Modal | 500, 1200, 2200, 3500, 5000, 7000, 9500, 13000 | — |
| Gravel | 1700 | 0.1 | 0.08 | D_Granular | — | 500 |
| Sand | 1600 | 0.001 | 0.20 | D_Granular | — | 400 |
| Snow | 300 | 0.001 | 0.30 | D_Granular | — | 100 |
| Glass | 2500 | 70 | 0.002 | C_Modal | 800, 1800, 3200, 5000, 7000, 9500, 12000, 16000 | — |
| Water | 1000 | 0.002 | 0.50 | D_Granular (fluid variant) | — | 150 |
| Concrete | 2400 | 30 | 0.015 | C_Modal | 180, 400, 700, 1100, 1600, 2200, 3000, 4200 | — |

### 3.3 Interaction types

| Type | Description | Velocity mapping | Duration |
|:-----|:------------|:----------------|:---------|
| Impact | Block place, tool hit, item drop | amplitude ∝ v² (kinetic energy) | 0.3-1.0 s decay |
| Footstep | Player walking/running on surface | amplitude ∝ v (momentum) + GRF envelope | 0.1-0.3 s |
| Footslide | Player sliding/stopping | continuous friction noise | 0.5-2.0 s |
| Scrape | Tool scraping across surface | continuous friction + mode excitation | 0.3-1.0 s |
| Dig | Shovel into aggregate material | grain burst density ∝ v | 0.2-0.5 s |

### 3.4 Measurement protocol

- **Hardware baseline:** см. `hardware-profile.md` §1-4 (Zen 3 5800X, powersave governor, 32 MiB L3, no AVX-512).
- **Compiler:** Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`.
- **Scenes (5):** 10 materials × (impact + footstep) × (velocity low/med/high) × 5 seeds = **1500 configs** per strategy.
- **Warmup:** 10 iterations before measurement.
- **Iterations:** 1000 per config.
- **Metrics:** mean µs per interaction, mean µs per 1024-sample audio block, PSNR vs B_SampleBased reference, memory (code+data).

### 3.5 Quality metric

PSNR (dB) vs B_SampleBased reference, computed over 0.5 s alignment window:

PSNR = 20 × log₁₀(MAX / √MSE), where MAX = 32767 (16-bit signed).

---

## 4. Prototype

**Location:** `prototype/`
**Build:** `prototype/build/` (self-contained, not корневой build/)
**File:** `prototype/material_audio_bench.cpp`
**Build command:**
```bash
cd prototype && mkdir -p build && cd build && \
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
  ../material_audio_bench.cpp -o material_audio_bench -lm && \
./material_audio_bench 2>&1 | tee run.log
```

### 4.1 Sample generation parameters (C/D/E)

- Sample rate: 44.1 kHz
- Output block: 1024 samples (~23 ms)
- Max event duration: 0.5 s (22050 samples)
- Modal resonators: 8-pole IIR filterbank (2nd-order section per mode)

### 4.2 Validation

- C/D/E outputs compared vs B via PSNR.
- All strategies verify non-null output (non-zero samples).
- Timing via `std::chrono::high_resolution_clock` (nanosecond precision, 1 s per-measurement wall time guard).

---

## 5. Results

(Заполняется после прогона прототипа)

---

## 6. Verdict

(Заполняется после анализа результатов)

---

## 7. Integration recommendation

(Заполняется в случае positive/mixed verdict)

---

## 8. Sources

См. [`sources.md`](./sources.md).
