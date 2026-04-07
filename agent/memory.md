# Memory

Долгоживущая память агента по `ProjectV`.

Дата последнего обновления: `2026-04-07`

---

## 1. Идентичность проекта

`ProjectV` сейчас — это ранний voxel-engine prototype / pre-MVP alpha на базе:

- `C++26`
- `Vulkan 1.4`
- `SDL3`
- `Volk`
- `VMA`
- `Tracy`

Проект уже вышел за пределы "hello triangle", но ещё не является завершённым интерактивным MVP.

---

## 2. Что реально есть в проекте

Подтверждённые рабочие элементы:

- Vulkan bootstrap и базовый runtime;
- окно, swapchain, resize/recreate path и shutdown path;
- voxel world на CPU;
- чанки, dirty queue и bookkeeping перестроения;
- compute meshing path;
- opaque/transparent indirect draw path;
- explicit control modes поверх камеры: `creative` как collision-backed flight/edit mode с движением даже при `pause`,
  `spectator` как observe-only noclip mode без `remove/place`, который подчиняется `pause`, и `walk` как grounded
  collision-based mode поверх physics, который тоже подчиняется `pause`;
- в flying modes (`creative` и `spectator`) `WASD` теперь двигают только по плоскости `XZ`; высота меняется только от
  `Space/Shift`, а не от pitch камеры;
- mouse look теперь clamp'ится почти до вертикали (`~89°`), не доходя до полного переворота камеры и вырождения базисов view matrix;
- явный `InputActions` слой поверх SDL keyboard input с bindable scancode slots, edge-triggered hotkeys и default controls: `WASD + Space/Shift`, `Ctrl` boost, `Alt` slow, `F1` debug UI toggle, `F2` cycle placement material, `F3` reset camera, `F4` control mode cycle, `F5` runtime scene cycle, `P` pause, `Tab` mouse capture toggle, плюс double-tap `Space` как быстрый toggle `creative <-> walk`;
- CPU voxel raycast от камеры;
- runtime block picking и remove/place interaction через `VoxelWorld::SetVoxelMaterial`;
- минимальный `JoltPhysics` slice в `src/physics`: static voxel collision world, physics raycast и общий `CharacterVirtual`
  controller для `creative`/`walk` без полного gameplay-framework rewrite;
- `creative` больше не является noclip fallback: этот режим использует тот же physics-backed capsule/controller, что и
  `walk`, но без гравитации и без jump-only ограничения по вертикали;
- при переходе из `creative`/`spectator` обратно в `walk` physics-character теперь сначала пытается сохранить текущую
  позицию камеры, а ground recovery нужен только как fallback для заведомо некорректной/врезанной в world позиции, поэтому
  mode switch больше не телепортирует игрока в центр мира и не роняет на пол только из-за отсутствия опоры под ногами;
- синхронизация voxel edits и physics идёт через `VoxelWorld::editVersion`: при реальном изменении материала physics world
  пересобирается из актуального набора solid voxels;
- текущий `walk` controller всё ещё минималистичный: лёгкое скатывание с краёв блоков считается ожидаемым follow-up до
  отдельного ground-sticking / ledge-stability tuning;
- block highlight и crosshair через отдельный debug overlay graphics pipeline;
- in-app debug HUD через отдельный graphics pipeline и CPU-built vertex buffer без `imgui`;
- `F1` теперь скрывает весь debug UI целиком: HUD, block highlight и crosshair;
- HUD после usability-pass больше не монолитный список: он уменьшен, разбит на отдельные stats/helpers panels, а crosshair рисуется более толстым inverted/XOR-style overlay с logic-op path на поддерживаемых GPU и alpha fallback на остальных;
- минимальный `flecs` ECS slice в `src/ecs`, где primary camera/player сущности и `world`/`debug` singleton data уже встроены в main loop, а chunk state зеркалится в ECS entity summary без полного переноса ownership `VoxelWorld`;
- корректное dirty-neighbor обновление чанков при редактировании блока у границы;
- reproducible Windows runtime smoke path для `resize / minimize / restore / maximize / graceful shutdown` и отдельный smoke checklist в `docs/`;
- controlled failure probes для missing shader и incomplete init через `PROJECTV_SHADER_BASE_DIR`, `PROJECTV_FAIL_INIT_STAGE` и `tools/windows/Invoke-ProjectVFailureProbes.ps1`;
- authored-docs entry set теперь живёт в `docs/ArchitectureGuide.md`, `docs/RenderArchitecture.md`, `docs/VoxelWorld.md`,
  `docs/BuildAndRun.md` и `docs/Debugging.md`; это текущий основной human-facing набор документов о mainline MVP;
