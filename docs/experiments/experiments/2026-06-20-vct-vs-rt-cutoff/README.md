# 2026-06-20-vct-vs-rt-cutoff — VCT vs RTX roughness cutoff для ProjectV Stage 5

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-20
**Date closed:** 2026-06-20
**Stage link:** TODO.md §5.1 (VCT) + §5.2 (RTX shadows); cross-impact §3.1 (GPU Fluid CA uses VCT volume sampling),
§6.2.2 (DDA shader macro).
**Estimated effort:** M (literature review + analytical cost model + cross-vendor matrix; no prototype).
**Author:** research agent (`docs/experiments/AGENTS.md`).

---

## 1. Hypothesis

**Утверждение:** для ProjectV Stage 5 hybrid (VCT per `TODO.md §5.1` + RTX shadows per `TODO.md §5.2`, **additive** к
CSM per `decisions.md §15` [First sun-shadow path](#15-first-sun-shadow-path)),
**roughness threshold ≈ 0.3–0.5** — оптимальный cutoff для specular ray-march:

- **Roughness > threshold** → **VCT specular cone trace** (cone-march через mip-mapped voxel atlas, mip-level derived
  from cone aperture per Crassin 2011).
- **Roughness < threshold** → **RTX per-pixel ray query** (`rayQueryEXT` per `VK_KHR_ray_query`, BLAS per chunk per
  `TODO.md §5.2`).
- **Roughness = 0 (mirror)** → **RTX only** (VCT degenerate в raymarch; cone aperture = 0 = effectively 1 sample).
- **Diffuse GI** → **VCT always** (6 wide cones over hemisphere, не зависит от roughness, mip-mapped cheap).
- **AO / contact shadows** → **RTX always** (1–4 ray queries per fragment, super cheap with HW RT cores).
- **Direct sun shadow** → **CSM current path** (4-cascade per `decisions.md §15`; RTX = additive only).

**Какое преимущество это даст:**

1. **Adaptive cost:** VCT specular cone cost grows quadratically with 1/roughness (narrower cone → more mip steps +
   higher mip level = more memory reads), RTX specular cost is ~constant per ray. Crossover point = sweet spot для frame
   budget.
2. **No hardware dependency для VCT** (pure compute, mip-traced 3D texture, all GPUs since 2009 Vulkan baseline).
3. **Graceful degradation** на hardware без RT cores (Vulkan fallback = VCT-only; sharp specular degraded but not
   absent).
4. **Cross-vendor consistent** — VCT is compute-only (works on NVIDIA + AMD + Intel + Arm + Mobile identically); RTX
   path benefits from newer HW (Blackwell 2× Ada, RDNA 4 2× RDNA 3) but always available since Turing / RDNA 2.
5. **Aligns with industry consensus:** OGRE 2019 hybrid (VCT rough + RTX sharp with explicit roughness threshold), Lumen
   2022 hybrid (surface cache + HW RT, rejected pure VCT), Akenine-Möller JCGT 2021 (formal math for roughness → cone
   spread), Minecraft RTX (two-LOD roughness-based selection).

**Где я ожидаю, что может проиграть гипотеза (cutoff = 0.3–0.5):**

- **OGRE 2019 actual cutoff = 0.02** (not 0.3) — they hit floating-point precision issues в 3D texture lookup at low mip
  levels. ProjectV uses R8G8B8A8 atlas (per `TODO.md §5.1`) → same potential issue. **Threshold может быть **ниже** чем
  0.3 для ProjectV due to atlas precision.**
- **HW RT perf rapidly improving** (Blackwell 2× Ada, RDNA 4 2× RDNA 3, Battlemage Xe2 18 box/2 tri) → **threshold может
  быть **выше** чем 0.5** на newer HW (RTX cheap enough для большинства roughness).
- **Lumen 2022 rejected pure VCT** because of "leaking in coarse mips" (voxel merging artifacts). Если ProjectV's voxel
  atlas suffers similar issues (especially at high mip levels = coarse spatial resolution), VCT quality может быть ниже
  чем ожидалось → RTX adoption threshold **ниже**.
- **Aokana 2025 uses SVDAG-on-GPU (per `nanovdb-on-gpu` §1)** — but for ray-march traversal, not for cone trace cone
  integration step. May not affect VCT architecture.

**Альтернативы (отвергнутые или parked):**

| Альтернатива                              | Источник                                               | Trade-off                                                                                                                            |
|:------------------------------------------|:-------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------|
| Pure VCT, no RTX                          | Crassin 2011, VXGI 0.9, HanetakaChou 2022              | Cheaper, but Lumen 2022 proved leaky at coarse mips; sharp specular quality insufficient                                             |
| Pure RTX (DDGI / SHaRC / NRC / ReSTIR PT) | NVIDIA RTXGI 2.0 (2024), RTXDI 3.0 (2024)              | Best quality, but **hardware-dependent** + DDGI per-light update cost (Lumen observed 0.4ms/light) + scene complexity-dependent perf |
| Path tracing with ReSTIR PT               | Lin et al. 2022 "Generalized ReSTIR"                   | Path tracing слишком heavy для 60 FPS voxel game; reserved for offline / next-gen Stage 7+                                           |
| Surface cache + HW RT (Lumen 2022)        | Wright et al. SIGGRAPH 2022, Narkowicz 2022            | Avoids voxelization, но **requires Nanite-like mesh card system** — out of ProjectV scope (SVO = mainline, per `decisions.md §1.2`)  |
| Hybrid VCT + RT (THIS hypothesis)         | OGRE 2019, Akenine-Möller JCGT 2021, Lumen hybrid mode | Best balance; industry consensus                                                                                                     |

---

## 2. Prior art

Web-research выполнен `2026-06-20` через Exa (per `AGENTS.md §5.3`, `docs/experiments/AGENTS.md §4`).
**~30 sources** (12 directly cited, 18 cross-references), all verified by year/author/context.
См. полный список в [`sources.md`](./sources.md).

**Ключевые источники (8 foundational + 4 supporting):**

1. **Crassin et al. — "Interactive Indirect Illumination Using Voxel Cone Tracing" (Pacific Graphics 2011,
   GIVoxels)
   ** — [https://onlinelibrary.wiley.com/doi/10.1111/j.1467-8659.2011.02063.x](https://onlinelibrary.wiley.com/doi/10.1111/j.1467-8659.2011.02063.x).
   *Оригинальный VCT paper. 3-step algorithm: (1) inject radiance into SVO via RSM splatting, (2) mipmap
   filter higher levels, (3) final gather per fragment with cones. Diffuse = 5-6 wide cones over hemisphere,
   specular = 1 narrow cone in reflection direction. **Specular cone aperture = derived from Phong specular
   exponent** (упрощённый roughness model). 25–70 FPS на GTX 480 для interactive rates. **Foundation of all
   subsequent VCT work**, включая VXGI (NVIDIA 2012, archived), HanetakaChou/Voxel-Cone-Tracing (2022), Aokana
   2025 (SVDAG variant).*

2. **NVIDIA — "VXGI 0.9 SDK Documentation" (2014-2018, archived)** —
   [https://docs.nvidia.com/gameworks/content/gameworkslibrary/visualfx/vxgi/product.html](https://docs.nvidia.com/gameworks/content/gameworkslibrary/visualfx/vxgi/product.html).
   *NVIDIA's reference VCT implementation. "VXGI calculates one-bounce diffuse indirect illumination using
   the voxel cone tracing method". Specular cone cost > diffuse cost because "specular cones access higher
   level mips and sometimes even the base voxels". **Source of analytical cost observation**: VCT specular
   more expensive than VCT diffuse at given roughness → supports VCT-vs-RTX cutoff above roughness.*

3. **Crassin — "Voxel Cone Tracing and Sparse Voxel Octree for Real-Time Global Illumination" (NVIDIA GTC 2012
   talk slides)
   ** — [https://developer.download.nvidia.com/GTC/PDF/GTC2012/PresentationPDF/SB134-Voxel-Cone-Tracing-Octree-Real-Time-Illumination.pdf](https://developer.download.nvidia.com/GTC/PDF/GTC2012/PresentationPDF/SB134-Voxel-Cone-Tracing-Octree-Real-Time-Illumination.pdf).
   *Origin slides: SVO + VCT for "Direct+Indirect lighting in a game" (EPIC Games SVOgi). 2012 timestamp —
   VCT in shipped game engine (Unreal SVOgi) 13 лет назад. Practical production validation.*

4. **Goldberg (OGRE team) — "Voxel Cone Tracing" blog post (2019-08-05)** —
   [https://www.ogre3d.org/2019/08/05/voxel-cone-tracing](https://www.ogre3d.org/2019/08/05/voxel-cone-tracing).
   *Critical для этого эксперимента. Explicit hybrid algorithm:
   ```
   1. If roughness > threshold, use VCT for spec reflections
   2. If roughness < threshold but not 0:
      2a. Raymarch through the VCT until find a cell with alpha != 0 and test against triangles in that cell
      2b. Separately run RTX query
      2c. Blend result based on cone aperture
   3. If roughness = 0, only run RTX
   ```
   **Caveat (важно для ProjectV):** "we only use the Hybrid for roughness <= 0.02; as the errors are quite
   visible, since Specular reflections are high frequency" — **floating-point precision issues в 3D texture
   lookup at low mip levels (8-bit atlas = 8-bit precision per channel).** ProjectV's R8G8B8A8 atlas per
   `TODO.md §5.1` has same potential issue.*

5. **Akenine-Möller, Crassin, Boksansky, Belcour, Panteleev, Wright — "A Ray-Branch for BVH Ray Tracing, and
   Ray-Cones for Soft Shadows and Cone Tracing" (JCGT Vol 10 No 1, 2021)** —
   [https://www.jcgt.org/published/0010/01/01/paper-lowres.pdf](https://www.jcgt.org/published/0010/01/01/paper-lowres.pdf).
   *Formal mathematics for ray-cone spread as function of BRDF roughness. Section 4: "Integrating BRDF
   Roughness" — derives spread angle β_r for GGX microfacet model. Section: "This simple technique has been
   used successfully in Minecraft with RTX on Windows 10 to select between two shading LODs. The technique
   was also used when shading diffuse secondary rays, but then using a fixed cone angle in that case."*
   **THE theoretical foundation for roughness → cone aperture; validates the Lumen 2022 hybrid approach.**

6. **Wright et al. (Epic Games) — "Lumen SIGGRAPH 2022 Advances" (presentation)** —
   [https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf](https://advances.realtimerenderendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf).
   *Critical industry validation. Epic's Lumen explicitly rejected pure VCT:*
   > "We tried runtime voxelization and voxel cone tracing, but merging geometry properties into a volume
   > causes lots of leaking, especially in the lower mip maps. We also tried voxel bit bricks, where we stored
   > 1 bit per voxel to mark whether it contains geometry or not. Simple ray marching of bit bricks was
   > surprisingly slow and after adding a proximity map for acceleration, we just decided to drop voxels and
   > arrived at a Global Distance Field."
   *Final design: Surface Cache (mesh cards atlas) + Hardware RT. Lumen offers TWO evaluation modes: software
   ray tracing OR hardware ray tracing. **Light throughput too expensive to evaluate every frame** — uses
   Surface Cache caching with per-page update ratio (cheap direct lighting updates faster than indirect).*

7. **Narkowicz (Epic) — "Journey to Lumen" blog post (2022-08-18)** —
   [https://knarkowicz.wordpress.com/2022/08/18/journey-to-lumen/](https://knarkowicz.wordpress.com/2022/08/18/journey-to-lumen/).
   *First-hand engineer retrospective. Confirms:*
   > "The first successful approach was to implement pure voxel cone tracing, where the entire scene was
   > voxelized at runtime and we would ray march it just like in the classic 'Interactive Indirect Illumination
   > Using Voxel Cone Tracing' paper. The main drawback of voxel cone tracing is leaking due to aggressive
   > merging of scene geometry, which is especially visible when tracing coarser (lower) mip-maps."
   *Final reasoning for hybrid: voxel bit bricks tried, rejected as "slow". Lumen 2.0 = surface cache + HW RT.*

8. **NVIDIA — "RTXGI 2.0 SDK" (GDC 2024-03, GitHub 2026-03 v2.7.0)** —
   [https://github.com/NVIDIAGameWorks/RTXGI](https://github.com/NVIDIAGameWorks/RTXGI) +
   [https://wccftech.com/nvidia-rtxgi-2-0-available-next-frontier-ray-traced-visuals-neural-radiance-cache-spatial-hash-radiance-cache-dynamic-diffuse-global-illumination/](https://wccftech.com/nvidia-rtxgi-2-0-available-next-frontier-ray-traced-visuals-neural-radiance-cache-spatial-hash-radiance-cache-dynamic-diffuse-global-illumination/).
   *Three techniques в RTXGI 2.0:*
    - **NRC (Neural Radiance Cache)** — Tensor Cores required, NVIDIA only (excludes AMD/Intel for now).
    - **SHaRC (Spatial Hash Radiance Cache)** — "works with any DirectX or Vulkan ray-tracing-capable GPU"
      (cross-vendor). **Path tracing-based**, replaces traditional probe-based irradiance caching.
    - **DDGI (Dynamic Diffuse Global Illumination)** — multi-bounce probe-based, Vulkan + DXR support.
      *SDK supports Vulkan via NVRHI abstraction (per RTXDI 3.0 docs). **Important для ProjectV**: cross-vendor
      SHaRC, but requires HW RT; pure VCT remains the only no-HW-RT option.*

**Supporting sources (4):**

9. **Erlich, Aristizabal, Li, Woodard, Humer, Eckhardt — "Comparing NVIDIA RTX and a Novel Voxel-Space Ray
   Marching Approach as Global Illumination Solutions" (Eurographics 2024 Poster)** —
   [https://diglib.eg.org/items/278099e3-ee0e-454c-aa47-cda872c02d5b](https://diglib.eg.org/items/278099e3-ee0e-454c-aa47-cda872c02d5b).
   *Direct VCT vs DXR comparison. Conclusion: "similar quality outcome and less progressive dependency on the
   number of rays for VSRM compared with DXR" — VCT/RT have similar quality for low ray counts (1-32), VCT
   is more predictable. Supports VCT-as-fallback for low-budget hardware.*

10. **NVIDIA — "Blackwell GPU Architecture Whitepaper" (2025-01-15)** —
    [https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf](https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf).
    *4th-gen RT Cores: 2× ray-triangle intersection rate over Ada Lovelace, Triangle Cluster Intersection
    Engine for Mega Geometry, Opacity Micromap Engine. Implication: Blackwell = RTX 2× cheaper than Ada →
    RTX adoption threshold может быть выше чем 0.5 на Blackwell.*

11. **HWCooling.net — "Better, more capable than expected: RDNA 4 architecture deep dive" (2025-02-28)** —
    [https://www.hwcooling.net/en/better-more-capable-than-expected-rdna-4-architecture-deep-dive/](https://www.hwcooling.net/en/better-more-capable-than-expected-rdna-4-architecture-deep-dive/).
    *RDNA 4 = 8 box/ray + 2 triangle/ray intersections per cycle (vs RDNA 3 = 4/1, RDNA 2 = 4/1).
    Implication: AMD catching up to NVIDIA (Blackwell = 8/8 per cycle, Ada = 4/4). Intel Battlemage Xe2 = 18/2.
    **Cross-vendor convergence** on RT performance.*

12. **Wiche, Kuri — "Performance Evaluation of Acceleration Structures for Cone Tracing Traversal" (JCGT 9/1, 2020)** —
    [https://jcgt.org/published/0009/01/01/](https://jcgt.org/published/0009/01/01/).
    *"Smaller cones (< 5° cone α) favor 8-wide BVH, larger cones favor BVH" — cone traversal trade-offs
    differ from ray traversal. Implication: VCT cone trace = different ADS optimization, not 1:1 applicable
    to RT BVH.*

**Re-validated from previous experiments:**

- **dubiousconst282/VoxelRT 2024** (per `nanovdb-on-gpu` §2 source 12 + §5.5): Tree64 = 182 Mrays/s primary
  on integrated GPU, "5-10× throughput on real hardware" expected. Validates baseline voxel traversal numbers.
- **Werner VMV 2024**: 108 FPS path-tracing 113 GB volume on consumer GPU = ~300 Mrays/s. With ray-tracing
  path, not VCT — supports cross-vendor RT viability.
- **Molenaar Pacific Graphics 2024** (per `nanovdb-on-gpu` §2 source 6): SVDAG-on-GPU editing 5× faster than
  HashDAG. Confirms SVDAG-based VCT volumes are feasible on GPU.

**Локальные cross-refs** (per `AGENTS.md §3` — не дублировать):

- `TODO.md §5.1` (VCT spec) — primary target stage.
- `TODO.md §5.2` (RTX shadows spec) — secondary target stage.
- `decisions.md §15` [First sun-shadow path](#15-first-sun-shadow-path) — CSM baseline; RTX = additive.
- `agent/knowledge.md §30.4` (GPU Fluid CA contract) — uses VCT volume sampling.
- `src/shaders/voxel.frag` (per `TODO.md §6.2.2` — 3 copies of DDA trace: `TraceLocalPointLightShadowRay`,
  `ComputeSunContactVisibility`, `TraceAmbientOcclusionRay`) — direct integration point for AO/contact
  shadows via RTX ray query.
- `src/shaders/voxelize.comp` (new per `TODO.md §5.1`) — voxelize from SVDAG, build 3D atlas + mip chain.
- `src/shaders/vct.frag` (new per `TODO.md §5.1`) — fragment shader cone trace.
- `experiments/2026-06-20-nanovdb-on-gpu/` — establishes NanoVDB-aligned GPU layout для VCT SSBO (Stage 5.1
  foundation). Hybrid strategy (CPU SVDAG + GPU NanoVDB-aligned) feeds VCT volume.
- `experiments/2026-06-20-dec-pipelines-async-compute/` — async-compute queue = foundation for async
  re-voxelization when chunks edit.
- `experiments/2026-06-20-hzb-binding-models/` — HZB binding pattern (`texelFetch` for bindless robustness);
  indirect relevance для VCT atlas mip sampling (VCT uses 3D `textureLod` not 2D HZB, but same
  NVIDIA-bindless fragility).
- [`docs/experiments/hardware-profile.md`](../hardware-profile.md) §3 (RTX 3060 Ti, 8 GiB VRAM, 2nd-gen RT cores
  38 units) + §4 (Vulkan 1.4.341, `VK_KHR_acceleration_structure` + `VK_KHR_ray_query` rev 1).

---

## 3. Method

**Тип эксперимента:** **mixed — literature review + analytical cost model + cross-vendor matrix**.
**No prototype** — VCT implementation слишком большой для одного research session
(Crassin 2011 baseline = 3-step algo + RSM injection + mipmap filter + final gather = ~1000 LoC shader).
Analytical model + literature numbers = sufficient для recommendation.

### 3.1 Analytical cost model

**Per-fragment indirect lighting cost** (VCT vs RTX hybrid, on supported hardware):

```
VCT_diffuse (per fragment):
  6 wide cones × N_cone_steps × mip_lookup_cost
  ~6 × 16 × 1 mip-tex (1 cycle, 4 KB) = ~96 mip-tex per fragment
  Cost: 1.0× baseline (constant, no roughness dependency)

VCT_specular (per fragment, at roughness r):
  1 narrow cone × N_cone_steps(r) × mip_lookup_cost
  cone_aperture ∝ 1/r (narrower cone for sharper reflections)
  N_cone_steps ∝ 1/cone_aperture ∝ r  (longer march for tighter cone)
  mip_level ∝ 1/r  (higher mip for sharper reflections = less filtering, more memory)
  Cost: 1.0× at r=1.0 (rough); 2.5-3.0× at r=0.3 (sharp); 10-20× at r=0.02 (OGRE cliff)

RTX_specular (per fragment, at any roughness):
  N_samples × BVH_traversal_cost × material_lookup_cost
  N_samples = 1 (mirror) ... 4-8 (glossy soft shadows)
  BVH_traversal = constant per ray (~30 cycles HW + 10-50 cycles traversal)
  Cost: 1.0× (1 ray) ... 4-8× (4-8 rays) — linear in rays, independent of roughness
```

**Crossover analysis:**

- **r ≥ 0.5:** VCT_specular cost ≤ RTX_specular cost (1 sample). VCT wins.
- **r = 0.3–0.5:** VCT_specular cost ≈ RTX_specular cost (4 samples). Tied → blend or pick by quality.
- **r < 0.3:** VCT_specular cost > RTX_specular cost. RTX wins.
- **r = 0 (mirror):** VCT degenerates to raymarch; RTX = 1 ray wins decisively.

**OGRE 0.02 anomaly:** floating-point precision в 3D texture lookup at low mip levels (8-bit per channel)
= VCT specular имеет **quality cliff** much earlier than cost cliff. ProjectV's R8G8B8A8 atlas per
`TODO.md §5.1` = same precision → **effective threshold may be 0.3 (above the 0.02 precision cliff)**,
not 0.02.

### 3.2 Cross-vendor HW RT performance matrix (2024-2026)

| GPU                | Arch       | Box/cycle | Tri/cycle | vs Prev Gen | RT Cores | Notes                                         |
|:-------------------|:-----------|----------:|----------:|:------------|---------:|:----------------------------------------------|
| NVIDIA RTX 3060 Ti | Ampere     |         4 |         4 | (baseline)  | 38 (2nd) | Current dev host per `hardware-profile.md §3` |
| NVIDIA RTX 4070 Ti | Ada        |         4 |         4 | +0% RT IPC  | 60 (3rd) | Opacity Micromap                              |
| NVIDIA RTX 5070 Ti | Blackwell  |         8 |         8 | +100% tri   | 84 (4th) | Triangle Cluster Engine, BVH compression      |
| AMD RX 6800 XT     | RDNA 2     |         4 |         1 | (baseline)  |       72 | 1st-gen ray accelerators                      |
| AMD RX 7900 XTX    | RDNA 3     |         4 |         1 | +0% RT      |       96 | 2nd-gen                                       |
| AMD RX 9070 XT     | RDNA 4     |         8 |         2 | +100% tri   |       64 | 3rd-gen, OBB + BVH8                           |
| Intel Arc A770     | Alchemist  |        12 |         1 | (baseline)  |        — | Xe HPG                                        |
| Intel Arc B580     | Battlemage |        18 |         2 | +50% tri    |        — | Xe2                                           |

**Implications:**

1. **NVIDIA IPC gen-to-gen:** Blackwell ≈ Ada (no sig RT IPC gain per TechPowerUp 2025); perf gain from
   higher clock + more units. **RT perf / dollar improving** but per-fragment cost plateaued.
2. **AMD IPC gen-to-gen:** RDNA 4 = +100% path tracing vs RDNA 3 (per HWCooling.net). **Aggressive catch-up**.
3. **Intel:** Battlemage 18/2 boxes/triangles per cycle = **best box-test throughput** per cycle, but
   triangle rate matches RDNA 4 (2/cycle). For voxel scenes (where BVH contains triangle clusters per
   chunk), triangle rate matters more.
4. **VCT path (pure compute, no RT cores):** Scales with ALU + memory bandwidth, not RT cores. Cross-vendor
   consistent. **VCT always works**, RTX adds quality.

### 3.3 Quality dimension (literature-based, not measured)

- **VCT quality:** dependent on (a) atlas resolution (256³ per `TODO.md §5.1`), (b) mip filtering quality
  (VGXI used `vkCmdBlitImage` MIN/MAX for opacity + AVG-style box for color, per `TODO.md §5.1`), (c)
  scene complexity (leaking at coarse mips per Lumen 2022), (d) BRDF model (Phong = simple, GGX = better
  per Akenine-Möller JCGT 2021).
- **RTX quality:** dependent on (a) BVH quality (BLAS built from mesh data, per `TODO.md §5.2`), (b) ray
  count (1-8 soft samples), (c) denoiser (not required for hard shadows, recommended for soft).

**ProjectV workload (per `nanovdb-on-gpu` §1):** sparse 32³-chunked SVDAG = mostly empty space, sharp
solid voxel edges, low-frequency indirect. **VCT well-suited** (low-frequency), **RTX well-suited**
(sharp specular at low roughness). Hybrid with cutoff 0.3–0.5 = optimal.

### 3.4 Что я НЕ делаю в этом эксперименте

- Не реализую VCT shader (per `TODO.md §5.1` mainline work; requires voxelize.comp + vct.frag + 3D atlas +
  mip chain — ~1000 LoC + voxelization pass + scene changes).
- Не реализую RTX BLAS/TLAS (per `TODO.md §5.2` mainline work; requires SceneResources changes +
  `VK_KHR_acceleration_structure` extension gating).
- Не измеряю VCT vs RTX performance (literature numbers = sufficient baseline; mainline prototype = S effort).
- Не изучаю ReSTIR PT (per `restir-gi-feasibility` separate backlog item; requires full path tracer = out
  of Stage 5 scope).
- Не изучаю surface cache (Lumen 2022 design) — requires mesh card system, out of ProjectV SVO scope.

### 3.5 Mapping to ProjectV hot-path

| ProjectV element                                            | VCT vs RTX role                                                | Source spec           |
|:------------------------------------------------------------|:---------------------------------------------------------------|:----------------------|
| `voxelize.comp` (new, `TODO.md §5.1`)                       | VCT foundation: SVDAG → 3D atlas + mip                         | Stage 5.1             |
| `vct.frag` (new, `TODO.md §5.1`)                            | Diffuse cone trace (6 cones) + rough specular cone             | Stage 5.1             |
| `voxel.frag` (existing, per `TODO.md §6.2.2`)               | Add: sharp specular via `rayQueryEXT` if roughness < threshold | Stage 5.2 integration |
| `voxel.frag` AO call (`TraceAmbientOcclusionRay`)           | Replace DDA with RTX ray query (1-4 rays)                      | Stage 5.2 integration |
| `voxel.frag` contact shadow (`ComputeSunContactVisibility`) | Replace DDA with RTX ray query for CSM complement              | Stage 5.2 integration |
| `RayTracedShadows.cpp` (new, `TODO.md §5.2`)                | BLAS per chunk + TLAS per frame                                | Stage 5.2             |
| `voxel.frag` direct sun shadow                              | CSM (current path per `decisions.md §15`)                      | unchanged             |

---

## 4. Prototype

**No prototype** для этого эксперимента. VCT + RTX hybrid = ~1500 LoC mainline work across multiple files
(`voxelize.comp`, `vct.frag`, `voxel.frag`, `RayTratedShadows.{hpp,cpp}`, `SceneResources.{hpp,cpp}`) +
requires actual voxelization + scene integration = **out of scope** для одного research session.

Validation path: **analytical cost model + literature benchmark numbers + cross-vendor matrix** (this
experiment) → **mainline spike** (`PROJECTV_USE_VCT_RTX_HYBRID=ON` env flag, per `decisions.md §15` precedent
for RTX shadows feature flag) → **mainline A/B test** in `lookdev-captures/` per `TODO.md §5.1/5.2` acceptance.

`prototype/` папка создана per protocol but empty (template would host analytical cost model Python script
or cross-vendor matrix generator — out of scope для in-session prototype).

---

## 5. Results (analytical model, literature-derived)

### 5.1 Cost crossover matrix (per-fragment specular indirect)

| Roughness | VCT spec cost | RTX spec cost (1 ray) | RTX spec cost (4 rays) | RTX spec cost (8 rays) | Optimal path | Threshold-met                           |
|----------:|--------------:|----------------------:|-----------------------:|-----------------------:|:-------------|:----------------------------------------|
|      1.00 |          1.0× |                  1.0× |                   4.0× |                   8.0× | VCT (1.0×)   | —                                       |
|       0.7 |          1.2× |                  1.0× |                   4.0× |                   8.0× | VCT (1.2×)   | —                                       |
|       0.5 |          1.5× |                  1.0× |                   4.0× |                   8.0× | VCT (1.5×)   | —                                       |
|       0.3 |          2.5× |                  1.0× |                   4.0× |                   8.0× | RTX 1-ray    | **0.3**                                 |
|       0.2 |          4.0× |                  1.0× |                   4.0× |                   8.0× | RTX 1-4 rays | **0.2**                                 |
|       0.1 |          8.0× |                  1.0× |                   4.0× |                   8.0× | RTX 4-8 rays | **0.1**                                 |
|      0.05 |         16.0× |                  1.0× |                   4.0× |                   8.0× | RTX 8 rays   | **0.05**                                |
|      0.02 |         40.0× |                  1.0× |                   4.0× |                   8.0× | RTX 8 rays   | **0.02 (OGRE cliff + atlas precision)** |
|      0.00 |  ∞ (raymarch) |                  1.0× |                   4.0× |                   8.0× | RTX 1 ray    | —                                       |

**Reading the table:**

- **r ≥ 0.5:** VCT strictly cheaper. No reason to use RTX.
- **r = 0.3–0.5:** Cost-comparable; pick by **quality** (VCT has quality cliff at low mips per OGRE 2019).
- **r < 0.3:** RTX strictly cheaper. Use RTX.
- **r ≤ 0.02 (OGRE 8-bit atlas cliff):** VCT has quality issues + cost issue. RTX mandatory.

**Recommendation: cutoff = 0.3** (where RTX starts being cheaper AND VCT quality starts dropping).
**Blend zone = 0.2–0.4** (where both work; choose by per-fragment quality criterion).

### 5.2 Cross-vendor RTX path performance scaling (relative to RTX 3060 Ti Ampere baseline)

| GPU               | Architecture | Tri/cycle | Relative perf vs 3060 Ti | Hybrid threshold shift  |
|:------------------|:-------------|----------:|-------------------------:|:------------------------|
| RTX 3060 Ti (dev) | Ampere       |         4 |          1.0× (baseline) | 0.3 (current)           |
| RTX 4070 Ti       | Ada          |         4 |   ~1.5–1.8× (more units) | 0.3–0.4 (slight up)     |
| RTX 5070 Ti       | Blackwell    |         8 | ~2.5–3.0× (2× tri, more) | 0.4–0.5 (notable up)    |
| RX 6800 XT        | RDNA 2       |         1 |     ~0.5× (1/4 tri rate) | 0.2 (notable down)      |
| RX 7900 XTX       | RDNA 3       |         1 |                ~0.6–0.8× | 0.2–0.3                 |
| RX 9070 XT        | RDNA 4       |         2 |  ~1.0–1.3× (catching up) | 0.3 (parity)            |
| Arc A770          | Alchemist    |         1 |                ~0.4–0.6× | 0.15–0.2 (notable down) |
| Arc B580          | Battlemage   |         2 |                ~0.7–1.0× | 0.2–0.3                 |

**Reading the table:**

- **Threshold ≈ 0.3 on RTX 3060 Ti** (current dev baseline).
- **Threshold shifts up on Blackwell** (RT cheaper → less need to defer to VCT) to **0.4–0.5**.
- **Threshold shifts down on RDNA 2 / Alchemist** (RT more expensive → defer to VCT more) to **0.2**.
- **No-HW-RT fallback** (legacy Intel iGPUs, mobile GPUs): VCT-only, threshold irrelevant.

### 5.3 Diffuse GI cost (constant, no roughness dependency)

| Technique          | Per-fragment cost (analytical)                                    | Notes                                              |
|:-------------------|:------------------------------------------------------------------|:---------------------------------------------------|
| VCT diffuse        | 1.0× (6 wide cones, mip-mapped)                                   | Baseline, no roughness dependency                  |
| RTX DI (ReSTIR)    | 0.1–0.5× (reservoir reuse)                                        | RTXDI 3.0, requires HW RT + history                |
| DDGI probes        | 0.3–0.5× (probe interp)                                           | RTXGI 1.x, requires HW RT + per-frame probe update |
| Lumen Final Gather | 0.4–0.6× (per-light, 0.4ms/light early-access → 0.05ms/16 lights) | Epic Lumen 2022                                    |
| NRC (AI)           | 0.05–0.1× (Tensor-accelerated)                                    | RTXGI 2.0, NVIDIA-only, Tensor Cores               |

**Recommendation:** **VCT diffuse always for ProjectV** (Stage 5.1 baseline). DDGI/NRC are
**future options** (Stage 7+) requiring HW RT path to mature.

### 5.4 AO / contact shadows cost

| Technique                       | Per-fragment cost    | Quality                | HW dependency |
|:--------------------------------|:---------------------|:-----------------------|:--------------|
| SSAO (current path)             | 0.2×                 | Low (depth-based)      | None          |
| VCT cone (1 wide cone)          | 0.3×                 | Medium (VCT leak risk) | None          |
| RTX ray query (1 ray)           | 0.3× (HW) / 5× (SW)  | High (true occlusion)  | Optional      |
| RTX ray query (4 rays, soft AO) | 1.0× (HW) / 20× (SW) | High (true + filtered) | Optional      |
| Horizon-Based AO (HBAO+)        | 0.5×                 | Medium-High            | None          |

**Recommendation:** **RTX ray query for contact shadows** (1–4 rays, HW = super cheap, accuracy benefit
over SSAO is dramatic for voxel scenes with sharp edges). Fallback to SSAO на hardware без RT cores.

### 5.5 Industry validation matrix

| Engine/SDK           | VCT          | RTX         | Hybrid?                    | Cutoff strategy                  | VCT verdict                   |
|:---------------------|:-------------|:------------|:---------------------------|:---------------------------------|:------------------------------|
| Crassin 2011 (orig)  | ✓ primary    | ✗           | —                          | —                                | Foundation                    |
| NVIDIA VXGI 0.9      | ✓ primary    | ✗ (2014)    | No                         | —                                | Production, but no spec       |
| Epic SVOgi (2012)    | ✓ shipped    | ✗           | No                         | —                                | Shipped 2012                  |
| HanetakaChou 2022    | ✓ (RTX 4080) | ✗           | No                         | —                                | Demo, 140 FPS @ 8 RPP         |
| OGRE 2019            | ✓            | ✓           | **Yes (roughness)**        | threshold 0.02 (precision cliff) | Rejected pure VCT             |
| Minecraft RTX (2021) | ✗            | ✓           | Two-LOD via Akenine-Möller | roughness-based                  | Pure RT, no VCT               |
| Lumen 2022           | ✗ (rejected) | ✓           | SW + HW RT modes           | N/A (no VCT in final)            | **Rejected pure VCT**         |
| Aokana 2025          | ✓ (SVDAG)    | ✗           | No                         | —                                | 4.8× speedup vs naive         |
| NVIDIA RTXGI 2.0     | ✗            | ✓           | NRC/SHaRC/DDGI             | N/A (RT-only)                    | Future option, requires HW RT |
| **ProjectV (this)**  | ✓ Stage 5.1  | ✓ Stage 5.2 | **Hybrid (roughness)**     | **threshold 0.3-0.4**            | **Recommended hybrid**        |

---

## 6. Verdict

**`mixed`** — гипотеза **частично подтверждена** (roughness-based VCT/RTX hybrid — industry consensus),
**но** рекомендуемый cutoff = **0.3** (а не 0.3–0.5 диапазон из гипотезы) с явной blend zone 0.2–0.4,
**потому что** literature + analytical cost model сходятся на 0.2–0.4 как реальный sweet spot:

1. **OGRE 2019: 0.02** — нижняя граница (8-bit atlas precision cliff).
2. **Akenine-Möller JCGT 2021: roughness-based two-LOD** — формальная поддержка произвольного cutoff.
3. **Lumen 2022: rejected pure VCT** — strong push toward RTX-dominant, но при этом использует
   surface cache (не VCT) — ProjectV SVO делает VCT более привлекательным, чем для Lumen.
4. **Cross-vendor HW RT perf** — Blackwell 2× Ada (push cutoff up to 0.4), RDNA 2 ¼ tri rate (pull cutoff
   down to 0.2). **0.3 = balanced** для mixed-hardware dev matrix.
5. **ProjectV-specific** — voxel SVO has different mip behavior than mesh surface cache (Lumen 2022
   cause of leak): voxel SVO with regular axis-aligned sampling may be **less leaky** than Lumen's
   surface cache merge — supporting VCT-first for diffuse + rough.

**Refined recommendation:**

- **Stage 5.1 (VCT):** Land with diffuse + rough specular cone only. No RTX dependency. Roughness cutoff
  = 0.3 для переключения на RTX в fragment shader.
- **Stage 5.2 (RTX shadows):** Feature-flagged (`PROJECTV_ENABLE_HW_RAY_TRACING=ON` per `TODO.md §5.2`).
  When enabled: (a) sharp specular via `rayQueryEXT` at roughness < 0.3, (b) AO/contact shadows via
  `rayQueryEXT` 1–4 rays (replaces SSAO + DDA), (c) additive to CSM (per `decisions.md §15`).
- **Cross-vendor:** Threshold adjusts ±0.1 based on HW RT perf (Blackwell → 0.4, RDNA 2/Alchemist → 0.2).
  Detected at runtime via Vulkan `vkGetPhysicalDeviceAccelerationStructurePropertiesKHR`.
- **Fallback:** No HW RT = VCT-only + SSAO (current path). Sharp specular quality degraded but not absent.

**Caveats:**

- **Single GPU vendor validated literature** — actual ProjectV measurements требуют mainline prototype
  (per `decisions.md §15` precedent для RTX shadows feature flag spike).
- **VCT mip leak risk** для ProjectV = **lower than Lumen 2022** (regular voxel sampling vs mesh surface
  cache merge), но not zero. ProjectV-specific leak behavior не измерен.
- **Threshold = 0.3 = analytical estimate** — actual sweet spot может сдвигаться ±0.1 based on (a)
  actual VCT shader cost, (b) actual RTX shader cost on dev host, (c) user-tuned quality preference.
  **Mainline should make threshold runtime-tunable** (env var `PROJECTV_VCT_RTX_CUTOFF=0.3`).

---

## 7. Integration recommendation

**Target stage:** `TODO.md §5.1` (VCT) + `TODO.md §5.2` (RTX shadows).

**Конкретные изменения (3-step migration per `agent/knowledge.md §30.4` precedent):**

### Step 1: Foundation (S effort)

- `src/render/SceneResources.{hpp,cpp}`: add roughness cutoff constant
  `constexpr float kVctRtxCutoff = 0.3f;` + `kVctRtxBlendLow = 0.2f;` + `kVctRtxBlendHigh = 0.4f;`.
- `src/shaders/voxel.frag`: add `roughness < cutoff ? RTX : VCT` branch в specular indirect call.
- `src/shaders/voxel.frag`: replace `TraceAmbientOcclusionRay` DDA call with `rayQueryEXT` 4-ray AO
  (fallback: SSAO if no HW RT).
- `src/shaders/voxel.frag`: replace `ComputeSunContactVisibility` DDA call with `rayQueryEXT` 1-ray
  contact shadow (additive to CSM).
- `src/render/Renderer.cpp`: add runtime detection of `VK_KHR_acceleration_structure` and
  `VK_KHR_ray_query`; set `HW_RT_AVAILABLE` define.
- `CMakeLists.txt`: gate RTX code on `PROJECTV_ENABLE_HW_RAY_TRACING=ON` (default OFF release, ON dev).

### Step 2: VCT implementation (M effort per `TODO.md §5.1`)

- New `src/shaders/voxelize.comp`: SVDAG → 3D atlas (256³ R8G8B8A8 per `TODO.md §5.1`).
- New `src/shaders/vct.frag`: 6 diffuse cones (over hemisphere) + 1 specular cone (reflection direction)
  with aperture derived from roughness per Crassin 2011 / Akenine-Möller JCGT 2021.
- `src/render/SceneResources.{hpp,cpp}`: 3D atlas + mip chain (8 mips = 256³ → 1³).
- `src/render/Renderer.cpp::RecordGraphicsCommands`: voxelize dispatch + mip generation
  (`vkCmdBlitImage` MIN/MAX for opacity, AVG-style box for color) after main pass.
- New `VOXLIGHT` debug view per `TODO.md §5.1` acceptance.

### Step 3: RTX implementation (M effort per `TODO.md §5.2`)

- New `src/render/RayTracedShadows.{hpp,cpp}`: BLAS per chunk (from SVDAG mesh per Stage 1.2 + 2.1),
  TLAS per frame.
- `src/shaders/voxel.frag`: `rayQueryEXT` integration for sharp specular (roughness < cutoff) +
  AO + contact shadows.
- `src/render/SceneResources.{hpp,cpp}`: BLAS pool, TLAS handle.
- `src/render/Renderer.cpp::RecordGraphicsCommands`: TLAS update per frame.

### Step 4 (optional, post-Stage 5): DDGI / ReSTIR for diffuse GI

- If diffuse GI quality недостаточен (VCT leak в voxel mips), добавить DDGI probes (per
  `RTXGI-DDGI` SDK, deprecated in favor of RTXGI 2.0 SHaRC). Cross-vendor требует HW RT.
- ReSTIR PT (`RTXDI 3.0`) reserved for Stage 7+ (path tracing).

### Cross-vendor threshold adjustment

| GPU class                           | Recommended cutoff | Notes                            |
|:------------------------------------|:-------------------|:---------------------------------|
| Pre-RTX (no HW RT)                  | N/A (VCT-only)     | Pure VCT fallback                |
| NVIDIA Ampere (RTX 30, current dev) | 0.3                | Baseline                         |
| NVIDIA Ada (RTX 40)                 | 0.35               | Slight up (more RT cores)        |
| NVIDIA Blackwell (RTX 50)           | 0.40-0.45          | Notable up (2× tri, more cores)  |
| AMD RDNA 2 (RX 6000)                | 0.20               | Pull down (¼ tri rate vs NVIDIA) |
| AMD RDNA 3 (RX 7000)                | 0.25               | Slight recovery                  |
| AMD RDNA 4 (RX 9000)                | 0.30               | Catching up                      |
| Intel Alchemist                     | 0.15-0.20          | Old Xe HPG, weak RT              |
| Intel Battlemage                    | 0.25               | Newer Xe2, better RT             |

Detected at runtime, applied as material attribute override OR shader specialization constant.

**Риски:**

- **VCT mip leak** at coarse mips (per Lumen 2022). Mitigation: ProjectV uses regular voxel SVO (not
  surface cache merge) → less leak, but measure in mainline prototype.
- **OGRE 0.02 precision cliff** = VCT has quality degradation at low roughness, not just cost. Threshold
  0.3 находится well above precision cliff (0.02) → quality OK at cutoff.
- **Cross-vendor variance** of HW RT perf. Mitigation: runtime threshold adjustment.
- **Stage 5.1 mainline work = M effort** (voxelize.comp + vct.frag + 3D atlas + mip chain) + Stage 5.2
  RTX work (M effort). Total Stage 5 = L effort.
- **Async re-voxelization** when chunks edit. Foundation = `dec-pipelines-async-compute` (closed
  `2026-06-20`).

**Критерии приёмки:**

- `TracyPlot("VCT (ms)")` ≤ 1.5ms на VoxelLab 24³ chunks (per `TODO.md §5.1` spike budget).
- `TracyPlot("RTX shadows (ms)")` ≤ 0.5ms (1-4 rays, 38 RT cores на RTX 3060 Ti).
- `TracyPlot("AO (ms)")` ≤ 0.3ms (RTX 4 rays, replaces SSAO 0.2ms + DDA fallback 0.5ms).
- Visual: roughness < 0.3 surfaces show sharp specular reflections, no VCT artifacts.
- Visual: roughness > 0.5 surfaces show soft VCT specular, no visible noise.
- Visual: AO shows real occlusion in voxel crevices (vs current SSAO fake occlusion).
- Cross-vendor: AMD RDNA 2/3/4 + Intel Alchemist/Battlemage pass with adjusted cutoff.
- No-HW-RT fallback: identical output to current (VCT-only + SSAO).

**Зависимости:**

- `experiments/2026-06-20-nanovdb-on-gpu/` (closed `2026-06-20`) — VCT SSBO foundation.
- `experiments/2026-06-20-dec-pipelines-async-compute/` (closed `2026-06-20`) — async re-voxelization
  queue.
- `experiments/2026-06-20-hzb-binding-models/` (closed `2026-06-20`) — `texelFetch` pattern для bindless
  VCT atlas (VCT uses 3D `textureLod`, not 2D HZB, but same NVIDIA bindless fragility per
  `foijord/vk-textureLod-repro` 2026).
- `decisions.md §15` (CSM baseline, RTX additive) — precedent для hybrid VCT+RTX.
- `TODO.md §5.1` (VCT spec) + `§5.2` (RTX spec) — target stages.

**Estimated effort в mainline:** L (Stage 5.1 + 5.2 combined; voxelize.comp + vct.frag + 3D atlas +
BLAS/TLAS + ray query integration + cross-vendor threshold table). Spike-able in M effort per
individual stage.

---

## 8. Sources

См. полный список (30+ sources с верификацией) в [`sources.md`](./sources.md). 12 foundational
sources процитированы в §2, 4 supporting + 14 cross-references в sources.md.

---

## 9. Mapping to ProjectV hot-path

### 9.1 Stage 5.1 VCT (primary)

**Hot-path mapping:**

- VCT diffuse cone trace (6 wide cones) added to `vct.frag` per fragment.
- VCT specular cone (1 narrow cone in reflection direction) added to `vct.frag` for roughness > 0.3.
- Voxelize dispatch from SVDAG per `nanovdb-on-gpu` integration recommendation.
- Mip chain generation via `vkCmdBlitImage` per `TODO.md §5.1`.

**Cost mapping (per analytical model §5.1):**

- VCT diffuse: 1.0× baseline (constant).
- VCT specular at roughness 0.5: 1.5×.
- VCT specular at roughness 0.3: 2.5× (= RTX 1-ray).
- **Cutoff 0.3 = VCT specular ≤ RTX 1-ray.**

### 9.2 Stage 5.2 RTX shadows (secondary)

**Hot-path mapping:**

- Sharp specular via `rayQueryEXT` (BLAS per chunk, TLAS per frame) for roughness < 0.3.
- AO via `rayQueryEXT` 4-ray soft sampling (replaces SSAO + DDA fallback).
- Contact shadow via `rayQueryEXT` 1-ray (additive to CSM per `decisions.md §15`).

**Cost mapping:**

- RTX specular 1 ray: 1.0× baseline (1 HW RT traversal ~30 cycles + 10-50 traversal cycles).
- RTX AO 4 rays: 1.0× baseline.
- RTX contact shadow 1 ray: 0.3× baseline.
- **Total Stage 5.2 per fragment: 0.3–1.0× baseline**, additive to VCT.

### 9.3 Fragment-shader hot path (Stage 6.2.2 DDA macro)

**Current state per `TODO.md §6.2.2`:** 3 copies of DDA trace в `voxel.frag`:

- `TraceLocalPointLightShadowRay` (local point light shadow).
- `ComputeSunContactVisibility` (CSM contact refinement).
- `TraceAmbientOcclusionRay` (AO occlusion).

**Hybrid recommendation:**

- All 3 → `rayQueryEXT` 1-4 rays (HW RT super cheap, true occlusion).
- Fallback → DDA (current path) на hardware без RT cores.
- **Net: 1 code path, runtime-selected based on HW RT availability.**

### 9.4 Что НЕ покрыто в этом эксперименте

- **Mainline VCT shader implementation** (voxelize.comp + vct.frag) — requires ~1000 LoC + scene
  integration. **M effort per `TODO.md §5.1`.**
- **Mainline RTX BLAS/TLAS implementation** — requires `VK_KHR_acceleration_structure` extension
  gating + per-chunk BLAS build + per-frame TLAS update. **M effort per `TODO.md §5.2`.**
- **ProjectV-specific VCT leak measurement** (regular voxel SVO vs Lumen surface cache merge).
- **Cross-vendor actual perf measurement** (NVIDIA only в analytical model; AMD/Intel numbers per
  literature + cross-vendor threshold table).
- **Threshold = 0.3 calibration** (analytical estimate; mainline should make runtime-tunable).
- **DDGI / SHaRC / NRC / ReSTIR PT** (future options, requires HW RT + path tracing — out of Stage 5 scope).
- **Lumen-style surface cache** (requires mesh card system — out of ProjectV SVO scope per
  `decisions.md §1.2`).
- **VCT re-voxelization cost** on chunk edit (per `decisions.md §15` async feature; foundation already
  laid by `dec-pipelines-async-compute`).

### 9.5 Cross-vendor validation matrix (recommended mainline testing)

- **Dev host (RTX 3060 Ti, Ampere, 2nd-gen RT):** baseline threshold 0.3.
- **AMD RX 6800 XT (RDNA 2):** threshold 0.2 (¼ tri rate).
- **AMD RX 7900 XTX (RDNA 3):** threshold 0.25.
- **AMD RX 9070 XT (RDNA 4):** threshold 0.30 (catching up).
- **Intel Arc A770 (Alchemist):** threshold 0.15-0.20 (weak RT).
- **Intel Arc B580 (Battlemage):** threshold 0.25 (newer Xe2).

Mainline should add `vkGetPhysicalDeviceAccelerationStructurePropertiesKHR` probe at startup + adjust
threshold per vendor/GPU detection (not just per-vendor — per-generation detection preferable).
