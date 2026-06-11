# TODO.md

Актуальная дорожная карта `ProjectV`.

Дата обновления: `2026-04-24`
Статус документа: `живой roadmap`

---

## 1. Mainline

Mainline `ProjectV` сейчас — это reproducible interactive voxel MVP.

Что уже есть в коде:

- runnable voxel slice на `Vulkan + SDL + Jolt`;
- `creative` / `spectator` / `walk`;
- voxel world, dirty chunks, meshing, frustum/distance culling;
- block interaction, snapshots, lightweight debug editor;
- HUD, Tracy, runtime smoke и failure probes;
- рабочий, но ещё тюнингуемый `walk`-контроллер.

Что не должно становиться блокером mainline:

- `SVO`;
- mesh shaders;
- bindless-everything;
- тяжёлая simulation R&D;
- большой editor;
- multiplayer;
- plugin/modding stack.

---

## 2. Текущий Milestone

Ближайший честный milestone:

- стабильный интерактивный voxel sandbox slice;
- repeatable `configure/build/test` loop plus targeted runtime smoke when lifecycle/Vulkan coverage is relevant;
- world edit + snapshots + lightweight debug tools;
- walk/controller feel без грубых runtime regressions;
- документация синхронизирована с кодом и `agent/`.

Критерий готовности milestone:

- проект легко запускается и проверяется;
- текущие control/debug loops не выглядят хрупкими;
- следующий gameplay/debug слой можно добавлять без новой документной или архитектурной зачистки.

---

## 3. Активные Приоритеты

### P0

- [x] Replay-capture-driven walk regression workflow is now the default mainline path: runtime input replay exists,
  replay fixtures cover live controller bugs, and future live repros should start from capture rather than handwritten
  key scripts.
- [x] Current refactor/lint/static-analysis sweep is closed enough that the next gameplay/debug slice is no longer
  blocked by warning cleanup; the latest DFA/tidy follow-ups kept `build -> tests -> smoke` green.
- [x] GUI runtime smoke stays developer-only and targeted: use it for Vulkan/bootstrap/swapchain/window lifecycle,
  present/screenshot sync, device-lost/hang risk, not as a mandatory ritual after every lighting/material/doc change.
- [x] `walk` / `creative` controller work is now explicitly bounded by replay/HUD/Tracy evidence instead of broad
  heuristics; that guardrail lives in the project contracts now, not as an open TODO item.

### P1

