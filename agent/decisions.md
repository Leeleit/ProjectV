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
