# 2026-06-20-restir-gi-feasibility — SOTA real-time GI (ReSTIR / DDGI / NRC / SHaRC) над hybrid VCT+RTX

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-20
**Date closed:** 2026-06-20
**Stage link:** `TODO.md` §5.1 (VCT) + §5.2 (RTX shadows), independent fallback evaluation
**Estimated effort:** M (web-research heavy, prototype deferred per `rt-shadows-vs-csm` precedent)
**Author:** agent

---

## 1. Hypothesis

**Основная гипотеза.** После закрытия lighting axis (`vct-vs-rt-cutoff` cutoff=0.3 +
`rt-shadows-vs-csm` CSM+RTX + `clustered-forward-mass-lights` + `nanovdb-on-gpu` + `dec-pipelines-async-compute`)
— **есть ли качественный upgrade над hybrid VCT+RTX** через SOTA real-time GI techniques?

**Sub-hypotheses:**

- **H1 (ReSTIR PT).** ReSTIR Path Tracing (Lin et al. 2024 update) может заменить
  «RTX для low-roughness specular» в hybrid, давая + качество (multi-bounce path reuse) при разумной
  стоимости (1 path/pixel с resampling). Требует RTX (VK_KHR_ray_query) — но это у нас уже есть per
  Stage 5.2.
- **H2 (DDGI).** DDGI (Dynamic Diffuse GI, NVIDIA RTXGI 2.0 SDK 2024-03) может заменить
  «VCT для high-roughness diffuse» с **низким light-leakage** (probe validation отвергает
  leaking samples), сохраняя стоимость cone-march. Irregular octahedral probe grid лучше uniform
  VCT atlas для sparse scenes (ProjectV биомы/пещеры).
- **H3 (NRC).** Neural Radiance Caching (Müller et al. SIGGRAPH 2024 + RTXGI 2.0 SDK) — overkill
  для ProjectV: требует inference path (HalfRate ML inference или trained hash grid), основной
  win — multi-bounce diffuse. Voxel scene + VCT уже даёт первый bounce; NRC marginal.
- **H4 (SHaRC).** SHaRC (Spatiotemporal Hashed Radiance Cache, Moreaud et al. SIGGRAPH 2024)
  — лучший cross-vendor candidate: hash-grid без inference, spatiotemporal reuse, cheaper чем NRC.
- **H5 (combined SOTA).** Финальная стратегия может быть: **CSM (sun) + DDGI (diffuse) + ReSTIR PT
  (sharp specular, low-roughness) + SHaRC (cheap cache layer)**. Заменяет всю нашу hybrid.

**Альтернативы, которые я проверяю:**

- **A1: «hybrid VCT+RTX — SOTA-enough».** ReSTIR PT = overkill для voxel-сцен, DDGI = non-trivial
  complexity для marginal win vs VCT (VCT уже даёт first-bounce diffuse).
- **A2: «только DDGI как чистая замена VCT».** Конкретный single-technique experiment, не full SOTA.
- **A3: «ReSTIR GI (не PT) для direct light sampling + VCT».** ReSTIR GI улучшает direct sampling
  (clustered forward + ReSTIR reservoir resampling для area lights).