- [x] Первый post-refactor gameplay/debug tooling slice закрыт: runtime inspect telemetry, chunk-oriented overlays, material pick, и anchored world-mutation helpers уже живут в sandbox.
- [ ] Продолжать lighting/look-dev foundation-first slice для текущей demo-scene цели:
  - [x] первый lighting contract для сцены теперь явный в runtime: `VoxelSceneLighting` держит
    sky/horizon/ground/sun/fog + exposure/tone-mapping baseline для каждого `VoxelScenePreset`;
  - [x] минимальная debug ladder для итерации освещения уже есть в sandbox: `B` cycles lighting view, `N` tone-map,
    `H/K` exposure, `V` reset, а detailed HUD показывает текущее lighting state;
  - [x] довести первый direct-light baseline до стабильного live look-dev состояния до local lights / advanced GI;
  - [x] первый shadow path для текущего voxel renderer уже выбран и прототипирован: scene-wide orthographic sun shadow
    map рендерится отдельным depth pass и семплируется в main voxel pass только для direct sun;
  - [x] дотюнить первый sun-shadow baseline в live look-dev:
    - [x] current baseline уже не single-sample prototype: shadow map теперь `2048x2048`, а main voxel shader использует
      weighted `5x5` PCF вместо одного compare sample;
    - [x] lighting debug ladder теперь включает dedicated `Shadow` view, а detailed HUD показывает current shadow
      strength / filter radius / bias;
    - [x] live look-dev capture больше не зависит от внешних тулов: `C` сохраняет текущий кадр в `.bmp` вместе с sidecar
      metadata-файлом для preset/exposure/shadow tuning;
    - [x] shadow projection больше не тратит большую часть карты на пустой padded world volume: CPU fit теперь сначала
      использует bounds активных chunk-ов и только потом fallback'ается на полные world bounds для пустой сцены;
    - [x] current direct-light baseline больше не держится на ad-hoc `spec power + shininess`: material visuals теперь
      упаковывают `base color`, `AO`, `roughness`, `metallic`, `reflectance`, transmission tint и emissive/fog hooks, а
      `voxel.frag` использует `GGX + Fresnel-Schlick + Smith` для direct sun без ломки текущего ambient/fog/shadow loop;
    - [x] после этого BRDF/material contract shift обновить opaque-heavy capture baselines (`ChunkGrid` /
      `MeshingStress`), чтобы дальнейший shadow tuning сравнивал уже новый lighting baseline, а не старые `FINAL` кадры
      до смены direct-light response;
      - current refreshed capture set generated through the runtime capture path under
        `build/windows-clang-debug/lookdev-captures/20260424-brdf-baseline-v2/`: `ChunkGrid` default camera and
        `MeshingStress` reference shot (`cam -25 19 25`, `look 0.62 -0.48 -0.62`) each have paired `FINAL` / `SHDW`
        `.bmp` + `.txt` sidecars.
      - startup camera/capture automation is now env-driven for look-dev repros:
        `PROJECTV_START_CAMERA_POSITION`, `PROJECTV_START_CAMERA_LOOK`, `PROJECTV_LOOKDEV_CAPTURE_VIEWS`,
        `PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES`, `PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES`,
        `PROJECTV_LOOKDEV_CAPTURE_QUIT`.
      - screenshot capture sync was fixed after the first scripted capture exposed stale swapchain/readback content:
        the submit now signals present after all recorded commands, including the post-render transfer copy.
    - [x] ambient/environment fill теперь явный, а не спрятанный shader-only gradient: `postProcess.y` хранит
      per-preset environment diffuse intensity, `voxel.frag` считает sky/horizon/ground fill отдельным слоем,
      detailed HUD и screenshot sidecar показывают `ENV` / `environment_intensity`, а scripted captures обновлены под
      `build/windows-clang-debug/lookdev-captures/20260424-env-fill-v1/` (`FINAL` / `AMB` / `SHDW` для `ChunkGrid` и
      `MeshingStress` reference shot).
    - [x] local cavity ambient follow-up landed on top of that fill contract: compute meshing now bakes a cheap
      per-face ambient-visibility term into `PackedSceneVoxelFace`, `voxel.frag` multiplies environment fill by it, and
      sealed voxel cavities no longer read as if they still see the full sky gradient. This is intentionally a bounded
      voxel-neighborhood visibility term, not `SSAO/GTAO`.
    - [x] minimal exposure/grading contract now exists before auto exposure: `VoxelSceneLighting.colorGrading` carries
      white point / contrast / saturation / lift, the shader applies it after tone mapping, clear color follows the same
      grading path, detailed HUD and screenshot sidecars expose the values, and scripted captures were refreshed under
      `build/windows-clang-debug/lookdev-captures/20260424-grading-v1/`.
    - [x] first auto-exposure policy is now explicit and intentionally CPU-side: per-preset `SceneKey` metering
      estimates
      exposure from authored sky/horizon/ground/sun brightness and clamps it through `exposureControl`, while `H/K`
      remains manual stop bias on top. This is not histogram/adaptive exposure yet, but it removes hidden fixed exposure
      as the only path and keeps the current forward renderer simple.
      - refreshed scripted captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-auto-exposure-v1/`; sidecars now include
        `exposure_metering`, `exposure_key`, `exposure_target_key`, `exposure_min`, and `exposure_max`.
    - [x] продолжить bias/normal-bias/coverage/strength tuning по живым capture/screenshot, пока не уйдут заметные
      aliasing / acne / peter-panning артефакты.
    - [x] keep current shadow tuning focused on opaque-heavy presets (`ChunkGrid` / `MeshingStress`) until the
      demo-scene either gains opaque anchor geometry or an explicit transparent-shadow policy.
      - current baseline in `src/voxel/VoxelMaterials.cpp`:
        - `ChunkGrid`: live `C` capture pass moved it to {0.76f, 0.0010f, 0.0040f, 1.30f}; the default-view shadow
          capture kept full coverage and reduced bright-surface speckle versus the old {0.76f, 0.0009f, 0.0065f, 1.40f}.
        - `MeshingStress`: the startup camera turned out to be a weak tuning case, but the user-provided reference
          shot (`cam -25 19 25`, `look 0.62 -0.48 -0.62`) is now the reproducible capture case instead. Tested moderate
          candidates around the current baseline (`{0.80f, 0.0012f, 0.0045f, 1.40f}`,
          `{0.80f, 0.0011f, 0.0045f, 1.40f}`, `{0.80f, 0.0011f, 0.0050f, 1.40f}`) did move the shadow image there, but
          none beat the baseline `{0.80f, 0.0010f, 0.0070f, 1.50f}` clearly enough to justify a preset change yet.
    - [x] `VoxelLab` now has explicit opaque anchor geometry: a small right-side solid stepped marker built from
      `FloorGray` / `FloorWhite` outside the glass/fluid sphere, so the demo scene has a stable opaque caster/receiver
      for the current opaque-only sun-shadow path.
      - refreshed captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-voxel-lab-anchor-v1/`.
    - [x] First real CSM render path is now wired: the sun shadow image is a 4-layer depth array, the shadow pass
      renders
      each cascade with its own uploaded light matrix, the final shader samples `sampler2DArrayShadow` by camera
      view-depth, and `CSM` debug view visualizes cascade selection.
    - [x] First CSM stabilization step landed: cascade projection centers snap to the shadow texel grid using the active
      shadow-map resolution, and tests cover sub-texel camera nudges so small movement does not continuously slide the
      first cascade projection.
    - [x] First bounded CSM diagnostics/coverage step landed: detailed HUD and screenshot sidecars now expose
      per-cascade
      view-depth ranges, ortho extents, and effective world-space texel size, so split coverage can be compared from
      scripted captures instead of only eyeballing the final frame. Cascade-specific caster culling and deeper temporal
      edge-case tuning remain follow-up work.
    - [x] First split-edge stability follow-up landed on top of those diagnostics: cascade `XY` fit now uses a
      rotation-stable sphere extent per view slice instead of a tight light-space AABB, so per-cascade extents/texel
      density stay predictable under camera yaw changes. The next quality gap is now shader-side split transition/caster
      coverage tuning, not another hidden CPU fit heuristic.
    - [x] First shader-side split transition follow-up landed on top of that stable fit: `voxel.frag` now blends the
      current and next cascade across a runtime-visible split band instead of hard-switching at the split edge, while
      detailed HUD and screenshot sidecars expose the active `BLND` value for tuning.
      - refreshed `MeshingStress` scripted captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-csm-blend-v1/` with `FINAL` / `SHDW` / `CSM`.
    - [x] First cascade-specific caster coverage follow-up landed on top of the split-blend baseline: per-cascade shadow
      depth fit no longer uses full active-scene bounds blindly, and instead extrudes the current receiver slice
      upstream
      along the sun direction before intersecting with active scene bounds. HUD and screenshot sidecars now expose
      `shadow_cascade_caster_light_ranges` so that caster-depth coverage can be compared from captures too.
      - refreshed `MeshingStress` scripted captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-csm-caster-coverage-v1/` with `FINAL` / `SHDW` / `CSM`.
    - [x] Cascade-specific caster coverage no longer affects only light-depth: per-cascade ortho `XY` fit now also
      expands around needed caster coverage, so shadows do not disappear just because a tall/upstream caster falls
      outside the receiver-only projected footprint of the nearer cascade.
      - refreshed scripted verification capture lives under
        `build/windows-clang-debug/lookdev-captures/20260424-csm-caster-xy-v1/`.
    - [x] Cascade-specific caster coverage no longer gets clipped by the shadow camera near plane: once upstream caster
      coverage expands a cascade beyond the receiver sphere, the light camera now moves upstream enough to keep the
      expanded caster range in front of the depth near plane instead of silently dropping it in mid/far cascades.
      - refreshed scripted verification capture lives under
        `build/windows-clang-debug/lookdev-captures/20260424-csm-nearplane-v1/`.
    - [x] Real per-cascade caster draw culling now exists on top of the projection-fit work: the shadow indirect buffer
      stores one chunk-draw range per cascade, CPU visibility rebuilds chunk AABBs against each cascade clip volume, and
      dirty-chunk meshing patches those same per-cascade commands on the GPU instead of drawing every opaque chunk into
      every cascade by default.
      - refreshed scripted verification captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-csm-draw-culling-v1/`.
      - bounded perf polish on top of that step: when a frame has no dirty meshing work and CPU culling reports an empty
        cascade, the renderer now skips the empty `vkCmdDrawIndirect` call for that cascade instead of submitting a
        known
        no-op draw.
    - [x] CSM split planning now follows the same visible-scene receiver range as main-pass chunk visibility instead of
      the raw camera far plane: cascades use camera near plus `min(farPlane, 64)`, so near cascades stop budgeting
      texels
      for receivers beyond the current mainline culling horizon.
    - [x] CSM default split distribution is now more near-biased after live user repro: the mainline cascade lambda
      moved
      from `0.65` to `0.80`, so the current `2048x2048` budget is spent more aggressively on near/mid receivers instead
      of leaving too much density in the far cascade.
      - refreshed scripted verification capture lives under
        `build/windows-clang-debug/lookdev-captures/20260424-csm-lambda-v1/`.
    - [x] transparent shadow policy is now explicit for the current mainline sun-shadow path:
      `GLASS_IGNORED_FLUID_CASTS`. Glass does not cast sun shadows until a separate tinted/transmission or RT path
      exists; `Fluid` casts through the current opaque shadow-map path instead of being silently ignored.
      - refreshed `VoxelLab` policy captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-fluid-shadow-policy-v1/`.
    - [x] close-range shadow acne/stair-step follow-up landed: receiver sampling now adds a small world-space bias
      toward
      the sun, skips shadow sampling for nearly unlit/backfacing surfaces, and replaces the old `3x3` box PCF with a
      weighted `5x5` PCF. Close `VoxelLab` captures live under
      `build/windows-clang-debug/lookdev-captures/20260424-shadow-acne-close-v2/`.
    - [x] one-sided voxel-face self-shadow acne is now fixed at the caster side: the shadow graphics pipeline enables
      static polygon depth bias, so lit-facing faces do not re-sample their own triangle raster pattern as
      micro-shadows.
      Verification capture lives under
      `build/windows-clang-debug/lookdev-captures/20260424-shadow-acne-caster-bias-v1/`.
    - [x] first contact-shadow baseline now lives on top of the current sun path without another render pass:
      the main voxel shader binds the same chunk descriptors + packed voxel payload as meshing, traces a short
      voxel DDA ray toward the sun, and attenuates direct sun locally through explicit
      `sunContactShadowParams={strength,maxDistance}` in `VoxelSceneLighting`.
      - `B` now also cycles a dedicated `CTSH` debug view, detailed HUD shows `CTSH STR/DST`, and screenshot sidecars
        write `contact_shadow_strength` / `contact_shadow_distance`.
      - current occluder policy matches the mainline transparent-shadow contract: `Glass` stays ignored, `Fluid`
        remains a contact-shadow occluder.
      - refreshed scripted verification captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-contact-shadow-v1/` with `FINAL` / `SHDW` / `CTSH`.
      - the first post-landing regression is fixed too: `voxel_shadow.vert` now matches the updated
        `SceneLightingBuffer` layout after `sunContactShadowParams` was inserted, so the shadow pass again reads the
        correct cascade matrices instead of shifted data.
    - [x] contact-shadow landing was revalidated with real runtime captures after the first failed `VoxelLab` check:
      `build/windows-clang-debug/lookdev-captures/20260424-contact-shadow-v4/` and the user-facing
      `build/windows-clang-debug-tracy-profiler/lookdev-captures/20260424-contact-shadow-tracy-v2/` both contain
      inspected `FINAL` / `SHDW` / `CSM` / `CTSH` frames. `FINAL` now shows the actual game frame, `SHDW` and `CSM`
      show the same visible sun-shadow region, and `CTSH` stays a local contact-only layer instead of replacing CSM.
    - [x] first ambient/contact-occlusion follow-up now exists as a bounded voxel-space `AOCC` baseline, not a full
      `SSAO/GTAO` pass: `VoxelSceneLighting.ambientOcclusionParams={strength,radius,minVisibility}` controls a short
      hemisphere DDA in `voxel.frag`, `B` cycles the dedicated `AOCC` debug view, HUD/sidecars expose the authored
      values, and the tuned baseline uses stronger normal weighting plus distance-squared falloff so large transparent
      volumes do not turn into broad fake shadows.
      - refreshed inspected captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-aocc-baseline-v2/`,
        `build/windows-clang-debug/lookdev-captures/20260424-aocc-meshing-v1/`, and
        `build/windows-clang-debug-tracy-profiler/lookdev-captures/20260424-aocc-tracy-v1/`.
      - full screen-space `SSAO/GTAO` remains a later quality pass if the renderer grows the right depth/normal pipeline;
        this slice only gives the current forward voxel path a small local occlusion layer.
    - [x] first authored local point-light contract now exists before local shadow maps/cubemaps: `VoxelSceneLighting`
      appends `localPointLightPositionAndRadius`, `localPointLightColorAndIntensity`, and `localPointLightParams`,
      presets author one inverse-square point light, and `voxel.frag` adds that light to the same GGX direct-light path.
      The current follow-up is a bounded local-shadow baseline inside that same forward voxel path:
      `localPointLightParams={enabled,sourceRadius,shadowStrength,shadowBias}` now drive a short opaque-only voxel DDA
      visibility term before any separate local shadow-map/cubemap resource exists. `Glass` and `Fluid` are both ignored
      as local-light shadow occluders for now; full local shadow maps/cubemaps remain the later quality layer.
      - `B` now also cycles `LOCL`, detailed HUD shows `LOCL` / `LCLR` / `LSHD`, screenshot sidecars write
        `local_point_light_*`, and scripted captures accept `PROJECTV_LOOKDEV_CAPTURE_VIEWS=LOCL`.
      - inspected captures live under
        `build/windows-clang-debug/lookdev-captures/20260424-local-shadow-v1/` and
        `build/windows-clang-debug/lookdev-captures/20260424-local-shadow-meshing-v1/`.
      - blocked-face local-light artifacts are now bounded too: the visibility ray no longer starts from the raw
        interpolated fragment position. It is first clamped onto a stable point on the owning voxel face, which removes
        the visible per-face fractal/moire pattern that appeared when opaque blocks fully enclosed the light. Close-up
        verification lives under
        `build/windows-clang-debug/lookdev-captures/20260424-local-shadow-fractal-fix-v1/`.
      - the same local-light fix is now also rechecked against the user-provided live repro angle instead of only a
        synthetic close-up: loading `latest.projectv.replay.snapshot.bin` through `PROJECTV_SNAPSHOT_PATH` plus the
        screenshot-sidecar camera (`cam -0.077 2.650 7.830`, `look 0.93 0.28 -0.22`) produces clean `FINAL` / `LOCL`
        captures under `build/windows-clang-debug/lookdev-captures/20260424-user-snapshot-camera-v1/`.
      - the next real close-range aliasing follow-up landed on top of that blocked-face fix too: local-light visibility
        no longer uses one hard per-pixel ray to the emitter center. `voxel.frag` now traces from a stable point on the
        owning voxel face and averages a small emitter disk around the authored `sourceRadius`, which removes the visible
        binary speckle that appeared on partially occluded faces in the user-provided `FINAL/LOCL` layer set.
      - refreshed close-up verification against the saved `F6` world snapshot lives under
        `build/windows-clang-debug/lookdev-captures/20260424-user-f6-close-angle1-v2/` and
        `build/windows-clang-debug/lookdev-captures/20260424-user-f6-close-angle2-v2/`.
      - that same local-light path no longer collapses to one constant visibility value per voxel face either: replacing
        the old face-center sample with a stabilized in-face point removes the obvious “every floor voxel has its own
        shadow bucket” artifact in close ground-level shots. Refreshed verification lives under
        `build/windows-clang-debug/lookdev-captures/20260424-user-floor-voxel-shadow-v2/`.
