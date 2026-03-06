# Решение проблем Tracy

**🟡 Уровень 2: Средний**

Устранение ошибок, оптимизация производительности и решение распространённых проблем при интеграции Tracy в приложения.

## На этой странице

- [Ошибки компиляции](#ошибки-компиляции)
- [Проблемы с подключением](#проблемы-с-подключением)
- [Отсутствие данных](#отсутствие-данных)
- [Высокий overhead](#высокий-overhead)
- [Проблемы с Vulkan](#проблемы-с-vulkan)
- [Проблемы с памятью](#проблемы-с-памятью)
- [Оптимизация производительности](#оптимизация-производительности)
- [Совместимость платформ](#совместимость-платформ)
- [Расширенная диагностика](#расширенная-диагностика)

---

## Ошибки компиляции

### Ошибка: "tracy/Tracy.hpp: No such file or directory"

**Проблема:** Заголовочный файл Tracy не найден.

**Решение:**

1. Убедитесь, что подмодуль Tracy добавлен:

```bash
git submodule update --init --recursive
```

2. Проверьте путь в CMake:

```cmake
# Правильный путь
add_subdirectory(external/tracy)
```

3. Убедитесь, что заголовочные файлы доступны:

```bash
ls external/tracy/public/tracy/Tracy.hpp
```

### Ошибка: "undefined reference to `tracy::CreateProfile()`"

**Проблема:** Не подключена библиотека Tracy.

**Решение:**
Добавьте линковку в CMake:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
```

### Ошибка: "TRACY_VULKAN is not defined"

**Проблема:** Vulkan поддержка не включена.

**Решение:**

```cmake
target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_VULKAN)
```

И убедитесь, что Vulkan SDK установлен.

### Ошибка: "macro ‘ZoneScoped’ requires 0 arguments, but 1 given"

**Проблема:** Неправильное использование макроса.

**Решение:**

```cpp
// Правильно:
ZoneScoped;  // Без аргументов
ZoneScopedN("MyZone");  // С именем

// Неправильно:
ZoneScoped("MyZone");  // ZoneScoped не принимает аргументы
```

### Ошибка компиляции в C++ режиме

**Проблема:** Tracy требует C++11 или новее.

**Решение:**

```cmake
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

---

## Проблемы с подключением

### Сервер Tracy не запускается

**Проблема:** Не удаётся запустить сервер Tracy.

**Решение:**

1. Проверьте наличие бинарника:

```bash
# Linux/macOS
./external/tracy/profiler/build/unix/tracy-release

# Windows
external\tracy\profiler\build\win64\Release\tracy.exe
```

2. Соберите сервер при необходимости:

```bash
cd external/tracy
mkdir build && cd build
cmake ..
make -j$(nproc)
```

3. Проверьте зависимости:

- Windows: Visual C++ Redistributable
- Linux: libcap, libgl, libfreetype
- macOS: X11, freetype

### "Unable to connect to server"

**Проблема:** Клиент не может подключиться к серверу.

**Решение:**

1. Проверьте, запущен ли сервер:

```bash
# Проверка процесса
ps aux | grep tracy
```

2. Проверьте порт по умолчанию (8947):

```bash
# Linux/macOS
netstat -an | grep 8947

# Windows
netstat -an | findstr 8947
```

3. Проверьте firewall:

```bash
# Linux
sudo ufw allow 8947/tcp
sudo ufw allow 8080/tcp  # Web UI

# Windows
# Разрешите tracy.exe в Windows Firewall
```

4. Укажите IP явно:

```cpp
tracy::SetServerAddress("127.0.0.1:8947");
```

### "Connection refused" или "Connection timeout"

**Проблема:** Сетевая проблема.

**Решение:**

1. Проверьте localhost:

```cpp
tracy::SetServerAddress("localhost:8947");
```

2. Используйте 127.0.0.1 вместо localhost:

```cpp
tracy::SetServerAddress("127.0.0.1:8947");
```

3. Проверьте антивирус/брандмауэр.

### Web UI не открывается

**Проблема:** Браузер не может подключиться к Web UI.

**Решение:**

1. Проверьте порт 8080:

```bash
# Linux/macOS
netstat -an | grep 8080

# Windows
netstat -an | findstr 8080
```

2. Откройте в браузере:

```
http://localhost:8080
http://127.0.0.1:8080
```

3. Проверьте запущен ли сервер с Web UI:

```bash
./tracy-release --web 8080
```

---

## Отсутствие данных

### Зоны не отображаются в timeline

**Проблема:** Зоны профилирования создаются, но не отображаются.

**Решение:**

1. Проверьте, включено ли профилирование:

```cpp
#ifdef TRACY_ENABLE
    ZoneScopedN("TestZone");
#endif
```

2. Проверьте подключение к серверу:

```cpp
if (tracy::IsConnected()) {
    TracyMessageL("Connected to Tracy server");
}
```

3. Проверьте переполнение буфера:

```cpp
// Увеличьте размер буфера
tracy::SetBufferSize(32 * 1024 * 1024);  // 32 MB
```

4. Проверьте фильтрацию в Web UI:

- Уберите фильтры в интерфейсе
- Проверьте все потоки (threads)

### Кадры не отображаются

**Проблема:** `FrameMark` не создаёт отметки кадров.

**Решение:**

1. Проверьте вызов `FrameMark`:

```cpp
while (running) {
    FrameMark;  // Должен быть в основном цикле
    
    // ...
}
```

2. Используйте именованные кадры для отладки:

```cpp
FrameMarkNamed("MainLoop");
```

3. Проверьте настройки захвата кадров:

```cpp
// Включите захват кадров
#ifndef TRACY_NO_FRAME_IMAGE
    // Захват активен
#endif
```

### Графики не обновляются

**Проблема:** `TracyPlot` не добавляет точки на графики.

**Решение:**

1. Проверьте частоту вызовов:

```cpp
// Слишком частые вызовы могут быть пропущены
static int counter = 0;
if (++counter % 10 == 0) {
    TracyPlot("MyMetric", value);
}
```

2. Проверьте тип значения:

```cpp
// Правильные типы:
TracyPlot("IntMetric", (int64_t)value);
TracyPlot("FloatMetric", (float)value);
TracyPlot("DoubleMetric", (double)value);
```

3. Настройте график:

```cpp
TracyPlotConfig("MyMetric", tracy::PlotFormatType::Number, true, 0, 0xFF0000);
```

### Сообщения не появляются

**Проблема:** `TracyMessage` не отправляет сообщения.

**Решение:**

1. Проверьте длину строки:

```cpp
std::string msg = "My message";
TracyMessage(msg.c_str(), msg.size());  // Правильно

TracyMessage("Short", 5);  // Правильно
TracyMessage("Too long without size", 0);  // Неправильно
```

2. Используйте `TracyMessageL` для литералов:

```cpp
TracyMessageL("This is a message");
```

3. Проверьте переполнение буфера сообщений.

---

## Высокий overhead

### Измерение overhead Tracy

**Проблема:** Tracy добавляет слишком много overhead.

**Решение:**

1. Измерьте overhead:

```cpp
auto start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 1000000; i++) {
    ZoneScopedN("OverheadTest");
}
auto end = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
float overheadPerZone = duration.count() / 1000000.0f;  // микросекунды на зону
```

2. Ожидаемые значения:

- `ZoneScoped`: 10-20 нс
- `ZoneScopedN`: 15-30 нс
- `FrameMark`: <1 нс
- `TracyPlot`: 20-50 нс

### Снижение overhead в релизе

**Проблема:** Высокий overhead в production сборках.

**Решение:**

1. Используйте conditional compilation:

```cpp
#ifdef TRACY_ENABLE
    #define PROFILE_ZONE(name) ZoneScopedN(name)
    #define PROFILE_FRAME() FrameMark
#else
    #define PROFILE_ZONE(name)
    #define PROFILE_FRAME()
#endif
```

2. Настройте CMake для релиза:

```cmake
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(ProjectV PRIVATE
            TRACY_ONLY_FRAME
            TRACY_NO_CALLSTACK
            TRACY_NO_BROADCAST
            TRACY_LOW_OVERHEAD
    )
endif ()
```

3. Отключите тяжёлые функции:

```cpp
#ifndef TRACY_NO_FRAME_IMAGE
    // Захват кадров отключён
#endif
```

### Оптимизация частых зон

**Проблема:** Зоны в tight loops создают высокий overhead.

**Решение:**

1. Группируйте зоны:

```cpp
// Плохо:
for (int i = 0; i < 1000; i++) {
    ZoneScopedN("ProcessItem");
    processItem(i);
}

// Хорошо:
{
    ZoneScopedN("ProcessAllItems");
    for (int i = 0; i < 1000; i++) {
        processItem(i);
    }
}
```

2. Используйте sampling:

```cpp
static int counter = 0;
for (int i = 0; i < 1000; i++) {
    if (++counter % 100 == 0) {
        ZoneScopedN("SampledProcess");
    }
    processItem(i);
}
```

3. Отключайте зоны conditionally:

```cpp
bool detailedProfiling = false;  // Настраивается во время выполнения

ZoneTransient("DetailedZone", detailedProfiling);
if (detailedProfiling) {
    // Детальное профилирование
}
```

### Управление частотой данных

**Проблема:** Слишком много данных перегружает сервер.

**Решение:**

1. Ограничьте частоту графиков:

```cpp
static int plotCounter = 0;
if (++plotCounter % 60 == 0) {  // Каждую секунду при 60 FPS
    TracyPlot("FPS", framesPerSecond);
}
```

2. Используйте буферизацию:

```cpp
tracy::SetBufferSize(64 * 1024 * 1024);  // 64 MB буфер
```

3. Включите компрессию:

```cpp
tracy::EnableCompression(true);
```

---

## Проблемы с Vulkan

### Ошибка: "TracyVkContext failed"

**Проблема:** Не удалось создать Vulkan контекст Tracy.

**Решение:**

1. Проверьте параметры Vulkan:

```cpp
VkPhysicalDevice physicalDevice = ...;
VkDevice device = ...;
VkQueue queue = ...;
uint32_t queueFamilyIndex = ...;

// Убедитесь, что параметры valid
if (physicalDevice && device && queue) {
    vkCtx = TracyVkContext(physicalDevice, device, queue, queueFamilyIndex);
}
```

2. Проверьте поддержку Vulkan:

```cpp
// Проверка доступности Vulkan
if (!checkVulkanSupport()) {
    TracyMessageL("Vulkan not supported, Tracy Vulkan disabled");
    vkCtx = nullptr;
}
```

3. Проверьте очередь:

```cpp
// Должна быть graphics или compute очередь
VkQueueFamilyProperties props;
vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, &props);

// Проверьте поддержку
if (props[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
    // Поддерживается
}
```

### GPU зоны не отображаются

**Проблема:** `TracyVkZone` создаётся, но не отображается в timeline.

**Решение:**

1. Проверьте вызов `TracyVkCollect`:

```cpp
vkBeginCommandBuffer(cmdBuf, ...);
{
    TracyVkZone(vkCtx, cmdBuf, "MyZone");
    // Команды Vulkan...
}
vkEndCommandBuffer(cmdBuf);

// Критически важно: вызвать TracyVkCollect
TracyVkCollect(vkCtx, cmdBuf);

// Затем submit
vkQueueSubmit(queue, ...);
```

2. Проверьте порядок вызовов:

```cpp
// Правильный порядок:
// 1. TracyVkZone в command buffer
// 2. vkEndCommandBuffer
// 3. TracyVkCollect
// 4. vkQueueSubmit
```

3. Проверьте multiple command buffers:

```cpp
// Для каждого command buffer
for (auto& cmdBuf : commandBuffers) {
    TracyVkZone(vkCtx, cmdBuf, "RenderPass");
    // ...
    TracyVkCollect(vkCtx, cmdBuf);
}
```

### Проблемы с multiple queues

**Проблема:** Несколько Vulkan очередей не работают с Tracy.

**Решение:**

1. Создайте отдельный контекст для каждой очереди:

```cpp
// Graphics очередь
auto graphicsCtx = TracyVkContext(physicalDevice, device, graphicsQueue, graphicsFamily);

// Compute очередь  
auto computeCtx = TracyVkContext(physicalDevice, device, computeQueue, computeFamily);
```

2. Используйте правильный контекст:

```cpp
// Graphics commands
TracyVkZone(graphicsCtx, graphicsCmdBuf, "GraphicsPass");

// Compute commands
TracyVkZone(computeCtx, computeCmdBuf, "ComputePass");
```

3. Очищайте правильно:

```cpp
TracyVkDestroy(graphicsCtx);
TracyVkDestroy(computeCtx);
```

### Проблемы с synchronization

**Проблема:** Vulkan synchronization мешает профилированию.

**Решение:**

1. Добавьте барьеры для Tracy:

```cpp
// После TracyVkCollect, перед submit
VkMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT
};

vkCmdPipelineBarrier(cmdBuf, 
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    0, 1, &barrier, 0, nullptr, 0, nullptr);
```

2. Используйте семантику timeline:

```cpp
// Убедитесь, что команды завершены перед чтением данных
VkFence fence;
vkCreateFence(device, &fenceInfo, nullptr, &fence);

vkQueueSubmit(queue, 1, &submitInfo, fence);
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
```

---

## Проблемы с памятью

### Утечки памяти в Tracy

**Проблема:** Tracy сам вызывает утечки памяти.

**Решение:**

1. Проверьте очистку контекста:

```cpp
void cleanup() {
    if (vkCtx) {
        TracyVkDestroy(vkCtx);
        vkCtx = nullptr;
    }
    
    // Дождитесь завершения Tracy
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

2. Используйте `TRACY_MANUAL_LIFETIME`:

```cpp
// В начале main
tracy::StartCapture();

// В конце main
tracy::StopCapture();
```

3. Проверьте аллокаторы:

```cpp
// Используйте Tracy-aware аллокаторы
class TrackedAllocator {
    void* allocate(size_t size) {
        void* ptr = malloc(size);
        TracyAlloc(ptr, size);
        return ptr;
    }
};
```

### Высокое использование памяти Tracy

**Проблема:** Tracy использует слишком много памяти.

**Решение:**

1. Ограничьте размер буфера:

```cpp
tracy::SetBufferSize(16 * 1024 * 1024);  // 16 MB вместо 64 MB
```

2. Отключите сбор ненужных данных:

```cpp
tracy::EnableMemoryTracking(false);
tracy::EnableZoneTracking(false);  // Только кадры
```

3. Используйте circular buffer:

```cpp
tracy::SetCircularBufferSize(32 * 1024 * 1024);  // 32 MB circular buffer
```

### Конфликты с custom аллокаторами

**Проблема:** Custom аллокаторы конфликтуют с Tracy.

**Решение:**

1. Интегрируйте аллокаторы с Tracy:

```cpp
class VoxelAllocator {
public:
    void* allocate(size_t size) {
        void* ptr = aligned_alloc(64, size);
        TracyAllocN(ptr, size, "VoxelData");
        return ptr;
    }
    
    void deallocate(void* ptr) {
        TracyFreeN(ptr, "VoxelData");
        aligned_free(ptr);
    }
};
```

2. Используйте Tracy secure аллокаторы:

```cpp
void* ptr = TracySecureAlloc(size);
// ...
TracySecureFree(ptr);
```

3. Отключите memory tracking если не нужно:

```cpp
#ifndef TRACY_MEMORY
    // memory tracking отключён
#endif
```

---

## Оптимизация производительности

### Профилирование критических участков

**Проблема:** Как определить что профилировать.

**Решение:**

1. Начните с высокоуровневого профилирования:

```cpp
while (running) {
    FrameMark;
    
    {
        ZoneScopedN("FullFrame");
        update();
        render();
    }
}
```

2. Детализируйте медленные участки:

```cpp
void render() {
    ZoneScopedN("Render");
    
    {
        ZoneScopedN("VulkanCommands");
        recordCommands();
    }
    
    {
        ZoneScopedN("Present");
        presentFrame();
    }
}
```

3. Используйте графики для метрик:

```cpp
TracyPlot("FPS", framesPerSecond);
TracyPlot("FrameTimeMs", frameTime);
TracyPlot("VoxelsRendered", voxelCount);
```

### Анализ результатов

**Проблема:** Как интерпретировать данные Tracy.

**Решение:**

1. Ищите самые длинные зоны:

- В Web UI: Statistics → Sort by Total Time
- Критично: зоны > 1 мс при 60 FPS

2. Анализируйте графики:

- Резкие падения FPS
- Постепенное увеличение использования памяти
- Скачки времени выполнения

3. Сравнивайте изменения:

```cpp
// До оптимизации
TracyPlot("BeforeOptimization", performanceMetric);

// После оптимизации  
TracyPlot("AfterOptimization", performanceMetric);
```

### Оптимизация под воксельный движок

**Проблема:** Специфичные проблемы воксельного рендеринга.

**Решение:**

1. Профилируйте chunk processing:

```cpp
void processChunk(Chunk& chunk) {
    ZoneScopedN("ProcessChunk");
    TracyPlot("ChunkSize", chunk.voxelCount);
    
    {
        ZoneScopedN("MeshGeneration");
        generateMesh(chunk);
    }
    
    {
        ZoneScopedN("Lighting");
        calculateLighting(chunk);
    }
}
```

2. Измеряйте memory bandwidth:

```cpp
TracyPlot("VoxelDataMB", voxelDataSize / (1024.0f * 1024.0f));
TracyPlot("TextureMemoryMB", textureMemory / (1024.0f * 1024.0f));
```

3. Оптимизируйте based on data:

```cpp
if (chunk.complexity > threshold) {
    ZoneScopedN("ComplexChunk");
    // Детальная обработка
} else {
    ZoneScopedN("SimpleChunk");
    // Упрощённая обработка
}
```

---

## Совместимость платформ

### Проблемы на Windows

**Проблема:** Tracy не работает на Windows.

**Решение:**

1. Проверьте Visual Studio:

- Версия: VS 2019 или новее
- Режим: Debug/Release x64
- Windows SDK: 10.0.17763.0 или новее

2. Проверьте зависимости:

- Vulkan SDK установлен
- Visual C++ Redistributable

3. Соберите Tracy для Windows:

```bash
cd external/tracy
mkdir build && cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
```

### Проблемы на Linux

**Проблема:** Tracy не работает на Linux.

**Решение:**

1. Проверьте зависимости:

```bash
# Ubuntu/Debian
sudo apt-get install libgl1-mesa-dev libfreetype6-dev libcap-dev

# Fedora
sudo dnf install mesa-libGL-devel freetype-devel libcap-devel
```

2. Проверьте permissions:

```bash
# Tracy может требовать capabilities для sampling
sudo setcap cap_sys_ptrace,cap_syslog=eip ./tracy-release
```

3. Соберите Tracy:

```bash
cd external/tracy
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Проблемы на macOS

**Проблема:** Tracy не работает на macOS.

**Решение:**

1. Установите Xcode Command Line Tools:

```bash
xcode-select --install
```

2. Установите зависимости:

```bash
brew install freetype
```

3. Соберите Tracy:

```bash
cd external/tracy
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
```

### Кроссплатформенные проблемы

**Проблема:** Разное поведение на разных платформах.

**Решение:**

1. Используйте conditional compilation:

```cpp
#ifdef _WIN32
    // Windows-specific код
    tracy::SetServerAddress("127.0.0.1:8947");
#elif __linux__
    // Linux-specific код
    tracy::SetServerAddress("localhost:8947");
#elif __APPLE__
    // macOS-specific код
    tracy::SetServerAddress("127.0.0.1:8947");
#endif
```

2. Тестируйте на всех платформах.

---

## Расширенная диагностика

### Логирование Tracy

**Проблема:** Нужны детальные логи для отладки.

**Решение:**

1. Включите verbose режим:

```cpp
#ifdef TRACY_VERBOSE
    TracyMessageL("Tracy verbose logging enabled");
#endif
```

2. Логируйте события:

```cpp
void traceEvent(const char* event) {
    TracyMessageL(event);
    TracyPlot("EventCount", 1);
}
```

3. Используйте системное логирование:

```bash
# Linux: просмотр логов Tracy
journalctl -f | grep tracy

# Windows: Event Viewer
```

### Отладка с gdb/lldb

**Проблема:** Нужно отлаживать вместе с Tracy.

**Решение:**

1. Запустите с отладчиком:

```bash
# Linux
gdb --args ./ProjectV

# macOS
lldb ./ProjectV
```

2. Установите breakpoints в Tracy:

```bash
break tracy::CreateProfile
break tracy::SetThreadName
```

3. Проверьте состояние Tracy:

```cpp
if (!tracy::IsConnected()) {
    std::cerr << "Tracy not connected" << std::endl;
}
```

### Профилирование в CI/CD

**Проблема:** Интеграция Tracy в pipeline.

**Решение:**

1. Условное включение в CI:

```cmake
if (DEFINED ENV{CI})
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE=0)
else ()
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)
endif ()
```

2. Сохранение артефактов профилирования:

```yaml
# GitHub Actions
- name: Save Tracy profile
  uses: actions/upload-artifact@v2
  with:
    name: tracy-profile
    path: output.tracy
```

3. Автоматический анализ:

```python
# Скрипт для анализа .tracy файлов
```

### Пользовательские инструменты

**Проблема:** Нужны специфичные инструменты для ProjectV.

**Решение:**

1. Создайте wrapper макросы:

```cpp
#define PROFILE_VOXEL_CHUNK(chunk) \
    ZoneScopedN("VoxelChunk"); \
    TracyPlot("ChunkSize", chunk.voxelCount)

#define PROFILE_ECS_SYSTEM(name, count) \
    ZoneScopedN(name); \
    TracyPlot(#name "_Entities", (int64_t)count)
```

2. Интегрируйте с другими инструментами:

```cpp
void profileWithTracyAndImGui() {
    ZoneScopedN("ImGuiRender");
    
    if (ImGui::Begin("Tracy Stats")) {
        ImGui::Text("FPS: %.1f", framesPerSecond);
        ImGui::Text("Frame time: %.2f ms", frameTime);
    }
    ImGui::End();
}
```

3. Создайте custom визуализации:

```cpp
void visualizeVoxelPerformance() {
    TracyPlot("VoxelsVisible", visibleVoxelCount);
    TracyPlot("ChunksLoaded", loadedChunkCount);
    TracyPlot("MemoryUsageMB", memoryUsage / (1024.0f * 1024.0f));
    
    if (performanceDegraded) {
        TracyMessageL("Performance degraded - check voxel rendering");
    }
}
```

---

## Дальше

**Следующий раздел:** [Обновление навигации](../map.md) — добавление Tracy в общую карту документации.

**См. также:**

- [Быстрый старт](quickstart.md) — минимальная интеграция Tracy в ProjectV
- [Интеграция](integration.md) — полная настройка CMake и компонентов
- [Справочник API](api-reference.md) — все макросы и функции Tracy

← [На главную документацию Tracy](../tracy/README.md)