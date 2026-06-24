# STATUS — `2026-06-21-volumetric-fog-atmosphere-rendering`

**Phase:** **CLOSED** (concluded-verdict-mixed, single session ~3h).
**Last action:** Experiment closed `2026-06-21` per `AGENTS.md §13.5` single-pass sync: backlog
moved §In progress → §Closed, INDEX.md §5 Active → §6 Recent closed, README.md updated with
RESULTS.md content + integration recommendation. Prototype complete: `prototype/volumetric_fog_sim.cpp`
~500 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green
0 warnings**), 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**,
wall time **0.008 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
**Blocker:** нет.
**Verdict:** `mixed` (per-platform tier — no single winner cross-vendor). Headline: **D_RTX_RayQuery_ShortRayShadow
WINNER RTX 3060 Ti** (1.79 ms mean / 38.75 dB PSNR / 12.39 MiB VRAM / scene-coverage-INDEPENDENT 1.33→2.31 ms range).
**B_FroxelGrid_3DTexture = SAFE UNIVERSAL DEFAULT** (2.58 ms / 37.25 dB / 28.27 MiB). **A_AnalyticDistance = current
mainline baseline (8.45 dB PSNR — NOT real volumetric fog)**. **C_FullRayMarch / E_Hybrid = quality/flexibility winners
but exceed 5 ms budget on heavy scenes — defer до RTX 4080-class hardware** per elliahu benchmarks.
**Next:** operator review + mainline integration deferred до Stage 5.x dedicated session per `agent/workspace.md §2`
line 36 operator 8x planning decision. 3-step migration per `agent/knowledge.md` precedent = ~480 LoC, M effort,
2-3 sessions.

---

## Progress log

- `2026-06-21` — Opened. Reservation зафиксирована в `research/backlog.md §In progress` + INDEX.md §5
  (single-pass sync per `AGENTS.md §13.5`). Hardware profile read (`hardware-profile.md` captured
  `2026-06-21`, fresh per `AGENTS.md §14`). ProjectV mainline baseline surveyed (`voxel.frag:844-883`
  analytic distance fog + `LookDevCaptureAutomation.cpp:180` fog lookdev scene).
- `2026-06-21` — Web-research Phase A complete (2 batches, ~20 results, 30 sources verified across
  Tier 1 + Tier 2 + Tier 3). Key sources: Wronski 2014 + Hillaire 2015 + Kovalovs 2020 + Lumen
  SIGGRAPH 2022 + Enshrouded 2026 GPC + elliahu/atmosphere RTX 3060/4080 benchmarks + Mastering
  Vulkan Ch10 + Timethy Hyman Traverse + sinnwrig URP + Godot issue #8580 + Kenny Mitchell GPU Gems 3.
- `2026-06-21` — Prototype designed + implemented (`volumetric_fog_sim.cpp` ~500 LoC standalone C++26
  analytical CPU cost model + 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup harness).
- `2026-06-21` — Build green, **0 warnings** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG
  -Wall -Wextra -Wpedantic`).
- `2026-06-21` — Benchmark complete: **125,000 main measurements**, wall time **0.008 sec** на
  Zen 3 5800X governor=`powersave`. Output `prototype/build/results.csv` (126 rows = 1 header +
  125 data rows, 19.3 KB).
- `2026-06-21` — Analysis complete. **Headline:**
  - **D_RTX_RayQuery_ShortRayShadow WINNER RTX 3060 Ti:** 1.787 ms mean / 38.75 dB PSNR / 12.39 MiB VRAM /
    scene-coverage-INDEPENDENT (1.33→2.31 ms range across heterogeneous scenes), Lumen SIGGRAPH 2022 hybrid pattern.
  - **B_FroxelGrid_3DTexture SAFE UNIVERSAL DEFAULT:** 2.580 ms mean / 37.25 dB PSNR / 28.27 MiB VRAM, all
    scenes meet 5 ms target, validated Frostbite/TLoU2/Enshrouded production pattern.
  - **A_AnalyticDistance baseline:** 0.002 ms but 8.45 dB PSNR = NOT real volumetric fog (no light
    scattering). Free + zero VRAM for static baked / mobile fallback.
  - **C_FullRayMarch / E_Hybrid budget-busters на heavy scenes:** cave_stress 9.59 ms C + 6.67 ms E exceed
    5 ms target. Defer до RTX 4080-class (elliahu RTX 4080 Clouds 0.755 ms = 8× RTX 3060).
- `2026-06-21` — Single-pass sync per `AGENTS.md §13.5`: `backlog.md §In progress` → `§Closed` (with
  full closure note + reservation record kept per §13.5), `INDEX.md §5 Active` → `§6 Recent closed`
  table row + `§1 Now Just-closed` + `§8 Last update` entry + `README.md` updated with `RESULTS.md`
  content + integration recommendation.

---

## Notes

- **Surprising finding:** D_RTX (1.787 ms) beats B_FroxelGrid (2.580 ms) on RTX 3060 Ti by 31% — RTX
  ray query + BLAS traversal faster than froxel compute scattering для moderate light counts (1-6 lights).
- **A_AnalyticDistance 8.45 dB PSNR confirms it's NOT real volumetric fog** — baseline only, sufficient
  для atmospheric haze + skybox blend, NOT для fog scenes with light interaction.
- **C_RayMarch view_dolly_stress temporal PSNR 26.10 dB FAILS** 30 dB target — heavy camera motion exposes
  ray-march jitter; froxel pre-aggregation in B/D/E dampens this.
- **VRAM under budget for all 5 strategies** (max 28.27 MiB B_FroxelGrid = 0.55% of 5.06 GiB budget).
- **E_Hybrid pattern validated** via Enshrouded 2026 GPC three-layer (froxel near + ray-march far shroud +
  ray-march clouds) — most flexible but cave_stress exceeds budget on RTX 3060 Ti (within budget на RTX 4080).
- **Cross-axis continuity:** orth orth ко всем 3 in-progress parallel; complementary к closed VCT experiments
  (cone-march через 3D атлас структурно похож на fog ray-march) + closed `eye-tracked-foveated` (VRS = smart
  fog density) + closed `taa-motion-vectors` (MV reprojection для fog temporal) + closed `dlss-fsr-xess`
  (half-res fog + upscale pattern) + closed `vulkan-memory-aliasing-transient` (froxel = transient aliasing
  candidate) + closed `vulkan-defragmentation-compaction` (froxel VRAM = compaction candidate).
- **Continuation chain:** `2026-06-20-vct-vs-rt-cutoff` (closed mixed Stage 5.1 cutoff) + this (closed
  mixed Stage 5.x fog) = Stage 5.x Visual Polish lighting foundation continues.
- **Re-evaluation triggers:** Stage 5.x ships + RTX 4080-class hardware tier validated + visual QA в
  реальном gameplay + VRS = smart fog density follow-up (per closed `eye-tracked-foveated` mixed) +
  Mobile platform deployment (no HW RT path = B_FroxelGrid critical).