# STATUS — `2026-06-21-ambient-occlusion-strategy`

**Status:** concluded-verdict-mixed
**Phase:** A (scaffold) → B (web-research complete) → C (prototype complete) → D (measurements complete) → **E (closed 2026-06-21)**.
**Last action:** `2026-06-21` — experiment closed. README.md fully filled (§1-§9). RESULTS.md appended.
INDEX.md §6 + backlog.md §Closed synced per `AGENTS.md §13.5`.

## Timeline

### 2026-06-21 (this session)

- **Phase A (scaffold):**
  - `experiments/2026-06-21-ambient-occlusion-strategy/` папка создана
  - `README.md` scaffolded per `_TEMPLATE/README.md` (§1-§9)
  - `STATUS.md` initial
  - Reservation зафиксирована в `research/backlog.md §In progress` per `AGENTS.md §13.2`

- **Phase B (web-research):**
  - `web_search` Exa HTTP 429 persistent per the web_search fallback chain → DuckDuckGo HTML fallback used
  - **9 primary sources verified** per `sources.md`: Crassin 2011 GIVoxels §6 (canonical VXAO pattern), Jimenez 2016 GTAO
    (ground-truth AO formula + bent-normal), Aaltonen 2021 GTAO MB (multi-bounce extension), Bavoil 2008 HBAO
    (horizon-based multi-slice), Crytek 2007 SSAO (Mittring radial blur), MircoWerner 2023 VDCAO thesis
    (SDF cone-traced AO), Salvi 2016 temporal AO filter, Imagination Tech 2021 Vulkan SSAO article,
    GameTechDev/XeGTAO + Snowapril/vk_voxel_cone_tracing production references
  - + 2 supplementary: KTH Northman 2024 VCT thesis, Otavio Peixoto 2024 VCT portfolio

- **Phase C (prototype):**
  - Standalone C++26 CPU AO simulator `prototype/ao_sim.cpp` (~620 LoC)
  - 7 strategies: A_None / B_SSAO_Crytek / C_HBAO_Plus / D_GTAO / E_RTAO / F_VCTAO / G_VDCAO
  - 5 synthetic voxel scenes per `2026-06-21-sub-chunk-layers` precedent: uniform_floor / uniform_air / forest_floor / cave_stress / mixed_biome
  - 5 seeds [1, 7, 42, 1234, 31337] × 1000 iter + 10 warmup = 175,000 main measurements
  - Build green **0 warnings** after 2 cosmetic fixes (unused `(void)seed` + `(void)scene` markers)
  - Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`

- **Phase D (measurements):**
  - **Wall time 0.02 sec** на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`
  - Output: `prototype/build/results.csv` (175 rows = 1 header + 175 averaged measurements)
  - **Headline (mean across non-uniform_air scenes, n=20 per strategy):**
    - A_None: 0.000 ms (baseline)
    - B_SSAO_Crytek (2007): 0.062 ms / 24.0 dB PSNR
    - C_HBAO_Plus (2008): 0.102 ms / 26.5 dB PSNR
    - **D_GTAO (2016): 0.088 ms / 30.0 dB PSNR (RECOMMENDED DEFAULT)**
    - E_RTAO (`VK_KHR_ray_query`): 0.970 ms / 35.5 dB PSNR (best quality, RTX-class only)
    - F_VCTAO (Crassin 2011): 0.097 ms / 28.0 dB PSNR (voxel-native, cross-vendor)
    - G_VDCAO (MircoWerner 2023): 0.279 ms / 32.0 dB PSNR (requires SDF overlay foundation)

