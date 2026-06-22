# RESULTS — 2026-06-22-in-game-commo-ping

## Setup

- **Compiler:** Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`
- **Host:** Zen 3 5800X, governor=powersave (per `hardware-profile.md §1`)
- **Protocol:** 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements
- **World:** 4×4×1 chunks (64×64×16 = 1,048,576 voxels), 8³ per chunk
- **Camera:** fixed at (2.0, 8.0, 2.0), random ping targets within scene bounds

## Overall means (cross-scene, cross-seed)

| Strategy | Mean (ns) | Accuracy (%) | Voxels traversed |
|:---------|:----------|:-------------|:-----------------|
| A_NoPing | 0.0 | 69.73 | 0.0 |
| B_PointMarker_NoContext | 0.0 | 69.73 | 0.0 |
| C_AmanatidesWoo_DDA | 127.6 | 78.26 | ~50 |
| D_Hierarchical_DDA | 125.6 | 78.37 | ~31 |
| E_MultiSample_AreaPing | 587.6 | 63.44 | ~260 |

## Per-strategy × per-scene means (ns / accuracy)

| Scene | A_NoPing | B_NoCtx | C_DDA | D_Hier | E_Multi |
|:------|:---------|:--------|:------|:-------|:--------|
| s1_open_terrain | 0 / 87.8% | 0 / 87.8% | 148 / 80.0% | 142 / 79.1% | 730 / 63.1% |
| s2_urban_building | 0 / 83.8% | 0 / 83.8% | 154 / 63.5% | 152 / 62.5% | 714 / 43.5% |
| s3_vehicle_encounter | 0 / 87.7% | 0 / 87.7% | 148 / 79.9% | 138 / 79.0% | 648 / 63.0% |
| s4_mixed_battlefield | 0 / 86.8% | 0 / 86.8% | 158 / 70.5% | 156 / 73.9% | 746 / 50.2% |
| s5_underground_cave | 0 / 2.6% | 0 / 2.6% | 30 / 97.4% | 40 / 97.4% | 100 / 97.4% |

## Hypothesis validation

### H1 (cost <10 µs/ping) ✅ CONFIRMED massively
- Worst-case mean: E_MultiSample at s4_mixed_battlefield = 746 ns = **13× under 10 µs budget**
- Worst-case p99: E_MultiSample at s1_open_terrain (seed 31337) = 3,360 ns = 3× under budget
- C and D at ~130-160 ns = **62-77× under budget**
- Even with 100 simultaneous pings/frame: worst 0.336 ms = 1.0% of 30 Hz frame

### H2 (>95% context accuracy for C, D) ⚠️ PARTIAL — CONFIRMED for cave, REJECTED for open scenes
- Underground cave (s5): **97.4%** — meets target ✅
- Open terrain (s1): **79-80%** — below 95% target ❌
- Urban (s2): **63%** — well below target ❌
- *Root cause:* accuracy metric compares `detected_voxel` (first non-air voxel hit by ray) vs `expected_voxel` (voxel at random target point). When target is in open air and ray traverses to nearest surface, detected ≠ expected. This is inherent to the random-ping benchmark design — a **player** aims at visible surfaces, not void points. In practice, accuracy on player-aimed pings is expected to be significantly higher.

### H3 (Hierarchical 2-5× faster than full DDA) ❌ REJECTED
- C (AmanatidesWoo): 127.6 ns mean
- D (Hierarchical): 125.6 ns mean — **only 1.6% faster**
- D traverses fewer voxels (31 vs 50 = 1.6× reduction), but chunk-level AABB test + entry-point computation overhead cancels savings for this world size (4×4×1 chunks)
- For larger worlds (64×64×64+), D would likely show 2-3× advantage due to chunk-skip on non-uniform density

### H4 (Multi-sample improves ambiguous accuracy) ❌ REJECTED — accuracy worse
- E is 4.6× more expensive than C (cost hypothesis confirmed) ✅
- But accuracy is **LOWER** than C (63.4% vs 78.3%) ❌
- Majority vote among 5 samples with ±0.05 angular spread **dilutes** the correct answer: when 2/5 or 3/5 samples miss the target voxel (hitting adjacent air/background), the majority votes wrong
- Cave scene: all 5 rays hit same wall → 97.4% = no dilution (equal to C/D)
- *Recommendation:* E is useful only for area-ping semantics ("this general area") where context detection accuracy is not critical

## Key observations

1. **B_PointMarker_NoContext** is not a "better" strategy than C/D — it trades all context for speed. Its 69.7% accuracy is a coincidence (many random targets are AIR → expected=AIR, detected=AIR).
2. **Per-seed variance is high for E_MultiSample** (e.g., s1 seed 31337: p99 3360 ns = 4× median 970 ns). Some rays in the 5-sample spread exit the world or traverse disproportionately far.
3. **Cave scene is anomalously fast** (30-40 ns for C/D) because all rays immediately hit nearby walls (1 voxel traversal). accuracy is 97.4% because both expected and detected are STONE.
4. **Accuracy interpretation note:** A/B 69.7% is not "6.9/10 correct" — it's "6.9/10 random points are in AIR". C/D 78.3% indicates the DDA correctly identifies the first non-air voxel ~78% of the time. The 22% mismatch is cases where the random target point's voxel ≠ the first voxel hit by the ray (e.g., ray hits a building wall 2 voxels before reaching the target location).

## Limitations

- Synthetic voxel scenes with uniform random distributions (not real gameplay chunk layouts)
- No GPU raytracing comparison
- No network bandwidth measurement for ping events
- No UI rendering cost (radial menu, ping icons, minimap markers)
- Static camera position (no player movement during ping)
- Expected voxel = voxel at target position, not "what the player intended to ping"
