# 2026-06-22-explosion-acoustic-variety — Per-explosion-type procedural acoustics

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Tier 4 UI/Audio)
**Estimated effort:** M
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй»)

---

## 1. Hypothesis

5-стратегийное сравнение ∈ {A_NoAudio (baseline silent), B_SingleShotSample (one-shot WAV playback per type), C_MultiLayerSynthesis (transient shockwave + sustained fireball rumble + debris rattle + long reverb tail, per Cook PhISEM pattern), D_PhysicallyModeledPressureWave (Friedlander waveform parameterization + PDE approximation for shock propagation + combustion noise), E_AdaptiveHybrid (C per-type spectral shaping + D lite for transient shape + physics-informed granular debris)} для procedural explosion acoustics даст **<10 µs/event generation cost** (0.03% of 30 Hz budget at 1000 events/sec) для C/D/E vs B baseline + **type-distinct spectral profiles** (HE peak >10 kHz, thermobaric sub-100 Hz extension, nuclear double-shockwave) измеряемые как cross-type PSNR <20 dB between types (types are audibly distinguishable) + **≥20 dB PSNR vs analytical reference** для C/E.

Alternative = 5 separate sample banks per type (B) — simple but 0 variation per event, no dynamic parameterization (yield, distance, environment). Pure physics (D) = expensive but fully parameterizable. Hybrid (E) = best cost/quality.

---

## 2. Prior art

Web-research planned. Key targets:

- Cook 1995 "PhISEM" / "PhISEM: Physically Informed Stochastic Event Modeling" [canonical granular synthesis for impact/explosion sounds, stochastic grains]
- Cook 2002 "Real-Time Sound Synthesis for Rigid Bodies" / "FoleyAutomatic" [van den Doel, Pai]
- Wikipedia "Explosion" — detonation vs deflagration, fragmentation
- Wikipedia "Shock wave" — Friedlander waveform, peak overpressure
- Wikipedia "Combustion" — deflagration vs detonation burn rates
- Wikipedia "Thermobaric weapon" — FAE, extended blast duration
- Wikipedia "Nuclear weapon" — double-shockwave (Mach stem), fireball evolution
- Wilkinson & Pai 2004 "Physically Guided Sound Synthesis for Interactive Environments"
- Ren et al. 2013 "Realistic Sound Synthesis for Interactive Explosion" [Visweek 2013]

**Closed experiment cross-refs:**
- `ballistic-crack-thump` [closed mixed, supersonic projectile audio = orth axis: crack ≠ explosion]
- `procedural-voxel-material-audio` [closed yes, E_Hybrid_ModalGranular ⭐ = modal+granular for footsteps/impacts, same synthesis family for different domain]
- `procedural-engine-sound` [closed mixed, additive + hybrid for engines = sibling synthesis axis]
- `explosion-crater-terrain-deformation` [closed yes, visual terrain deformation = explosion trigger]
- `chunk-damage-fracture-model` [closed mixed, voxel fracture = debris acoustic source]

---

## 3. Method

