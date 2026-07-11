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
- Probe-grid aliasing (small static dots on a regular 8 m spacing) on the water back face by replacing the sharp Chebyshev visibility test in `SampleRtxGiProbeIrradiance` (`voxel.frag`) with a smooth Gaussian falloff. The previous `p = variance / (variance + g*g)` transition was sharp for probes inside opaque geometry (water, glass) where `variance` is small (~0.1–0.5) and `mean` is small (~1 m), producing visible aliased dots that jumped on camera motion. The new `exp(-distExcess^2 / (2 * max(variance, 0.25)))` falloff smooths the transition over ≥0.5 m while preserving occlusion behavior.
- DDA bug for rays starting inside a non-air voxel in `TraceVoxelIntersection` (`voxel.frag` and `probe_update.comp`). When a refraction ray started just past a water back face (re-entering the water interior) or a probe at a position inside water/glass geometry, the DDA committed at `tCurrent = tMin` on the first iteration, and the normal computed in the outer hit block was derived from the tiny position offset (5 mm along the ray) instead of the actual wall direction. The wrong normal caused the shadow ray inside `EvaluateVoxelLighting` to escape to sky for ALL directions from a probe inside opaque geometry, so probes stored bright "sky" values in their octahedral irradiance map and shadow Factor stayed 0 → wrong diffuse GI on the water back face. Fix: detect rays starting inside a non-air voxel and advance `tMin` past the wall of the starting voxel before the DDA loop runs. Applied to both `voxel.frag::TraceVoxelIntersection` and `probe_update.comp` (in both the main ray-query DDA and the inline shadow-ray DDA inside `EvaluateVoxelLighting`).
- Hit-normal bug in `TraceVoxelIntersection` (`voxel.frag` and `probe_update.comp`): the normal returned from the lighting-eval hit block was computed from the 5 mm position offset (`insidePos = worldHitPos + dir * 0.005`, then picking the closest of 6 face directions from the voxel center diff). The offset is tiny compared to the voxel-grid spacing, so its discretization picked whichever face happened to be marginally closer after FP rounding — this frequently produced a normal that did NOT correspond to the wall the ray actually exited through. The wrong normal leaked into `EvaluateVoxelLighting`'s shadow ray: for refraction hits just past a water back face, the shadow ray often escaped into the air gap above the water instead of finding more water, giving `shadowFactor = 1` → bright "sky" color in the refraction result → small bright dots on the water back face visible through the glass sphere (matches user-reported symptom: dots NOT visible in any debug view, only in Final — because refraction is not exposed as a separate debug view). Fix: compute the hit normal from the ray's dominant-axis direction (the wall a DDA ray exits the voxel through is perpendicular to the axis with the largest `|dir|` component; outward normal is `sign(dir.dominantAxis) * e_dominantAxis`). Applied to both `voxel.frag` and `probe_update.comp`.
- TAA motion-vector reprojection was jittered on both prev and curr frames. The projection matrix in
  `BuildGraphicsPushConstants` (`src/app/Camera.cpp:200-244`) baked per-frame sub-pixel jitter into `M[2][0..1]`, and
  `FramePreparation.cpp:280` saved that jittered matrix as `taaPrevViewProjectionMatrix`. With `taaJitterScale > 0`,
  motion vectors carried a synthetic sub-pixel offset that mis-targeted the previous-frame sample in
  `voxel.frag:1391-1402`, producing screen-wide shake. Fix: added a parallel `viewProjectionUnjittered` field to
  `GraphicsPushConstants` (offset 128, struct size 128→192 bytes), built by `BuildGraphicsPushConstants` with
  `M[2][0..1] = 0`, and use it for **both** the stored prev matrix and the current-frame projection read in the motion
  vector compute. Byte-exact mirrors in `voxel.vert` and `probe_update.comp` updated accordingly. `RtxGiProbes.cpp`
  push-constant range also bumped from literal `128` to `sizeof(GraphicsPushConstants)` to track the C++ struct.
- TAA history buffer (`taaPrevViewProjectionMatrix` in `RenderState`) is now stored unjittered, so cross-frame
  reprojection contains only the real camera transform.
