# Tracy: Чистый справочник для студентов

**Tracy** — кроссплатформенный профилировщик реального времени для C и C++. Обеспечивает наносекундное разрешение с
накладными расходами менее 1% CPU. Использует клиент-серверную архитектуру: клиент встраивается в приложение, сервер
визуализирует данные через GUI или браузер.

> **Для понимания:** Tracy — как цифровой диагност автомобиля. Когда механик (программист) ремонтирует двигатель (код),
> он подключает диагностический прибор (Tracy) и видит в реальном времени, какие детали (функции) работают, сколько
> времени тратится на каждую операцию, и где возникла пробка (bottleneck). Всё это без остановки двигателя —
> профилирование идёт параллельно с работой.

## Архитектура

### Клиент-серверная модель

```
┌─────────────────┐         TCP :8947        ┌─────────────────┐
│  Приложение     │ ◄──────────────────────► │  Tracy Server   │
│  (Клиент)       │                         │  (GUI или Web)  │
└─────────────────┘                         └─────────────────┘
```

**Клиент** — заголовочные файлы и макросы, встраиваемые в исходный код. Отправляет данные через TCP.

**Сервер** — отдельное приложение с GUI (Windows/Linux/macOS) или веб-интерфейс (требует WebGL2).

> **Для понимания:** Клиент — это "чёрный ящик" в самолёте, который записывает данные полёта. Сервер — это "центр
> управления", который расшифровывает записи и показывает пилотам, что происходило в кабине во время рейса.

## Основные концепции

### Zones (Зоны)

Зона — измеряемый участок кода с автоматическим захватом времени начала и окончания.

```cpp
#include "tracy/Tracy.hpp"

void processData() {
    ZoneScoped;  // Имя зоны = имя функции

    // Работа...
}
```

**Типы зон:**

```cpp
// Базовая зона (имя функции)
ZoneScoped;

// Именованная зона
ZoneScopedN("DataProcessing");

// Цветная зона (красный 0xFF0000)
ZoneScopedC(0xFF0000);

// Именованная + цветная
ZoneScopedNC("Physics", 0xFF8800);
```

**Ручное управление:**

```cpp
void conditionalWork() {
    ZoneNamed(zone, true);  // Активна если Tracy подключен

    if (someCondition) {
        ZoneText(zone, "Condition met", 13);
    }

    // zone автоматически завершается при выходе
}
```

**Транзиентные зоны** — для динамических имён:

```cpp
void dynamicFunction(const char* name) {
    ZoneTransient(zone, true);
    ZoneTransientN(zone, name, true);

    // Работа...
}
```

### Frames (Кадры)

Логические отметки кадра приложения — игрового цикла или рендеринга.

```cpp
void gameLoop() {
    while (running) {
        FrameMark;  // Отметка начала кадра

        update();
        render();
    }
}
```

**Именованные кадры** — для нескольких независимых циклов:

```cpp
// Игровой цикл
FrameMarkNamed("GameLoop");

// Рендер-цикл
FrameMarkNamed("RenderLoop");

// Физика (несколько шагов за кадр)
for (int i = 0; i < 4; i++) {
    FrameMarkNamed("PhysicsStep");
    simulatePhysics();
}
```

**Вложенные кадры:**

```cpp
void renderFrame() {
    FrameMarkStart("ShadowPass");
    renderShadows();
    FrameMarkEnd("ShadowPass");
}
```

### Plots (Графики)

Визуализация числовых метрик во времени.

```cpp
void updateMetrics() {
    TracyPlot("FPS", static_cast<int64_t>(fps));
    TracyPlot("FrameTime", frameTimeMs);
    TracyPlot("MemoryMB", memoryUsage / (1024.0 * 1024.0));
}
```

**Типы графиков:**

```cpp
// Number — обычные числа
TracyPlotConfig("Value", tracy::PlotFormatType::Number, false, true, 0);

// Memory — автоматическое форматирование KB/MB/GB
TracyPlotConfig("Memory", tracy::PlotFormatType::Memory, false, true, 0);

// Percentage — 0-100%
TracyPlotConfig("CPU", tracy::PlotFormatType::Percentage, false, true, 0);
```

> **Для понимания:** Plots — как график пульса на кардиомониторе. Каждая точка — это "удар сердца" (кадр), а линия
> показывает общее состояние (производительность) во времени. Плоские участки — стабильная работа, пики — проблемы.

### Messages (Сообщения)

Текстовые аннотации в таймлайне.

