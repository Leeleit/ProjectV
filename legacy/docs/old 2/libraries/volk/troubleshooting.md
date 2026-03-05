# Решение проблем

🟢 Уровень 1: Начинающий

Частые ошибки при использовании volk и способы их исправления.

## Ошибки компиляции

### `error: To use volk, you need to define VK_NO_PROTOTYPES before including vulkan.h`

**Причина:** `vulkan.h` (или заголовок, который его включает — VMA, SDL_vulkan и т.д.) подключён раньше volk.h, и
`VK_NO_PROTOTYPES` не определён.

**Решение:** В начале файла (до любых `#include`) добавьте:

```cpp
#define VK_NO_PROTOTYPES
```

Или убедитесь, что volk.h включается первым, до всех зависимостей, которые тянут vulkan.h.

---

### `vkCreateWin32SurfaceKHR` (или другая платформенная функция) не объявлена

**Причина:** Не задан платформенный define для Vulkan. На Windows нужен `VK_USE_PLATFORM_WIN32_KHR`.

**Решение:** В CMake перед `add_subdirectory(external/volk)`:

```cmake
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)  # Windows
# Linux X11:  VK_USE_PLATFORM_XLIB_KHR
# Linux Wayland: VK_USE_PLATFORM_WAYLAND_KHR
# macOS: VK_USE_PLATFORM_MACOS_MVK
```

---

### `VOLK_NAMESPACE is only supported in C++`

**Причина:** Опция `VOLK_NAMESPACE` включена, но volk.c компилируется как C (namespace в C не поддерживается).

**Решение:** Либо отключите `VOLK_NAMESPACE` в CMake, либо volk автоматически переключится на C++ при включённой опции —
проверьте, что CMake передаёт её корректно.

**Подробнее:** Когда `VOLK_NAMESPACE` включен, volk автоматически компилирует volk.c как C++ код. Убедитесь, что ваш
CMake проект обрабатывает это правильно. Если вы компилируете volk.c вручную, используйте флаг C++ компилятора.

---

## Ошибки линковки

### Символы `vkCreateInstance`, `vkEnumerateInstanceExtensionProperties` и т.д. не найдены

**Причина:** Приложение линкуется с Vulkan loader (`vulkan-1` / `libvulkan`), но при этом используется
`VK_NO_PROTOTYPES`. Линкер не находит реализации, потому что прототипы отключены, а volk предоставляет их через
указатели во время выполнения.

**Решение:** Убедитесь, что вы **не** линкуете `vulkan` или `Vulkan::Vulkan` в исполняемый файл. `find_package(Vulkan)`
нужен только для заголовков и путей include. volk сам загружает loader в `volkInitialize()`.

Если VMA или другая библиотека требует Vulkan::Vulkan — используйте `VK_NO_PROTOTYPES` во всех единицах трансляции,
которые включают Vulkan/volk, и не линкуйте loader в executable.

---

## Ошибки во время выполнения

### `volkInitialize()` возвращает `VK_ERROR_INITIALIZATION_FAILED`

**Причина:** Vulkan loader не найден. На Windows — отсутствует `vulkan-1.dll`; на Linux — `libvulkan.so.1` или
`libvulkan.so`.

**Решение:**

