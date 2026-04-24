# Status

Short active snapshot on top of `TODO.md`; no roadmap duplication.

Updated: `2026-04-24`

---

## 1. Now

- Project phase: `pre-MVP alpha / working vertical slice`.
- Mainline still has the runnable voxel sandbox slice with interaction, control modes, snapshots, lightweight editor, profiling, smoke probes, and the current `walk` controller.
- Legacy reference docs are now consolidated under a single `legacy/docs` tree: `philosophy/`, `standards/`, and `libraries/` remain the active reference roots, `libraries/` again preserves the broad per-library corpus instead of only minimal summaries, restored `guides/`, `tutorials/`, and `examples/` keep the older learning material inside the same unified root, `architecture/` now also exposes its speculative `future/` slice explicitly, and former `latest` / `old` planning duplicates now live only under `legacy/docs/archive/roadmaps/`.
- Latest closed slice: the current lighting baseline is now not only runtime-tunable and runtime-capturable, but also materially closer to the original `10.5.1` contract without washing the scene flat. The sun-shadow path still stays intentionally simple (`2048x2048`, weighted `5x5` PCF, dedicated all-occluder `shadowIndirectBuffer`, active-chunk shadow fit, authored sun vector flipped only inside the CPU shadow camera, angle-aware receiver bias plus a small sun-direction receiver offset), yet the direct-light side no longer lives on ad-hoc `spec power + shininess`. `VoxelMaterialVisual` now packs explicit `AO / roughness / metallic / reflectance` data plus transmission tint and fog/emissive/ambient/direct-response hooks, and `voxel.frag` shades the sun with a `GGX + Fresnel-Schlick + Smith` baseline while still honoring authored ambient and diffuse weights so shadow contrast does not disappear into fill light. Real live capture passes still exist on top of that baseline in both opaque-heavy presets: `ChunkGrid` ships with tighter defaults (`depth=0.0010`, `normal=0.0040`, `filter=1.30`) based on `C` captures, while `MeshingStress` now has a reproducible reference shot (`cam -25 19 25`, `look 0.62 -0.48 -0.62`) that produces meaningful diffs for bias candidates even though the tested moderate variants still did not beat the current code baseline clearly enough to justify changing it. The latest warning-cleanup follow-up on top of that baseline also removed the remaining current-source inspection nits in screenshot capture, lighting debug helper contracts, shadow-projection use-sites, descriptor pool constants, and test BMP/debug-HUD helpers; a fresh `problems/tests/` pass then also closed the remaining helper nits in `tests/VoxelWorldTests.cpp` and left only a deliberate file-level `CppDFAUnreachableFunctionCall` suppression for the bespoke single-TU test runner.
- Current session also closed the next fresh root-level `problems/` export follow-up on top of that same shadow baseline:
  remaining inspection tails in `SceneResources.cpp/.hpp`, `ShadowProjection.cpp/.hpp`, `ScreenshotCapture.cpp`,
  `DebugHud.cpp`, `Types.hpp`, `FramePreparation.cpp`, `VoxelMaterials.cpp/.hpp`, `VoxelWorldTests.cpp`, and the
  touched shadow docs are now gone from the current-source pass. The cleanup stayed bounded: helper APIs that never
  accepted `nullptr` now use references, CSM matrix copy sites no longer depend on iterator+cast boilerplate, and the
  SDL Vulkan include contract now lives explicitly in `VulkanBootstrap.cpp` instead of leaking through `Types.hpp`.
  Sequential `windows-clang-debug` build + `ctest` are green again; no runtime smoke was needed for this pass.
- Current session closed the immediate post-BRDF capture gap: `ChunkGrid` and the fixed `MeshingStress` reference shot now have scripted `FINAL` / `SHDW` captures under `build/windows-clang-debug/lookdev-captures/20260424-brdf-baseline-v2/`, and the runtime has env-driven startup camera + capture automation so this can be regenerated without manual camera/input timing. The first scripted pass also exposed a real screenshot readback race; `Renderer.cpp` now signals present after all commands so the post-render transfer copy cannot race with presentation.
- Current session also closed the ambient/environment fill contract step: `postProcess.y` is now the per-preset environment diffuse intensity, detailed HUD and capture sidecars expose it, and refreshed `FINAL` / `AMB` / `SHDW` captures live under `build/windows-clang-debug/lookdev-captures/20260424-env-fill-v1/`.
- Current session also fixed the first obvious local ambient-occlusion bug on top of that fill contract: compute meshing now stores a cheap per-face ambient-visibility byte in `PackedSceneVoxelFace`, `voxel.frag` multiplies sky/horizon/ground fill by it, and closed voxel cavities no longer read as if an upward normal automatically saw the whole sky. This is still a bounded voxel-neighborhood visibility term, not `SSAO/GTAO`.
- Current session also closed the minimal exposure/grading contract step: `VoxelSceneLighting.colorGrading` now carries
  white point / contrast / saturation / lift, `voxel.frag` and clear color share the same post-tone-map grading path,
  detailed HUD and capture sidecars expose the values, and refreshed captures live under
  `build/windows-clang-debug/lookdev-captures/20260424-grading-v1/`.
