# Sources — 2026-06-21-dynamic-battlefield-decal-system

**Verified sources via web-search (DuckDuckGo HTML fallback per `agent/knowledge.md Part B §9`; Exa HTTP 429
persistent). Tier 1 = primary production/canonical, Tier 2 = supporting.**

## Tier 1 — Primary canonical / production references

1. **Frostbite "Shadows & Decals: D3D10 Techniques in Frostbite" (GDC'09)** —
   [slideshare.net/slideshow/02-g-d-c09-shadow-and-decals-frostbite-final3flat/1228973](https://www.slideshare.net/slideshow/02-g-d-c09-shadow-and-decals-frostbite-final3flat/1228973)
   (Johansen, Drobot et al., EA DICE, GDC 2009). **Canonical decal-via-geometry-shader approach**:
   extract decal geometry from visual meshes via GS + stream-out into decal buffer; cull + transform +
   transfer UV sets + clip decals in single pass; minimize CPU overhead; GPU-driven decal mesh generation
   per frame. **Why important:** directly maps to our decal placement hypothesis — GPU-generated decal
   geometry, no CPU per-decal mesh creation, batched rendering. 68 slides, full production reference
   from Battlefield 3 era (Frostbite 1.5).

2. **Bindless Deferred Decals in The Surge 2** — Philip Hammer (DECK13 Interactive, Digital Dragons 2019) —
   [slideshare.net/.../bindless-deferred-decals-in-the-surge-2/148105513](https://www.slideshare.net/slideshow/bindless-deferred-decals-in-the-surge-2/148105513) +
   [youtube.com/watch?v=e2wPMqWETj8](https://www.youtube.com/watch?v=e2wPMqWETj8). **Production reference
   for bindless decal atlas**: D3D12/Vulkan bindless resource binding model; decals are part of lighting
   shader (don't modify G-buffer); use-cases + common problems + optimizations; includes code samples.
   **Why important:** directly matches our strategy D (bindless atlas + indirect draw); explicit
   persistent decal storage pattern; combat-decals use-case (The Surge 2 is a Souls-like with persistent
   blood/scorch marks). 58 slides.

3. **TheRealMJP/DeferredTexturing** — MJP, GitHub (D3D12 rendering sample) —
   [github.com/TheRealMJP/DeferredTexturing](https://github.com/TheRealMJP/DeferredTexturing).
   **Reference implementation** of bindless deferred texturing + decals: textures are not sampled in
   geometry pass; deferred texturing approach with decal atlas; open-source D3D12 sample. **Why important:**
   canonical reference code for atlas + indirect sampling pattern.

4. **Khronos Vulkan Samples — Multi Draw Indirect** —
   [docs.vulkan.org/samples/latest/samples/performance/multi_draw_indirect/README.html](https://docs.vulkan.org/samples/latest/samples/performance/multi_draw_indirect/README.html).
   **Canonical pattern** for GPU-driven indirect draw: `VkDrawIndexedIndirectCommand` array populated
   by compute shader (frustum culling → instance count 0/1); `vkCmdDrawIndexedIndirect` for single
   batched dispatch. 16×16 grid of sub-meshes demonstrated; bindless texture array + model information
   buffer = entire scene rendered without per-draw binds. **Why important:** exact mechanism for our
   strategy D indirect draw — compute shader writes decal draw commands → single batched draw.

5. **GPU Gems 2 — Ch. 5: "Decal Applications"** (Mitchell 2005) — canonical taxonomy of decal
   approaches (screen-space / world-space / decal atlas) + projection math + batching strategies.
   **Note:** primary PDF not verified-fetched this session (web_search 429); referenced per
   GPU Gems 2 canonical index.

## Tier 2 — Supporting references

6. **GPU-Driven Pipeline** — Zhytou (2025-03-18) —
   [zhytou.github.io/post/2025-3-18/gpu-driven/](https://zhytou.github.io/post/2025-3-18/gpu-driven/).
   Indirect draw + compute shader + GPU-driven culling architecture overview; Nanite comparison;
   Mesh Cluster Rendering pattern. Supports our architecture choice.

7. **GPUOpen "GDC 2024 Work graphs and draw calls"** — AMD GPUOpen —
   [gpuopen.com/learn/gdc-2024-workgraphs-drawcalls/](https://gpuopen.com/learn/gdc-2024-workgraphs-drawcalls/).
   Work graphs as alternative to indirect draw for decal-style updates; forward-looking AMD direction.
   Orth to our hypothesis but useful for cross-vendor matrix.

8. **Optimizing the Graphics Pipeline with Compute (GDC 2016)** — Graham Wihlidal (EA) —
   [slideshare.net/.../optimizing-the-graphics-pipeline-with-compute-gdc-2016](https://www.slideshare.net/slideshow/optimizing-the-graphics-pipeline-with-compute-gdc-2016/).
   Compute-shader-driven pipeline patterns (Frostbite evolution); LRU + indirect draw precedents for
   GPU-side state management. 99 slides.

9. **Alex Tardif "Bindless" blog** — alextardif.com (canonical guide to bindless graphics patterns) —
   [alextardif.com/Bindless.html](https://alextardif.com/Bindless.html). References Philip Hammer's
   Surge 2 talk + MJP's DeferredTexturing as canonical practical examples. Useful for integration patterns.

10. **GPU-Driven Instancing — PLAYERUNKNOWN Productions** (2025-07-22) —
    [playerunknownproductions.net/news/gpu-driven-instancing](https://playerunknownproductions.net/news/gpu-driven-instancing).
    GPU-side instance generation + indirect draw for instanced rendering; transferable pattern to decal
    instances.

## Cross-references inside ProjectV (closed experiments)

- **Closed `2026-06-21-mesh-shader-mega-instancing`** [mixed, C_AmplificationShaderOnly 62-544×] —
  `src/shaders/voxel_mesh.task` mesh shader pattern; transferable to decal placement via amplification.
- **Closed `2026-06-21-chunk-damage-fracture-model`** [mixed, C_Greedy3D 2.88 µs] — chunk-edit
  invalidation methodology for decals attached to chunk faces.
- **Closed `2026-06-21-voxel-topology-analysis`** [yes, 2.73 µs CCL] — exposed-face classification for
  decal placement target selection.
- **Closed `2026-06-21-bindless-descriptor-overhead`** — decal atlas as bindless texture array pattern.
- **Closed `2026-06-21-vulkan-memory-aliasing-transient`** — atlas + SSBO aliasing patterns.
- **Closed `2026-06-21-chunk-storage-compression-axis`** — persistent decal storage as part of chunk
  file format (deferred integration).

## Open backlog cross-references

- **Closed `2026-06-21-explosion-crater-terrain-deformation`** [Tier 1, h, verdict=yes] — crater generation feeds decal placement. E_RasterizedSphereMarch = 0.128 µs mean (1.82× speedup vs naive, 100% boundary correctness).
- **Open `destructible-building-system`** [Tier 1, h] — building damage → persistent scorch decals.
- **Open `ballistic-projectile-simulation`** (closed yes, but bullet-hit events) — bullet impact → decal
  spawn event source.

## Citing format for README §2

Format in README follows Tier 1 sources only (most-relevant 5); Tier 2 listed here for full traceability.
