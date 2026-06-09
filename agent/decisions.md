# Decisions

Живые инженерные договорённости. Roadmap живёт в `TODO.md`, общий протокол — в `AGENTS.md`.

Дата обновления: `2026-04-24`

---

## 1. Document boundaries

Решение:

- `TODO.md` хранит roadmap, приоритеты, backlog и риски.
- `AGENTS.md` хранит только обязательный протокол работы агента.
- `agent/memory.md` хранит только долговечные repo-specific факты и ограничения.
- `agent/status.md` хранит только короткий активный снимок.
- `agent/decisions.md` хранит только действующие инженерные договорённости.

Почему:

- Иначе цена обязательного чтения растёт быстрее полезного контекста, а документы начинают пересказывать друг друга.

## 2. Mainline vs R&D

Решение:

- Mainline = reproducible interactive voxel MVP.
- Near-term mainline emphasis for this repo is demo-scene graphics/look-dev plus foundational mechanics, not gameplay-loop expansion.
- Тяжёлый R&D (`SVO`, mesh shaders, heavy simulation, big-world systems, большой editor, multiplayer, plugin stack) не должен блокировать ближайший practical milestone.
- Gameplay-facing sandbox interactions can live in R&D/backlog until the lighting/look-dev foundation is stronger.

Почему:

- Ближайшая ценность проекта — живой, измеримый и расширяемый sandbox slice, а не новый фундамент.

## 3. Control-mode contract

Решение:

- `creative` = collision-backed flight/edit mode и подчиняется `pause`.
- `spectator` = observe-only noclip без world edits и без подчинения `pause` для movement/look.
- `walk` = grounded physics mode.
- Double-tap `Space` переключает только `creative <-> walk`.
- `F4` остаётся общим циклом control modes.

Почему:

- Режимы должны быть явными и предсказуемыми, а physics-backed path не должен обходить paused simulation.

## 4. Build / verification contract

Решение:

- Mainline repeatable build path живёт на `windows-clang-debug` и `windows-clang-debug-ci`.
- Verification loop выполняется только последовательно: build/test/smoke-команды не запускать параллельно в одном build tree.
- Runtime smoke остаётся отдельной developer-only GUI-проверкой и вызывается как официальный target, но это targeted
  lifecycle check, а не mandatory DoD для каждой задачи.
- `ProjectVRuntimeSmoke` запускать после изменений в Vulkan/bootstrap/swapchain/window lifecycle/present/screenshot sync
  или при риске device-lost/hang. Для shader/material/lighting tuning, docs и unit-testable логики использовать
  build/tests плюс task-specific validation вроде scripted captures.
- Shader compile path принимает `glslc` или `glslangValidator`.
- Для translation units с Jolt include-contract начинается с `<Jolt/Jolt.h>`; auto-refactor не должен поднимать другие Jolt headers выше него.

Почему:

- Это сохраняет reproducible contour для mainline без лишней хрупкости, конфликтов build tree и пустых smoke-ритуалов.

## 5. Interaction contract

Решение:

- World edit остаётся CPU-authored через `VoxelRaycast` и `VoxelWorld`.
- Постановка блока запрещается до мутации мира, если `placementVoxel` пересекает текущий physics-character volume.
- После successful world-edit rebuild через `SyncPhysicsWorld` cached walk support ownership надо инвалидировать до следующего walk tick.
- Lightweight debug world-mutation stays keyboard-driven on the same interaction path: `X` toggles a box anchor for paint/erase tools, `M` picks the current hit material, and the HUD/overlay path stays the source of truth for preview/debug facts.

Почему:

- Physics помогает interaction path, но не заменяет его как source of truth.
- Reject-before-mutate проще и устойчивее, чем разрешать edit и потом выталкивать игрока из нового блока.
- Stale support/anchors после удаления блока не считаются допустимым контрактом.

## 6. Walk authority contract

Решение:

- Static-world `walk` в этом репо авторится voxel solver'ом из `PhysicsWorld.cpp`, а не `CharacterVirtual::ExtendedUpdate`.
- `CharacterVirtual` остаётся proxy/stance carrier и частью collision/contact infrastructure, но не главным источником grounded ownership.
- Для live walk diagnosis приоритетны fixed-step tests, HUD и Tracy.