- Current session also closed the first auto-exposure policy step without adding an HDR histogram pass:
  `VoxelSceneLighting.exposureControl` now carries metering mode / target key / min exposure / max exposure, current
  presets use CPU-side `SceneKey` metering, and refreshed captures live under
  `build/windows-clang-debug/lookdev-captures/20260424-auto-exposure-v1/`.
- Current session also added opaque anchor geometry to `VoxelLab`: a small right-side solid stepped marker outside the
  glass/fluid sphere gives the current opaque-only sun-shadow path a readable caster/receiver in the demo scene.
  Refreshed captures live under `build/windows-clang-debug/lookdev-captures/20260424-voxel-lab-anchor-v1/`.
- Current session also moved `10.5.2` from CSM planning to the first real CSM render path: the sun shadow image is a
  4-layer depth array, the shadow pass renders each cascade with its own matrix, the final shader samples a
  `sampler2DArrayShadow` selected by view-depth, `CSM` debug view visualizes cascade selection, and tests cover both
  split planning and per-cascade projection fit.
- Current session then added the first bounded CSM stabilization step: cascade projection centers are snapped to the
  shadow texel grid using the active shadow-map resolution, with a regression covering sub-texel camera movement.
- Current session then closed the first bounded CSM diagnostics/coverage step: CPU cascade fit now also publishes
  per-cascade view-depth ranges, ortho extents, and world-space texel size into runtime state, detailed HUD, screenshot
  sidecars, and tests, so scripted `FINAL/SHDW/CSM` captures can compare split coverage directly instead of only reading
  the final image.
- Current session then applied the first actual split-edge stability follow-up on top of that data: cascade `XY` fit now
  uses a rotation-stable sphere extent per frustum slice, and tests cover that camera yaw no longer changes cascade
  extent/texel density for the same split/FOV setup.
- Current session then closed the next split-edge visual follow-up on top of that stable fit: `voxel.frag` now blends
  between the current and next cascade over a runtime-visible split band instead of hard-switching at the exact split,
  detailed HUD now shows `BLD`, screenshot sidecars expose `shadow_cascade_blend`, and refreshed `MeshingStress`
  `FINAL/SHDW/CSM` captures live under `build/windows-clang-debug/lookdev-captures/20260424-csm-blend-v1/`.
- Current session then closed the next coverage follow-up on top of that split-blend baseline: each cascade now builds
  caster depth coverage from the receiver slice extruded upstream along the sun direction and intersected with active
  scene bounds, instead of always spanning the full active-scene bounds. Detailed HUD now shows per-cascade `CD` ranges,
  screenshot sidecars expose `shadow_cascade_caster_light_ranges`, and refreshed `MeshingStress` `FINAL/SHDW/CSM`
  captures live under `build/windows-clang-debug/lookdev-captures/20260424-csm-caster-coverage-v1/`.
- Current session then fixed the next live CSM mismatch the user found: cascade split planning now follows the same
  visible-scene receiver range as main-pass chunk visibility (`min(camera.farPlane, 64)`) instead of the raw camera far
  plane, so tower-top receivers no longer get demoted just because the unseen half of the frustum still existed on paper.
- Current session then moved the default CSM split distribution itself after the user confirmed the range fix was not
  enough: mainline `sunShadowCascadeSplitLambda` now defaults to `0.80` instead of `0.65`, and a fresh scripted capture
  (`build/windows-clang-debug/lookdev-captures/20260424-csm-lambda-v1/`) confirms the current `MeshingStress` split plan
  is `3.62 / 8.43 / 19.78 / 64.00` rather than `5.95 / 12.86 / 25.08 / 64.00`.
- Current session then fixed the next real cascade-specific clipping bug the user reported: caster coverage now expands
  not only light-depth but also per-cascade light-space `XY` extents, so a nearer cascade no longer loses a tall/upstream
  tower just because its receiver-fit footprint was smaller than the caster's projected silhouette. Fresh scripted CSM
  verification lives under `build/windows-clang-debug/lookdev-captures/20260424-csm-caster-xy-v1/`.
