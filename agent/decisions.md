# Decisions

Живые инженерные договорённости. Roadmap живёт в `TODO.md`, общий протокол — в `AGENTS.md`.

Дата обновления: `2026-06-12` (+§19 + §20 TAA contracts + §26 frame-step/slow-motion + §27 per-pass timings + §28 audio engine)

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
- **Per-vertex ambient occlusion is disabled (`2026-06-10`, P0.3 follow-up v2).** The earlier 3-neighbor
  (Lysenko), 8-surrounding and 4-axis-aligned variants all produced a visible
  "pseudo-shadow" on the 3D-угол of a 2x2x2 cube (or any 4-voxel junction) because
  the count of solid axis-aligned neighbors peaks at convex corners with three
  abutting voxels (3 of 4 = AO 64 = 25% lit), even though sky is visible from
  the outward diagonal direction. A face-independent model cannot distinguish
  "concave" from "convex" from a single neighbor count, so any per-corner AO
  will always have a discrete darkening at cube-corner junctions of a 2x2x2
  mass. Mainline now writes `outAmbientVisibility = 1.0` in `voxel.vert` and
  the AOCC term (`ComputeAmbientOcclusionVisibility` in `voxel.frag`) supplies
  all per-pixel cavity darkening, which has no face-boundary seams. Re-introducing
  per-vertex AO requires a per-face uniform AO (compute-shader-baked) or a real
  weld/duplication-aware welded mesh; both are deferred to a future R&D pass.
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
- The P0.3 per-corner AO contract is face-corner dependent on purpose, but the rasterizer's face-boundary
  interpolation now runs into a discrete step at every 3D-угол because 3 different faces touching the same
  corner each store their own per-(face, corner) AO. A full GPU-hash-table mesh welding (welded vertex /
  index buffers driven by `voxel_mesh.comp`, `vkCmdBindIndexBuffer`, `VkDrawIndexedIndirectCommand`, vertex
  input state in the graphics pipeline) would re-merge them at the 3D-position level, but the change is
  large enough to dominate a single session. The pragmatic equivalent chosen for mainline is **face-independent
  AO computed in the vertex shader from the eight voxels surrounding the integer 3D corner position**.
  `voxel.vert` therefore binds `PackedChunkVoxelPayload` at descriptor-set binding 5 in the vertex stage as well
  as the fragment stage, and the graphics descriptor set layout must list `VERTEX_BIT | FRAGMENT_BIT` for that
  binding (VUID-VkGraphicsPipelineCreateInfo-layout-07988 otherwise). The 3-neighbor per-face-corner algorithm
  (Mikola Lysenko, *Ambient occlusion for Minecraft-like worlds - 0 FPS*) is preserved as a reference helper
  in `voxel_mesh.comp::ComputeFaceCornerAmbientLevel` for a possible revert, but the mainline renderer no longer
  reads `PackedFace::lightingData` for AO. Per-corner interpolation inside a face is preserved because the four
  corners of one face are four different 3D positions, each with its own 4-axis-aligned AO (the 4
  face-sharing neighbors at the 3D-угол, excluding the 4 diagonal octants). The first pass
  (8-surrounding) produced a 50% dark spot at every 4-voxel junction, which the 4-axis-aligned
  variant removes. Until a real welding
  path lands, this is the agreed contract for new face-vertex AO work.
  - While the per-corner AO is face-independent, the *material* at a welded 3D-угол is still per-face (a
  voxel-emitted face picks one `materialIndex` from its own PackedFace, not from a shared vertex). If a future
  welding pass needs to merge materials, it must pick a deterministic rule (e.g. take the owning voxel's
  material at that 3D-угол via `ReadVertexNeighborMaterial`); do not silently average, because Glass and
  Opaque read very differently in `voxel.frag` and a blended value would give neither.
- The `InputState::skipFirstMouseMotion` flag exists because `SDL_SetWindowRelativeMouseMode(true)` does
  not reset the cursor position: the first `SDL_EVENT_MOUSE_MOTION` after enabling it carries a delta from
  the unrestrained pre-capture position, which yanks the camera look on launch (typically pitching it
  sharply to the floor in walk / creative / spectator modes). The flag is defaulted to true in
  `InputState` so the first motion on launch is dropped, and `SetRelativeMouseMode` resets it on every
  (re-)enable so tab-toggle in-flight also gets a clean first frame.
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

- Mainline — reproducible interactive voxel MVP. `AGENTS.md` §2 (Project metadata, platforms) explicitly enumerates Windows + Linux dev trees and does not forbid multiplatform, и «использовать такие технологии, что можно себе не только ногу отстрелить» из user intent означает native Linux toolchain, а не «выкинь Windows». Мультиплатформенность — это второй dev-контур, **не** поджигание мостов с Windows.
- `AGENTS.md` §7.4 (Synchronization) requires sync `agent/` после заметной работы — Linux-факты идут в `agent/memory.md` (долговечный context) и `agent/status.md` (сжатый snapshot), roadmap follow-up — в `TODO.md`.
- Submodule-VMA `b3cbbb43` уже 8+ месяцев без обновления в репо. На текущей upstream `v3.4.0` header остаётся `include/vk_mem_alloc.h` (не `vma/vk_mem_alloc.h`). `vma/vk_mem_alloc.h` — Windows-Vulkan-SDK layout. Поправка include path — минимальное вмешательство, фиксит обе платформы. Upstream submodule bump — отдельный follow-up.
- Build options `if (MSVC) ... endif()` + Linux `else()` branch — кросс-платформенный contract. На Windows ничего не меняется (MSVC истинен); на Linux clang-native flags применяются корректно.

## 18. TAA contract (`2026-06-12`)

Решение:

- **Default `taaEnabled=true`.** Live visual TAA — основной путь рендеринга, не opt-in. Anti-jitter baseline из `ee82c6f` это устанавливает; см. `agent/memory.md` §10.14 для предыстории.
- **TAA on/off variants в SPIR-V, не runtime branches.** `voxel.frag` компилируется в `voxel.frag.spv` (TAA-off, `outColor` Location 0) и `voxel.frag.taa_on.spv` (TAA-on, `outSceneColor` Location 1). Validation layer больше не видит неиспользуемый output — переменная физически отсутствует в SPIR-V. `02c297c` починил это; `b0fcd9b` — оригинальная per-frame specialization (предшественник, deferred).
- **Tuning ladder: live runtime knobs, no preset file.** 5 hotkeys в `;`/`'`/`-`/`=`/`,`/`.` (см. `agent/memory.md` §10.16). Default values — `taaBlend=0.10`, `taaJitterScale=1.0`, `taaNeighbourhoodRadius=1` (3×3), `taaHistoryValid=false` until second frame. Любой change инвалидирует history (`taaHistoryValid=false`) на следующий кадр.
- **History invalidation triggers (6 событий):**
  1. Swapchain resize (`VulkanSwapchain.cpp::CreateOrRecreateSwapchain`).
  2. World reload через `FinalizeActiveVoxelWorldReload` (`main.cpp`).
  3. `T` toggle (TAA on↔off).
  4. `taaJitterScale` change (live `;`/`'`).
  5. `taaBlend` change (live `-`/`=`).
  6. `taaNeighbourhoodRadius` change (live `,` cycle).
  7. `.` history-invalidate single press.
