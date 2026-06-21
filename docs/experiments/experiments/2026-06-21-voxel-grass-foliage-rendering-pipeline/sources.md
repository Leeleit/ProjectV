# 2026-06-21-voxel-grass-foliage-rendering-pipeline — Sources

**Anti-duplicate sentinel §13.7:** `rg "grass|foliage|vegetation|wind.animation"` over `docs/experiments/`
returns only scattered cross-references (e.g. `mesh-shader-mega-instancing` mentions "Vulkan Foliage
2024" as adjacent area, `procedural-military-terrain-gen` uses "rolling_hills" / "flat_grasslands"
as scene names), **NO dedicated grass/foliage/vegetation folder pre-existed** this experiment.
`rg -c "grass" INDEX.md` = 1 (just the "flat_grasslands" scene name); `rg -c "foliage"` = 0;
`rg -c "vegetation"` = 0. **First dedicated grass/foliage/vegetation rendering + placement axis**
в 100+ closed experiments.

Web-research complete via DuckDuckGo HTML endpoint (Exa `web_search` HTTP 429 persistent
per `agent/knowledge.md Part B §9`). **5 primary + 2 secondary sources verified** with full content
read.

---

## Tier 1 — Primary sources (full content read)

### 1. AMD GPUOpen — "Procedural grass rendering" (mesh shader series, Part 4)
**URL:** `https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/`
**Authors:** Carsten Faber (Coburg Univ.), Bastian Kuth (Coburg Univ. / Erlangen-Nuremberg),
Quirin Meyer (Coburg Univ. CS Graphics professor), Max Oberberger (AMD GPU Architecture).
**Published:** March 20, 2024.
**Why critical:** This is the **canonical mesh-shader-based grass rendering algorithm** для 2024+ —
extends Jahrmann/Wimmer 2017 i3D tessellation-shader approach to mesh shaders. Provides
full source code, all formulas, and discusses RDNA 3 / Ampere / Ada performance.

**Key data points (verified by full content read):**
- Each blade = 8 vertices, 6 triangles (Bézier curve, 4 evaluation points per edge, 2 edges).
- 32 blades per patch max (mesh shader `max_vertices=256` constraint per
  `hardware-profile.md §3` для RTX 3060 Ti GA104 Ampere).
- Patch = 1 mesh shader work group (group size 128, 2 iterations per thread for vert/tri).
- `GrassPatchArguments { float3 position; float3 groundNormal; float height; }` SSBO.
- Random scattering via `r = patchRadius * sqrt(rand(seed))` (uniform-in-disc).
- Project blade offset onto terrain surface via `groundNormal` (avoids floating).
- **LOD:** `bladeCountF = lerp(32, 2, pow(saturate(distanceToCamera / (GRASS_END_DISTANCE*1.05)), 0.75))`,
  `bladeCount = ceil(bladeCountF)`, **fractional scaling** (width *= frac(bladeCountF) for last blade).
- **Geometry compensation:** `width *= 32 / bladeCountF` (visual consistency across LOD).
- **Wind:** `cos(WindDirection)*pos.x - sin(WindDirection)*pos.y + 4*PerlinNoise2D(0.1*pos)` for
  per-blade phase shift; `2*sin(0.5*t)`, `1*sin(1.0*t)` for x/y offsets.
- **Pixel shader:** fake self-shadow via `pow((worldY - rootHeight)/height, 1.5)`, darkening near roots;
  perlin noise color variation `0.75 + 0.25*PerlinNoise2D(0.25*worldXZ)`; normal softened
  `lerp(float3(0,0,1), normal, 0.25)`.
- **Bottleneck analysis:** "Tessellation" part of mesh shader = primary cost; group-of-128 stride.