- Current session then fixed the next live CSM clipping bug on top of that `XY` coverage: expanded upstream caster
  ranges no longer sit behind the shadow camera near plane in mid/far cascades. The cascade light camera now moves
  upstream enough to keep the whole expanded caster range in front of the near plane, and refreshed scripted
  verification lives under `build/windows-clang-debug/lookdev-captures/20260424-csm-nearplane-v1/`.
- Current session then closed the first real post-fit CSM draw-submission step: shadow indirect commands are now built
  per cascade rather than shared blindly across all cascades, CPU chunk visibility rebuilds those commands against the
  current cascade clip volumes, and dirty-chunk meshing patches the same per-cascade commands on the GPU so the current
  frame does not fall back to stale all-opaque shadow draws. Refreshed scripted verification lives under
  `build/windows-clang-debug/lookdev-captures/20260424-csm-draw-culling-v1/`.
- Current session also added the bounded empty-cascade perf polish on top of that: if a frame has no dirty meshing work
  and CPU culling already knows a cascade has zero casters, the renderer skips the empty shadow indirect draw call for
  that cascade.
- Current session also resolved the transparent-shadow policy for the current mainline path: sun shadows render only
  non-glass casters and report `GLASS_IGNORED_FLUID_CASTS` in HUD/capture metadata. Glass does not cast shadows until a
  separate tinted/transmission or RT-oriented path is worth adding; `Fluid` casts through the current opaque shadow-map
  path. Refreshed `VoxelLab` `FINAL/SHDW` captures live under
  `build/windows-clang-debug/lookdev-captures/20260424-fluid-shadow-policy-v1/`.
- Current session also fixed the close-range shadow acne/stair-step complaint in the shader baseline: receiver projection
  now adds a small bias toward the sun, skips shadow sampling for nearly unlit/backfacing faces, and uses weighted `5x5`
  PCF instead of the old `3x3` box kernel. Close captures live under
  `build/windows-clang-debug/lookdev-captures/20260424-shadow-acne-close-v2/`.
- Current session then fixed the actual one-sided micro-triangle acne root cause at the caster side: the shadow graphics
  pipeline now enables static polygon depth bias for shadow-map writes. Verification capture lives under
  `build/windows-clang-debug/lookdev-captures/20260424-shadow-acne-caster-bias-v1/`.

## 2. Nearest Gap

- The old P0 process reminders are no longer left open in `TODO.md`: replay-first controller diagnosis, developer-only runtime smoke, and the current warning-cleanup closure are already treated as established baseline, not as unfinished work.
- Runtime smoke policy changed: `ProjectVRuntimeSmoke` is now a targeted lifecycle/Vulkan check, not a default DoD step
  for every lighting/material/doc change.
- If another warning-cleanup pass is needed, regenerate `Problems/` first instead of continuing from the current XML export.
- The next concrete mainline feature gap is no longer shadow plumbing, shadow tuning, direct-light BRDF, post-BRDF capture refresh, ambient/environment fill, fixed preset grading, first scene-key auto exposure, missing demo-scene opaque anchors, CSM split planning, the first CSM render hookup, basic CSM texel snapping, basic per-cascade coverage diagnostics, the first stable sphere-fit split-edge follow-up, the first shader-side split transition blend follow-up, the first cascade-specific caster coverage follow-up, or the visible-scene receiver-range fix that aligned cascades with current chunk visibility. The remaining `10.5.1` caveat is that exposure is not histogram/adaptive yet; adding that should wait for an HDR/luminance path rather than being faked in the shader.
- The current shadow limitation is now explicit and accepted: glass does not cast shadows in the mainline sun-shadow
  pass. `Fluid` casts as an opaque shadow-map caster; physical/tinted glass shadows remain future R&D.

## 3. Next Steps

1. Keep any further CSM work bounded to the next truly different step, not another chunk-level CPU-fit tweak:
   if shadows still need quality work after this, the next honest layer is filtering/quality policy or more cascades, not undoing per-cascade draw culling.
2. Keep using scripted look-dev captures for regressions: `PROJECTV_START_CAMERA_POSITION`, `PROJECTV_START_CAMERA_LOOK`, and `PROJECTV_LOOKDEV_CAPTURE_VIEWS=FINAL,SHDW,CSM` are now the reproducible path for these CSM steps.
3. Do not reintroduce fake glass shadow surrogates unless there is a real, separately scoped transparent-shadow path.

## 4. Risks

- Documentation will drift again if session history starts leaking back into `TODO.md` or `agent/`.
- Parallel `build/test/smoke` in the same build tree is still unsafe; smoke should be sequential and only when it is a
  relevant lifecycle/Vulkan check.
- `walk` tuning without replay/HUD/Tracy evidence will regress back toward synthetic-case patching.
