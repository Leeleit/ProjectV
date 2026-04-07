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
- explicit control modes поверх камеры: `free-fly` как debug/tool mode с движением и edits даже при `pause`, и `spectator` как observe-only mode без `remove/place`, который подчиняется `pause`;
- явный `InputActions` слой поверх SDL keyboard input с bindable scancode slots, edge-triggered hotkeys и default controls: `WASD + Space/Shift`, `Ctrl` boost, `Alt` slow, `F1` HUD, `F2` cycle placement material, `F3` reset camera, `F4` control mode toggle, `P` pause, `Tab` mouse capture toggle;
- CPU voxel raycast от камеры;
- runtime block picking и remove/place interaction через `VoxelWorld::SetVoxelMaterial`;
- block highlight и crosshair через отдельный debug overlay graphics pipeline;
- in-app debug HUD через отдельный graphics pipeline и CPU-built vertex buffer без `imgui`;
- корректное dirty-neighbor обновление чанков при редактировании блока у границы;
- reproducible Windows runtime smoke path для `resize / minimize / restore / maximize / graceful shutdown` и отдельный smoke checklist в `docs/`;
- controlled failure probes для missing shader и incomplete init через `PROJECTV_SHADER_BASE_DIR`, `PROJECTV_FAIL_INIT_STAGE` и `tools/windows/Invoke-ProjectVFailureProbes.ps1`;
- минимальный `RuntimeDiagnostics` layer на `fmt` для unified runtime error logs и `PV_CHECK_OR_RETURN` / `PV_ASSERT` в bootstrap/render/pipeline path;
- физически структурированное `src/` с каталогами `app/`, `core/`, `platform/`, `render/`, `render/vulkan/`, `voxel/` и `debug/`;
- project и test targets теперь используют только корень `src/` как include boundary; внутренние project headers подключаются qualified-путями вида `app/Camera.hpp`, `core/Types.hpp`, `render/vulkan/VulkanInit.hpp`;
- fixed-step update loop;
- процедурная demo-scene;
- Tracy hooks;
- unit tests для `VoxelWorld`, raycast и dirty-neighbor инвариантов.

---

## 3. Чего ещё не хватает до честного MVP

Главные пробелы:

- save/load;
- walk / noclip layer поверх уже существующих `free-fly` и `spectator` modes;
- CI и интеграция локального smoke path в автоматический контур;
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
- `external/tracy` синхронизирован с upstream HEAD на `2026-04-07` и очищен от локальных ad-hoc правок внутри сабмодуля;
- `external/fmt` добавлен как git submodule и подключён в root CMake через `add_subdirectory`, но пока не навязывает новый logging layer;
- clean submodules в `external/`, включая уже очищенный `external/tracy`, были обновлены до upstream HEAD на `2026-04-07`;
- `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1` и `docs/voxel_mvp_smoke_checklist.md` подтверждают рабочий runtime path на `2026-04-07`: `resize -> minimize -> restore -> maximize -> restore -> graceful shutdown`;
- `tools/windows/Invoke-ProjectVFailureProbes.ps1` на `2026-04-07` подтверждает controlled failure path для empty shader override и intentional init failure injection;
- корневой `TODO.md` создан как новый актуальный roadmap;
- после закрытия `8.2` build/test targets используют только корневой include dir `src/`, а legacy-style local includes без module path убраны;
- актуальный root-facing overview на `2026-04-07` живёт в `README_NEW.md`; legacy `README.md` в этой сессии сознательно не трогался по прямому ограничению пользователя;
- при проверке одного и того же `build/windows-clang-debug` нельзя гонять два независимых `cmake --build` параллельно: CMake regeneration может конфликтовать на зависимостях вроде `SDL`;
- локальный toolchain на `2026-04-07`: `clang-cl 21.1.8` + `CMake 4.3.0-rc1` + `Ninja 1.13.2` + MSVC STL/Windows SDK include path;
- в этом окружении direct `clang-cl` probe подтверждает рабочие named modules, но текущий CMake не включает module scanning для `clang-cl` с `MSVC` frontend variant;
- `import std` в этом окружении не готов: direct probe падает с `module 'std' not found`, а CMake `Clang-CXX-CXXImportStd.cmake` поддерживает только `libc++` и `libstdc++`.

Это означает:

- нельзя бездумно трогать чужие изменения в `README.md`;
- нельзя без явной причины откатывать обновлённый набор `external/*` submodules без повторной проверки сборки;
- при любом новом изменении roadmap нужно синхронизировать, а не заводить параллельный план в другом месте.
