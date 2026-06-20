# Results — SVDAG-on-64-tree vs NanoVDB-like (32^3 regions)

Host: AMD Ryzen 7 5800X, clang 22.1.6, -O3 -march=native -DNDEBUG.
All scenes are 32^3 = 32768 voxels. Build = SetCell over full scene.
SetCell bench = 1000 random SetCell calls after build (microseconds).
GetCell bench = 10000 random GetCell calls after build (nanoseconds).

## Per-tree summary

| Tree             | Scene            | Non-air | Total bytes | B/non-air | SetCell mean us | SetCell p99 us | GetCell mean ns | GetCell p99 ns | Build ms | Verify mism |
|:-----------------|:-----------------|--------:|------------:|----------:|----------------:|---------------:|----------------:|---------------:|---------:|------------:|
| svdag64_no_dedup | empty_32         |     987 |      143456 |    145.34 |             0.1 |           1.43 |           30.28 |             50 |     0.01 |           0 |
| svdag64_dedup_on | empty_32         |     987 |      150368 |    152.34 |            1.16 |           2.98 |           28.43 |             40 |     0.01 |           0 |
| nanovdb_like     | empty_32         |     439 |       11936 |     27.18 |            0.04 |           0.16 |           23.51 |             30 |     0.01 |           0 |
| svdag64_no_dedup | solid_32         |   32768 |      286816 |      8.75 |            0.04 |           0.06 |           25.83 |             40 |     1.12 |           0 |
| svdag64_dedup_on | solid_32         |   32768 |      295152 |         9 |            0.93 |           1.82 |           24.25 |             40 |     20.2 |           0 |
| nanovdb_like     | solid_32         |     512 |       11936 |     23.31 |            0.02 |           0.03 |           22.47 |             30 |     0.28 |           0 |
| svdag64_no_dedup | ground_32        |    4970 |      143456 |     28.86 |            0.04 |           0.07 |           24.12 |             40 |     0.07 |           0 |
| svdag64_dedup_on | ground_32        |    4970 |      150544 |     30.29 |            0.87 |           1.49 |           26.08 |             50 |      2.6 |           0 |
| nanovdb_like     | ground_32        |     474 |       11936 |     25.18 |            0.02 |           0.06 |           22.91 |             30 |     0.07 |       12288 |
| svdag64_no_dedup | checkered_32     |   16905 |      286816 |     16.96 |            0.03 |           0.04 |           30.52 |             50 |     0.41 |           0 |
| svdag64_dedup_on | checkered_32     |   16905 |      295152 |     17.45 |            0.93 |           1.82 |            29.7 |             50 |    10.41 |           0 |
| nanovdb_like     | checkered_32     |     480 |       11936 |     24.86 |            0.03 |           0.08 |           23.09 |             30 |     0.15 |           0 |
| svdag64_no_dedup | brick_32         |    4961 |      143456 |     28.91 |            0.04 |           0.05 |           25.33 |             40 |     0.07 |           0 |
| svdag64_dedup_on | brick_32         |    4961 |      150496 |     30.33 |            0.84 |           1.28 |            23.2 |             30 |     2.55 |           0 |
| nanovdb_like     | brick_32         |     448 |       11936 |     26.64 |            0.03 |            0.1 |           23.58 |             30 |     0.04 |        2688 |
| svdag64_no_dedup | voxel_lab_32     |    4818 |      286816 |     59.53 |            0.03 |           0.05 |           23.89 |             40 |     0.12 |           0 |
| svdag64_dedup_on | voxel_lab_32     |    4818 |      295152 |     61.26 |            0.67 |           1.18 |           24.62 |             40 |     2.81 |           0 |
| nanovdb_like     | voxel_lab_32     |     512 |       11936 |     23.31 |            0.02 |           0.03 |           23.16 |             30 |     0.08 |       31709 |
| svdag64_no_dedup | sparse_random_32 |    4070 |      286816 |     70.47 |            0.03 |           0.04 |           22.86 |             30 |      0.1 |           0 |
| svdag64_dedup_on | sparse_random_32 |    4070 |      295136 |     72.51 |            0.65 |           0.73 |           30.69 |             50 |     2.35 |           0 |
| nanovdb_like     | sparse_random_32 |     512 |       11936 |     23.31 |            0.02 |           0.03 |           22.81 |             30 |     0.07 |       31387 |

## Notes

- SVDAG-on-64-tree (no dedup) = current ProjectV mainline per-chunk storage baseline.
- SVDAG-on-64-tree (dedup ON) = Stage 1.2 lazy SVDAG (SetDeduplicationEnabled(true)).
- NanoVDB-like = 4-level B+tree (Root[8] -> Upper[8] -> Lower[8] -> Leaf[8]).
  Same essential structure as openvdb/nanovdb/NanoVDB.h (multi-level fixed-depth
  B+tree with bitmask skips for uniform children). This prototype uses 2^3=8
  branching (octree-style, 4 levels cover 32^3 chunks exactly) so all bitmasks
  fit in u8 and the tree depth matches our chunk size. NanoVDB's actual layout
  uses 32^3/16^3/8^3 branching which is unfavorable for 32^3 chunks (each upper
  covers 64^3 cells, way more than needed).
  Per-node sizes: Root=40 B, Upper=40 B, Lower=40 B, Leaf=16 B +
  fixed 736 B grid overhead (GridData 672 B + TreeData 64 B per NanoVDB.h).
- verify_mismatches MUST be 0 for all rows (byte-exact correctness vs flat voxels).
