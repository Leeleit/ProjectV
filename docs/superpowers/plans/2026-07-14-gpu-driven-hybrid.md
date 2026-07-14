# GPU-driven hybrid — Implementation Plan (Phase G → B)

> **For agentic workers:** execute task-by-task. Prefer verification after each phase.

**Goal:** Raster-primary voxel face-clusters + real bindless heap; RT/tensor slots documented.

**Architecture:** See [2026-07-14-gpu-driven-hybrid-design.md](../specs/2026-07-14-gpu-driven-hybrid-design.md).

**Tech stack:** C++26, Vulkan 1.4, GLSL mesh shaders, descriptor indexing.

---

## Files map

| File | Role |
|------|------|
| `src/shaders/voxel_face_cluster.comp` | Build FaceCluster[] from chunk drawRanges |
| `src/shaders/voxel_mesh_pre.comp` | Cull clusters → visible IDs + indirect mesh cmds |
| `src/shaders/voxel_mesh.mesh` | Pull PackedFace quads (no greedy) |
| `src/render/vulkan/VulkanMeshShader*.cpp` | Pipelines, bindings, cull, draw |
| `src/core/Types.hpp` | FaceCluster buffers / capacities |
| `src/render/BindlessHeap.hpp/.cpp` | Shared bindless sampled heap |
| `src/render/PostFx*.cpp` | Consume BindlessHeap |
| `src/shaders/post_composite_bindless.comp` | `nonuniformEXT` indices |
| `agent/knowledge.md` / `workspace.md` | Contracts + snapshot |

---

### Task 1: FaceCluster build compute

- [ ] Add `voxel_face_cluster.comp` (`kFacesPerCluster=64`, chunk AABB → center/halfExtent)
- [ ] Wire module/pipeline; run after meshing when `meshShaderEnabled`
- [ ] Allocate `faceClusterBuffer` + `faceClusterCountBuffer` per frame

### Task 2: Rewrite mesh pull + cull + indirect

- [ ] Rewrite `voxel_mesh.mesh` to decode PackedFace (mirror `voxel.vert`)
- [ ] Bindings: 0=chunks, 1=packedFaces, 2=faceClusters, 3=visibleClusterIds, 4=indirectCmds/counter
- [ ] Cull writes `VkDrawMeshTasksIndirectCommandEXT` + visible IDs
- [ ] Draw via `vkCmdDrawMeshTasksIndirectEXT` (env `PROJECTV_MESH_SHADER_INDIRECT` default ON when mesh ON)
- [ ] Remove dual greedy path

### Task 3: BindlessHeap (Phase B)

- [ ] `BindlessHeap` create/destroy/register/update with UPDATE_AFTER_BIND
- [ ] PostFX composite uses heap slots; `nonuniformEXT` in shader
- [ ] Optional material albedo: sentinel index → SSBO color; else sample heap

### Task 4: Docs + verification

- [ ] `agent/knowledge.md` contract §39 GPU-driven hybrid
- [ ] `agent/workspace.md` snapshot
- [ ] `ninja` ProjectV + `ctest` 44/44 (or current count)

---

## Verification commands

```bash
ninja -C build/linux-clang-debug ProjectV
ctest --test-dir build/linux-clang-debug --output-on-failure
# smoke (operator machine):
# PROJECTV_MESH_SHADER_PIPELINE=ON PROJECTV_BINDLESS=ON PROJECTV_ENABLE_VALIDATION=ON
```
