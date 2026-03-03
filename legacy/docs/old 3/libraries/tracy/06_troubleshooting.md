# Устранение неполадок Tracy

**🟡 Уровень 2: Средний** — Решение типичных проблем при работе с Tracy.

---

## Проблемы компиляции

### Ошибка: 'TracyProfiler' is not a member of 'tracy'

**Причина:** Не определён макрос `TRACY_ENABLE`.

**Решение:**

```cmake
target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)
```

### Ошибка: undefined reference to 'tracy::...'

**Причина:** Не линкуется TracyClient.

**Решение:**

```cmake
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
```

### Ошибка: Multiple definition of 'tracy::...'

**Причина:** TracyClient.cpp скомпилирован несколько раз.

**Решение:** Не добавляйте `TracyClient.cpp` вручную. Используйте CMake-цель `Tracy::TracyClient`.

### Ошибки с libatomic на Linux

**Причина:** На некоторых архитектурах требуется libatomic.

**Решение:**

```cmake
find_library(ATOMIC_LIBRARY atomic)
if(ATOMIC_LIBRARY)
    target_link_libraries(YourApp PRIVATE ${ATOMIC_LIBRARY})
endif()
```

---

## Проблемы подключения

### Сервер не видит приложение

**Возможные причины:**

1. **Брандмауэр Windows** — добавьте исключение для порта 8947
2. **Разные сети** — сервер и клиент должны быть в одной сети
3. **Неправильный адрес** — укажите IP явно

**Решение:**

```bash
# Проверка порта
netstat -an | grep 8947

# Подключение к конкретному IP
# В интерфейсе Tracy введите: 192.168.1.100
```

### Приложение падает при подключении

**Причина:** Конфликт с другими инструментами профилирования.

**Решение:**

1. Отключите другие профилировщики
2. Проверьте совместимость с RenderDoc / PIX
3. Обновите драйверы GPU

### Таймаут подключения

**Причина:** Медленная сеть или большая загрузка.

**Решение:**

```cpp
// Увеличить таймаут (до подключения)
#define TRACY_CONNECTION_TIMEOUT 30  // секунд
```

---

## Проблемы производительности

### Высокий overhead профилирования

**Симптомы:** Приложение работает значительно медленнее.

**Решения:**

1. **Уменьшите глубину стека:**

```cpp
#define TRACY_CALLSTACK 4  // Вместо 8 или 16
```

2. **Отключите лишнее:**

```cpp
#define TRACY_NO_SAMPLING
#define TRACY_NO_CONTEXT_SWITCH
#define TRACY_NO_SYSTEM_TRACING
```

3. **Используйте on-demand режим:**

```cpp
#define TRACY_ON_DEMAND
```

4. **Условное профилирование:**

```cpp
if (TracyIsConnected) {
    ZoneScopedN("ExpensiveZone");
}
```

### Большой размер трейса

**Симптомы:** Tracy потребляет много памяти, медленно открывается.

**Решения:**

1. Уменьшите количество зон
2. Отключите FrameImage: `#define TRACY_NO_FRAME_IMAGE`
3. Используйте квоты памяти:

```cpp
// Ограничение буфера (в байтах)
#define TRACY_BUFFER_SIZE (100 * 1024 * 1024)  // 100 MB
```

---

## Проблемы GPU профилирования

### GPU зоны не отображаются

**Проверка:**

1. Вызывается ли `TracyVkCollect` / `TracyGpuCollect`?
2. Поддерживает ли GPU timestamp queries?
3. Правильно ли передана queue?

**Vulkan:**

```cpp
// Проверка поддержки timestamp queries
VkPhysicalDeviceFeatures features;
vkGetPhysicalDeviceFeatures(physicalDevice, &features);
if (!features.shaderStorageImageExtendedFormats) {
    // GPU может не поддерживать нужные функции
}
```

### Ошибка: VK_ERROR_OUT_OF_DEVICE_MEMORY

