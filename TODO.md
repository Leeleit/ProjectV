# TODO.md

Актуальная дорожная карта `ProjectV`.

Дата фиксации: `2026-04-07`
Статус документа: `живой roadmap`

---

## 0. Режим ведения проекта

Этот файл должен обновляться постоянно.

Правила:

- `TODO.md` — главный живой roadmap проекта.
- После каждой заметной задачи сюда нужно добавлять новые идеи, помечать выполненные пункты и убирать устаревшее.
- Агент не должен хранить важный план только в переписке; если мысль переживает текущую сессию, она должна попасть либо сюда, либо в `agent/`.
- Перед заметной работой агент обязан читать `TODO.md`, `agent/memory.md` и `agent/status.md`.
- После заметной работы агент обязан обновлять `TODO.md` и `agent/status.md`, а при необходимости и `agent/memory.md` / `agent/decisions.md`.
- `AGENTS.md` хранит только протокол работы агента; roadmap и текущий статус проекта не должны жить там как дубликат
  `TODO.md` или `agent/`.
- `agent/` хранит только delta-контекст поверх `TODO.md` и `AGENTS.md`; roadmap и обязательный протокол туда не
  дублируются.
- `legacy/docs/latest/philosophy` — источник инженерных принципов, а не замена актуального roadmap.
- `legacy/docs/latest/TODO.md` и старые академические планы — исторический контекст и источник идей, но не основной план реализации.

Инфраструктура этого режима:

- [x] Создать корневой `AGENTS.md` с обязательным протоколом работы.
- [x] Создать папку `agent/` для постоянной памяти агента.
- [x] Завести `agent/memory.md`, `agent/status.md`, `agent/decisions.md` и `agent/session-checklist.md`.
- [ ] Поддерживать эти файлы в актуальном состоянии на каждой содержательной сессии.

---

## 1. Где проект находится сейчас

### 1.1. Короткий диагноз

`ProjectV` уже **не** является `hello triangle`, но ещё **не** является завершённым MVP воксельного движка.

Текущая стадия проекта:

- `pre-MVP alpha`
- ранний, но уже рабочий `vertical slice`
- хорошая база для MVP-ветки
- ещё не продуктовая демонстрационная версия

### 1.2. Что уже реально есть в коде

- Vulkan bootstrap на `Vulkan 1.4`
- SDL окно, swapchain, resize/recreate path, controlled shutdown
- graphics pipeline + dynamic rendering
- compute pipeline для voxel meshing
- `VoxelWorld` на CPU
- чанки, dirty queue, chunk rebuild bookkeeping
- материалы мира: воздух, стекло, жидкость, белый/серый пол
- procedural demo-scene в духе `Voxel Laboratory`
- минимальный `flecs` ECS slice
- creative / spectator / walk control modes
- MVP physics layer на `JoltPhysics` поверх voxel world
- fixed-step simulation loop
- indirect draw для opaque/transparent проходов
- Tracy profiling hooks
- unit tests для `VoxelWorld`, interaction/physics glue и базовой логики материалов

### 1.3. Что уже показывает проект

На текущий момент проект должен выводить не треугольник, а раннюю voxel-сцену:

- пол
- стеклянную сферу
- жидкость внутри
- полёт камерой по сцене

### 1.4. Чего пока нет или не хватает до настоящего MVP

- save/load мира
- CI и автоматического smoke-контура
- чёткого разделения между mainline MVP и R&D-веткой

### 1.5. Главный вывод

Проект уже дошёл до состояния, когда **самый правильный следующий шаг** не “делать ещё один фундамент”, а:

1. закрыть интерактивный voxel MVP,
2. стабилизировать pipeline,
3. синхронизировать документацию,
4. и только потом расширяться в сторону больших R&D-идей.

---

## 2. Северная звезда проекта

Ближайший честный milestone:

- окно стабильно запускается и закрывается
- есть creative flight, spectator и базовый walk mode с collision
- есть voxel world из нескольких чанков
- можно поставить и удалить блок
- изменённые чанки корректно перестраиваются
- есть базовый block picking / interaction
- есть debug HUD
- есть базовые замеры производительности
- есть reproducible demo scene
- есть понятная документация для запуска, архитектуры и отладки

Когда этот milestone достигнут, проект уже можно считать:

- не “заготовкой”
- не “архитектурным скелетом”
- а ранним прототипом воксельного движка, пригодным для демо, защиты и дальнейшего развития

---

## 3. Базовые принципы