1. Установите [Vulkan SDK](https://vulkan.lunarg.com/) (включает loader и validation layers).
2. Или обновите драйвер видеокарты — современные драйверы NVIDIA, AMD, Intel поставляют Vulkan loader.
3. На Linux: установите пакет `libvulkan1` (Debian/Ubuntu) или аналог для вашего дистрибутива.

---

### Вызов `vkCmdDraw` или другой device-функции приводит к падению (NULL pointer, crash)

**Причина:** `volkLoadDevice(device)` не был вызван после создания device, или вы используете `volkLoadInstanceOnly` и
забыли загрузить device через `volkLoadDeviceTable`.

**Решение:** Сразу после `vkCreateDevice` вызовите:

```cpp
volkLoadDevice(device);
```

Либо при table-based интерфейсе — `volkLoadDeviceTable(&table, device)` и вызывайте функции через
`table.vkCmdDraw(...)`.

---

---

## Диагностика с Tracy

### Профилирование Vulkan вызовов

Tracy может измерять время выполнения Vulkan функций. При использовании volk важно передавать правильные указатели на
функции.

```cpp
#include "volk.h"
#include "TracyVulkan.hpp"

// После volkLoadDevice(device)
if (volkGetLoadedDevice() != VK_NULL_HANDLE) {
    // Tracy использует указатели volk для точных измерений
    auto vkGetInstanceProcAddr = volkGetInstanceProcAddr;
    auto vkGetDeviceProcAddr = volkGetDeviceProcAddr;
    
    TracyVkCtx TracyVkContext = TracyVkContextCalibrated(
        physicalDevice,
        device,
        queue,
        commandBuffer,
        vkGetInstanceProcAddr,
        vkGetDeviceProcAddr
    );
}
```

**Ошибка:** Tracy показывает неверное время выполнения Vulkan вызовов.

**Причина:** Переданы стандартные указатели Vulkan loader вместо volk указателей.

**Решение:** Используйте `volkGetInstanceProcAddr` и `volkGetDeviceProcAddr` для инициализации Tracy.

---

### Измерение Dispatch Overhead

С volk можно измерить реальный выигрыш от пропуска dispatch цепочки:

```cpp
// Включите Tracy профилирование
ZoneScopedN("Vulkan Draw Call");

// Вызов через volk (прямой доступ к драйверу)
vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
```

**Анализ:** Сравните трассы Tracy с volk и без него. Ожидаемое улучшение — 6-7% для частых вызовов.

---

### Диагностика проблем производительности в воксельном рендерере

**Симптом:** Низкий FPS при рендеринге миллионов вокселей.

**Диагностика с Tracy:**

1. Включите Vulkan profiling в Tracy
2. Измерьте время `vkCmdDraw` вызовов
3. Сравните с CPU временем подготовки данных

**Возможные причины:**

- Dispatch overhead (если volk не инициализирован правильно)
- Конфликты с validation layers (если включены в релизе)
- Неправильная интеграция с VMA

**Решение:**

```cpp
// Проверка инициализации volk
if (volkGetLoadedDevice() == VK_NULL_HANDLE) {
    // volkLoadDevice не был вызван
    volkLoadDevice(device);
}

// Профилирование с Tracy
TracyVkZone(TracyVkContext, commandBuffer, "Voxel Rendering");
for (const auto& chunk : voxelChunks) {
    vkCmdDraw(commandBuffer, chunk.vertexCount, 1, 0, 0);
}
```

---

## Дополнительные инструменты отладки

### Валидация с Vulkan Layers

volk полностью совместим с validation layers. Включите их для отладки:

```cpp
const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
VkInstanceCreateInfo instanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
instanceInfo.ppEnabledLayerNames = layers;
instanceInfo.enabledLayerCount = 1;
```

**Ошибка:** Validation layers не работают с volk.

**Реальность:** Работают корректно. volk загружает функции через loader, который включает layers.

---

### Интеграция с RenderDoc

RenderDoc работает с volk без дополнительной настройки. Для захвата фрейма:

1. Убедитесь, что `volkLoadDevice` был вызван
2. Запустите приложение через RenderDoc
3. RenderDoc перехватит вызовы Vulkan через volk указатели

**Проблема:** RenderDoc не видит Vulkan вызовы.

**Решение:** Убедитесь, что вы не смешиваете volk и стандартный Vulkan loader в одном процессе.

---

## CMake targets и конфигурация

### Доступные CMake targets

volk предоставляет два основных CMake target:

1. **`volk`** — статическая библиотека
   ```cmake
   add_subdirectory(external/volk)
   target_link_libraries(my_application PRIVATE volk)
   ```

   Платформенные defines можно передать через `VOLK_STATIC_DEFINES`:
   ```cmake
   if (WIN32)
      set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
   elseif(UNIX AND NOT APPLE)
      set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
   endif()
   add_subdirectory(volk)
   target_link_libraries(my_application PRIVATE volk)
   ```

2. **`volk_headers`** — интерфейсный target для header-only режима
   ```cmake
   add_subdirectory(external/volk)
   target_link_libraries(my_application PRIVATE volk_headers)
   ```

   В коде:
   ```cpp
   #define VOLK_IMPLEMENTATION
   #include "volk.h"
   ```

### VOLK_NAMESPACE

**Проблема:** Конфликты символов при смешивании volk с прямым использованием Vulkan.

**Решение:** Включите опцию `VOLK_NAMESPACE` в CMake:

```cmake
set(VOLK_NAMESPACE ON)
add_subdirectory(external/volk)
```

Или определите вручную при компиляции volk.c:

```cpp
#define VOLK_NAMESPACE
```

При этом все символы volk помещаются в namespace `volk::`. Требуется компиляция volk.c в режиме C++ (происходит
автоматически при использовании CMake).

### VOLK_NO_DEVICE_PROTOTYPES

**Проблема:** Ошибки компиляции при использовании `volkLoadInstanceOnly` и `volkLoadDeviceTable` без правильного
обращения к функциям.

**Решение:** Определите `VOLK_NO_DEVICE_PROTOTYPES`, чтобы скрыть прототипы device-функций:

```cpp
#define VOLK_NO_DEVICE_PROTOTYPES
#include "volk.h"
```

Это позволяет компилятору обнаруживать ошибки использования device-функций без предварительной загрузки через
`volkLoadDevice`.

### VOLK_INSTALL и find_package

По умолчанию установка отключена. Для использования через `find_package`:

```bash
cmake -DVOLK_INSTALL=ON ..
cmake --build . --target install
```

В проекте:

```cmake
find_package(volk CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE volk::volk)
```

Импортированные targets: `volk::volk` и `volk::volk_headers`.

---

## См. также

- [Интеграция](integration.md) — полный порядок инициализации и интеграция с другими библиотеками
- [Производительность](performance.md) — бенчмарки и оптимизации
- [Практические сценарии](use-cases.md) — примеры использования volk
- [Архитектура и концепции](concepts.md) — глубокое понимание работы volk
- [Интеграция с ProjectV](projectv-integration.md) — специфичные паттерны для воксельного движка