**Причина:** Слишком много timestamp queries.

**Решение:** Ограничьте количество GPU зон или увеличьте query pool.

---

## Проблемы памяти

### Утечки памяти при профилировании

**Причина:** Несоответствие TracyAlloc / TracyFree.

**Решение:** Проверьте все пары:

```cpp
void* ptr = malloc(size);
TracyAlloc(ptr, size);
// ...
TracyFree(ptr);
free(ptr);
```

### TracyMemoryDiscard не работает

**Причина:** Имя не совпадает с именем в TracyAllocN.

**Решение:**

```cpp
TracyAllocN(ptr, size, "MyData");  // Имя должно совпадать
TracyMemoryDiscard("MyData");      // То же имя
```

---

## Проблемы многопоточности

### Визуальные артефакты в таймлайне

**Причина:** Потоки создаются/уничтожаются динамически.

**Решение:** Tracy автоматически обрабатывает это, но при интенсивном создании потоков могут быть задержки.

### Lock contention не отображается

**Причина:** Используются обычные mutex вместо TracyLockable.

**Решение:**

```cpp
// Было:
std::mutex mtx;

// Стало:
TracyLockable(std::mutex, mtx);
```

---

## Проблемы платформ

### Windows: Ошибка доступа к ETW

**Причина:** Требуются права администратора для системного профилирования.

**Решение:**

```cpp
#define TRACY_NO_SYSTEM_TRACING  // Отключить системную трассировку
```

### Linux: Ошибка доступа к perf

**Причина:** Требуются права root или настройка perf_event_paranoid.

**Решение:**

```bash
# Разрешить perf без root
sudo sysctl kernel.perf_event_paranoid=1

# Или отключить sampling
#define TRACY_NO_SAMPLING
```

### macOS: Sampling не работает

**Причина:** macOS требует специальные права для sampling.

**Решение:**

```cpp
#define TRACY_NO_SAMPLING
```

---

## Отладка Tracy

### Включение verbose режима

```cpp
#define TRACY_VERBOSE
```

### Проверка состояния

```cpp
if (TracyIsConnected) {
    printf("Tracy connected\n");
}

if (TracyIsStarted) {
    printf("Profiling started\n");
}
```

### Логирование сообщений Tracy

```cpp
TracyMessageL("Debug: checkpoint reached");
TracyMessageLC("Warning: low memory", 0xFFFF00);
TracyMessageLC("Error: critical failure", 0xFF0000);
```

---

## Частые ошибки

### Зоны с одинаковыми именами

```cpp
// Плохо: все зоны называются "Update"
for (auto& entity : entities) {
    ZoneScopedN("Update");  // Имя дублируется!
}

// Хорошо: уникальные имена
for (auto& entity : entities) {
    ZoneScopedNC("Update", entity.type);  // Уникальный цвет
}
```

### Зоны вне FrameMark

```cpp
// Плохо: зоны без FrameMark
void gameLoop() {
    while (running) {
        ZoneScopedN("Frame");  // Не то же самое что FrameMark!
        update();
        render();
    }
}

// Хорошо: явный FrameMark
void gameLoop() {
    while (running) {
        FrameMark;
        update();
        render();
    }
}
```

### Неправильный порядок макросов

```cpp
// Ошибка: TracyAlloc после malloc
void* ptr = malloc(size);
TracyAlloc(ptr, size);  // Правильно

// Ошибка: TracyFree до free
TracyFree(ptr);
free(ptr);  // Правильно

// Ошибка: TracyFree без TracyAlloc
void* ptr = malloc(size);
TracyFree(ptr);  // Ошибка! TracyAlloc не был вызван
```

---

## Получение помощи

1. [GitHub Issues](https://github.com/wolfpld/tracy/issues)
2. [Документация Tracy](https://github.com/wolfpld/tracy)
3. Проверьте `external/tracy/README.md`