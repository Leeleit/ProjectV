# Decisions

Краткий журнал решений, которые должны переживать отдельные сессии.

---

## 2026-04-07

### Источник истины для планирования

Решение:

- главным roadmap считается корневой `TODO.md`;
- `agent/` хранит постоянную память агента;
- legacy-планы больше не считаются главным планом реализации.

Почему:

- legacy-документы уже расходятся с реальным состоянием кода;
- проекту нужен один живой backlog, а не несколько конкурирующих.

### Главный путь проекта

Решение:

- mainline проекта — интерактивный graphics-based voxel MVP;
- большие эксперименты живут отдельно как R&D backlog.

Почему:

- проект уже имеет работающий vertical slice;
- ближайшая наибольшая ценность — не новый фундамент, а живая интерактивность и стабильная демонстрация.

### Обязательная постоянная память агента

Решение:

- в репозитории создаётся папка `agent/`, и агент обязан читать и обновлять её на каждой заметной сессии.

Почему:

- без этого агент будет терять контекст между задачами;
- статус проекта, инженерные решения и рабочий фокус должны переживать один ответ и одну сессию.

### Философия внедрения новых систем

Решение:

- новые зависимости, подсистемы и архитектурные слои добавляются только по факту практической пользы;
- философия проекта применяется прагматично, а не догматично.

Почему:

- проекту нужен живой MVP, а не коллекция заготовленных абстракций;
- даже хорошие идеи вредны, если появляются раньше своего времени.

### Интерактивное редактирование мира

Решение:

- интерактивный mainline использует отдельный CPU `VoxelRaycast` на плотном `VoxelWorld` и явный `InteractionState`;
- runtime remove/place block идёт напрямую через `SetVoxelMaterial`, без нового gameplay/framework слоя;
- один voxel edit помечает dirty регион `3x3x3`, чтобы rebuild гарантированно захватывал соседние чанки на границах.

Почему:

- это закрывает честный interactive slice без нового renderer/gameplay refactor;
- явный `InteractionState` сохраняет данные selection/placement для следующего шага с highlight/HUD;
- border-safe dirty marking важнее микроэкономии на этом этапе, потому что устраняет визуально неверные chunk rebuild'ы на границах.

### Visual feedback для interaction

Решение:

- block highlight и crosshair реализуются отдельным debug overlay pipeline поверх основного voxel pass;
- selection прокидывается в `FrameRenderData`, а overlay-геометрия генерируется в shader'е из `gl_VertexIndex`, без новых vertex/index buffers и без вмешательства в meshing path.

Почему:

- это даёт видимый feedback для picking, не трогая packed scene buffers и compute meshing;
- overlay path хорошо переживает swapchain recreate и остаётся изолированным от main voxel rendering;
- для mainline MVP это дешевле и безопаснее, чем встраивать selection-highlight в основной voxel material/render path.

### Debug HUD без `imgui`

Решение:

- debug HUD реализуется как фиксированный read-only overlay поверх существующего voxel/debug pass, без подключения `imgui`;
- текст HUD собирается на CPU в host-mapped vertex buffer из встроенного bitmap/stroke-подобного шрифта и рисуется отдельным graphics pipeline;
- HUD использует уже существующие `DebugStats`, `CameraState` и `InteractionState`, плюс hotkey toggle, вместо новой UI-системы.
- screen-space координаты HUD мапятся в Vulkan NDC с учётом обычного positive-height viewport, поэтому верх экрана соответствует отрицательному `Y` в NDC.

Почему:

- это закрывает ближайший mainline milestone из `TODO.md` без ввода новой тяжёлой зависимости;
- отдельный HUD pipeline не вмешивается в meshing path и легко переживает swapchain recreate;
- для текущего объёма UI фиксированный overlay проще, дешевле и честнее, чем тащить editor/debug framework раньше времени.
- это убирает перевёрнутый/bottom-anchored HUD без ввода negative viewport или отдельной матрицы для overlay.

### Vendoring `fmt` и обновление submodules

Решение:

- `fmt` добавляется как обычный git submodule в `external/fmt` и подключается в root CMake через `add_subdirectory`, как и другие реально используемые зависимости;
- `fmt` интегрируется без module-target, чтобы не тащить CMake/C++20 modules проблемы в текущий toolchain;
- массовое обновление third-party делается только для clean submodules; если bundled submodule грязный, его сначала нужно явно очистить, и только потом bump'ать.

