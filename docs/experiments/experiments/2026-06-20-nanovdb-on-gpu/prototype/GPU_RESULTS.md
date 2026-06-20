# Results — GPU-side: SVDAG-on-64-tree vs NanoVDB-aligned (chunkSize=8, RTX 3060 Ti)

GPU: NVIDIA GeForce RTX 3060 Ti (Vulkan 1.4).
Rays per dispatch: 65536. Workgroup size 64 (= 2x subgroupSize=32).
Warm-up=10, Measure=50 dispatches per scene. Median ns reported.
Mrays/s = rays / (ns / 1e9) / 1e6.

| Kernel          | Scene           |   Mean ms | Mrays/s | Verify mism | GPU bytes |
|:----------------|:----------------|----------:|--------:|------------:|----------:|
| svdag64         | solid_8         | 0.0499494 | 1312.05 |           0 |      2640 |
| nanovdb_aligned | solid_8         | 0.0518266 | 1264.53 |           0 |      1128 |
| svdag64         | ground_8        |  0.103443 |  633.55 |           0 |      1584 |
| nanovdb_aligned | ground_8        |  0.058953 | 1111.67 |           0 |       392 |
| svdag64         | brick_8         | 0.0500806 | 1308.61 |           0 |      2640 |
| nanovdb_aligned | brick_8         | 0.0533082 | 1229.38 |           0 |      1128 |
| svdag64         | voxel_lab_8     |  0.120083 | 545.758 |           0 |      2640 |
| nanovdb_aligned | voxel_lab_8     |  0.057191 | 1145.91 |           0 |       888 |
| svdag64         | sparse_random_8 |   0.13137 | 498.865 |           0 |      2640 |
| nanovdb_aligned | sparse_random_8 | 0.0618221 | 1060.07 |           0 |       780 |