### 2. rcm7133 — "Modern-Grass-Rendering" (Unity URP, 120k GPU instanced grass)
**URL:** `https://github.com/rcm7133/Modern-Grass-Rendering`
**Published:** Jan 3, 2026.
**Why critical:** Production-grade open-source reference для **GPU compute placement + GPU
instancing + distance-based LOD + custom GPU Frustum Culling**. Empirical numbers для 120k
blades (close to ProjectV's Stage 4.3 128m draw distance scale).

**Key data points (verified by full content read):**
- Per-blade struct = `float3 position + float height + float2 worldUV` = **24 bytes/blade**.
- 120,000 blades = 2.9 MB GPU memory.
- HLOD mesh: 11 verts / 9 tris. LLOD mesh: 7 verts / 5 tris.
- LOD via 2 separate buffers (HLOD + LLOD): **72 bytes/blade total** = 8.64 MB for 120k.
- LOD perf gain: **~40%** (massive tri reduction in distance).
- Frustum culling gain: **~10%** (using `GeometryUtility.CalculateFrustumPlanes` + 6 plane
  distance check per blade).
- Compute shader generates grass positions on grid (heightmap-sampled), Perlin noise for XZ
  jitter + height variation.
- Billboarding via `cross(bladeToCamera, up)` → cross with up → camera-facing direction.
- Vertex animation: wind sway texture (sine oscillator `amplitude * sin(id.xy * freq + time * speed)`),
  higher vertices = more sway.
- Fragment shader: 4-color gradient (AO bottom → middle × 2 → tip).

### 3. NVIDIA GPU Gems — Chapter 7: "Rendering Countless Blades of Waving Grass"
**URL:** `https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-7-rendering-countless-blades-waving-grass`
**Author:** Kurt Pelzer (Piranha Bytes / Codecreatures Benchmark 2002).
**Published:** 2004 (5th printing 2007).
**Why critical:** The **classic reference** (20+ years) для billboarded grass objects. Documents
3 animation methods (per-cluster CPU, per-vertex GPU, per-object GPU) — pre-mesh-shader
patterns that still inform modern LOD/streaming.

**Key data points (verified by full content read):**
- Grass object = 3 intersecting quads (12 verts, 4 tris), backface culling disabled, alpha
  blending enabled.
- Grass texture: alpha-transparent with multiple green/yellow shades; dense blade cluster per tile.
- Sort back-to-front at runtime.
- 3 animation methods (CPU vs GPU tradeoff):
  - **Per-cluster (CPU):** uniform shift for entire group, complex CPU wind sim, but **many
    draw calls** (clustering artifacts possible).
  - **Per-vertex (GPU):** 1 draw call, sin(time+pos) on each vertex, but **distortions** (edge
    length not preserved) and **homogeneous** (needs pseudo-random for chaos).
  - **Per-object (GPU):** 1 draw call, no distortion, **local chaos** via per-object center;
    requires extra per-vertex data (object center vector).
- Z-test/write enabled; per-pixel lighting via normal = vertical edge of polygon.
- Distance LOD: blend grass objects in/out as camera moves.

### 4. NVIDIA GPU Gems 3 — Chapter 6: "GPU-Generated Procedural Wind Animations for Trees"
**URL:** `https://developer.nvidia.com/gpugems/gpugems3/part-i-geometry/chapter-6-gpu-generated-procedural-wind-animations-trees`
**Author:** Renaldas Zioma (Electronic Arts / Digital Illusions CE = **DICE**).
**Published:** 2008.
**Why critical:** Authoritative wind simulation reference (DICE = Battlefield series), provides
**measured performance table** для hierarchical tree motion on DirectX 9 vs DirectX 10.
Directly applicable to foliage/grass wind via stochastic noise (Stam 1997).

**Key data points (verified by full content read):**
- Wind as 2D force field (vector field), wind primitives = analytical functions `v = G(x, t)`.
- Tree hierarchy 2-3 nodes deep: trunk → branches → leaves. Stiffness + mass + damping
  (mass-spring physics Ota 2003: `m*a + c*v + k*x = f(t)`).
- **Stochastic** simulation (Stam 1997) instead of physical: noise functions combine
  frequency bands (low freq = trunk drag, high freq = small branches, mid = turbulence).
- Three-branch scenarios: facing wind (lift+drag, pressed to trunk) / back side (high amplitude
  swaying) / perpendicular (bending around axis).
- Inertia + phase shifts per branch (chaos).
- HLSL quaternion-based simulation per vertex, concatenated from parent→child branches.
- D3D10 stream-out for branch transforms (avoids per-vertex recalculation).
- **Performance table (D3D9 vs D3D10):** 1,000 instances / 80,000 branches = 38.21 ms (D3D9) /
  22.48 ms (D3D10 SLOD3) / 21.61 ms (D3D10 two-bone skinning without branch sim).
- 256 instances / 20,480 branches = 9.81 ms (D3D9) / 5.82 ms (D3D10 SLOD3).
- Per-vertex cost scales linearly with branch count; SLOD = Simulation Level of Detail.

### 5. ReeCocho — "Article: Mesh Shaders" (Aug 19, 2024)
**URL:** `https://reecocho.github.io/2024/08/19/mesh-shaders/`
**Author:** Connor Bramham (game/graphics engineer, portfolio).
**Published:** 2024-08-19.
**Why critical:** Independent personal-engine integration of mesh shaders, confirms **10%
perf gain** для general GPU-driven rendering (validates our hypothesis that mesh shaders help
ProjectV's grass pipeline). Also documents "Procedural geometry" use case — exactly grass.

**Key data points (verified by full content read):**
- Mesh shader = fully programmable replacement for vertex + tessellation + geometry stages.
- Task (amplification) shader = optional, dispatches mesh shader work groups with payload.
- Work group size limit: 128-256 threads typical.
- Meshlets: sub-regions of mesh that fit mesh shader output limits, generated via bounding
  sphere algorithm (Jensen et al. performance comparison).
- Coarse-grained culling in first thread of task shader, then fine-grained culling distributed
  across remaining threads.
- Per-primitive culling: view-cone test per triangle.
- **"Procedural geometry" use case explicitly mentioned:** "Since you don't feed in 'vertex'
  and 'index' buffers to this stage like in the traditional pipeline, you can technically use
  whatever data structure you want to generate your geometry" — directly relevant for
  procedural grass blade generation per patch.
- 10% perf gain from mesh shader integration (general GPU-driven workload).
- Multi-view support: NVIDIA = 4 max view targets, AMD = 6 (cube map workaround).

---

## Tier 2 — Secondary sources (referenced / cross-referenced)

### 6. Jahrmann & Wimmer — "Responsive real-time grass rendering for general 3D scenes" (2017)
**Venue:** i3D '17 (ACM SIGGRAPH Symposium on Interactive 3D Graphics and Games).
**Referenced by:** AMD GPUOpen mesh shader series Part 4 (verified citation).
**Why relevant:** Canonical pre-mesh-shader grass paper using **tessellation shaders** to
subdivide predefined blades. AMD extended this concept to mesh shaders, but the algorithmic
foundation (per-blade Bezier, LOD, geometry compensation) comes from this paper. Closed form
analytical, not fetched directly.

### 7. Gilbert Sanders (Guerrilla Games) — "Between Tech and Art: The Vegetation of Horizon Zero Dawn"
**Venue:** GDC 2017 talk.
**Referenced by:** AMD GPUOpen mesh shader series Part 4 (verified citation).
**Why relevant:** Production reference для wind animation (sine waves in x/y, perlin noise
phase shift). HZD vegetation = de facto industry standard. Closed form, cited in AMD
GPUOpen's `GetWindOffset` function. Not fetched directly.

---

## Tier 3 — Background context (from INDEX.md cross-references, NOT primary)

- **`2026-06-21-mesh-shader-mega-instancing`** — orthogonal: large-scale instancing для
  static military units. Grass = per-pixel-per-frame organic placement, complement
  (mega-instancing = bulk CPU upload; grass = per-chunk GPU procedural).
- **`2026-06-21-eye-tracked-foveated`** — complementary: VRS Tier 2 attachment. Grass detail
  in periphery = follow-up via VRS density map (per HZD 2017 production pattern).
- **`2026-06-21-vk-fragment-shading-rate-voxel`** — complementary: same VRS pipeline.
- **`2026-06-21-procedural-military-terrain-gen`** — complementary: terrain shape, not
  surface vegetation. Grass placement = "what grows on the terrain surface", not "how the
  terrain is shaped".
- **`2026-06-21-biome-transition-blending`** — complementary: biome identity interpolation,
  grass density per biome is downstream consumer.

---

## Cross-axis map (which closed experiments this complements / orthogonals)

| Experiment | Status | Cross-axis | Note |
|:-----------|:-------|:-----------|:-----|
| `mesh-shader-mega-instancing` | mixed | complement | mega-instancing = bulk upload, grass = per-chunk procedural |
| `mesh-shader-vs-compute-cull` | mixed | orth | Stage 2.1 voxel meshing = different mesh shader use |
| `eye-tracked-foveated` | mixed | complement | VRS Tier 2 reduces grass detail in periphery |
| `vk-fragment-shading-rate-voxel` | mixed | complement | same VRS pipeline |
| `procedural-military-terrain-gen` | mixed | complement | terrain shape, not surface vegetation |
| `biome-transition-blending` | mixed | complement | biome identity, grass density is downstream |
| `genlayer-functional-biome-pipeline` | mixed | complement | biome chain, grass density per biome is downstream |
| `cloudscape-rendering` | mixed | orth | atmospheric, no surface vegetation |
| `volumetric-fog-atmosphere-rendering` | mixed | orth | participating media, no surface vegetation |
| `precomputed-atmospheric-sky` | yes | orth | background sky, no surface vegetation |
| `wfc-procedural-worlds` | mixed | orth | WFC gen, not surface vegetation |
| `trilinear-noise-interpolation` | mixed | complement | noise sampling, grass can use trilinear density lookup |
| `tonemap-color-grading` | yes | complement | grass color grading post-process |
| `aerial-perspective` | in-progress | complement | aerial perspective affects grass in distance |

**Total cross-references in INDEX.md / backlog.md:** scattered mentions only, **0 dedicated
experiments** on grass/foliage/vegetation rendering before this one.