Почему:

- проекту полезен лёгкий typed-formatting backend для будущего logging layer, но пока не нужен тяжёлый logging framework;
- отключение `FMT_MODULE` сохраняет современную версию `fmt`, не ломая текущую сборку на `clang-cl` + Ninja;
- `ProjectV` не должен зависеть от скрытых локальных one-off патчей внутри vendored profiler submodule; `external/tracy` после очистки и bump'а должен оставаться чистым и проверяемым через обычный build/test/smoke проход.

### Swapchain recreate при transient `0x0` extent

Решение:

- `RecreateSwapchain` не должен уничтожать graphics/depth resources до тех пор, пока surface действительно не подтвердил ненулевой pixel extent;
- если lifecycle-событие даёт временный `0x0` extent во время `minimize/restore`, старый graphics pipeline сохраняется, а полноценный destroy/recreate откладывается до следующего валидного размера;
- runtime smoke для `resize -> minimize -> restore -> maximize -> restore -> graceful shutdown` фиксируется отдельным Windows script и checklist'ом в `docs/`.

Почему:

- ранний destroy pipeline на transient `0x0` surface ломал recreate через `CreateDepthResources`, потому что depth image пытался создаться для нулевого extent;
- сохранение старого pipeline до подтверждённого ненулевого размера делает window lifecycle устойчивым без лишней хрупкости;
- reproducible smoke path важнее ручных догадок и позволяет повторяемо проверять `7.4` после дальнейших изменений render/runtime слоя.

### Runtime diagnostics и controlled failure probes

Решение:

- для bootstrap/render/pipeline runtime-path вводится минимальный `RuntimeDiagnostics` слой с единым форматом логов `[ProjectV][Subsystem][Step] ...`, плюс `PV_CHECK_OR_RETURN` и `PV_ASSERT`;
- missing shader проверяется не через ручные переименования `.spv`, а через `PROJECTV_SHADER_BASE_DIR`, который переопределяет shader search base directory;
- intentional incomplete init проверяется не через временную порчу кода, а через `PROJECTV_FAIL_INIT_STAGE`, который позволяет воспроизводимо оборвать `InitVulkan` на явных стадиях;
- runtime smoke и failure probes фиксируются отдельными Windows scripts в `tools/windows/`.

Почему:

- unified runtime diagnostics даёт читаемый stderr в failure probes и убирает разношёрстные `SDL_Log`-сообщения в критическом init/render path;
- env-driven probes воспроизводимы, не мутируют build outputs и не требуют ручного вмешательства в рабочее дерево;
- минимальные checks/asserts полезнее сейчас, чем полноценный logging framework, потому что закрывают `7.4` без отдельного архитектурного рефактора.

### Постепенное структурирование `src/`

Решение:

- первый `8.2` slice делает физическую раскладку `src/` по зонам ответственности: `app/`, `core/`, `platform/`, `render/`, `render/vulkan/`, `voxel/`, `debug/`;
- в этом же проходе не делается массовый rewrite include-строк; совместимость сохраняется через `target_include_directories` в `src/CMakeLists.txt` и `tests/CMakeLists.txt`;
- `src/shaders/` пока остаётся на старом месте до отдельной практической необходимости.

Почему:

- это даёт более явные responsibility boundaries уже сейчас, не смешивая file moves с semantic refactor;
- такой bridge сохраняет рабочую сборку и снижает риск поломать mainline на ровном месте;
- это соответствует TODO: сделать постепенный перенос, а не гигантский rewrite всего `src/` одним махом.

### Последовательный build/run для одного build tree

Решение:

- для одного `build/windows-clang-debug` нельзя запускать несколько независимых `cmake --build` процессов одновременно; build/test verification этого дерева должна идти последовательно.

Почему:

- параллельные процессы могут столкнуться в CMake regeneration и дать ложные ошибки конфигурации на сторонних зависимостях;
- это workflow-ограничение среды, а не дефект текущего кода, и его лучше держать зафиксированным в памяти проекта.

