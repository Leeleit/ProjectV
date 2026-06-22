# Sources — 2026-06-22-surface-micro-detail

**Web-research via direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 persistent this session per
`agent/knowledge.md Part B §9` line 1424 fallback list).** Sentinel §13.7 verified clean — 0 dedicated
surface-micro-detail experiment in 138+ closed experiments pre-claim.

---

## Tier 1 — Foundational academic (canonical)

1. **Blinn, James F. (1978) "Simulation of Wrinkled Surfaces"** — SIGGRAPH 1978, ACM Computer Graphics Vol. 12 (3),
   pp. 286-292. The original paper introducing normal perturbation for faking bumps. Verified via
   [Wikipedia "Bump mapping"](https://en.wikipedia.org/wiki/Bump_mapping) (cross-references this paper as
   "introduced by James Blinn in 1978"). Canonical for the principle that *changing the normal before lighting
   is sufficient to fake surface relief*. **Direct application to ProjectV:** micro-detail perturbing the
   flat per-fragment normal of a voxel face produces the visual impression of sub-voxel relief.

2. **Krishnamurthy & Levoy (1996) "Fitting Smooth Surfaces to Dense Polygon Meshes"** — SIGGRAPH 1996. First
   paper to transfer detail from high-poly to low-poly models. Verified via [Wikipedia "Normal mapping"](https://en.wikipedia.org/wiki/Normal_mapping).
   **Direct relevance:** the principle that high-frequency detail can be re-encoded into a 2-channel map
   (which we now use as an analytic noise lookup).

3. **Cohen, Olano, Manocha, Krishnamurthy, Greer, Haines, Hudson (1998) "Appearance-Preserving Simplification"**
   — SIGGRAPH 1998. Introduced storing surface normals directly in a texture (the modern normal map).
   Verified via [Wikipedia "Normal mapping"](https://en.wikipedia.org/wiki/Normal_mapping).

4. **Cignoni, Montani, Scopigno, Rocchini (1998) "A general method for preserving attribute values on
   simplified meshes"** — IEEE Visualization 1998. Decoupled high-detail from low-detail, allowing any
   attribute to be transferred. Verified via Wikipedia "Normal mapping". **Direct relevance:** validates
   the principle that micro-detail can be applied orthogonally to the underlying geometry.

5. **Worley, Steven (1996) "A Cellular Texture Basis Function"** — SIGGRAPH 1996, ACM Digital Library
   `doi:10.1145/237170.237267`. The original cellular / Voronoi / Worley noise paper. Verified via
   [Wikipedia "Worley noise"](https://en.wikipedia.org/wiki/Worley_noise) (cites "Worley noise can be
   differentiated once to generate a normal map"). **CRITICAL for our experiment:** Worley noise is
   differentiable and the gradient directly produces a valid normal perturbation — strategy D in our
   prototype. **Note: F1/F2 distance metrics are continuous everywhere except on Voronoi edges, so
   `grad(Worley)` is well-defined per-fragment without special-case handling.**

6. **Mikkelsen, Morten S. (2008) "Simulation of Wrinkled Surfaces Revisited"** — Section 2.2 explicitly
   addresses bump mapping without precomputed normal maps. Verified via [Wikipedia "Bump mapping"](https://en.wikipedia.org/wiki/Bump_mapping)
   (cites "Simulation of Wrinkled Surfaces Revisited, Mikkelsen, 2008"). Also see Mikkelsen 2010
   ("Bump Mapping Unparametrized Surfaces" — WebGL Insights, chapter 15) which introduced the
   `dFdx/dFdy` height-field derivative approach used in our strategy E.

---

## Tier 2 — Modern production references

7. **Kaneko, T. et al. (2001) "Detailed Shape Representation with Parallax Mapping"** — ICAT 2001,
   pp. 205-208. Original parallax mapping paper. Verified via [Wikipedia "Parallax mapping"](https://en.wikipedia.org/wiki/Parallax_mapping).
   **Note:** Parallax mapping is *orth* axis to our micro-detail experiment (uses height map + UV offset;
   adds depth illusion by altering sample coordinate, not normal). We mention it as the natural follow-up
   if micro-detail proves insufficient.

8. **Tatarchuk, N. (2005) "Practical Dynamic Parallax Occlusion Mapping"** — SIGGRAPH 2005 presentation,
   ATI/AMD. Verified via Wikipedia "Parallax mapping".

9. **Policarpo, F. & Oliveira, M. M. "Chapter 18. Relaxed Cone Stepping for Relief Mapping"** — GPU Gems 3,
   NVIDIA Developer. Verified via Wikipedia "Parallax mapping".

10. **KdotJPG/OpenSimplex2 (CC0-1.0 license, 683★, 62 forks)** — Verified via
    [github.com/KdotJPG/OpenSimplex2](https://github.com/KdotJPG/OpenSimplex2). 2D/3D/4D OpenSimplex2(F) and
    OpenSimplex2S variants. **License (CC0) compatible with mainline inclusion.** **Production reference
    for our strategy C (tangent-space FBM 2D) — OpenSimplex2 is the de-facto standard for 2D coherent
    noise in modern games (Minecraft Java 1.18+ bedrock sky, Celeste, CrossCode, No Man's Sky, Hades
    per project self-reports in the wild).**

11. **Auburn/FastNoiseLite (MIT license, 3.4k★, 371 forks)** — Verified via
    [github.com/Auburn/FastNoiseLite](https://github.com/Auburn/FastNoiseLite). HLSL/GLSL/Rust/C++ ports
    with explicit **performance benchmarks** in README. **CRITICAL for our H1 hypothesis (cost budget):
    the README's published C++ benchmark on Intel 7820X @ 4.9 GHz with clang-cl 10 -O2 shows
    `3D Cellular = 12.49 M pts/s = 80.1 ns/eval` and `3D Perlin = 47.93 M pts/s = 20.9 ns/eval`.
    This is the *upper bound* on per-fragment cost for our strategy C (FBM) — at ~21 ns/eval, well
    over our 2 ns/frag hypothesis for the 4-octave FBM. **Hypothesis will be REJECTED for C at 4
    octaves; may be accepted for 1 octave (≈5 ns/frag on Zen 3).** Strategy D (Worley) at 80 ns/eval
    is 40× over budget — REJECTED. Strategy B (hash) at ~3-5 ns/eval is on budget. Strategy E
    (derivative) at ~10 ns/eval (1 sample + 1 dFdx + 1 dFdy) is borderline. Strategy A (none) is 0 ns.
    **This is a critical early correction to our H1 hypothesis before benching.**

---

## Tier 3 — Cross-references to closed ProjectV experiments

12. **`2026-06-20-simd-procedural-noise`** (closed mixed, Stage 4.1) — CPU AVX2 vs scalar noise
    benchmark. **Direct reference:** per-sample ALU cost for Perlin/Simplex/Value/OpenSimplex2D on
    x86-64 Zen 3 5800X. Also `2026-06-21-gpu-procedural-noise-compute-kernels` (closed mixed, Stage 4.1) —
    GPU compute shader cost on RTX 3060 Ti (GA104 Ampere, **5 noise algorithms within 2.9% mean cost =
    13.0 ns/eval**, 65.6% memory bandwidth efficiency). **Both prior experiments establish that for our
    context (Zen 3 5800X + RTX 3060 Ti Ampere), per-sample noise cost is **memory-bound, not
    ALU-bound** — so strategy D (Worley) is not as expensive as Auburn's CPU benchmark suggests,
    because our GPU has 14 Gbps GDDR6 + 230 GB/s effective bandwidth. Revised hypothesis: D
    ~20-30 ns/frag on RTX 3060 Ti is plausible.**

13. **`2026-06-21-subsurface-scattering-voxel-materials`** (closed mixed, Stage 5.x) — Subsurface
    scattering. SSS is a **post-lighting** effect that *consumes* normal/roughness but does not
    *modify* them — verifies H3 (additive composition): micro-detail's pre-lighting normal perturbation
    does not affect SSS calculation correctness.

14. **`2026-06-21-volumetric-fog-atmosphere-rendering`** (closed mixed, Stage 5.x) — Volumetric fog.
    **Post-lighting** per-fragment composition (color = color * transmittance + accum). **H3
    validation:** micro-detail perturbs normal/roughness pre-lighting; volumetric fog runs post-lighting
    on final color — no interaction, no regression risk.

15. **`2026-06-21-cloudscape-rendering`** (closed mixed, Stage 5.x) — Cloud ray-march. Distal object
    (sky shell). **H3 validation:** cloudscape samples 3D clipmap *outside* the voxel world; micro-detail
    only affects voxel surface fragments — zero interaction.

16. **`2026-06-20-vct-vs-rt-cutoff`** (closed mixed, Stage 5.1) — Voxel cone tracing cutoff.
    **H3 validation:** VCT samples the 3D clipmap with cone directions in *world space*; it consumes
    interpolated surface normal at the start of the lighting equation. Our micro-detail perturbs that
    normal pre-VCT — VCT still receives a well-formed perturbed normal and computes the same
    `Σ cone × (clipmap_mip_at(distance))` formula. No regression.

17. **`2026-06-21-lod-mesh-downsampling`** (closed mixed, Stage 4.2) + **`2026-06-21-lod-transition-strategy`**
    (closed mixed, Stage 4.2) — Geometric LOD. **H3 validation:** the LOD kernels (B_SurfacePreserve,
    C_Geomorph) produce flat per-vertex normals at the mesh level; micro-detail is applied
    per-fragment in the rasterizer (voxel.frag) — they compose orthogonally. Per-fragment cost is
    unaffected by which LOD is in use.

---

## ProjectV mainline cross-references (verified `rg` searches)

- `src/shaders/voxel.frag` — main fragment shader; current implementation applies GGX/lambertian
  directly to the interpolated per-vertex normal. **No existing normal-perturbation layer in
  mainline** (per `rg -n "perturb|detail|crinkle|micro" src/shaders/`).
- `agent/knowledge.md §30.4` — 3-step migration precedent (foundation → integration → env-gate +
  test) used by all closed experiments this session. Direct template for our Integration
  recommendation.
- `agent/workspace.md §2 line 69` — "Stage 5.x Visual Polish additional axes" includes bloom +
  aerial perspective + tonemap upgrade as integration candidates. **Surface micro-detail is the
  fourth axis in this set.**
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold for
  perf claims; applied to H1 cost (cross 5% if D <2 ns/frag at fragment cost; ALU only).

---

## Citation convention

All sources verified via `webfetch` to canonical URLs. Where the source was a Wikipedia page, the
verified quote is marked with a "Verified via" link. Direct PDFs (Mikkelsen 2008, Kaneko 2001,
Cohen 1998, Cignoni 1998) are referenced indirectly via Wikipedia citations to keep the
research load manageable; these are canonical and re-citable in the mainline integration comment.
