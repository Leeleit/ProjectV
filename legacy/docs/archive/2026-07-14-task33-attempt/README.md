# Task 3.3 GPU-driven indirect mesh dispatch — attempt archive (2026-07-14)

## Gate result: FAIL (rolled back)

VoxelLab 120-frame bench (`PROJECTV_MESH_SHADER_PIPELINE=ON`):

| Mode | mean_ms | mean_fps |
|---|---|---|
| `PROJECTV_MESH_SHADER_INDIRECT=OFF` | 2.069 | 483.23 |
| `PROJECTV_MESH_SHADER_INDIRECT=ON`  | 2.101 | 475.86 |

Delta: ~+1.5% frame time (slower). Gate requires ≥10% mesh-pass CPU or frame improvement → rollback.

Fix during attempt: removed `vkCmdPipelineBarrier2` from `RecordMeshShaderDraw` (runs inside
`vkCmdBeginRendering`); sync already covered by `RecordMeshShaderPreCull` post-dispatch barrier.

Artifacts: `task33-mesh-indirect.patch`, `voxel_mesh_pre.comp`.