```cpp
TracyMessage("Loading started", 16);
TracyMessageL("Loading started");  // Авторазмер

// Цветные
TracyMessageC("Error!", 6, 0xFF0000);
TracyMessageLC("Warning", 0xFFFF00);
```

## Профилирование памяти

### Отслеживание аллокаций

```cpp
void* ptr = malloc(size);
TracyAlloc(ptr, size);

free(ptr);
TracyFree(ptr);

// С именем пула
TracyAllocN(ptr, size, "TextureData");
TracyFreeN(ptr, "TextureData");
```

### Безопасные аллокации

Для памяти, которая может быть освобождена после завершения сессии профилирования:

```cpp
TracySecureAlloc(ptr, size);
TracySecureFree(ptr);

TracySecureAllocN(ptr, size, "TempData");
TracySecureFreeN("TempData");
```

> **Для понимания:** TracyAlloc — это "маячок" на грузовике с товаром. Когда майлоп (free) вызывается, Tracy знает, что
> грузовик уехал. Но если программа завершилась, а грузовик ещё не уехал (память не освобождена), безопасные аллокации
> корректно обработают это без ложных предупреждений об утечках.

## Многопоточность

### Автоматическое отслеживание

Tracy автоматически отслеживает все потоки, использующие макросы профилирования.

### Блокировки (Locks)

```cpp
// Tracy поддерживает профилирование блокировок
// Для lock-free структур используйте TracyPlot для отслеживания состояния атомарных переменных:
// TracyPlot("LockFreeCounter", counter.load(std::memory_order_relaxed));
```

**Примечание:** Tracy может профилировать contention даже в lock-free структурах через `TracyPlot` для отслеживания состояния атомарных переменных.

### Fibers

```cpp
#define TRACY_FIBERS

void fiberWork() {
    TracyFiberEnter("MyFiber");
    // Работа...
    TracyFiberLeave;
}

// С группой
TracyFiberEnterHint("WorkerFiber", 0);
```

## Callstack (Стек вызовов)

### Глобальная настройка

```cpp
#define TRACY_CALLSTACK 8  // Глубина 0-62
```

### Явное указание для зоны

```cpp
ZoneScopedS(16);           // Со стеком глубиной 16
ZoneScopedNS("Name", 16);
ZoneScopedCS(0xFF0000, 16);
ZoneScopedNCS("Name", 0xFF0000, 16);
```

### Для аллокаций

```cpp
TracyAllocS(ptr, size, 16);
TracyFreeS(ptr, 16);
```

## GPU профилирование

Tracy поддерживает множество GPU API, но для современного C++ воксельного движка релевантен только **Vulkan**.

### Vulkan

```cpp
#include "tracy/TracyVulkan.hpp"

// Создание контекста
tracy::VkCtx* vkCtx = tracy::CreateVkContext(
    physicalDevice,
    device,
    queue,
    initCommandBuffer  // Для инициализации timestamp queries
);

// Профилирование команд
void render(VkCommandBuffer cmd) {
    TracyVkZone(vkCtx, cmd, "RenderFrame");

    // Команды рендеринга...
    vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

// Сбор данных послеQueueSubmit
TracyVkCollect(vkCtx, queue);

// Очистка
tracy::DestroyVkContext(vkCtx);
```

**Транзиентные GPU зоны:**

```cpp
void renderPass(VkCommandBuffer cmd) {
    TracyVkZoneTransient(vkCtx, zone, cmd, "GBuffer", true);

    // Команды...
    vkCmdDraw(cmd, 0, 1, 0, 0);

    zone = nullptr;  // Завершить
}
```

### Ограничения GPU профилирования

| Параметр              | Типичное значение   |
|-----------------------|---------------------|
| Timestamp resolution  | ~100 ns             |
| Max queries per queue | ~32768              |
| Overhead на зону      | 2 timestamp queries |

> **Для понимания:** GPU timestamp queries — как секундомер на спортивной трассе. Судья (CPU) даёт команду "старт" и "
> финиш", а потом смотрит на время. Но между стартом и финишем гонщик (GPU) едет сам по себе — судья не может вмешаться.
> Данные доступны только когда гонщик пересекает финишную черту (команда выполнена).

## On-Demand профилирование

Режим, при котором данные отправляются только при подключённом сервере.

```cpp
#define TRACY_ON_DEMAND

void hotPath() {
    if (TracyIsConnected) {
        // Дорогая диагностика только когда нужно
        ZoneScopedN("DebugInfo");
    }
}
```

## Конфигурационные макросы

### Основные