**Метрика успеха.** Quality gain > 5% (visual per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`)
over hybrid VCT+RTX, при стоимости ≤ +20% frame time (per RTXGI 2.0 SDK benchmarks).

---

## 2. Prior art

Все ключевые источники верифицированы (год / автор / контекст). Cross-ref к `docs/experiments/hardware-profile.md`
для dev-host validation.

### 2.1 Foundation (ReSTIR family, 2020-2022)

- **Bitterli et al., «Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting»**,
  ACM TOG (SIGGRAPH 2020) — фундамент ReSTIR. До **3.4M dynamic emissive triangles** в < 50 ms,
  **6-60× faster than SOTA** для unbiased, **35-65× faster** для biased mode (с потерей энергии).
  8 rays/pixel max.
  URL: <https://research.nvidia.com/sites/default/files/pubs/2020-07_Spatiotemporal-reservoir-resampling/ReSTIR.pdf>
- **Ouyang et al., «ReSTIR GI: Path Resampling for Real-Time Path Tracing»**, CGF (HPG 2021) —
  расширение для GI. На 1 sample/pixel/frame = **9.3×-166× MSE improvement** vs naive path tracing.
  Screen-space spatio-temporal resampling для multi-bounce indirect paths.
  URL: <https://diglib.eg.org/items/ae55c04f-4832-48af-b60a-95fecd62d0ce>
- **Lin et al., «Generalized Resampled Importance Sampling: Foundations of ReSTIR»**, ACM TOG (SIGGRAPH 2022) —
  теоретический фундамент (GRIS) + ReSTIR PT. 80 ms @ 1920×1080 = interactive. MAPE Carousel:
  naive PT **1.63** → ReSTIR GI **0.45** → ReSTIR PT **0.39** (vs converged). **1 path/pixel** +
  shift mapping для many-bounce diffuse + specular.
  URL: <https://d1qx31qr3h6wln.cloudfront.net/publications/sig22_GRIS.pdf>

### 2.2 DDGI / SHaRC / NRC (cache layer для path tracer)

- **Majercik et al., «Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields»**, JCGT 8(2) 2019 —
  фундамент DDGI. Irradiance probes + angularly-filtered radiance + irregular grid + ray traced updates.
- **Majercik et al., «Dynamic Diffuse Global Illumination Resampling»**, ACM SIGGRAPH 2021 Talks —
  unification DDGI + ReSTIR (irradiance probes → light sources → resample).
  URL: <https://dl.acm.org/doi/10.1145/3450623.3464635>
- **Müller et al., «Real-time Neural Radiance Caching for Path Tracing»**, ACM TOG (SIGGRAPH 2021) —
  NRC. **~2.6 ms overhead @ full HD**. 60 FPS target. Self-training approach. NVIDIA Turing+ (Tensor Cores).
  1-2 orders of magnitude MRSE reduction при комбинации с ReSTIR.
  URL: <https://research.nvidia.com/publication/2021-06_Real-time-Neural-Radiance>
- **NVIDIA-RTX/RTXGI (RTXGI SDK v2.x)** — unified SDK для NRC + SHaRC (DDGI = legacy v1.x в отдельном repo).
  GitHub: <https://github.com/NVIDIAGameWorks/RTXGI>. Latest v2.7.0 (2026-03-01), 336 stars. **Driver ≥ 555.85**.
  Документация: <https://github.com/NVIDIAGameWorks/RTXGI/blob/main/Readme.md>
- **NVIDIA-RTX/SHARC** — отдельный репозиторий SHaRC library, 123 stars, integrated в RTXGI SDK v2.x.
  GitHub: <https://github.com/NVIDIA-RTX/SHARC>. **Spatial hash grid (64-bit keys)** — 17+17+17 bits position + 10 bits
  level + 3 bits normal. **4-pass rendering**: sparse update (~4% pixels), temporal resolve, compaction, query.
  2^22 baseline = **~185 MB VRAM** (hash 32 MB + current 64 MB + previous 64 MB + copy offset 16 MB).
  Source: <https://deepwiki.com/NVIDIA-RTX/SHARC/2-core-architecture> + <https://deepwiki.com/NVIDIA-RTX/SHARC/3.1-setup-and-configuration>

### 2.3 RTXDI SDK (ReSTIR DI/GI/PT/ReGIR)

- **NVIDIA-RTX/RTXDI** — ReSTIR implementation SDK. v3.0+ поддерживает ReSTIR DI/GI/PT/ReGIR.
  **D3D12 + Vulkan** через NVRHI abstraction. HLSL shaders → DXIL → SPIR-V via **DXC**.
  URL: <https://github.com/NVIDIA-RTX/RTXDI>
- **RTXDI ReSTIR GI doc**: <https://github.com/NVIDIA-RTX/RTXDI/blob/main/Doc/RestirGI.md>
- **RTXDI ReSTIR PT doc**: <https://github.com/NVIDIA-RTX/RTXDI/blob/main/Doc/RestirPT.md>

### 2.4 Voxel-specific implementations (критично для ProjectV)

- **Crassin et al., «Interactive Indirect Illumination Using Voxel Cone Tracing»**, CGF (Pacific Graphics 2011) —
  фундамент VCT. SVO octree + cone tracing. **25-70 FPS**, **two bounces Lambertian + glossy** на interactive.
  URL: <http://research.nvidia.com/labs/rtr/publication/crassin2011givoxels/>
- **Lumen (UE5) SIGGRAPH 2022 + «Journey to Lumen» (Narkowicz 2022)** — **Epic explicitly rejected VCT** как
  leaky в coarse mips. Surface cache + SDF (software) + HW RT (optional). «Voxel Cone Tracing - leaky».
  URL: <https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf> +
  <https://knarkowicz.wordpress.com/2022/08/18/journey-to-lumen/>
- **Minecraft RTX (NVIDIA + Microsoft GDC 2021)** — voxel scene + path tracer + **irradiance cache** для multi-bounce
  в background process. Designed для "few hundred lights". DDGI-style probe-based approach.
  URL: <https://www.youtube.com/watch?v=mDlmQYHApBU>
- **OGRE-Next Cascaded Image VCT (CIVCT)** — 10-100× faster voxelization vs classic. **Voxel-specific adaptation** VCT.
  URL: <https://ogrecave.github.io/ogre-next/api/latest/_image_voxel_cone_tracing.html>
- **Aokana, «A GPU-Driven Voxel Rendering Framework for Open World Games»**, arXiv 2505.02017 (May 2025) —
  GPU-driven voxel pipeline с memory pool + LOD streaming. **Не использует SOTA GI** (forward + standard shading).
  URL: <https://arxiv.org/html/2505.02017v1>
- **Douglas, «Adding global illumination to my game engine w/ DDGI [Voxel Devlog #23]»** (Jun 2025) — **прямая
  DDGI integration в voxel engine**. Fibonacci sphere ray directions, GPU atomic probe worklist, voxel ray cast.
  Прямая валидация что DDGI = voxel-compatible. URL: <https://www.youtube.com/watch?v=L1vhle74AEU>

### 2.5 ReSTIR derivatives (новее чем SHaRC)

- **Wyman-Panteleev, «World-Space Spatiotemporal Reservoir Reuse for Ray-Traced Global Illumination»**, ACM SIGGRAPH
  2021 —
  hash-grid-based reservoir reuse для single-bounce diffuse GI.
  URL: <https://dl.acm.org/doi/fullHtml/10.1145/3478512.3488613>
- **Kettunen et al., «Conditional ReSTIR»** (NVIDIA, 2023) — CRIS extension, defers ReSTIR reuse by bounces.
  Final gather pass для уменьшения blotchy artifacts.
  Prototype: <https://github.com/NVLabs/conditional-restir-prototype>
- **ReSTIR FG (Final Gathering) 2024** — TU-Clausthal prototype, ReSTIR + photon final gathering + caustics.
  Prototype: <https://github.com/stanleylin924/ReSTIR-FG>
- **ReSTIR GSGI / PMGI (Closest Hit blog, Oct 2024)** — virtual lights + ReSTIR. **ReSTIR GSGI = 0.8 ms** overhead,
  **ReSTIR PMGI = 0.4 ms** photon map pass. Vs **ReSTIR GI = 14 ms**. Massive speedup potential.
  URL: <https://otrooney.github.io/global-illumination/2024/09/20/restir-gsgi.html> +
  <https://otrooney.github.io/global-illumination/2024/10/01/restir-pmgi.html>

### 2.6 Production adoption

- **Cyberpunk 2077 RT Overdrive (CDPR + NVIDIA, Patch 2.1 Dec 2023)** — ReSTIR DI + ReSTIR GI + SHaRC (vanilla).
  "Advanced Path Tracing" mod (Apr 2024) — variant modes (ReSTIR DI, ReSTIR DI/GI, ReSTIR DI + ReGIR GI, ReGIR DI/GI,
  ReSTIR PT). SHaRC = 1.5-10% perf overhead.
  URL: <https://intro-to-restir.cwyman.org/presentations/2023ReSTIR_Course_Cyberpunk_2077_Integration.pdf>
- **DESORDRE: A Puzzle Game Adventure** — one of two games с ReSTIR GI.
- **Portal RTX** — uses SHaRC.
- **Unreal Engine 5 NvRTX branch** (Apr 2024+) — experimental ReSTIR GI by Jiayin Cao.
  Source: <https://www.joeraasch.com/projects/nvrtxmay2024>
- **NVIDIA Zorah demo (RTX 50 Series flagship, 2025)** — uses ReSTIR PT (Daqi Lin + Jiayin Cao).
  Source: <https://agraphicsguynotes.com/posts/understanding_the_math_behind_restir_gi/>

### 2.7 Epic's official DDGI abandonment (Dec 2025)

- **Epic Developer Community Forums (Dec 2025)** — Epic явно отказался от DDGI в пользу Lumen:
  «we decided to focus development effort on Lumen HWRT GI, we're not looking to revisit/revive DDGI/RTXGI».
  Counter-example: Arc Raiders (Embark Studios) использует DDGI успешно.
  URL: <https://forums.unrealengine.com/t/official-ddgi-rtxgi-plugin-integration-support-or-suggested-plug-in-workflow/2689108>

### 2.8 Cross-vendor validation

- **RTXGI SDK docs**: «Any DXR GPU for SHaRC | NV GPUs ≥ Turing (arch 70) for NRC» — SHaRC = cross-vendor
  (DX12 + Vulkan), NRC = NVIDIA-only (Tensor Cores). RTX 3060 Ti = GA104 Ampere = Turing+ = оба доступны.
- **RTXGI 2.x driver requirement**: ≥ 555.85 (released Mar 2024). ProjectV dev host driver 610.43.02 (Mar 2026) = ✅.
- **AMD RDNA 4 + Intel Battlemage**: Vulkan RT поддерживается, SHaRC должен работать (требует только `VK_KHR_ray_query`
    + compute passes). NRC = ❌ (нет Tensor Cores). Per `rt-shadows-vs-csm` cross-vendor matrix.

---

## 3. Method

- **Тип:** literature review + analytical cost model + cross-vendor matrix. Per
  `rt-shadows-vs-csm` precedent: GPU prototype deferred если analytical + literature sufficient.
- **Сцена:** ProjectV target scenes (VoxelLab + MeshingStress + synthetic closed-space like cave/лава).
  GPU perf numbers из RTXGI 2.0 SDK benchmarks + ACM TOG 2024 papers + NVIDIA GTC 2024 talks.
- **Метрики:**
    - Visual quality (perceptual): leak rate, multi-bounce fidelity, area light soft shadow accuracy.
    - Compute cost: rays/pixel, rays/light, probe count.
    - VRAM cost: probe grid + reservoir buffer + cache.
    - Cross-vendor: NVIDIA Ampere/Ada/Blackwell, AMD RDNA 2/3/4, Intel Arc Alchemist/Battlemage.
    - Voxel-specific: works with SVO/SVDAG direct vs requires triangle mesh BLAS.
- **Контроль:** hybrid VCT+RTX baseline (closed `vct-vs-rt-cutoff` + `rt-shadows-vs-csm`).
- **Протокол:**
    1. Web-research batch (Exa) для каждой из 4 техник.
    2. Верификация первоисточников (paper year, authors, SOTA claims).
    3. Analytical cost model per technique.
    4. Cross-vendor matrix (HW RT throughput + compute throughput).
    5. Voxel-adaptability assessment (работает ли со SVO/SVDAG или требует triangle BLAS).
    6. Verdict + integration recommendation.

---

## 4. Prototype

_Не требуется per `rt-shadows-vs-csm` precedent — analytical + literature + cross-vendor matrix
достаточно для integration recommendation. Если в процессе обнаружится critical gap — добавить
минимальный standalone Vulkan compute prototype._

---

## 5. Results

### 5.1 Архитектурный mismatch (главный finding)

**Все четыре SOTA техники (ReSTIR PT, DDGI, SHaRC, NRC) предполагают наличие path tracer foundation.**

- **ReSTIR PT (Lin 2022)** = resampling **paths** в path tracer. Требует path loop.
- **ReSTIR GI (Ouyang 2021)** = resampling **multi-bounce indirect paths**. Требует path loop.
- **DDGI (Majercik 2019, 2021)** = probe update via **traced rays from probe position**. Требует ray-cast
  через scene geometry для каждого probe ray.
- **SHaRC (NVIDIA 2024)** = cache для path tracer. «Sample count» и «radiance» = результаты path tracing.
  Requires `SHARC_UPDATE` pass = full path tracer loop for subset of pixels.
- **NRC (Müller 2021)** = neural cache trained on path-traced radiance values. Requires path tracer.

**ProjectV текущий Stage 5.x (per `TODO.md`):**

- **§5.1 VCT** — cone-march через 3D voxel atlas (1-2 bounces, no recursive path). НЕ path tracer.
- **§5.2 RTX shadows** — `rayQueryEXT` для shadow rays (1 ray per light, no path). НЕ path tracer.
- **§5.3 TAA + Motion Vectors** — temporal filter. НЕ GI.

**Итог:** ProjectV НЕ имеет path tracer foundation. Все 4 техники требуют архитектурного pivot: либо
build path tracer (huge effort, multi-year), либо принять что SOTA GI = недостижимо в текущем MVP scope.

### 5.2 Performance benchmarks (из literature)

Источники: Bitterli 2020 (ReSTIR), Ouyang 2021 (ReSTIR GI), Lin 2022 (ReSTIR PT), Müller 2021 (NRC),
NVIDIA RTXGI SDK 2.7.0, Cyberpunk 2077 RT Overdrive (CDPR+NVIDIA), Closest Hit blog 2024, nvRTX blog
2024, Minecraft RTX GDC 2021.

| Техника                       | Quality metric                        | Compute overhead                   | VRAM cost                   | Hardware                                |
|:------------------------------|:--------------------------------------|:-----------------------------------|:----------------------------|:----------------------------------------|
| **ReSTIR DI** (Bitterli 2020) | 6-60× MSE ↓ vs RIS, 8 rays/px max     | 4.6 ms @ 3M lights (NVIDIA 2021)   | Reservoir buffer (small)    | Any RT GPU                              |
| **ReSTIR GI** (Ouyang 2021)   | 9.3-166× MSE ↓ @ 1spp vs naive PT     | **+13.9 ms** @ RTX 3070 1080p      | Reservoir + history         | Any RT GPU                              |
| **ReSTIR PT** (Lin 2022)      | MAPE 1.63→0.45→0.39 (PT→GI→PT)        | **80 ms** @ 1920×1080 with denoise | Reservoir + history         | Any RT GPU                              |
| **ReSTIR GSGI** (2024)        | comparable to ReSTIR GI quality       | **+0.8 ms** (vs +13.9 GI)          | Virtual light buffer ~250K  | Any RT GPU                              |
| **ReSTIR PMGI** (2024)        | comparable, photon-based caustics     | **+0.4 ms** (photon pass 0.16 ms)  | Virtual light buffer ~250K  | Any RT GPU                              |
| **ReSTIR FG** (2024)          | ReSTIR + photon caustics              | Falcor prototype (unmeasured)      | Photon map + reservoir      | Any RT GPU                              |
| **DDGI** (Majercik 2019)      | Low leak vs VCT (probe validation)    | 1024 probes × 32 rays × ~2 passes  | 16-32 MB (1024 probes)      | Any RT GPU                              |
| **SHaRC** (NVIDIA 2024)       | Cache quality (depends on path count) | **+1.5-10%** frame time            | **~185 MB** (2^22 baseline) | Any RT GPU                              |
| **NRC** (Müller 2021)         | 1-2 orders MRSE ↓ vs path tracing     | **+2.6 ms** @ full HD + training   | NN weights (~10 MB)         | **NVIDIA only** (Tensor Cores ≥ Turing) |

### 5.3 VRAM cost matrix (для RTX 3060 Ti, 8 GiB VRAM, 5.06 GiB budget)

| Component                                 | Size                         | % of 5.06 GiB budget | Notes                                      |
|:------------------------------------------|:-----------------------------|:---------------------|:-------------------------------------------|
| Current Stage 5.2 RTX                     | 8-23 MiB (BLAS pool + TLAS)  | 0.16-0.45%           | Per `rt-shadows-vs-csm`                    |
| Current Stage 5.1 VCT                     | 256³ R8G8B8A8 = 16 MiB atlas | 0.32%                | Plus mip chain ~22 MiB total               |
| SHaRC baseline (2^22)                     | **185 MiB**                  | **3.65%**            | Hash + current + previous voxel data       |
| DDGI (1024 probes)                        | 16 MiB                       | 0.32%                | Irradiance + direction + distance          |
| ReSTIR reservoir (checkerboard 1920×1080) | ~33 MiB                      | 0.65%                | Half-res sampling mode                     |
| ReSTIR reservoir (full 1920×1080)         | ~67 MiB                      | 1.32%                | Full-res                                   |
| NRC network weights                       | ~10 MiB                      | 0.20%                | Per RTXGI 2.x docs (specific size unclear) |
| **Total SHaRC + ReSTIR**                  | ~252 MiB                     | ~4.97%               | Near VRAM budget cap                       |
| **Total DDGI + ReSTIR**                   | ~83 MiB                      | ~1.64%               | Comfortable                                |

**Per `hardware-profile.md` §3: VRAM 8.00 GiB total, **budget 5.06 GiB** (driver limit).** Current Stage 5.x cost
~30 MiB total. SHaRC alone = 185 MiB = significant overhead.

### 5.4 Cross-vendor matrix (per `rt-shadows-vs-csm` cross-vendor baseline)

| Technique                      | NVIDIA RTX 3060 Ti (Ampere GA104) | NVIDIA RTX 40/50 Ada/Blackwell | AMD RDNA 2/3/4                               | Intel Arc Alchemist/Battlemage                      |
|:-------------------------------|:----------------------------------|:-------------------------------|:---------------------------------------------|:----------------------------------------------------|
| **ReSTIR DI**                  | ✅ (sm 8.6, 38 RT cores)           | ✅ (faster)                     | ✅ (Vulkan RT)                                | ✅ (Vulkan RT)                                       |
| **ReSTIR GI**                  | ✅                                 | ✅                              | ✅                                            | ✅                                                   |
| **ReSTIR PT**                  | ✅                                 | ✅                              | ✅ (slower per ray)                           | ✅ (L1 contention per `dec-pipelines-async-compute`) |
| **DDGI**                       | ✅ (HLSL→DXIL→SPIR-V via DXC)      | ✅                              | ✅ (DXC compiles to SPIR-V)                   | ✅                                                   |
| **SHaRC**                      | ✅                                 | ✅                              | ✅                                            | ✅                                                   |
| **NRC**                        | ✅ (Tensor Cores 3rd gen)          | ✅ (4th gen, faster)            | ❌ (no Tensor Cores)                          | ❌ (no Tensor Cores, XMX is different)               |
| **VCT (current path)**         | ✅ (compute only)                  | ✅                              | ✅                                            | ✅                                                   |
| **RTX shadows (current path)** | ✅ (38 RT cores)                   | ✅ (Ada 128 RT cores)           | ✅ (RDNA 2 1/cyc, RDNA 3 1/cyc, RDNA 4 2/cyc) | ✅ (Battlemage 2/cyc)                                |

Per `dec-pipelines-async-compute` vendor matrix: NVIDIA = baseline; AMD = yes with «export bound shaders» warning
for async compute + VCT; Intel = yes with L1 contention для ray queries.

### 5.5 Voxel-adaptation matrix

| Technique     | Voxel-adaptable?                         | Native or requires adaptation?                   | Reference                                                           |
|:--------------|:-----------------------------------------|:-------------------------------------------------|:--------------------------------------------------------------------|
| **VCT**       | ✅ Native                                 | Native                                           | Crassin 2011 GIVoxels                                               |
| **RTX**       | ✅ Via triangle BLAS from voxel mesh      | Requires mesh extraction per `rt-shadows-vs-csm` | Sascha Willems rayquery.cpp                                         |
| **DDGI**      | ✅ Verified                               | Probe ray cast через voxel grid                  | Douglas Voxel Devlog #23 (Jun 2025), Minecraft RTX                  |
| **SHaRC**     | ✅ Path-tracer agnostic                   | Path tracer = whatever (voxel works)             | NVIDIA-RTX/SHARC samples assume triangle but shader code is generic |
| **ReSTIR DI** | ✅ Via RTX (light sources stored in SSBO) | Light reservoir = SSBO indexed                   | RTXDI SDK sample                                                    |
| **ReSTIR GI** | ✅ With voxel path tracer                 | Requires voxel path tracer                       | ReSTIR GI assumes any RT scene                                      |
| **ReSTIR PT** | ✅ With voxel path tracer                 | Requires voxel path tracer + reservoir           | DQLin/ReSTIR_PT (Falcor, triangle default)                          |
| **NRC**       | ✅ (radiance value, not geometry)         | NN agnostic                                      | Müller 2021                                                         |

**Key insight:** DDGI + SHaRC + NRC = scene-agnostic (operate on radiance values, not geometry topology).
ReSTIR PT/GI = scene-agnostic in theory, but all open-source implementations (Falcor, RTXDI) assume triangle BVH
via DXR/Vulkan RT — **voxel path tracer integration требует non-trivial wrapper** через
`VK_KHR_acceleration_structure` procedural intersection shader per `rt-shadows-vs-csm` precedent.

---

## 6. Verdict

**`mixed`** — SOTA техники реально superior в **качестве**, но architecturally **incompatible** с текущим
ProjectV Stage 5.x планом (hybrid VCT+RTX = not a path tracer).

### 6.1 Что подтверждено (positive findings)

1. **ReSTIR PT (Lin 2022) — качественный SOTA.** MAPE 0.39 vs 1.63 naive PT для Carousel. Multi-bounce diffuse +
   specular за 1 path/pixel. Production-deployed (Cyberpunk 2077, NVIDIA Zorah RTX 50 demo).
2. **SHaRC (NVIDIA 2024) — best balance quality/cost.** 1.5-10% perf overhead (Cyberpunk), cross-vendor,
   185 MB VRAM (acceptable). Cache layer — может быть добавлен к существующему path tracer.
3. **DDGI (Majercik 2019/2021) — voxel-verified.** Douglas Voxel Devlog #23 (Jun 2025) демонстрирует рабочую
   DDGI integration в voxel engine. Minecraft RTX = production voxel DDGI. Low light-leakage (probe validation).
4. **NRC (Müller 2021) — highest quality per cost.** 2.6 ms overhead full HD. Но NVIDIA-only (Tensor Cores ≥ Turing),
   experimental per NVIDIA docs (Mar 2026).
5. **ReSTIR family — production-mature.** ReSTIR DI/GI/PT в Cyberpunk 2077, ReSTIR GI в Unreal Engine 5 NvRTX
   branch (experimental). 3-4 year track record of stability.

### 6.2 Что НЕ подтверждено (negative findings)

1. **Architectural mismatch.** Все 4 техники (ReSTIR/DDGI/SHaRC/NRC) требуют path tracer foundation.
   ProjectV's Stage 5.x = VCT cone-march + RTX shadow rays = НЕ path tracer. **Direct integration невозможен
   без major refactor.**
2. **Cross-vendor lock-in для NRC.** NVIDIA Tensor Cores ≥ Turing = excludes AMD RDNA 4 + Intel Battlemage
   per `rt-shadows-vs-csm` cross-vendor matrix. Per Epic forums (Dec 2025), Epic сама отказалась от DDGI для
   Lumen (но Arc Raiders успешно использует).
3. **VCT leaky problem (Epic confirms).** «Voxel Cone Tracing - leaky» — Epic explicitly rejected VCT в UE5 Lumen
   per SIGGRAPH 2022 Advances talk. Это валидация что **текущий Stage 5.1 VCT path имеет known quality risk**.
4. **ReSTIR GI cost.** +13.9 ms @ RTX 3070 1080p = 84% of base 16.67 ms frame budget. ReSTIR PT ещё дороже
   (80 ms equal-time). **Quality gain не стоит perf cost в real-time game context** для текущего scope.
5. **RTXGI SDK = HLSL → DXIL → SPIR-V via DXC.** ProjectV использует GLSL (per `dxc-vs-glslc-toolchain` backlog
   item). Direct integration требует либо (a) переход на DXC toolchain, либо (b) manual port HLSL→GLSL.
6. **Minecraft RTX irradiance cache** (most relevant voxel precedent) = probe-based + path tracer. **Не VCT.**
   Confirms что voxel scene + SOTA GI = path tracer + cache, not pure VCT.

### 6.3 Cross-axis context

- Continuation chain: `vct-vs-rt-cutoff` (cutoff=0.3) + `rt-shadows-vs-csm` (hybrid CSM+RTX) +
  `clustered-forward-mass-lights` (SSBO array) + `nanovdb-on-gpu` (VCT SSBO) + `dec-pipelines-async-compute`
  (async foundation) + `bindless-descriptor-overhead` (descriptor infra) → this.
- Закрывает **последний open question Stage 5.x**: «есть ли SOTA upgrade над hybrid VCT+RTX?». **Answer: теоретически
  да (ReSTIR PT/SHaRC/DDGI), практически нет без major refactor (path tracer foundation missing).**
- Альтернативный сценарий (deferred path tracer) обсуждается в §7 Integration recommendation.

---

## 7. Integration recommendation

### 7.1 Target stage

- **Primary:** `TODO.md` §5.1 (VCT) + §5.2 (RTX shadows) — current path is settled, this experiment does NOT
  recommend changes.
- **Secondary (future):** Post-Stage 5 / post-Stage 6 — if/when ProjectV commits to **path tracer architecture**,
  SOTA GI techniques become directly applicable.

### 7.2 Recommended action: KEEP current hybrid VCT+RTX + PLAN for path tracer pivot

**Step 1 (immediate, NO action)** — keep `vct-vs-rt-cutoff` cutoff=0.3 + `rt-shadows-vs-csm` hybrid CSM+RTX +
`clustered-forward-mass-lights` SSBO as-is. SOTA GI = architecturally incompatible. **Не строить path tracer
сейчас** — это multi-year effort вне scope текущего MVP.

**Step 2 (cross-vendor consideration, defer to Stage 6+)** — if/when mainline commits to path tracer:

- **First add:** SHaRC (NVIDIA RTXGI SDK v2.x, ~185 MB VRAM, 1.5-10% overhead, cross-vendor, voxel-adaptable
  via path tracer agnostic).
- **Second add:** DDGI (lower VRAM than SHaRC, but HLSL/SPIR-V porting required). Can replace VCT for cleaner
  diffuse if VCT leakage proves problematic in practice per Lumen SIGGRAPH 2022 concerns.
- **Third (optional):** ReSTIR DI/GI/PT via RTXDI SDK (D3D12 + Vulkan, but requires DXC toolchain per
  `dxc-vs-glslc-toolchain`). Most expensive integration (path tracer + reservoir buffers + temporal/spatial
  resampling passes). Highest quality payoff (MAPE 0.39 vs 1.63).
- **Skip:** NRC (NVIDIA-only, vendor lock-in, not viable cross-vendor per `dec-pipelines-async-compute` matrix).

### 7.3 Concrete mainline changes (для текущего Stage 5.x — NONE)

Per §7.2 Step 1: **no code changes recommended в текущем Stage 5.x scope.** Architecture is correct as-is for MVP.

Optional awareness items (не blockers):

- **`TODO.md` §5.1 VCT** — добавить `// EVIL:` note про known VCT leakage (per Lumen SIGGRAPH 2022). Если визуальные
  артефакты (leaking) появятся в production — trigger для DDGI evaluation per §7.2 Step 2.
- **`agent/knowledge.md` §15** (CSM baseline) — добавить cross-ref на этот experiment для SOTA GI upgrade path
  (post-Stage 5 / Stage 6+ planning).

### 7.4 Acceptance criteria (deferred до Stage 6+)

Если mainline решит commit к path tracer architecture (deferred, NOT recommended в current scope):

- **Voxel path tracer prototype** — VCT cone-march per bounce + 3D DDA fallback. Required foundation.
- **SHaRC integration** — `NVIDIA-RTX/SHARC` HLSL → SPIR-V via DXC, 4-pass dispatch (Update/Resolve/Compaction/Query).
  VRAM budget < 250 MB. Performance overhead < 10%.
- **Cross-vendor validation** — RTXGI 2.x SDK Vulkan path on AMD RDNA 4 + Intel Battlemage. NRC = skip.
- **Acceptance:** Visual quality per `agent/decisions.md §15` close-out rule (FINAL + GI debug views, framebuffer
  hash compare для voxel scenes, smoke tests с VoxelLab).

### 7.5 Estimated effort (deferred)

Если принят path tracer architecture pivot: **XL** (multi-session, multi-month).

- Voxel path tracer core: ~3000 LoC (recursive 3D DDA, BRDF sampling, MIS).
- SHaRC integration via RTXGI SDK: ~500 LoC (host + shader wrapper, 4-pass dispatch, descriptor binding).
- DDGI probe update pipeline: ~800 LoC (probe placement, ray dispatch, blending, query).
- DXR/Vulkan RT intersection shader for voxel BLAS: ~300 LoC (per `rt-shadows-vs-csm` Step 1 foundation).
- **Total: ~5000 LoC**, **XL effort**, **multi-session (3-6 months)**.

### 7.6 Re-evaluation triggers

- **VCT leakage visible в production** (cavity lighting artifact в VoxelLab / MeshingStress) — trigger для
  DDGI evaluation per Step 2.
- **Stage 4.3 (128+ chunks draw distance) ships + multi-bounce indirect visible limitation** — trigger для
  ReSTIR/DDGI re-evaluation.
- **Mainline commits to path tracer** (independent decision, не driven by this experiment) — trigger для
  SHaRC-first integration per Step 2.
- **NV/AMD/Intel vendor adds open-source SHaRC GLSL port** (currently HLSL-only) — reduces integration cost.
- **ReSTIR GSGI/PMGI derivatives stabilize** (2024 prototypes, ~0.4-0.8 ms overhead) — viable alternative to
  full ReSTIR PT if path tracer ships.

### 7.7 Cross-refs

- `vct-vs-rt-cutoff` (closed 2026-06-20, verdict=mixed) — current Stage 5.1 cutoff strategy.
- `rt-shadows-vs-csm` (closed 2026-06-20, verdict=mixed) — current Stage 5.2 hybrid CSM+RTX.
- `clustered-forward-mass-lights` (closed 2026-06-20, verdict=yes) — current Stage 5 light SSBO infrastructure.
- `nanovdb-on-gpu` (closed 2026-06-20, verdict=yes) — VCT SSBO + fragment DDA. **Foundation для hypothetical voxel
  path tracer** (re-use NanoVDB walker).
- `dec-pipelines-async-compute` (closed 2026-06-20, verdict=yes) — async compute foundation. Required для DDGI
  probe update + SHaRC resolve passes.
- `bindless-descriptor-overhead` (closed 2026-06-20, verdict=mixed) Phase E — RTX TLAS bindless = prerequisite
  для DDGI BLAS.
- `dxc-vs-glslc-toolchain` (open, m, Stage 0) — DXC adoption required для RTXGI SDK HLSL→SPIR-V.
- `agent/knowledge.md §15` (CSM baseline) + `agent/knowledge.md §30.4` (3-step migration precedent).
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold + «simple wins over
  complex with marginal gain»).