Этот TODO опирается на философию проекта и фиксирует практический режим работы.

### 3.1. Основные правила

- Сначала рабочий вертикальный срез, потом усложнение.
- Сначала видимый результат, потом архитектурная красота.
- Данные важнее иерархий.
- Явный контроль важнее скрытой магии.
- Производительность подтверждается замерами, а не фантазией.
- Сложные подсистемы не должны блокировать MVP.
- Любая крупная идея должна иметь минимальную реализацию.
- Если задачу можно быстро поменять позже, не надо полировать её неделю сейчас.
- Warning cleanup не должен сводиться к заглушкам ради тишины: если анализатор нашёл мёртвую ветку, фальшивую
  generic-обёртку или кривую
  границу ответственности, чинить нужно структуру данных/контракта, а не только симптом.

### 3.2. Практическая интерпретация

- `graphics-based voxel MVP` — главный путь.
- `SVO / mesh shaders / CA / destruction showcase` — отдельный R&D-трек.
- `Flecs`, `Jolt`, `ImGui`, сериализация и asset pipeline добавляются по реальной необходимости.
- Heavy optimization начинается после того, как появляется живой интерактивный MVP и базовые профили.

### 3.3. Definition of Done для любой важной задачи

Задача считается завершённой, только если:

- код собирается в основном debug-профиле;
- приложение запускается;
- нет новых необъяснённых validation errors;
- не ломаются resize / recreate / shutdown;
- есть понятный способ проверить результат;
- при необходимости обновлена документация;
- если задача влияет на hot path, есть хотя бы базовый замер.

---

## 4. Приоритеты

- `P0` — критический путь. Без этого MVP не движется.
- `P1` — очень важно для ближайшего большого шага.
- `P2` — полезно и желательно, но не блокирует основной путь.
- `P3` — расширение, polish, post-MVP или R&D.

Дополнительная шкала оценки:

- `Необходимость` — насколько задача обязательна для ближайшей цели.
- `Целесообразность` — насколько она рациональна именно сейчас.
- `Современность` — насколько решение двигает проект к актуальной архитектуре.
- `Оптимизационная отдача` — насколько хорошо задача улучшает производительность/масштабируемость.

---

## 5. Самые важные проблемы текущего состояния

### 5.1. Документацию всё ещё нужно держать в sync с кодом

Сейчас код ушёл вперёд относительно части старых документов.

Наблюдаемая проблема:

- старые legacy-планы всё ещё местами описывают состояние уровня triangle/foundation;
- актуальный root-facing overview уже живёт в `README_NEW.md`, а `README.md` сознательно не трогается без отдельного
  разрешения пользователя;
- базовый authored-docs набор в `docs/` уже создан, но его теперь нельзя снова оставлять устаревать по мере развития
  mainline.

Следствие:

- новому участнику сложно быстро понять реальное состояние проекта;
- дорожная карта легко уходит в неверную сторону;
- есть риск параллельно реализовывать уже решённые или неактуальные вещи.

### 5.2. Есть интерактивный slice, но ещё не хватает следующего слоя MVP

Проект уже умеет:

- хранить voxel world,
- паковать данные чанков,
- rebuildить dirty chunks,
- мешить faces compute-шейдером,
- рендерить opaque/transparent geometry,
- выбирать блок,
- удалять и ставить блок,
- моментально видеть корректное обновление мира,
- использовать это как основу для physics/debug demo с базовым walk collision.

Но пока ещё не хватает следующего слоя полезности уровня MVP:

- save/load мира,
- следующего gameplay/debug слоя поверх уже существующих interaction + ECS + physics.

### 5.3. Есть сильный соблазн уйти в R&D слишком рано

Самая опасная ловушка проекта:

- снова свернуть в сторону “идеального будущего движка”;
- раньше времени пойти в SVO, mesh shaders, fluids, destruction, bindless-everything;
- и затормозить завершение ближайшего настоящего milestone.

---

## 6. Главный маршрут разработки

### 6.1. Track A — основной путь: интерактивный MVP

Это путь, который должен жить в `mainline`.

Цель:

- довести текущий voxel vertical slice до честного интерактивного MVP;
- сделать проект демонстрируемым и устойчивым;
- только затем расширять его.

### 6.2. Track B — отдельный путь: Vision / R&D

Это путь для экспериментов:

- SVO
- mesh shaders
- fluids
- destruction
- big-world systems
- GPU-driven crazy stuff

Правило:

