# Archived: agent/status.md §1 "Now" (pre-compaction snapshot)

**Archived on:** 2026-06-10
**Reason:** §1 "Now" had grown to ~178 lines of session-by-session shadow/lighting history
(most of which is duplicated in `agent/memory.md` §1, §10 and `agent/decisions.md` §15).
**Kept in active file** the last 2 sessions (P0.2 LINEAR fix re-apply and P0.3 per-corner AO design).
**Restored reference** if a historical audit needs the full chronological list.

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
- Current session then started `10.5.3` with a bounded contact-shadow baseline instead of another new pass:
  the main voxel shader now binds chunk descriptors plus the packed chunk voxel payload, traces a short voxel DDA ray
  toward the sun for local contact occlusion, and exposes that layer through explicit
  `sunContactShadowParams={strength,maxDistance}` in `VoxelSceneLighting`. `B` now includes `CTSH`, detailed HUD shows
  `CTSH STR/DST`, screenshot sidecars write `contact_shadow_strength` / `contact_shadow_distance`, and refreshed
  scripted verification captures live under `build/windows-clang-debug/lookdev-captures/20260424-contact-shadow-v1/`.
- Current session also closed the first post-landing contact-shadow regression: `voxel_shadow.vert` had not been kept in
  sync with the new `SceneLightingBuffer` layout after `sunContactShadowParams` was inserted, so the shadow pass was
  reading shifted cascade matrices and the visible sun shadows collapsed into view-dependent garbage. The shader layout
  is now synced again and a fresh scripted `VoxelLab` `FINAL/SHDW/CTSH` capture (`1777044425...`) is sane.
- Current session then revalidated that claim with the stricter real-capture rule and fixed the remaining default
  `VoxelLab` visibility gap: the initial `20260424-contact-shadow-v2` check was too washed out, so `VoxelLab` now uses
  stronger readable sun-shadow contrast (`shadow strength=0.88`, `environment=0.88`, `contact strength=0.50`).
  Inspected `FINAL` / `SHDW` / `CSM` / `CTSH` capture sets are green in both
  `build/windows-clang-debug/lookdev-captures/20260424-contact-shadow-v4/` and the user-facing
  `build/windows-clang-debug-tracy-profiler/lookdev-captures/20260424-contact-shadow-tracy-v2/`.
- Current session then continued `10.5.3` with the first ambient/contact-occlusion baseline in the same forward voxel
  path, not as a full `SSAO/GTAO` pass. `VoxelSceneLighting.ambientOcclusionParams` now carries
  `strength/radius/minVisibility`, `voxel.frag` traces a short hemisphere DDA with distance-squared falloff, `B` includes
  `AOCC`, detailed HUD and screenshot sidecars expose the values, and the tuned default keeps this layer local instead
  of letting big transparent-heavy scenes read as broad fake shadows. Inspected capture sets are green under
  `build/windows-clang-debug/lookdev-captures/20260424-aocc-baseline-v2/`,
  `build/windows-clang-debug/lookdev-captures/20260424-aocc-meshing-v1/`, and
  `build/windows-clang-debug-tracy-profiler/lookdev-captures/20260424-aocc-tracy-v1/`.
- Current session then added the first authored local point-light contract before local shadow maps/cubemaps and
  immediately followed it with the first bounded local-shadow baseline in the same forward voxel path:
  `VoxelSceneLighting` now appends local point-light position/radius, color/intensity, and
  `localPointLightParams={enabled,sourceRadius,shadowStrength,shadowBias}`; presets author one inverse-square point
  light; `voxel.frag` evaluates it through the same GGX direct-light helper and gates it with a short opaque-only voxel
  DDA visibility term; `LOCL` isolates the resulting contribution; and detailed HUD/capture metadata now expose
  `LOCL` / `LCLR` / `LSHD` plus `local_point_light_shadow_*`. `Glass` and `Fluid` are both ignored as local-light
  shadow occluders for now. Inspected debug-build captures are green under
  `build/windows-clang-debug/lookdev-captures/20260424-local-shadow-v1/` and
  `build/windows-clang-debug/lookdev-captures/20260424-local-shadow-meshing-v1/`.
- Current session then fixed the first live blocked-face defect in that local-light path: starting the local-shadow DDA
  from the interpolated fragment position produced visible fractal/moire patterns on some fully shadowed voxel faces.
  `voxel.frag` now anchors the local-light visibility ray to a stable point on the owning voxel face before applying
  bias, and the
  close-up `VoxelLab` `FINAL` / `LOCL` verification under
  `build/windows-clang-debug/lookdev-captures/20260424-local-shadow-fractal-fix-v1/` is visually sane again.
- Current session also rechecked that same local-light fix against the actual user repro instead of only the synthetic
  close-up: loading `C:\Users\Le1t\AppData\Local\Temp\ProjectV\InputReplay\latest.projectv.replay.snapshot.bin` through
  `PROJECTV_SNAPSHOT_PATH` plus the screenshot-sidecar camera (`cam -0.077 2.650 7.830`, `look 0.93 0.28 -0.22`)
  produces clean `FINAL` / `LOCL` captures under
  `build/windows-clang-debug/lookdev-captures/20260424-user-snapshot-camera-v1/`.
- Current session then fixed the next real local-light aliasing complaint the user surfaced through close-up screenshots
  and a saved `F6` world snapshot: the local visibility term no longer uses a single hard ray from each pixel to the
  emitter center. `voxel.frag` now traces from a stable point on the owning voxel face and averages a small emitter disk built from
  the authored `sourceRadius`, which keeps partially occluded voxel faces from exploding into binary white/black speckle
  at close range. Refreshed saved-snapshot verification lives under
  `build/windows-clang-debug/lookdev-captures/20260424-user-f6-close-angle1-v2/` and
  `build/windows-clang-debug/lookdev-captures/20260424-user-f6-close-angle2-v2/`.