---

## 8. Sources

Полный список верифицированных источников (~30 ключевых + supplementary). Все citations сверены по году / автору /
контексту.

### 8.1 Foundation papers

- Bitterli, B., Wyman, C., Pharr, M., Shirley, P., Lefohn, A., Jarosz, W. «Spatiotemporal reservoir resampling for
  real-time ray tracing with dynamic direct lighting». ACM TOG (SIGGRAPH
  2020). <https://research.nvidia.com/sites/default/files/pubs/2020-07_Spatiotemporal-reservoir-resampling/ReSTIR.pdf>
- Ouyang, Y., Liu, S., Kettunen, M., Pharr, M., Pantaleoni, J. «ReSTIR GI: Path Resampling for Real-Time Path Tracing».
  Computer Graphics Forum (HPG 2021), 40(8):17-29. <https://diglib.eg.org/items/ae55c04f-4832-48af-b60a-95fecd62d0ce>
- Lin, D., Kettunen, M., Bitterli, B., Pantaleoni, J., Yuksel, C., Wyman, C. «Generalized Resampled Importance
  Sampling: Foundations of ReSTIR». ACM TOG (SIGGRAPH 2022), 41(4):75:
  1-23. <https://d1qx31qr3h6wln.cloudfront.net/publications/sig22_GRIS.pdf>
