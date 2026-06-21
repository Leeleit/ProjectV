# Sources — `2026-06-21-god-rays-crepuscular`

**Total verified sources:** 11 primary + 3 secondary (web-search через Exa, успешно на этой сессии).

## Tier 1 — Canonical / Production

### 1. Kenny Mitchell, "Volumetric Light Scattering as a Post-Process", GPU Gems 3 Ch 13 (2008, NVIDIA)

- **URL:** https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering-post-process
- **Year:** 2008 (originally published online 2007)
- **Author:** Kenny Mitchell, Electronic Arts (DICE/Battlefield team)
- **Why important:** **Foundational production-grade paper** для screen-space radial blur approach (B_ScreenSpaceRadialBlur).
  Analytic daylight model + per-pixel radial blur post-process. Validates that no preprocessing or scene
  setup required. Cites Radomir Mech 2001, James 2003/2004 (depth peeling), Mitchell 2004 (shadow maps),
  Hoffman & Preetham 2003 (homogeneous media scattering).
- **Adopted strategy:** B_ScreenSpaceRadialBlur baseline formula.

### 2. Crytek, "Crysis Next-Gen Effects" (GDC 2008)

- **URL:** https://www.slideshare.net/slideshow/crysis-nextgen-effects-gdc-2008/25051981
- **Year:** 2008 (slide 63-66 canonical sun shafts algorithm)
- **Author:** Crytek (Cevat Yerli / Tiago Sousa / Martin Mittring team)
- **Why important:** **Production implementation** of screen-space sun shafts в Crysis 1.
  Algorithm: depth mask + radial blur + sun screen-space position + 3 passes = 512 samples.
  Iterative quality improvement (8/64/512 samples). Compose via additive blending.
  Known limitations: "screen edges problematic" — strength attenuated based on view angle.
- **Adopted strategy:** B_ScreenSpaceRadialBlur (Crytek variant).

### 3. Egor Yusov, "High Performance Outdoor Light Scattering Using Epipolar Sampling", GPU Pro 5 Ch 28-33 (2014)

- **URL:** https://www.oreilly.com/library/view/gpu-pro-5/9781482208641/chapter-28.html
- **Year:** 2014 (GDC 2013 presentation precursor)
- **Author:** Egor Yusov
- **Why important:** **SOTA screen-space outdoor light scattering** (god rays / atmospheric).
  Uses epipolar sampling + 1D Min/Max binary trees. Quality table at Intel HD Graphics 5000 (1280×720):
  brute force 209.6 ms, high quality 23.6 ms, balanced 10.35 ms, high performance 6.19 ms.
  4 quality presets × cascade slicing × 1D min/max trees for shadow lookups.
- **Adopted strategy:** Inspiration для C_AnalyticOccludedRayMarch (epipolar-optimized).

### 4. Nathan Vos, "Volumetric Light Effects in Killzone: Shadow Fall", GPU Pro 5 Ch 38 (2014)

- **URL:** https://www.oreilly.com/library/view/gpu-pro-5/9781482208641/chapter-38.html
- **Year:** 2014 (Killzone: Shadow Fall production)
- **Author:** Nathan Vos, Guerrilla Games
- **Why important:** **Production implementation** для PS4 launch title.
  Algorithm: low-resolution rendering + dithered ray marching + scattering control + transparent objects.
  Section 3.3-3.7 covers optimization tricks (low-res base, separable bilateral filter per Pham/van Vliet 2005,
  adaptive volume shadow maps per Salvi 2011).
- **Adopted strategy:** Inspiration для C (low-resolution base pattern).

### 5. Sébastien Hillaire et al., "Towards Unified and Physically-Based Volumetric Lighting in Frostbite", SIGGRAPH 2015 Advances

