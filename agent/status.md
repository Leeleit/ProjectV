# Status

Текущее состояние проекта и ближайший рабочий фокус.

Дата последнего обновления: `2026-04-07`

---

## 1. Текущий статус

Проект находится в стадии `pre-MVP alpha / ранний vertical slice`.

Сильная сторона:

- базовый voxel rendering pipeline уже существует и работает.
- интерактивный editing loop уже поднят до уровня CPU raycast + runtime remove/place block.
- selection visual feedback уже поднят до отдельного overlay path с block highlight и crosshair.
- debug stats уже перенесены в in-app HUD с hotkey toggle, camera/selection telemetry, корректной top-left screen-space привязкой и базовым static-analysis cleanup в input/test glue, включая test fixture setup.
- `8.1` закрыт: keyboard input больше не завязан на `SDL_GetKeyboardState`, а идёт через bindable `InputActions` слой с relative mouse toggle, debug hotkeys, pause toggle, speed modifiers и явными `creative` / `spectator` / `walk` control modes; актуальная default-схема полёта теперь `Space/Shift` вверх-вниз, `Ctrl` boost, `Alt` slow, а `WASD` в flying modes больше не меняют высоту от pitch камеры.
- follow-up usability pass на `2026-04-07` уже добавил double-tap `Space` для быстрого `creative <-> walk` toggle, почти вертикальный look clamp и runtime scene cycling по `F5` без перезапуска приложения.
- debug UI теперь ведёт себя как один слой: `F1` скрывает и HUD, и selection highlight, и crosshair; сам HUD уменьшен и разбит на отдельные stats/helpers panels, а crosshair переведён на более толстый inverted/XOR-style overlay.
- crosshair follow-up polish тоже уже закрыт: marker сохраняет нормальное пересечение `+`, но больше не ловит XOR-дыру в центре, потому что геометрия теперь не накладывает два инвертирующих прямоугольника друг на друга.
- `fmt` уже vendored в `external/`, а весь текущий набор `external/*` submodules, включая `external/tracy`, синхронизирован с upstream HEAD; root build, `ctest` и короткий runtime smoke проходят на этом наборе.
- `external/draco` на `2026-04-07` приведён к clean recursive state без локального nested-submodule drift: `third_party/eigen`, `filesystem`, `googletest` и `tinygltf` возвращены к gitlinks, записанным самим `draco`.
- `resize / minimize / restore / maximize / graceful shutdown` теперь подтверждены отдельным Windows smoke script; transient `0x0` surface extent больше не ломает swapchain recreate, а smoke checklist вынесен в `docs/`.
- controlled failure для missing `.spv` и intentional incomplete init теперь подтверждён отдельным failure-probe script; bootstrap/render/pipeline path переведён на единый runtime diagnostics стиль с `PV_CHECK_OR_RETURN` / `PV_ASSERT`.
- `8.2` закрыт целиком: `src/` физически разложен на `app/core/platform/render/render/vulkan/voxel/debug`, build/test targets используют только корень `src/`, project headers подключаются qualified include-путями, а doc entry points синхронизированы через `README_NEW.md` и `docs/source_layout.md`.
- `8.3` закрыт минимальным practical slice: `flecs` подключён в build/test, `src/ecs` держит primary camera/player entities, `world`/`debug` singleton data и chunk mirror summary, а `SDL_AppEvent` / `SDL_AppIterate` / `InitVulkan` уже читают camera/debug/world через ECS glue без большого gameplay-framework rewrite.
- `8.4` закрыт как MVP physics slice: `JoltPhysics` подключён в main/test build, `src/physics` держит static voxel collision world, physics raycast и `CharacterVirtual` controller, а `creative / spectator / walk` теперь образуют честный набор control modes поверх одного app loop.
- follow-up control-mode redesign на `2026-04-07` уже убрал старый `free-fly`: `creative` теперь летает с collision, `spectator` остаётся отдельным observe-only noclip mode, а переход обратно в `walk` сохраняет текущую позицию и не телепортирует камеру в центр/на пол при отсутствии опоры.
- `8.5` закрыт authored-docs slice: в `docs/` появились отдельные entry guides для architecture, render, `VoxelWorld`, build/run и debugging, а `README_NEW.md` и `docs/source_layout.md` теперь ведут в этот набор вместо implicit knowledge в коде.
- `9.1` закрыт как profiling/measurement slice: `VoxelWorld` теперь умеет builtin baseline scenes (`VoxelLab`, `FlatBenchmark`,
  `TransparencyStress`, `ChunkGrid`, `MeshingStress`) через `PROJECTV_SCENE_PRESET`, HUD показывает текущий scene preset, а
  `docs/Profiling.md` фиксирует scene purposes, Tracy metrics pack и reproducible perf methodology.
- edge-slide на краях блоков в `walk` сейчас считается ожидаемым следствием минимального `CharacterVirtual` slice без отдельного ground-sticking/ledge-stability tuning; это зафиксировано как follow-up polish, а не как runtime-stability regression.
- follow-up warning cleanup продолжает держать mainline в норме: Jolt runtime acquire path теперь non-failing `void`
  helper без фиктивной проверки, поэтому `PhysicsWorld.cpp` не оставляет clangd warning'и вида `condition is always false`
  и `unreachable code`.
