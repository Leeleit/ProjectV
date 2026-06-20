# Session plan — 2026-06-21

## Direction
**Stage 2.1 Pattern C mesh shader full integration** (10h, dirty tree, build+ctest per phase).

## Decisions
- Pattern C = `voxel_mesh_pre.comp` (compute cull) → `vkCmdDrawMeshTasksEXT(visibleCount, 1, 1)` → `voxel_mesh.mesh` (greedy emission)
- **Replacing** main PackedFace indirect draw when enabled; shadow pass remains on PackedFace (PackedFace SSBO still produced for shadow)
- **Delete** `voxel_mesh.task` (vestigial spike; Pattern C = compute pre-cull, not task)
- Feature flag: `PROJECTV_MESH_SHADER_PIPELINE=ON` (default OFF per `mesh-shader-vs-compute-cull` verdict=mixed)
- Graceful fallback: `vkGetPhysicalDeviceFeatures2` `meshShader == VK_FALSE` → disable without crash

## Architectural invariants (locked)
- `PackedFace` SSBO produced by `voxel_mesh.comp` — **not** removed. Shadow pass reads it via `voxel_shadow.vert`.
- `voxel_mesh.comp` still runs (writes PackedFace). Mesh shader path only replaces main pass DRAW, not compute cull.
- `outAmbientVisibility = 1.0` (no-op AO) in mesh shader — consistency with `voxel.vert:137`. Per-corner AO inside mesh shader is future work per `agent/knowledge.md §14`.
- `voxel.vert`/`voxel.frag` unchanged. Mesh shader writes same vertex outputs (normal/worldPos/materialIndex/ambientVisibility).
- Per `agent/knowledge.md §15` lighting contract: SceneLightingBuffer binding byte-exact across all consumers.

## Phases

| # | Phase | Hours | DoD |
|---|-------|-------|-----|
| 1 | Port GreedyFacePass → voxel_mesh.mesh | 3.0 | glslc clean, SPIR-V valid, meshShader capability present |
| 2 | Pre-cull compute integration check | 0.5 | visibleCount init via vkCmdFillBuffer, capacity ≥ chunkCount |
| 3 | Pipeline creation + feature gate | 1.5 | Validation layer green, env toggle works |
| 4 | Renderer integration (replace main draw) | 1.5 | Tracy triangles ≈ match, shadow still correct |
| 5 | outAmbientVisibility = 1.0 | 0.5 | identical to voxel.vert:137 |
| 6 | Tests + visual smoke | 1.5 | 20/20 ctest, VoxelLab visual OK |
| 7 | Doc sync + EVIL + delete task + commit prompt | 1.0 | docs updated, commit pending operator |
| 8 | Buffer: extra polish (1.3 min + benchmark hook) | 0.5 | scope opt |
| | **Total** | **10.0** | |

## files-touched-intent
- `src/shaders/voxel_mesh.mesh` — port greedy (Phase 1)
- `src/shaders/voxel_mesh_pre.comp` — minor adjustments (Phase 2)
- ~~`src/shaders/voxel_mesh.task`~~ — **DELETE** (Phase 7)
- `src/render/vulkan/VulkanVoxelMeshingPipeline.cpp` — mesh shader pipeline (Phase 3)
- `src/render/HizCulling.{hpp,cpp}` — `IsMeshShaderPipelineEnabled()` analog (Phase 3)
- `src/render/Renderer.cpp` — integration (Phase 4)
- `src/render/SceneResources.{hpp,cpp}` — VisibleChunkIdBuffer/VisibilityCounter (Phase 2-3)
- `src/core/Types.hpp` — only if new struct fields needed
- `src/CMakeLists.txt` — shader registration adjustments (Phase 7)
- `tests/MeshShaderTests.cpp` — new compile-test (Phase 6)
- `CHANGELOG.md`, `agent/workspace.md`, `agent/knowledge.md`, `COMMENTS.md`, `TODO.md` — doc sync (Phase 7)

## Commit policy
**Dirty + закрытие в конце** (per operator). ONE prompt "Commit?" at end of session with full diff stat.

## Build verification
`cmake --build build/linux-clang-debug` + `ctest --test-dir build/linux-clang-debug` после каждой фазы (per operator).

## Open risks
- Per-chunk worst case (8³ empty) = 6 × 8 × 8 = 384 quads × 4 verts = 1536 vertices. `max_vertices=256`/`max_primitives=128` may clip — must validate or raise.
- `voxel_mesh.comp` still dispatching even when mesh shader path active (PackedFace for shadow). This is the cost of additive shadow path. Per operator "shadow path untouched".
- NVIDIA June 2025 driver bug mesh-shading+async (per `dec-pipelines-async-compute`) — relevant if async HZB is added later, NOT in this session.
- Per `hzb-binding-models` Phase E future work: bindless TLAS — NOT in scope.
