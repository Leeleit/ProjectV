# Концепции Tracy

**🟡 Уровень 2: Средний** — Основные концепции и инструменты профилирования.

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