- Track B не должен ломать и тормозить Track A.
- Если новая идея мешает закрывать MVP, она выносится в отдельную ветку и отдельный backlog.

---

## 7. P0 — критический путь MVP

## 7.1. Стабилизация и синхронизация проекта

Оценка:

- Необходимость: `5/5`
- Целесообразность: `5/5`
- Современность: `3/5`
- Оптимизационная отдача: `2/5`

Задачи:

- [ ] Создать и поддерживать в корне актуальный `TODO.md`.
- [x] Создать актуальный root-facing overview в `README_NEW.md`; `README.md` без отдельного разрешения пользователя не
  трогать.
- [ ] Зафиксировать current milestone и ближайший next milestone.
- [ ] Убрать рассинхрон между корневой документацией и `legacy/docs/latest`.
- [ ] Отдельно обозначить, что является `mainline MVP`, а что `R&D`.
- [x] Описать актуальный smoke checklist:
- configure
- build
- test
- run
- resize
- minimize/restore
- shutdown
- shader missing path

Критерий готовности:

- новый участник может понять, что делает проект сейчас и что надо делать следующим.

## 7.2. Интерактивность мира

Оценка:

- Необходимость: `5/5`
- Целесообразность: `5/5`
- Современность: `4/5`
- Оптимизационная отдача: `4/5`

Задачи:

- [x] Добавить block picking от камеры.
- [x] Добавить raycast по voxel world.
- [x] Добавить выбор целевого блока и соседней позиции.
- [x] Добавить удаление блока.
- [x] Добавить постановку блока.
- [x] Добавить dirty-region update при изменении мира.
- [x] Перестраивать только нужные чанки.
- [x] Корректно отмечать dirty соседние чанки, если изменён блок у границы.
- [x] Добавить визуальное выделение выбранного блока.
- [x] Добавить минимальный crosshair.

Критерий готовности:

- пользователь может летать по сцене, выбирать блоки и изменять мир в runtime без перезапуска.

## 7.3. Debug HUD и видимость состояния проекта

Оценка:

- Необходимость: `5/5`
- Целесообразность: `5/5`
- Современность: `4/5`
- Оптимизационная отдача: `3/5`

Задачи:

- [x] Перенести debug-статистику из title bar в нормальный HUD.
- [ ] Показать:
- FPS
- frame time
- simulation steps
- triangles
- dirty chunks
- active chunks
- non-air voxels
- memory usage
- validation/debug flags
- [x] Сделать toggle HUD по hotkey.
- [x] Добавить overlay для camera position / look direction.
- [x] Добавить overlay для выбранного блока.
- [x] Сделать так, чтобы `F1` скрывал весь debug UI, а не только текстовый HUD.
- [x] Уменьшить HUD и разнести его на отдельные stats/helpers panels вместо одного монолитного блока.
- [x] Сделать crosshair толще и через inverted/XOR-style overlay вместо обычной тонкой белой линии.
- [x] Убрать XOR-артефакт в центре crosshair, сохранив нормальное пересечение `+` без дырки посередине.
- [x] Исправить screen-space ориентацию HUD для Vulkan viewport, чтобы overlay оставался в верхнем левом углу без вертикального переворота.
- [x] Убрать очевидные static-analysis warning'и в HUD hotkey path, test helper'ах и chunk-fixture world setup.

Критерий готовности:

- внутреннее состояние движка читается прямо из приложения без внешней магии.

## 7.4. Стабильность runtime

Оценка:

- Необходимость: `5/5`
- Целесообразность: `5/5`
- Современность: `4/5`
- Оптимизационная отдача: `3/5`

Задачи:

- [x] Перепроверить resize path.
- [x] Перепроверить minimize/restore path.
- [x] Перепроверить shutdown path.
- [x] Добавить reproducible Windows smoke script для `resize -> minimize -> restore -> maximize -> restore -> graceful shutdown`.
- [x] Проверить controlled failure при отсутствии `.spv`.
- [x] Проверить controlled failure при неполной инициализации.
- [x] Добавить reproducible failure-probe script для `PROJECTV_SHADER_BASE_DIR` и `PROJECTV_FAIL_INIT_STAGE`.
- [x] Привести логирование ошибок Vulkan/SDL к единому стилю в bootstrap/render/pipeline/runtime-stability path.
- [x] Добавить helper для `VkResult -> readable message`.
- [x] Добавить минимальные debug asserts/check macros.

Критерий готовности:

- проект не ощущается хрупким при базовых сценариях эксплуатации.

---

## 8. P1 — закрытие MVP в usable-форме

## 8.1. Input actions и режимы управления

Оценка:

- Необходимость: `4/5`
- Целесообразность: `5/5`
- Современность: `4/5`
- Оптимизационная отдача: `2/5`

Статус на `2026-04-07`:

- action-layer и explicit control modes уже закрыты;
- creative flight layout подправлен под более привычный Minecraft-like полёт: `Space/Shift` для вертикали, `Ctrl` для
  ускорения, `Alt` для slow modifier.
- runtime movement теперь действительно держит `WASD` в плоскости `XZ`; высота в `creative`/`spectator` меняется только
  от `Space/Shift`, а не от pitch камеры.
- `double-space` теперь даёт быстрый toggle только между `creative` и `walk`, near-vertical pitch clamp доведён почти до
  `89°`, а `F5` циклически переключает builtin scene presets прямо в рантайме через безопасный world/resource reload
  path.
- `creative` больше не noclip: это physics-backed flight mode с collision, а переход обратно в `walk` сохраняет текущую
  позицию и не телепортирует камеру в центр/на пол, если под ней нет опоры.

Задачи:

- [x] Вынести action mapping слой поверх SDL input.
- [x] Добавить bindable actions.
- [x] Добавить toggle relative mouse mode.
- [x] Добавить debug hotkeys.
- [x] Добавить pause toggle.
- [x] Добавить explicit creative / spectator / walk modes поверх нового action layer.
- [x] Добавить speed modifiers.

## 8.2. Структурирование `src/`

Оценка:

- Необходимость: `4/5`
- Целесообразность: `4/5`
- Современность: `4/5`
- Оптимизационная отдача: `3/5`

Статус на `2026-04-07`:

- Первый practical slice уже выполнен: код разложен по `src/app`, `src/core`, `src/platform`, `src/render`, `src/render/vulkan`, `src/voxel` и `src/debug`.
- `src/CMakeLists.txt` и `tests/CMakeLists.txt` уже переключены на новую физическую раскладку файлов.
- Следующий cleanup slice уже выполнен: project headers теперь подключаются только через qualified include paths от корня `src/`, а transitional include directories из `src/CMakeLists.txt` и `tests/CMakeLists.txt` убраны.
- Документация под новую раскладку уже синхронизирована через `README_NEW.md`, `docs/source_layout.md` и обновлённый smoke checklist; legacy `README.md` при этом сознательно не трогался.
- `src/shaders/` пока оставлен на месте как отдельный follow-up, потому что это не блокирует текущий mainline.

Задачи:

- [x] Выполнить первый перенос в структуру:

```text
src/
  app/
  core/
  platform/
  render/
  render/vulkan/
  voxel/
  debug/
```

- [x] Сохранить low-risk migration path без giant refactor: сначала file moves + CMake include directories, потом semantic cleanup.
- [x] Выделить более узкие responsibility boundaries на уровне физической раскладки файлов.
- [x] Дальше сужать include boundaries и убирать transitional include directories по мере работы над подсистемами.
- [x] Обновить корневую и authored documentation под новую раскладку `src/`.

## 8.3. ECS — минимально и по делу

Оценка:

- Необходимость: `4/5`
- Целесообразность: `4/5`
- Современность: `4/5`
- Оптимизационная отдача: `3/5`

Статус на `2026-04-07`:

- `flecs` уже подключён в main build и test target.
- Минимальный ECS slice уже поднят в `src/ecs`: primary camera entity, primary player entity, `world`/`debug` singleton
  data и chunk mirror entities.
- Main loop уже читает camera/debug/world через ECS glue, но `VoxelWorld` пока сознательно остаётся owned через
  `WorldState`, а ECS держит на него явный binding вместо premature full migration.
- Полный ECS scheduler, gameplay systems и physics-layer по-прежнему не являются частью `8.3`.

Задачи:

- [x] Подключить `flecs` только когда уже есть интерактивный world loop.
- [x] Ввести минимальные сущности:
- camera
- player
- world singleton
- debug singleton
- chunk entity при необходимости
- [x] Не строить большой ECS-каркас “на будущее”.
- [x] Держать компоненты как данные, системы как трансформации.
- [x] Убрать post-`8.3` static-analysis warning'и в `RuntimeDiagnostics`, `InputActions`, HUD helper'ах, voxel raycast и
  ECS chunk mirror.

## 8.4. Physics для MVP

