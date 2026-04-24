# Memory

Долговечный delta-контекст поверх `TODO.md` и `AGENTS.md`.

Дата обновления: `2026-04-24`

---

## 1. Runtime facts
Shadow-path update `2026-04-22`:

- The earlier "dense opaque face prefix" assumption is obsolete. Packed voxel faces live in sparse per-chunk ranges, so any consumer of packed faces must follow `firstInstance`/indirect addressing instead of naive `0..faceCount`.
- The shadow pass now uses a dedicated shadow descriptor/pipeline layout instead of reusing the main graphics descriptor set while the shadow image is simultaneously written as depth.
- The current stable baseline uses a dedicated all-occluder `shadowIndirectBuffer` for opaque shadow rendering. Off-frustum opaque casters are represented again; `Glass` is intentionally skipped by policy, while `Fluid` casts through the current opaque shadow-map path.


- `creative` — physics-backed flight/edit mode на том же `CharacterVirtual`, что и `walk`, но без гравитации; подчиняется `pause`.
- Boosted `creative` collision path now substeps long `CharacterVirtual::ExtendedUpdate` travel much more finely (`~0.05 m` cap, max `32` substeps); normal speed already slid correctly, but high-speed coarse steps could wedge both against dense voxel columns and on exact glass-corner hits.
- `spectator` — observe-only noclip mode: не даёт world edits, но оставляет movement/look даже при `pause`.
- Возврат в `walk` сначала сохраняет текущую позицию камеры; ground recovery — только fallback.
- В flying modes `WASD` двигают только по `XZ`; `Space/Shift` отвечают за высоту. В `walk` `Shift` — это sneak/crouch, а не descend.
- Double-tap `Space` переключает только `creative <-> walk`.
- Block interaction остаётся на CPU `VoxelRaycast` + `VoxelWorld::SetVoxelMaterial`; physics raycast не является источником истины для world edit.
- Lightweight debug editing now includes read-only inspect telemetry plus two mutation helpers: `X` toggles a box anchor for paint/erase tools, and `M` copies the currently hit voxel material into the placement material.
- После successful `SyncPhysicsWorld` на world edit walk-контроллер обязан сбрасывать cached support ownership (`edge/takeoff/sneak/anchors`), иначе удалённая геометрия может ещё тик-два жить как fake grounded support.
- Один voxel edit помечает dirty только для своего chunk и реально затронутых boundary-neighbors.
- Chunk visibility обновляется каждый кадр через frustum/distance culling; dirty chunks всё равно домешиваются даже вне кадра.
- `VoxelScenePreset` теперь задаёт и builtin geometry, и lighting look.
- `VoxelSceneLighting` теперь несёт не только sky/horizon/ground/sun/fog, но и baseline exposure/environment-fill/tone-map/debug-view post-process contract; `postProcess.y` is the per-preset environment diffuse intensity, `colorGrading` is white point / contrast / saturation / lift, `exposureControl` is metering mode / target key / min exposure / max exposure, `UpdateSceneResources` освежает его каждый кадр из preset + runtime look-dev controls, а renderer clear color берёт тот же contract, а не отдельную hardcoded константу.
- First mainline CSM path is live: sun shadows render into a 4-layer depth image array, `VoxelSceneLighting` carries
  four `sunShadowViewProjections` plus view-depth split values, the shadow pass renders each cascade with a
  `ShadowPushConstants::cascadeIndex`, and the final shader samples `sampler2DArrayShadow` selected from camera
  view-depth. The first stabilization step is also live: cascade projection centers snap to the shadow texel grid using
  the active shadow-map resolution. The first coverage diagnostics step is also live: runtime state, detailed HUD, and
  screenshot sidecars now expose per-cascade view-depth ranges, ortho extents, and effective world-space texel size.
  The next stability step is also live: cascade `XY` fit now uses a rotation-stable sphere extent per view slice instead
  of a tight light-space AABB, so camera yaw does not churn cascade width/height and texel density for the same split.
  The first shader-side split follow-up is now live too: the final shader blends current/next cascades over a
  runtime-visible split band (`shadowCascadeBlendParams.x`) instead of hard-switching at the split edge. Cascade-specific
  caster coverage tuning is also live now: per-cascade depth fit no longer spans full active-scene bounds blindly, and
  instead uses the receiver slice extruded upstream along the sun direction before intersecting with active scene bounds.
  That coverage now affects projected `XY` extents too, not just light-depth, so nearer cascades do not clip tall or
  upstream casters that still shadow the current receiver slice.
  True per-cascade caster draw culling and deeper split-edge tuning remain future work.