Почему:

- Именно этот path сейчас покрыт regression suite и соответствует текущему runtime behavior.

## 7. Walk jump / air-control contract

Решение:

- Rising jump не должен использовать voxel top-promotion.
- `WalkAirControlMode::MinecraftLike` — default; `Realistic` сохраняет older direction-lock behavior.
- Held `Space` снова считается валидным manual jump request после возвращения в grounded-like state.
- Ordinary `walk` horizontal motion нельзя анализировать по `velocity.xz`; для него нужны explicit walk-step facts.
- Cached ground-takeoff grace может авторизовать coyote/takeoff handoff только до первого jump commit; после того как ballistic jump уже active, она не даёт second airborne jump.
- Cached ground-takeoff plane не переобновляется во время active ballistic jump, а landing-back handoff разрешён только на тот же cached takeoff plane в пределах cached drift; широкий support вокруг стоп не считается достаточным сам по себе.
- Moving partial edge support при активном ходе тоже считается grounded-like handoff: если `footSupportScore` держится примерно на половине footprint, `feetY` стабилен и `velY` не растёт вверх, `UpdateWalkGroundSupport` должен выдавать `EdgeGrace`, а не `Air`.
- Ultra-thin edge support не превращается в generic sticky ledge hold: дополнительный handoff для `footSupportScore < 0.2` разрешён только под активный jump request и только чтобы первый jump press на самой кромке всё ещё мог стартовать с оставшихся support hits.
- Landing обратно на recent ground-takeoff plane после jump ballistic path тоже считается grounded-like handoff: если широкий takeoff-support ещё валиден и стопы уже вернулись на ту же top-plane, `UpdateWalkGroundSupport` должен вернуть хотя бы `EdgeGrace`, а не оставлять `Air`.
- Sneak-support region не должен считать боковой wall voxel опорой сам по себе: crouch-grounded ownership разрешён только когда capsule footprint реально перекрывает top-face support voxel, а не просто попадает в расширенный `XZ`-region рядом со стеной.
- Sneak-support region anchor по `Y` должен быть реальной sampled top-plane, а не текущей высотой стоп вызывающего path; иначе crouch wall-cling может получить fake grounded в midair.
- Sneak-support region membership требует не только `XZ` overlap, но и близость стоп к sampled support plane; если стопы ощутимо ниже `referenceFeetPosition[1]`, crouch не должен получать grounded ownership на более высокой поверхности.

Почему:

- Это текущий минимально устойчивый контракт, который не ломает established edge/jump regressions и остаётся достаточно понятным для дальнейшего тюнинга.

## 8. Auto-jump contract

Решение:

- One-block auto-jump остаётся optional traversal path, а не always-on movement baseline.
- Runtime default for auto-jump is `off`; `J` переключает existence auto-jump.
- Если auto-jump включён, `F12` переключает только `delay on/off`, а countdown starts only once the immediate one-block rise is actually reachable.
- Manual held jump обнуляет pending auto-jump delay countdown.

Почему:

- Нужны оба режима: manual baseline without silent auto-step, plus delayed Minecraft-like traversal и instant response для будущих bunny-hop experiments.

## 9. HUD verbosity contract

Решение:

- `F1` по-прежнему переключает весь debug UI.
- `G` переключает normal HUD и detailed HUD.
- Normal HUD держит только high-level sandbox/control facts; low-level walk grace counters, selection/chunk/mutation/replay telemetry и зелёный placement preview показываются только в detailed HUD.

Почему:

- Обычный runtime screen должен оставаться читаемым, а диагностическая перегрузка нужна только когда агент или пользователь реально разбирает баг.

## 10. Debug / repro contract

- When a live walk bug diverges from synthetic fixtures, the preferred artifact is an input replay capture over another handwritten `SendKeyEvent` sequence.

Решение:

- Claims о walk/runtime regressions сначала проверяются через live repro + `PhysicsWalkDebugInfo`/HUD/Tracy, а не через blind heuristic patch.
- Высокий render FPS сам по себе не считается доказанной причиной walk bugs, пока это не подтверждено через real fixed-step path.