| Макрос                 | По умолчанию | Описание                     |
|------------------------|--------------|------------------------------|
| `TRACY_ENABLE`         | Не определён | Включить Tracy               |
| `TRACY_ON_DEMAND`      | 0            | Профилирование по запросу    |
| `TRACY_CALLSTACK`      | 0            | Глубина стека (0-62)         |
| `TRACY_NO_CALLSTACK`   | 0            | Полностью отключить стек     |
| `TRACY_NO_FRAME_IMAGE` | 0            | Без скриншотов кадра         |
| `TRACY_NO_SAMPLING`    | 0            | Без системного сэмплирования |

### Оптимизация

| Макрос                    | Описание              |
|---------------------------|-----------------------|
| `TRACY_LOW_OVERHEAD`      | Минимальный overhead  |
| `TRACY_ONLY_FRAME`        | Только FrameMark      |
| `TRACY_NO_SYSTEM_TRACING` | Без системных событий |

### Пример конфигурации Release

```cpp
#define TRACY_ENABLE
#define TRACY_ON_DEMAND
#define TRACY_CALLSTACK 4
#define TRACY_NO_SAMPLING
#define TRACY_NO_SYSTEM_TRACING
#define TRACY_NO_CONTEXT_SWITCH
```

## Производительность и overhead

### Типичные накладные расходы

| Операция                  | Overhead |
|---------------------------|----------|
| ZoneScoped (без стека)    | ~50 ns   |
| ZoneScopedS (со стеком 8) | ~500 ns  |
| FrameMark                 | ~50 ns   |
| TracyPlot                 | ~100 ns  |
| TracyMessage              | ~200 ns  |
| TracyAlloc/Free           | ~100 ns  |

### Рекомендации

1. **Не профилируйте каждый draw call** — группируйте
2. **Ограничьте глубину стека** — `TRACY_CALLSTACK=4` для Release
3. **Используйте TracyIsConnected** — проверка перед дорогими операциями
4. **Избегайте ZoneText в hot paths** — форматирование строки имеет overhead

> **Для понимания:** Профилирование — как медицинский осмотр. Анализ крови (ZoneScoped) — быстрая процедура, но если
> делать полное МРТ (стек глубиной 62) на каждый чих — пациент устанет, а врач потеряет время. Золотое правило: минимум
> диагностики, максимум информации.

## Глоссарий

| Термин              | Определение                                                             |
|---------------------|-------------------------------------------------------------------------|
| **Zone**            | Измеряемый участок кода с временем начала и окончания                   |
| **Frame**           | Логическая единица выполнения (кадр приложения)                         |
| **Plot**            | Временной ряд числовых значений                                         |
| **Message**         | Текстовая аннотация в таймлайне                                         |
| **Callstack**       | Последовательность вызовов функций                                      |
| **Lock contention** | Ожидание блокировки несколькими потоками                                |
| **Fiber**           | Легковесный поток с кооперативной многозадачей                          |
| **On-demand**       | Режим работы, когда данные отправляются только при подключённом сервере |
| **Timestamp query** | GPU запрос для измерения времени выполнения команд                      |
| **Hot path**        | Участок кода, выполняющийся очень часто (критический путь)              |

## Поддерживаемые платформы

| Платформа     | CPU профилирование | GPU                  | Системное |
|---------------|--------------------|----------------------|-----------|
| Windows x64   | Да                 | Vulkan, D3D11, D3D12 | ETW       |
| Linux x64/ARM | Да                 | Vulkan               | perf      |
| macOS x64/ARM | Да                 | Vulkan, Metal        | —         |
| Android       | Да                 | Vulkan               | ATrace    |

## Лицензия

Tracy распространяется под **BSD 3-Clause** — допускается свободное использование в коммерческих проектах.

## Дополнительные возможности

### Параметры

```cpp
void paramCallback(uint32_t idx, int32_t val) {
    // Обработка
}

TracyParameterRegister(paramCallback, nullptr);
TracyParameterSetup(0, "MaxFPS", false, 60);
```

### Имя программы

```cpp
TracySetProgramName("MyGame v1.0");
```

### Сброс памяти

```cpp
TracyMemoryDiscard("TextureData");
```

## Сравнение с аналогами

| Инструмент   | Overhead | GPU | Real-time | Лицензия  |
|--------------|----------|-----|-----------|-----------|
| Tracy        | <1%      | Да  | Да        | BSD       |
| RenderDoc    | Высокий  | Да  | Нет       | MIT       |
| VTune        | <5%      | Нет | Да        | Коммерция |
| Superluminal | <1%      | Нет | Да        | Коммерция |