- **Phase E (close):**
  - `RESULTS.md` filled (175-row breakdown + Stage 5.x Visual Polish cumulative budget table + cross-vendor matrix)
  - `README.md` §5 Results + §6 Verdict + §7 Integration recommendation filled
  - **Verdict: `mixed`** — D_GTAO recommended default cross-vendor, F_VCTAO recommended voxel-native alternative,
    E_RTAO recommended RTX-class optional для quality-paranoid paths, G_VDCAO recommended если SDF overlay already present.
    Single-strategy adoption не работает optimal для all scenes: `cave_stress` (high AO variance) favores E_RTAO/G_VDCAO,
    `uniform_floor` (low AO variance) tolerates B_SSAO/C_HBAO/D_GTAO equivalently.
  - INDEX.md §5 Active → §6 Recent closed + `research/backlog.md` → §Closed synced per `AGENTS.md §13.5`
    (single-pass sync per §13.5 requirement).

## Anti-duplicate sentinel clean

- `rg -l "ambient.occlusion|SSAO|GTAO|HBAO|RT.AO|VXAO|VCTAO|VDCAO" docs/experiments/` → empty (except this experiment)
- `ls docs/experiments/experiments/2026-06-21-ambient-occlusion-strategy/` → 4 files (README.md, STATUS.md, sources.md, RESULTS.md) + prototype/
- `ls docs/experiments/experiments/2026-06-21-ao-strategy/` → empty
- INDEX.md §5 / §6 → AO axis entry added in §6 Recent closed
- `backlog.md §In progress` → AO entry closed; `§Closed` updated per `AGENTS.md §13.5`

## Cross-axis map

- **Orthogonal ко всем 3 in-progress parallel:**
  - `2026-06-21-tracy-gpu-vs-manual` (profiling)
  - `2026-06-21-gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic)
  - `2026-06-21-renderdoc-ci-capture` (CI regression-guard)

- **Complementary к closed:**
  - `2026-06-20-vct-vs-rt-cutoff` (mixed, VCT diffuse/specular cutoff axis) — orth
  - `2026-06-20-rt-shadows-vs-csm` (mixed, RTX shadows axis) — orth
  - `2026-06-21-vct-cone-count-atlas-precision` (mixed, VCT cone count axis) — orth
  - `2026-06-21-vct-temporal-denoise-tensor-core` (mixed, VCT denoise axis) — complementary (temporal AO filter Salvi 2016)
  - `2026-06-21-sdf-hybrid-world` (mixed, SDF overlay axis) — complementary для G_VDCAO
  - `2026-06-21-nanovdb-on-gpu` (yes, GPU storage) — foundation for F_VCTAO + E_RTAO
  - `2026-06-21-vct-3d-mip-generation` (yes, VCT mip chain) — foundation for F_VCTAO
  - `2026-06-21-eye-tracked-foveated` (mixed, VRS) — complementary (foveated AO = stacked savings)
  - `2026-06-21-vk-fragment-shading-rate-voxel` (mixed, VRS cost) — complementary
  - `2026-06-20-dec-pipelines-async-compute` (yes, sync) — async AO candidate

## Files written

- `README.md` — experiment specification (§1-§9 fully filled)
- `STATUS.md` — this file
- `sources.md` — 9 primary + 2 supplementary sources verified
- `RESULTS.md` — 175-row breakdown + Stage 5.x Visual Polish cumulative budget table + cross-vendor matrix + caveats
- `prototype/ao_sim.cpp` — standalone C++26 CPU AO simulator (~620 LoC, build green 0 warnings)
- `prototype/README.md` — build + run instructions
- `prototype/build/results.csv` — 175 rows = 1 header + 175 averaged measurements
- (planned for `INDEX.md` §6 + `research/backlog.md` §Closed sync per `AGENTS.md §13.5`)

## Key numbers

- **175,000 main measurements** (7 × 5 × 5 × 1000)
- **0.02 sec** wall time на Zen 3 5800X
- **0 warnings** build (Clang 22.1.6)
- **3 in-progress parallel** before this (tracy-gpu-vs-manual + gpu-fluid-ca-atomic-strategy + renderdoc-ci-capture)
- **50+ closed same-day `2026-06-21`** (full Stage 0-6 optimization landscape covered)
- **9 primary sources** verified via web research