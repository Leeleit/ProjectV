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
- `8.1` закрыт: keyboard input больше не завязан на `SDL_GetKeyboardState`, а идёт через bindable `InputActions` слой с relative mouse toggle, debug hotkeys, pause toggle, speed modifiers и явными `free-fly` / `spectator` control modes; актуальная default-схема полёта теперь `Space/Shift` вверх-вниз, `Ctrl` boost, `Alt` slow, а `WASD` в flying modes больше не меняют высоту от pitch камеры.
- `fmt` уже vendored в `external/`, а весь текущий набор `external/*` submodules, включая `external/tracy`, синхронизирован с upstream HEAD; root build, `ctest` и короткий runtime smoke проходят на этом наборе.
- `external/draco` на `2026-04-07` приведён к clean recursive state без локального nested-submodule drift: `third_party/eigen`, `filesystem`, `googletest` и `tinygltf` возвращены к gitlinks, записанным самим `draco`.
- `resize / minimize / restore / maximize / graceful shutdown` теперь подтверждены отдельным Windows smoke script; transient `0x0` surface extent больше не ломает swapchain recreate, а smoke checklist вынесен в `docs/`.
- controlled failure для missing `.spv` и intentional incomplete init теперь подтверждён отдельным failure-probe script; bootstrap/render/pipeline path переведён на единый runtime diagnostics стиль с `PV_CHECK_OR_RETURN` / `PV_ASSERT`.
- `8.2` закрыт целиком: `src/` физически разложен на `app/core/platform/render/render/vulkan/voxel/debug`, build/test targets используют только корень `src/`, project headers подключаются qualified include-путями, а doc entry points синхронизированы через `README_NEW.md` и `docs/source_layout.md`.
- `8.3` закрыт минимальным practical slice: `flecs` подключён в build/test, `src/ecs` держит primary camera/player entities, `world`/`debug` singleton data и chunk mirror summary, а `SDL_AppEvent` / `SDL_AppIterate` / `InitVulkan` уже читают camera/debug/world через ECS glue без большого gameplay-framework rewrite.
- `8.4` закрыт как MVP physics slice: `JoltPhysics` подключён в main/test build, `src/physics` держит static voxel collision world, physics raycast и `CharacterVirtual` walk controller, а `free-fly / spectator / walk` теперь образуют честный набор control modes поверх одного app loop.
- edge-slide на краях блоков в `walk` сейчас считается ожидаемым следствием минимального `CharacterVirtual` slice без отдельного ground-sticking/ledge-stability tuning; это зафиксировано как follow-up polish, а не как runtime-stability regression.
- follow-up warning cleanup продолжает держать mainline в норме: Jolt runtime acquire path теперь non-failing `void`
  helper без фиктивной проверки, поэтому `PhysicsWorld.cpp` не оставляет clangd warning'и вида `condition is always false`
  и `unreachable code`.
- follow-up static-analysis cleanup после `8.3` тоже закрыт: `RuntimeDiagnostics` больше не маскирует `return false` через bool-returning logger'ы, `InputActions` и HUD internal helper'ы используют честные reference-based/non-null сигнатуры, voxel raycast helper'ы теперь опираются на знак `directionAxis` вместо отдельного `step`-параметра, `InitFailureStageToString` починен, а ECS chunk mirror держит только реально используемые summary fields.
- исследование `clang-cl + Windows + C++ modules` показало: direct compiler named modules уже работают, но текущий `CMake 4.3.0-rc1 + Ninja` не умеет module scanning для `clang-cl` MSVC-frontend, а `import std` недоступен на MSVC STL.

Главный разрыв:

- после закрытия `8.4` главный ближайший разрыв уже не в interaction/runtime/physics mainline, а в authored documentation из `8.5`, затем в `save/load` и benchmark scene presets для повторяемых perf/profiling прогонов.

---

## 2. Ближайший рабочий milestone

Ближайшая цель:

- синхронизировать authored docs с уже собранным mainline MVP: interaction loop, HUD, ECS glue, physics walk path, runtime smoke и failure probes должны быть описаны так же честно, как они уже работают в коде.

Это следующий шаг, который отделяет просто рабочий код от воспроизводимого и объяснимого раннего MVP.

---

## 3. Что уже сделано по организации проекта

Уже добавлено:

- корневой `TODO.md` как живой roadmap;
- корневой `AGENTS.md` как обязательный протокол работы;
- папка `agent/` для памяти, статуса и решений.

---

## 4. Что сейчас рекомендуется делать дальше

Приоритетный порядок:

1. Идти следующим practical slice в `8.5`: authored docs для build/run/architecture/debugging поверх уже собранного mainline MVP.
2. После этого идти в `save/load`, benchmark scene presets и следующий practical gameplay/debug layer уже поверх связки `InputActions + ECS + physics`.
3. Отдельно дочищать authored docs и решать, что из runtime diagnostics стоит расширять дальше, а что оставить до полноценного logging layer.

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

---

## 7. Обязательное обновление после следующей заметной задачи

После следующей содержательной задачи нужно:

- обновить соответствующие пункты в `TODO.md`;
- обновить этот файл;
- при необходимости дополнить `memory.md`;
- при наличии долгоживущего решения обновить `decisions.md`.