- `sunDirectionAndWrap.xyz` is authored as the shading-side vector toward the sun. `BuildSunShadowProjection` must flip that vector to the actual light-travel direction before building the shadow camera; otherwise direct light and shadow placement drift apart and the scene can read as if the light source disappeared. This sign contract is now covered by a regression test.
- Current shadow quality baseline поверх этого path: `2048x2048` shadow map, weighted shader-side `5x5` PCF instead of
  single compare / old `3x3` box sampling, and angle-aware receiver biasing that scales the authored depth/normal bias
  by `N.L`. Receiver projection also adds a small world-space offset toward the sun and skips shadow sampling for nearly
  unlit/backfacing faces. One-sided micro-triangle acne on lit voxel faces must be treated as caster-side self-shadowing:
  the shadow pipeline now enables static Vulkan polygon depth bias for shadow-map writes.
- CSM planning remains explicit runtime state: `BuildSunShadowCascadeSplits` produces the deterministic 4-cascade
  practical split scheme from camera near plus the current visible-scene receiver range (`min(camera.farPlane, 64)` in
  mainline) and default lambda `0.80`, HUD/sidecars expose `shadow_cascade_*`, and `CSM`
  debug view visualizes cascade selection. Coverage diagnostics now also expose `shadow_cascade_view_ranges`,
  `shadow_cascade_ortho_extents`, and `shadow_cascade_texel_world`. Split blending is also runtime-visible now:
  HUD shows `BLD`, sidecars include `shadow_cascade_blend` and `shadow_cascade_blend_offset`, and the shader uses
  `shadowCascadeBlendParams.y` as the first cascade near plane when building the blend band. Caster-depth coverage is
  runtime-visible too now: HUD per-cascade lines include `CD`, and sidecars include `shadow_cascade_caster_light_ranges`.
  If those per-cascade caster near depths go negative, the cascade light camera is too close to the receiver slice and
  expanded upstream casters are being clipped by the shadow near plane before sampling; the current baseline avoids that
  by moving the cascade light camera upstream enough to keep expanded caster coverage positive.
- The shadow draw path is no longer one shared all-opaque indirect list for every cascade. `shadowIndirectBuffer` now
  stores `kSunShadowCascadeCount * chunkCount` draw commands, CPU chunk visibility rebuilds them against each cascade's
  clip volume, and dirty-chunk meshing patches the same per-cascade commands on the GPU so the current frame keeps
  correct shadow counts after meshing.
- Empty-cascade draw skipping is intentionally conservative: the renderer only skips a cascade's `vkCmdDrawIndirect`
  call when CPU culling says the cascade is empty and the frame has no dirty meshing work. Otherwise the draw call stays,
  because dirty chunks can still patch per-cascade shadow commands later in the same frame.
- `VoxelMaterialVisual` no longer encodes the old ambient/diffuse/spec/shininess knobs directly. The stable packing is now `baseColor`, `surface={AO, roughness, metallic, reflectance}`, `medium={tint.rgb, transmission}`, `shading={fogFactor, emissiveStrength, ambientResponse, directDiffuseResponse}`; the current direct-sun baseline shades that contract with `GGX + Fresnel-Schlick + Smith`, but it still preserves authored ambient/diffuse response weights so the new BRDF does not wash shadow contrast out of the scene.
- Ambient/environment fill is no longer purely normal-based. Compute meshing now writes a cheap per-face local
  ambient-visibility byte into `PackedSceneVoxelFace::lightingData`, `voxel.vert` forwards it flat, and `voxel.frag`
  multiplies sky/horizon/ground fill by it so enclosed voxel cavities stop reading as if they still saw full sky.
  Current blocker policy for that term is `Air/Open`, `Glass/Open`, `Fluid/Occluder`, `Opaque/Occluder`; this is a
  bounded voxel-neighborhood visibility term, not screen-space AO/GTAO.