Почему:

- Этот проект уже несколько раз платил за попытки чинить live runtime bug только по synthetic-case тестам.

## 11. Creative flight collision contract

Решение:

- `creative` остаётся на `CharacterVirtual::ExtendedUpdate`, но boosted flight не делает один длинный collision step.
- `TickCreativeCharacter` делит длинный boosted travel на capped substeps по расстоянию (`~0.05 m`, максимум `32` substeps) и повторяет `ExtendedUpdate` на каждом substep.
- Regression для этого path держится на exact replay fixtures `tests/fixtures/creative_transparency_boost_stuck.*` и `creative_transparency_boost_corner_stuck.*`, а не на коротком synthetic-case приближении.

Почему:

- Normal-speed creative collision уже скользил корректно; ломался только high-speed coarse-step path, включая точные corner hits.
- Exact replay здесь надёжнее выдуманного теста, потому что старый synthetic-case уже давал ложный red/green сигнал и не совпадал ни с реальным клином на стеклянных колоннах, ни с клином ровно в угол.

## 12. Static-analysis cleanup contract

Решение:

- Checked-in `Problems/*.xml` inspection exports are treated as hints, not as the source of truth for live code.
- During warning cleanup, only issues that still reproduce on the current source, or are trivially visible in the current code, should be patched immediately.
- After a meaningful cleanup pass, regenerate `Problems/` before starting the next pass.
- For the bespoke single-TU runner in `tests/VoxelWorldTests.cpp`, file-level JetBrains suppression of `CppDFAUnreachableFunctionCall` is acceptable: the custom harness still builds and runs correctly, but JetBrains DFA does not model its reachability graph reliably enough to make that inspection actionable there.

Почему:

- The current refactor/lint sweep already made several exported line-based findings stale mid-pass, and blindly following them risks fixing the wrong code.
- The remaining `CppDFAUnreachableFunctionCall` rows in a fresh `problems/tests/` export were not pointing at dead code; they were pointing at directly called tests/helpers inside the custom harness.

## 13. Transparency meshing contract

Решение:

- Transparent-neighbor meshing is intentionally asymmetric: opaque voxels emit faces against `Glass`, but `Glass` keeps the internal shared face culled against opaque neighbors.

Почему:

- Иначе блок под стеклом теряет видимую верхнюю грань, а double-face на одной плоскости дало бы z-fighting и лишнюю transparent geometry.

## 14. Lighting look-dev contract

Решение:

- Первый lighting contract живёт в `VoxelSceneLighting`: sky/horizon/ground/sun/fog плюс baseline exposure/tone-map/debug-view post-process.
- `postProcess.y` in `VoxelSceneLighting` is reserved for per-preset environment diffuse intensity. It is not a generic scratch slot.
- Ambient/environment fill must not stay purely normal-based once it causes sealed voxel cavities to read as open sky.
  The current bounded fix is a cheap meshing-side local visibility term in `PackedSceneVoxelFace::lightingData`, which
  the main voxel shader multiplies into sky/horizon/ground fill. Current blocker policy for that term is
  `Air/Open`, `Glass/Open`, `Fluid/Occluder`, `Opaque/Occluder`; this is not `SSAO/GTAO`.
- `colorGrading` in `VoxelSceneLighting` is reserved for the minimal grading contract: white point, contrast,
  saturation, lift. It is applied after tone mapping and the clear color must use the same grading path.
- `exposureControl` in `VoxelSceneLighting` is reserved for exposure metering mode, target scene key, minimum exposure,
  and maximum exposure. Current mainline policy is CPU-side `SceneKey`, not GPU histogram/adaptive exposure.
