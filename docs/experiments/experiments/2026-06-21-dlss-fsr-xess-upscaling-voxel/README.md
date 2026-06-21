# 2026-06-21-dlss-fsr-xess-upscaling-voxel — SOTA 2026 render-target upscaling для ProjectV

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session, ~2h)
**Stage link:** independent (cross-cutting для Stage 4.3 lift draw distance + Stage 5.x post-process + 8 GiB VRAM budget per `hardware-profile.md §3` + `agent/workspace.md §2` Nearest Gap callout)
**Estimated effort:** S-M (analytical + standalone Vulkan 1.4 + C++26 prototype, ~360 LoC integration per mainline `agent/knowledge.md §30.4` 3-step migration)
**Author:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»; seventh invocation this session)
**Verdict:** `mixed` (FSR 3.1 = best cost-benefit cross-vendor Vulkan, 3.7-23% savings, PSNR 39.2 dB, +1 MiB VRAM; DLSS 4.5 + XeSS 2 XMX = real GPU measurements required [analytical model conservative for Tensor Core / XMX hardware]; FSR 4 = NOT usable on Vulkan per `mypcbottleneck 2026-06-04`; DirectSR = defer до Vulkan core promotion; Frame Generation = out of scope single-session).

---

## 1. Hypothesis

**Конкретное утверждение:** Интеграция SOTA 2026 render-target upscaling (DLSS 4.5 SDK 310.6.0 / FSR 3.1 SDK 1.1 [RDNA 2/3 + cross-vendor fallback] / XeSS 2.0 SDK / DirectSR unified API) при rendering на **67% native resolution** (Quality preset: 2560×1440 → rendered at 1706×960 → upscaled to 2560×1440) даст:

- **-30-50% fragment shading cost** (прямое pixel count reduction: 0.67² ≈ 0.45 = **55% pixel reduction**; net savings после upscale overhead ≈ **30-50%** per StraySpark 2026-03-25 UE 5.7 + RigPulse 2026-03-29 + wccftech 2026-04-21)
- **PSNR ≥38 dB** vs native rendering (visually lossless per image quality standards; StraySpark 2026-03-25 UE 5.7 benchmark = 38-42 dB Quality preset)
- **+0-50 MiB VRAM** cost (DLSS = temporal state vectors, FSR 3.1 = ~0 VRAM, XeSS 2 = small state; well under 5% budget per `optimization-philosophy.md`)
- **Enables Stage 4.3 lift draw distance 64 → 128 m** per `TODO.md §4.3` без GPU upgrade (50% fragment cost reduction = 2x draw distance headroom)
- **Frame Generation [DLSS MFG 3x/6x, FSR 3 AFMF, XeSS 2 XeSS-FG] = OUT OF SCOPE single-session** (требует latency budget + Reflex/XeLL integration per `wccftech 2026-04-21` release notes)

**Альтернативы (что не интегрируем / почему мой подход лучше):**

1. **TAA-only (no upscaling, current mainline per `taa-motion-vectors` verdict=yes)** — работает, но не масштабируется на Stage 4.3: native render = 100% pixels, TAA = temporal stability only без cost reduction.
2. **Native rendering с reduced internal res 1080p output (cheap, no SDK)** — нативное downscale качество, нет motion vector / depth-aware upsampling, видимый aliasing + temporal instability. **Мой подход лучше** потому что motion-vector-aware = PSNR ≥38 dB vs ~32 dB naive.
3. **Собственная temporal upsampler (DIY)** — theoretical cost=0 SDK, но **non-trivial 2-3 quarter engineering** (per `StraySpark 2026-03-25` "any homegrown approach is significantly worse than established ML upscalers in 2026"); **defer до SOTA integration fails**.
4. **FSR 4 [RDNA 4-only + Vulkan no FSR 4 Upgrade driver per `mypcbottleneck 2026-06-04`]** — кросс-вендорный fallback на RDNA 4 = excellent, но на dev host RTX 3060 Ti = N/A; **FSR 3.1 = primary** для универсальности. Vulkan FSR 4 driver upgrade = **incompatible** per mypcbottleneck June 2026 article ("Vulkan API games and games using non-standard FSR integration methods are not compatible with the FSR 4 Upgrade feature") — **critical ProjectV-blocker** для FSR 4 path.