### C++ modules в текущем `clang-cl` toolchain

Решение:

- не переводить mainline `ProjectV` на C++ named modules в текущем `clang-cl + CMake 4.3.0-rc1 + Ninja + MSVC STL` стеке;
- считать named modules здесь пока `toolchain R&D`, а не практической задачей mainline;
- вернуться к теме только после released CMake с рабочим module scanning для `clang-cl` или после явной смены toolchain на вариант, где и build graph, и `import std` реально поддержаны.

Почему:

- direct compiler probe показал, что сам `clang-cl 21.1.8` named modules уже компилирует и линкует, то есть проблема не в языке и не в фронтенде как таковом;
- но установленный CMake 4.3.0-rc1 на Windows включает clang module scanning только для `GNU` frontend variant, а не для `clang-cl`/`MSVC` frontend, поэтому нормальная CMake-managed миграция сейчас упирается в build-system gap;
- `import std` в этом окружении тоже не готов: текущий `clang-cl` использует MSVC STL, а CMake-путь для Clang `import std` поддерживает только `libc++` и `libstdc++`, что подтверждается и локальным direct probe с `module 'std' not found`.

### Input action layer поверх SDL keyboard input

Решение:

- keyboard input переводится с прямого `SDL_GetKeyboardState` polling на явный `InputActions` слой с per-action `down/pressed` state и bindable scancode slots;
- camera movement, speed modifiers и runtime hotkeys потребляют только этот слой, а не raw SDL keyboard state;
- mouse buttons для `remove/place` пока остаются отдельным edge-triggered interaction path, чтобы не раздувать первый `8.1` slice в общий full-input refactor;
- default controls фиксируются так: `WASD + Space/Shift`, `Ctrl` boost, `Alt` slow, `F1` HUD, `F2` cycle placement material, `F3` reset camera, `P` pause, `Tab` relative mouse toggle;
- `pause` добавляется уже сейчас как часть debug/control glue, а explicit `free-fly / spectator / walk` mode system остаётся следующим follow-up поверх этого foundation.

Почему:

- action-layer делает input path тестируемым и убирает скрытую связку камеры с глобальным SDL keyboard state;
- bindable scancode slots дают честную основу для дальнейших control modes без преждевременной конфиг-системы;
- relative mouse toggle, debug hotkeys и speed modifiers полезны уже на текущем MVP этапе и не требуют отдельного UI/framework слоя;
- оставление mouse remove/place вне общего action refactor держит `8.1` в разумном practical scope и не мешает mainline.

### Разведение `free-fly` и `spectator`

Решение:

- `free-fly` и `spectator` не считаются синонимами: это два явных control mode поверх одного camera/input backend;
- `free-fly` трактуется как debug/tool mode: камера может двигаться и смотреть даже при `pause`, а world edits (`remove/place`) остаются разрешены;
- `spectator` трактуется как observe-only mode: те же базовые camera controls работают только вне `pause`, а `remove/place` отключены, но selection/raycast остаются для inspection;
- control mode переключается отдельным hotkey (`F4`), виден в HUD и не сбрасывается `ResetCameraState`.

Почему:

- это создаёт реальную поведенческую разницу между режимами, а не дублирование названий;
- такой split соответствует текущему состоянию проекта: debug-camera уже нужна сейчас, а полноценный player/controller layer ещё не существует;
- spectator без world edits полезен как честный observe mode уже на MVP этапе, не требуя вводить walk/collision/player stack раньше времени.

### Qualified include paths от корня `src/`

Решение:

- transitional include directories `src/app`, `src/core`, `src/render` и другие убираются из `src/CMakeLists.txt` и `tests/CMakeLists.txt`;
- production и test targets используют только корневой include dir `src/`;
- project headers всегда подключаются qualified-путями от корня `src/`, например `app/Camera.hpp`, `core/Types.hpp`, `render/vulkan/VulkanInit.hpp`, `voxel/VoxelWorld.hpp`.

Почему:

- это превращает физическую раскладку `src/` в реальную архитектурную границу, а не просто в cosmetic file move;
- такой include style сразу показывает subsystem ownership и не даёт незаметно возвращаться к flat-source layout;
- одинаковое правило для production и tests убирает скрытые зависимости от широких include paths.
