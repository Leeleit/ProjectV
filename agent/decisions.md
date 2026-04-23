# Decisions

Живые инженерные договорённости. Roadmap живёт в `TODO.md`, общий протокол — в `AGENTS.md`.

Дата обновления: `2026-04-22`

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
- Verification loop выполняется только последовательно: `build -> tests -> smoke`.
- Runtime smoke остаётся отдельной developer-only GUI-проверкой и вызывается как официальный target.
- Shader compile path принимает `glslc` или `glslangValidator`.
- Для translation units с Jolt include-contract начинается с `<Jolt/Jolt.h>`; auto-refactor не должен поднимать другие Jolt headers выше него.

Почему:

- Это минимальный reproducible contour для mainline без лишней хрупкости и конфликтов build tree.

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
- `UpdateSceneResources` освежает current scene lighting из `VoxelScenePreset` и runtime look-dev controls каждый кадр, а renderer clear color использует тот же contract вместо отдельной hardcoded sky-константы.
- Current look-dev ladder остаётся keyboard-first внутри живого sandbox loop: `B` cycles lighting debug views, `N` cycles tone-map, `H/K` adjust exposure, `V` resets to preset baseline.
- Reproducible look-dev capture stays inside the same runtime path too: `C` saves a `.bmp` of the current frame plus a sidecar metadata file with preset/exposure/shadow tuning, instead of treating screenshot capture as an external-tool-only workflow.

Почему:

- Так lighting/look-dev остаётся reproducible внутри текущего MVP loop без отдельного editor path и без скрытого shader-only состояния, которое трудно отлаживать и сравнивать между сценами.

## 15. First sun-shadow path
Update `2026-04-22`:

- The earlier "render the whole opaque face prefix with a direct draw" version of this path is obsolete. Packed opaque faces live in sparse per-chunk ranges, and the dense-prefix assumption caused `VK_ERROR_DEVICE_LOST` when switching into `TransparencyStress`.
- The shadow pass now binds its own descriptor/pipeline layout; it must not reuse the main graphics descriptor set that already samples the shadow image while that image is simultaneously written as a depth attachment.
- The current stability-first baseline now uses a dedicated all-occluder `shadowIndirectBuffer`. Compute meshing updates that buffer for dirty chunks, CPU keeps it warm for unchanged chunks, and the shadow pass no longer inherits camera-frustum culling from the main opaque visibility commands.
- The remaining limitation is now explicit rather than accidental: the current sun-shadow baseline is still opaque-only, so transparent-heavy demo scenes like `VoxelLab` can look almost shadowless even when the opaque shadow path is functioning correctly.


Решение:

- Первый practical shadow path для mainline — один scene-wide orthographic sun shadow map, а не cascades, RT shadows или более тяжёлый lighting stack.
- Shadow contract живёт в том же `VoxelSceneLighting`: per-preset shadow tuning (`strength/bias/normal-bias`) плюс `sunShadowViewProjection`, который CPU собирает из bounds активных chunk-ов и направления солнца, с fallback на полные `VoxelWorld` bounds только для пустой сцены.
- `sunDirectionAndWrap.xyz` remains the authored vector toward the sun for the main shading pass. The CPU shadow fit must invert it to the actual light-travel direction when building `sunShadowViewProjection`; this sign is part of the stable lighting contract, not an implementation detail.
- Shader-side receiver bias stays on the same authored `depth-bias` / `normal-bias` controls, but it should respond to sun angle instead of acting like one flat offset everywhere. The current baseline therefore scales those authored bias values by `N.L` in the voxel shader instead of adding a second hidden bias ladder.
- The first practical direct-light BRDF upgrade should stay within the current material buffer and shader path instead of introducing a separate PBR framework. `VoxelMaterialVisual` therefore now packs `AO/roughness/metallic/reflectance` plus transmission tint and fog/emissive/ambient/direct-response hooks in the same 64-byte table, and the main voxel shader consumes that contract with a `GGX + Fresnel-Schlick + Smith` sun-light baseline while still honoring authored ambient/diffuse response weights inside the existing ambient gradient, fog and shadow integration path.
- Shadow depth pass consumes a dedicated all-occluder opaque indirect buffer instead of the main camera-culling visibility commands; main voxel pass потом семплирует shadow map только для direct sun.
- Первый quality/debug follow-up для этого path тоже остаётся прагматичным: baseline shadow map держится на `2048x2048`, main voxel shader использует лёгкий `3x3` PCF, а shadow tuning/debug живёт внутри уже существующего lighting loop (`B` debug views + detailed HUD), а не в отдельном editor/debug framework.

Почему:

- Текущие built-in demo scenes конечные и компактные, поэтому scene-wide orthographic projection даёт дешёвый и понятный первый baseline без раннего ухода в R&D.
- The shadow pass still stays intentionally simple, but it must have its own opaque occluder command source; reusing camera-visible indirect draws is not acceptable because it makes shadow presence depend on the current view frustum.
- Так shadow slice остаётся совместимым с нынешним explicit CPU scene contract и dynamic-rendering path, а следующий шаг — тюнинг bias/stability/debug, а не новый lighting framework.
- Так текущий shadow slice становится достаточно читаемым и настраиваемым для mainline look-dev без раннего перехода к cascades, render graph или отдельному tooling stack.