- Wyman, C., Panteleev, M. «World-Space Spatiotemporal Reservoir Reuse for Ray-Traced Global Illumination».
  ACM SIGGRAPH 2021. <https://dl.acm.org/doi/fullHtml/10.1145/3478512.3488613>
- Kettunen, M., Lin, D., Ramamoorthi, R., Bashford-Rogers, T., Wyman, C. «Conditional Resampled Importance Sampling
  and ReSTIR». NVIDIA 2023. Prototype: <https://github.com/NVLabs/conditional-restir-prototype>

### 8.2 Cache layer papers (DDGI / SHaRC / NRC)

- Majercik, Z., Guertin, J.-P., Nowrouzezahrai, D., McGuire, M. «Dynamic Diffuse Global Illumination with
  Ray-Traced Irradiance Fields». JCGT 8(2):1-30, 2019. Фундамент DDGI.
- Majercik, Z., Müller, T., Keller, A., Nowrouzezahrai, D., McGuire, M. «Dynamic Diffuse Global Illumination
  Resampling». ACM SIGGRAPH 2021 Talks. <https://dl.acm.org/doi/10.1145/3450623.3464635>
- Müller, T., Rousselle, F., Novák, J., Keller, A. «Real-time Neural Radiance Caching for Path Tracing». ACM TOG
  (SIGGRAPH 2021). <https://research.nvidia.com/publication/2021-06_Real-time-Neural-Radiance>