Оценка:

- Необходимость: `4/5`
- Целесообразность: `4/5`
- Современность: `4/5`
- Оптимизационная отдача: `3/5`

Статус на `2026-04-07`:

- `JoltPhysics` уже подключён в main build и test target как vendored dependency.
- В `src/physics` уже поднят минимальный physics slice: static voxel collision world, physics raycast и
  `CharacterVirtual`
  walk controller.
- lifecycle helper для Jolt runtime уже упрощён до non-failing API без фиктивного bool-return, чтобы physics slice не
  плодил unreachable/static-analysis noise.
- Синхронизация physics с voxel edits уже идёт через `VoxelWorld::editVersion`, без отдельного gameplay ownership
  rewrite.
- Control modes теперь образуют practical MVP-тройку: `creative` как collision-backed flight/edit mode, `spectator` как
  observe-only
  noclip mode и `walk` как grounded collision-based player mode поверх того же input/app loop; `creative` подчиняется
  `pause` вместе с physics, а `spectator` остаётся свободной noclip-камерой даже при остановленной симуляции.
- Runtime `remove/place` interaction сознательно остаётся на CPU `VoxelRaycast`; physics усиливает MVP, а не заменяет
  уже
  работающий interaction loop.
- Текущий walk slice всё ещё использует минимальный `CharacterVirtual` без отдельного ground-sticking / ledge-stability
  tuning, поэтому лёгкое скатывание с краёв блоков пока считается ожидаемым follow-up, а не багом mainline-стабильности.

Задачи:

- [x] Подключить `JoltPhysics`.
- [x] Сделать базовый raycast.
- [x] Добавить простую collision-модель игрока.
- [x] Добавить режимы walk / creative / spectator поверх physics slice.
- [x] Связать interaction с world edits.

Важно:

- physics должна усиливать MVP, а не заменять его.

## 8.5. Документация проекта

Оценка:

- Необходимость: `4/5`
- Целесообразность: `5/5`
- Современность: `3/5`
- Оптимизационная отдача: `1/5`

Статус на `2026-04-07`:

- authored-docs entry set уже создан в `docs/`: `ArchitectureGuide`, `RenderArchitecture`, `VoxelWorld`,
  `BuildAndRun` и `Debugging`;
- документация теперь честно описывает текущий interaction/runtime/ECS/physics loop, scene resources, meshing path,
  Tracy, smoke и failure probes;
- `README_NEW.md` и `docs/source_layout.md` уже переключены на эти документы как на основные entry points.

Задачи:

- [x] Создать `docs/ArchitectureGuide.md`.
- [x] Создать `docs/RenderArchitecture.md`.
- [x] Создать `docs/VoxelWorld.md`.
- [x] Создать `docs/BuildAndRun.md`.
- [x] Создать `docs/Debugging.md`.
- [x] Зафиксировать:
- как собирается проект;
- как идёт кадр;
- как устроен `VoxelWorld`;
- как готовятся scene resources;
- как работает meshing path;
- как смотреть Tracy;
- как воспроизводить smoke checks.

---

## 9. P1 — качество, производительность и повторяемость

## 9.1. Профилирование и измеримость

Оценка:

- Необходимость: `4/5`
- Целесообразность: `5/5`
- Современность: `4/5`
- Оптимизационная отдача: `4/5`

Задачи:

- [x] Сделать baseline-сцены для профилирования.
- [x] Зафиксировать набор Tracy-графиков.
- [x] Снимать:
- frame time
- chunk rebuild count
- repacked voxel count
- generated opaque faces
- generated transparent faces
- upload sizes
- [x] Добавить benchmark methodology.
- [x] Описать, как воспроизводить замеры.

## 9.2. Build и automation hygiene

Оценка:

- Необходимость: `4/5`
- Целесообразность: `5/5`
- Современность: `4/5`
- Оптимизационная отдача: `2/5`

Задачи:

- [ ] Довести оба основных CMake preset до полностью повторяемого состояния.
- [ ] Следить, чтобы тесты реально собирались во всех нужных пресетах.
- [ ] Добавить CI хотя бы на:
- configure
- build
- tests
- [ ] Подготовить smoke target/script.
- [ ] Ввести понятные опции:
- `PROJECTV_ENABLE_VALIDATION`
- `PROJECTV_ENABLE_TRACY`
- `PROJECTV_ENABLE_IMGUI`
- `PROJECTV_ENABLE_RENDERDOC_MARKERS`

