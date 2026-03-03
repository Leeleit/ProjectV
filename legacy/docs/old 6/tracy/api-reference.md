# Справочник API Tracy

**🟡 Уровень 2: Средний**

Полный справочник всех макросов и функций Tracy с примерами использования и информацией о производительности.

## На этой странице

- [Макросы зон](#макросы-зон)
- [Макросы кадров](#макросы-кадров)
- [Графики и сообщения](#графики-и-сообщения)
- [Профилирование памяти](#профилирование-памяти)
- [Vulkan интеграция](#vulkan-интеграция)
- [Управление потоками](#управление-потоками)
- [Управление контекстом](#управление-контекстом)
- [Продвинутые функции](#продвинутые-функции)
- [Конфигурационные макросы](#конфигурационные-макросы)

---

## Макросы зон

### Основные макросы

#### `ZoneScoped`

Создаёт зону профилирования с именем текущей функции.

```cpp
void myFunction() {
    ZoneScoped;  // Имя зоны: "myFunction"
    // ...
}
```

**Параметры:** Нет  
**Overhead:** ~10 нс  
**Пример использования:** Для автоматического профилирования функций.

#### `ZoneScopedN(name)`

Создаёт именованную зону профилирования.

```cpp
void processChunk() {
    ZoneScopedN("ProcessChunk");
    // ...
}
```

**Параметры:**

- `name`: Строковый литерал или `const char*` с именем зоны

**Overhead:** ~15 нс  
**Пример использования:** Для явного именования критичных участков кода.

#### `ZoneScopedC(color)`

Создаёт цветную зону профилирования.

```cpp
void criticalSection() {
    ZoneScopedC(0xFF0000);  // Красная зона
    // ...
}
```

**Параметры:**

- `color`: Цвет в формате 0xRRGGBB

**Overhead:** ~20 нс  
**Пример использования:** Для выделения критичных секций (физика, рендеринг).

#### `ZoneScopedNC(name, color)`

Именованная цветная зона.

```cpp
void renderScene() {
    ZoneScopedNC("SceneRendering", 0x00FF00);  // Зелёная зона
    // ...
}
```

**Параметры:**

- `name`: Имя зоны
- `color`: Цвет

**Overhead:** ~25 нс  
**Пример использования:** Для категоризации различных типов обработки.

### Продвинутые макросы зон

#### `ZoneScopedS(depth)`

Зона с указанной глубиной стека вызовов.

```cpp
void deepFunction() {
    ZoneScopedS(16);  // Запись 16 фреймов стека
    // ...
}
```

**Параметры:**

- `depth`: Глубина стека (0-62)

**Overhead:** Зависит от глубины  
**Использование в ProjectV:** Для отладки сложных call chains в воксельном рендеринге.

#### `ZoneScopedCS(depth, color)`

Цветная зона с глубиной стека.

```cpp
void complexVoxelAlgorithm() {
    ZoneScopedCS(8, 0xFFA500);  // Оранжевая зона с глубиной 8
    // ...
}
```

**Параметры:**

- `depth`: Глубина стека
- `color`: Цвет

**Overhead:** Зависит от глубины  
**Использование в ProjectV:** Для анализа производительности алгоритмов генерации вокселей.

#### `ZoneText(text, size)`

Добавляет текстовое описание к текущей зоне.

```cpp
void loadVoxelData(const std::string& filename) {
    ZoneScopedN("LoadVoxelData");
    ZoneText(filename.c_str(), filename.size());
    // ...
}
```

**Параметры:**

- `text`: Текст описания
- `size`: Длина текста

**Overhead:** ~30 нс  
**Использование в ProjectV:** Для добавления контекста к операциям загрузки чанков.

#### `ZoneName(text, size)`

Изменяет имя текущей зоны.

```cpp
void adaptiveFunction(int mode) {
    ZoneScopedN("AdaptiveFunction");
    
    if (mode == 1) {
        ZoneName("Mode1Processing", 16);
        // ...
    } else {
        ZoneName("Mode2Processing", 16);
        // ...
    }
}
```

**Параметры:**

- `text`: Новое имя зоны
- `size`: Длина текста

**Overhead:** ~25 нс  
**Использование в ProjectV:** Для динамического именования зон в зависимости от режима работы.

### Временные зоны

#### `ZoneTransient(name, active)`

Создаёт временную зону, которая может быть отключена.

```cpp
void voxelProcessing(bool detailedProfiling) {
    ZoneTransient("DetailedVoxelAnalysis", detailedProfiling);
    
    if (detailedProfiling) {
        // Детальное профилирование...
    }
}
```

**Параметры:**

- `name`: Имя зоны
- `active`: Булево значение, включающее/выключающее зону

**Overhead:** ~5 нс (если неактивно)  
**Использование в ProjectV:** Для условного профилирования детальных алгоритмов.

#### `ZoneTransientC(name, active, color)`

Временная цветная зона.

```cpp
void renderWithDebug(bool debugMode) {
    ZoneTransientC("DebugRendering", debugMode, 0xFF00FF);
    
    if (debugMode) {
        // Debug rendering...
    }
}
```

**Параметры:**

- `name`: Имя зоны
- `active`: Включена ли зона
- `color`: Цвет

**Overhead:** ~5 нс (если неактивно)  
**Использование в ProjectV:** Для условного профилирования debug-режимов.

---

## Макросы кадров

### Основные макросы

#### `FrameMark`

Отмечает начало нового кадра.

```cpp
while (running) {
    FrameMark;  // Отметка кадра
    
    updateGame();
    renderFrame();
}
```

**Параметры:** Нет  
**Overhead:** <1 нс  
**Использование в ProjectV:** В игровом цикле для измерения FPS и времени кадра.

#### `FrameMarkNamed(name)`

Отмечает кадр с именем.

```cpp
void renderToTexture() {
    FrameMarkNamed("RenderToTexture");
    // ...
}

void mainRender() {
    FrameMarkNamed("MainRender");
    // ...
}
```

**Параметры:**

- `name`: Имя кадра

**Overhead:** ~5 нс  
**Использование в ProjectV:** Для раздельного профилирования разных проходов рендеринга.

#### `FrameMarkStart(name)`

Начинает именованный кадр.

```cpp
void asyncOperation() {
    FrameMarkStart("AsyncCompute");
    
    // Асинхронные вычисления...
    
    FrameMarkEnd("AsyncCompute");
}
```

**Параметры:**

- `name`: Имя кадра

**Overhead:** ~5 нс  
**Использование в ProjectV:** Для профилирования асинхронных операций.

#### `FrameMarkEnd(name)`

Завершает именованный кадр.

```cpp
// См. пример выше
```

**Параметры:**

- `name`: Имя кадра

**Overhead:** ~5 нс  
**Использование в ProjectV:** В паре с `FrameMarkStart`.

### Измерение метрик кадров

#### `FrameImage(image, width, height, offset, flip)`

Захватывает изображение кадра.

```cpp
void captureFrame() {
    uint32_t* pixels = getFrameBuffer();
    FrameImage(pixels, 1920, 1080, 0, false);
}
```

**Параметры:**

- `image`: Указатель на пиксели
- `width`: Ширина изображения
- `height`: Высота изображения
- `offset`: Смещение в байтах
- `flip`: Отразить по вертикали

**Overhead:** Зависит от размера изображения  
**Использование в ProjectV:** Для захвата кадров при отладке рендеринга вокселей.

#### `FrameImageText(text, size)`

Добавляет текст к захваченному кадру.

```cpp
void debugCapture(const std::string& info) {
    FrameImageText(info.c_str(), info.size());
    // ...
}
```

**Параметры:**

- `text`: Текст
- `size`: Длина текста

**Overhead:** ~20 нс  
**Использование в ProjectV:** Для аннотирования захваченных кадров.

---

## Графики и сообщения

### Графики (Plots)

#### `TracyPlot(name, value)`

Добавляет точку на график.

```cpp
void updateMetrics() {
    TracyPlot("FPS", framesPerSecond);
    TracyPlot("MemoryMB", memoryUsage / (1024.0f * 1024.0f));
    TracyPlot("VoxelCount", (int64_t)voxelCount);
}
```

**Параметры:**

- `name`: Имя графика
- `value`: Числовое значение (int64_t, float, double)

**Overhead:** ~20 нс  
**Использование в ProjectV:** Для отслеживания метрик производительности воксельного движка.

#### `TracyPlotConfig(name, type, step, fillcolor, color)`

Настраивает график.

```cpp
void setupPlots() {
    // Линейный график
    TracyPlotConfig("FPS", tracy::PlotFormatType::Number, true, 0x00FF00, 0xFF0000);
    
    // Гистограмма
    TracyPlotConfig("MemoryDistribution", tracy::PlotFormatType::Memory, false, 0x0000FF, 0);
}
```

**Параметры:**

- `name`: Имя графика
- `type`: Тип графика (`PlotFormatType`)
- `step`: Ступенчатый график
- `fillcolor`: Цвет заливки
- `color`: Цвет линии

**Overhead:** ~10 нс  
**Использование в ProjectV:** Для настройки визуализации метрик.

### Сообщения

#### `TracyMessage(text, size)`

Отправляет текстовое сообщение.

```cpp
void loadChunk(int x, int y, int z) {
    std::string msg = "Loading chunk at " + 
                      std::to_string(x) + "," + 
                      std::to_string(y) + "," + 
                      std::to_string(z);
    TracyMessage(msg.c_str(), msg.size());
    // ...
}
```

**Параметры:**

- `text`: Текст сообщения
- `size`: Длина текста

**Overhead:** ~50 нс  
**Использование в ProjectV:** Для логирования событий загрузки чанков.

#### `TracyMessageL(text)`

Отправляет сообщение из строкового литерала.

```cpp
void initializeEngine() {
    TracyMessageL("Engine initialization started");
    // ...
    TracyMessageL("Engine initialization completed");
}
```

**Параметры:**

- `text`: Строковый литерал

**Overhead:** ~30 нс  
**Использование в ProjectV:** Для фиксированных сообщений.

#### `TracyMessageC(text, size, color)`

Цветное сообщение.

```cpp
void handleError(const std::string& error) {
    TracyMessageC(error.c_str(), error.size(), 0xFF0000);  // Красное сообщение
    // ...
}
```

**Параметры:**

- `text`: Текст
- `size`: Длина текста
- `color`: Цвет

**Overhead:** ~60 нс  
**Использование в ProjectV:** Для выделения ошибок и предупреждений.

#### `TracyMessageLC(text, color)`

Цветное сообщение из литерала.

```cpp
void warning(const char* msg) {
    TracyMessageLC(msg, 0xFFFF00);  // Жёлтое сообщение
}
```

**Параметры:**

- `text`: Строковый литерал
- `color`: Цвет

**Overhead:** ~40 нс  
**Использование в ProjectV:** Для фиксированных предупреждений.

---

## Профилирование памяти

### Аллокации и освобождения

#### `TracyAlloc(ptr, size)`

Отслеживает выделение памяти.

```cpp
void* allocateVoxelData(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        TracyAlloc(ptr, size);
    }
    return ptr;
}
```

**Параметры:**

- `ptr`: Указатель на выделенную память
- `size`: Размер выделения

**Overhead:** ~30 нс  
**Использование в ProjectV:** Для отслеживания аллокаций воксельных данных.

#### `TracyAllocN(ptr, size, name)`

Отслеживает выделение с именем.

```cpp
VoxelChunk* createChunk() {
    VoxelChunk* chunk = new VoxelChunk();
    TracyAllocN(chunk, sizeof(VoxelChunk), "VoxelChunk");
    return chunk;
}
```

**Параметры:**

- `ptr`: Указатель
- `size`: Размер
- `name`: Имя типа

**Overhead:** ~40 нс  
**Использование в ProjectV:** Для категоризации аллокаций по типам.

#### `TracyAllocS(ptr, size, depth)`

Отслеживает выделение с глубиной стека.

```cpp
void* debugAllocate(size_t size) {
    void* ptr = malloc(size);
    TracyAllocS(ptr, size, 8);  // 8 фреймов стека
    return ptr;
}
```

**Параметры:**

- `ptr`: Указатель
- `size`: Размер
- `depth`: Глубина стека

**Overhead:** Зависит от глубины  
**Использование в ProjectV:** Для отладки утечек памяти.

#### `TracyAllocNS(ptr, size, name, depth)`

Комбинированный вариант.

```cpp
// Все параметры вместе
```

**Overhead:** Зависит от глубины  
**Использование в ProjectV:** Для детального отслеживания аллокаций.

#### `TracyFree(ptr)`

Отслеживает освобождение памяти.

```cpp
void freeVoxelData(void* ptr) {
    TracyFree(ptr);
    free(ptr);
}
```

**Параметры:**

- `ptr`: Указатель на освобождаемую память

**Overhead:** ~25 нс  
**Использование в ProjectV:** Для отслеживания освобождений.

#### `TracyFreeN(ptr, name)`

Освобождение с именем.

```cpp
void destroyChunk(VoxelChunk* chunk) {
    TracyFreeN(chunk, "VoxelChunk");
    delete chunk;
}
```

**Параметры:**

- `ptr`: Указатель
- `name`: Имя типа

**Overhead:** ~35 нс  
**Использование в ProjectV:** Для категоризации освобождений.

#### `TracyFreeS(ptr, depth)`

Освобождение с глубиной стека.

```cpp
void debugFree(void* ptr) {
    TracyFreeS(ptr, 8);
    free(ptr);
}
```

**Параметры:**

- `ptr`: Указатель
- `depth`: Глубина стека

**Overhead:** Зависит от глубины  
**Использование в ProjectV:** Для отладки.

### Secure аллокации

#### `TracySecureAlloc(ptr, size)`

Secure аллокация (проверка переполнения).

```cpp
void* secureAllocate(size_t size) {
    void* ptr = TracySecureAlloc(size);
    return ptr;
}
```

**Параметры:**

- `size`: Размер выделения

**Возвращает:** Указатель на выделенную память  
**Overhead:** ~50 нс  
**Использование в ProjectV:** Для критичных аллокаций.

#### `TracySecureFree(ptr)`

Освобождение secure памяти.

```cpp
void secureFree(void* ptr) {
    TracySecureFree(ptr);
}
```

**Параметры:**

- `ptr`: Указатель

**Overhead:** ~30 нс  
**Использование в ProjectV:** В паре с `TracySecureAlloc`.

---

## Vulkan интеграция

### Контекст Vulkan

#### `TracyVkContext(physicalDevice, device, queue, queueFamilyIndex)`

Создаёт контекст Vulkan для Tracy.

```cpp
VkPhysicalDevice physicalDevice = ...;
VkDevice device = ...;
VkQueue queue = ...;
uint32_t queueFamilyIndex = ...;

auto vkCtx = TracyVkContext(physicalDevice, device, queue, queueFamilyIndex);
```

**Параметры:**

- `physicalDevice`: VkPhysicalDevice
- `device`: VkDevice
- `queue`: VkQueue
- `queueFamilyIndex`: Индекс семейства очередей

**Возвращает:** `tracy::VkCtx*`  
**Overhead:** Незначительный (однократно)  
**Использование в ProjectV:** При инициализации Vulkan рендерера.

#### `TracyVkDestroy(context)`

Уничтожает контекст Vulkan.

```cpp
void cleanup() {
    TracyVkDestroy(vkCtx);
}
```

**Параметры:**

- `context`: Контекст Vulkan Tracy

**Overhead:** Незначительный  
**Использование в ProjectV:** При очистке Vulkan ресурсов.

### Зоны Vulkan

#### `TracyVkZone(context, commandBuffer, name)`

Создаёт зону профилирования для Vulkan команд.

```cpp
void recordCommands(VkCommandBuffer cmdBuf) {
    TracyVkZone(vkCtx, cmdBuf, "VoxelRenderPass");
    
    vkCmdBeginRenderPass(cmdBuf, ...);
    // ...
    vkCmdEndRenderPass(cmdBuf);
}
```

**Параметры:**

- `context`: Контекст Vulkan
- `commandBuffer`: VkCommandBuffer
- `name`: Имя зоны

**Overhead:** ~20 нс  
**Использование в ProjectV:** Для профилирования Vulkan команд.

#### `TracyVkZoneC(context, commandBuffer, name, color)`

Цветная зона Vulkan.

```cpp
TracyVkZoneC(vkCtx, cmdBuf, "ComputePass", 0x00FF00);
```

**Параметры:**

- `context`: Контекст
- `commandBuffer`: Командный буфер
- `name`: Имя
- `color`: Цвет

**Overhead:** ~25 нс  
**Использование в ProjectV:** Для категоризации Vulkan операций.

#### `TracyVkZoneTransient(context, zone, commandBuffer, name, active)`

Временная зона Vulkan.

```cpp
TracyVkZoneTransient(vkCtx, zone, cmdBuf, "DebugMarker", debugEnabled);
```

**Параметры:**

- `context`: Контекст
- `zone`: Переменная для хранения зоны
- `commandBuffer`: Командный буфер
- `name`: Имя
- `active`: Активна ли зона

**Overhead:** ~5 нс (если неактивно)  
**Использование в ProjectV:** Для условного профилирования.

### Сбор данных

#### `TracyVkCollect(context, commandBuffer)`

Собирает данные профилирования для командного буфера.

```cpp
void submitCommands(VkCommandBuffer cmdBuf) {
    // Запись команд...
    vkEndCommandBuffer(cmdBuf);
    
    TracyVkCollect(vkCtx, cmdBuf);
    
    vkQueueSubmit(queue, 1, &submitInfo, fence);
}
```

**Параметры:**

- `context`: Контекст Vulkan
- `commandBuffer`: Командный буфер

**Overhead:** ~15 нс  
**Использование в ProjectV:** После записи команд, перед submission.

---

## Управление потоками

### Именование потоков

#### `tracy::SetThreadName(name)`

Устанавливает имя текущего потока.

```cpp
void renderThread() {
    tracy::SetThreadName("RenderThread");
    // ...
}

void physicsThread() {
    tracy::SetThreadName("PhysicsThread");
    // ...
}
```

**Параметры:**

- `name`: Имя потока (C-string)

**Overhead:** ~10 нс (однократно)  
**Использование в ProjectV:** При создании потоков для лучшей идентификации.

#### `tracy::SetThreadName(std::thread& thread, name)`

Устанавливает имя для std::thread.

```cpp
std::thread worker([]() {
    // ...
});
tracy::SetThreadName(worker, "WorkerThread");
```

**Параметры:**

- `thread`: Ссылка на std::thread
- `name`: Имя потока

**Overhead:** ~10 нс  
**Использование в ProjectV:** Для именования потоков извне.

### Fiber поддержка

#### `tracy::FiberEnter(name)`

Вход в fiber.

```cpp
void fiberFunction() {
    tracy::FiberEnter("VoxelFiber");
    // ...
    tracy::FiberLeave();
}
```

**Параметры:**

- `name`: Имя fiber

**Overhead:** ~15 нс  
**Использование в ProjectV:** При использовании fibers для воксельной обработки.

#### `tracy::FiberLeave()`

Выход из fiber.

```cpp
// См. пример выше
```

**Параметры:** Нет  
**Overhead:** ~10 нс  
**Использование в ProjectV:** В паре с `FiberEnter`.

---

## Управление контекстом

### Инициализация и очистка

#### `tracy::StartCapture()`

Начинает захват данных профилирования.

```cpp
void startProfilingSession() {
    tracy::StartCapture();
    TracyMessageL("Profiling session started");
}
```

**Параметры:** Нет  
**Overhead:** Незначительный  
**Использование в ProjectV:** Для начала сессий профилирования.

#### `tracy::StopCapture()`

Останавливает захват данных.

```cpp
void stopProfilingSession() {
    tracy::StopCapture();
    TracyMessageL("Profiling session stopped");
}
```

**Параметры:** Нет  
**Overhead:** Незначительный  
**Использование в ProjectV:** Для завершения сессий.

#### `tracy::IsConnected()`

Проверяет подключение к серверу Tracy.

```cpp
void checkConnection() {
    if (!tracy::IsConnected()) {
        TracyMessageL("Not connected to Tracy server");
    }
}
```

**Возвращает:** `bool`  
**Overhead:** ~5 нс  
**Использование в ProjectV:** Для проверки состояния подключения.

### Настройка подключения

#### `tracy::SetServerAddress(address)`

Устанавливает адрес сервера Tracy.

```cpp
void setupRemoteProfiling() {
    tracy::SetServerAddress("192.168.1.100:8947");
}
```

**Параметры:**

- `address`: Адрес сервера (строка)

**Overhead:** Незначительный  
**Использование в ProjectV:** Для remote profiling.

#### `tracy::SetServerPassword(password)`

Устанавливает пароль для подключения.

```cpp
tracy::SetServerPassword("projectv_dev");
```

**Параметры:**

- `password`: Пароль

**Overhead:** Незначительный  
**Использование в ProjectV:** Для безопасного удалённого профилирования.

#### `tracy::SetBufferSize(size)`

Устанавливает размер буфера.

```cpp
tracy::SetBufferSize(16 * 1024 * 1024);  // 16 MB
```

**Параметры:**

- `size`: Размер буфера в байтах

**Overhead:** Незначительный  
**Использование в ProjectV:** Для настройки под нагрузку воксельного движка.

### Управление данными

#### `tracy::EnableZoneTracking(enable)`

Включает/выключает отслеживание зон.

```cpp
// Отключить зоны для снижения overhead
tracy::EnableZoneTracking(false);
```

**Параметры:**

- `enable`: Включить или выключить

**Overhead:** Незначительный  
**Использование в ProjectV:** Для оптимизации overhead в релизе.

#### `tracy::EnableFrameTracking(enable)`

Включает/выключает отслеживание кадров.

```cpp
tracy::EnableFrameTracking(true);  // Всегда включено
```

**Параметры:**

- `enable`: Включить или выключить

**Overhead:** Незначительный  
**Использование в ProjectV:** Базовое профилирование.

#### `tracy::EnablePlotTracking(enable)`

Включает/выключает отслеживание графиков.

```cpp
tracy::EnablePlotTracking(false);  // Отключить в релизе
```

**Параметры:**

- `enable`: Включить или выключить

**Overhead:** Незначительный  
**Использование в ProjectV:** Для контроля overhead.

#### `tracy::EnableMemoryTracking(enable)`

Включает/выключает отслеживание памяти.

```cpp
tracy::EnableMemoryTracking(true);  // Включить для отладки
```

**Параметры:**

- `enable`: Включить или выключить

**Overhead:** Незначительный  
**Использование в ProjectV:** Для отладки утечек памяти.

---

## Продвинутые функции

### Профилирование вызовов

#### `TracyCallstack(depth)`

Записывает стек вызовов.

```cpp
void debugFunction() {
    TracyCallstack(16);  // Записать 16 фреймов стека
    // ...
}
```

**Параметры:**

- `depth`: Глубина стека

**Overhead:** Зависит от глубины  
**Использование в ProjectV:** Для отладки сложных call chains.

#### `TracyCallstackC(depth, color)`

Записывает стек вызовов с цветом.

```cpp
TracyCallstackC(8, 0xFF00FF);
```

**Параметры:**

- `depth`: Глубина
- `color`: Цвет

**Overhead:** Зависит от глубины  
**Использование в ProjectV:** Для выделенных стеков вызовов.

### Аппаратные счётчики

#### `TracyHwCounter(id, value)`

Записывает значение аппаратного счётчика.

```cpp
void recordPerformanceCounter() {
    uint64_t cycles = readPerformanceCounter();
    TracyHwCounter(0, cycles);
}
```

**Параметры:**

- `id`: ID счётчика
- `value`: Значение

**Overhead:** ~20 нс  
**Использование в ProjectV:** Для низкоуровневого профилирования.

#### `TracyHwCounterConfig(id, name, color)`

Настраивает аппаратный счётчик.

```cpp
TracyHwCounterConfig(0, "CPU Cycles", 0x00FF00);
```

**Параметры:**

- `id`: ID счётчика
- `name`: Имя счётчика
- `color`: Цвет

**Overhead:** Незначительный  
**Использование в ProjectV:** Для настройки мониторинга аппаратных метрик.

### Пользовательские данные

#### `TracyAllocSysTrace(data, size)`

Выделяет память для системного трассирования.

```cpp
void* traceBuffer = TracyAllocSysTrace(4096);
```

**Параметры:**

- `size`: Размер буфера

**Возвращает:** Указатель на буфер  
**Overhead:** Зависит от размера  
**Использование в ProjectV:** Для расширенного трассирования.

#### `TracyFreeSysTrace(ptr)`

Освобождает память системного трассирования.

```cpp
TracyFreeSysTrace(traceBuffer);
```

**Параметры:**

- `ptr`: Указатель на буфер

**Overhead:** Незначительный  
**Использование в ProjectV:** В паре с `TracyAllocSysTrace`.

---

## Конфигурационные макросы

### Компиляционные определения

| Макрос                       | Значение по умолчанию | Описание                                      |
|------------------------------|-----------------------|-----------------------------------------------|
| `TRACY_ENABLE`               | Не определено         | Включить Tracy                                |
| `TRACY_ON_DEMAND`            | 0                     | On-demand profiling                           |
| `TRACY_NO_FRAME_IMAGE`       | 0                     | Отключить захват кадров                       |
| `TRACY_NO_VSYNC_CAPTURE`     | 0                     | Отключить vsync capture                       |
| `TRACY_NO_CONTEXT_SWITCH`    | 0                     | Отключить отслеживание переключений контекста |
| `TRACY_NO_SAMPLING`          | 0                     | Отключить sampling                            |
| `TRACY_NO_BROADCAST`         | 0                     | Отключить broadcast сообщения                 |
| `TRACY_NO_CALLSTACK`         | 0                     | Отключить стек вызовов                        |
| `TRACY_NO_CALLSTACK_INLINES` | 0                     | Отключить inline функции в стеке              |
| `TRACY_NO_EXIT`              | 0                     | Не завершать приложение при ошибках           |
| `TRACY_NO_SYSTEM_TRACING`    | 0                     | Отключить системное трассирование             |
| `TRACY_NO_CODE_TRANSFER`     | 0                     | Отключить передачу кода                       |
| `TRACY_NO_STATISTICS`        | 0                     | Отключить статистику                          |
| `TRACY_MANUAL_LIFETIME`      | 0                     | Ручное управление временем жизни              |
| `TRACY_DELAYED_INIT`         | 0                     | Отложенная инициализация                      |
| `TRACY_FIBERS`               | 0                     | Поддержка fibers                              |
| `TRACY_VULKAN`               | 0                     | Поддержка Vulkan                              |
| `TRACY_OPENGL`               | 0                     | Поддержка OpenGL                              |
| `TRACY_DIRECTX`              | 0                     | Поддержка DirectX                             |
| `TRACY_MEMORY`               | 0                     | Поддержка memory profiling                    |
| `TRACY_LOW_OVERHEAD`         | 0                     | Низкий overhead режим                         |
| `TRACY_CALLSTACK`            | 0                     | Глубина стека вызовов (0-62)                  |
| `TRACY_ONLY_FRAME`           | 0                     | Только отметки кадров                         |
| `TRACY_VERBOSE`              | 0                     | Подробные сообщения                           |

### Пример конфигурации для ProjectV

```cpp
// В CMakeLists.txt
target_compile_definitions(ProjectV PRIVATE
    TRACY_ENABLE
    TRACY_VULKAN
    TRACY_MEMORY
    TRACY_CALLSTACK=8
    TRACY_NO_FRAME_IMAGE
    TRACY_LOW_OVERHEAD
)

// Для Debug сборки
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ProjectV PRIVATE
        TRACY_VERBOSE
        TRACY_NO_EXIT
        TRACY_FIBERS
    )
endif()

// Для Release сборки
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(ProjectV PRIVATE
        TRACY_ONLY_FRAME
        TRACY_NO_CALLSTACK
        TRACY_NO_BROADCAST
    )
endif()
```

---

## Рекомендации для ProjectV

### Оптимальные настройки

1. **Debug/Development:**
   ```cpp
   TRACY_ENABLE
   TRACY_VULKAN
   TRACY_MEMORY
   TRACY_CALLSTACK=8
   TRACY_FIBERS
   TRACY_VERBOSE
   ```

2. **Release/Production:**
   ```cpp
   TRACY_ENABLE
   TRACY_ONLY_FRAME
   TRACY_NO_CALLSTACK
   TRACY_NO_BROADCAST
   TRACY_LOW_OVERHEAD
   ```

3. **Минимальный overhead:**
   ```cpp
   TRACY_ENABLE
   TRACY_ONLY_FRAME
   TRACY_NO_FRAME_IMAGE
   TRACY_NO_VSYNC_CAPTURE
   ```

### Распространённые паттерны

```cpp
// Для воксельного рендеринга
void renderVoxelChunk(VkCommandBuffer cmdBuf, const Chunk& chunk) {
    TracyVkZone(vkCtx, cmdBuf, "RenderChunk");
    TracyPlot("ChunksRendered", 1);
    
    // Рендеринг...
}

// Для ECS систем
TRACY_ECS_SYSTEM(world, UpdateVoxelPhysics, EcsOnUpdate, 
                 VoxelTransform, VoxelPhysics) {
    TracyPlot("VoxelsUpdated", (int64_t)it->count);
    // ...
}

// Для memory tracking
class VoxelAllocator {
    void* allocate(size_t size) {
        void* ptr = malloc(size);
        TracyAllocN(ptr, size, "VoxelData");
        return ptr;
    }
};
```

---

## Дальше

**Следующий раздел:** [Решение проблем](troubleshooting.md) — устранение ошибок и оптимизация.

**См. также:**

- [Быстрый старт](quickstart.md) — минимальная интеграция Tracy в ProjectV
- [Интеграция](integration.md) — полная настройка CMake и компонентов
- [Глоссарий](glossary.md) — термины и определения

← [На главную документации](../README.md)