- `UpdateSceneResources` освежает current scene lighting из `VoxelScenePreset` и runtime look-dev controls каждый кадр, а renderer clear color использует тот же contract вместо отдельной hardcoded sky-константы.
- Current look-dev ladder остаётся keyboard-first внутри живого sandbox loop: `B` cycles lighting debug views, `N` cycles tone-map, `H/K` adjust exposure, `V` resets to preset baseline.
- Reproducible look-dev capture stays inside the same runtime path too: `C` saves a `.bmp` of the current frame plus a sidecar metadata file with preset/exposure/shadow tuning, instead of treating screenshot capture as an external-tool-only workflow.
- Baseline refreshes that need exact camera/view reproducibility should use the env-driven startup camera and capture automation (`PROJECTV_START_CAMERA_POSITION`, `PROJECTV_START_CAMERA_LOOK`, `PROJECTV_LOOKDEV_CAPTURE_VIEWS`, warmup/interval/quit knobs) instead of manual key timing.
- Screenshot capture is part of the frame command stream. If capture copy commands are recorded after color rendering, the render-finished semaphore must not be signaled at `COLOR_ATTACHMENT_OUTPUT`; present has to wait for all recorded commands so the transfer copy and final layout transition cannot race presentation.

Почему:

- Так lighting/look-dev остаётся reproducible внутри текущего MVP loop без отдельного editor path и без скрытого shader-only состояния, которое трудно отлаживать и сравнивать между сценами.
- Так scripted captures become a real baseline artifact rather than a best-effort manual screenshot, and screenshot readback remains deterministic enough for visual comparisons.
- Explicit environment intensity keeps ambient readability tunable per scene without treating indirect fill as hidden shader magic or faking it through shadow strength.
- The first cavity-darkening fix should stay inside the existing voxel meshing + forward shading contract instead of
  jumping straight to screen-space AO. A local voxel-neighborhood visibility term solves the obvious "closed niche still
  sees full sky" bug without adding another heavy pass or pretending mainline already has real GI.
- Minimal grading is a fixed per-preset contract for now; auto exposure remains a separate follow-up and should not be
  smuggled in as hidden shader state.
- The first auto-exposure policy should stay deterministic and cheap until the renderer has a real HDR/luminance path:
  `SceneKey` estimates authored scene brightness from sky/horizon/ground/sun terms on CPU, then manual exposure bias is
  applied on top.

## 15. First sun-shadow path
Update `2026-04-22`:

- The earlier "render the whole opaque face prefix with a direct draw" version of this path is obsolete. Packed opaque faces live in sparse per-chunk ranges, and the dense-prefix assumption caused `VK_ERROR_DEVICE_LOST` when switching into `TransparencyStress`.
- The shadow pass now binds its own descriptor/pipeline layout; it must not reuse the main graphics descriptor set that already samples the shadow image while that image is simultaneously written as a depth attachment.
- The current stability-first baseline now uses a dedicated all-occluder `shadowIndirectBuffer` for opaque casters. Compute meshing updates that buffer for dirty chunks, CPU keeps it warm for unchanged chunks, and the shadow pass no longer inherits camera-frustum culling from the main opaque visibility commands.
- Transparent shadow policy is explicit: the current mainline sun-shadow path uses `GLASS_IGNORED_FLUID_CASTS`. `Glass`
  does not cast shadows until a separate tinted/transmission or RT-oriented path exists; `Fluid` casts through the
  current opaque shadow-map path.
- Because `Fluid` still uses the main opaque draw range for forward rendering, the shadow fragment shader must only
  discard `Glass`. Discarding `Fluid` makes water incorrectly shadowless.
- `VoxelLab` may contain opaque anchor geometry for look-dev readability. This is scene composition for the current
  opaque-only shadow path, not a decision that glass/fluid should cast into the shadow map.
- CSM entered mainline as explicit bounded stages. The current accepted stage is the first real renderer hookup:
  4 cascades, practical split lambda `0.80`, 4-layer Vulkan depth array, per-cascade light matrices,
  `sampler2DArrayShadow` sampling selected by camera view-depth, HUD/sidecar/test visibility, and `CSM` debug view.
  Cascade projection centers snap to the shadow texel grid using the active shadow-map resolution. The next accepted
  bounded stage on top of that is coverage diagnostics, not another shadow feature: per-cascade view ranges, ortho
  extents, and texel density are runtime-visible in HUD/sidecar/test output before any deeper caster culling or
  split-edge tuning is attempted.
  The first actual split-edge stability follow-up after that diagnostics stage is a rotation-stable sphere fit for the
  cascade `XY` extent, not another opaque hidden AABB heuristic.
  The next accepted split-edge follow-up after that stable fit is shader-side split transition blending, but only as an
  explicit runtime contract: blend width is tunable via the same shadow ladder/HUD/sidecar loop, not a hidden shader
  constant. The next accepted coverage follow-up after that is cascade-specific caster depth coverage: build per-cascade
  caster bounds from the current receiver slice extruded upstream along the sun direction, not from blindly reusing full
  active-scene bounds for every cascade.


