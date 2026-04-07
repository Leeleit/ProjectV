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
- `8.1` закрыт: keyboard input больше не завязан на `SDL_GetKeyboardState`, а идёт через bindable `InputActions` слой с relative mouse toggle, debug hotkeys, pause toggle, speed modifiers и явными `free-fly` / `spectator` control modes; актуальная default-схема полёта теперь `Space/Shift` вверх-вниз, `Ctrl` boost, `Alt` slow.
- `fmt` уже vendored в `external/`, а весь текущий набор `external/*` submodules, включая `external/tracy`, синхронизирован с upstream HEAD; root build, `ctest` и короткий runtime smoke проходят на этом наборе.
- `resize / minimize / restore / maximize / graceful shutdown` теперь подтверждены отдельным Windows smoke script; transient `0x0` surface extent больше не ломает swapchain recreate, а smoke checklist вынесен в `docs/`.
- controlled failure для missing `.spv` и intentional incomplete init теперь подтверждён отдельным failure-probe script; bootstrap/render/pipeline path переведён на единый runtime diagnostics стиль с `PV_CHECK_OR_RETURN` / `PV_ASSERT`.
- `8.2` закрыт целиком: `src/` физически разложен на `app/core/platform/render/render/vulkan/voxel/debug`, build/test targets используют только корень `src/`, project headers подключаются qualified include-путями, а doc entry points синхронизированы через `README_NEW.md` и `docs/source_layout.md`.
- исследование `clang-cl + Windows + C++ modules` показало: direct compiler named modules уже работают, но текущий `CMake 4.3.0-rc1 + Ninja` не умеет module scanning для `clang-cl` MSVC-frontend, а `import std` недоступен на MSVC STL.

Главный разрыв:

- после закрытия `8.1` и `8.2` главный ближайший разрыв уже не в input/layout glue, а в следующем функциональном слое mainline: либо идти в минимальный `8.3` ECS slice, либо расширять authored docs из `8.5` до более подробного architecture/render/world уровня.

---

## 2. Ближайший рабочий milestone

Ближайшая цель:

- интерактивный voxel MVP с уже работающими block picking, remove/place loop, block highlight, crosshair и базовым HUD, плюс проверенный runtime stability path и smoke checklist.

Это главный критерий, который отделяет текущий рендер-прототип от настоящего раннего MVP.

---

## 3. Что уже сделано по организации проекта

Уже добавлено:

- корневой `TODO.md` как живой roadmap;
- корневой `AGENTS.md` как обязательный протокол работы;
- папка `agent/` для памяти, статуса и решений.

---

## 4. Что сейчас рекомендуется делать дальше

Приоритетный порядок:

1. Решить, идти ли следующим practical slice в `8.3` минимальный ECS или сначала расширять authored docs из `8.5`.
2. Отдельно решить, что из runtime diagnostics стоит расширить на remaining helper/world modules, а что оставить до полноценного logging layer.
3. После этого заходить в `walk / noclip`, player controller, physics и save/load.

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