- Current sun-shadow baseline policy is `TransparentShadowPolicy::GlassIgnoredFluidCasts`, reported as
  `GLASS_IGNORED_FLUID_CASTS`. `Glass` does not cast shadows; `Fluid` casts through the current opaque shadow-map path.
  Do not add fake frame-only glass shadows back into mainline; real tinted/transmission glass shadows need a separate
  future path.
- `VoxelLab` now also includes a small right-side opaque stepped anchor made from `FloorGray` / `FloorWhite` outside the
  glass/fluid sphere. It is intentionally there as a stable opaque sun-shadow caster/receiver for look-dev; it is not a
  transparent-shadow policy.
- The default `MeshingStress` camera is a weak discriminating case for bias tuning. Use the reference shot `cam -25 19 25`, `look 0.62 -0.48 -0.62` instead: it produces meaningful `SHDW` / `FINAL` diffs for moderate bias candidates, but the tested variants around the current code baseline still did not beat `{0.80f, 0.0010f, 0.0070f, 1.50f}` clearly enough to justify a preset change yet.
- HUD остаётся лёгким CPU-built overlay path без `imgui`; `G` now switches between a normal HUD and a detailed HUD, and the noisy selection/mutation/replay counters plus the green placement preview stay detailed-only.
- Current lighting look-dev ladder is keyboard-driven inside the live sandbox: `B` cycles lighting debug views including dedicated `Shadow`, `N` cycles tone-map, `H/K` adjust exposure, and `V` resets lighting tuning to the preset baseline.
- Detailed HUD now also exposes current shadow resolution/strength/filter/bias, so sun-shadow tuning is reproducible without a separate editor path.
- Live look-dev capture now also exists inside the same runtime loop: `C` saves the current frame as a `.bmp` plus a `.txt` sidecar with preset/exposure/shadow tuning, and `PROJECTV_SCREENSHOT_DIR` can override the output directory.
- Scripted look-dev capture now exists for reproducible baseline refreshes without manual camera/input timing:
  `PROJECTV_START_CAMERA_POSITION`, `PROJECTV_START_CAMERA_LOOK`, `PROJECTV_LOOKDEV_CAPTURE_VIEWS`,
  `PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES`, `PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES`, and
  `PROJECTV_LOOKDEV_CAPTURE_QUIT`. The first refreshed post-BRDF set lives locally under
  `build/windows-clang-debug/lookdev-captures/20260424-brdf-baseline-v2/` with paired `FINAL` / `SHDW` captures for
  `ChunkGrid` and the `MeshingStress` reference shot.
- The current ambient/environment fill capture set lives under
  `build/windows-clang-debug/lookdev-captures/20260424-env-fill-v1/` and uses `FINAL` / `AMB` / `SHDW` for `ChunkGrid`
  plus the fixed `MeshingStress` reference shot. Capture metadata now includes `environment_intensity`.
- The current minimal grading capture set lives under
  `build/windows-clang-debug/lookdev-captures/20260424-grading-v1/` and uses `FINAL` / `AMB` / `SHDW` for `ChunkGrid`
  plus the fixed `MeshingStress` reference shot. Capture metadata now includes `grading_white_point`,
  `grading_contrast`, `grading_saturation`, and `grading_lift`.
- The current first auto-exposure capture set lives under
  `build/windows-clang-debug/lookdev-captures/20260424-auto-exposure-v1/`. It uses CPU-side `SceneKey` metering instead
  of a GPU histogram/adaptive exposure pass; capture metadata now includes `exposure_metering`, `exposure_key`,
  `exposure_target_key`, `exposure_min`, and `exposure_max`.
- The current `VoxelLab` opaque-anchor capture set lives under
  `build/windows-clang-debug/lookdev-captures/20260424-voxel-lab-anchor-v1/` with `FINAL` / `AMB` / `SHDW` captures.
