# Task 3.2 HZB-driven indirect RT shadow dispatch — attempt archive (2026-07-14)

## Gate result: FAIL (rolled back)

VoxelLab 120-frame bench (`PROJECTV_BENCHMARK_FRAMES=120 PROJECTV_BENCHMARK_QUIT=1`):

| Mode | mean_ms | mean_fps |
|---|---|---|
| `PROJECTV_RTX_HZB_INDIRECT=OFF` | 2.008 | 497.99 |
| `PROJECTV_RTX_HZB_INDIRECT=ON`  | 2.057 | 486.15 |

Delta: ~+2.4% frame time (slower). Gate requires ≥10% RT/frame improvement → rollback.

Validation (`PROJECTV_ENABLE_VALIDATION=ON` + HZB ON, 10 frames): clean, VoxelAwareRtxShadows ready.

Likely cause: VoxelLab is small; compaction + clear + barrier overhead outweighs cull savings.
HZB mips are LINEAR-filtered (not min-reduction), so `kHzbOccluderMip=2` is approximate.

Artifacts: `task32-hzb-indirect.patch`, `rtx_visible_pixels.comp`, `voxel_rtx_shadow.rgen`.
