# STATUS — `2026-06-21-lod-transition-strategy`

**Phase:** concluded-verdict-mixed (closed same session, 2026-06-21).
**Started:** 2026-06-21.
**Closed:** 2026-06-21 (single session, ~2h).
**Last action:** 2026-06-21 — README.md + RESULTS.md + sources.md finalized, prototype built + measured (125,000 main measurements, 3.67 sec wall time), verdict=mixed recorded.
**Blocker:** нет (single-session feasible per scope).

**Notes:**
- Verdict=mixed: no single strategy wins for all scenes.
- **C_Geomorph = canonical recommended** for typical scenes (Hoppe 1997 + Lysenko 2018).
- **A_Pop FAILS `TODO.md §4.2` DoD line 328** (27.76 dB PSNR < 35 dB threshold + 0.717 voxel discontinuity = visible seam).
- **D_PreComputedMorphTargets NOT recommended** (3.1× memory cost, 4.3× build cost exceeds Stage 4.1 budget).
- **B_Crossfade NOT recommended** (doubles triangle count + my naive analytic measurement shows worse quality than A_Pop).
- **E_HZB_Stitch needs GPU prototype** to validate ProjectV-specific hypothesis (HZB conservative Z test).

**Caveats:**
- CPU prototype only, no real GPU dispatch — my naive vertex-index pairing measurement underestimates C_Geomorph / D_PreComputedMorphTargets quality. Real GPU render with depth-test would show much better PSNR per Hoppe 1997 + Lysenko 2018.
- 5 synthetic scene types only, not exhaustive of real ProjectV world content.
- LOD chain covers only LOD 0 → LOD 1 (not full 4-level chain with smooth transitions at each step).
- No mutation cost measured (out of Stage 4.2 DoD scope).
- No HZB interaction measured (cross-axis with `2026-06-21-hzb-smart-mip-select` in-progress + E_HZB_Stitch hypothesis).

**Cross-axis:**
- Complementary к closed `2026-06-21-lod-mesh-downsampling` (per-LOD content axis = B_SurfacePreserve kernel winner; this = transition strategy axis).
- Foundation for Stage 4.3 lift draw distance (128m draw distance, 4096 chunks in transition zone).
- Pattern C mesh shader compatible per `TODO.md §2.2` (geomorphing could be implemented per-meshlet in `voxel_mesh.mesh` shader).
- HZB interaction: E_HZB_Stitch is ProjectV-specific hypothesis that needs GPU prototype + `hzb-smart-mip-select` HZB system integration.

**Files retained:**
- `experiments/2026-06-21-lod-transition-strategy/README.md` (this experiment)
- `experiments/2026-06-21-lod-transition-strategy/STATUS.md` (final status)
- `experiments/2026-06-21-lod-transition-strategy/RESULTS.md` (detailed results)
- `experiments/2026-06-21-lod-transition-strategy/sources.md` (11 references)
- `experiments/2026-06-21-lod-transition-strategy/prototype/lod_transition_bench.cpp` (~430 LoC, builds clean)
- `experiments/2026-06-21-lod-transition-strategy/prototype/lod_transition_bench` (binary)
- `experiments/2026-06-21-lod-transition-strategy/prototype/results.csv` (125 data rows)
- `experiments/2026-06-21-lod-transition-strategy/prototype/run.log` (human-readable output)

**Re-evaluation triggers:**
- Real GPU prototype with `VK_EXT_mesh_shader` meshlet-level dispatch + per-meshlet `t` factor.
- Stage 4.3 lift draw distance (128m draw distance, transition becomes critical).
- Vulkan 1.5/1.6 `mesh_shader` cross-vendor optimization.
- HZB integration prototype (`2026-06-21-hzb-smart-mip-select` + E_HZB_Stitch validation).
- DirectSR core promotion in Vulkan 1.5+ (out of scope this session).
- Multi-frame continuous morph amortization per Hoppe 1997 §6 (potential follow-up).