Решение:

- Первый practical CSM shadow path для mainline — 4-layer sun shadow depth array, not RT shadows or a heavier lighting stack.
- Shadow contract живёт в том же `VoxelSceneLighting`: per-preset shadow tuning (`strength/bias/normal-bias`) плюс
  `sunShadowViewProjections[4]` and `shadowCascadeDepthSplits`, которые CPU собирает из camera view slices, active
  scene bounds, and sun direction.
- `sunDirectionAndWrap.xyz` remains the authored vector toward the sun for the main shading pass. The CPU shadow fit must invert it to the actual light-travel direction when building sun-shadow projections; this sign is part of the stable lighting contract, not an implementation detail.
- Shader-side receiver bias stays on the same authored `depth-bias` / `normal-bias` controls, but it should respond to sun angle instead of acting like one flat offset everywhere. The current baseline therefore scales those authored bias values by `N.L`, adds a small world-space receiver offset toward the sun, and avoids shadow sampling on nearly unlit/backfacing surfaces instead of adding a second hidden bias ladder.
- Shadow-map writes also need caster-side polygon depth bias. One-sided triangular acne on a lit voxel face means the
  caster surface is re-sampling its own rasterized shadow-map triangles; increasing PCF alone is the wrong fix.
- The first practical direct-light BRDF upgrade should stay within the current material buffer and shader path instead of introducing a separate PBR framework. `VoxelMaterialVisual` therefore now packs `AO/roughness/metallic/reflectance` plus transmission tint and fog/emissive/ambient/direct-response hooks in the same 64-byte table, and the main voxel shader consumes that contract with a `GGX + Fresnel-Schlick + Smith` sun-light baseline while still honoring authored ambient/diffuse response weights inside the existing ambient gradient, fog and shadow integration path.
- Shadow depth pass consumes a dedicated all-occluder opaque indirect buffer instead of the main camera-culling visibility commands; main voxel pass потом семплирует shadow map только для direct sun.
- Fake frame-only glass shadows are rejected for mainline because they read as noisy geometry, not as glass. Keep glass
  ignored until there is a real transparent-shadow design, but do not suppress `Fluid` shadows.
- The first contact-shadow follow-up should also stay inside the existing forward voxel path instead of adding a second
  shadow pass or a fake screen-space surrogate immediately. The current bounded baseline therefore binds the same chunk
  descriptors + packed voxel payload in `voxel.frag`, traces a short voxel DDA ray toward the sun, and exposes only an
  explicit `sunContactShadowParams={strength,maxDistance}` contract plus `CTSH` debug visibility.
- The first ambient-occlusion follow-up follows the same bounded rule. It is a forward voxel-space `AOCC` layer driven
  by explicit `ambientOcclusionParams={strength,radius,minVisibility}`, not a claim that mainline already has full
  screen-space `SSAO/GTAO`. Keep it low-strength, short-radius, and distance-faded until a real depth/normal
  screen-space pipeline exists.
- The first local-light step should still stay bounded before real local shadow maps. Current mainline therefore
  supports one per-preset inverse-square point light in `VoxelSceneLighting`, evaluates it through the same GGX
  direct-light helper as the sun, and now gates it with a short opaque-only voxel DDA visibility term driven by
  `localPointLightParams={enabled,sourceRadius,shadowStrength,shadowBias}`. Spot shadow maps, point-light cubemaps,
  and local-light culling remain separate follow-ups, not hidden inside this contract step.