- [x] Текущий узкий `walk` / `creative` feel-tuning slice закрыт на нынешнем наборе live repro: `MinecraftLike`
  air-control уже baseline, high-speed creative flight wedges закрыты, held-jump restored, а auto-jump path теперь
  runtime-toggleable и replay-covered.
- [x] HUD/debug counter policy уже codified в runtime: normal vs detailed HUD split введён, а новые low-level counters
  не должны возвращаться в обычный экран без явной диагностической пользы.
- [x] **Multiplatform dev baseline (`2026-06-09`)** теперь живёт: `ProjectV` is expected to build and run on both
  `windows-clang-debug` (existing) и `linux-clang-debug` (new). Arch Linux — active Linux dev host. Linux toolchain:
  clang 22.1.6 native + lld 22.1.6 + libstdc++ 16.1.1 + SDL3 3.4.10 + Vulkan 1.4.350. Configure / build / ctest зелёные
  на `linux-clang-debug`. Source-side fixes, которые приземлились как часть baseline: `src/CMakeLists.txt`
  (`GPUOpen::VulkanMemoryAllocator` uncommented), `src/core/Types.hpp` (VMA include path), `src/ecs/EcsWorld.hpp`
  (`<cstddef>`). Root `CMakeLists.txt` Windows-жёсткие опции теперь platform-gated. `CMakePresets.json` получил
  `linux-clang-debug*` семейство. Подробное описание и follow-up риски — в `agent/memory.md` §5-8 и `agent/decisions.md`
  §17.
