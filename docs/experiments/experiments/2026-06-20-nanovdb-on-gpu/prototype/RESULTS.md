# Results — CPU-side: SVDAG-on-64-tree vs NanoVDB-aligned (byte-exact, chunkSize=8)

Host: AMD Ryzen 7 5800X, clang 22.1.6, -O3 -march=native -DNDEBUG.
All scenes 8^3 = 512 voxels (matches mainline chunkSize per VoxelWorld.hpp:78).
Build = full SetCell traversal. Ray-march = CPU simulation of GPU traversal
pattern (sequential descent, no batching). kWarmup=100, kMeasure=1000 rays/scene.

| Tree            | Scene           | NonAir | Bytes |   B/vox | Nodes | Build ms | Verify mism | Ray ns mean |
|:----------------|:----------------|-------:|------:|--------:|------:|---------:|------------:|------------:|
| svdag64         | solid_8         |    512 |  4520 | 8.82812 |     9 |  0.02114 |           0 |       12.09 |
| nanovdb_aligned | solid_8         |    512 |  2192 | 4.28125 |    73 |  0.00491 |           0 |        9.61 |
| svdag64         | ground_8        |     64 |  2280 |  35.625 |     5 |  0.00177 |           0 |        8.55 |
| nanovdb_aligned | ground_8        |     64 |  1264 |   19.75 |    21 |  0.00115 |           0 |        8.56 |
| svdag64         | brick_8         |    512 |  4520 | 8.82812 |     9 | 0.015879 |           0 |        8.53 |
| nanovdb_aligned | brick_8         |    512 |  2192 | 4.28125 |    73 |  0.00415 |           0 |        8.51 |
| svdag64         | voxel_lab_8     |     86 |  4520 | 52.5581 |     9 |  0.00302 |           0 |        8.53 |
| nanovdb_aligned | voxel_lab_8     |     86 |  2192 | 25.4884 |    53 |  0.00338 |           0 |       13.93 |
| svdag64         | sparse_random_8 |     53 |  4520 |  85.283 |     9 |  0.00325 |           0 |       13.38 |
| nanovdb_aligned | sparse_random_8 |     53 |  2192 | 41.3585 |    44 |  0.00254 |           0 |       15.32 |

## Notes

- **svdag64** = standalone re-implementation of src/voxel/Sparse64Tree.hpp semantics. Node = fillMask:u64 + 64 child
  slots:u32 + structuralHash:u64 + refCount:u32 = 280 B. Includes leaf flag (0x80000000u), homogeneous flag (
  0x40000000u), node index mask (0x3FFFFFFFu), material mask (0xFFu). Matches mainline.
- **nanovdb_aligned** = 3-level structure (Upper[8^3] -> Lower[4^3] -> Leaf[2^3]) for chunkSize=8. Per NanoVDB.h actual
  structure (scaled: full NanoVDB uses 32^3/16^3/8^3 for full Grid; here each level covers 1/8 of parent in each axis).
  Byte-exact: always materializes children on SetCell (bugfix vs 2026-06-20 svdag_vs_nanovdb prototype which had
  uniform-tile lie). Fixed grid overhead 736 B (GridData 672 B + TreeData 64 B per NanoVDB.h).
- verify_mismatches MUST be 0 for all rows (byte-exact correctness vs flat voxels).
- This prototype differs from 2026-06-20-svdag-vs-vdb-memory-throughput in 2 ways:
    1. **chunkSize=8 (not 32):** matches mainline per VoxelWorld.hpp:78.
    2. **NanoVDB impl byte-exact:** bugfix removes the uniform-tile lie, all scenes have verify_mismatches=0.
