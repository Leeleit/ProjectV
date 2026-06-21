# 2026-06-21-deferred-translucent-sorting — Deferred translucent geometry sorting with distance-based priority

**Status:** `open`
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** Stage 5.x (rendering), independent
**Estimated effort:** S
**Author:** self (derived from VoxelCore source analysis)

---

## 1. Hypothesis

VoxelCore's `ChunksRenderer.cpp:349-421` implements deferred translucent sorting: translucent geometry is collected into `SortingMeshData` entries during chunk meshing, then sorted by distance from camera every `TRANSLUCENT_BLOCKS_SORT_INTERVAL=8` frames. Entries with AABB collapsed to <0.01 on any axis are merged into a single mesh. This amortizes sort cost over multiple frames while maintaining acceptable visual quality.

**Hypothesis:** Deferred translucent sorting (every N frames, not every frame) reduces translucent sort overhead by 70-90% vs per-frame sorting, with <0.5 ms visual artifact window on rapid camera movement. The 8-frame interval is a good default for 30-60 Hz rendering.

**Alternatives:** per-frame translucent sort (correct but expensive), no sort (visual artifacts), front-to-back per-chunk sort (incomplete — misses inter-chunk ordering).

---

## 2. Prior art

- **VoxelCore `ChunksRenderer.cpp:349-421`** — deferred translucent sorting every 8 frames; AABB collapse merge optimization.
- **VoxelCore `BlocksRenderer.cpp:697-766`** — three-pass chunk build: translucent → opaque → dense.
- **Minecraft 1.12 `RenderChunk.java:145-322`** — per-block render layer separation (CUTOUT, CUTOUT_MIPPED, TRANSLUCENT).
- **OpenGL/Vulkan alpha blending** — standard translucent rendering requires back-to-front ordering.
- **closed `2026-06-21-taa-motion-vectors`** — TAA motion vectors (orthogonal — TAA is post-process, this is geometry sorting).
- **closed `2026-06-21-vulkan-memory-aliasing-transient`** — render graph (orthogonal).

---

## 3. Method

- **Type:** analytical + standalone C++26 CPU prototype
- **Scene:** 5 scenes with varying translucent content (no_translucent, water_surface, glass_building, ice_cave, mixed_translucent)
- **Metrics:** sort time (µs/frame), visual artifact count (wrong blending order), sort frequency (Hz), memory overhead
- **Baseline:** per-frame translucent sort (every frame)
- **Strategies:**
  - A_PerFrame: sort every frame (baseline, correct but expensive)
  - B_EveryN: sort every N frames (N=4, 8, 16)
  - C_DistanceAdaptive: sort frequency based on camera rotation speed (fast rotation = every frame, slow = every 8)
  - D_PerChunkSort: sort within each chunk only (no inter-chunk ordering)

---

## 4. Prototype

Standalone C++26 CPU harness measuring sort overhead vs visual quality across translucent scenes.

```bash
cd prototype && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/translucent_sort_bench
```

---

## 5. Results

_TBD — experiment not started._

---

## 6. Verdict

`open` — hypothesis only, no measurements yet.

---

## 7. Integration recommendation

- **Target stage:** Stage 5.x (translucent rendering pass)
- **Конкретные изменения:** `src/render/ChunksRenderer.{hpp,cpp}` — deferred sort manager with configurable interval.
- **Подход:** collect translucent entries during chunk meshing; sort by camera distance every N frames; merge small AABB entries.
- **Риски:** visual popping during fast camera rotation; sort cost spike on first frame after interval.
- **Критерии приёмки:** >70% sort overhead reduction vs per-frame; <0.5 ms artifact window on rapid camera movement.
- **Зависимости:** existing translucent render pass, mesh shader pipeline.
- **Estimated effort:** S (~150 LoC, 1-2 sessions)

---

## 8. Sources

- VoxelCore `ChunksRenderer.cpp:349-421` (local source)
- VoxelCore `BlocksRenderer.cpp:697-766` (local source)
- Minecraft 1.12 `RenderChunk.java:145-322` (local source)
- closed `2026-06-21-taa-motion-vectors` (orthogonal TAA axis)

---

## 9. Mapping to ProjectV hot-path

- **Target:** `src/render/Renderer.cpp` — translucent render pass (future).
- **Assumptions:** translucent objects are relatively static (water, glass, ice); camera rotation speed <180°/sec typical.
- **Unmeasured:** actual translucent geometry count in real ProjectV scenes; GPU sort (compute shader) vs CPU sort overhead.