- follow-up static-analysis cleanup после `8.3` тоже закрыт: `RuntimeDiagnostics` больше не маскирует `return false` через bool-returning logger'ы, `InputActions` и HUD internal helper'ы используют честные reference-based/non-null сигнатуры, voxel raycast helper'ы теперь опираются на знак `directionAxis` вместо отдельного `step`-параметра, `InitFailureStageToString` починен, а ECS chunk mirror держит только реально используемые summary fields.
- следующий warning-cleanup follow-up на `2026-04-07` тоже сделан через корневой рефактор, а не через suppress-патчи: double-tap `Space` в
  `InputActions` больше не прячется за фальшивый generic helper, а scene preset path в `VoxelWorld` разделяет общий `VoxelWorldConfig` и
  `VoxelLab`-specific shell builder, поэтому `VoxelLab`-only код больше не живёт под мёртвыми ветками вида `sphereRadius <= 0`.
- ещё один follow-up на `2026-04-07` дочистил те же причины до конца: `VoxelLab` теперь собирается через dedicated builder path без
  промежуточного aggregate config, а HUD panel helper честно отражает fixed left-column layout вместо псевдонастраиваемого `minXPx`.
- residual unreachable switch-arms для `VoxelLab` в non-`VoxelLab` helpers тоже убраны: dedicated builder contract теперь выражен не только в
  верхнем scene dispatch, но и внутри самих helper-функций `GetNonVoxelLabWorldConfig` / `BuildNonVoxelLabSceneWorld`.
- последний follow-up на `2026-04-07` добил и interprocedural enum-noise в scene preset path: `VoxelWorld.cpp` больше не держит
  псевдо-generic `GetNonVoxelLabWorldConfig(scenePreset)`, а использует отдельные config builders для `FlatBenchmark`,
  `TransparencyStress`, `ChunkGrid` и `MeshingStress`, поэтому clangd больше не получает ложные always-true/always-false условия по preset'ам.
- исследование `clang-cl + Windows + C++ modules` показало: direct compiler named modules уже работают, но текущий `CMake 4.3.0-rc1 + Ninja` не умеет module scanning для `clang-cl` MSVC-frontend, а `import std` недоступен на MSVC STL.

Главный разрыв:

- после закрытия `9.1` главный ближайший разрыв уже не в profiling/docs baseline, а в reproducible build/automation и data-layer follow-up:
  `9.2` build hygiene, `save/load`, CI и automated smoke/benchmark path.

---

## 2. Ближайший рабочий milestone

Ближайшая цель:

- сделать следующий reproducibility slice вокруг уже измеримого MVP: `9.2` build/automation hygiene, затем вернуться к `save/load`
  поверх уже существующих builtin scene presets.

Это следующий шаг, который отделяет просто хорошо объяснённый MVP от более полноценного sandbox prototype.

---

## 3. Что уже сделано по организации проекта

Уже добавлено:

- корневой `TODO.md` как живой roadmap;
- корневой `AGENTS.md` как обязательный протокол работы;
- папка `agent/` для памяти, статуса и решений.

---

## 4. Что сейчас рекомендуется делать дальше

Приоритетный порядок:

1. Идти следующим practical slice в `9.2` build/automation hygiene: presets, test coverage по build presets, smoke/CI glue.
2. После этого вернуться к `10.1` и делать `save/load` поверх уже собранного interaction + ECS + physics mainline и builtin scene presets.
3. Затем идти в следующий practical gameplay/debug layer уже поверх связки `InputActions + ECS + physics`.
4. Отдельно поддерживать authored docs в sync и решать, что из runtime diagnostics стоит расширять дальше, а что оставить до полноценного logging layer.

---

## 5. Что не должно сбить фокус

Пока не делать главным направлением:

- `SVO`
- mesh shaders
- большой renderer rewrite
- тяжёлый job system заранее
- complex editor
- мультиплеер
- модульную/плагинную платформу

Это допустимо только как отдельный R&D-уголок без блокировки mainline.

---

## 6. Рабочие риски

- Корневой roadmap может снова устареть, если его не обновлять после каждой заметной задачи.
- Legacy-документы могут уводить в сторону foundation/R&D, если забыть, что код уже ушёл дальше.
- Преждевременная оптимизация и архитектурный перфекционизм могут затормозить появление живого MVP.
- Неаккуратная работа с рабочим деревом может затронуть чужие изменения в `README.md` и указатели обновлённых `external/*` submodules.
- Крупные обновления third-party всё ещё требуют обязательного build/test/smoke прохода, даже если все submodules уже чистые.
- transitional include directories после `8.2` полезны как low-risk bridge, но их нельзя оставлять вечной архитектурой: их надо постепенно сужать по мере работы над подсистемами.
- параллельный запуск двух `cmake --build` процессов против одного `build/windows-clang-debug` может ловить гонку регенерации CMake; проверки этого дерева надо гонять последовательно.
- переход на C++ modules в текущем `clang-cl` mainline сейчас упирается не в язык, а в build-system/toolchain gap: direct probe проходит, но CMake-managed migration пока преждевременна.
- warning cleanup без структурной причины теперь считается отдельным риском: если проблема указывает на плохой контракт или ложную абстракцию,
  её нельзя "затыкать" ради тишины без явного согласования с пользователем.

---

## 7. Обязательное обновление после следующей заметной задачи

После следующей содержательной задачи нужно:

- обновить соответствующие пункты в `TODO.md`;
- обновить этот файл;
- при необходимости дополнить `memory.md`;
- при наличии долгоживущего решения обновить `decisions.md`.