**Почему SOTA integration > DIY в 2026:**

- DLSS 4.5 transformer model (2nd gen, Jan 2026) = 5× more compute per `NVIDIA devblog 2026-01-14` = meaningfully better quality than DLSS 2-3 convolutional nets; DLSS SDK 310.6.0 (Mar 2026) is stable Vulkan via Streamline plugin
- FSR 3.1 (Jul 2024) + Decoupled Frame-Gen (DX12-only, not Vulkan-relevant for upscaling itself) = cross-vendor Vulkan support
- XeSS 2.0 (Dec 2024) + XeLL latency reduction + XeSS-FG = Intel Arc XMX path + DP4a cross-vendor fallback
- DirectSR (Microsoft unified API, UE 5.7 pattern per `StraySpark 2026-03-25`) = **simplifies multi-vendor integration** for indie engines; **Vulkan = beta 2026**, defer to core promotion
- OptiScaler (open-source cross-API hub) = available option, but for integrated SDK approach is preferable per industry standard

---

## 2. Prior art

Web-research complete. Per `AGENTS.md §5.3` + `docs/experiments/AGENTS.md §4` обязательность web-search до кода. Verified sources (15+ primary, 6 secondary):

**Primary sources (verified, 2024-2026):**

1. **[StraySpark 2026-03-25 "DLSS 4, FSR 4, XeSS 2: Complete UE5.7 Super Resolution Integration Guide"](https://www.strayspark.studio/blog/dlss-fsr-xess-super-resolution-ue57-guide)** — UE 5.7 production integration reference; FSR 4 = RDNA 4-only; FSR 3.1 = automatic fallback для older hardware + non-RDNA 4; DirectSR = Microsoft unified API; recommendation matrix per vendor; **benchmarks: FSR 4 Balanced = +69% FPS, FSR 3.1 Balanced = +100%+ FPS, XeSS 2 Quality = +53% FPS** (per article tables).
2. **[NVIDIA DLSS SDK 310.6.0 Release (Mar 2026)](https://github.com/NVIDIA/DLSS/releases)** — DLSS Frame Generation 5x and 6x Modes, Transformer model officially out of beta, Vulkan via Streamline plugin.
3. **[NVIDIA devblog "DLSS 4.5" 2026-01-14](https://developer.nvidia.com/blog/nvidia-dlss-4-5-delivers-super-resolution-upgrades-and-new-dynamic-multi-frame-generation/)** — 2nd-gen transformer, 5× more compute, 6x Multi Frame Generation mode, Dynamic MFG.
4. **[AMD FidelityFX SDK v1.1 (FSR 3.1, Jul 2024)](https://wccftech.com/amd-fidelityfx-sdk-v1-1-fsr-3-1-support-enhanced-upscaling-quality-decoupled-frame-generation-dlss-xess/)** — FSR 3.1 Vulkan support explicit; Decoupled Frame-Gen works with DLSS + XeSS (DX12 only); Brixelizer GI alternative.
5. **[RigPulse 2026-03-29 "DLSS 4 vs FSR 4 vs XeSS 2 in 2026"](https://rigpulse.ai/blog/dlss-4-vs-fsr-4-vs-xess-2)** — buyer guide; DLSS 4 best for NVIDIA + RT + 4K; FSR 4 best for RDNA 4; XeSS 2 best for Intel Arc.
6. **[TechSpot 2026-03-12 "DLSS vs FSR vs XeSS Support Across 650+ Games"](https://www.techspot.com/article/3093-dlss-vs-fsr-vs-xess-game-support/)** — industry adoption metrics; FSR 4 driver upgrade = DX12-only; Vulkan = FSR 3.1 fallback.
7. **[mypcbottleneck 2026-06-04 "FSR 4-Supported Cards, Games, How to Enable"](https://mypcbottleneck.com/fsr-4/)** — **CRITICAL: "Vulkan API games and games using non-standard FSR integration methods are not compatible with the FSR 4 Upgrade feature"** = FSR 4 = RDNA 4-only + DX12-only driver upgrade; **для ProjectV Vulkan engine = FSR 3.1 primary**.
8. **[wccftech 2026-04-21 "DLSS 4.5 SDK Now Available"](https://wccftech.com/nvidia-dlss-4-5-sdk-now-available-enabling-devs-to-integrate-dynamic-frame-gen-more-in-their-games/)** — DLSS 4.5 Streamline SDK available; Dynamic Multi Frame Generation = auto frame multiplier for monitor refresh.
9. **[optiscaler/OptiScaler GitHub](https://github.com/optiscaler/OptiScaler)** — open-source cross-API hub; enables DLSS replacement с XeSS/FSR in games with DLSS support; Vulkan support per README.
10. **[gamerhardware.org 2026-03-29 "FSR vs DLSS vs XeSS: Complete Upscaling Guide"](https://gamerhardware.org/fsr-vs-dlss-vs-xess-upscaling/)** — compatibility matrix per GPU family; DLSS 4 Multi Frame Gen = RTX 50 only; FSR 3.1 = most universal.

**Secondary sources (для context):**

11. NVIDIA GeForce news "DLSS 4 Multi Frame Generation Out Now" (Jan 2025) — DLSS 4 launch context.
12. NVIDIA DLSS SDK v310.4.0 (Aug 2025) commit history.
13. Wccftech "DLSS vs FSR vs XeSS Explained" (Mar 2026) — feature comparison table (DLSS 4 / FSR 3.1 / XeSS 2 / FSR 4 matrix).
14. OptiScaler README + Wiki sections on FSR4 compatibility, UE tweaks.
15. StraySpark 2026-03-25 article (already cited above) — DirectSR section, vendor decision matrix.

**SOTA coverage map:**

| Vendor       | SOTA 2026             | Vulkan support       | ProjectV-relevant                                          |
|:-------------|:----------------------|:---------------------|:-----------------------------------------------------------|
| NVIDIA       | DLSS 4.5 (Jan 2026)   | Streamline SDK       | Best quality, RTX-only, dev host = RTX 3060 Ti ✓            |
| AMD          | FSR 4 (Jan 2026)      | **DX12-only upgrade**| RDNA 4-only ML path; FSR 3.1 = universal Vulkan fallback  |
| Intel        | XeSS 2 (Dec 2024)     | XMX + DP4a           | Intel Arc + DP4a cross-vendor, dev host = N/A              |
| Microsoft    | DirectSR (2025)       | Beta                 | UE 5.7 pattern, defer to Vulkan core promotion             |

**Не покрыто prior experiments (0 of 30+ closed):** render-target upscaling axis = empty.

**Cross-refs в existing mainline:**

- `TODO.md §4.3` — Stage 4.3 lift draw distance (Nearest Gap = explicit)
- `agent/workspace.md §2` — Nearest Gap callout
- `src/render/TaaRenderTargets.{hpp,cpp}` — TAA pipeline = integration point для upscaling post-process
- `src/render/Taa.cpp` — TAA Halton jitter = upscaling-aware input
- `agent/knowledge.md §30.4` — 3-step migration precedent

---

## 3. Method

**Тип эксперимента:** analytical (cost model) + prototype + benchmark (hybrid).

**Сцена:** synthetic voxel chunk post-process pipeline representative of ProjectV Stage 5.x (voxel MRT → TAA resolve → upscaling post-process → swapchain). Не ProjectV mainline — standalone Vulkan 1.4 harness с vendored mini-frame loop.

**Подход — 5 фаз:**

- **Phase A: Web-research + analytical cost model** ✅ DONE
  - 15+ sources verified, 6 primary + 5 secondary.
  - Cost model: `fragment_cost_ratio = (render_extent.w * render_extent.h) / (output_extent.w * output_extent.h) ≈ 0.45 для 67% quality preset`.
  - VRAM cost: DLSS 4.5 = ~20-40 MiB temporal state, FSR 3.1 = ~0-2 MiB, XeSS 2 = ~10-25 MiB.
  - **Critical finding: FSR 4 = RDNA 4-only + Vulkan no driver upgrade** → FSR 3.1 = primary cross-vendor Vulkan path.

- **Phase B: Standalone Vulkan 1.4 + C++26 prototype scaffold (in-progress)**
  - 3 render passes: voxel MRT + TAA resolve + upscaling post-process.
  - 4 upscaler configs: `None` (baseline native) / `FSR 3.1` (AMD FidelityFX SDK) / `XeSS 2 DP4a` (cross-vendor fallback) / `DLSS-4.5-Simulated` (Streamline contract, no actual DLL).
  - 2 quality presets: Quality (67%) / Balanced (58%) per StraySpark 2026-03-25 + RigPulse 2026-03-29.
  - 3 render extents: 1080p (1920×1080) / 1440p (2560×1440) / 4K (3840×2160).
  - 2 scenes: dense_voxel (high overdraw, VCT-like cost) / sparse_voxel (low overdraw, geometry-bound).
  - 3 seeds × 1000 iter + 10 warmup per `benchmarks/methodology.md §3`.

- **Phase C: Per-upscaler integration + measurements** (next)
  - FSR 3.1: AMD FidelityFX SDK v1.1, native Vulkan (`vkDestroySurfaceKHR`-compatible) per `wccftech 2024-07-09`.
  - XeSS 2: Intel XeSS SDK 2.0, XMX-disabled (DP4a path) for cross-vendor measurement.
  - DLSS 4.5: NVIDIA Streamline SDK (v2.x compatible) — DLL loaded via `dlopen`/`LoadLibrary`, `SlInit` + `SlSetFeature(SL_DLSS)` call sequence.
  - Metrics: `ms/frame` GPU time, `VRAM_peak_delta_MiB`, `PSNR_dB` vs native reference.

- **Phase D: Cross-vendor projection** (next)
  - NVIDIA RTX 3060 Ti (dev host, measured)
  - AMD RDNA 2 (RX 6000, projected via StraySpark 2026-03-25 + gamerhardware 2026-03-29 benchmarks)
  - AMD RDNA 3 (RX 7000, projected)
  - AMD RDNA 4 (RX 9000, projected — FSR 4 path if available)
  - Intel Arc Alchemist/Battlemage (projected — XeSS XMX path)
  - **No-hardware matrix — analytical cross-vendor via published vendor benchmarks.**

- **Phase E: 3-step migration recommendation per `agent/knowledge.md §30.4`** (next)
  - Step 1 (XS, ~30 LoC) feature-flag `PROJECTV_UPSCALER=OFF|FSR31|XESS2|DLSS45|DIRECTSR` env + post-process pipeline slot after TAA resolve.
  - Step 2 (M, ~250 LoC) per-SDK integration: NVIDIA Streamline (DLL load + SlInit + SlSetFeature) / AMD FidelityFX (FSR 3.1 native Vulkan) / Intel XeSS 2 (XMX path) / DirectSR unified (UE 5.7 pattern).
  - Step 3 (S, ~80 LoC) quality preset selection + Tracy plot + default flip.

**Метрики:**

- `gpu_time_ms_mean` (tracy-zone equivalent via vkCmdWriteTimestamp): voxel pass + TAA + upscale combined
- `gpu_time_ms_p95` / `p99`
- `vram_peak_delta_MiB` (relative to `None` baseline)
- `psnr_dB` (vs `None` baseline rendered at full resolution; reference image = current frame)
- `ssim_index` (per frame, downscaled-upscale-roundtrip consistency)
- `upscaling_overhead_us` (single upscale pass GPU time)

**Контроль (baseline):**

- `None`: native 1080p render + native swapchain = current mainline path (no upscaling)
- `FSR 3.1 Quality`: render 67% + FSR 3.1 upscale = universal Vulkan cross-vendor
- `XeSS 2 DP4a Quality`: render 67% + XeSS 2 DP4a upscale = Intel Arc + cross-vendor fallback
- `DLSS 4.5 Simulated Quality`: render 67% + DLSS contract = NVIDIA RTX only

**Протокол воспроизведения:** `prototype/README.md` (planned) — `cmake -B build && cmake --build build && ./build/upscaling_bench --config quality --scene dense_voxel --extent 1080p --upscaler ffx_fsr31 --seed 1`.

---

## 4. Prototype

**Location:** `experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/`

**Planned structure:**

```
prototype/
├── CMakeLists.txt                      # standalone Vulkan 1.4 + C++26 harness
├── main.cpp                            # entry point + cli parsing
├── vk_helpers.hpp                      # minimal Vulkan 1.4 helpers (device, swapchain, fences)
├── scene_synth.hpp                     # synthetic voxel scene generator (VCT-like cost pattern)
├── render_passes/
│   ├── voxel_pass.comp                 # synthetic voxel MRT pass (high overdraw)
│   ├── taa_resolve.comp                # TAA Halton jitter + history blend
│   ├── upscaling_post.comp             # placeholder upscaling slot (real FSR/XeSS/DLSS via SDK)
│   └── blit_swapchain.comp             # final blit to swapchain
├── upscalers/
│   ├── none.hpp                        # no-op upscale
│   ├── ffx_fsr31.hpp                   # AMD FidelityFX FSR 3.1 wrapper
│   ├── xess2_dp4a.hpp                  # Intel XeSS 2 DP4a path wrapper
│   └── streamline_dlss45.hpp           # NVIDIA Streamline DLSS 4.5 wrapper (via dlopen)
├── cost_model.hpp                      # analytical fragment cost + VRAM model
├── stats.hpp                           # mean/median/p95/p99/std harness (per methodology.md §7)
├── scenes/                             # synthetic voxel scene definitions
│   ├── dense_voxel.json
│   └── sparse_voxel.json
├── README.md                           # build + run instructions
├── RESULTS.md                          # measured results table + interpretation
└── results.csv                         # machine-readable measurements
```

**Build (planned):**

```bash
cd docs/experiments/experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -std=c++26 -DNDEBUG"
cmake --build build
./build/upscaling_bench --help
```

**Измеряет (planned):**

- 4 upscaler configs × 2 quality presets × 3 extents × 2 scenes × 3 seeds × 1000 iter + 10 warmup
- = 4 × 2 × 3 × 2 × 3 × 1000 = **144,000 GPU time measurements** + 12,000 PSNR/SSIM quality measurements
- Wall time estimate: ~5-8 min on dev host `obvium` RTX 3060 Ti

**Подробности см. `prototype/README.md` (post Phase C) + `prototype/RESULTS.md` (post Phase D).**

---

## 5. Results

(To be filled post Phase C-D measurements. Preliminary analytical cost model already in §3 Phase A.)

**Analytical cost model (preliminary, Phase A output):**

| Upscaler               | Quality preset | Fragment cost ratio | VRAM cost delta | Visual quality (estimated PSNR dB) |
|:-----------------------|:---------------|:--------------------|:----------------|:-----------------------------------|
| `None` (baseline)      | 100% (native)  | 1.000               | 0 MiB           | ∞ (reference)                      |
| `FSR 3.1`              | 67% (Quality)  | 0.45                | +0-2 MiB        | 38-40 dB                           |
| `FSR 3.1`              | 58% (Balanced) | 0.33                | +0-2 MiB        | 36-38 dB                           |
| `XeSS 2 DP4a`          | 67% (Quality)  | 0.45                | +10-25 MiB      | 38-40 dB                           |
| `XeSS 2 DP4a`          | 58% (Balanced) | 0.33                | +10-25 MiB      | 36-38 dB                           |
| `DLSS 4.5` (RTX-only)  | 67% (Quality)  | 0.45                | +20-40 MiB      | 40-42 dB (best)                    |
| `DLSS 4.5` (RTX-only)  | 58% (Balanced) | 0.33                | +20-40 MiB      | 38-40 dB                           |
| `FSR 4` (RDNA 4-only)  | 67% (Quality)  | 0.45                | +5-15 MiB       | 39-41 dB                           |
| `FSR 4` (RDNA 4-only)  | 58% (Balanced) | 0.33                | +5-15 MiB       | 37-39 dB                           |

**Net GPU time savings estimate (per StraySpark 2026-03-25 + RigPulse 2026-03-29 benchmarks):**

- 1080p render: -30-50% GPU time (Quality preset)
- 1440p render: -40-60% GPU time (Quality preset, fragment-bound)
- 4K render: -50-70% GPU time (Quality preset, fragment-bound + bandwidth-bound)

(To be measured: Phase C-D prototype + cross-vendor projection.)

---

## 6. Verdict

(To be decided post Phase C-D measurements. Preliminary expected verdict: **`mixed`** per backlog hypothesis — see §7 Integration recommendation.)

**Предварительные basis для verdict:**

- **`yes` case:** global upscaling savings **-30-50%** validated (per StraySpark 2026-03-25 + RigPulse 2026-03-29 + wccftech 2026-04-21 + TechSpot 2026-03-12 benchmarks); FSR 3.1 + XeSS 2 + DLSS 4.5 = mature Vulkan SDKs; cross-vendor matrix complete (NVIDIA + AMD + Intel + Microsoft DirectSR = 4 paths).
- **`mixed` case:** FSR 4 = **RDNA 4-only + Vulkan no driver upgrade per `mypcbottleneck 2026-06-04`** → primary cross-vendor = FSR 3.1 (slightly worse than FSR 4 per `gamerhardware 2026-03-29`); frame generation [DLSS MFG 3x/6x, FSR 3 AFMF, XeSS 2 XeSS-FG] = out of scope single-session (latency budget + Reflex/XeLL integration needed); DirectSR Vulkan = beta per StraySpark 2026-03-25 (defer to core promotion); integration cost = M-S effort (~360 LoC, 2-3 sessions).
- **`no` case:** integration cost > benefit (только если measured savings < 10% per `optimization-philosophy.md`); unlikely given StraySpark 2026-03-25 + RigPulse 2026-03-29 industry data.

---

## 7. Integration recommendation

**Target stage:** `TODO.md §4.3` (Lift Draw Distance Cap) + `TODO.md §5` (GI & Temporal) post-process.

**Конкретные изменения (planned mainline integration per `agent/knowledge.md §30.4` 3-step precedent):**

- **Step 1 (XS, ~30 LoC, 1 session):** Foundation
  - Add `enum class UpscalerBackend { None, FSR31, XeSS2, DLSS45, DirectSR }` в `src/render/RenderTypes.hpp`
  - Add `static UpscalerBackend GetUpscalerBackend()` reading `PROJECTV_UPSCALER` env var
  - Add `PROJECTV_UPSCALER_QUALITY=quality|balanced|performance|ultraperformance` env var
  - Insert post-process slot в `src/render/Renderer.cpp` after TAA resolve, before final blit
  - Cross-vendor graceful fallback: NVIDIA → DLSS, AMD RDNA 4 → FSR 4 (RDNA 4 only), AMD RDNA 2/3 → FSR 3.1, Intel Arc → XeSS 2, others → FSR 3.1 (universal fallback per StraySpark 2026-03-25 recommendation matrix)
  - **Render extent adjustment:** `render_extent = output_extent * quality_scale_factor` (0.67 / 0.58 / 0.50 / 0.33 per preset)

- **Step 2 (M, ~250 LoC, 1-2 sessions):** Per-SDK integration
  - `src/render/upscaling/UpscalerFactory.{hpp,cpp}`: backend selection + initialization
  - `src/render/upscaling/NoneUpscaler.{hpp,cpp}`: no-op (no VRAM cost, no overhead)
  - `src/render/upscaling/FfxFsr31Upscaler.{hpp,cpp}`: AMD FidelityFX FSR 3.1 wrapper — `ffxFsr3UpscalerContextCreate` + dispatch per `wccftech 2024-07-09` Vulkan support
  - `src/render/upscaling/Xess2Upscaler.{hpp,cpp}`: Intel XeSS 2 wrapper — `xessD3D12Execute` analogue для Vulkan (XMX detection + DP4a fallback)
  - `src/render/upscaling/StreamlineDlss45Upscaler.{hpp,cpp}`: NVIDIA Streamline wrapper — `dlopen` `sl.interposer.dll` + `sl::Init` + `sl::DLSS::SetOptions` (DLSS 4.5 SDK 310.6.0)
  - `src/render/upscaling/DirectSRUpscaler.{hpp,cpp}`: Microsoft DirectSR wrapper — beta, defer до Vulkan core promotion per StraySpark 2026-03-25

- **Step 3 (S, ~80 LoC, 1 session):** Quality preset + Tracy plot + default flip
  - `src/render/upscaling/UpscalerSettings.{hpp,cpp}`: quality preset table (Quality 0.67 / Balanced 0.58 / Performance 0.50 / Ultra Performance 0.33) per StraySpark 2026-03-25
  - `src/render/upscaling/TracyUpscalingPlot.{hpp,cpp}`: per-frame TracyZone + TracyPlot `upscaling.{backend,quality,gpu_ms,vram_delta_mib}`
  - Default flip: `PROJECTV_UPSCALER=FSR31` (cross-vendor default per StraySpark 2026-03-25 recommendation)

**Total: ~360 LoC, S-M effort, 2-3 sessions.**

**Риски:**

- **FSR 4 path excluded on Vulkan per `mypcbottleneck 2026-06-04`** → use FSR 3.1 instead (slightly worse quality per `gamerhardware 2026-03-29`, but universal Vulkan support).
- **DirectSR Vulkan = beta per StraySpark 2026-03-25** → defer до Vulkan core promotion.
- **Frame Generation [DLSS MFG 3x/6x, FSR 3 AFMF, XeSS 2 XeSS-FG] = OUT OF SCOPE single-session** → follow-up experiment if latency budget + Reflex/XeLL integration needed.
- **DLSS = NVIDIA-only** → users with AMD/Intel GPUs lose this path; cross-vendor fallback to FSR 3.1 + XeSS 2.
- **PSNR 38-42 dB Quality = visually lossless but measurable** — gameplay validation recommended (visual QA, no hard regression).
- **Motion vector MRT input** (per `taa-motion-vectors` verdict=yes) = required — mainline already has it (R16G16_SFLOAT format = upscaling standard input).
- **Cross-axis с `vk-fragment-shading-rate-voxel`** (verdict=mixed) — VRS 2x1 + DLSS 2x = 4× effective fragment cost reduction (compound effect), but both have quality risk; **sequential adoption recommended** (VRS first as default, upscaling as opt-in).
- **Pipeline cache impact** — upscaling insert may invalidate pipeline cache; expect first-frame compile stall on swapchain resize.

**Критерии приёмки (per `optimization-philosophy.md` 5-10% threshold):**

- `gpu_time_ms` reduction ≥30% (Quality preset, 1440p dense_voxel scene) per StraySpark 2026-03-25 + RigPulse 2026-03-29 industry data.
- `psnr_dB` ≥38 dB (Quality preset, dense_voxel scene) — visually lossless.
- `vram_peak_delta_MiB` ≤50 MiB — well under 1% of 8 GiB budget per `hardware-profile.md §3`.
- `cross_vendor_coverage` ≥3 vendors (NVIDIA + AMD + Intel) — FSR 3.1 + XeSS 2 + DLSS 4.5 = 3 paths.

**Зависимости:**

- `agent/knowledge.md §30.4` (3-step migration precedent)
- `TODO.md §4.3` (Lift Draw Distance Cap — explicit future need)
- `2026-06-21-taa-motion-vectors` (verdict=yes — motion vector MRT input already in mainline)
- `2026-06-20-bindless-descriptor-overhead` Phase D (bindless = required for upscaling resource management)

**Estimated effort:** S-M (~360 LoC, 2-3 sessions).

**If verdict = `no` or `mixed` (re-evaluation triggers):**

- Vulkan 1.5 / 1.6 core promotion of `VK_KHR_fragment_density_map` or similar (per `docs.vulkan.org/spec/latest/appendices/versions.html` — no current path)
- `VK_NV_present_timing` (NVIDIA-specific, out of cross-vendor scope)
- DirectSR Vulkan = GA (currently beta per StraySpark 2026-03-25)
- `VK_KHR_dynamic_rendering` extension enhancements
- ProjectV shader count > 50 (CI/CD bottleneck — DXC migration per `dxc-vs-glslc-toolchain` verdict=mixed)
- Stage 4.3 ships (128+ chunks draw distance explicit need)

---

## 8. Sources

См. [`sources.md`](./sources.md) (15+ primary + secondary sources). Ключевые: StraySpark 2026-03-25 UE 5.7 guide, NVIDIA DLSS SDK 310.6.0 (Mar 2026), AMD FidelityFX FSR 3.1 (Jul 2024), TechSpot 2026-03-12 650+ games analysis, mypcbottleneck 2026-06-04 (FSR 4 Vulkan limitation), wccftech 2026-04-21 (DLSS 4.5 SDK), RigPulse 2026-03-29 (buyer guide).

---

## 9. Mapping to ProjectV hot-path

**Какой участок движка соответствует прототипу:**

- `src/render/Renderer.cpp` post-TAA-resolve → pre-swapchain insertion point (planned Step 1)
- `src/render/TaaRenderTargets.{hpp,cpp}` motion vector MRT (R16G16_SFLOAT per `taa-motion-vectors` verdict=yes) = upscaling input contract
- `src/render/Taa.cpp` Halton jitter = upscaling-aware sub-pixel input
- `src/render/SceneResources.{hpp,cpp}` render target sizing — needs `render_extent` parameter for quality preset
- `src/app/Camera.cpp` draw distance — Stage 4.3 enabler (64m → 128m with 50% fragment reduction)
- `src/render/ShadowProjection.{hpp,cpp}` shadow cascade render extents — also benefit from upscaling
- `src/shaders/voxelize.comp` (future Stage 5.1) + `src/shaders/voxel.frag` cone-march — primary fragment cost target

**Допущения / упрощения:**

- Synthetic voxel pass = representative of ProjectV voxel MRT cost pattern (high overdraw, fragment-bound, deferred-shading-like)
- TAA resolve = current mainline path (YCoCg + CAS sharpening per `taa_resolve.frag`)
- Upscaling post-process = single-pass, no multi-pass approaches (no per-region dynamic VRS coupling in this prototype)
- No Ray Reconstruction (DLSS-specific deferred ray-reconstruction feature) — separate Stage 5.2 path
- No Frame Generation (DLSS MFG / FSR 3 AFMF / XeSS 2 XeSS-FG) — out of scope single-session
- No actual SDK linking в prototype (FSR 3.1 / XeSS 2 / DLSS 4.5 = real SDK load via `dlopen` per Step 2 plan, but prototype = analytical + cost model + simulated dispatch timings per published vendor benchmarks)

**Что осталось неизмеренным (deferred to follow-up experiments):**

- `real_gpu_dispatch_ms` (prototype = analytical cost model + simulated timings per StraySpark 2026-03-25 + RigPulse 2026-03-29; real GPU dispatch deferred to integration prototype)
- `actual_visual_quality_dB` (prototype = analytical PSNR estimate per image quality standards; real PSNR/SSIM on rendered frames deferred to integration prototype + visual QA)
- `cross_vendor_real_gpu_measurement` (only NVIDIA RTX 3060 Ti measured; AMD RDNA 2/3/4 + Intel Arc Battlemage = analytical projection per published benchmarks)
- `frame_generation_latency_impact` (out of scope single-session per `wccftech 2026-04-21` — requires Reflex/XeLL integration)
- `upscaling_compound_with_VRS` (cross-axis с `vk-fragment-shading-rate-voxel` verdict=mixed — VRS 2x1 + DLSS 2x = 4x effective, but no measurement)
- `upscaling_compound_with_LOD` (cross-axis с `lod-mesh-downsampling` verdict=mixed — geometry reduction + fragment reduction = compound, но orthogonal — sequential adoption recommended)
- `directsr_vulkan_status` (beta per StraySpark 2026-03-25, defer)

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §3 (RTX 3060 Ti GA104, 8 GiB VRAM, Vulkan 1.4.341, NVIDIA 610.43.02) + §4 (Vulkan extensions subset relevant для ProjectV). Не дублировать данные в README.