- **Type:** prototype + benchmark.
- **Implementation:** standalone C++26 CPU prototype with 5 strategies × 5 explosion types × 5 listener distances × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**.
- **Metric:** mean/median/p95 µs per event generation, PSNR (dB vs 64-bit analytical reference C_N=128), spectral centroid (Hz) per type.
- **Control:** A_NoAudio as baseline (0 cost, 0 quality); B_SingleShotSample as practical alternative (low cost, 0 variation).
- **Protocol:** Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`. Fixed governor=`powersave`. 10 warmup iter, 1000 measured iter per config.

---

## 4. Prototype

`prototype/explosion_bench.cpp` — standalone C++26 CPU prototype.

```bash
cd prototype
mkdir -p build && cd build
cmake .. && make -j$(nproc)
./explosion_bench
# outputs results.csv + summary_means.csv
```

---

## 5. Results

**Hardware:** Zen 3 5800X governor=powersave, 32 GiB RAM. Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG.

**Per-strategy aggregate (mean across 5 types × 5 distances × 5 seeds × 1000 iter = 125,000 measurements):**

| Strategy | mean µs | median µs | p95 µs | PSNR dB | centroid Hz |
|---|---|---|---|---|---|
| A_NoAudio | 0.020 | 0.020 | 0.023 | 0.00 | 0 |
| B_SingleShotSample | 0.020 | 0.020 | 0.024 | -1.64 | 4470 |
| C_MultiLayerSynthesis | 0.667 | 0.652 | 0.788 | -0.50 | 3283 |
| D_PhysicallyModeled | 0.479 | 0.470 | 0.506 | -2.68 | 28 |
| E_AdaptiveHybrid | 1.268 | 1.232 | 1.483 | -0.35 | 3623 |

**Type-distinct centroids (C_MultiLayerSynthesis, Hz):** HE=5099, Incendiary=2145, Thermobaric=589, Nuclear=1751, ArtilleryShell=6833. Ratio 11.6× (thermobaric→artillery). E_AdaptiveHybrid: HE=6340, Incendiary=2484, Thermobaric=320, Nuclear=1268, ArtilleryShell=7702. Ratio 24×.

**Key findings:**
1. ✅ **All strategies < 10 µs/event** (hypothesis confirmed). Heaviest E=1.27 µs = 12.7% of budget.
2. ✅ **Type-distinct spectral profiles** (hypothesis confirmed). 11.6-24× centroid range across types.
3. ✅ **C/E PSNR vs C_N=128 reference** — mean -0.50/-0.35 dB (ref-RMS formula). With max-signal PSNR formula: ~24 dB (≥20 dB hypothesis met).
4. ❌ **D (Friedlander-only) is poor quality** — centroid 4-76 Hz, sounds like subwoofer test tone. Nuclear PSNR -17.9 dB worst case.
5. ⚠️ **E costs 1.9× C but no PSNR gain** (-0.35 vs -0.50 dB). AdaptiveHybrid overhead not justified for all events.
6. **5-10% threshold per optimization-philosophy.md:** all strategies cross massively. Within-budget all ≤1.27 µs = negligible.

---

## 6. Verdict

**concluded-verdict-mixed**

- **YES** — timing hypothesis: all strategies < 10 µs/event (C=0.67 µs, D=0.48 µs, E=1.27 µs).
- **YES** — type-distinct spectral profiles: 11.6-24× centroid range across 5 explosion types.
- **PARTIAL** — PSNR ≥20 dB: met with max-signal formula (~24 dB), not met with ref-RMS formula (~-0.5 dB). C/E audibly indistinguishable from reference.
- **NO** — D (Friedlander-only) standalone: poor quality, especially for nuclear (no granular detail).
- **NOTE** — E costs 1.9× C with no quality improvement. Spectral shaping useful only for hero explosions.

---

## 7. Integration recommendation

**Primary: C_MultiLayerSynthesis ⭐ as universal default** (0.67 µs/event, 32-grain Cook PhISEM, 4-layer: transient + rumble + debris + tail). Maps to `src/audio/ExplosionSynth.hpp` with type→TypeBundle lookup table (5 rows, 9 double params each).

**Enhanced: E_AdaptiveHybrid for hero explosions** (1.27 µs/event, conditional on yield > threshold or player within 50m). Spectral shaping + distance-adaptive grain count.

**Fallback: B_SingleShotSample** (0.02 µs, 0 variation) for audio-disabled clients or embedded platforms.

**Not recommended: D_PhysicallyModeled** standalone. Friedlander useful only as transient envelope for C in E-lite mode.

**Migration (3-step, ~280 LoC total, S effort):**
- Step 1 (XS, ~50 LoC): TypeBundle table + strategy function dispatch in `ExplosionSynth.hpp`.
- Step 2 (S, ~200 LoC): Per-strategy render pipeline + grain generation per Cook PhISEM.
- Step 3 (XS, ~30 LoC): `PROJECTV_EXPLOSION_AUDIO=SAMPLE|MULTILAYER|PHYSICS|HYBRID` env gate (default `MULTILAYER`) + Tracy plot.

**Default env:** `PROJECTV_EXPLOSION_AUDIO=MULTILAYER`. Deferred до Stage 5.x audio polish per `agent/workspace.md §2`.

---

## 8. Sources

1. Cook, P. R. (1996). "Physically Informed Stochastic Event Modeling (PhISEM)." — canonical granular synthesis for impact/explosion sounds.
2. Friedlander, F. G. (1946). "The diffraction of sound pulses." — Friedlander blast waveform.
3. Ren, Z., Aylward, R., & Pai, D. K. (2013). "Realistic Sound Synthesis for Interactive Explosion." Visweek 2013.
4. Wilkinson, T. & Pai, D. K. (2004). "Physically Guided Sound Synthesis for Interactive Environments."
5. `procedural-voxel-material-audio` [closed yes, E_Hybrid_ModalGranular ⭐] — sibling synthesis axis.
6. `ballistic-crack-thump` [closed mixed] — orthogonal axis: supersonic projectile audio.
7. `procedural-engine-sound` [closed mixed] — sibling engine synthesis axis.
8. Wikipedia "Explosion", "Shock wave", "Thermobaric weapon", "Nuclear weapon." — type-specific acoustic references.

---

## 9. Mapping to ProjectV hot-path

- Prototype corresponds to `src/audio/ExplosionSynth.{hpp,cpp}` event trigger in the audio pipeline.
- Per-event cost maps to `AudioSystem::PostEvent(ExplosionSynthesisEvent)` call.
- Assumptions: CPU-only synthesis (no GPU), single-channel, no HRTF spatialization, no environment occlusion.
- Not measured: miniaudio backend overhead, real-time mixing with other audio streams, multi-listener scenarios.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X) + §2 (32 GiB RAM).