- TAA defaults were tuned for invisible AA but produced effectively no smoothing at typical settings (`taaBlend=0.10`,
  `taaJitterScale=0.0`, `taaNeighbourhoodRadius=1`). The previous combination forced the operator to crank jitter to
  ≥1.5 to see any AA, which then exposed the rendered-scene sub-pixel wobble per frame. New defaults: `taaBlend=0.40` (
  4× stronger history weight; per-frame visible shift at jitter=0.75 drops from 0.675 px to 0.45 px),
  `taaJitterScale=1.0` (jitter ON by default; the previous 0.0 silently disabled TAA sub-pixel reconstruction),
  `taaNeighbourhoodRadius=1` (3×3 for the YCoCg clamp; CAS corner samples are now collected from a separate fixed 3×3
  window so the corner radius no longer tracks the outlier radius — at large radii the corners would span multiple
  texels and the CAS high-pass would over-shoot edges, producing visible halos).
- CAS corner samples in `taa_resolve.frag::GetSceneColorRange` no longer scale with `taaNeighbourhoodRadius`. CAS needs
  corners close to the center pixel (±1 texel) to compute a meaningful local high-pass; the previous implementation
  reused the outlier-rejection loop and pulled corners from the same radius, so at radius 3 (7×7) the corners were 6
  texels apart and the resulting high-pass over-shot contrast edges, producing outlines/halos around every element.
  Fixed by sampling CAS corners in a dedicated ±1-texel window.
- CAS sharpening formula in `taa_resolve.frag` was inverted: `sharpenAmount = (1.0 - blend) * max` produced strong
  sharpening at low blend and weak sharpening at high blend, exactly the opposite of what temporal accumulation
  requires. Fixed to `sharpenAmount = blend * max`: more temporal averaging now correctly applies more sharpening to
  recover lost detail.
- TAA color-space mismatch (the root cause of the outlines and persistent shake). When TAA was on, `voxel.frag` and
  `model.frag` wrote linear HDR (pre-tonemap, pre-grading) to the scene color, but the TAA resolve blended it with the
  previous frame's LDR history (post-tonemap+grading) — a mathematically undefined operation that produced the
  halos/обводка and inconsistent per-frame brightness. Fix: tonemap + color grading moved into `voxel.frag` and
  `model.frag` (applied unconditionally before any output), so the scene color and the history are both LDR.
  `taa_resolve.frag` simplified to a pure LDR blend + CAS + output; removed `ApplyTaaToneMap`/`ApplyTaaColorGrading` and
  the post-blend exposure multiplication (already applied in the voxel pass). The exposure-multiplication-after-blend
  was a second bug: applying `x * exposure` after a non-linear tonemap is not equivalent to `tonemap(x * exposure)`, so
  the per-frame brightness oscillated as the blend factor changed.
- TAA motion vector attachment conflict between voxel and model passes. Both passes wrote to `taaSceneColorTarget` (
  Location 1), and the model pass ran after the voxel pass, so for fragments covered by a model the voxel output was
  overwritten. Worse, the TAA resolve then reprojected the model output using the voxel's motion vector (the only one
  written), so model frames were reprojected against their own background's motion — the second TAA ghosting source.
  Fix: model pass now also writes a motion vector (Location 3) so each fragment has the correct motion source. The model
  pipeline was extended to four attachments (color, scene color, layer history, motion vector) and `ModelPushConstants`
  grew to 192 bytes (added `viewProjectionUnjittered` for the model motion vector compute). The TAA resolve is
  unchanged — it now reads the correct motion vector per fragment, with the load op preserving the voxel motion vector
  for non-model fragments.
- MSAA skeleton added (`AntialiasingMode` enum, `MsaaSamplesForMode`/`IsTaaEnabledForMode` helpers in
  `src/render/AntialiasingMode.hpp`, `taaSceneColorMsTarget` field, conditional dynamic rendering MS resolve, pipeline
  multisampling derived from `aaMode`). Default `aaMode = TAA` — single-sample TAA active. MSAA path is inert until
  `VK_EXT_multisampled_render_to_single_sampled` is enabled or all main-pass attachments are converted to
  multi-sampled (follow-up). Fixed a related regression: `colorAttachment1.storeOp` had been hardcoded to `DONT_CARE`,
  which is correct for the MSAA path (attachment consumed by auto-resolve) but invalid for the single-sample path (TAA
  resolve reads the attachment as a sampled image, which requires `STORE`). Restored `STORE` for the single-sample path.

### Added

- `viewProjectionUnjittered` to `GraphicsPushConstants` (`src/core/Types.hpp:213-229`, offset 128, `static_assert`
  enforces layout). C++ struct mirrors byte-exact in `voxel.frag`, `voxel.vert`, `probe_update.comp` push constant
  blocks.
- Two unit tests in `tests/GraphicsPushConstantsTests.cpp` covering the unjittered field: zero-jitter equivalence with
  `viewProjection`, and jitter-only difference in `M[2][0..1]`. Test count: 6→8.

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