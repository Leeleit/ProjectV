# Build, CI, Release

Документ описывает build-систему, CI pipeline, release flow для
высокопроизводительных движков.

---

## Build-система: CMake 4.x

CMake 4.x — рекомендуемая версия для проектов на C++26. CMake 4.4 в
разработке добавляет полную поддержку C++ modules.

### Минимальные требования

```cmake
cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
project(MyEngine
    VERSION 0.1.0
    LANGUAGES C CXX)
```

CMake 3.30 — минимум для большинства современных фич. CMake 4.0+ —
рекомендуется для C++26 модулей.

### Стандарт C++

```cmake
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # никаких GNU-расширений
```

`CXX_EXTENSIONS OFF` — без `-std=gnu++26`, только `-std=c++26`.

### Standard library

```cmake
set(CMAKE_CXX_STDLIB libc++)  # Clang + libc++ для import std;
```

libc++ поддерживает `import std;`. libstdc++ ещё нет.

### Оптимизации Release

```cmake
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(
        -O3
        -flto=thin
        -DNDEBUG
        -ffunction-sections
        -fdata-sections
        -fno-finite-math-only   # запрет -ffast-math для детерминизма
    )
    add_link_options(
        -flto=thin
        -Wl,--gc-sections
    )
endif()
```

`-fno-finite-math-only` — критично для детерминизма floating point.

---

## Преcеты (CMakePresets.json)

CMakePresets.json — стандартизированная конфигурация. Пример:

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "linux-clang-debug",
            "displayName": "Linux Clang Debug (Dev)",
            "generator": "Ninja",
            "binaryDir": "build/linux-clang-debug",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CMAKE_CXX_COMPILER": "clang++",
                "CMAKE_CXX_STANDARD": "26",
                "PROJECTV_ENABLE_VALIDATION": "ON",
                "PROJECTV_ENABLE_TRACY": "ON"
            }
        },
        {
            "name": "linux-clang-release",
            "displayName": "Linux Clang Release",
            "generator": "Ninja",
            "binaryDir": "build/linux-clang-release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_CXX_COMPILER": "clang++",
                "PROJECTV_ENABLE_TRACY": "OFF"
            }
        }
    ]
}
```

Использование:

```bash
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
```

---

## CI gates (ворота CI)

Каждый PR проходит через ворота. Любое падение = провал.

### Gate 1: Build Debug

```bash
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
```

Любая ошибка компиляции = провал. Любое warning (с `-Werror`) = провал.

### Gate 2: Tests

```bash
cd build/linux-clang-debug && ctest --output-on-failure
```

Все тесты должны проходить. 100% pass rate.

### Gate 3: Validation

В Debug-сборке `PROJECTV_ENABLE_VALIDATION=ON`. Vulkan validation
layers включены.

Любое предупреждение validation = провал.

### Gate 4: Lint

```bash
clang-tidy --config-file=.clang-tidy src/
clang-format --check src/
```

Любое предупреждение clang-tidy или нарушение формата = провал.

### Gate 5: Sanitizers

В отдельной Debug-сборке `PROJECTV_SANITIZER=address` или `thread`.
Любой отчёт санитайзера = провал.

### Gate 6: Benchmark (еженедельно)

Google Benchmark для критических путей. Регрессия > 5% = алерт.

---

## Release flow

### Версионирование

Semantic versioning: `MAJOR.MINOR.PATCH`.

- `MAJOR`: breaking changes в API.
- `MINOR`: новая функциональность без breaking.
- `PATCH`: bugfixes.

Тег в git: `v0.1.0`, `v0.2.1`, etc.

### Release-build pipeline

1. **PGO сборка.** Первый проход: инструментированная сборка, сбор
   профиля через типичную нагрузку (1000 кадров с реальной сценой).
2. **Оптимизированная сборка.** Второй проход с `-fprofile-instr-use`.
3. **ThinLTO.** Кросс-TU оптимизация при линковке.
4. **Strip символов.** Финальный бинарник без отладочных символов.
5. **Sanity check.** 60 FPS на reference сцене (RTX 3060 Ti).

### Release artifacts

- Бинарник движка.
- Заголовочные файлы (public API).
- Asset packs (если есть).
- Документация (генерируется из комментариев).
- Changelog.

---

## Координация нескольких агентов

При работе нескольких агентов над одним репозиторием — правила
координации обязательны.

### Непересекающиеся scope

Распределённые задачи должны иметь строго непересекающиеся scope (разные
файлы, модули или уровни абстракции).

### Разрешение конфликтов

При возникновении конфликтов (merge conflict или перезапись):

1. Прекратить автоматические исправления.
2. Выполнить `git status`, `git diff`, `git log -p` для анализа обеих веток.
3. Определить автора параллельных изменений.
4. Выполнить ручной merge.
5. Зафиксировать резолюцию конфликта в engineering docs.

### Редактирование хабов

Файлы-хабы (CMakeLists.txt, конвенции, общие заголовки) — точечные
изменения сразу после `git pull` или ручной проверки отсутствия свежих
изменений.

---

## Submodule policy

Все внешние зависимости — git submodule'и. Никаких FetchContent,
auto-download.

Принципы:

- **`include-what-you-pay-for`** — каждая зависимость оправдана.
- **Предпочитать header-only** — упрощает интеграцию.
- **Версия фиксируется в submodule SHA** — reproducible builds.

### Как добавить новую зависимость

1. Проверить таблицу в [91_tooling-landscape.md](91_tooling-landscape.md).
2. Открыть issue с обоснованием.
3. Pin в submodule с конкретным commit SHA.
4. Добавить в `CMakeLists.txt`.
5. Обновить tooling landscape.
6. Обновить engineering contracts, если меняется контракт.

---

## Пример: реализация в ProjectV

ProjectV использует `CMakePresets.json` с 6 presets (linux/windows ×
debug/release × dev/CI). CI gates — 5 уровней: build, test, validation,
lint, sanitizer. Release flow — PGO + ThinLTO + strip. Engineering
contracts в `agent/knowledge.md` фиксируют важные решения.

---

## Источники и дальнейшее чтение

- **CMake documentation** — cmake-buildsystem(7), cmake-cxxmodules(7).
  <https://cmake.org/cmake/help/latest/>
- **CMakePresets.json schema**.
  <https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html>
- [14_compiler.md](14_compiler.md) — PGO, ThinLTO, санитайзеры.
- [19_debugging.md](19_debugging.md) — Tracy workflow.
- [33_testing.md](33_testing.md) — тестирование.
- [90_code-review-checklist.md](90_code-review-checklist.md) — review
  gates.
- [91_tooling-landscape.md](91_tooling-landscape.md) — инструменты.