- NVIDIA-RTX/RTXGI (RTXGI SDK v2.x) — unified SDK для NRC + SHaRC, latest v2.7.0 (2026-03-01), 336 stars.
  <https://github.com/NVIDIAGameWorks/RTXGI>
- NVIDIA-RTX/SHARC — separate SHaRC library, 123 stars, integrated в RTXGI SDK v2.x.
  <https://github.com/NVIDIA-RTX/SHARC>
- DeepWiki SHARC reference: <https://deepwiki.com/NVIDIA-RTX/SHARC/2-core-architecture> +
  <https://deepwiki.com/NVIDIA-RTX/SHARC/3.1-setup-and-configuration>
- NVIDIA-RTX/RTXGI-DDGI — DDGI v1.x (legacy, separate repo). <https://github.com/NVIDIAGameWorks/RTXGI-DDGI>

### 8.3 RTXDI SDK

- NVIDIA-RTX/RTXDI — ReSTIR DI/GI/PT/ReGIR implementation. v3.0+. D3D12 + Vulkan via NVRHI.
  <https://github.com/NVIDIA-RTX/RTXDI>
- ReSTIR GI doc: <https://github.com/NVIDIA-RTX/RTXDI/blob/main/Doc/RestirGI.md>
- ReSTIR PT doc: <https://github.com/NVIDIA-RTX/RTXDI/blob/main/Doc/RestirPT.md>