---

## 10. P2 — осмысленное расширение функциональности

## 10.1. Save/Load и data-driven scene presets

Оценка:

- Необходимость: `3/5`
- Целесообразность: `4/5`
- Современность: `4/5`
- Оптимизационная отдача: `2/5`

Задачи:

- [ ] Добавить сохранение snapshot мира.
- [ ] Добавить загрузку snapshot мира.
- [x] Развести общий `VoxelWorldConfig` и dedicated `VoxelLab` builder, чтобы scene presets не делили один и тот же
  искусственно общий
  конфиг и не плодили мёртвые analyzer-ветки вокруг lab-only геометрии, включая residual `switch`-arms в non-`VoxelLab`
  helper path.
- [x] Убрать псевдо-generic helper `GetNonVoxelLabWorldConfig(scenePreset)`: каждый non-`VoxelLab` preset теперь держит
  свой dedicated
  config builder, чтобы interprocedural analyzer не видел ложные always-true/always-false ветки по enum-dispatch.
- [x] Сделать несколько сцен как builtin presets через `PROJECTV_SCENE_PRESET`:
- `VoxelLab`
- `FlatBenchmark`
- `TransparencyStress`
- `ChunkGrid`
- `MeshingStress`

## 10.2. Улучшение meshing path

Оценка:

- Необходимость: `3/5`
- Целесообразность: `4/5`
- Современность: `4/5`
- Оптимизационная отдача: `5/5`

Задачи:

- [x] Доделать текущий face culling path до уверенного production-like состояния.
- [x] Добавить нормальную обработку border cases между чанками.
- [x] Рассмотреть greedy meshing как следующий шаг: пока оставляем его отдельным follow-up после visibility/culling,
  потому что текущий transparent/material split всё ещё опирается на per-face path.
- [x] Добавить frustum/distance culling чанков.
- [x] Сделать frustum culling консервативным на краях экрана, чтобы chunk AABB не отрезался раньше видимой геометрии.
- [x] Зафиксировать upload path chunk descriptors так, чтобы voxel edit не обнулял draw commands у не-dirty чанков.
- [x] Уменьшить число rebuild'ов при локальном изменении мира.

## 10.3. Материалы и визуальная выразительность

Оценка:

- Необходимость: `2/5`
- Целесообразность: `4/5`
- Современность: `4/5`
- Оптимизационная отдача: `2/5`

Задачи:

- [ ] Сделать более осмысленный material pipeline.
- [ ] Улучшить стекло.
- [ ] Улучшить жидкость.
- [ ] Вынести lighting/material parameters из hardcode.
- [ ] Добавить scene presets с разным освещением.

## 10.4. Debug editor mode

Оценка:

- Необходимость: `2/5`
- Целесообразность: `4/5`
- Современность: `4/5`
- Оптимизационная отдача: `2/5`

Задачи:

- [ ] Режим paint.
- [ ] Режим erase.
- [ ] Режим fill.
- [ ] Режим inspect.
- [ ] Отображение chunk bounds.
- [ ] Отображение dirty chunk overlay.

---

## 11. P2 — зависимости, которые надо вводить по факту пользы

### В ближайшей перспективе

- [x] `fmt` — vendored formatting dependency для будущего logging layer без ввода тяжёлого logging framework.
- [x] `tracy` — bundled submodule синхронизирован с upstream HEAD и очищен от локальных ad-hoc patch'ей внутри `external/tracy`.
- [x] `flecs`
- [x] `JoltPhysics`
- [ ] `imgui`
- [ ] `glaze` — только когда схема данных стабилизируется

### Позже, когда появится практический кейс

- [ ] `fastgltf`
- [ ] `meshoptimizer`
- [ ] `draco`
  - before actual adoption, `external/draco` must stay recursively clean: nested `third_party/*` submodules should match
    the commits recorded by `draco`, not random local checkouts.

### Не тащить в mainline слишком рано

- [ ] `RmlUi`
- [ ] `miniaudio`
- [ ] `freetype`
- [ ] `stdexec`
- [ ] сложный plugin/mod API

---

## 12. Что не делать слишком рано

- [ ] Не строить “идеальный движок” раньше интерактивного MVP.
- [ ] Не уходить в SVO как обязательное условие первого зрелого демо.
- [ ] Не делать mesh shaders обязательным путём mainline.
- [ ] Не строить полную ECS-архитектуру без реального gameplay loop.
- [ ] Не начинать тяжёлую физику раньше стабильного world interaction.
- [ ] Не тащить в MVP мультиплеер, AI, сложный UI, аудио и полноценный editor.
- [ ] Не оптимизировать гипотетические bottleneck'и без Tracy и повторяемых сцен.
- [ ] Не размазывать ответственность по команде без owner'ов подсистем.

