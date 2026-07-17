# Voxel ASCII Guide

Дата фиксации: `2026-07-17`

ASCII-представление воксельного мира для агентов и отладки: этажи как в Dwarf Fortress
(срез по **Y**, карта **XZ** сверху) и opt-in tick-лог в файл с дедупом по слоям.

Код: [`src/voxel/VoxelWorldAscii.hpp`](../src/voxel/VoxelWorldAscii.hpp),
[`src/voxel/VoxelAsciiTickLogger.hpp`](../src/voxel/VoxelAsciiTickLogger.hpp).
Контракт: `agent/knowledge.md` §8.1.

---

## Координаты и вид

| ProjectV | Смысл           | В ASCII                                              |
|:---------|:----------------|:-----------------------------------------------------|
| **Y**    | высота (вверх)  | индекс этажа (DF «z-level»)                          |
| **X**    | восток          | колонки слева → направо                              |
| **Z**    | север/юг в мире | строки сверху → вниз (`z` возрастает вниз по тексту) |

Один этаж — горизонтальный срез: все клетки с фиксированным `y`, окно по `x`/`z`.

---

## Легенда символов

| Символ | `VoxelMaterial` |
|:-------|:----------------|
| `.`    | Air             |
| `G`    | Glass           |
| `~`    | Fluid           |
| `#`    | FloorWhite      |
| `%`    | FloorGray       |

---

## Разовый dump (API)

Возвращает `std::string` — печать на стороне вызывающего (stderr, тест, агент).

```cpp
#include "voxel/VoxelWorldAscii.hpp"

// один этаж; bounds=nullopt → tight AABB всех non-Air (+ options.padding)
std::string layer = FormatVoxelAsciiYLayer(world, /*y=*/1);

// стек этажей high→low; yMin/yMaxExclusive=nullopt → из auto-AABB
std::string stack = FormatVoxelAsciiYLayers(world);

VoxelAsciiOptions opt{};
opt.padding = 1;           // воздушная рамка вокруг non-Air
opt.includeLegend = true;
opt.includeZRowLabels = true;

auto bounds = ComputeVoxelAsciiBounds(world, /*padding=*/0);
std::string clipped = FormatVoxelAsciiYLayer(world, 1, bounds, opt);

// явный override окна (полный мир и т.п.)
VoxelAsciiBounds full{world.min, world.maxExclusive};
std::string wide = FormatVoxelAsciiYLayer(world, 1, full);
```

Пример вывода:

```text
y=1 (x=2..4, z=3..4)
z=  3: #~G
z=  4: .%.
```

Пустой мир / `y` вне мира → пустая строка.

**Bounds:** по умолчанию не весь VoxelLab (±12), а tight AABB non-Air. Полный мир — только явным `VoxelAsciiBounds`.

---

## Tick-лог в файл (60 Hz, per-layer dedup)

### Зачем

Снимать мир на каждом simulation tick (`1/60` с), но **не писать дубли**: если вода
потекла только снизу, верхние этажи с тем же содержимым в файл не попадают.

Нельзя полагаться только на `editVersion`: переходы Air↔Fluid его не бампят.

### Включение (PowerShell)

**По умолчанию лог ВКЛЮЧЁН** (env не нужен).

**Куда пишется:** рядом с `ProjectV.exe`, файл `voxel-ascii-tick.log`
(через `SDL_GetBasePath()`). Обычно это:

`build/windows-clang-debug/bin/voxel-ascii-tick.log`

При старте в stderr: `[VoxelAsciiTickLog] writing to '...'`.

```powershell
# default ON — просто запуск
.\build\windows-clang-debug\bin\ProjectV.exe

# свой путь (родительская папка создаётся автоматически)
$env:PROJECTV_ASCII_TICK_LOG = 'C:\temp\ascii-fluid.log'
.\build\windows-clang-debug\bin\ProjectV.exe

# выключить
$env:PROJECTV_ASCII_TICK_LOG = '0'
# или: 'OFF' / 'FALSE' / 'NO'
```

В PowerShell для путей с `\` лучше **одинарные** кавычки: `'C:\temp\log.txt'`
(в двойных `\t` не tab, но привычка с одинарными надёжнее).

| Значение env | Поведение |
|:-------------|:----------|
| unset / `""` / `1` / `ON` / `TRUE` / `YES` | **ON**, файл рядом с exe |
| `0` / `OFF` / `FALSE` / `NO` | выкл |
| любой другой путь | писать в этот файл (mkdir -p родителя) |

Хук: `MaybeLogVoxelAsciiTick` в `RunSimulationTickLoop` после `++simulationTick`.
На паузе тики не крутятся → лог не растёт. Env читается **один раз** при первом тике.

### Формат файла

```text
PROJECTV_ASCII_TICK_LOG path=voxel-ascii-tick.log
legend: .=Air G=Glass ~=Fluid #=FloorWhite %=FloorGray
# tick=120
y=2 (x=2..2, z=2..2)
z=  2: G
y=0 (x=2..2, z=2..2)
z=  2: #
# tick=135
y=0 (x=2..2, z=2..2)
z=  2: ~
```

- Запись тика есть **только если** изменился хотя бы один слой.
- Внутри тика печатаются **только изменившиеся** `y` (сверху вниз по высоте).
- Сдвиг окна **XZ** сбрасывает кэш хешей → слои перепишутся; расширение только по **Y**
  сохраняет хеши нижних/средних этажей.

### Как читать агенту

1. Открыть лог после прогона (или `Get-Content -Wait` во время).
2. Искать `# tick=N` — моменты изменений.
3. Смотреть только напечатанные `y=` — остальное с прошлого тика **не изменилось**.
4. Склеивать картину этажа: последний dump для данного `y` до текущего tick.

Fluid CA по умолчанию ~5 Hz, sim — 60 Hz: между fluid-тиками файл молчит (хеши совпали).

---

## Использование в тестах

Цель: `ProjectVVoxelWorldAsciiTests`.

```powershell
cmake --build build/windows-clang-debug --target ProjectVVoxelWorldAsciiTests --config Debug
.\build\windows-clang-debug\bin\ProjectVVoxelWorldAsciiTests.exe
```

Для своих тестов — `OnSimulationTickTo(world, tick, ostream)` без файла:

```cpp
#include "voxel/VoxelAsciiTickLogger.hpp"
#include <sstream>

VoxelAsciiTickLogger logger{};
std::ostringstream out;
logger.OnSimulationTickTo(world, /*tick=*/1, out);
// второй тик без правок мира → out2 пустой
```

---

## Ограничения

- Нет видов сбоку и срезов по X/Z — только этажи по Y.
- Нет hotkey / ImGui / stderr sink в mainline (только env → файл).
- Первый тик с контентом дампит все слои AABB (cache miss).
- Полный мир в логе — не default; нужен явный bounds в API или широкий non-Air.

---

## Связанные документы

- [Debugging](Debugging.md) — HUD / hotkeys / smoke
- [CODEBASE_GUIDE](CODEBASE_GUIDE.md) — voxel pipeline
- `agent/knowledge.md` §8.1 — короткий контракт для агентов
