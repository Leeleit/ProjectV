# 2026-06-21-flood-fill-visgraph-culling — Flood-fill VisGraph face-to-face visibility for chunk occlusion

**Status:** `open`
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** Stage 2.x (culling), independent
**Estimated effort:** M
**Author:** self (derived from Minecraft 1.12 source analysis)

---

## 1. Hypothesis

Minecraft 1.12's `VisGraph.java` computes per-chunk face-to-face visibility via BFS flood-fill through non-opaque voxels. The result is a compact 6×6 boolean matrix (which faces can see through to which other faces), enabling fast inter-chunk occlusion culling without GPU readback. This is more precise than simple AABB frustum culling and cheaper than GPU occlusion queries.

**Hypothesis:** A VisGraph-style flood-fill per 16³ (or 8³ for ProjectV chunkSize=8) section computes face-to-face visibility in <5 µs, enabling 30-50% chunk draw reduction via neighbor visibility propagation. The flood-fill is cache-friendly (BitSet iteration) and avoids GPU readback latency.

**Alternatives:** software rasterization occlusion (expensive), GPU occlusion queries (readback latency), simple frustum culling only (no occlusion).

---

## 2. Prior art

- **Minecraft 1.12 `VisGraph.java:36-128`** — flood-fill from each face edge through non-opaque blocks, recording face-to-face visibility in `SetVisibility` (6×6 BitSet matrix).
- **Minecraft 1.12 `SetVisibility.java`** — stores `BitSet[6]` where bit j in set i = "face i can see face j".
- **Minecraft 1.12 `CompiledChunk.java`** — stores VisGraph result per render chunk for inter-chunk visibility queries.
- **Akenine-Möller 1997** — frustum culling survey (foundational).
- **closed `2026-06-21-lod-mesh-downsampling`** — LOD downsampling (orthogonal to occlusion).
- **closed `2026-06-21-lod-transition-strategy`** — LOD transitions (orthogonal).

---

## 3. Method

- **Type:** analytical + standalone C++26 CPU prototype
- **Scene:** 5 voxel scenes (uniform_open, cave_stress, indoor_room, dense_column, mixed_biome)
- **Metrics:** flood-fill time (µs), matrix accuracy (vs brute-force ray casting), chunk draw reduction (%), cache miss rate
- **Baseline:** simple AABB frustum culling only (no inter-chunk occlusion)
- **Strategies:**
  - A_FrustumOnly: AABB frustum test (baseline)
  - B_VisGraph8: flood-fill per 8³ section (ProjectV chunkSize=8)
  - C_VisGraph16: flood-fill per 16³ section (Minecraft scale)
  - D_VisGraphWithEarlyExit: <128 opaque blocks → skip flood-fill (Minecraft optimization)

---

## 4. Prototype

Standalone C++26 CPU harness measuring flood-fill time and accuracy across voxel scenes.

```bash
cd prototype && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/visgraph_culling_bench
```

---

## 5. Results

_TBD — experiment not started._

---

## 6. Verdict

`open` — hypothesis only, no measurements yet.

---

## 7. Integration recommendation

- **Target stage:** Stage 2.x (culling system)
- **Конкретные изменения:** new `src/render/VisGraph.{hpp,cpp}` for per-chunk flood-fill; integration with `HiZCulling.cpp` for inter-chunk visibility propagation.
- **Подход:** compute VisGraph per chunk during meshing (already traversing all voxels); store 6×6 matrix in `CompiledChunk` equivalent; propagate visibility to neighbors during culling pass.
- **Риски:** flood-fill cost on dense chunks (>4096 non-air voxels); stale visibility during rapid edits.
- **Критерии приёмки:** >30% chunk draw reduction on cave_stress scene; <5 µs per 8³ section flood-fill.
- **Зависимости:** existing `HiZCulling.cpp` infrastructure, mesh shader pipeline.
- **Estimated effort:** M (~300 LoC, 2 sessions)

---

## 8. Sources

- Minecraft 1.12 `VisGraph.java:36-128` (local source)
- Minecraft 1.12 `SetVisibility.java` (local source)
- Minecraft 1.12 `CompiledChunk.java` (local source)
- closed `2026-06-21-lod-mesh-downsampling` (orthogonal LOD axis)

---

## 9. Mapping to ProjectV hot-path

- **Target:** `src/render/HiZCulling.cpp:800-805` — current hardcoded mip=0 culling.
- **Assumptions:** chunkSize=8 (512 voxels per section, not 4096); opaque test via existing `isOpaqueCube()` equivalent.
- **Unmeasured:** GPU occlusion query comparison (driver overhead), multi-frame visibility staleness impact.
