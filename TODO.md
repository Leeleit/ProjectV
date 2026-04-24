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
- [x] Текущий узкий `walk` / `creative` feel-tuning slice закрыт на нынешнем наборе live repro: `MinecraftLike`
  air-control уже baseline, high-speed creative flight wedges закрыты, held-jump restored, а auto-jump path теперь
  runtime-toggleable и replay-covered.
- [x] HUD/debug counter policy уже codified в runtime: normal vs detailed HUD split введён, а новые low-level counters
  не должны возвращаться в обычный экран без явной диагностической пользы.

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
