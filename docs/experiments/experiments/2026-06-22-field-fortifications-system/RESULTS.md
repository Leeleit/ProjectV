# Results — 2026-06-22-field-fortifications-system

## Summary Table

| Strategy | Scene | Mean Latency (µs) |
| :--- | :--- | :--- |
| A_NaivePerVoxel | anti_tank_ditch_50m | 16.067 |
| A_NaivePerVoxel | beach_obstacle_line_30 | 6.029 |
| A_NaivePerVoxel | defensive_complex_20 | 37.941 |
| A_NaivePerVoxel | dragon_teeth_field_48 | 3.671 |
| A_NaivePerVoxel | road_block_urban | 0.560 |
| B_TemplateAABB_RLE | anti_tank_ditch_50m | 4.424 |
| B_TemplateAABB_RLE | beach_obstacle_line_30 | 2.644 |
| B_TemplateAABB_RLE | defensive_complex_20 | 12.609 |
| B_TemplateAABB_RLE | dragon_teeth_field_48 | 2.256 |
| B_TemplateAABB_RLE | road_block_urban | 0.592 |
| C_PrefabPhysicsHull | anti_tank_ditch_50m | 5.390 |
| C_PrefabPhysicsHull | beach_obstacle_line_30 | 7.935 |
| C_PrefabPhysicsHull | defensive_complex_20 | 24.099 |
| C_PrefabPhysicsHull | dragon_teeth_field_48 | 3.272 |
| C_PrefabPhysicsHull | road_block_urban | 2.465 |
| D_HierarchicalMultiLayer | anti_tank_ditch_50m | 4.760 |
| D_HierarchicalMultiLayer | beach_obstacle_line_30 | 2.887 |
| D_HierarchicalMultiLayer | defensive_complex_20 | 13.049 |
| D_HierarchicalMultiLayer | dragon_teeth_field_48 | 3.262 |
| D_HierarchicalMultiLayer | road_block_urban | 0.785 |
| E_AdaptiveTerrain | anti_tank_ditch_50m | 5.184 |
| E_AdaptiveTerrain | beach_obstacle_line_30 | 3.515 |
| E_AdaptiveTerrain | defensive_complex_20 | 17.709 |
| E_AdaptiveTerrain | dragon_teeth_field_48 | 5.945 |
| E_AdaptiveTerrain | road_block_urban | 0.958 |

## Hypothesis Validation

- **H1 (Latency < 200 µs):** ✅ **CONFIRMED MASSIVELY**. Even the worst-case configuration (`A_NaivePerVoxel` on `defensive_complex_20` with 20 structures and 40,640 voxels) took only **37.94 µs** of CPU time. This represents less than 0.12% of a single 30 Hz frame (33.3 ms).
- **H2 (Template AABB RLE (B) vs Naive Mutation (A)):** ✅ **CONFIRMED**. Strategy B achieved significant speedups over A on all scenes with a high voxel count. For example, on the `anti_tank_ditch_50m` scene, it achieved a **3.63× speedup** (4.424 µs vs 16.067 µs). On the `defensive_complex_20` scene, it achieved a **3.01× speedup** (12.609 µs vs 37.941 µs).
- **H3 (Physics/Terrain Overhead < 50 µs):** ✅ **CONFIRMED**. The adaptive terrain strategy `E_AdaptiveTerrain` conforms obstacles to the ground profile and runs within **17.71 µs** worst-case (`defensive_complex_20` scene), which is well below the 50 µs limit.

## Observations

1. **RLE Copy Advantage:** The bulk RLE copy strategy (B) is the most efficient for building dense structures. It reduces the per-voxel overhead dramatically, as expected.
2. **Physics Hull (C) Overhead:** The `C_PrefabPhysicsHull` strategy incurs a higher per-structure registration overhead (modeled after Jolt body creation). For smaller scenes like `road_block_urban` (3 structures, 960 voxels), strategy C takes **2.465 µs** compared to A's **0.560 µs** and B's **0.592 µs**. However, for dense/high-voxel count structures, the bulk paint optimization in C outweighs the initialization cost.
3. **Hierarchical (D) Overhead:** The `D_HierarchicalMultiLayer` strategy manages layered placement (e.g. anti-tank ditch followed by barbed wire, hedgehogs, and sandbags) at a very low CPU overhead, yielding results comparable to B but with orchestration benefits.
