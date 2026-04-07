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
- free-fly camera;
- CPU voxel raycast от камеры;
- runtime block picking и remove/place interaction через `VoxelWorld::SetVoxelMaterial`;
- block highlight и crosshair через отдельный debug overlay graphics pipeline;
- in-app debug HUD через отдельный graphics pipeline и CPU-built vertex buffer без `imgui`;
- корректное dirty-neighbor обновление чанков при редактировании блока у границы;
- fixed-step update loop;
- процедурная demo-scene;
- Tracy hooks;
- unit tests для `VoxelWorld`, raycast и dirty-neighbor инвариантов.

---

## 3. Чего ещё не хватает до честного MVP

Главные пробелы:

- save/load;
- CI и reproducible smoke path;
- актуальная authored documentation в `docs/`;
- внятное разделение mainline MVP и R&D.

---

## 4. Главный курс проекта

Главный путь на ближайшее время:

1. Довести текущий voxel vertical slice до интерактивного MVP.
2. Стабилизировать interaction/rendering/documentation loop.
3. Потом расширяться в ECS, physics, save/load и tooling.

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
- `external/tracy` уже был помечен как изменённый submodule;
- корневой `TODO.md` создан как новый актуальный roadmap.

Это означает:

- нельзя бездумно трогать чужие изменения в `README.md`;
- нельзя случайно "починить" состояние `external/tracy` откатом;
- при любом новом изменении roadmap нужно синхронизировать, а не заводить параллельный план в другом месте.
