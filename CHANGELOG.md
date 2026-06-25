# Changelog

All notable changes to ProjectV are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

**Pre-reset history (2026-02-24 → 2026-06-24, 274 commits):** archived at
`legacy/docs/archive/2026-06-24-pre-reset-snapshot/CHANGELOG.md`. Treat as historical
artifact — see WARNING header in that file.

**Active doc state:**

- Design rationale and ongoing decisions → `agent/knowledge.md`
- Session log and active tasks → `agent/workspace.md`
- Roadmap and priorities → `TODO.md`
- Agent protocol → `AGENTS.md`

## [Unreleased] — post-reset baseline (2026-06-24)

### Fixed

- Flickering white/light dots inside water volume (refraction and global illumination) by adding a `1e-4` precision offset to `tMin` on chunk entry in `voxel.frag` and `probe_update.comp` to prevent ray query boundary misses.
- Probe-grid aliasing (small static dots on a regular 8 m spacing) on the water back face by replacing the sharp Chebyshev visibility test in `SampleRtxGiProbeIrradiance` (`voxel.frag`) with a smooth Gaussian falloff. The previous `p = variance / (variance + g*g)` transition was sharp for probes inside opaque geometry (water, glass) where `variance` is small (~0.1–0.5) and `mean` is small (~1 m), producing visible aliased dots that jumped on camera motion. The new `exp(-distExcess^2 / (2 * max(variance, 0.25)))` falloff smooths the transition over ≥0.5 m while preserving occlusion behaviour.
- DDA bug for rays starting inside a non-air voxel in `TraceVoxelIntersection` (`voxel.frag` and `probe_update.comp`). When a refraction ray started just past a water back face (re-entering the water interior) or a probe at a position inside water/glass geometry, the DDA committed at `tCurrent = tMin` on the first iteration, and the normal computed in the outer hit block was derived from the tiny position offset (5 mm along the ray) instead of the actual wall direction. The wrong normal caused the shadow ray inside `EvaluateVoxelLighting` to escape to sky for ALL directions from a probe inside opaque geometry, so probes stored bright "sky" values in their octahedral irradiance map and shadow Factor stayed 0 → wrong diffuse GI on the water back face. Fix: detect rays starting inside a non-air voxel and advance `tMin` past the wall of the starting voxel before the DDA loop runs. Applied to both `voxel.frag::TraceVoxelIntersection` and `probe_update.comp` (in both the main ray-query DDA and the inline shadow-ray DDA inside `EvaluateVoxelLighting`).
- Hit-normal bug in `TraceVoxelIntersection` (`voxel.frag` and `probe_update.comp`): the normal returned from the lighting-eval hit block was computed from the 5 mm position offset (`insidePos = worldHitPos + dir * 0.005`, then picking the closest of 6 face directions from the voxel centre diff). The offset is tiny compared to the voxel-grid spacing, so its discretisation picked whichever face happened to be marginally closer after FP rounding — this frequently produced a normal that did NOT correspond to the wall the ray actually exited through. The wrong normal leaked into `EvaluateVoxelLighting`'s shadow ray: for refraction hits just past a water back face, the shadow ray often escaped into the air gap above the water instead of finding more water, giving `shadowFactor = 1` → bright "sky" colour in the refraction result → small bright dots on the water back face visible through the glass sphere (matches user-reported symptom: dots NOT visible in any debug view, only in Final — because refraction is not exposed as a separate debug view). Fix: compute the hit normal from the ray's dominant-axis direction (the wall a DDA ray exits the voxel through is perpendicular to the axis with the largest `|dir|` component; outward normal is `sign(dir.dominantAxis) * e_dominantAxis`). Applied to both `voxel.frag` and `probe_update.comp`.

### Changed

- Archived `CHANGELOG.md`, `COMMENTS.md`, `agent/knowledge.md`, `agent/workspace.md`
  to `legacy/docs/archive/2026-06-24-pre-reset-snapshot/`. Live files now start from
  minimal baseline; full pre-reset content preserved in archive (with WARNING header).
- Squashed all 274 pre-reset commits into a single `chore(reset): pre-fresh-start
  baseline` initial commit. Original commit history preserved in
  `legacy/docs/archive/2026-06-24-pre-reset-snapshot/git-history.md`.
- Deleted local branches `forge/rtx-feature-lab`, `forge/backlog-diversification`.

### Notes

- Future commits follow `AGENTS.md` §5.1 (commit message format) strictly.
- Operator policy: do not cite pre-reset documentation as authoritative.