---

## 13. Ближайший спринт

Если нужен ответ на вопрос “что делать прямо сейчас”, то делать вот это.

### Sprint A — добить интерактивный voxel MVP

- [x] Актуализировать корневую документацию.
- [x] Добавить block picking.
- [x] Добавить удаление блока.
- [x] Добавить постановку блока.
- [x] Добавить correct dirty-neighbor handling.
- [x] Добавить block highlight + crosshair.
- [x] Вынести debug stats в HUD.
- [x] Проверить resize / restore / shutdown.
- [x] Подготовить smoke checklist.

### Sprint A — результат

- проект перестаёт быть просто “рендером красивой сцены”;
- появляется реальное взаимодействие с voxel world;
- база становится готовой для ECS, physics и дальнейшего growth.

### Sprint B — после Sprint A

- [x] Подключить минимальный ECS.
- [x] Подключить MVP physics raycast/collision.
- [ ] Добавить save/load.
- [x] Добавить benchmark scene presets.
- [x] Начать authored docs в `docs/`.

---

## 14. Карта возможностей проекта

Ниже не строгий порядок реализации, а большая карта того, что вообще осмысленно делать в `ProjectV`.

### 14.1. Core / App / Tooling

- [ ] App layer
- [ ] config system
- [ ] logging system
- [ ] C++ named modules pilot — только после released CMake с рабочим `clang-cl` module scanning или после осознанной смены toolchain; в текущем `clang-cl + MSVC STL` окружении `import std` не готов.
- [ ] assert/check layer
- [ ] filesystem abstraction
- [ ] project settings persistence
- [ ] CI
- [ ] smoke automation
- [ ] benchmark automation
- [x] reproducible debug scenes

### 14.2. Platform / Input / UX

- [x] action mapping
- [x] rebindable controls
- [x] mouse capture toggle
- [x] debug hotkeys
- [x] spectator mode
- [x] walk / creative / spectator modes
- [ ] screenshot hotkey
- [ ] frame-step / slow-motion debug modes

### 14.3. Camera / Space / Interaction

- [ ] creative flight polish
- [ ] player controller
- [x] block picking
- [x] block interaction
- [ ] walk controller polish: ground sticking / edge-slide tuning
- [ ] inspect tools
- [ ] gizmo/debug overlays

### 14.4. Voxel World

- [ ] richer chunk model
- [ ] chunk neighbors bookkeeping
- [x] dirty region tracking
- [x] scene presets
- [ ] world snapshots
- [ ] world editing tools
- [ ] multi-material test scenes
- [ ] chunk-level fixtures for tests

### 14.5. Rendering

- [ ] stable chunk rendering
- [ ] transparent pass quality
- [x] frustum culling
- [x] distance culling
- [ ] greedy meshing
- [ ] visual debug modes
- [ ] render stats
- [ ] RenderDoc-friendly markers

### 14.6. Debug / Profiling

- [x] in-app HUD
- [x] HUD panel autosizing / bounds safety
- [x] HUD stacked panel width alignment
- [x] Tracy metrics pack
- [ ] per-pass timings
- [ ] chunk update timings
- [ ] upload bandwidth metrics
- [ ] validation status reporting
- [x] debug scene toggles

### 14.7. Gameplay / MVP Extensions

- [x] ECS glue
- [x] physics raycast
- [x] collision
- [x] walk controller
- [ ] simple sandbox interactions
- [ ] debug tools for world mutation

---

## 15. Отдельный backlog красивых и крутых механик

Это не критический путь, но это хорошие идеи, которые могут реально сделать проект интереснее.

### 15.1. Sandbox-механики

- [ ] разные типы блоков с поведением
- [ ] хрупкое стекло
- [ ] сыпучие материалы
- [ ] простая жидкость
- [ ] basic heat / fire / cooling interactions
- [ ] “пушка” для разрушения блоков
- [ ] debug brush-инструменты
- [ ] режим “лаборатории материалов”

### 15.2. Визуальные штуки

- [ ] мягкое небо / gradient sky
- [ ] volumetric-like fog lite
- [ ] хорошие материалы стекла
- [ ] вода с лучшим shading
- [ ] day/night preset transitions
- [ ] разные mood-предустановки сцены
- [ ] stylized tech-lab visual direction

