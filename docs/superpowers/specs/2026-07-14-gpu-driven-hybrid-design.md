# GPU-driven hybrid renderer — Design Spec

**Date:** 2026-07-14  
**Status:** Approved for implementation (operator: implement plan as specified)  
**Primary path lock:** **Raster-primary** for voxel surfaces (face-clusters + mesh/VS). RT-first for lighting, analytic primitives, and optional occlusion; tensor cores reserved for future upscale/denoise.

---

## 1. Goals

1. Minimize CPU work to frame-graph, residency, one-shot heap binds, and large kicks.
2. One **geometry truth** for voxels: `voxel_mesh.comp` → `PackedFace` (no dual greedy in mesh stage).
3. GPU cull → indirect mesh tasks over **face-clusters** (`PackedFace` groups ≤64 faces).
4. Real **bindless** resource layer (descriptor indexing + `UPDATE_AFTER_BIND`), not PostFX-only demo.
5. Hybrid-ready spine: voxel / polygon / analytic producers share cull+bindless patterns; analytics are **RT-first**.

## 2. Non-goals (this workstream)

- Versioned cluster ABI / Nanite DAG.
- Path-traced primary voxel shading.
- Day-1 `VK_EXT_descriptor_heap` requirement.
- Replacing all culling with RT.
- Shipping model meshlets or analytic custom-hit in Phase G (slots only).
- Tensor/DLSS implementation (document hooks only).

## 3. Architecture

```text
CPU: update tables → bind heaps → kick cull / meshing / mesh-indirect / RT / post
GPU producers:
  Voxels:    occupancy → PackedFace → FaceCluster[]
  Models:    (later) meshlets
  Analytics: (later) procedural AABB + RT intersection params
GPU spine:
  compute frustum (+ HZB) → visible IDs + indirect args
  mesh shader pulls PackedFace for one FaceCluster
  fragment: MaterialVisual SSBO + bindless texture indices
  RT: shadows / AO / GI / analytic hits
  tensor (future): DLSS / ML denoise after radiance
```

### 3.1 Voxel primary = raster face-clusters

Locked: visible voxel **surfaces** are rasterized from face-clusters. RT does not replace primary opaque fill in this workstream.

### 3.2 FaceCluster (Phase G, hard PackedFace payload)

```c
// GPU std430; CPU mirror must match
struct FaceCluster {
    uint faceOffset;   // into packedFaces[]
    uint faceCount;    // 1..kFacesPerCluster (64)
    uint chunkIndex;
    uint flags;        // bit0 = transparent (reserved)
    vec4 aabbCenterHalfExtent; // xyz = center, w = half-extent (conservative)
};
```

- `kFacesPerCluster = 64` (fits mesh output 256 verts / 256 prims with 4 verts + 2 tris per quad).
- AABB v1 = parent chunk AABB (conservative). Cone culling optional later.
- No versioned ABI yet; second payload type triggers ABI when needed.

### 3.3 Mesh path (Pattern C rewritten)

| Was | Becomes |
|-----|---------|
| Mesh stage re-runs greedy from voxels | Mesh stage **pulls** `PackedFace` for one cluster |
| `DrawMeshTasksEXT(N_chunks)` | `DrawMeshTasksIndirectEXT` / IndirectCount from visible cluster count |
| Frustum-only chunk cull | Cluster frustum cull (+ HZB when wired) |
| Dual meshing when mesh ON | Single meshing; transparent stays PackedFace consumer |

### 3.4 Bindless (Phase B)

- Persistent variable-count `COMBINED_IMAGE_SAMPLER` heap with `UPDATE_AFTER_BIND` + `PARTIALLY_BOUND`.
- Descriptor-indexing path first; heap/buffer API later behind the same index ABI.
- Materials keep `MaterialVisual` SSBO colors; add optional `albedoTextureIndex` (uvec4 padding / extend carefully) or sample bindless via push/material table when index ≠ sentinel.
- `nonuniformEXT` required for divergent indices.
- PostFX composite becomes one consumer of the shared heap manager.

### 3.5 RT-max + tensor (slots)

| Unit | Role now | Future slot |
|------|----------|-------------|
| RT cores | Shadows, AO, GI, analytic hits | Optional RT occlusion pass vs HZB (measure) |
| SM/compute | Frustum, HZB, cluster compact, meshing | Unchanged spine |
| Raster/mesh | Primary voxel (+ later model) surfaces | Model meshlets |
| Tensor | — | DLSS/NIS, ML denoise, coop-matrix; do not block async compute scheduling |

Raymarch stays compute/SDF unless marching against RT-resident geometry. Analytic shapes: RT custom/procedural intersection when implemented.

## 4. Hybrid producers (doors open)

- **Do not** overload `PackedFace` for meshes or spheres.
- Shared concepts: visibility work-item, bindless indices, indirect kick.
- Model path today: CPU `DrawIndexed` — replace later with meshlet producer.
- Analytic path: RT-first; raster proxy optional.

## 5. Verification

- Build green; `ctest` green.
- Mesh path: `PROJECTV_MESH_SHADER_PIPELINE=ON` renders opaque without dual greedy; validation clean for smoke frames.
- Bindless: `PROJECTV_BINDLESS=ON` uses shared heap path; PostFX still composites correctly.
- Document contracts in `agent/knowledge.md` + snapshot in `agent/workspace.md`.

## 6. Phased delivery

1. **Phase G** — FaceCluster + pull-mesh + indirect + kill dual greedy.
2. **Phase B** — BindlessHeap manager + PostFX + material/texture index path with `nonuniformEXT`.
3. Docs — RT/tensor slots in this spec + knowledge contract.