- That bounded local-light DDA must stay stable per voxel face too: the visibility ray is anchored to a stable
  point on the owning voxel face before bias is applied, instead of starting from the interpolated fragment position.
  Do not collapse that to one constant face-center sample: it fixes one defect but creates visible per-voxel shadow
  bucketing on large flat receivers. On top of that, close-range visibility must not stay a single hard ray to the
  emitter center either: partially occluded faces produce visible binary speckle. Current accepted bounded fix is a
  tiny emitter-disk average around the authored `sourceRadius`, still inside the same forward voxel DDA path.
- The current local-light transparent policy is stricter than the sun/contact baseline on purpose: `Glass` and `Fluid`
  are both ignored as local-light shadow occluders until there is a separately scoped transmission/tinted-shadow path.
- `PROJECTV_START_CAMERA_POSITION/LOOK` are no longer startup-only in practice. For reproducible look-dev/snapshot
  repros, camera overrides must also survive world reload paths (`F7`, replay snapshot load, preset reload), so
  `FinalizeActiveVoxelWorldReload` reapplies them before snapping the active control-mode character state.
- Until shared shader includes exist for lighting state, every `SceneLightingBuffer` declaration must stay byte-identical
  across `voxel.frag`, `voxel_shadow.vert`, and `voxel_mesh.comp` whenever `VoxelSceneLighting` changes. Breaking that
  contract is not a cosmetic bug: the shadow pass starts sampling shifted cascade matrices immediately.
- Contact shadows follow the same transparent policy as the current mainline sun-shadow path: `Glass` stays ignored as a
  local occluder, while `Fluid` remains a valid contact-shadow occluder.
- Local voxel AO follows the same local transparent policy for now: `Glass` stays ignored, `Fluid` remains an occluder,
  and broad transparent/tinted occlusion is deferred to a separate future path instead of faking glass shadows here.
- Sun/contact/AO/local-light changes are not considered validated from build/tests or screenshot sidecars alone anymore.
  The close-out artifact must include inspected runtime frames for `FINAL` plus the relevant debug views (`SHDW`, `CSM`,
  `CTSH`, `AOCC`, `LOCL` when applicable), because the contact-shadow landing already produced a passing metadata path
  while the actual `VoxelLab` shadow frame was nearly white.
- Первый quality/debug follow-up для этого path тоже остаётся прагматичным: baseline shadow map держится на `2048x2048`, main voxel shader использует weighted `5x5` PCF, а shadow tuning/debug живёт внутри уже существующего lighting loop (`B` debug views + detailed HUD), а не в отдельном editor/debug framework.
- Cascades must not be smuggled into the shader as hidden constants: split depths and lambda are runtime-visible state
  before the renderer starts sampling multiple shadow maps.
- Cascade receiver planning must stay aligned with the actual visible-scene contract too. In current mainline, chunk
  visibility already caps receiver distance to `min(camera.farPlane, 64)`, so CSM split planning must use that same
  receiver max distance instead of the raw camera far plane; otherwise near cascades waste texel budget on receivers
  that scene culling never draws.
- The current mainline default split distribution is intentionally more near-biased than the original first CSM hookup:
  live `MeshingStress` repro showed that `0.65` kept too much quality in far receivers, so the default lambda is now
  `0.80` until a better data-backed scheme or more cascades replaces it.
- CSM stabilization belongs in the CPU projection contract first: small camera movement below one shadow texel should
  not continuously slide the cascade projection across the world.
- CSM quality follow-up should stay measurement-first too: before changing cascade culling, blend policy, or heavier
  filtering, the runtime must expose per-cascade coverage data in the same HUD/capture loop that artists already use.
- Once that data exists, prefer deterministic CPU-fit improvements first. The current chosen follow-up is sphere-fit
  cascade extents because it reduces camera-rotation-driven extent churn without adding another shader-side feature.
- Once split transition blending is introduced, keep it in the same explicit contract too: the shader may blend current
  and next cascades near a split, but the blend width must stay runtime-visible/tunable (`BLD` in HUD, sidecar
  metadata), not another hidden hardcoded threshold.