- The current transparent-shadow policy capture set lives under
  `build/windows-clang-debug/lookdev-captures/20260424-fluid-shadow-policy-v1/`; it confirms VoxelLab `Fluid` casts a
  sun shadow while `Glass` no longer draws the rejected frame-only shadow surrogate.
- The current close-range shadow-acne capture set lives under
  `build/windows-clang-debug/lookdev-captures/20260424-shadow-acne-close-v2/` and uses the VoxelLab camera from the
  user-reported HUD (`cam -5.724 2.650 -5.554`, `look 0.83 -0.12 0.61`).
- The current caster-side shadow-acne verification capture lives under
  `build/windows-clang-debug/lookdev-captures/20260424-shadow-acne-caster-bias-v1/`.
- Screenshot capture must signal present only after the post-render transfer copy completes. The current renderer uses
  `VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT` for the render-finished signal because `COLOR_ATTACHMENT_OUTPUT` was too early
  once screenshot copy commands were appended after color rendering.
- Near-term user intent is demo-scene/look-dev oriented rather than gameplay-loop oriented: gameplay-facing sandbox expansion is not the current mainline target, while lighting/scene-look foundation is.

## 2. Walk / traversal facts

- Meshing transparency contract: opaque voxels still emit faces against `Glass`, while `Glass` keeps the internal shared face culled; otherwise covered blocks lose their visible top face.

- Static-world `walk` в этом репо voxel-authoritative и живёт в `src/physics/PhysicsWorld.cpp`; `CharacterVirtual` остаётся proxy/stance carrier, а не главным автором grounded motion.
- `UpdateApp` гонит `walk` через fixed-step accumulator (`1/60`), даже если render FPS значительно выше.
- `walk` использует continuous foot-support sampling и separate `Shift` safe-walk path.
- `Shift` safe-walk grounded-only: если crouch jump реально уходит с края с movement input, airborne path не должен превращаться в generic edge cling.
- Sneak-support faces должны подтверждать реальный overlap capsule footprint с top-face; одного расширенного `XZ`-region недостаточно, иначе боковой wall voxel может ложно стать grounded-support при crouch-jump рядом со стеной.
- Sneak-support region `referenceFeetPosition[1]` должен означать реальную sampled top-plane (`voxelY + 1 + clearance`), а не текущий `feetY` вызывающего кода; иначе midair crouch у stacked wall может ложно стать grounded на произвольной высоте.
- Sneak-support region membership требует не только `XZ` overlap, но и разумную близость стоп к `referenceFeetPosition[1]`; если стопы заметно ниже sampled support plane, midair crouch не должен активировать grounded support на более высокой top-plane.
- Ordinary `walk` horizontal motion здесь не авторится через `velocity.xz`; `X/Z` двигаются вручную через feet-position deltas.
- Moving partial edge support тоже может быть валидным grounded-like состоянием: при стабильном `feetY`, невосходящем `velY` и `footSupportScore≈0.5` контроллер должен держать `EdgeGrace`, а не падать в synthetic `Air`.
- Самый узкий edge-jump case не должен требовать, чтобы `supportState` уже был grounded-like до применения текущего `Space`: если под стопой ещё есть реальные support samples на takeoff-plane, jump может переавторизоваться в этом же тике, но этот fallback нельзя оставлять включённым для обычного walk-off без jump request.
- После ballistic jump возврат на recent ground-takeoff plane тоже должен уметь reacquire `EdgeGrace`, даже если обычный footprint score на самой кромке уже низкий; иначе возможен late drop при `feetY` уже на support plane.
- Cached ground-takeoff grace — это pre-jump/coyote helper, а не airborne retry authority: когда ballistic jump уже active, этот cache не должен давать second jump commit в воздухе.
- Cached ground-takeoff support должен оставаться привязанным к recent takeoff plane: во время active ballistic jump его нельзя переобновлять на чужую top-plane, а `landedBackOnGroundTakeoffSupport` обязан совпадать с cached plane и drift, а не с любым широким support под стопами.
- Rising jump motion не должен выполнять voxel top-promotion.
- Jump-on-block late rise сейчас трактуется как camera-side smoothing issue; broad airborne `step-up` path остаётся активным, потому что его заужение уже ломало established regressions.
- `WalkAirControlMode::MinecraftLike` — default; `WalkAirControlMode::Realistic` оставляет direction-lock + scalar brake.
- `walk` jump input больше не `pressed`-only: held `Space` снова должен давать повторный jump request после возвращения в grounded-like state.
- One-block auto-jump is now default-off and runtime-toggleable via `J`; if enabled, `F12` still toggles only `delay on/off`, and the delay countdown starts only once the immediate one-block rise is actually reachable.