- [x] **Shadow-quality audit + targeted fix pass (`2026-06-09`, same-day)** закрыт в ответ на «тени лесенкой, 120 FPS,
  иногда пропадают». Шесть фиксов: A1 — `shadowRasterizer.cullMode = VK_CULL_MODE_NONE` в `VulkanGraphicsPipeline.cpp`
  (back-face cull от main pass чопал shadow map); A2 — local-light DDA теперь стартует вдоль faceNormal only
  (`voxel.frag`), self-shadow на lit-гранях уходит; A3 — frustum-cull near-check восстановлен и снабжён
  длинным комментарием (`SceneResources.hpp`), убранный по ошибке во время первой попытки и пойманный
  `TestIsSceneChunkVisibleInShadowCascade` в `tests/VoxelWorldTests.cpp:2157`; A5 — `filterRadius` clamp к `[0, 2]`
  в `voxel.frag` (debug-ladder `H/K` ceiling 8.0 раздувал PCF kernel); B1a/b/c — fragment-budget culling:
  AOCC directions 5→3, AOCC steps 6→4, local-light DDA steps 32→12. Worst-case per-pixel budget
  на лит-voxel: 252 → 134 reads (-47%). **Visual verification** через новый
  `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` (counterpart `Invoke-ProjectVRuntimeSmoke.ps1`): 6/6 captures
  на `cam -25 19 25 look 0.62 -0.48 -0.62` (`VoxelLab`), HUD FPS 121.7/123.2/116.4 для FINAL/CSM/SHDW,
  shadows continuous, no holes, contact/AOCC/local-light слои живые. Deferred: B2 (shadow map 2048→1536)
  и B3 (per-frame chunk visibility cache) — отдельные задачи. Подробный diff и lesson-learned — в
  `agent/memory.md` §10.
- [x] **Shadow-quality pass v2 — P0 закрыт (`2026-06-10`).** Stratified Poisson disk PCF (12 taps) заменил
  uniform-grid 5×5 weighted в `voxel.frag::SampleSunShadowCascade`. Бюджет чтений: 25 → 12 (снижение, не
  рост). `pcfStepScale = filterRadius * 0.75` сохранён, A5 `filterRadius` clamp `[0, 2]` остаётся в силе.
  Каждый тап additionally `clamp`-ится в `[1e-5, 1-1e-5]`, чтобы избежать `clamp-to-edge` артефактов на
  стыке каскадов. Build green, ctest 1/1, smoke 4/4 (`FINAL SHDW CSM CTSH` на
  `VoxelLab cam -25 19 25 look 0.62 -0.48 -0.62`). Visual inspection подтверждает уход
  пиксельной лесенки на стыке каскадов.
- [x] **Shadow-quality pass v2 — P0.2 реально закрыт (`2026-06-10`)** изменением `magFilter`/`minFilter`
  shadow-сэмплера с `VK_FILTER_NEAREST` на `VK_FILTER_LINEAR` в
  `src/render/vulkan/VulkanGraphicsPipeline.cpp:399-400` (одна строка). Vulkan spec 1.4 §20.2.4
  описывает hardware 2×2 PCF при LINEAR фильтре, что даёт плавный gradient
  вместо дискретных 0/1. **Lost-and-reapplied incident `2026-06-10`:** первоначальный
  fix был uncommitted в прошлой сессии, и `git checkout -- .` + `git stash drop` в W1-W5
  detour его уничтожил. Re-apply сводился к одной 2-строчной правке. Working rule:
  перед `git checkout -- .` — сохранить uncommitted state в `/tmp/`. Visual verify:
  `build/linux-clang-debug/lookdev-captures/20260610-p0_2_fix_redo/SHDW.bmp` показывает
  мягкий gradient на VoxelLab чекерном полу.