- **URL:** https://advances.realtimerendering.com/s2015/index.html
- **Year:** 2015
- **Author:** Sébastien Hillaire (Frostbite/FIFA/Mass Effect)
- **Why important:** Production framework for Frostbite volumetric lighting: cascaded extinction volume,
  voxelization projection, volumetric shadow map, physically based parameters.
  Adopted by all subsequent Frostbite titles (FIFA, Star Wars Battlefront, Anthem, Battlefield).
- **Adopted strategy:** Inspiration для E_HybridRadialBlurPlusVolumetric (B + volumetric fog integration).

### 6. Krzysztof Wright et al., "Lumen — Hybrid Ray Tracing Pipeline", SIGGRAPH 2022

- **URL:** https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf
- **Year:** 2022 (UE5 release)
- **Author:** Krzysztof Wright + Krzysztof Narkowicz + team, Epic Games
- **Why important:** **SOTA hybrid ray tracing pipeline.** Pattern: Screen Tracing → Software RT → Hardware RT
  → Skylight (cascade handoff via ray state continuation). Production-proven для Unreal Engine 5.
  Supports hardware RT path OR software RT path OR hybrid. Critical insight: "Hardware Ray Tracing is great
  and is the future, but we need options to scale down."
- **Adopted strategy:** D_VolumetricConeTraceRayQuery follows Lumen hardware RT handoff pattern;
  E_HybridRadialBlurPlusVolumetric follows Screen Tracing → Hardware RT handoff.

### 7. Krzysztof Narkowicz, "Journey to Lumen", Personal Blog (Aug 2022)

- **URL:** https://knarkowicz.wordpress.com/2022/08/18/journey-to-lumen/
- **Year:** 2022
- **Author:** Krzysztof Narkowicz (Lumen lead, Epic Games)
- **Why important:** **Insider retrospective** на Lumen design evolution.
  Concept: "trace continuation" = Screen Tracing first (most accurate near ray start),
  handoff to Software RT / Hardware RT via ray state.
  Hardware RT performance: still bounded by GPU RT throughput (1-2 rays/pixel на Ampere).
- **Adopted strategy:** Validates D_VolumetricConeTraceRayQuery RTX ray query choice.

## Tier 2 — Open-Source / Reference

### 8. super-shaman/crepuscular-rays-Unity (GitHub, 2019)

- **URL:** https://github.com/super-shaman/crepuscular-rays-Unity
- **Year:** 2019 (created 2019-02-12)
- **Author:** super-shaman
- **Why important:** **Working open-source implementation** of GPU Gems 3 Ch 13 in Unity.
  12 stars, 2 forks. Pure ShaderLab + C#. Direct port of Mitchell 2007 algorithm.
- **Adopted strategy:** Reference implementation для B_ScreenSpaceRadialBlur.

### 9. Merlin3d, "Crepuscular Rays in Screen Space", Blog (2012)

- **URL:** https://merlin3d.wordpress.com/2012/04/20/crepuscular-rays-in-screen-space/
- **Year:** 2012-04-20
- **Author:** Merlin3d (engine developer)
- **Why important:** **Practical blog implementation** of GPU Gems 3 Ch 13 в custom engine.
  Notes: "two new built-in shader parameters: sun screen-space position + camera-direction-to-sun angle
  attenuation for off-screen sun handling." Validates B algorithm stability в custom engine integration.
- **Adopted strategy:** Validates B off-screen sun handling.

### 10. Paul Scharf, "Localised Crepuscular Rays", .NET Code Geeks (2015)

- **URL:** https://www.dotnetcodegeeks.com/2015/08/localised-crepuscular-rays.html
- **Year:** 2015-08-19
- **Author:** Paul Scharf (NCG partner, Roche Fusion developer)
- **Why important:** **Detailed algorithm + code walkthrough** для localised crepuscular rays.
  Steps: (1) shoot ray toward light, (2) sample scene render target along ray, (3) weighted combine,
  (4) additive blend. Used in Roche Fusion for explosion god rays. Validates per-pixel approach.