- Once caster coverage tuning is introduced, keep that explicit too: per-cascade diagnostics must expose the resulting
  caster light-depth ranges, so follow-up tuning compares real capture numbers rather than another invisible CPU-fit
  heuristic.
- Caster coverage must influence more than cascade light-depth. If a nearer cascade only expands `Z` for upstream casters
  but keeps `XY` from the receiver slice alone, tall/upstream casters can disappear exactly at cascade transitions. The
  current baseline therefore lets caster coverage expand light-space `XY` extents too.
- Expanded caster coverage must also stay in front of the cascade shadow near plane. If the light camera stays anchored
  only to the receiver sphere after caster coverage grows upstream, mid/far cascades can still clip those casters before
  the shadow map is sampled. The current baseline therefore also moves the cascade light camera upstream enough to keep
  the expanded caster range positive in light depth.
- The first real post-fit culling step for CSM is chunk-level per-cascade draw culling, not another projection tweak:
  the shadow indirect buffer now carries one draw-command slice per cascade, CPU visibility tests chunk AABBs against the
  current cascade clip volumes, and dirty-chunk meshing patches those same per-cascade commands on the GPU for current-frame correctness.
- Empty-cascade draw skipping is acceptable only when it is deterministic for the current frame. The current bounded
  policy therefore skips a cascade shadow draw only when CPU culling reports zero casters and there is no dirty meshing
  work that could still patch shadow commands later in the frame.

Почему:

- Текущие built-in demo scenes конечные и компактные, поэтому scene-wide orthographic projection даёт дешёвый и понятный первый baseline без раннего ухода в R&D.
- The shadow pass still stays intentionally simple, but it must have its own opaque occluder command source; reusing camera-visible indirect draws is not acceptable because it makes shadow presence depend on the current view frustum.
- Так shadow slice остаётся совместимым с нынешним explicit CPU scene contract и dynamic-rendering path, а следующий шаг — тюнинг bias/stability/debug, а не новый lighting framework.
- Так текущий shadow slice становится достаточно читаемым и настраиваемым для mainline look-dev без раннего перехода к cascades, render graph или отдельному tooling stack.

## 16. Legacy docs structure

Решение:

- Legacy documentation now lives in one unified `legacy/docs` tree; parallel `latest` and `old` roots are retired.
- Active reference material belongs under `legacy/docs/philosophy`, `legacy/docs/standards`, and `legacy/docs/libraries`.
- `legacy/docs/libraries` should preserve the full useful per-library corpus inside that unified tree. Canonical entry points may come from the newer `01_reference.md` / `02_integration.md` docs, but deeper topical files should be removed only after a content-level merge proves they are redundant.
- Older learning/support sections such as `guides/`, `tutorials/`, and text-based `examples/` should live in the same unified tree rather than being discarded just because they are not part of the stricter standards/reference layer.
- `legacy/docs/architecture` keeps design material, but documents there should carry explicit status (`reference`, `historical`, `speculative`) instead of silently mixing active guidance with archival notes.
- Historical plans and cleanup artefacts belong under `legacy/docs/archive/`, currently `legacy/docs/archive/roadmaps/`, rather than competing with active reference roots.
- Project-facing links should target only unified `legacy/docs/...` paths (`AGENTS.md`, `agent/session-checklist.md`, future docs cross-links).

Почему:

- Parallel `latest` / `old` trees were creating duplicated content, contradictory status, and broken navigation for the same topics.
- The previous two-file library reduction destroyed too much useful material; for this repo, careful curation has to prefer completeness until duplicate sections are proven safely mergeable.
- A single tree with explicit status labels keeps the legacy corpus readable and searchable without letting historical planning documents masquerade as current project guidance.

## 17. Multiplatform baseline (Linux-port инициализация `2026-06-09`)

Решение:

- `ProjectV` теперь expected to build and run on both `windows-clang-debug` (existing) and `linux-clang-debug` (new). Arch Linux — active Linux dev host. Linux toolchain — **clang 22.1.6 native (not clang-cl) + lld 22.1.6 + libstdc++ 16.1.1 + SDL3 3.4.10 + Vulkan 1.4.350**.
- `linux-clang-debug` preset mirrors `windows-clang-debug` shape (BUILD_TESTING=ON, Debug, Tracy instrumentation, validation=ON), но pins native clang, `CMAKE_LINKER_TYPE=LLD`, and explicitly does **not** set clang-cl-only variables (`/W4 /WX /permissive- /utf-8 /EHsc /D_CRT_SECURE_NO_WARNINGS`).
- Windows presets are untouched.
- `CMakeLists.txt` gates Windows-specific options за `if (MSVC) ... endif()`: `/W4 /WX /permissive- /utf-8` и `NOMINMAX` теперь только для MSVC. `VK_NO_PROTOTYPES` остаётся глобально (volk требует). `VOLK_STATIC_DEFINES` теперь platform-gated: `WIN32 -> VK_USE_PLATFORM_WIN32_KHR`, `APPLE -> VK_USE_PLATFORM_MACOS_MVK`, `ANDROID -> VK_USE_PLATFORM_ANDROID_KHR`, иначе `VK_USE_PLATFORM_XCB_KHR`.
- Non-MSVC `else ()` branch добавляет два INTERFACE options:
  - `-Wno-deprecated-declarations` — libstdc++ 16 пометил `std::is_trivial` deprecated, `external/flecs 2.2.0` (lines 66, 93) его ещё использует. Это `flecs` upstream lag, не project bug.
  - `-include cstring` — legacy `std::memcpy` / `std::memset` / `std::strcmp` calls без explicit `<cstring>` include. MSVC transitive include через `<cstdint>`/`<cstdlib>`, libstdc++ нет.
- `src/CMakeLists.txt` — `GPUOpen::VulkanMemoryAllocator` uncommented in `ProjectV` link block. Причина: на Windows-CLion `ProjectV` собирался без explicit VMA link, и `#include "vma/vk_mem_alloc.h"` резолвился через Vulkan SDK (Windows-SDK layout: `vma/vk_mem_alloc.h` под `C:\VulkanSDK\...\include\`). На Linux Vulkan SDK нет; единственный путь — `external/VulkanMemoryAllocator/include/vk_mem_alloc.h` (submodule layout, no `vma/` subdir на pinned SHA `b3cbbb43`). Uncomment делает обе платформы consistent через submodule copy.
- `src/core/Types.hpp` — `#include "vma/vk_mem_alloc.h"` → `#include "vk_mem_alloc.h"` под pinned submodule-VMA layout. Header резолвится на обеих платформах через submodule.
- `src/ecs/EcsWorld.hpp` — `#include <cstddef>` добавлен перед `<cstdint>`. На libstdc++ 16 `size_t` не transitively тянется из `<cstdint>`. MSVC transitive включает — Windows не affected.
- `src/render/SceneResources.cpp` — `#include <cstring>` добавлен для симметрии (covered и глобальным `-include cstring`, но local include чище и позволяет MSVC keep current transitive story).

Почему:

- Mainline — reproducible interactive voxel MVP. `AGENTS.md` §4 не запрещает multiplatform dev setup, и «использовать такие технологии, что можно себе не только ногу отстрелить» из user intent означает native Linux toolchain, а не «выкинь Windows». Мультиплатформенность — это второй dev-контур, **не** поджигание мостов с Windows.
- `AGENTS.md` §3 требует sync `agent/` после заметной работы — Linux-факты идут в `agent/memory.md` (долговечный context) и `agent/status.md` (сжатый snapshot), roadmap follow-up — в `TODO.md`.
- Submodule-VMA `b3cbbb43` уже 8+ месяцев без обновления в репо. На текущей upstream `v3.4.0` header остаётся `include/vk_mem_alloc.h` (не `vma/vk_mem_alloc.h`). `vma/vk_mem_alloc.h` — Windows-Vulkan-SDK layout. Поправка include path — минимальное вмешательство, фиксит обе платформы. Upstream submodule bump — отдельный follow-up.
- Build options `if (MSVC) ... endif()` + Linux `else()` branch — кросс-платформенный contract. На Windows ничего не меняется (MSVC истинен); на Linux clang-native flags применяются корректно.