## 3. Runtime debug / repro facts

- Runtime input replay is now first-class: `R` records the current sandbox into a snapshot plus per-frame input file, `Y` replays the latest capture, and the same replay file can be loaded by tests.
- The high-speed creative-flight wedge regressions are pinned by repo fixtures `tests/fixtures/creative_transparency_boost_stuck.*` and `creative_transparency_boost_corner_stuck.*`; prefer those exact captures over another synthetic approximation.

- Live walk diagnosis нужно делать по `PhysicsWalkDebugInfo`, HUD (`CAM/FEET/support/grace`) и Tracy, а не по округлённой камере.
- Perf/repro scenes задаются через `PROJECTV_SCENE_PRESET`: `VoxelLab`, `FlatBenchmark`, `TransparencyStress`, `ChunkGrid`, `MeshingStress`.
- Tracy UI в этом репо — отдельный build target: `tracy-profiler.exe` не появляется от `--target ProjectV`.

## 4. Build / repo constraints

- `Problems/*.xml` from JetBrains inspections are point-in-time snapshots; during warning cleanup they must be validated against the current source or local `clang-tidy` before applying edits, because line-based entries go stale quickly during the same refactor pass.
- The latest current-source cleanup already removed the visible warning targets in `VoxelMaterials.cpp`, `Renderer.cpp`, `AppUpdate.cpp`, `SceneResources.cpp`, `VulkanGraphicsPipeline.cpp`, and `VoxelWorldTests.cpp`; if the checked-in `problems/tests/*.xml` still reports `CppDFAUnreachableFunctionCall` / `CppDFAConstantParameter` rows there, treat them as stale export artifacts until a fresh JetBrains inspection is generated.
- Even with a fresh export, JetBrains DFA does not reliably model the bespoke single-TU runner in `tests/VoxelWorldTests.cpp`; the file now carries a deliberate `// ReSharper disable CppDFAUnreachableFunctionCall` suppression there, while real helper/dataflow issues in the same file should still be fixed normally.
- Mainline repeatable path идёт через `windows-clang-debug` и `windows-clang-debug-ci`.
- В одном build tree нельзя запускать несколько независимых `cmake --build` / `ctest` / smoke одновременно.
- Для `.cpp`, которые тянут Jolt internals, `<Jolt/Jolt.h>` должен идти раньше остальных Jolt headers; иначе рушатся `JPH_*` macros/typedefs и `PhysicsWorld.cpp` перестаёт собираться.
- `ProjectVRuntimeSmoke` — официальный target поверх `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1`.
- `ProjectVRuntimeSmoke` remains a developer-only GUI smoke check for now, not the current CI contract, and it is targeted
  to Vulkan/bootstrap/swapchain/window lifecycle/present/screenshot sync or device-lost/hang risk. Do not treat it as
  mandatory after ordinary shader/material/lighting tuning, docs, or unit-testable logic.
- Shader compile path принимает либо `glslc`, либо `glslangValidator`.
- `README_NEW.md` — текущий root-facing overview; `README.md` не трогать без явного запроса пользователя.
- `legacy/docs` is now the only supported legacy-doc root: engineering principles live under `legacy/docs/philosophy`, unified reference material lives under `legacy/docs/{standards,libraries,architecture}`, restored learning/support material lives under `legacy/docs/{guides,tutorials,examples}`, and historical planning stays under `legacy/docs/archive/roadmaps`. In `legacy/docs/libraries`, keep the canonical `01_reference.md` / `02_integration.md` entry docs plus the deeper per-library corpus when it still carries useful material. Do not recreate parallel `latest` / `old` trees.