- **Не invalidate:** pause toggle (нет изменения геометрии), voxel edit (sub-frame изменение, TAA depth-reproject handles), camera movement (motion vectors — основная задача TAA).
- **`taaClampColorSpace = YCoCg` (vs RGB).** Y/Co/Cg lossless reversible transform, 1-tap bright пиксель двигает только Y. RGB clamp либо дискардил highlight, либо вымывал chroma. `a2972fa` + см. `agent/memory.md` §10.16.
- **Neighbourhood radius = 1/3/5/7.** Не `1/2/3/4` — radius симметричный, `radius=1` = 3×3, `radius=3` = 7×7, etc. Shader snap'ит in-between к нижнему valid odd. `taaHistoryParams.w` slot раньше был `reserved` (byte layout не изменился, semantic только).
- **Per-frame `taaParams` field layout:** `(jitterX, jitterY, blend, enabled)`. Blended как `0.0` when `taaEnabled=false`, иначе `taaBlend`. Enabled = `1.0/0.0`. Packed в `vec4` SSBO, контракт с `voxel.frag`/`voxel_shadow.vert`/`voxel_mesh.comp`/`taa_resolve.frag` byte-exact (см. `static_assert` в `VoxelMaterials.hpp:125-145`).
- **Per-frame `taaHistoryParams` field layout:** `(texelSizeX, texelSizeY, historyValid, neighbourhoodRadius)`. Texel sizes = 0 на `RefreshSceneLightingBuffer` (CPU не знает swapchain extent), `FramePreparation::UploadSceneFrameResources` патчит позже. `historyValid` = 0 invalidate.
- **`PROJECTV_ENABLE_RENDERDOC_MARKERS` CMake option (Debug default ON, `linux-clang-debug` preset OFF).** Gated compile-time; `PV_PROFILE_GPU_LABEL`/`PV_PROFILE_GPU_LABEL_COLOR` macros no-op когда OFF. Function pointers грузятся volk'ом (extension `VK_EXT_debug_utils` always enabled). Future pass'ы: добавить 2 строки в start of function body.

Почему:

- TAA on by default потому что anti-jitter — базовая UX проблема (perceived camera shake on every frame), TAA — единственный cheap fix. Не делать это opt-in — значит заставлять пользователя нажимать `T` на каждом запуске. Per-frame `1/60s` jitter ring buffer с `TaaParams` (8-tap Halton 2,3) — bounded cost.
- SPIR-V variants вместо `if (taaEnabled) { ... }` branches: branches добавляют uniform-dependent divergence на hot path. Variant SPIR-V (compile-time constant) — zero-cost. Pre-`02c297c` validation layer ругался на `outSceneColor unused` в TAA-off frame; 2 SPIR-V файла — это physical fix, не warning suppress.
- Live tuning ladder (not preset file) потому что TAA параметры должны tuning'иться per-scene. YCoCg clamp + neighbourhood radius + blend — не «save the preset» параметры, а runtime knobs. HUD + sidecar capture metadata — recordable, reproducible.
- YCoCg над RGB clamp потому что highlights самая проблемная зона для RGB clamp (sample variance огромная), а luma/chroma split даёт physically meaningful separation. MJP notes + Yang GPU Gems 3 reference.
- Neighbourhood radius как 1/3/5/7 (не 1/2/3/4) потому что radius symmetric about center pixel: `[-r, +r]` итого `2r+1` taps. 1 = 3×3 (original), 3 = 7×7, etc. Shader на GLSL не умеет dynamic loop bounds; snap к odd values держит shader simple.

Cross-refs: `agent/memory.md` §10.12–§10.16 (full timeline). TODO §5 Блок 1 (1.1, 1.4 closed; 1.2, 1.3, 1.5, 1.6, 1.7, 1.8 in progress / R&D).

## 19. TAA camera-cut + CAS contract (`2026-06-12`)

Решение:

- **Camera-cut detection** (1.2). Chebyshev (L-infinity, max-abs over
  16 floats) distance между `taaPrevViewProjectionMatrix` (frame N-1)
  и `frame->graphicsPushConstants.viewProjection` (frame N) проверяется
  в `FramePreparation::BuildFrameData` после `AdvanceTaaPixelJitter` и
  **до** `taaPrevViewProjectionMatrix` stash. Если delta > `0.10f` (10%
  per-element), то `taaHistoryValid = false` + `taaCameraCutCount++`.
  - 7-й history-invalidation trigger (поверх swapchain resize / world
    reload / Taa toggle / jitter scale / blend / neighbourhood radius /
    `.` invalidate). Per-frame `taaCameraCutMaxDelta` accumulating worst
    case since startup.
  - **First-frame guard через `taaPrevViewProjectionMatrixInitialized`
    bool, не `taaFrameCounter > N` heuristic.** `taaPrevViewProjectionMatrix`
    zero-initialised, naive detector регистрирует `maxDelta ≈ |viewProj|max`
    на frame 0 / post-swapchain-recreate. Companion bool ставится на
    first stash, ресетится в `VulkanSwapchain.cpp::CreateOrRecreateSwapchain`
    рядом с `taaPrevViewProjectionMatrix = {}`. Frame-counter heuristic
    сломался бы, если counter reset'ится по другой причине (separate concern).
- **Inline CAS post-TAA** (1.3). AMD FidelityFX CAS (Bartłomiej Wronski,
  GPUOpen 2020) integrated в `taa_resolve.frag` single-pass. High-pass
  `center - 4-corner-avg`, weight `clamp(highPass / (max - min), 0, 1)`,
  output `clamp(color + highPass * (sharpenAmount * weight), min, max)`.
  - **No extra texture lookups.** Existing `2r+1 × 2r+1` neighbourhood
    loop в `GetSceneColorRange` extended: `rgbMin` / `rgbMax` (5-tap
    cross+center), `rgbCornerSum` (4-tap corners). Bandwidth-negligible.
  - **`sharpenAmount = max(0, (1.0 - taaBlend) * taaCasSharpnessMax)`**
    derived in-shader. High blend (stable) -> less sharpening, low
    blend (noisy) -> more. TAA-off falls through with `taaBlend = 0`,
    `sharpenAmount = taaCasSharpnessMax` (ceiling alone).
  - **Linear-light pre-tonemap.** CAS reads pre-tonemap scene, applying
    sRGB-space high-pass даёт wrong gamma curve и ломает "clamp to
    local range" overshoot guard. TAA-resolve sequence теперь:
    `clampedCurrent / clampedHistory` -> `linearOut` -> `*= exposure`
    -> **`ApplyCasLinear(linearOut, rgbMin, rgbMax, rgbCornerSum, sharpenAmount)`**
    -> `ApplyTaaToneMap` -> `ApplyTaaColorGrading` -> `outColor`.
  - **Push constant byte layout unchanged.** `ResolvePushConstants`
    заменил `vec2 reservedPadding` (8 B) на `float taaBlend; float
    taaCasSharpnessMax;` (8 B). Total 144 B preserved. `static_assert`
    в `core/Types.hpp:212-218` обновлён: `offsetof(..., taaBlend) ==
    136`, `...taaCasSharpnessMax == 140`. GLSL `pushConstants` block
    в `taa_resolve.frag` mirror-обновлён.
- **Default `taaCasSharpnessMax = 0.5f`.** При default `taaBlend = 0.10`,
  effective sharpening = `(1 - 0.10) * 0.5 = 0.45`. AMD CAS reference
  target for stable post-TAA output.
- **`taaCasSharpnessMax = 0.0f` отключает CAS step** (`ApplyCasLinear`
  short-circuits на `sharpenAmount <= 0.0`), не требует shader branch.
- **Inline CAS, не отдельный pipeline.** Альтернатива — отдельный
  `cas.frag` + 7-й graphics pipeline + descriptor set + render pass
  slot + третий fullscreen draw per frame. Trade-off: `taa_resolve.frag`
  теперь делает TAA + CAS, но bandwidth-neutral (existing loop) и
  без swapchain readback (CAS читает pre-tonemap linear). Plus: не
  трогает `VulkanGraphicsPipeline.cpp` (shared с asset-pipeline M4).

Почему:

- Camera-cut detection потому что motion vectors (depth-reconstructed
  или нет) не могут sensibly reproject history если viewpoint changed
  beyond ~0.10 element-wise delta. Без detector: ghost trails на
  teleport / snap rotation / preset switch. Threshold 0.10 — single
  constant (не live hotkey) потому что operator data clean separates
  "ordinary motion" от "intentional cut" без per-session dial.
- Inline CAS потому что (a) reuse the 3×3/5×5/7×7 neighbourhood loop
  (bandwidth-free), (b) linear-light contract simple (no extra
  swapchain readback), (c) не трогает shared `VulkanGraphicsPipeline.cpp`
  (asset-pipeline territory per AGENTS.md §7.2.6). Trade-off: один shader
  делает две вещи, но bounded (1 shader, 1 push-constant expansion, 0
  new pipelines).
- Linear-light CAS (не sRGB) потому что AMD reference CAS работает в
  display-referred space, а наш resolve pass — linear -> tonemap. Применение
  sRGB-space high-pass на linear data даст wrong gamma curve.