- [x] **Shadow-quality pass v2 — P0.3 — 3-4 видимые полосы на стеке voxel'ов (`2026-06-10`,
  диагноз изменился).** Изначально диагностировано как flat-shading banding и попытка
  per-corner normal averaging в `voxel_mesh.comp` (W1-W5, откачено, см. `agent/memory.md` §10.11).
  **Реальная причина — flat per-face `inAmbientVisibility` в `voxel.frag`.** Когда 4 угла грани
  voxel'я получают разные ambient occlusion значения (нижний voxel тёмный, верхний светлый),
  per-face усреднение даёт скачок яркости на границе между voxel'ами. Корректный fix —
  **per-corner packed AO**: упаковать 4 corner AO (по 8 бит) в 24 unused bits
  `PackedFace::lightingData`, в `voxel.vert` убрать `flat` с `outAmbientVisibility`,
  в `voxel.frag` убрать `flat` с `inAmbientVisibility`. Тогда rasterizer билинейно
  интерполирует AO между углами внутри каждой грани, и скачок яркости между voxel'ями
  пропадает. Дизайн и reference: Mikola Lysenko, *Ambient occlusion for Minecraft-like
  worlds - 0 FPS* (https://blog.0fps.net/2013/09/25/ambient-occlusion-for-minecraft-like-worlds/).
  Касается 3 файлов шейдеров: `voxel_mesh.comp` (функция `ComputeFaceAmbientVisibilityByte`),
  `voxel.vert` (убрать `flat`), `voxel.frag` (убрать `flat`). Без C++ изменений — данные
  уже лежат в `lightingData`. **Status: смержен в этой сессии (`2026-06-10`), visual verified.**
  `voxel_mesh.comp` теперь использует `ComputeFaceCornerPackedAO` (4×8-bit packed AO в
  `PackedFace::lightingData`), `voxel.vert` снимает `flat` с `outAmbientVisibility` и распаковывает
  `cornerIndex` байт через `(lightingData >> (cornerIndex*8)) & 0xFF`, `voxel.frag` снимает
  `flat` с `inAmbientVisibility`. Captures на VoxelLab reference shot
  `cam 3.233 4.301 12.320 look 0.65 -0.03 -0.76` живут под
  `build/linux-clang-debug/lookdev-captures/20260610-p03-per-corner-ao-v3/` (FINAL view показывает
  плавный vertical gradient вместо 3-4 горизонтальных полос). Build green, ctest 1/1.
  **Lesson learned (см. `agent/memory.md` §10.11):** incremental `cmake --build` не копирует
  свежие `.spv` в `bin/`, если `ProjectV` ELF уже up-to-date. После правки шейдеров всегда
  `cp build/.../src/voxel*.spv build/.../bin/` или `cmake --build` с явной пересборкой ELF.
  Иначе capture выглядит как pre-fix даже после корректного merge'а.
- [x] **P0.3 follow-up — face-independent 4-axis-aligned AO в vertex shader
  (`2026-06-10`, две ревизии в той же сессии).** Диагноз после P0.3: per-corner
  AO снимает flat-shading banding внутри грани, но 3 GPU vertex'а, попадающих
  в один 3D corner, по-прежнему пишут разные AOs (потому что
  `ComputeFaceCornerPackedAO` зависит от `(face, corner)`, а 3 разных грани
  дают 3 разных neighbor sets). В P0.3 это проявилось как «тёмное пятно в
  центре лицевой грани башни»: rasterizer плавно интерполирует внутри грани,
  но на стыке с другой гранью виден скачок.
  **Корректный fix — AO, привязанный к 3D-позиции, а не к (face, corner).**
  Поскольку полный GPU-hash-table welding потребовал бы ~400-600 lines изменений
  (новые welded vertex / index буферы, hash table, новые dispatches в
  `voxel_mesh.comp`, vertex input state, `VkDrawIndexedIndirectCommand`,
  `vkCmdBindIndexBuffer`, новые binding'и в `VulkanVoxelMeshingPipeline.cpp`)
  и не влезал в разумный объём одной сессии, реализован pragmatic equivalent:
  **4-axis-aligned AO в vertex shader** на integer 3D-позиции. AO level =
  (4 − occluderCount), где occluderCount = количество non-Air / non-Glass
  вокселей среди 4 axis-aligned соседей 3D-угла. **4 «диагональных»
  октанта исключены**, поэтому 4-voxel junction (4 solid + 4 air вокруг
  угла) читается как 0 occluder'ов / fully lit → 50% dark spot,
  который давал 8-surrounding, исчез. Per-corner интерполяция внутри грани
  сохраняется: 4 угла одной грани = 4 разных 3D-позиции = 4 разных AO.
  **Файлы:**
  - `src/shaders/voxel.vert` — добавлены `PackedChunkVoxelPayload` binding (binding 5),
    helper'ы `DecodeChunkVoxelMaterialVertex` / `ReadVertexNeighborMaterial` /
    `IsVertexAoOccluder` / `ComputeVertexAmbientOcclusionByte`, в `main()`
    `outAmbientVisibility` вычисляется из 4-axis-aligned через
    `ivec3(floor(worldPosition + 0.5))`.
  - `src/shaders/voxel_mesh.comp` — `ComputeFaceCornerPackedAO` становится
    no-op (возвращает 0); `PackedFace::lightingData` остаётся в 12-байтной
    структуре для совместимости descriptor barrier'а, но больше не читается.
  - `src/render/vulkan/VulkanGraphicsPipeline.cpp` — binding 5 в graphics
    descriptor set layout получает `stageFlags = VERTEX_BIT | FRAGMENT_BIT`
    (раньше был только FRAGMENT_BIT, что ломало `vkCreateGraphicsPipelines`).
  **Trade-off:** алгоритм AO меняется с 3-neighbor (Lysenko) на 4-axis-aligned.
  Оба варианта — валидный Minecraft-style AO. Per-corner интерполяция
  внутри грани сохранена. Вдвое меньше reads per vertex (4 vs 8).
  **C++ side:** 1 строка (`stageFlags` для binding 5). Никаких новых
  буферов, dispatch'ей, indirect commands, descriptor'ов.
  **Build:** green. **ctest:** 1/1 passed. **Visual:** user запустил binary
  с `cam 5.152 4.379 13.694 look 0.42 -0.12 -0.90` и подтвердил, что
  scene рендерится корректно (FPS 117.8) и тёмных пятен на 4-voxel
  junctions больше нет. Scripted smoke capture от моего agent-session
  показал пустую сцену из-за pre-existing unnamed SPIR-V binding noise
  (binding 4/6/9 в `vkCmdDrawIndirect`/`vkCmdDispatch`), но это шум
  validation layer'а — binary сам по себе рендерит корректно, что и
  подтверждает user-side capture.
  **Pre-existing багфикс заодно:** при запуске программы мышь улетала вниз,
  потому что первый `SDL_EVENT_MOUSE_MOTION` после
  `SDL_SetWindowRelativeMouseMode(true)` несёт огромный pre-capture delta.
  Добавлен `InputState::skipFirstMouseMotion` (default true) + gate в
  `HandleCameraEvent` + reset в `SetRelativeMouseMode`. После правки камера
  стабильна на старте.
  **Reference:** Mikola Lysenko, *Ambient occlusion for Minecraft-like
  worlds - 0 FPS*,
  https://blog.0fps.net/2013/09/25/ambient-occlusion-for-minecraft-like-worlds/
  (per-face-corner neighbor check, описанный там, остаётся в
  `voxel_mesh.comp::ComputeFaceCornerAmbientLevel` для reference / возможного
  revert, но не используется в рендере).
- [x] **P0.3 follow-up v2 — per-vertex AO полностью отключён
  (`2026-06-10`).** User подтвердил, что 4-axis-aligned модель всё ещё
  оставляет «псевдотень» на 3D-углу 2x2x2 куба: `3 из 4 axis-aligned соседей
  solid → AO=64 = 25% lit`, хотя с этого угла видно небо из диагонали.
  Это **структурный** артефакт: face-independent per-corner AO, считающий
  solid axis-aligned соседей, не различает «concave» (стенки вокруг 1x1
  дырки — действительно темно) и «convex 3-walls-1-sky» (выпуклый угол
  2x2x2 — небо видно, но 3 оси закрыты) — оба дают одинаково высокий count.
  **Решение:** `voxel.vert` устанавливает `outAmbientVisibility = 1.0`
  безусловно, без чтения storage buffers. Binding 5 (`PackedChunkVoxelPayload`)
  удалён из vertex shader, его descriptor-stage флаги в
  `VulkanGraphicsPipeline.cpp` свёрнуты до `FRAGMENT_BIT` (vertex shader
  больше не использует). Все helper-функции per-vertex AO
  (`ReadVertexNeighborMaterial`, `IsVertexAoOccluder`,
  `ComputeVertexAmbientOcclusionByte`, `DecodeChunkVoxelMaterialVertex`)
  удалены как dead code. Per-pixel cavity darkening сохранён через
  `ComputeAmbientOcclusionVisibility` в `voxel.frag` (AOCC ray-cast),
  который не имеет face-boundary seams и корректно затемняет
  настоящие 1x1 дыры, не трогая выпуклые углы.
  **Файлы:**
  - `src/shaders/voxel.vert` — `outAmbientVisibility = 1.0` (было: 4-axis-aligned
    formula). Удалены binding 5 и 4 helper-функции. Binding 3 (`PackedChunkDescriptors`)
    остаётся (используется для `chunkDescriptor` в vertex shader). Комментарий
    сверху объясняет, почему AO выключен и как вернуть (через compute-baked
    per-face uniform AO или welded mesh).
  - `src/render/vulkan/VulkanGraphicsPipeline.cpp` — binding 5 в graphics
    descriptor set layout получает `stageFlags = FRAGMENT_BIT` (было:
    `VERTEX_BIT | FRAGMENT_BIT`).
  **Build:** green. **ctest:** 1/1 passed. **Lesson learned:** per-vertex
  AO при face-independent constraint (welded mesh отсутствует) **фундаментально**
  не способен правильно обработать 2x2x2 corner geometry; либо weld mesh,
  либо per-pixel AOCC, либо no per-vertex AO. Зафиксировано в
  `agent/decisions.md` §14. Visual verify отдан оператору.
  **Backlog (R&D, не блокирует mainline):** per-face uniform AO,
  compute-baked; welded mesh с GPU-hash-table; full SSAO/GTAO поверх
  depth/normal G-buffer (отдельный pass, отложен до HDR/luminance пути).
- [ ] **P1 — моргание теней при движении камеры (shadow flicker / shimmer).** `clip-space` проекция
- [x] **Swapchain semaphore reuse fix (`2026-06-09`, same-day)** закрыт через **per-frame *acquire*-semaphore +
  per-image *submit*-semaphore** pattern из Vulkan SDK 1.4 guide `swapchain_semaphore_reuse.html` (документы
  в `docs/VulkanSDK-Linux-Docs-1.4.350.1/`, агент должен читать их **до** grep'а headers). Root cause: per-frame
  `imageAvailableSemaphores[2]` и `renderFinishedSemaphores[2]` индексировались по `currentFrame % MAX_FRAMES_IN_FLIGHT`
  вместо swapchain `imageIndex`. Сделано: `submitSemaphores[imageIndex]` per-swapchain-image в `SwapchainState`,
  создаются в `CreateOrRecreateSwapchain`; `vkQueueSubmit2::pSignalSemaphores[0]` и
  `vkQueuePresentKHR::pWaitSemaphores[0]` оба используют `submitSemaphores[imageIndex]`. Также opportunistically
  enabled `VK_KHR_swapchain_maintenance1` (+ dependency instance extensions
  `VK_KHR_get_surface_capabilities2` + `VK_KHR_surface_maintenance1`). Финальный warning count: **0**
  (-100% от 20), build green, ctest 1/1, smoke 6/6, vision verify FINAL view — continuous soft sun shadow,
  no staircasing, no full-floor dark. **Working rule записан в `agent/memory.md` §10.7**: для Vulkan
  semantic вопросов — docs first, headers/vulkaninfo second.

### P2

- [ ] Начать lighting/shadow foundation-first контур:
  - HDR / exposure / material contract;
  - sun shadows;
  - local shadow/contact-occlusion;
  - reflections / atmosphere;
  - temporal stabilization;
  - quality/debug ladder.

---

## 4. Mainline Backlog

### Gameplay / Debug

- [x] inspect tools;
- [x] debug tools for world mutation;
- [x] screenshot hotkey;
- [ ] frame-step / slow-motion debug modes;
- [ ] extra gizmo/debug overlays beyond current inspect / chunk / mutation overlays.

### World / Render / Tooling

- [ ] richer chunk model;
- [ ] richer world-editing workflows beyond the current lightweight debug editor;
- [ ] greedy meshing follow-up;
- [ ] richer render stats / explicit per-pass timings / chunk update timings beyond current HUD + Tracy baseline;
- [ ] RenderDoc-friendly markers;
- [ ] benchmark automation.

### Visual Quality

- [ ] HDR / tone mapping / exposure;
- [ ] physically coherent material/lighting contract;
- [ ] cascaded sun shadows;
- [ ] local lights + local shadows;
- [ ] `SSAO/GTAO`;
- [ ] `SSR`;
- [ ] volumetric fog / shafts;
- [ ] `TAA` / temporal stabilization;
- [ ] quality tiers and debug views.

---

## 5. R&D Backlog

Эти темы не блокируют ближайший milestone:

- [ ] simple sandbox interactions / gameplay loop поверх текущего sandbox;
- [ ] `SVO` и альтернативные voxel representations;
- [ ] mesh shaders / GPU-driven rendering / visibility buffer;
- [ ] hardware RT shadows / reflections / GI;
- [ ] large-world streaming / origin shifting / LOD;
- [ ] job system / heavy simulation;
- [ ] destruction playground;
- [ ] большой editor / plugin stack / multiplayer.

### Post-TAA follow-ups (отложено из `2026-06-11` TAA сессии)

Улучшения качества поверх текущего TAA baseline (RGB 3×3 clamp, motion vectors через
depth-reconstruct, 8-tap Halton(2,3), HDR linear scene color). Не блокируют mainline, не
влияют на anti-jitter цель, но дадут заметный прирост чистоты/производительности:

- [ ] **YCoCg neighbourhood clamp** в TAA resolve вместо RGB clamp (лучше clamps bright
      highlights и не теряет chroma, цена — 1 vec3 unpack/pack на пиксель).
- [ ] **Mesh-side motion vectors** через `gl_PrimitiveID` / per-face velocity (избавляет от
      depth-reconstruct, открывает путь к dilated motion для прозрачных граней).
- [ ] **Adaptive sharpening / CAS после TAA** (`tdrx`/`ffx-cas` style sharpening intensity
      от `1 - blend`, чтобы скомпенсировать perceived blur от temporal accumulation).
- [ ] **DLSS / FSR 2 / XeSS** (отдельная работа, R&D); потребует per-frame motion vectors в
      R16G16 формате, depth/normal G-buffer и UI для quality tier (Perf / Balanced / Quality).
- [ ] **Per-pass TAA tuning HUD ladder** (как у `B` debug views / `H/K` exposure): runtime
      `jitter scale`, `blend`, `neighbourhood radius`, `valid` — отдельные горячие клавиши
      + sidecar metadata.
- [ ] **Variable-rate shading** (VRS) для каскадов shadow / AOCC / contact shadow — экономия
      fragment bandwidth в зонах, где TAA уже гарантирует temporal stability.
- [ ] **Camera-cut detection** (резкий скачок `viewProjDelta` > threshold → автоматически
      invalidates history на 1+ кадров, чтобы не было «smear» артефакта при instant teleport).
- [ ] **Anti-flicker для contact / AOCC / local light** через тот же history buffer
      (отдельный mini-TAA проход на этих слоях, blend factor поменьше).
- [ ] **Velocity buffer для diffuse irradiance / specular probes** (отложено до deferred
      pass; current forward path не имеет G-buffer, поэтому diffuse TAA не применим).
- [ ] **Halton(2,3) → longer cycle (16-tap)** если 8-tap покажет visible shimmer-pattern
      на slow-look сценах; trade-off — больше aliasing frequency, но плавнее converge.
- [ ] **HDR scene color → R11G11B10_UFloat** как эксперимент bandwidth saving (текущий
      R16G16B16A16_SFLOAT — самый дорогой формат; на 1440p ~24MB на target).
- [ ] **Quality tier abstraction** (`TaaQuality::Off` / `Light` / `Standard` / `High`) с
      presets, runtime switchable через debug ladder (отдельный P-уровень от `taaEnabled`).

---

## 6. Риски

- Документация и `agent/` быстро дрейфуют, если после задачи обновить только код.
- В `build/windows-clang-debug` нельзя гонять несколько `build/test/smoke` параллельно; если smoke нужен, запускать его
  только последовательно после build/tests.
- `walk` легко регрессирует от широких эвристик; для него приоритетны live repro, fixed-step tests, HUD и Tracy.
- `Problems/` export из JetBrains быстро устаревает; перед следующим warning-cleanup pass его нужно заново выгружать, а
  не считать текущий XML источником истины.
- `README.md`, vendored submodules и часть `docs/` могут содержать user-owned изменения; incidental edits нежелательны.

---

## 7. Недавние Закрытия

Держать здесь только крупные факты, которые влияют на ближайший roadmap:

- [x] legacy docs больше не split между параллельными `latest` / `old` roots: теперь это один унифицированный `legacy/docs` tree с явной навигацией, `archive/roadmaps` для исторических планов, обновлёнными ссылками из `AGENTS.md` / `agent/session-checklist.md`, восстановленным полным library corpus и возвращёнными `guides/`, `tutorials/`, `examples/`, `architecture/future/` + missing `theory` notes.
- [x] `TODO.md` / `AGENTS.md` / `agent/` снова синхронизированы по ролям и очищены от длинного исторического дубляжа;
- [x] snapshots (`F6/F7`);
- [x] lightweight debug editor (`F8/F9/F10`);
- [x] repeatable build/test/smoke contour;
- [x] walk live HUD + Tracy diagnostics;
- [x] dual air-control modes (`F11`);
- [x] placement/body overlap guard;
- [x] delayed auto-jump toggle (`F12`);
- [x] inspect tooling now exposes target/placement chunk facts, local voxel coords, hit normal, mutation anchor state, preview box, and world-edit version directly in the HUD/overlay path.
- [x] lightweight world-mutation helpers now include `X` anchor-based cuboid paint/erase and `M` material pick, covered by fixed tests instead of ad-hoc runtime checks.
- [x] one-block auto-jump is now runtime-toggleable (`J`) and defaults off; when enabled, the `F12` micro-delay arms only once the immediate one-block rise is actually reachable instead of pre-arming from a distant probe.
- [x] debug HUD now has basic vs detailed modes (`G`): normal HUD hides low-level walk/selection/replay counters and the green placement preview, while detailed HUD keeps the full diagnosis surface.
- [x] held-jump repeat restored.
- [x] moving narrow-edge traversal with held `W` no longer drops `walk` into synthetic `Air`; jump can still commit from partial edge support (`5.tracy` case).
- [x] exact first jump press on the thinnest edge support no longer depends on pre-tick `supportState`: the replay fixture now proves the jump still launches from remaining support hits, while ordinary walk-off without jump is no longer kept alive by the same rule.
- [x] jump landing back onto recent takeoff-plane no longer stays `Air` on the support plane and then drops late (`6.tracy` case).
- [x] author-refactor fallout no longer breaks `PhysicsWorld.cpp` build; Jolt include order restored and `build/test/smoke` are green again.
- [x] crouch-jump into a glass column no longer turns side-wall voxels into fake sneak support; `Shift` wall-slide regression is covered by fixed-step tests and `build/test/smoke` stay green.
- [x] active ballistic jump can no longer reuse cached ground-takeoff grace for a second airborne jump; the `7.tracy` retry-jump path is now covered by a state-driven fixed-step regression instead of a guessed frame.
- [x] successful `SyncPhysicsWorld` after voxel edits now invalidates cached walk support/anchors before the next tick, so removed support cannot survive only as stale walk ownership.
- [x] Runtime input replay now exists for walk bugs: `R` records snapshot + per-frame input into the latest replay file, `Y` replays it in-game, and tests can load the same capture instead of rebuilding long manual key scripts.
- [x] replay-driven crouch wall-cling no longer authorizes grounded support at the caller's midair `feetY`; sneak support is now anchored to the sampled top-plane and the strengthened two-block regression stays green.
- [x] replay-driven stacked placed-block wall climb no longer reacquires foreign ground-takeoff planes or midair crouch support from too far below the support plane; the same live replay now stays at `feetY=1.050` instead of climbing to `2.050/3.050`.
- [x] replay-driven boosted creative flight through `TransparencyStress` no longer wedges on glass columns or exact glass-corner hits at high speed; `TickCreativeCharacter` now substeps long boosted travel more aggressively and the exact captures live in `tests/fixtures/creative_transparency_boost_stuck.*` and `creative_transparency_boost_corner_stuck.*`.
- [x] opaque voxel faces under glass no longer disappear: the meshing shader now emits opaque faces against `Glass` neighbors while still culling the internal glass face on the same plane.
- [x] Problems-driven warning cleanup removed the remaining current `clang-tidy` findings around `InputActions`, `Types`, `EcsWorld`, and `PhysicsWorld`; `build -> tests -> smoke` is green again, and the next warning pass now has an explicit reminder to regenerate `Problems/` instead of trusting stale XML line numbers.
- [x] refreshed JetBrains `Problems/` export no longer points at the old stale-pointer helpers in `PhysicsWorld.cpp`: internal walk helpers now use reference contracts where `nullptr` was never meaningful, dead descending-ledge/jump-lock DFA-only paths were removed, and `build -> tests -> smoke` remains green after the refactor.
- [x] follow-up `Problems/` + `problems/tools/` cleanup removed the remaining live switch/default warning in `PhysicsWorld.cpp`, collapsed several test-only inspection nits in `VoxelWorldTests.cpp` (bitmask helpers, constexpr/deduced arrays, structured bindings, integer scan loop), and kept `build -> tests -> smoke` green; the leftover `CppDFAUnreachableFunctionCall` rows in the checked-in tools export still need a fresh re-export because the current `main()` already calls those tests directly.
- [x] follow-up DFA cleanup in `VoxelInteraction.cpp` / `DebugOverlays.cpp` removed more stale nullable-out-param and redundant-branch warnings by switching file-local helpers to reference contracts and by tightening anchored-paint preconditions without changing runtime behavior.
- [x] latest checked-in `problems/` + `problems/tests/` follow-up removed the remaining visible current-source
  inspection nits in `VoxelMaterials.cpp`, `Renderer.cpp`, `AppUpdate.cpp`, `SceneResources.cpp`,
  `VulkanGraphicsPipeline.cpp`, and `VoxelWorldTests.cpp` (reference-only helper contracts, stale `+0.5f` integer cast,
  redundant BMP parsing expressions, structured bindings, constexpr data, and safe `size_t` sizing). Sequential
  `build -> tests -> smoke` is green again in `windows-clang-debug`, plus `build + smoke` is green in
  `windows-clang-debug-tracy-profiler`; any leftover `CppDFAUnreachableFunctionCall` / `CppDFAConstantParameter` rows in
  the checked-in XML should now be treated as stale export artifacts until the next fresh JetBrains export.
- [x] fresh `problems/tests/` follow-up after a new JetBrains export removed the remaining real helper/DFA nits in
  `tests/VoxelWorldTests.cpp` (`repeat`-specific key helper, one-off bitmask helper, LE32 helper shape) and now
  suppresses `CppDFAUnreachableFunctionCall` at file scope for that custom single-TU runner. `ProjectVTests` still
  builds and passes in `build/windows-clang-debug`; the remaining usefulness of `problems/tests/` is now in real
  helper/dataflow issues, not in JetBrains trying to rediscover reachability through this bespoke test harness.
- [x] fresh root-level `problems/` export follow-up removed the remaining current-source inspection tails in
  `SceneResources.cpp/.hpp`, `ShadowProjection.cpp/.hpp`, `ScreenshotCapture.cpp`, `DebugHud.cpp`, `Types.hpp`,
  `FramePreparation.cpp`, `VoxelMaterials.cpp/.hpp`, `VoxelWorldTests.cpp`, and the touched shadow docs. The cleanup
  replaced fake-nullable helper contracts with references where `nullptr` was not meaningful, removed stale
  iterator/cast boilerplate around CSM matrix copies, restored the local SDL Vulkan include contract to
  `VulkanBootstrap.cpp` after pruning an unused transitive header include, and kept sequential
  `cmake --build build/windows-clang-debug --config Debug --parallel 8` +
  `ctest --test-dir build/windows-clang-debug --output-on-failure -C Debug`
  green. Runtime smoke was not rerun because this pass did not touch the targeted Vulkan/bootstrap/swapchain lifecycle
  contract.
- [x] detailed HUD help no longer flickers when stat lines get longer at runtime: the debug-HUD vertex budget was too
  small for the pixel-quad text path with shadows, so the buffer cap is now larger and covered by a rich detailed-HUD
  regression.
- [x] roadmap wording was tightened so shipped baseline features no longer masquerade as untouched backlog: current
  lightweight world editing, inspect/chunk/mutation overlays, and the existing HUD + Tracy stats path are now treated as
  the baseline, while the open backlog explicitly points only at richer follow-up tooling.
- [x] gameplay-oriented `simple sandbox interactions` are no longer treated as the next mainline slice: they moved to
  R&D/backlog, while the next practical work is now lighting/look-dev for the current demo-scene direction.
- [x] first lighting/look-dev kickoff landed in runtime: scene presets now carry explicit exposure/tone-map baseline
  alongside sky/horizon/ground/sun/fog, the renderer clear color follows the same scene-lighting contract, and the live
  sandbox now has a minimal lighting debug ladder (`B/N/H/K/V`) with HUD visibility.
- [x] first practical sun-shadow prototype landed for the current renderer: a single scene-wide orthographic shadow map
  now renders the opaque voxel scene before the main pass, the main voxel shader samples it for direct sun only, and
  `build -> tests -> smoke` is green on that path.
- [x] first sun-shadow quality/debug follow-up landed: the baseline shadow map is now `2048x2048`, the main voxel shader
  uses weighted `5x5` PCF instead of a single hard compare sample, and the existing lighting debug ladder now
  includes a dedicated `Shadow` view plus detailed HUD telemetry for current shadow tuning.
- [x] entering `TransparencyStress` after another preset no longer device-lost: the shadow pass now has its own
  descriptor contract and follows the renderer's sparse per-chunk face layout via opaque indirect commands instead of
  assuming a dense opaque-face prefix. `build -> tests -> smoke` plus manual
  `VoxelLab -> FlatBenchmark -> TransparencyStress` repro are green in both debug build trees.
- [x] the shadow path no longer inherits camera-frustum culling: compute meshing now maintains a dedicated all-occluder
  `shadowIndirectBuffer` for opaque faces, the shadow pass consumes that buffer instead of the main opaque visibility
  commands, and `build -> tests -> smoke` stayed green after the change. The remaining limitation is now more honest:
  transparent-heavy `VoxelLab` still reads mostly shadowless because the current sun-shadow baseline is opaque-only.
- [x] shadow tuning is now capture-friendly inside the runtime loop itself: `C` saves a `.bmp` of the current frame plus
  a `.txt` sidecar with preset/exposure/shadow state, and `build -> tests -> smoke` is green in both debug build trees
  after wiring that path.
- [x] sun-shadow projection no longer wastes coverage on empty scene padding: the CPU fit now prefers active chunk
  bounds over full `VoxelWorld` bounds, so the same `2048x2048` map lands on denser useful texels in opaque-heavy
  presets without adding cascades or camera-fit R&D.
- [x] the sun-shadow contract no longer disagrees about the sign of the authored sun vector: `sunDirectionAndWrap.xyz`
  remains the shading-side vector toward the sun, while the CPU shadow projection now flips it to the actual
  light-travel direction before building the shadow camera, so direct light and shadow placement no longer fight each
  other. `build -> tests -> smoke` is green in both debug build trees after the fix.
- [x] shadow receiver bias is no longer a flat brute-force offset for every angle: the voxel shader now scales the
  authored depth/normal bias by `N.L`, which reduces grazing-angle acne without demanding the same constant offset on
  directly lit flat tops. `build -> tests -> smoke` is green in both debug build trees after the change.
- [x] close-range shadow acne/stair-step artifacts got a bounded shader-side fix: receiver projection now includes a
  small sun-direction world-space bias, shadow sampling skips nearly unlit/backfacing faces, and PCF is a weighted `5x5`
  kernel rather than the old `3x3` box kernel.
- [x] the remaining one-sided micro-triangle acne was identified as caster-side self-shadowing, not a filtering problem:
  the shadow pipeline now uses Vulkan polygon depth bias for shadow-map writes, with a close `VoxelLab` repro capture
  under `build/windows-clang-debug/lookdev-captures/20260424-shadow-acne-caster-bias-v1/`.
- [x] first physically-coherent direct-light follow-up landed on top of the preset-based lighting contract:
  `VoxelMaterialVisual` no longer packs ad-hoc ambient/diffuse/spec/shininess knobs, the runtime material table now
  carries `AO/roughness/metallic/reflectance` plus transmission tint and fog/emissive/ambient/direct-response hooks, and
  `voxel.frag` shades direct sun with a `GGX + Fresnel-Schlick + Smith` baseline while keeping the current ambient
  gradient, fog and sun-shadow loop intact.
- [x] opaque-heavy capture baselines were refreshed after the BRDF/material shift using a scripted runtime path rather
  than manual keyboard timing: startup camera override + `FINAL/SHDW` capture automation now cover `ChunkGrid` and the
  fixed `MeshingStress` reference shot, and the screenshot readback path no longer races present against the
  post-render transfer copy.
- [x] ambient/environment fill contract is now explicit in the existing lighting buffer: `postProcess.y` carries the
  per-preset environment diffuse intensity, the shader separates sky/horizon/ground fill from direct sun, and refreshed
  `FINAL/AMB/SHDW` captures prove the fill layer without hiding sun-shadow contrast.
- [x] local ambient visibility now complements that fill contract: compute meshing bakes a cheap per-face sky-visibility
  term into the packed face payload, `voxel.frag` multiplies ambient fill by it, and closed voxel cavities no longer
  read as if their upward-facing surfaces still saw unobstructed sky.
- [x] minimal exposure/grading contract is now explicit too: `VoxelSceneLighting.colorGrading` carries white point,
  contrast, saturation and lift; shader output and clear color both apply the same post-tone-map grading, and refreshed
  `FINAL/AMB/SHDW` captures live under `build/windows-clang-debug/lookdev-captures/20260424-grading-v1/`.
- [x] first auto-exposure policy is now explicit without adding an HDR histogram pass:
  `VoxelSceneLighting.exposureControl`
  carries metering mode / target key / min exposure / max exposure, `SceneKey` exposure is computed on CPU from the
  authored scene brightness, and refreshed captures live under
  `build/windows-clang-debug/lookdev-captures/20260424-auto-exposure-v1/`.
- [x] `VoxelLab` gained opaque anchor geometry for look-dev: a small solid stepped marker outside the transparent sphere
  gives the demo scene a readable opaque sun-shadow caster/receiver without changing the transparent-shadow policy.
- [x] first real CSM renderer step landed: CPU split planning now feeds a 4-layer Vulkan shadow depth array, per-cascade
  light matrices, shader-side cascade selection, `CSM` debug visualization, capture metadata, texel-grid snapping, and
  regression tests.
- [x] first shader-side CSM split-transition follow-up landed: split edges no longer hard-switch in `voxel.frag`, the
  blend band is runtime-tunable via the existing shadow ladder, HUD/sidecar metadata expose `shadow_cascade_blend`, and
  refreshed `MeshingStress` `FINAL/SHDW/CSM` captures live under
  `build/windows-clang-debug/lookdev-captures/20260424-csm-blend-v1/`.
- [x] first cascade-specific CSM caster coverage follow-up landed: per-cascade depth fit now uses the receiver slice
  extruded upstream along the sun direction instead of blindly using full active-scene bounds for every cascade, and
  sidecars/HUD now expose `shadow_cascade_caster_light_ranges`. Refreshed `MeshingStress` `FINAL/SHDW/CSM` captures live
  under `build/windows-clang-debug/lookdev-captures/20260424-csm-caster-coverage-v1/`.
- [x] CSM receiver planning now follows the same visible-scene distance contract as chunk visibility: cascade splits are
  built from camera near plus `min(farPlane, 64)` instead of the raw camera far plane, so current mainline no longer
  spends shadow split budget on receivers that chunk culling never draws.
- [x] transparent shadow policy is now intentionally simple: the current sun-shadow pass renders only opaque casters and
  reports `GLASS_IGNORED_FLUID_CASTS` in HUD/capture metadata. Glass shadows wait for a later dedicated
  tinted/transmission path, while `Fluid` casts through the current opaque shadow-map path.
  Refreshed `VoxelLab` `FINAL/SHDW` captures live under
  `build/windows-clang-debug/lookdev-captures/20260424-fluid-shadow-policy-v1/`.
