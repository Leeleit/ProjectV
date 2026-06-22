# STATUS — `2026-06-21-boid-flocking-steering-axis`

**Phase:** _concluded-verdict-mixed_ ✓ **CLOSED**.

**Closed:** `2026-06-21` (single session, ~2.5h claim + prototype + bench + analysis).

**Last action (2026-06-21):**

- Prototype `prototype/boid_bench.cpp` written (~530 LoC C++26, build green 0 warnings after 4 fix
  iterations: `operator/` для Vec3, перенос `_mm256_reduce_add_ps` выше использования, unused
  `r` warning, `csv.flush()` после abort-resilient output).
- Full benchmark completed: 85,000 main measurements (4 strategies × 5 scenes × 5 seeds × 1000 iter
  + 10 warmup), wall time **518.09 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
- Output: `prototype/build/results.csv` (86 rows = 1 header + 85 data, 15 skip-rows for A_Naive
  @ N≥5000 impractical).
- All 5 hypothesis validations completed per [`RESULTS.md`](./RESULTS.md) §4:
  - H1 "<0.5 ms @ N=10k" REJECTED (10× over)
  - H2 ">100× speedup" REJECTED (actual ~15-18×)
  - H3 "C_KDTree balanced" PARTIAL (1.18-1.73× faster than B, not 100×)
  - H4 "D_SIMD 4-8× over B" REJECTED (0.92×, slightly slower)
  - H5 "all cross 5-10% threshold" CONFIRMED massively (150-162% relative gain)
- **Verdict:** `mixed` — C_KDTree validated as CPU winner for N=100-10k; GPU compute required for
  N≥50k; D_SIMD_AVX2 negative result (SIMD overhead at uniform low-density distributions).

**Blocker:** нет.

**Next tick:** N/A (closed). Re-evaluation triggers documented в README §7 Integration
recommendation.

**Headline stats (mean ns/iter, N=1000):**
- A_Naive = 832 µs (baseline)
- B_SpatialHash = 332 µs (2.51× speedup, 151% gain)
- C_KDTree ⭐ = 318 µs (2.62× speedup, 162% gain) — **UNIVERSAL WINNER**
- D_SIMD_AVX2 = 373 µs (2.23× speedup, 123% gain)

**Cross-axis:** orth ко всем in-progress parallel (data-driven-vehicle-weapon-definitions Tier 0 only);
complementary к closed `flow-field-pathfinding-10k-units` + `multi-resolution-collision-broadphase` +
`ecs-1m-entities-bottleneck` + `mesh-shader-mega-instancing` + `flood-fill-visgraph-culling` +
`hierarchical-tactical-ai-btree`.

**New axis opened:** first dedicated boid/flocking steering axis в 100+ closed experiments; opens
Stage 6+ military sandbox drone swarms + Stage 5.x ambient wildlife + Tier 2 AI prerequisites
(`drone-swarm-ai` h + `formation-flight-wingman` m + `flocking-wildlife-ambient` m).