- profiling baseline на `2026-04-07` теперь тоже зафиксирован: builtin `VoxelScenePreset` layer в `VoxelWorld` выбирается через
  `PROJECTV_SCENE_PRESET`, сцены `VoxelLab` / `FlatBenchmark` / `TransparencyStress` / `ChunkGrid` / `MeshingStress` служат
  reproducible perf baselines, а `docs/Profiling.md` фиксирует scene purposes, Tracy plot pack и benchmark methodology;
- минимальный `RuntimeDiagnostics` layer на `fmt` для unified runtime error logs и `PV_CHECK_OR_RETURN` / `PV_ASSERT` в bootstrap/render/pipeline path;
- `RuntimeDiagnostics` log helpers теперь трактуются как side-effect-only logging API без bool-return contract; failure path в mainline коде должен оставаться явным как `log + return false`, а не скрываться в `return Log...`;
- физически структурированное `src/` с каталогами `app/`, `core/`, `debug/`, `ecs/`, `physics/`, `platform/`,
  `render/`, `render/vulkan/` и `voxel/`;
- project и test targets теперь используют только корень `src/` как include boundary; внутренние project headers подключаются qualified-путями вида `app/Camera.hpp`, `core/Types.hpp`, `render/vulkan/VulkanInit.hpp`;
- fixed-step update loop;
- процедурная demo-scene;
- Tracy hooks;
- unit tests для `VoxelWorld`, raycast, dirty-neighbor инвариантов и physics/walk glue.
- ECS chunk mirror в `src/ecs` сейчас хранит только `rebuildQueued` и `nonAirVoxelCount`, то есть ровно тот summary state, который реально читает debug/world sync path; `min/maxExclusive` не дублируются до появления практического потребителя.

---

## 3. Чего ещё не хватает до честного MVP

Главные пробелы:

- save/load;
- CI и интеграция локального smoke path в автоматический контур;
- внятное разделение mainline MVP и R&D.

---

## 4. Главный курс проекта

Главный путь на ближайшее время:

1. Довести текущий voxel vertical slice до интерактивного MVP.
2. Стабилизировать interaction/rendering/documentation loop.
3. Потом расширяться в save/load, benchmark/profiling tooling и следующий gameplay/debug layer поверх уже существующих ECS и physics slices.

Mainline сейчас не должен ломаться и тормозиться ради:

- `SVO`
- mesh shaders
- больших world-streaming систем
- тяжёлой CA/simulation R&D
- преждевременного bindless/perfect-engine refactor

Эти темы допустимы только как отдельный экспериментальный трек, если они не мешают основному пути.

---

## 5. Источники истины и как их трактовать

- Корневой `TODO.md` — главный живой roadmap.
- `agent/` — постоянная память и статус агента.
- Реальный код и тесты — главный источник фактов о текущем состоянии.
- `legacy/docs/latest/philosophy` — набор инженерных принципов, которыми нужно руководствоваться.
- `legacy/docs/latest/TODO.md` — исторический план, уже устаревший.
- `legacy/docs/latest/architecture/academic/02_mvp_defense_demo.md` — vision и inspiration, но не обязательная траектория реализации.

---

## 6. Инженерные принципы, которые нужно держать в голове

- Performance is a requirement.
- Data > code.
- Explicit control > hidden magic.
- Measure first, optimize second.
- Оптимизация идёт по порядку: данные -> алгоритм -> код.
- Data-oriented решения предпочтительны в hot path.
- В hot path избегать лишних аллокаций, строк, pointer chasing и плохо контролируемых абстракций.
- Сначала рабочий vertical slice, потом усложнение архитектуры.
- Ожидаемые ошибки обрабатывать явно; нарушения инвариантов валить быстро.
- Отладка должна опираться на телеметрию, захваты кадров, validation и тесты, а не только на брейкпоинты.

---

## 7. Практические правила для агента

- Каждый заметный сеанс начинается с чтения `TODO.md`, `agent/memory.md` и `agent/status.md`.
- Каждый заметный сеанс заканчивается обновлением `TODO.md` и `agent/status.md`.
- Если в ходе работы родилась новая полезная идея, её нужно добавить в `TODO.md`, а не держать только в переписке.
- Если принято решение, которое переживёт текущую задачу, его нужно записать в `agent/decisions.md`.
- Если новая идея затрагивает архитектуру, нужно явно определить: это `mainline`, `extension` или `R&D`.

---

## 8. Технический статус, который уже подтверждён

Подтверждённое ранее состояние:

- тесты `ctest` проходили в debug-конфигурациях после сборки тестового таргета;
- `README.md` уже был изменён в рабочем дереве;
- `external/tracy` синхронизирован с upstream HEAD на `2026-04-07` и очищен от локальных ad-hoc правок внутри сабмодуля;
- `external/fmt` добавлен как git submodule и подключён в root CMake через `add_subdirectory`, но пока не навязывает новый logging layer;
- `JoltPhysics` на `2026-04-07` подключён в build через `external/JoltPhysics/Build`; для текущего `clang-cl` debug toolchain
  он должен собираться с `USE_STATIC_MSVC_RUNTIME_LIBRARY=OFF`, иначе проект и библиотека расходятся по CRT;
- clean submodules в `external/`, включая уже очищенный `external/tracy`, были обновлены до upstream HEAD на `2026-04-07`;
- `external/draco` на `2026-04-07` не bump'ался на новый upstream commit, но был возвращён к clean recursive состоянию: nested `third_party/*` submodules внутри него должны совпадать с gitlinks, записанными самим `draco`;
- follow-up warning cleanup на `2026-04-07` теперь считается полноценной инженерной работой, а не cosmetic pass: если warning указывает на
  фальшивую абстракцию, мёртвую ветку или кривую границу ответственности, ProjectV ожидает root-cause refactor, а не suppress/workaround без
  явного согласования с пользователем;
- scene preset build path после `9.1` больше не делит один искусственно общий lab-конфиг между всеми сценами: `VoxelWorldConfig` держит только
  общие параметры мира, а `VoxelLab` собирается через dedicated builder path со своим shell config внутри `VoxelWorld.cpp`;
- double-tap `Space` в `InputActions` теперь синтезирует `ToggleWalkCreativeMode` через явный detection path, а не через фальшивый generic
  `TriggerActionPressed(...)`, который на деле имел один-единственный special-case call site;
- HUD panel layout в `DebugHud.cpp` сейчас честно одно-колоночный: left anchor зафиксирован в helper'е, поэтому panel geometry больше не
  притворяется произвольно позиционируемой, если реально весь debug HUD живёт в одном top-left column layout;
- non-`VoxelLab` scene helpers в `VoxelWorld.cpp` больше не держат `switch`-ветки для `VoxelLab`: dedicated builder contract теперь
  зафиксирован и на уровне helper control flow, а не только внешнего dispatch;
- non-`VoxelLab` scene presets теперь не проходят через один enum-параметризованный config helper: у `FlatBenchmark`,
  `TransparencyStress`, `ChunkGrid` и `MeshingStress` свои dedicated config builders, поэтому scene path не плодит interprocedural
  always-true/always-false warning'и вокруг конкретных preset'ов;
- `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1` и `docs/voxel_mvp_smoke_checklist.md` подтверждают рабочий runtime path на `2026-04-07`: `resize -> minimize -> restore -> maximize -> restore -> graceful shutdown`;
- `tools/windows/Invoke-ProjectVFailureProbes.ps1` на `2026-04-07` подтверждает controlled failure path для empty shader override и intentional init failure injection;
- корневой `TODO.md` создан как новый актуальный roadmap;
- после закрытия `8.2` build/test targets используют только корневой include dir `src/`, а legacy-style local includes без module path убраны;
- актуальный root-facing overview на `2026-04-07` живёт в `README_NEW.md`; legacy `README.md` в этой сессии сознательно не трогался по прямому ограничению пользователя;
- authored documentation для mainline MVP на `2026-04-07` распределена по отдельным entry docs в `docs/`, а не по одному
  монолитному README;
- builtin profiling scenes на `2026-04-07` уже не являются backlog-идеей: reproducible scene selection идёт через env var
  `PROJECTV_SCENE_PRESET`, а текущий benchmark contract живёт в `docs/Profiling.md` и `src/debug/Profiling.hpp`;
- при проверке одного и того же `build/windows-clang-debug` нельзя гонять два независимых `cmake --build` параллельно: CMake regeneration может конфликтовать на зависимостях вроде `SDL`;
- локальный toolchain на `2026-04-07`: `clang-cl 21.1.8` + `CMake 4.3.0-rc1` + `Ninja 1.13.2` + MSVC STL/Windows SDK include path;
- в этом окружении direct `clang-cl` probe подтверждает рабочие named modules, но текущий CMake не включает module scanning для `clang-cl` с `MSVC` frontend variant;
- `import std` в этом окружении не готов: direct probe падает с `module 'std' not found`, а CMake `Clang-CXX-CXXImportStd.cmake` поддерживает только `libc++` и `libstdc++`.

Это означает:

- нельзя бездумно трогать чужие изменения в `README.md`;
- нельзя без явной причины откатывать обновлённый набор `external/*` submodules без повторной проверки сборки;
- при любом новом изменении roadmap нужно синхронизировать, а не заводить параллельный план в другом месте.