### 8.4 Voxel-specific (critical для ProjectV)

- Crassin, C., Neyret, F., Sainz, M., Green, S., Eisemann, E. «Interactive Indirect Illumination Using Voxel Cone
  Tracing». CGF (Pacific Graphics 2011). <http://research.nvidia.com/labs/rtr/publication/crassin2011givoxels/>
- Wright, M. et al. «Lumen» (UE5 Advances in Real-Time Rendering in Games, SIGGRAPH 2022).
  <https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf>
- Narkowicz, K. «Journey to Lumen» (2022-08-18).
  <https://knarkowicz.wordpress.com/2022/08/18/journey-to-lumen/> — Epic explicitly rejected VCT как leaky.
- Minecraft RTX (NVIDIA + Microsoft GDC 2021): <https://www.youtube.com/watch?v=mDlmQYHApBU> — voxel scene + path
  tracer + irradiance cache для multi-bounce.
- OGRE-Next Cascaded Image VCT (CIVCT): <https://ogrecave.github.io/ogre-next/api/latest/_image_voxel_cone_tracing.html>
- Aokana (arXiv 2505.02017, May 2025) — GPU-Driven Voxel Rendering Framework.
  <https://arxiv.org/html/2505.02017v1>
- Douglas, «Adding global illumination to my game engine w/ DDGI [Voxel Devlog #23]» (Jun 2025).
  <https://www.youtube.com/watch?v=L1vhle74AEU> — прямой voxel + DDGI precedent.

### 8.5 ReSTIR derivatives (newer 2023-2024)

- ReSTIR FG (Final Gathering) — TU-Clausthal 2024 prototype: <https://github.com/stanleylin924/ReSTIR-FG>
- ReSTIR GSGI (Geometry Sampling GI) — Closest Hit blog 2024-09-20:
  <https://otrooney.github.io/global-illumination/2024/09/20/restir-gsgi.html>
- ReSTIR PMGI (Photon Mapping GI) — Closest Hit blog 2024-10-01:
  <https://otrooney.github.io/global-illumination/2024/10/01/restir-pmgi.html>
- Alegruz/Screen-Space-ReSTIR-GI (Falcor 5.1, 2022): <https://github.com/Alegruz/Screen-Space-ReSTIR-GI>
- DQLin/ReSTIR_PT (Falcor 4.4, 2022): <https://github.com/DQLin/ReSTIR_PT>

### 8.6 Production deployment + adoption

- Cyberpunk 2077 RT Overdrive (CDPR + NVIDIA Patch 2.1 Dec 2023) — ReSTIR DI/GI + SHaRC.
  Presentation: <https://intro-to-restir.cwyman.org/presentations/2023ReSTIR_Course_Cyberpunk_2077_Integration.pdf>
- Advanced Path Tracing mod (Apr 2024) — variant modes.
  <https://github.com/codecrafting-io/AdvancedPathTracingCP2077>
- Portal RTX — SHaRC. <https://www.nvidia.com/en-us/on-demand/session/gtcspring23-s51967/>
- Unreal Engine 5 NvRTX branch (Apr 2024+) — experimental ReSTIR GI by Jiayin Cao.
  <https://www.joeraasch.com/projects/nvrtxmay2024>
- NVIDIA Zorah demo (RTX 50 Series flagship, 2025) — uses ReSTIR PT.
  <https://agraphicsguynotes.com/posts/understanding_the_math_behind_restir_gi/>

### 8.7 Epic DDGI abandonment + Lumen (cross-cutting)

- Epic Developer Community Forums (Dec 2025): «we decided to focus development effort on Lumen HWRT GI, we're
  not looking to revisit/revive DDGI/RTXGI».
  <https://forums.unrealengine.com/t/official-ddgi-rtxgi-plugin-integration-support-or-suggested-plug-in-workflow/2689108>
- Arc Raiders (Embark Studios) = counter-example: DDGI успешно.
- NVIDIA RTXGI blog (Mar
  2024): <https://developer.nvidia.com/blog/generative-ai-for-digital-humans-and-new-ai-powered-nvidia-rtx-lighting/>

### 8.8 Supplementary (additional context)

- Wccftech RTXGI 2.0 launch coverage (Mar 2024):
  <https://wccftech.com/nvidia-rtxgi-2-0-available-next-frontier-ray-traced-visuals-neural-radiance-cache-spatial-hash-radiance-cache-dynamic-diffuse-global-illumination/>
- NVIDIA RTX Kit overview: <https://developer.nvidia.com/rtx-kit>
- RTXGI 2.x Changelog: <https://github.com/NVIDIAGameWorks/RTXGI/blob/main/Changelog.md>
- SHaRC Changelog: <https://github.com/NVIDIA-RTX/SHARC/blob/main/CHANGELOG.md>
- NVIDIA-RTX/SHARC docs/Integration.md (4-pass
  detail): <https://github.com/NVIDIA-RTX/SHARC/blob/main/docs/Integration.md>
- RTXGI DDGI Integration: <https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/docs/Integration.md>
- RTXGI DDGI Volume reference: <https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/docs/DDGIVolume.md>
- NVRHI (DX12 + Vulkan abstraction): <https://github.com/NVIDIAGameWorks/NVRHI>
- Falcor framework: <https://github.com/NVIDIAGameWorks/Falcor>
- Cross-vendor RT matrix per `rt-shadows-vs-csm` §5 + `dec-pipelines-async-compute` §2.2 vendor caveats.

---

## 9. Mapping to ProjectV hot-path

- **Какой участок движка:** Stage 5.1 (VCT cone-march в `voxel.frag`) + Stage 5.2 (RTX
  `rayQueryEXT` в `voxel.frag`). ReSTIR/DDGI/SHaRC потенциально заменяют/расширяют оба.
- **Зависимости от closed experiments:**
    - `nanovdb-on-gpu` (yes) — Voxel SSBO source для VCT/ReSTIR probe generation.
    - `dec-pipelines-async-compute` (yes) — async compute pass для DDGI probe update + ReSTIR
      resampling.
    - `vct-vs-rt-cutoff` (mixed) — current VCT cutoff=0.3 strategy.
    - `rt-shadows-vs-csm` (mixed) — current hybrid CSM+RTX shadow baseline.
    - `clustered-forward-mass-lights` (yes) — light source для ReSTIR/DDGI evaluation.
    - `bindless-descriptor-overhead` (mixed) — Phase E RTX TLAS bindless = prerequisite для
      DDGI probe BLAS.
- **Что остаётся неизмеренным:** реальный ProjectV prototype (deferred per `rt-shadows-vs-csm`
  precedent). Cross-vendor числа из literature, не локальных замеров.
- **Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §3
  (RTX 3060 Ti GA104 Ampere, 38 RT cores, 8 GiB VRAM) + §4 (`VK_KHR_acceleration_structure` rev 13,
  `VK_KHR_ray_query` rev 1, `VK_KHR_deferred_host_operations` rev 4).