- Current session then fixed the next regression that follow-up introduced: using a full face-center sample made local
  visibility constant per voxel face, so close ground shots looked as if each floor voxel had its own independent shadow
  bucket. `voxel.frag` now preserves the fragment's in-face position while clamping it to the owning voxel face plane,
  and the saved-snapshot verification at `cam 2.917 2.650 6.217`, `look 0.08 -0.45 -0.89` lives under
  `build/windows-clang-debug/lookdev-captures/20260424-user-floor-voxel-shadow-v2/`.
- **Multiplatform dev baseline opened (`2026-06-09`).** `ProjectV` is now expected to build and run on both
  `windows-clang-debug` (existing) and `linux-clang-debug` (new). Arch Linux is the active Linux dev host. The current
  Linux baseline is **clang 22.1.6 (native clang, not clang-cl) + lld 22.1.6 + libstdc++ 16.1.1 + SDL3 3.4.10 + Vulkan
  1.4.350**. Configure / build / ctest on `linux-clang-debug` are green: `ProjectV` (50.5 MB ELF) and `ProjectVTests`
  (47.8 MB ELF) link, `ctest` passes 1/1 in 1.44s, and `ProjectV` correctly refuses to init because
  `VK_LAYER_KHRONOS_validation` is not installed (validation is `ON` in the preset; layers package is a follow-up). The
  source-side fixes that landed as part of that baseline: `src/CMakeLists.txt` (`GPUOpen::VulkanMemoryAllocator`
  uncommented on the `ProjectV` link block), `src/core/Types.hpp` (`vma/vk_mem_alloc.h` → `vk_mem_alloc.h`), and
  `src/ecs/EcsWorld.hpp` (`#include <cstddef>` before `<cstdint>`). Root `CMakeLists.txt` got
  `VOLK_STATIC_DEFINES`/`NOMINMAX`/`/W4 /WX /permissive- /utf-8` properly platform-gated, plus a non-MSVC
  `else()` branch that adds `-Wno-deprecated-declarations` (libstdc++ 16 + flecs 2.2.0 lag) and `-include cstring`
  (legacy `std::mem*`/`std::str*` calls without explicit includes). `CMakePresets.json` gained
  `linux-clang-debug-base` / `linux-clang-debug` / `linux-clang-debug-build` / `linux-clang-debug-tests`. Windows
  presets are untouched. The current dirty tree also still contains the user's Windows-side pending edits
  (`AGENTS.md`, `TODO.md`, `agent/*`, `src/*`, `tests/VoxelWorldTests.cpp`, `docs/*`, `legacy/docs/*`,
  `.editorconfig`, `.gitignore`, `.gitmodules`); the agent intentionally did not commit on the user's behalf.
- **Dev-tool gaps closed (`2026-06-09`).** 14 packages installed via `agent/_linux_packages_install.sh` (operator ran
  it manually — sudo requires a real TTY, agent cannot pipe passwords). All 14 binaries smoke-tested live in the
  same session. Native repo: `gh 2.93.0` (authenticated as **Leeleit** via SSH, awaiting `GH_TOKEN` for REST API),
  `jq 1.8.1`, `tree 2.3.2`, `bloaty 1.1`, `valgrind 3.25.1`, `hyperfine 1.20.0`, `lldb 22.1.6` (matches clang major),
  `delta 0.19.2`, `lazygit 0.62.2`, `perf 7.0.10-1`. AUR: `gitleaks`, `trufflehog`, `tldr 3.4.4`, `sccache 0.15.0`. Notable
  diagnostic outputs captured during smoke: `bloaty` reports ProjectV ELF as 31% `.debug_info` (14.9 MiB) + 25.9%
  `.text` (12.5 MiB), `gitleaks detect --no-git` on `src/` reports 0 leaks, `hyperfine` baseline for `/usr/bin/true`
  is 470.5 µs ± 188.1 µs, `valgrind --quiet /usr/bin/true` is clean. The full self-audit + verification scripts are
  preserved in `agent/_linux_post_install_verify.sh` for next-session re-validation.
- **Git/editor baseline closed (`2026-06-09`).** `~/.gitconfig` now uses `core.pager = delta` (side-by-side with line numbers,
  hyperlinks, navigate), `core.editor = /home/le1t/.emacs.d/bin/doom emacs -nw` (Doom Emacs wrapper at explicit path;
  `core.editor = codium -w` was the previous value but codium/vscodium is not used as the IDE, so it was replaced
  with the Doom wrapper that loads the operator's `~/.doom.d/{config.el,init.el}`). User identity preserved
  (`Leeleit` / `le1t@list.ru`). `gh` is configured for SSH (`gh.protocol = ssh`). `gh.api_user` requires `GH_TOKEN` env
  var for REST calls (SSH-based auth does not expose an API token).
- **Four-build Linux tree zoo validated (`2026-06-09`).** `linux-clang-debug` (everyday, 39.8 s cold, ctest 1.44 s),
  `linux-clang-debug-sccache` (sccache 665/665 compile requests validated end-to-end, 0.049 s no-op rebuild), `linux-clang-debug-ci`
  (27.4 s cold, ctest 1.37 s, quieter log level for CI), and `linux-clang-debug-tracy-profiler` (26.6 s cold, `ProjectV` itself
  with Tracy instrumentation enabled — Tracy UI binary itself currently fails on Linux/glibc due to upstream
  `wolfpld/tracy` `tidy-html5` binding to obsolete `uint` / `ulong`; decision deferred to operator, see
  `agent/memory.md` § "Linux build tree zoo" for workarounds).

