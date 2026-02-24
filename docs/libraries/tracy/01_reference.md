## Обзор Tracy

<!-- anchor: 00_overview -->

Кроссплатформенный профилировщик C++ приложений с наносекундным разрешением.

---

## Что такое Tracy

Tracy — инструмент для профилирования C и C++ приложений в реальном времени. Предоставляет наносекундное разрешение,
низкий overhead (<1% CPU) и визуализацию данных через графический интерфейс.

Исходный код: [wolfpld/tracy](https://github.com/wolfpld/tracy)

Текущая версия: **0.11+**

---

## Возможности

### Профилирование CPU

- Зоны (zones) — измерение времени выполнения участков кода
- Автоматический захват стека вызовов (callstack)
- Глубина стека до 62 фреймов
- Именованные и цветные зоны

### Профилирование GPU

- Vulkan 1.1+
- OpenGL 3.3+
- Direct3D 11/12
- Metal
- CUDA, OpenCL

### Профилирование памяти

- Отслеживание аллокаций и освобождений
- Именованные пулы памяти
- Анализ утечек памяти
- Перехват malloc/free, new/delete

### Дополнительные возможности

- Графики (Plots) — числовые метрики в реальном времени
- Сообщения — текстовые аннотации в таймлайне
- Многопоточность — профилирование всех потоков
- Lock contention — анализ блокировок
- Fibers — поддержка корутин и файберов
- Remote profiling — профилирование удалённых устройств

---

## Архитектура

Tracy использует клиент-серверную архитектуру:

```
┌─────────────────┐         ┌─────────────────┐
│  Приложение     │  TCP    │  Tracy Server   │
│  (Tracy Client) │ ──────► │  (Profiler GUI) │
│                 │  :8947  │                 │
└─────────────────┘         └─────────────────┘
```

**Клиент** встраивается в приложение через заголовочные файлы и макросы.

**Сервер** — отдельное приложение с GUI для визуализации данных. Может работать на другой машине.

---

## Поддерживаемые платформы

| Платформа          | Профилирование CPU | GPU                          | Системное |
|--------------------|--------------------|------------------------------|-----------|
| Windows x86/x64    | Да                 | Vulkan, D3D11, D3D12, OpenGL | ETW       |
| Linux x86/x64, ARM | Да                 | Vulkan, OpenGL               | perf      |
| macOS x64, ARM     | Да                 | Vulkan, Metal, OpenGL        | —         |
| Android ARM, ARM64 | Да                 | Vulkan, OpenGL               | ATrace    |
| *BSD               | Да                 | Vulkan, OpenGL               | —         |

---

## Поддерживаемые языки

| Язык    | Интеграция         |
|---------|--------------------|
| C       | TracyC.h           |
| C++     | Tracy.hpp          |
| Lua     | TracyLua.hpp       |
| Python  | pytracy            |
| Fortran | TracyClient.F90    |
| Rust    | tracy-client crate |
| Zig     | ztracy             |
| C#      | tracy-net          |
| Odin    | odin-tracy         |

---

## Требования

### Компиляция клиента

- C++11 или новее (рекомендуется C++17+)
- C99 — для C-интерфейса
- CMake 3.10+ — для интеграции через CMake

### Запуск сервера

- Браузер с WebGL2 (Chrome, Firefox, Edge)
- Или нативный клиент (Windows, Linux, macOS)

### Дополнительные требования

- Для GPU профилирования: соответствующий графический API
- Для системного профилирования: права администратора/root
- Для remote profiling: сетевое подключение TCP порт 8947

---

## Лицензия

Tracy распространяется под лицензией **BSD 3-Clause**.

---

## Концепции Tracy

<!-- anchor: 03_concepts -->

Основные концепции и инструменты профилирования.

---

## Зоны (Zones)

Зона — основной инструмент измерения времени выполнения кода. Автоматически захватывает время начала и окончания.

### Базовые зоны

```cpp
void myFunction() {
    ZoneScoped;  // Имя зоны = имя функции
}
```

### Именованные зоны

```cpp
void process() {
    ZoneScopedN("DataProcessing");
}
```

### Цветные зоны

```cpp
void render() {
    ZoneScopedC(0xFF0000);  // Красный
    ZoneScopedNC("Physics", 0x00FF00);  // Зелёный с именем
}
```

### Ручное управление зонами

```cpp
void conditionalProfiling() {
    ZoneNamed(zone, true);  // true = активна

    // Код...

    if (someCondition) {
        zone.Text("Condition met", 14);
    }
}
```

### Аннотации зон

```cpp
void loadData(const char* filename, int size) {
    ZoneScoped;

    // Текстовая аннотация
    ZoneText(filename, strlen(filename));

    // Форматированный текст
    ZoneTextF("Size: %d bytes", size);

    // Динамическое имя
    ZoneName(filename, strlen(filename));

    // Числовое значение
    ZoneValue(size);

    // Цвет зоны
    ZoneColor(0xFF0000);

    // Проверка активности
    if (ZoneIsActive) {
        // Дополнительная работа только при профилировании
    }
}
```

---

## Кадры (Frames)

Кадры — отметки начала логического кадра приложения (игрового цикла, рендера и т.д.).

### Основной кадр

```cpp
while (running) {
    FrameMark;
    // Игровая логика...
}
```

### Именованные кадры

Для нескольких независимых циклов:

```cpp
// Основной игровой цикл
void gameLoop() {
    while (running) {
        FrameMarkNamed("GameLoop");
        // ...
    }
}

// Рендер цикл
void renderLoop() {
    while (running) {
        FrameMarkNamed("RenderLoop");
        // ...
    }
}
```

### Вложенные кадры

```cpp
void renderFrame() {
    FrameMarkStart("ShadowPass");
    renderShadows();
    FrameMarkEnd("ShadowPass");

    FrameMarkStart("GeometryPass");
    renderGeometry();
    FrameMarkEnd("GeometryPass");
}
```

### Изображения кадра

```cpp
void captureFrame() {
    uint8_t* imageData = getFramebuffer();
    FrameImage(imageData, width, height, 0, false);
}
```

---

## Графики (Plots)

Графики отображают числовые значения во времени.

### Базовое использование

```cpp
void updateMetrics() {
    TracyPlot("FPS", (int64_t)fps);
    TracyPlot("FrameTime", frameTimeMs);
    TracyPlot("MemoryMB", memoryUsage / (1024.0 * 1024.0));
}
```

### Конфигурация графиков

```cpp
// Типы: Number, Memory, Percentage
TracyPlotConfig("MemoryMB", tracy::PlotFormatType::Memory, false, true, 0);

// Number — обычные числа
// Memory — форматирование как память (KB, MB, GB)
// Percentage — процентные значения
```

---

## Сообщения (Messages)

Текстовые аннотации в таймлайне.

### Базовые сообщения

```cpp
TracyMessage("Loading started", 16);
TracyMessageL("Loading started");  // С автоматическим размером
```

### Цветные сообщения

```cpp
TracyMessageC("Error!", 6, 0xFF0000);  // Красный
TracyMessageLC("Error!", 0xFF0000);    // С автоматическим размером
```

### Информация о приложении

```cpp
TracyAppInfo("MyApp v1.0", 10);
```

---

## Профилирование памяти

### Отслеживание аллокаций

```cpp
void* ptr = malloc(size);
TracyAlloc(ptr, size);

free(ptr);
TracyFree(ptr);
```

### Именованные аллокации

```cpp
void* texture = malloc(size);
TracyAllocN(texture, size, "TextureData");

free(texture);
TracyFreeN(texture, "TextureData");
```

### Безопасные аллокации

Для памяти, которая может быть освобождена после завершения профилирования:

```cpp
TracySecureAlloc(ptr, size);
TracySecureFree(ptr);

TracySecureAllocN(ptr, size, "Name");
TracySecureFreeN(ptr, "Name");
```

### Сброс данных памяти

```cpp
TracyMemoryDiscard("TextureData");
TracySecureMemoryDiscard("TextureData");
```

---

## Многопоточность

### Автоматическое отслеживание потоков

Tracy автоматически отслеживает все потоки, использующие макросы профилирования.

### Блокировки (Locks)

```cpp
// Обычный mutex
std::mutex mtx;
// Заменить на:
TracyLockable(std::mutex, mtx);

// С именем
TracyLockableN(std::mutex, mtx, "DataMutex");

// Shared lock (read-write mutex)
TracySharedLockable(std::shared_mutex, rwMtx);
TracySharedLockableN(std::shared_mutex, rwMtx, "RWMutex");
```

### Отметки блокировок

```cpp
TracyLockable(std::mutex, mtx);

void protectedFunction() {
    std::lock_guard lock(mtx);
    LockMark(mtx);  // Отметка точки входа в блокировку
}
```

### Fibers

```cpp
#define TRACY_FIBERS

void fiberFunction() {
    TracyFiberEnter("MyFiber");
    // Код файбера...
    TracyFiberLeave;
}

// С подсказкой группы
TracyFiberEnterHint("MyFiber", groupHint);
```

---

## Стек вызовов (Callstack)

### Автоматический захват стека

```cpp
// Глобальная настройка глубины
#define TRACY_CALLSTACK 8  // До 62 фреймов
```

### Явное указание глубины

```cpp
// Зона со стеком глубиной 16
ZoneScopedS(16);
ZoneScopedNS("Name", 16);
ZoneScopedCS(0xFF0000, 16);
ZoneScopedNCS("Name", 0xFF0000, 16);

// Аллокации со стеком
TracyAllocS(ptr, size, 16);
TracyFreeS(ptr, 16);

// Сообщения со стеком
TracyMessageS("text", 4, 16);
TracyMessageLS("text", 16);
```

---

## Транзиентные зоны

Зоны, не создаваемые статически (для динамических имён):

```cpp
void dynamicZone(const char* name) {
    ZoneTransient(zone, true);  // true = активна
    ZoneTransientN(zone, name, true);
    ZoneTransientNC(zone, name, 0xFF0000, true);
}
```

---

## On-demand профилирование

Режим, при котором данные отправляются только при подключении сервера.

```cpp
#define TRACY_ON_DEMAND

void function() {
    if (TracyIsConnected) {
        // Выполнить только при подключении
    }
}
```

---

## Параметры и калбеки

### Регистрация параметров

```cpp
void parameterCallback(uint32_t idx, int32_t val) {
    // Обработка параметра
}

TracyParameterRegister(parameterCallback, nullptr);
TracyParameterSetup(0, "MaxFPS", false, 60);
```

### Регистрация калбека исходного кода

```cpp
void sourceCallback(const char* data, size_t size) {
    // Обработка данных
}

TracySourceCallbackRegister(sourceCallback, nullptr);
```

### Установка имени программы

```cpp
TracySetProgramName("MyApplication");
```

---

## Overhead и производительность

### Типичный overhead

| Операция                | Overhead                     |
|-------------------------|------------------------------|
| ZoneScoped (без стека)  | ~50 ns                       |
| ZoneScopedS (со стеком) | ~500 ns + зависит от глубины |
| FrameMark               | ~50 ns                       |
| TracyPlot               | ~100 ns                      |
| TracyMessage            | ~200 ns                      |
| TracyAlloc/Free         | ~100 ns                      |

### Рекомендации по минимизации overhead

1. Используйте `TRACY_LOW_OVERHEAD` в Release
2. Ограничьте глубину стека (`TRACY_CALLSTACK=8`)
3. Используйте `TRACY_ON_DEMAND` для production
4. Избегайте `ZoneText` в hot paths
5. Используйте `TRACY_ONLY_FRAME` для минимального профилирования

---

## Глоссарий Tracy

<!-- anchor: 07_glossary -->

Термины и определения, используемые в Tracy.

---

## A

### Allocation (Аллокация)

Выделение памяти в куче. Tracy отслеживает аллокации через макросы `TracyAlloc`, `TracyAllocN`.

---

## C

### Callstack (Стек вызовов)

Последовательность вызовов функций, приведших к текущей точке выполнения. Tracy может автоматически захватывать стек для
зон и аллокаций.

### Client (Клиент)

Часть Tracy, встроенная в профилируемое приложение. Отправляет данные на сервер через TCP.

### Collect (Сбор)

Операция получения данных GPU после выполнения команд. Для Vulkan: `TracyVkCollect`, для OpenGL: `TracyGpuCollect`.

### Context (Контекст)

Объект, представляющий связь с GPU API. Например, `tracy::VkCtx*` для Vulkan. Создаётся один раз при инициализации.

---

## E

### ETW (Event Tracing for Windows)

Системный механизм трассировки событий в Windows. Tracy использует ETW для системного профилирования на Windows.

---

## F

### Fiber (Файбер)

Легковесный поток выполнения с кооперативной многозадачностью. Tracy поддерживает профилирование файберов через
`TracyFiberEnter` / `TracyFiberLeave`.

### Frame (Кадр)

Логическая единица выполнения, обычно одна итерация игрового цикла. Отмечается макросом `FrameMark`.

### Frame Image (Изображение кадра)

Снимок содержимого экрана, захваченный для отображения в Tracy. Создаётся через `FrameImage`.

---

## G

### GPU Zone (GPU зона)

Измеряемый участок выполнения GPU команд. Создаётся через timestamp queries.

---

## H

### Hot Path (Горячий путь)

Участок кода, выполняющийся очень часто. В hot paths следует минимизировать количество зон и аннотаций.

---

## L

### Lock Contention (Конфликт блокировок)

Ситуация, когда несколько потоков пытаются получить одну блокировку одновременно. Tracy визуализирует время ожидания.

### Lockable

Обёртка над mutex для отслеживания блокировок. Создаётся через `TracyLockable`, `TracySharedLockable`.

---

## M

### Memory Pool (Пул памяти)

Именованная группа аллокаций для отслеживания использования памяти по категориям. Создаётся через `TracyAllocN` с
одинаковым именем.

---

## O

### On-demand

Режим работы, при котором Tracy отправляет данные только при подключении сервера. Включается через `TRACY_ON_DEMAND`.

### Overhead

Накладные расходы на профилирование. Измеряется в наносекундах на операцию.

---

## P

### Plot (График)

Временной ряд числовых значений. Отображается как график в интерфейсе Tracy. Создаётся через `TracyPlot`.

### Profiler (Профилировщик)

Инструмент для измерения производительности кода. Tracy — статистический профилировщик с инструментированием.

---

## Q

### Query Pool (Пул запросов)

Объект GPU API для хранения timestamp queries. Tracy создаёт и управляет query pools автоматически.

---

## R

### Remote Profiling (Удалённое профилирование)

Профилирование приложения, запущенного на другом устройстве. Требует сетевого подключения TCP порт 8947.

---

## S

### Sampling (Сэмплирование)

Метод профилирования, при котором периодически снимается состояние всех потоков. Требует специальных прав на большинстве
платформ.

### Scope (Область видимости)

Время жизни объекта зоны. Зона автоматически завершается при выходе из scope.

### Secure Allocation (Безопасная аллокация)

Аллокация, которая корректно обрабатывается даже если освобождение происходит после завершения профилирования.
`TracySecureAlloc` / `TracySecureFree`.

### Server (Сервер)

GUI-приложение Tracy для визуализации данных профилирования. Подключается к клиенту по сети.

### Source Location (Исходное местоположение)

Информация о месте в коде: файл, функция, строка. Автоматически захватывается для зон.

### Symbol Resolution (Разрешение символов)

Процесс преобразования адресов в имена функций. Выполняется сервером Tracy.

---

## T

### Thread (Поток)

Единица выполнения. Tracy автоматически отслеживает все потоки, использующие макросы профилирования.

### Timestamp Query (Timestamp-запрос)

GPU запрос для измерения времени. Используется для GPU зон.

### Timeline (Таймлайн)

Визуализация временной шкалы выполнения в интерфейсе Tracy.

### Trace (Трейс)

Запись событий профилирования. Сохраняется в файл `.tracy`.

### Transient Zone (Транзиентная зона)

Зона, создаваемая динамически без статического source location. Используется для динамических имён.

---

## V

### VSync

Вертикальная синхронизация. Tracy может отслеживать vsync события на поддерживаемых платформах.

---

## Z

### Zone (Зона)

Основной элемент профилирования — измеряемый участок кода. Создаётся через `ZoneScoped`, `ZoneNamed` и др.

---

## Цветовые коды зон

| Цвет       | HEX        | Типичное использование     |
|------------|------------|----------------------------|
| Красный    | `0xFF0000` | Ошибки, критические секции |
| Зелёный    | `0x00FF00` | Успешные операции          |
| Синий      | `0x0000FF` | Рендеринг                  |
| Жёлтый     | `0xFFFF00` | Ввод/вывод                 |
| Оранжевый  | `0xFF8800` | Физика                     |
| Фиолетовый | `0x8800FF` | Аудио                      |
| Голубой    | `0x00FFFF` | Сеть                       |
| Розовый    | `0xFF00FF` | UI                         |

---

## Форматы данных

### .tracy

Собственный бинарный формат Tracy для сохранения трейсов.

### Экспорт

Tracy поддерживает экспорт в:

- JSON (для интеграции с другими инструментами)
- CSV (для анализа в таблицах)