### 15.3. Демо-сценарии

- [ ] `Voxel Laboratory`
- [ ] glass dome arena
- [ ] checkerboard hall
- [ ] chunk stress field
- [ ] transparency stress scene
- [ ] physics interaction room
- [ ] destruction playground

---

## 16. Уголок хардкорных оптимизаций и экспериментов

Это **отдельный R&D-угол**, а не основной маршрут.

Сюда складываются идеи, которые могут дать огромный технологический буст, но не должны тормозить mainline MVP.

## 16.1. Рендер-эксперименты высокого уровня

- [ ] `SVO` как отдельное представление мира
- [ ] GPU-driven rendering без классического CPU-oriented draw preparation
- [ ] mesh shaders prototype
- [ ] compute-only visibility pipeline
- [ ] visibility buffer / material resolve path
- [ ] bindless resource model
- [ ] cluster / meshlet / chunklet rendering
- [ ] software occlusion culling prototype
- [ ] hierarchical Z / occlusion experiment
- [ ] ray query / hybrid visibility experiments

## 16.2. Hardcore voxel representations

- [ ] dense chunks vs compressed chunks benchmark
- [ ] bit-packed voxel payload variants
- [ ] palette-compressed chunk materials
- [ ] run-length encoded chunk layers
- [ ] Morton/Z-order storage experiments
- [ ] sparse chunk residency
- [ ] clipmap / brick hierarchy
- [ ] hybrid dense-near / sparse-far world

## 16.3. Низкоуровневые CPU-оптимизации

- [ ] SoA-представление горячих данных
- [ ] chunk iteration order tuning под cache locality
- [ ] SIMD для rebuild/packing path
- [ ] branch-reduction в hot loops
- [ ] false-sharing audit
- [ ] align/padding audit
- [ ] prefetch-friendly data traversal
- [ ] arena/transient allocators для кадра
- [ ] small-buffer optimization для временных контейнеров
- [ ] string-free hot path

## 16.4. Низкоуровневые GPU-оптимизации

- [ ] device-local scene buffers вместо упора в host-mapped path
- [ ] staging upload path
- [ ] partial uploads по dirty ranges
- [ ] async compute для meshing
- [ ] compute compaction / prefix sums
- [ ] indirect command generation на GPU
- [ ] descriptor buffer / descriptor indexing experiments
- [ ] barrier minimization audit
- [ ] occupancy / wave-size profiling
- [ ] transparent pass alternatives

## 16.5. Streaming и большие миры

- [ ] background chunk streaming
- [ ] region file format
- [ ] async IO pipeline
- [ ] chunk residency manager
- [ ] origin shifting
- [ ] fixed-point / precision strategy for large worlds
- [ ] near/far world split
- [ ] LOD for chunk meshes

## 16.6. Simulation / Systems R&D

- [ ] job system
- [ ] task graph
- [ ] deterministic fixed-step world pipeline
- [ ] CA liquids
- [ ] CA fire / smoke / sand
- [ ] voxel destruction pipeline
- [ ] debris bridge to rigid bodies
- [ ] fracture-like chunk splitting

## 16.7. Хардкорный debug/tooling stack

- [ ] frame capture presets
- [ ] automated RenderDoc captures
- [ ] microbenchmark harness
- [ ] per-commit perf regression tracking
- [ ] memory budgeting dashboard
- [ ] chunk heatmap overlays
- [ ] GPU marker taxonomy

### 16.8. Правило для этого раздела

Любая задача из `Hardcore` раздела должна сначала ответить на 5 вопросов:

1. Что она даёт проекту практически?
2. Нужна ли она сейчас mainline MVP?
3. Как выглядит минимальный эксперимент?
4. Как измерить результат?
5. Что она рискует сломать в текущем working build?

Если ответы слабые, задача остаётся в R&D и не лезет в критический путь.

---

## 17. Итоговая стратегия

Правильная стратегия проекта на ближайшее время:

1. не расползаться;
2. добить интерактивный voxel MVP;
3. стабилизировать сборку, тесты и документацию;
4. сделать проект демонстрируемым и измеримым;
5. только потом расширяться в сторону ECS, physics, editor-like tooling и тяжёлых R&D-идей.

Главный ориентир:

> Не “самый умный движок на бумаге”, а живой, запускаемый, измеримый и расширяемый voxel prototype.