- **Adopted strategy:** Validates B per-pixel sample loop.

## Tier 3 — Supplementary

### 11. Unreal Engine 5 Lumen Official Tech Blog (May 2022)

- **URL:** https://www.unrealengine.com/en-US/tech-blog/unreal-engine-5-goes-all-in-on-dynamic-global-illumination-with-lumen
- **Year:** 2022-05-27
- **Author:** Unreal Engine team
- **Why important:** **Official documentation** для Lumen features.
  Software RT vs Hardware RT tradeoffs: "Hardware ray tracing is more accurate but more expensive.
  Software ray tracing is faster, supports older GPUs."
  "Lumen completely replaces screen space and ray trace reflections" when enabled.
- **Adopted strategy:** Validates per-platform tier matrix (software RT = no-HW-RT fallback).

### 12. Unreal Engine 5 "Lumen in UE5: Let there be light" (YouTube, 2021)

- **URL:** https://www.youtube.com/watch?v=Dc1PPYl2uxA
- **Year:** 2021-08-10
- **Author:** Unreal Engine (official)
- **Why important:** Official video walkthrough of Lumen hybrid pipeline.
  Confirms "screen trace rays" → "software ray trace" → "hardware ray trace" → "skylight" cascade.
- **Adopted strategy:** Validates E_Hybrid cascade pattern.

### 13. Frostbite PBR Sky+Clouds (Hillaire 2016, EA)

- **URL:** https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
- **Year:** 2016
- **Author:** Sébastien Hillaire et al., EA
- **Why important:** Production-grade sky+clouds framework. Volumetric scattering + Eyring
  analytical model + Bouthors 2008 reference + Schneider 2015 reference.
- **Adopted strategy:** Inspiration для analytical scattering in D/E.

---

## Sources NOT directly used (out of scope / weaker relevance)

- **Nishita 1987/2006** "A Fast Rendering Method for Shafts of Light in Outdoor Scene" — academic
  reference, less practical for voxel engines.
- **Dobashi 2002** "A quick rendering method using basis functions for interactive lighting design" —
  slice-based volumetric, superseded by post-process approach.
- **James 2003/2004** depth peeling polygonal volumes — superseded by post-process.
- **Hoffman & Preetham 2003** GPU shader for light scattering in homogeneous media — superseded
  by Mitchell 2007 volumetric shadows extension.
- **Karras 1997** radial blur demo — historical only.

---

## Cross-references в mainline

- **TODO.md §5** (Stage 5 Visual Polish — god rays **not currently planned** — это новая ось, deferred).
- **`agent/knowledge.md §30.4`** 3-step migration precedent (Step 1 foundation + Step 2 strategy +
  Step 3 default flip + Tracy plot + test target).
- **`docs/experiments/experiments/2026-06-21-volumetric-fog-atmosphere-rendering/`** (closed mixed) —
  complementary axis (fog = near-atmospheric scattering, god rays = directional sun shafts через occluders).
- **`docs/experiments/experiments/2026-06-20-rt-shadows-vs-csm/`** (closed mixed) — sun shadow
  contribution to shafts.
- **`docs/experiments/experiments/2026-06-20-vct-vs-rt-cutoff/`** (closed mixed) — RTX cutoff policy
  для cone trace.
- **`docs/experiments/experiments/2026-06-21-rtx-screen-space-reflections/`** (closed mixed) — similar
  hybrid RTX pattern via ray query.
- **`docs/experiments/hardware-profile.md §3/§4`** — RTX 3060 Ti + `VK_KHR_ray_query` rev 1 + Vulkan 1.4.341.
- **`docs/experiments/benchmarks/methodology.md §3`** — measurement protocol (1000 iter + 10 warmup).
- **`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`** — 5-10% threshold.
- **`legacy/docs/philosophy/03_domain/04_testing-philosophy.md`** — покрытие тестами.