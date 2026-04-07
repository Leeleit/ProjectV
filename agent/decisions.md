# Decisions

Живые инженерные договорённости. Roadmap живёт в `TODO.md`, общий протокол — в `AGENTS.md`.

---

## 2026-04-07

### Границы `agent/`

Решение:

- `TODO.md` хранит roadmap и приоритеты.
- `AGENTS.md` хранит обязательный протокол.
- `AGENTS.md` не пересказывает roadmap, статус проекта и длинный исторический журнал.
- `agent/memory.md` хранит только долговечные repo-specific факты и ограничения.
- `agent/status.md` хранит только короткий active snapshot.
- `agent/decisions.md` хранит только ещё действующие контракты, а не подробный журнал всех прошлых шагов.

Почему:

- дубль roadmap/protocol раздувает обязательное чтение каждой сессии и увеличивает цену контекста без новой информации.

### Mainline vs R&D

Решение:

- mainline = reproducible interactive voxel MVP;
- `SVO`, mesh shaders, heavy simulation, big-world systems, complex editor и похожие темы живут только как отдельный R&D backlog.

Почему:

- ближайшая ценность проекта — демонстрируемый и измеримый MVP, а не новый фундамент.

### Root-cause fixes

Решение:

- warning cleanup и runtime cleanup лечат причину: мёртвую ветку, ложную абстракцию, скрытый control flow или кривой contract;
- suppress/workaround допустим только по явному согласованию с пользователем.

Почему:

- в этом проекте warning noise обычно указывает на реальную структурную проблему, а не на косметику.

### Control-mode contract

Решение:

- `creative` = collision-backed flight/edit mode;
- `creative` подчиняется `pause`, потому что движение идёт через physics character;
- `spectator` = observe-only noclip без world edits и без подчинения `pause` для camera movement/look;
- `walk` = grounded physics mode;
- double-tap `Space` переключает только `creative <-> walk`;
- `F4` остаётся общим циклом `creative -> spectator -> walk`;
- переход в `walk` сохраняет текущую позицию, а ground recovery используется только как fallback.

Почему:

- режимы должны быть явными и предсказуемыми во время реальной интеракции с миром;
- если режим объявлен как physics-backed, он не должен получать special-case movement мимо paused physics, а noclip-наблюдение наоборот не должно замораживаться вместе с миром.

### Repro contract

Решение:

- perf baseline и scene switching строятся на builtin presets через `PROJECTV_SCENE_PRESET`, а не на editor/config stack;
- smoke и failure checks должны оставаться scriptable и env-driven.

Почему:

- проекту нужна воспроизводимая проверка и измеримость уже сейчас, до save/load и editor layer.

### Build/toolchain contract

Решение:

- mainline пока не мигрирует на C++ named modules;
- `RuntimeDiagnostics` остаётся явным logging/check слоем без bool-returning logger helpers.

Почему:

- текущий `clang-cl + CMake + MSVC STL` стек не даёт безопасной module migration и плохо переносит hidden control flow.

### Build/automation contract

Решение:

- mainline repeatable build path живёт на двух preset'ах: `windows-clang-debug` для локальной разработки и `windows-clang-debug-ci` для automation/CI;
- repeatable automation loop фиксируется через CMake build/test presets и `tools/windows/Invoke-ProjectVBuildChecks.ps1`, а не через ad-hoc набор локальных команд;
- runtime smoke остаётся Windows GUI-проверкой, но поднимается как явный target `ProjectVRuntimeSmoke` поверх существующего PowerShell script;
- `windows-clang-debug-tracy-profiler` остаётся opt-in tooling preset и не должен блокировать основной CI contour;
- shader compile path принимает `glslc` или `glslangValidator`, чтобы mainline не зависел от одного конкретного имени Vulkan SDK tool.

Почему:

- build hygiene нужна mainline MVP прямо сейчас, а profiler-side FetchContent graph не должен быть обязательным проходом для каждого изменения;
- один scriptable entrypoint для configure/build/tests уменьшает drift между локальной проверкой и CI;
- smoke должен оставаться воспроизводимым и вызываемым из build system, но windowed runtime automation пока всё ещё отдельный слой относительно headless CI;
- fallback на `glslangValidator` убирает ненужную хрупкость вокруг конкретного shader compiler binary name.

### Meshing visibility contract

Решение:

- локальный voxel edit помечает dirty только для своего chunk и реально затронутых boundary-neighbors, а не для blanket `3x3x3`;
- chunk frustum/distance visibility обновляется каждый кадр в CPU frame-prep через indirect-команды;
- frustum visibility для чанков проверяет не только центр+радиус, а консервативный chunk AABB против near/left/right/top/bottom planes, чтобы геометрия не исчезала на краях экрана;
- CPU-side patch/full upload chunk descriptors сохраняет GPU-сгенерированные `drawRanges` counts; `sceneUploadVersion` нужен для layout/static descriptor changes, а не для каждого voxel edit;
- compute meshing всё равно домешивает dirty chunks даже вне кадра и затем перезаписывает их draw commands с учётом текущей visibility.

Почему:

- visibility зависит от камеры каждый кадр, а `drawRanges` должны оставаться в sync с voxel payload независимо от того, виден chunk сейчас или нет;
- side-plane тест вида `abs(center) <= depth * tan + sphereRadius` оказался слишком агрессивным на наклонённых ракурсах; chunk-level culling должен предпочитать plane-vs-AABB проверку;
- если CPU reupload сотрёт face counts у не-dirty чанков, текущий indirect path начнёт мигать на edit'ах, потому что compute в этот кадр домешивает только dirty subset;
- это уменьшает rebuild cost без возврата border cracks и без stale mesh state, когда ранее culled chunk снова попадает в кадр.

### Debug HUD layout contract

Решение:

- HUD panels не полагаются на fixed width для legend/stats строк; panel width считается от реально измеренного текста с учётом glyph advance и shadow offset.
- Вертикально стэкнутые stats/helper panels используют общую stack width по максимальной из двух content-driven ширин, а не две независимые правые границы.

Почему:

- helper legend уже содержит строки длиннее старой константы `244px`, и фиксированная ширина визуально выталкивает текст за рамку даже при корректном screen-space positioning;
- content-driven sizing сохраняет лёгкий CPU-built HUD path без перехода на полноценную UI-систему;
- разные независимые ширины у верхней и нижней панели дают рваный силуэт в одном и том же HUD stack, хотя информационно это один блок.

### Debug editor contract

Решение:

- debug editor остаётся lightweight keyboard-driven слоем поверх текущего interaction path, а не отдельным editor/UI framework;
- `OFF` сохраняет старый `LMB remove / RMB place` contract;
- `F8` циклически переключает `OFF -> PAINT -> ERASE -> FILL -> INSPECT`;
- `F9` включает global `chunk bounds`, а `F10` — `dirty chunk overlay`;
- `INSPECT` использует тот же CPU `VoxelRaycast` и chunk metadata из `VoxelWorld`, а не отдельный selection/scene graph.

Почему:

- это закрывает `10.4` без раннего входа в сложный editor stack и без конфликта с mainline MVP;
- keyboard-driven path легко держать reproducible, testable и совместимым с текущим HUD/overlay/render pipeline;
- сохранение `OFF` как дефолта не ломает уже существующий remove/place runtime contract.

### World snapshot contract

Решение:

- save/load пока сериализует только `VoxelWorld` truth: preset id, config, world bounds, voxel payload и `editVersion`;
- binary snapshot header держит reserved-поля внутри layout для будущего versioning; writer обязан явно zero-init'ить их, а reader до format bump обязан отклонять non-zero reserved bytes;
- camera state, control mode, physics internals и GPU scene resources в snapshot не входят;
- `F6/F7` работают по пути из `PROJECTV_SNAPSHOT_PATH`, а fallback — `ProjectV.snapshot.bin` рядом с executable;
- load snapshot проходит через тот же явный ECS/render/physics reload path, что и reload scene preset, и сбрасывает камеру.

Почему:

- ближайшая практическая ценность — persistence мира для mainline MVP, а не полный session/savegame stack;
- сериализация только source-of-truth слоя не плодит дубли между CPU world, physics и GPU staging;
- reset камеры и полный reload path убирают скрытые зависимые состояния после подмены мира.

### Material and scene-lighting contract

Решение:

- material response живёт в CPU-authored `VoxelMaterialVisual` table, а не в hardcoded числах внутри `voxel.frag`;
- scene-wide lighting/fog/sun параметры живут в отдельном CPU-authored `VoxelSceneLighting` buffer, который выбирается по `VoxelScenePreset`;
- текущий `VoxelScenePreset` задаёт и world geometry, и reproducible visual look;
- стекло остаётся единственным transparent voxel material в текущем meshing path, а `Fluid` пока не переводится в transparent/sorted pipeline и получает richer look через lighting/fresnel/emissive terms.

Почему:

- это закрывает `10.3` без раннего перехода к asset/editor stack и без data-driven material system, которая пока не нужна mainline MVP;
- CPU-authored visual contract легче проверять через tests/smoke/docs, чем набор shader-only magic numbers;
- transparent sorting остаётся отдельной render-quality задачей и не должен блокировать базовое улучшение материалов уже сейчас.
