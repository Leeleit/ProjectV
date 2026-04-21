# Decisions

Живые инженерные договорённости. Roadmap живёт в `TODO.md`, общий протокол — в `AGENTS.md`.

Дата обновления: `2026-04-21`

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
- Тяжёлый R&D (`SVO`, mesh shaders, heavy simulation, big-world systems, большой editor, multiplayer, plugin stack) не должен блокировать ближайший practical milestone.

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
- Runtime smoke остаётся отдельной GUI-проверкой, но вызывается как официальный target.
- Shader compile path принимает `glslc` или `glslangValidator`.
- Для translation units с Jolt include-contract начинается с `<Jolt/Jolt.h>`; auto-refactor не должен поднимать другие Jolt headers выше него.

Почему:

- Это минимальный reproducible contour для mainline без лишней хрупкости и конфликтов build tree.

## 5. Interaction contract

Решение:

- World edit остаётся CPU-authored через `VoxelRaycast` и `VoxelWorld`.
- Постановка блока запрещается до мутации мира, если `placementVoxel` пересекает текущий physics-character volume.
- После successful world-edit rebuild через `SyncPhysicsWorld` cached walk support ownership надо инвалидировать до следующего walk tick.

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

- One-block auto-jump остаётся частью active traversal path.
- Default path держит micro-delay (`40` fixed frames).
- `F12` переключает только `delay on/off`, а не existence auto-jump.
- Manual held jump обнуляет pending auto-jump delay countdown.

Почему:

- Нужны оба режима: delayed Minecraft-like traversal и instant response для будущих bunny-hop experiments.

## 9. Debug / repro contract

- When a live walk bug diverges from synthetic fixtures, the preferred artifact is an input replay capture over another handwritten `SendKeyEvent` sequence.

Решение:

- Claims о walk/runtime regressions сначала проверяются через live repro + `PhysicsWalkDebugInfo`/HUD/Tracy, а не через blind heuristic patch.
- Высокий render FPS сам по себе не считается доказанной причиной walk bugs, пока это не подтверждено через real fixed-step path.

Почему:

- Этот проект уже несколько раз платил за попытки чинить live runtime bug только по synthetic-case тестам.

## 10. Creative flight collision contract

Решение:

- `creative` остаётся на `CharacterVirtual::ExtendedUpdate`, но boosted flight не делает один длинный collision step.
- `TickCreativeCharacter` делит длинный boosted travel на capped substeps по расстоянию (`~0.05 m`, максимум `32` substeps) и повторяет `ExtendedUpdate` на каждом substep.
- Regression для этого path держится на exact replay fixtures `tests/fixtures/creative_transparency_boost_stuck.*` и `creative_transparency_boost_corner_stuck.*`, а не на коротком synthetic-case приближении.

Почему:

- Normal-speed creative collision уже скользил корректно; ломался только high-speed coarse-step path, включая точные corner hits.
- Exact replay здесь надёжнее выдуманного теста, потому что старый synthetic-case уже давал ложный red/green сигнал и не совпадал ни с реальным клином на стеклянных колоннах, ни с клином ровно в угол.

## 11. Static-analysis cleanup contract

Решение:

- Checked-in `Problems/*.xml` inspection exports are treated as hints, not as the source of truth for live code.
- During warning cleanup, only issues that still reproduce on the current source, or are trivially visible in the current code, should be patched immediately.
- After a meaningful cleanup pass, regenerate `Problems/` before starting the next pass.

Почему:

- The current refactor/lint sweep already made several exported line-based findings stale mid-pass, and blindly following them risks fixing the wrong code.