- First-frame guard через bool (не frame-counter) потому что bool — single
  concern (init state), counter — orthogonal concern (Halton sequence).
  Bool resets в одном месте (swapchain recreate); counter может reset'иться
  по разным причинам (separate policy).

Cross-refs: `agent/memory.md` §10.17 (full timeline), `TODO.md` Блок 1
(1.2 + 1.3 closed in this session).

## 20. TAA scene color format (`2026-06-12`)

Решение:

- **`taaSceneColorTarget` + `taaHistoryColorTarget` формат = `VK_FORMAT_B10G11R11_UFLOAT_PACK32` (4 B/pixel).** Раньше — `VK_FORMAT_R16G16B16A16_SFLOAT` (8 B/pixel). 2× bandwidth save на resolve-pass read и history copy. Single source of truth — `inline constexpr VkFormat kTaaSceneColorFormat` в `projectv::taa` namespace (`src/render/TaaRenderTargets.hpp`).
- **Two consumers, one constant:** `CreateOrRecreateTaaRenderTargets` (image allocation) и `VulkanGraphicsPipeline::CreateGraphicsPipeline` (`pColorAttachmentFormats[1]` declaration) оба consume `kTaaSceneColorFormat`. Format cannot drift.
- **Shader code не трогаем.** `voxel.frag` и `model.frag.taa_on.spv` пишут `vec4 outSceneColor` (Location 1), `taa_resolve.frag` читает `texture(historyColor, ...).rgb` — Vulkan spec: alpha of packed formats is undefined on store, but resolve only consumes `.rgb`, dropped alpha is no-op. Resolve output пишет в swapchain (B8G8R8A8 UNORM) — format transition transparent.
- **`vkCmdCopyImage` format compatibility:** src и dst оба `kTaaSceneColorFormat`, identical → spec §7.1.1 satisfied.

Почему:

- **Bandwidth wins** на resolve-pass read + per-frame history update — это 2 из 3 самых горячих transfer paths в TAA pipeline.
- **5/6/5 bits per channel + 5-bit shared exponent** — узкий dynamic range, но matches HDR linear color после tone-map. Dim areas (< 0.1% intensity) могут banding'ить — fallback revert к R16G16B16A16 = 1-line change.
- **R11G11B10_UFLOAT, не R10G10B10A2_UNORM:** linear HDR + RGB-only. A2UNORM тратит 2 bits на unused alpha; UFLOAT matches our tone-map output.
- **B10G11R11, не R11G11B11:** standard "Vulkan R11G11B10" name. `B10G11R11_UFLOAT_PACK32` = R in low bits, B in high bits. Same memory layout, just a naming convention.
- **Single-source-of-truth constant** (не magic literal × 2): pattern из §18/§19 (push-constant byte layout invariance). Inline constexpr + 2 consumers = compiler-enforced consistency.

Cross-refs: `agent/memory.md` §10.18 (full timeline), `TODO.md` Блок 1
(1.7 closed in this session).

## 21. TAA per-layer history contract (`2026-06-12`)

Решение:

- **3-й MRT attachment на voxel pass пишет packed `vec4 outLayerMask` (Location 2, R = CTSH sun contact shadow visibility, G = AOCC cavity occlusion, B = LOCL local-point-light visibility, A = 1.0).** Формат — `VK_FORMAT_R8G8B8A8_UNORM` (4 B/pixel). Per-frame `vkCmdCopyImage` копирует `taaLayerSceneColorTarget` → `taaLayerHistoryColorTarget`. Fragment shader сэмплит `sampler2D layerHistory` (binding 6, graphics descriptor set) и применяет `mix(rawCurrent, history, blend=0.4)` к AOCC + LOCL в main lighting. CTSH пишется в history, но **не blended** в main lighting (deferred — `ComputeSunShadowSample` refactor needed для separation cascade shadow от contact shadow).
- **Single source of truth: `inline constexpr VkFormat kTaaLayerHistoryColorFormat = VK_FORMAT_R8G8B8A8_UNORM` в `projectv::taa` namespace** (`src/render/TaaRenderTargets.hpp`). Consumed by `CreateOrRecreateTaaRenderTargets` (image allocation для обоих layer scene color + layer history) и `VulkanGraphicsPipeline::CreateGraphicsPipeline` (`pColorAttachmentFormats[2]` declaration). 2 consumer'а не могут дрифтнуть.
- **Blend-at-read, not blend-at-write.** `output = mix(raw, history, blend)`. Uniform contribution per frame, нет exponential-decay artefacts. Alternative — blend-at-write (`history = mix(raw, history, blend)`) — даёт geometric decay old samples (history → 0 при sustained motion), wrong weighting.
- **Pack все 3 layer values в 1 `vec4`** чтобы уложиться в component budget RTX 3060 = 8 vec4 outputs per fragment (`maxFragmentOutputComponents`). TAA-off: `outColor 4 + outLayerMask 4 = 8`. TAA-on: `outSceneColor 4 + outLayerMask 4 = 8`. 3-й attachment slot bound в обоих path'ах, но per-frame `VkRenderingAttachmentInfo::imageView` = `VK_NULL_HANDLE` на unused slot — `dynamicRenderingUnusedAttachments` allows.
- **`VoxelSceneLighting::taaLayerHistoryParams` vec4** (texelX, texelY, neighbourhoodRadius, blendFactor) packed в существующий SSBO на offset 608 (после `taaHistoryParams` 16 B + 16 B padding), total struct 624 B (rounded up from 616 → multiple of 16 B per std430 layout rules). `static_assert(sizeof(VoxelSceneLighting) == 624)` + `static_assert(offsetof(VoxelSceneLighting, taaLayerHistoryParams) == 608)` enforces byte layout invariance. Mirrors в `voxel_mesh.comp` + `voxel_shadow.vert` shader side.
- **Layer history invalidation привязан к 6 existing TAA triggers** (Taa toggle, jitter scale change, blend change, neighbourhood radius change, `.` invalidate, Taa toggle duplicate). New fields: `RenderState::taaLayerHistoryValid` (bool), `taaLayerBlendFactor` (float, [0,1], default 0.4), `taaLayerSceneColorCurrentLayout` + `taaLayerHistoryColorCurrentLayout` (VkImageLayout trackers). Reset в `VulkanSwapchain.cpp::CreateOrRecreateSwapchain` paired with existing TAA resets.

Почему:

- **Component budget constraint — единственный driver для packed `vec4`.** 3 отдельных attachments дали бы 12 components (exceed budget 8). 1 attachment с 3 components даёт 7 components, leaves room для других outputs. Packing 3 layer values в 1 `vec4` — практически forced.
- **Blend-at-read > blend-at-write** для GPU-computed values (lighting, AO, shadows). Light intensity history, contact shadow history, AO history — все это per-frame sample of a **frame-to-frame correlated** signal, не exponential-smoothed. Geometric mean weighting (blend-at-read) matches signal statistics; exponential decay (blend-at-write) assumes AR(1) which doesn't hold for sudden viewpoint changes.
- **CTSH deferred** — `ComputeSunShadowSample` объединяет cascade shadow (viewpoint-dependent) и contact shadow (viewpoint-independent) в single value. Blending combined value with history reprojected from previous frame = wrong direction in cascade transition zones (cascade shadow "ghosts" because history reprojection thinks the contact shadow should follow the old viewpoint, but contact shadow doesn't follow viewpoint at all). Skip blend для CTSH пока правильно — visual artefact > flicker в этом specific layer.
- **Format = `R8G8B8A8_UNORM`** — все 3 layer values — **выходы lighting equations, clamped [0,1]**. AOCC inherently [0,1] (1.0 = no occlusion). LOCL inherently [0,1] (1.0 = fully lit by point lights). CTSH inherently [0,1] (1.0 = full contact shadow). Half-float wasted bits. Bandwidth-efficient.
- **`R8G8B8A8_UNORM` не `R10G10B10A2_UNORM`** — A2 wastes 2 bits on alpha, мы используем alpha = 1.0 constant. R8G8B8A8 has full 8 bits per channel для 3 layer values.
- **Single source of truth constant** (не magic literal × 2) — pattern из §18/§19/§20. Inline constexpr + 2 consumers = compiler-enforced consistency.
- **Layer history `initialLayout = UNDEFINED`** (VUID-VkImageCreateInfo-initialLayout-00993). Pre-fix имел `SHADER_READ_ONLY_OPTIMAL` — forbidden. First-frame per-frame transition в `Renderer.cpp` is the only way to get image into read layout. Same fix as 1.7's `taaSceneColorTarget` / `taaHistoryColorTarget` initialLayout (те же VUID, те же fixes в `4d8b4c8`).
- **Pipeline-declared `pColorBlendState->attachmentCount` = 3** (VUID-VkGraphicsPipelineCreateInfo-renderPass-06055). Pre-fix имел 2. Validation layer would fire на pipeline creation; без validation driver silently dropped write.
- **Graphics descriptor pool `combinedSamplers` = 4 = 2 frames × 2 samplers** (binding 5 shadow + binding 6 layer history). Pre-fix имел 2 = 2 frames × 1 sampler. `VUID-VkDescriptorPool-size-...` triggers when descriptor sets can't be allocated.

Cross-refs: `agent/memory.md` §10.21 (full timeline + build/test/smoke + working rules), `TODO.md` Блок 1 (1.5 closed), `agent/status.md` §12 (in-progress session snapshot), `agent/active-sessions.md` session-2026-06-12-taa-quality-1.5 (closed).

## 22. Two-level chunk visibility cache (`2026-06-12`)

Решение:

- **Cache lives on `RenderState::chunkVisibilityCache`** (single, not per-frame). Cached `VkDrawIndirectCommand` arrays are frame-independent because both `sceneFrameResources[0]` and `[1]` get the same `memcpy`'d commands from this single cache on a hit. Frame-independence holds because the per-frame GPU mapped memory is just a write-only destination.
- **Hash input:** 6 quantized camera ints (3 position @ 0.25 voxel units, 3 forward @ 0.005 ~0.3° steps) + `sceneVoxelPayloadVersion` + `chunkDescriptorCount`. 1-voxel camera moves always invalidate; sub-1° rotations also invalidate.
- **Hash function:** splitmix64-style fold with 7 per-input mixers (`0x9E3779B185EBCA87`, `0xC2B2AE3D27D4EB4F`, …) and a final 3-step avalanche. The exact constants don't matter for correctness — only that a 1-bit change in any input flips ~half the hash bits (avalanche property).
- **Cache invalidation:** any of (a) hash mismatch, (b) `chunkDescriptorCount` change, (c) `sceneVoxelPayloadVersion` change. The hash alone is sufficient; the explicit checks in the if-condition are belt-and-suspenders against a future refactor that drops one of the fields from the hash.
- **Cache miss path:** `RebuildChunkVisibilityAndFillCache` runs the canonical per-chunk loop AND fills the cache in the same pass. No extra copy step on the cold path.
- **Cache hit path:** `ApplyCachedChunkVisibilityCommands` does three `memcpy` calls (opaque, shadow, transparent). At 300 chunks that's 300*16 + 300*4*16 + 300*16 = ~24 KB — well under any L1. Replaces 1500+ dot products per frame.
- **Profiler plots:** existing `Visible Chunks` / `Culled Chunks` plots stay populated on both hit and miss (read from cache on hit, computed on miss). New `ChunkVisibilityCacheHits` plot tracks the consecutive-hit counter — useful for correlating cache behaviour with profiler traces.
- **Quantization functions in `projectv::visibility_cache` namespace** (`src/render/SceneResources.hpp`): `QuantizeCameraPositionComponent` (floor(value / 0.25)) and `QuantizeCameraForwardComponent` (lround(clamp(value, -1, 1) / 0.005)). Plus `ComputeVisibilityCacheHash(parameters, sceneVoxelPayloadVersion, chunkDescriptorCount)`.

Почему:

- **Per-frame CPU cull is pure waste on a static camera.** `UpdateChunkVisibilityAndIndirectCommands` runs every frame on every chunk in `chunkDescriptorCount`; on a static replay / capture / look-dev scene, all that work produces identical commands. The cache is a direct 5-15× speedup of the cull pass on those workloads.
- **Quantization 0.25 voxel / 0.005 forward** is the smallest change the operator perceives as "the camera moved". A 1-voxel move should always rebuild (otherwise the cache serves stale data that the operator notices); a 0.1-voxel move is sub-perceptual and can be served from cache. Sub-1° rotations don't visibly change the cull set either.
- **splitmix64 over FNV-1a** because splitmix64 has a stronger avalanche (FNV-1a has known bad behaviour on small input changes). Constants from the public-domain splitmix64 reference implementation.
- **Single `RenderState`-level cache, not per-frame** because the cached commands are frame-independent. Two `memcpy` calls instead of one would double the GPU bus traffic; one `memcpy` to both `sceneFrameResources[0]` and `[1]` keeps the per-frame behaviour identical to the pre-cache path.
- **Belt-and-suspenders explicit checks** in the if-condition — the hash itself folds all 8 inputs, but a future refactor that accidentally drops one of the fields from the hash would silently extend the cache lifetime. The explicit `chunkDescriptorCount == frameResources.chunkDescriptorCount` etc. checks make the dependency explicit at the call site.
- **Cache miss writes to BOTH mapped buffer and cache in one pass** because the cost of the per-chunk math dominates the cost of the vector element assignment; doubling the work to "write to cache separately" would erase the gain on miss-heavy workloads.
- **5.3 benchmark automation** (next section) is the verification path for the cache's hit/miss ratio: `PROJECTV_BENCHMARK_FRAMES=N PROJECTV_BENCHMARK_QUIT=1` runs N frames in a controlled setting, and the `ChunkVisibilityCacheHits` plot reports the consecutive-hit count per frame.

Cross-refs: `agent/memory.md` §10.19 (working rules + full build/ctest/smoke state), `TODO.md` §4 World/Render/Tooling (closed), `agent/status.md` §13 (this session's snapshot), `agent/active-sessions.md` session-2026-06-12-lowlevel-perf-tooling (closed).

## 23. Debug gizmo overlay contract (`5.2`, `2026-06-12`)

Решение:

- **Cascade split plane boxes** — 4 thin AABBs, one per CSM cascade, world-axis-aligned (because `DebugOverlayBox` is `Int3 min/maxExclusive` and cannot rotate). XZ footprint uses the cascade's `orthoWidths[cascadeIndex]` / `orthoHeights[cascadeIndex]` (so the operator gets a "shadow frustum footprint" cue), Y is a thin slab around the camera-relative Y. Four distinct hues (red/orange/cyan/magenta) so cascades 0-3 are distinguishable at a glance.
- **Cursor hit normal shaft** — ≤2 voxel boxes along `selection.hitNormal` (±1 in one axis, guaranteed by `VoxelRaycast`), emitted *beyond* the hit voxel so it reads as a "next to selection" arrow rather than overlapping the yellow selection box. Zero-norm `hitNormal` is a no-op (defensive).
- **Hotkeys:** `L` cycles cascade split planes (reserved per `agent/status.md §9` TAA tuning-ladder footnote: "L остался свободен на будущее"); `Z` cycles cursor hit normal. Both follow the same hotkey-on / `hudVisible`-on emission contract that `showChunkBounds` / `showDirtyChunkOverlay` already use.
- **`BuildDebugOverlayBoxes` signature:** trailing `CameraState camera = CameraState{}` and `RenderState render = RenderState{}` default-valued params. The 2 existing tests at `tests/VoxelWorldTests.cpp:7302` and `:7348` keep their 4-arg call shape and stay green; expected box counts (14, 10) unchanged because gizmos default to off.

Почему:

- **World-axis-aligned cascade boxes** (not camera-aligned) because `DebugOverlayBox` API doesn't support rotation. The XZ footprint uses each cascade's ortho extent because that's the useful diagnostic for split-lambda tuning; a thin Y slab keeps the box visible from any camera angle.
- **Cascade boxes emit before selection box** so the yellow selection box (when present) wins Z-test for ties against the dimmer cascade boxes (alpha 0.55).
- **Cursor hit normal shaft emits *after* selection box** so the dim-white shaft reads as a "next to selection" arrow, not as a replacement marker.
- **Default-valued trailing params** keep the test API stable. If a future feature wants to render gizmos without the HUD, move the `hudVisible` early-return out of `BuildDebugOverlayBoxes` (the per-gizmo flags already gate emission independently).

Cross-refs: `agent/memory.md` §10.19, `TODO.md` §4 Gameplay/Debug (closed), `agent/status.md` §13, `agent/active-sessions.md` session-2026-06-12-lowlevel-perf-tooling (closed).

## 24. Benchmark automation contract (`5.3`, `2026-06-12`)

Решение:

- **4 env vars:** `PROJECTV_BENCHMARK_FRAMES` (master gate, unset = inactive), `PROJECTV_BENCHMARK_WARMUP_FRAMES` (default 30, discarded before measurement), `PROJECTV_BENCHMARK_LOG_EVERY` (default 60, progress log frequency), `PROJECTV_BENCHMARK_QUIT` (`1` returns `SDL_APP_SUCCESS` after the last measured frame).
- **State struct:** `BenchmarkAutomationState` mirrors `LookDevCaptureAutomationState` shape (active / quitWhenDone / completed) for symmetrical wiring in `main.cpp`. `minFrameSeconds` uses a sentinel `1e30f` initial value so the first valid frame always wins; `maxFrameSeconds` uses `0.0f`. The mean is `totalFrameSeconds / framesRendered`.
- **Per-frame tick:** `UpdateBenchmarkAutomation(state, debugStats, frameCounter)` returns `true` only when the benchmark is done AND `quitWhenDone` is set, so `main.cpp` can return `SDL_APP_SUCCESS` and exit cleanly.
- **New field on `AppState`:** `BenchmarkAutomationState benchmark{}`. Inactive when `PROJECTV_BENCHMARK_FRAMES` is unset (zero overhead).

Почему:

- **30 warmup frames** matches the operator-visible "first stable frame" on a cold ProjectV launch. Without warmup, the first 30 frames include Vulkan pipeline compile, VMA pool warmup, SPIR-V load, and the first chunk meshing dispatch — none of which represent steady-state cost. 30 is a safe floor; 60 would also work but doubles the run time of small N.
- **`min/maxFrameSeconds` sentinels** (1e30f / 0.0f) — the alternative (compute the first sample inline, set min=max=firstFrame) adds branches on the hot path. The sentinel approach means `std::min` / `std::max` on the first valid frame is correct without special-casing.
- **`quitWhenDone` is opt-in** because the canonical "look at the HUD for FPS" use case doesn't want the process to exit. The CI / scripted use case sets `PROJECTV_BENCHMARK_QUIT=1` and reads the structured SDL_Log line.
- **Symmetrical with `LookDevCaptureAutomationState`** so a future "all automation types" refactor can move them behind a single `AutomationRegistry` without per-state plumbing.
- **`PROJECTV_BENCHMARK_FRAMES` read once in `SDL_AppInit`** (not a per-frame env re-read) — the alternative would race with the operator's `$EDITOR` and invalidate in-flight measurements. If a future feature wants mid-session re-arm, the env-reader should be split out of `ConfigureBenchmarkAutomationFromEnvironment` and called from a hotkey.

Cross-refs: `agent/memory.md` §10.19, `TODO.md` §4 World/Render/Tooling (closed), `agent/status.md` §13, `agent/active-sessions.md` session-2026-06-12-lowlevel-perf-tooling (closed).

## 25. Greedy meshing contract (`4.1`, `2026-06-12`)

Решение:

- **Per-axis dispatch** в `voxel_mesh.comp::GreedyFacePass`. Один compute pass на chunk, но 6 внутренних greedy-проходов — по одному на каждый `(axis, sign)` (`X+/X-/Y+/Y-/Z+/Z-`). Per-axis (vs single triple-nested) даёт clean kill switch (`#define GREEDY_MESHING 0` + fallback), внятный data flow, и trivially-parallelizable на будущее (если станет нужен real parallel-merge). Per-frame dispatch count НЕ растёт (всё ещё `gl_GlobalInvocationID.x = dirtyChunkListIndex`, 1 thread/chunk) — work концентрируется в 6 sequential greedy scans per thread, ~6×`extentU*extentV` cell reads на chunk (vs 6×`extentX*extentY*extentZ` в pre-A1).
- **Merge condition — solid + same exposed state.** Two adjacent cells on the same face plane can merge iff они оба:
  1. `cellMaterial` одинаковый (тот же voxel type), AND
  2. `ShouldEmitVoxelFace(cellMaterial, neighborMaterial)` одинаковый — то есть `neighborMaterial` попадает в тот же `{Air, Glass}` set (для opaque/fluid) или в `Air` (для glass — `ShouldEmitVoxelFace(glass, glass)=false` per `decisions.md §13`).
  - AO не участвует в merge condition: per-vertex AO disabled (`decisions.md §14` v2), `lightingData` no-op. Face-independent AO фундаментально даёт pseudo-shadow на convex 2x2x2 corners; face-corner AO — face-boundary discontinuity. Merge only by material+neighbor — visually correct, perf-maximizing.
  - Transparent voxels (`material == 1`, Glass) participate в greedy как обычно — но на 1 quad, не multiple instances. Z-sort assumption: greedy сортировка сохраняется because all cells in a merged quad share `material` and emit at one anchor `localVoxelCoord` + face. Front-to-back order определяется per-quad `firstFace` offset, не per-cell.
- **`PackedFace` extension 12 → 16 bytes.** Add 4th uint `packedExtents = (width, height, _, _)` 8 bits each. All 4 consumers synchronized:
  - C++ `PackedSceneVoxelFace` в `core/Types.hpp:47-65` (added field, 4 `static_assert` обновлены: `sizeof == 16`, 4×`offsetof`).
  - `voxel_mesh.comp::PackedFace` (struct mirror).
  - `voxel.vert::PackedFace` + `ApplyGreedyScale` helper.
  - `voxel_shadow.vert::PackedFace` + `ApplyGreedyScale` mirror.
  - **`SceneResources.cpp:927`** uses `sizeof(PackedSceneVoxelFace) * count` — auto-adapts to 16 bytes, no manual change needed (sizeof = single source of truth for the buffer stride).
- **Vertex shader scaling.** `voxel.vert` (and shadow mirror) extracts `quadExtents = (width, height)` from `packedExtents` и применяет `ApplyGreedyScale(faceIndex, unitOffset, quadExtents)`. The helper:
  - Maps `faceIndex` → in-plane channels: 0/1 → (Y, Z), 2/3 → (X, Z), 4/5 → (X, Y).
  - Multiplies the 0/1 unit offset's in-plane channels by `(width, height)` соответственно.
  - The normal-axis channel stays 0/1 (1 voxel thick — face plane is at `localVoxelCoord + normal_offset`).
  - For unit quads `(width=1, height=1)` это no-op; per-corner unit offset produces the same 1×1 quad as pre-A1.
- **Visited bitmask — `kMaxChunkExtentForGreedy = 64`.** 64×64 plane = 4096 bits = 128 uints (512 bytes) per axis+direction pass. 6 passes per chunk = 3KB stack-allocated local memory (GLSL local array with `const` size = fine on RTX 3060).
  - **Fallback to per-voxel (1×1 quads) для oversized chunks** where `extentU > 64` или `extentV > 64`. PackedFace's 8-bit per-axis packing всё ещё allows up to 256, но practical chunk size in this project ≤ 64 (TODO §4.5 perf budget). Fallback path shares emit logic с pre-A1 (1×1 quad per exposed cell).
- **`DrawCommand(6u, ...)` unchanged.** 1 quad = 2 triangles = 6 indices, всегда. Greedy merge reduces INSTANCE count (1 instance = 1 quad, was 1 instance per voxel-face); vertex stage output drops proportionally.
- **`ShouldEmitVoxelFace` policy unchanged.** Same asymmetry (opaque emits vs Air/Glass, glass only vs Air, fluid vs Air/Glass) per `decisions.md §13`. Greedy merge preserves per-cell behavior — the merged quad's neighbor check uses per-cell `neighborMaterial`, not quad-level.
- **Cross-chunk reads.** `ReadVoxelMaterial` returns 0 (Air) для out-of-world позиций, и `ShouldEmitVoxelFace(faceMaterial, 0)` returns true для non-zero face material — поэтому faces on chunk boundary emit toward outside-world voxels as expected. Greedy pass seamlessly works on chunk boundaries без per-chunk coordination.

Почему:

- **Per-axis algorithm, single dispatch.** Per-axis clean separates the 6 directions для reasoning; single dispatch избегает 6× dispatch overhead per chunk (612k dispatches/sec при 100 chunks × 60Hz). Work density ~same per chunk (6× per-axis cells vs 6× triple-nested per voxel) but per-cell constant factor slightly higher (greedy extension reads).
- **AO exact match НЕ required** because per-vertex AO is disabled. Если future welded-mesh + per-vertex AO land (`decisions.md §14` future path), merge condition должен добавить `aoMask` в state vector — но это separate future work, не A1 blocker.
- **`kMaxChunkExtentForGreedy = 64`** — buffer-driven choice: 64×64 plane fits comfortably в L2 cache (256KB on RTX 3060), 6 passes × 512 bytes = 3KB local memory, zero spillover. Larger chunks get fallback to per-voxel (no crash, just no merge benefit).
- **Default-valued `width=1, height=1` в non-greedy path** so future code paths (debug overlay boxes, manual emit, replay fixtures) can keep emitting 1×1 quads without populating `packedExtents`.
- **`PackedFace` 12→16 bytes** is the minimum viable extension. Альтернатива (separate per-instance extents SSBO) добавил бы 7th binding + new descriptor set + storage cost. 16-byte struct with `static_assert`-enforced byte layout prevents drift.
- **Cross-chunk `ReadVoxelMaterial`** уже handles OOB (returns 0=Air) — greedy pass автоматически extends to chunk boundary без chunk-coordination protocol. Worst case: chunk boundary quads merge with `neighborMaterial=0` (Air), и следующий chunk на adjacent face plane будет also have `neighborMaterial=0` (тоже Air, если сосед тоже empty) — но chunk is invisible until meshed, so independent dispatch is safe.

Cross-refs: `agent/memory.md` §10.20, `TODO.md` §4 (greedy meshing closed) + §4.5 (perf budget context), `agent/status.md` §14, `agent/active-sessions.md` session-2026-06-12-greedy-meshing.

## 26. Frame-step / slow-motion debug contract (`2026-06-12`)

Решение:

- **Time scale is a continuous axis independent of `paused`.** `SimulationState::timeScale` (float, default `1.0`, range `[0, 4]`) multiplies `frameDeltaSeconds` after `ComputeFrameDeltaSeconds`. The existing `TogglePause` (`P`) handler continues to flip `simulation->paused` and reset the accumulator on transition — pause and slow-motion are deliberately **distinct runtime axes** so the operator can leave `timeScale = 0.25` for fine-tuning camera framing while still being free to step one frame at a time with `\`.
- **`timeScale = 0` and `paused = true` produce the same effective sim-stop but are not the same state.** The wall-clock `framesPerSecond` / `frameTimeMilliseconds` stats still report real-time even at `timeScale = 0` because the scaling is applied to `simulation->frameDeltaSeconds` after `ComputeFrameDeltaSeconds`, not before. Input replay recording records the wall-clock delta the same way.
- **4 hotkeys, all keyboard, all runtime, no preset file.** `[` halves `timeScale` (snaps to `0` below `0.01` for a discrete "pause" stop), `]` doubles `timeScale` (`timeScale == 0` bounces to `0.5` so the operator can escape zero; clamped to `4.0` at the top), `\` queues exactly one fixed-step tick (`frameStepRequested = true`; consumed at the top of `UpdateApp`), `` ` `` resets to `1.0`. `\` / `` ` `` were chosen over `[` / `]` because they sit on the QWERTY backtick/backslash row and are unused by the TAA ladder (`;`/`'`/`-`/`=`/`,`/`.`) or the 5.2 gizmo ladder (`L`/`Z`).
- **`effectivePaused = simulation->paused && !frameStepRequestedNow`.** The frame-step handler reads-and-clears `frameStepRequested` at the top of `UpdateApp`, so the `effectivePaused` local is true only when the user explicitly paused AND did not press `\` this frame. Three `simulation->paused` references — the `cameraCanUpdate` flag, the accumulator update block, and the physics-tick while loop condition — were switched to `effectivePaused` so a same-frame step bypasses the pause gate cleanly. The `paused && spectator` camera-tick block is also gated on `effectivePaused` so the camera can look during a frame step.
- **Frame-step accumulator override.** When `frameStepRequestedNow` is true, `simulation->simulationAccumulatorSeconds = simulation->fixedSimulationDeltaSeconds` overrides the per-frame scaled-delta accumulation, so exactly one fixed tick runs that frame regardless of `timeScale` and regardless of `paused`. The flag is read-then-cleared, so back-to-back presses translate to "one tick per press" (the per-frame `while` loop only ever holds one step at a time).
- **Frame-step is orthogonal to TAA history invalidation.** Unlike world reload / swapchain resize / TAA toggle, the `frameStepRequested` event does **not** invalidate `taaHistoryValid` or `taaLayerHistoryValid` — TAA's reprojection is per-frame and `\` is per-frame, so a single step just appears as a single frame in the TAA history chain. If a future bug shows a frame-step-induced TAA artifact, the right fix is camera-cut detection (1.2), not a new `frameStepRequested → invalidate` rule.
- **HUD surfaces.** New `TIME x.xx` line adjacent to the existing `MODE / PAUSE / AIR` line so the two pause-related runtime axes read as a group. One-frame `STEP` indicator (only emitted when `simulationFrameStepPending` is true on the press frame). Helper panel: 2 new lines `TIMECTL DOWN UP` and `TIMESTEP STEP RESET 1X` in the detailed-HUD section, using only glyphs the existing font supports (A-Z, 0-9, `.`, `-`, `:`) — the bracket / backslash / backtick keys are spelled out in the helper text because their raw glyphs are not in the font.

Почему:

- Continuous time-scale axis, not a discrete slow/normal toggle, because the operator's first instinct when a frame looks wrong is "let me see that slower" — a 0.25x / 0.5x / 1x / 2x / 4x ladder captures every common case without inventing per-preset speed labels. The 0.01 snap threshold on the `[` key is a UX concession: a half-step from `0.0156` would round to `0.0078` and the operator would wonder why the sim crawled.
- `effectivePaused` rather than mutating `simulation->paused` itself, because toggling `paused` would re-zero the accumulator (`simulation->simulationAccumulatorSeconds = 0.0f` in the `TogglePause` handler) and undo the one-step budget. The local read+clear is a one-frame escape hatch, not a state machine.
- Frame-step does not invalidate TAA history, because every `paused`-state frame already goes through the TAA path normally — the resolve pass just sees one frame of "current only" because `taaHistoryValid` is set false by the existing triggers. The new frame-step path sits one layer up (the accumulator / sim tick) and does not need to touch the TAA contract.

Cross-refs: `agent/memory.md §10.23` (working rules), `agent/status.md §15` (session snapshot), `agent/active-sessions.md session-2026-06-12-frame-step-slow-motion`, `TODO.md §4 "frame-step / slow-motion debug modes"` (closed).

## 27. Per-pass CPU timing contract (`2026-06-12`)

Решение:

- **CPU-side per-pass timing, not GPU `VkQueryPool` timestamps.** Each `Record*Commands` function in `Renderer.cpp` measures its own wall-clock CPU time with `SDL_GetPerformanceCounter` (same primitive as `ComputeFrameDeltaSeconds` in `AppUpdate.cpp`). RAII wrapper `ScopedPassTimer` in the anonymous namespace handles early-return paths automatically — the destructor writes the ms value when the function exits, even if it returns early because the pipeline is null. 6 measurements: `shadowMs`, `meshingMs`, `graphicsMs`, `taaResolveMs`, `debugOverlayMs`, `debugHudMs`. Plus `otherMs = frameTimeMs - graphicsMs` derived in `AppUpdate.cpp`.
- **Manual timer for the inlined TAA resolve block.** `RecordGraphicsCommands` is too large to wrap a `ScopedPassTimer` around the whole thing and call that "graphics" — the TAA resolve is one of 5 distinct sub-passes inside it. The TAA resolve inline block (~60 lines between the `PV_PROFILE_GPU_LABEL_COLOR` and the resolve `vkCmdDraw(cmd, 3, 1, 0, 0)`) gets a manual `SDL_GetPerformanceCounter` start/end pair, and the outer `RecordGraphicsCommands` gets a `ScopedPassTimer` for the total. The 5 sub-pass measurements (shadow / meshing / taaResolve / debugOverlay / debugHud) are subsets of `graphicsMs` — the HUD shows both the total and the breakdown.
- **`RenderPassTimings` struct, not loose fields.** All 6 measured fields + 1 derived (`otherMs`) + 1 count (`dirtyChunkRebuiltCount`) live in a single `RenderPassTimings` struct on `RenderState`. `DebugStats` mirrors them as 7 float fields + 1 uint32 so the HUD and capture sidecar can read the per-pass breakdown without poking into `RenderState` directly. Future render-side observability state (GPU-side `vkCmdWriteTimestamp` results, drawcall counts per pass, etc.) can either extend `RenderPassTimings` or live in a sibling struct — the struct-vs-fields decision is the easier one to change later.
- **HUD line is detailed-only.** The 2 per-pass HUD lines (`RPASS GFX / OTH` + `RPASS SHAD / MES / TAA / OVL / HUD / CHNK`) are emitted in the `detailedHudVisible` branch of `BuildStatsLines`. Reason: the test harness uses a 65536-vertex buffer (`std::vector<DebugHudVertex>(65536)`) for the geometry-output sanity check, and the original detailed-only HUD was already at the cap. Adding the per-pass lines to the basic section would push both basic AND detailed to the cap, breaking the `detailedVertexCount > basicVertexCount` invariant. Diagnostic data is also more appropriate for the detailed-HUD path (it complements the existing `SUN / ENV / SHDW / BIAS / CTSH / AOCC / LOCL` line family there).
- **`kMaxStatsLineCount = 38` (was 36).** Two new lines for the per-pass timing. The cap exists to bound the `std::array<std::array<char, kHudLineBufferSize>, kMaxStatsLineCount>` allocation in `BuildDebugHudVertices`; production runtime uses the much larger `DEBUG_HUD_MAX_VERTEX_COUNT = 262144` (VMA-allocated buffer), so the stats-line-count cap is a compile-time safety net, not a runtime limit.
- **Sidecar metadata split into a second `fmt::format` call.** The existing `SaveScreenshotCaptureMetadata` already used 99 args in the main `fmt::format` call (the `fmt` 99-arg compile-time checker trips with 7 more). The 7 per-pass keys + 1 count get their own `stream << fmt::format(...)` call concatenated to the same sidecar file. Existing parsers see the new keys at the end of the file and existing assertions (`text.find("scene_preset=...")`) keep passing because they look for specific `key=value` substrings, not positional.
- **`dirtyChunkRebuiltCount` snapshots at the start of `RecordVoxelMeshingCommands`, not at the dispatch site.** If the function early-returns (pipeline null, descriptor set null, etc.), the operator still sees what was requested. The "what was actually dispatched" value is derivable from `vkCmdDispatch(cmd, frameRenderData.dirtyChunkCount, 1, 1)` at the call site, but for the HUD's purposes "what was requested" is the more useful question (it answers "is the mesher stalled on dirty chunks?" not "did the mesher pipeline compile?").
- **GPU-side `vkCmdWriteTimestamp` is a follow-up, not a parallel implementation.** CPU-side timing is sufficient for the "where is my frame budget going" use case (TODO §4.5 perf-budget analysis). GPU timestamps would be needed only if the operator wanted to distinguish "CPU stalled in `vkCmdDraw`" from "GPU stalled in pipeline execution" — that is a separate quality question, not a question this slice answers.

Почему:

- CPU over GPU timing for v1, because: (a) zero setup cost (no `VkQueryPool` allocation, no per-frame reset, no command-buffer recording for timestamp queries); (b) sub-millisecond accuracy is sufficient at the 8-12 ms / frame budget the current mainline path runs at; (c) the test harness would need to be aware of GPU query pool sizes and reset semantics, adding complexity for marginal value.
- RAII over manual start/end everywhere, because the `Record*Commands` functions have 1-3 early-return paths each, and missing one would silently leave the previous frame's stale number on the HUD. The wrapper costs nothing at -O0/-O2 and makes the call sites one line.
- Detailed-only HUD placement, because the basic HUD is meant to be readable at a glance and the per-pass breakdown is diagnostic. The "always-on" data (frame time, FPS, sim steps, triangle count) stays in the basic section; "where is my budget going" lives in detailed mode where the operator has already opted in for the verbose SHDW/BIAS/CTSH/AOCC/LOCL line family.
- `kMaxStatsLineCount` is the one knob the operator is most likely to bump, so it stays a named constant near the top of the file rather than being computed from another constant. If the per-pass lines ever need to be 4 lines instead of 2, only the cap and the HUD block change — no renderer / struct changes.

Cross-refs: `agent/memory.md §10.24` (working rules), `agent/status.md §16` (session snapshot), `agent/active-sessions.md session-2026-06-12-richer-render-stats`, `TODO.md §4 "richer render stats / explicit per-pass timings"` (closed).

## 28. Audio engine contract (`2026-06-12`)

Решение:

- **miniaudio, not SDL_mixer or OpenAL.** miniaudio is a single-header C library (100k lines, all in `miniaudio.h`) with a built-in MP3 decoder (no external `libmpg123` dep), a one-stop `ma_engine` API, and clean Linux PipeWire routing via its PulseAudio backend → `pipewire-pulse` shim. SDL_mixer pulls in a runtime ABI mismatch per release; OpenAL is a heavier API surface and lacks the `ma_engine_set_volume` / per-track-loop ergonomics we want. Per `legacy/docs/architecture/practice/02_engine_bootstrap_spec.md:533` the audio subsystem has been planned for years; this is the v1 implementation.
- **Playback format = 16-bit signed PCM at 44.1 kHz stereo, device-native per engine config.** The `ma_engine_config` API only exposes `sampleRate` and `channels` directly; the `playback.format` substruct is `ma_device_config`-only. The engine picks the device's native format (typically `ma_format_s16` on built-in Linux audio, which matches the user-spec "16/44100"). If a future slice needs to force a specific format, it has to drop to the lower-level `ma_device` API. v1 doesn't.
- **Linux backend = PulseAudio → pipewire-pulse → PipeWire.** miniaudio has no direct PipeWire backend. On this host, `pactl info` reports `Server String: /run/user/1000/pulse/native` — that's the `pipewire-pulse` shim serving the PulseAudio wire protocol, with PipeWire as the actual audio server. miniaudio's `find_package(PulseAudio)` resolves `libpulse.so.0` and the output is automatically routed to PipeWire. The user's "выход pipewire pcm" requirement is satisfied by this chain.
- **`MusicState` enum: `Stopped | Playing | Paused`.** Three-valued for HUD/sidecar clarity. **Cursor semantics, 2026-06-13 fix (was wrong before):** `ma_sound_stop` (called by both `pauseImpl()` and `stop()`) preserves the cursor in-place on the `ma_sound` struct — it only sets the node state to stopped (miniaudio.h:78774), the `pSound->cursor` field is untouched. A subsequent `ma_sound_start` resumes from that cursor. So v1 **does** have true pause/resume, no custom decoder wrapper needed. The "no `ma_sound_set_time` → pause forgets cursor" claim was a misreading of the miniaudio API: the absence of `ma_sound_set_time` only prevents arbitrary SEEK, not the stop/start cursor-preservation cycle. The original 2026-06-12 audio-engine slice had a real bug — the Paused branch of `togglePlayPause` unconditionally called `loadCurrentTrack()` which unloaded and re-init'd from disk, always resetting the cursor to 0 — but the bug was in the code path, not in the underlying miniaudio API. The 2026-06-13 fix adds the `if (!m_soundLoaded)` guard to the Paused branch (mirroring the Stopped branch) so the cursor is preserved across pause → resume. `m_pausedCursorMs` is **dead code** since `ma_sound_stop` already preserves the cursor; kept for field-shape stability, candidate for v2 cleanup.
- **`AudioEnginePtr` uses a function-pointer deleter at global scope, matching `DestroyEcsState` / `DestroyPhysicsState`.** The deleter (`DestroyAudioEngine` in `audio/AudioEngine.cpp` at global scope) is `delete engine`, which transitively calls `~AudioEngine() → shutdown()`. This pattern keeps `core/Types.hpp` header-only (no need to include `<miniaudio.h>` there), which matters because `core/Types.hpp` is included by ~20 TUs and `<miniaudio.h>` is a 100k-line single-header library.
- **5-second playlist refresh, sticky `m_currentIndex`.** The playlist is rebuilt every 5 seconds via `std::filesystem::directory_iterator`. If the currently-loaded track is still in the new playlist, the index is remapped to its new position (so new files added before the current track don't disrupt playback). If the current track is gone, the engine unloads the sound and transitions to `Stopped` (so the next `Q` press loads whatever's at index 0 now). 0-second refresh would be wasteful; 30-second refresh would be visibly laggy when the operator drops a new file in. 5 is the empirically-sensible midpoint.
- **Loop = `MA_TRUE` for v1.** Music is a "fire-and-forget" experience in this engine; the operator doesn't expect to manually restart. If a future slice adds an SFX layer, that layer can use the default `MA_FALSE`.
- **4 hotkeys in v1: `Q` play/pause, `E` stop, `7` vol-, `8` vol+.** v1 layout is placeholder per the operator's note "надо переназначить все кнопки, потому что текущая раскладка неудобная, но это потом." These are the only free letters/digits in the existing `InputAction` enum (Q, E, 7, 8 are not bound; the bracket and backslash/backtick keys from the time-scale ladder and the TAA ladder already take `[ ] \ `` ` ``). The full hotkey rebind is a follow-up slice.
- **Volume = 0.0..1.0, step 0.05, default 0.8.** Step matches the existing `kLightingExposureStepStops` style (5 cents per press); default 0.8 is the legacy spec from `legacy/docs/architecture/practice/40_cpp26_reality_spec.md:262` (`volume_music{0.8f}`). Applied to the music `ma_sound_group` bus-level volume, so future SFX/Ambient groups can have their own bus-level volumes without cross-contamination.
- **Graceful degradation on every failure mode.** miniaudio init fail / empty folder / broken `.mp3` file / operator press when playlist is empty — all are logged via `runtime::LogRuntimeFailure` and silently degrade. The program keeps running; the HUD shows `MUSIC OFF VOL 0.80` or `MUSIC STOP VOL 0.80 NO TRACKS`; hotkeys are no-ops. Per `decisions.md §4` build/verification contract: the renderer-side smoke is a targeted check, not mandatory DoD.
- **Sidecar `music_*` keys write `initialized=0` for now.** The screenshot capture path doesn't have a direct pointer to the `AppState::audio` engine (`DrawFrame` → `RecordGraphicsCommands` → `SaveRequestedScreenshot` → `SaveScreenshotCaptureMetadata` none of which take an audio pointer). Plumb the audio engine pointer through `FrameRenderData` (or via a `RenderContext` struct) is a follow-up slice. The HUD's `MUSIC <STATE> VOL 0.80 TRK <name>` is the authoritative live view.
- **Track switching contract (follow-up slice, 2026-06-12).** `nextTrack()` / `previousTrack()` cycle through the playlist with wrap-around. Per-state behavior on a switch: Playing = interrupt + reload + start (what the user expects from "Next" mid-playback); Paused = reload only (state stays Paused so `Q` plays the new track); Stopped = index update only (no sound to reload). Empty playlist = no-op (the hotkey does nothing — same as the play/pause no-op on empty playlist). The `m_pausedCursorMs` field is reset to 0 on every switch (the new track's cursor is 0; v1 has no resume-from-cursor regardless of which track). Hotkeys `9` and `0` are the only adjacent free digit pair in the existing `InputAction` table (7/8 went to volume in the audio-engine slice). v1 layout is still placeholder per the operator's note "надо переназначить все кнопки ... но это потом."
- **`MA_SOUND_FLAG_STREAM` for the file loader.** The MP3 is streamed from disk rather than pre-decoded to RAM. For typical music files (3-10 MB) this is a small saving, but the right semantic for "playlist that can change every 5 seconds" — pre-loading the file would mean re-loading it every time the operator drops a new file in. Flag is bitwise-orable with future flags.
- **4-line music HUD block (follow-up slice, 2026-06-13).** Replaces the 2026-06-12 1-line `MUSIC <state> VOL 0.80 TRK <name>` with a 4-line layout, one line per field the operator asked for: `MUSIC <state>  VOL 0.80` (always, basic+detailed), then 3 gated lines (`ARTIST <name>`, `TITLE <name>`, `POS m:ss / m:ss`) emitted only when the engine is initialized AND the playlist is non-empty. The state+volume share one line so the cap stays at 4 lines and `kMaxStatsLineCount=38` does not need to be bumped (basic +3, detailed +3, both still fit in 38 with headroom). Artist / title are parsed from the cached `m_currentTrackName` on the engine side via `audio::ParseArtistTitle` (case-insensitive `.mp3` strip, split on first ` - `, fallback `artist="-"`/`title=full-stem`) and re-parsed only on track change, not per frame. Position / duration are queried each frame via `ma_sound_get_cursor_in_seconds` / `ma_sound_get_length_in_seconds` (both O(1) miniaudio reads, both guarded by `m_soundLoaded` and falling back to 0.0f on `MA_FAILURE`); the `FormatMmSs` helper in `DebugHud.cpp` formats them, with `treatZeroAsValid=true` for position (so "0:00" shows at the start of a track) and `false` for duration (so "--:--" is the "decoder did not expose length" sentinel). The previous `TRK <full-filename>` label is gone — the full filename is no longer shown, since ARTIST and TITLE together are strictly more informative for the operator's eye (the filename is still in `audioMusicTrackName` for the sidecar follow-up).

Почему:

- miniaudio over SDL_mixer / OpenAL: single-file, no runtime ABI mismatch, built-in MP3 decoder, one-stop engine API, clean Linux PipeWire routing.
- 16/44100 at the engine config layer + device-native format at the device layer: the user said "16/44100" and on any sane Linux desktop the device picks 16-bit s16; forcing a specific format would require dropping to the lower-level API which is out of v1 scope.
- 5-second playlist refresh: 0 = wasteful, 30 = visibly laggy, 5 = responsive enough that the operator can drop a file and quickly verify it's in the playlist.
- Loop = `MA_TRUE` for v1: matches the user request "музыка" (music), which is intrinsically looping; an SFX layer can override.
- Hotkeys Q/E/7/8: the operator explicitly said the v1 layout is placeholder and the full rebind is a follow-up.
- `AudioEnginePtr` with function-pointer deleter at global scope: keeps `core/Types.hpp` header-only, avoids the 100k-line `<miniaudio.h>` include in ~20 TUs.
- Sidecar defaults to `music_initialized=0`: capture-side audio plumbing is a separate plumbing refactor (add `FrameRenderData::audioEngine` field, thread it from `DrawFrame`); out of v1 scope.
- 4-line music HUD over 1-line: the operator explicitly asked for "автора, названия, продолжительности, на какой мы минуте:секунде, надписи Playing/paused/stopped" — five pieces of info that don't fit in one 96-char line with the existing `kHudLineBufferSize`. Multi-line is the only sane way to expose all five; the volume was kept as a sub-field on the MUSIC line (rather than a 5th line) to preserve cap headroom and because the operator can already see the live value tick when 7/8 is pressed.

Cross-refs: `agent/memory.md §10.26` (working rules), `agent/status.md §18` (session snapshot), `agent/active-sessions.md session-2026-06-12-audio-engine` + `session-2026-06-13-music-hud-4line`, `legacy/docs/architecture/practice/02_engine_bootstrap_spec.md:533` (the planned `AudioSystem` that this slice implements).
