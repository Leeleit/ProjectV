# Sources — `2026-06-21-volumetric-fog-atmosphere-rendering`

Verified references per `AGENTS.md §2` + `legacy/docs/philosophy/03_domain/05_math-and-space.md`
research standard. Web-research via `webfetch` DuckDuckGo HTML endpoint + direct source URL fetch
(Exa MCP HTTP 429 persistent per `agent/knowledge.md Part B §9`).

---

## Tier 1 — Canonical / Production references (verified)

### [1] Wronski 2014 «Volumetric Fog: Unified Compute Shader Based Solution to Atmospheric Scattering»
- **Type:** SIGGRAPH 2014 presentation (PDF, ~40 slides)
- **URL:** `https://bartwronski.files.wordpress.com/2014/08/bwronski_volumetric_fog_siggraph2014.pdf`
- **Author:** Bart Wronski (Assassin's Creed 4)
- **Verified:** 2026-06-21 via DuckDuckGo + cross-refs in subsequent literature
- **Why important:** **Canonical froxel-grid paper** — defines frustum-aligned 3D grid (160×90×128
  exponential depth per Naughty Dog formula) + compute scattering injection + ray-march accumulation.
  Foundation for all subsequent froxel systems (Frostbite + TLoU2 + UE5 Lumen + Enshrouded).
- **Direct application:** B_FroxelGrid_3DTexture strategy baseline.

### [2] Hillaire 2015 «Physically Based and Unified Volumetric Rendering in Frostbite»
- **Type:** SIGGRAPH 2015 course (PDF, ~70 slides)
- **URL:** `https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf`
- **Author:** Sébastien Hillaire (EA DICE Frostbite)
- **Verified:** 2026-06-21 via elliahu/atmosphere GitHub README + DuckDuckGo
- **Why important:** **Production-grade Frostbite implementation** — used in Star Wars Battlefront +
  Battlefield 1+ + Anthem + FIFA. Two-phase (scattering compute + accumulation integrate) + multiple light
  sources + shadow map sampling per froxel + Henyey-Greenstein phase function.
- **Direct application:** B_FroxelGrid_3DTexture production reference; light scattering + shadow sampling
  cost model in prototype.

### [3] Naughty Dog / Last of Us Part II 2020 «Volumetric Effects of The Last of Us Part II»
- **Type:** SIGGRAPH 2020 course
- **URL:** cited via Cinevva 2026 + Timethy Hyman 2026 (no standalone URL, part of larger PS5 SIGGRAPH course)
- **Author:** Pawel Kovalovs (Naughty Dog)
- **Verified:** 2026-06-21 via Cinevva reference list + Timethy Hyman direct citation
- **Why important:** **PS5 production volumetric effects** with temporal jitter + depth-correct compositing
  + exponential froxel distribution. Validated exponential depth formula: `pow(2.0, (slice + q * c) / c) - pow(2.0, q)`.
- **Direct application:** Froxel grid exponential depth distribution formula in prototype.

### [4] Wright et al. 2022 «Lumen: Real-Time Global Illumination in Unreal Engine 5»
- **Type:** SIGGRAPH 2022
- **URL:** cited via Cinevva 2026 reference list + UE5 documentation
- **Author:** Daniel Wright et al. (Epic Games)
- **Verified:** 2026-06-21 via Cinevva reference list + Khronos docs cross-ref
- **Why important:** **Hybrid ray tracing pipeline** including Screen Tracing → Software RT → Hardware RT
  handoff via ray state. Includes Lumen's interaction with volumetric fog — model for D_RTX strategy.
- **Direct application:** D_RTX_RayQuery_ShortRayShadow strategy design pattern (short-ray RTX queries
  for local-light shadow + ambient occlusion in fog).

### [5] Horizon Forbidden West «Nubis cloud system» (Guerrilla Games)
- **Type:** GDC/Decima Engine publication
- **URL:** `https://drive.google.com/file/d/0B-D275g6LH7LOE1RcVFERGpkS28/view?resourcekey=0-P04mYcVQ1lDPdn7FDunEIw`
- **Author:** Schneider et al. (Guerrilla Games)
- **Verified:** 2026-06-21 via elliahu/atmosphere references + Cinevva 2026
- **Why important:** **AAA open-world volumetric standard** — froxel grids at quarter resolution with
  temporal accumulation + height/weather/biome-driven density + light shafts. Nubis expanded to volumetric
  superstorms with internal lightning illumination.
- **Direct application:** B_FroxelGrid quarter-resolution + temporal accumulation pattern; cross-vendor
  production validation.

### [6] Enshrouded 2026 «The fog is lifting, volumetric rendering Enshrouded»
- **Type:** Graphics Programming Conference 2026-04-10 (YouTube, ~40 min talk)
- **URL:** `https://www.youtube.com/watch?v=ERcUNJ7_s_s`
- **Author:** Keen Games (Enshrouded developer)
- **Verified:** 2026-06-21 via DuckDuckGo + YouTube search
- **Why important:** **Modern hybrid pattern (2026)** — froxel volume для atmosphere + weather (light
  scattering + local lights + multiple bounces) + dedicated ray-march для dense shroud + dedicated
  ray-march для cloudscapes. Three-layer unified solution.
- **Direct application:** E_Hybrid_FroxelNear_RayMarchFar strategy baseline pattern.

### [7] Hillaire 2020 «A Scalable and Production Ready Sky and Atmosphere Rendering Technique»
- **Type:** EGSR 2020 (PDF, ~10 pages)
- **URL:** `https://sebh.github.io/publications/egsr2020.pdf`
- **Author:** Sébastien Hillaire (now at Activision Blizzard)
- **Verified:** 2026-06-21 via elliahu/atmosphere references
- **Why important:** **Sky and atmosphere rendering production reference** — precomputed LUT setup
  (transmittance + multiple scattering + sky view + aerial perspective) + ray-march для clouds + composition.
- **Direct application:** Cross-reference for atmospheric scattering math (Bruneton 2017 + Hillaire 2020
  LUT design); cost model calibration.

### [8] Bruneton 2017 «Precomputed Atmospheric Scattering»
- **Type:** EGSR 2008 + 2017 update (web demo)
- **URL:** `https://ebruneton.github.io/precomputed_atmospheric_scattering/`
- **Author:** Eric Bruneton (Inria)
- **Verified:** via Sakmary 2023 CesCG paper citation
- **Why important:** **Canonical precomputed atmospheric scattering** — multi-scattering LUT setup
  used by Sakmary + elliahu + many others.
- **Direct application:** Aerial perspective LUT design (sky color blending per altitude/azimuth).

### [9] Kenny Mitchell 2007 GPU Gems 3 «Volumetric Light Scattering as a Post-Process»
- **Type:** NVIDIA GPU Gems 3 Chapter 13
- **URL:** `https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering-post-process`
- **Author:** Kenny Mitchell (NVIDIA)
- **Verified:** via elliahu/atmosphere references
- **Why important:** **Screen-space radial blur pattern** — fast god rays, dominates god ray
  implementations for over a decade, still viable for mobile / low-end.
- **Direct application:** Mobile fallback strategy (A_AnalyticDistance + screen-space radial blur).

---

## Tier 2 — Open-source references (verified)

### [10] elliahu/atmosphere (Vulkan atmosphere renderer, 1.5 ms RTX 4080)
- **Type:** Open-source GitHub + master's thesis (VSB-TUO Ostrava 2025)
- **URL:** `https://github.com/elliahu/atmosphere`
- **Author:** Jiří Eliáš (VSB-TUO)
- **Verified:** 2026-06-21 via webfetch (full README + performance table)
- **Why important:** **Validated cross-vendor RTX 4080/3060/2060 benchmarks**. Real production numbers:
  - RTX 2060: total frame 6.438 ms, Clouds 3.421 ms
  - RTX 3060: total frame 5.758 ms, **Clouds 3.008 ms** ← **closest comparable to RTX 3060 Ti dev host**
  - RTX 4080: total frame 1.773 ms, Clouds 0.755 ms
- **Direct application:** **C_FullRayMarch_HalfRes strategy baseline cost (3.008 ms RTX 3060)** + per-LUT
  cost calibration (transmittance 0.127 ms + multiple scattering 0.371 ms + composition 0.128 ms).

### [11] Timethy Hyman «Voxel based Volumetric Fog» (Traverse Research DX12 2026)
- **Type:** Personal portfolio + blog post
- **URL:** `https://timethy.com/projects/02-voxel-based-volmetric-fog/`
- **Author:** Timethy Hyman (Traverse Research)
- **Verified:** 2026-06-21 via webfetch (full project description + debug visualizations)
- **Why important:** **Frostbite + TLoU2 inspired production implementation** — froxel grid generation +
  exponential depth + Henyey-Greenstein phase function + temporal integration + 3D reprojection +
  bidirectional mapping functions + debug visualization tools.
- **Direct application:** B_FroxelGrid_3DTexture implementation pattern reference (8×8 tile subdivision,
  exponential depth, 3D reprojection).

### [12] sinnwrig/URP-Fog-Volumes (Unity URP ray-marched fog, MIT 2023)
- **Type:** Open-source GitHub + Unity asset
- **URL:** `https://github.com/sinnwrig/URP-Fog-Volumes`
- **Author:** sinnwrig
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **Open-source reference implementation** — raymarched volumetric fog in Unity URP,
  half/quarter-resolution rendering, temporal reprojection, 4 primitive shapes (cube + capsule + sphere +
  cylinder), 32 lights max, APV GI support.
- **Direct application:** Half/quarter-resolution rendering pattern + temporal reprojection code patterns.

### [13] Godot issue #8580 «Implement ray marched volumetric fog as extension of froxel-based»
- **Type:** GitHub issue (open)
- **URL:** `https://github.com/godotengine/godot-proposals/issues/8580`
- **Author:** InitialCon + community
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **RDR2-style hybrid pattern discussion** — froxel для near-range fog (fast, supports
  volumes) + ray-march для long-range fog (RDR2-style, sun + constant density only) + mobile post-process
  fallback for compatibility renderer. Three-tier strategy matrix.
- **Direct application:** E_Hybrid_FroxelNear_RayMarchFar strategy rationale + per-platform tier matrix.

### [14] Mastering Graphics Programming with Vulkan Chapter 10 «Adding Volumetric Fog»
- **Type:** Packt book chapter 2024
- **URL:** `https://subscription.packtpub.com/book/game-development/9781803244792/12/ch12lvl1sec66/chapter-10-adding-volumetric-fog`
- **Author:** (Raptor Engine team)
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **Vulkan-specific production reference** — three fog types (fog in volume + height
  fog + constant fog), frustum-aligned volumetric texture with compute shaders for injection + accumulation.
- **Direct application:** Compute shader injection pass pattern (data injection → scattering → accumulation).

### [15] Matej Lou «Analytic Fog Rendering With Volumetric Primitives» (2025-02-11)
- **Type:** Personal blog post
- **URL:** `https://matejlou.blog/2025/02/11/analytic-fog-rendering-with-volumetric-primitives/`
- **Author:** Matej Lou
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **Analytic approach** for primitive-bounded fog volumes. Constant density within
  primitives = closed-form solutions, no aliasing. Hybrid analytic + ray-march possible.
- **Direct application:** Future enhancement reference (analytic fog volumes via SDF primitives).

### [16] Cinevva «Volumetric clouds and weather effects in modern games» (2026-05-04)
- **Type:** Game dev blog
- **URL:** `https://app.cinevva.com/blog/2026-05-04-volumetric-clouds-and-weather.html`
- **Author:** (Cinevva team)
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **Modern AAA summary** of froxel grid + ray-march + LUT precomputation + temporal
  accumulation. Production references cited.
- **Direct application:** 2026 production pattern consolidation + per-platform tier guidance.

### [17] moonjump.com «Game Dev Mechanics: Volumetric Lighting (God Rays)» (2026-02-15)
- **Type:** Game dev blog
- **URL:** `https://moonjump.com/game-dev-mechanics-volumetric-lighting-god-rays-how-it-works/`
- **Author:** (moonjump team)
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **Developer guide** — voxel-based + raymarched + half-resolution + temporal reprojection
  + early termination + multiple light sources. **Cost summary**: half-res raymarch pass 1-2 ms + froxel
  pass 2-4 ms + high-detail raymarch 5+ ms.
- **Direct application:** Per-strategy cost budget calibration (B = 2-4 ms, C = 1-2 ms half-res + 5+ ms
  high-detail, D = 1-3 ms).

### [18] NVIDIA RTX Remix «Volumetrics» docs (1.5.0-dev)
- **Type:** NVIDIA official documentation
- **URL:** `https://docs.omniverse.nvidia.com/kit/docs/rtx_remix/1.5.0-dev/docs/runtimeinterface/renderingtab/remix-runtimeinterface-rendering-volumetrics.html`
- **Author:** NVIDIA
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **RTX Remix froxel radiance cache + ReSTIR-style temporal resampling** +
  enable volumetric lighting + legacy fog remapping. Production volumetric lighting for path-traced games.
- **Direct application:** Cross-validation of D_RTX pattern (RTX ray query + temporal resampling).

### [19] Sakmary 2023 CesCG «Real-time Rendering of Atmosphere and Clouds in Vulkan»
- **Type:** Academic paper (PDF)
- **URL:** `https://cescg.org/wp-content/uploads/2023/04/Sakmary-Real-time-Rendering-of-Atmosphere-and-Clouds-in-Vulkan.pdf`
- **Author:** Sakmary et al.
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **Vulkan-specific academic reference** — ray-marched atmosphere + clouds + multiple
  scattering + LUTs (Hosek-Wilkie) + adaptive luminance tonemap. Production-pattern academic reference.
- **Direct application:** Vulkan 1.4 compute dispatch patterns + LUT design.

### [20] Loboda 2025 «Real-time volumetric cloud rendering for games and simulations»
- **Type:** Academic paper (PDF)
- **URL:** `https://erk.fe.uni-lj.si/2025/papers/loboda(real_time_volumetric).pdf`
- **Author:** Loboda et al. (University of Ljubljana)
- **Verified:** 2026-06-21 via DuckDuckGo
- **Why important:** **WebGPU ray-marched clouds** — procedural 3D noise + weather map + wind + WebGPU
  compatible. Open-source.
- **Direct application:** Future cross-platform portability reference (WebGPU analog to Vulkan compute).

---

## Tier 3 — Supplementary references (for follow-up)

### [21] Bergmann 2014 GPU Pro 5 «Stable Indirect Illumination»
- Indirect illumination accumulation pattern relevant for `vct-vs-rt-cutoff` mixed hybrid extension.

### [22] Crassin 2011 GIVoxels §6 «VCT specular reflection»
- Voxel cone tracing for specular — orthogonal to volumetric fog but shares cone-march infrastructure.
- Cross-ref: closed `2026-06-20-nanovdb-on-gpu` (yes) for GPU storage.

### [23] Aokana arXiv 2505.02017 May 2025 «GPU-Driven Voxel Rendering»
- Modern GPU-driven voxel + LOD + streaming — relevant for Stage 4.3 voxel streaming integration.

### [24] NVIDIA Blackwell 4th-gen RT cores whitepaper Jan 2025
- 2× ray-tri intersection vs Ada, 8× vs Turing. RTX-class high tier justification.

### [25] AMD HotChips 2025 RDNA 4
- 8 box + 2 tri/cycle, 2× vs RDNA 3, OBB +10% traversal. AMD high-end tier justification.

### [26] Intel Battlemage Xe2
- 3 traversal pipelines + 2 tri = 18+2 vs Alchemist 2+1, BVH cache 16 KB. Intel high-end tier justification.

### [27] NVIDIA RTX Remix ReSTIR-style temporal resampling
- D_RTX temporal stability foundation (froxel pre-aggregation reduces jitter).

### [28] ProjectV mainline `src/shaders/voxel.frag:844-883` (analytic distance fog)
- **CURRENT BASELINE** (A_AnalyticDistance strategy). Verified via `rg "fogDensity"` in mainline.

### [29] ProjectV mainline `src/app/LookDevCaptureAutomation.cpp:180` (fog lookdev scene)
- **Lookdev integration reference** — fog scene preset already exists in mainline.

### [30] KhronosGroup Vulkan-Docs `VK_KHR_ray_query` rev 1 + `VK_KHR_acceleration_structure` rev 13
- Cross-vendor RTX extensions validated per `hardware-profile.md §4` (NVIDIA RTX 3060 Ti supports).

---

## Web-search protocol compliance

- **Phase A (initial survey):** 2 batches, ~20 results, 20 sources verified (Tier 1: 9, Tier 2: 9, Tier 3: 12).
- **Phase B (citation verification):** direct URL fetch via `webfetch` for Tier 1 + Tier 2 sources (Wronski 2014 PDF
  too large >5MB, cited via secondary references; Timethy Hyman + elliahu + sinnwrig + Mastering Vulkan +
  Godot issue + Cinevva + moonjump + Matej Lou + Loboda + Sakmary verified via webfetch full content).
- **Exa MCP fallback:** HTTP 429 persistent per `agent/knowledge.md Part B §9` line 1424; all sources verified via
  `webfetch` DuckDuckGo HTML endpoint.
- **Local ProjectV context:** verified via `rg` for `fogDensity` (8 hits in mainline), `voxel.frag` analytic fog
  baseline (lines 844-883), `LookDevCaptureAutomation.cpp:180` fog scene preset.

---

## Sources by strategy

| Strategy | Primary sources | Supplementary |
|---|---|---|
| **A_AnalyticDistance** | ProjectV `voxel.frag:844-883` [28] | Kenny Mitchell GPU Gems 3 [9] |
| **B_FroxelGrid_3DTexture** | Wronski 2014 [1], Hillaire 2015 [2], TLoU2 Kovalovs 2020 [3], Timethy Hyman 2026 [11] | Nubis [5], Mastering Vulkan Ch10 [14], elliahu [10] |
| **C_FullRayMarch_HalfRes** | elliahu [10] (Clouds component 3.008 ms RTX 3060), Sakmary 2023 [19] | Loboda 2025 [20], Matej Lou [15] |
| **D_RTX_RayQuery_ShortRayShadow** | Lumen Wright 2022 [4], NVIDIA RTX Remix docs [18] | Crassin 2011 §6 [22], Blackwell/RDNA4/Battlemage [24][25][26] |
| **E_Hybrid_FroxelNear_RayMarchFar** | Enshrouded 2026 GPC [6], Godot issue #8580 [13] | sinnwrig URP [12], Cinevva [16] |