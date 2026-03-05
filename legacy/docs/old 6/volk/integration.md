# Интеграция volk [🟡 Уровень 2]

**🟡 Уровень 2: Средний** — Настройка сборки и использование.

Полное руководство по интеграции volk в проект, включая CMake, инициализацию, конфигурацию и оптимизацию.

## Оглавление

- [1. CMake интеграция](#1-cmake-интеграция)
- [2. Header-only режим](#2-header-only-режим)
- [3. Инициализация volk](#3-инициализация-volk)
- [4. Интеграция с другими библиотеками](#4-интеграция-с-другими-библиотеками)
- [5. Конфигурация и оптимизация](#5-конфигурация-и-оптимизация)
- [6. Многопоточность и несколько устройств](#6-многопоточность-и-несколько-устройств)

---

## 1. CMake интеграция

volk предоставляет два основных CMake target:

### 1.1. Target `volk` (статическая библиотека)

```cmake
# Добавьте volk как подмодуль или скопируйте в external/
add_subdirectory(external/volk)

# Подключите к вашему приложению
target_link_libraries(YourApp PRIVATE volk)
```

**Платформенные defines:** Передавайте через `VOLK_STATIC_DEFINES`:

```cmake
if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    # Для X11
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
    # Или для Wayland
    # set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
elseif(APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif()

add_subdirectory(external/volk)
target_link_libraries(YourApp PRIVATE volk)
```

### 1.2. Target `volk_headers` (header-only режим)

```cmake
add_subdirectory(external/volk)
target_link_libraries(YourApp PRIVATE volk_headers)
```

**Преимущества:**

- Не требует компиляции volk.c отдельно
- Позволяет настраивать defines в коде
- Удобно для простых проектов

### 1.3. Установка и find_package

```bash
# Включите установку
cmake -DVOLK_INSTALL=ON ..
cmake --build . --target install
```

```cmake
# Использование в проекте
find_package(volk CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE volk::volk)
```

Импортированные targets: `volk::volk` и `volk::volk_headers`.

---

## 2. Header-only режим

### 2.1. Базовое использование

```cpp
// В одном исходном файле (.cpp)
#define VOLK_IMPLEMENTATION
#include "volk.h"

// В других файлах просто включайте
#include "volk.h"
```

### 2.2. Настройка платформенных defines

```cpp
// Определите платформенные defines до включения volk.h
#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VOLK_IMPLEMENTATION
#include "volk.h"
```

### 2.3. Важные макросы

| Макрос                      | Назначение                                           |
|-----------------------------|------------------------------------------------------|
| `VOLK_IMPLEMENTATION`       | Включает реализацию volk (только в одном файле)      |
| `VK_NO_PROTOTYPES`          | Запрещает прототипы функций в vulkan.h (обязательно) |
| `VOLK_NAMESPACE`            | Помещает символы volk в namespace `volk::`           |
| `VOLK_NO_DEVICE_PROTOTYPES` | Скрывает прототипы device-функций                    |

---

## 3. Инициализация volk

### 3.1. Стандартный процесс

```cpp
#include "volk.h"

int main() {
    // 1. Инициализация loader
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
        // Vulkan не доступен
        return -1;
    }
    
    // 2. Создание instance
    VkInstance instance = createVulkanInstance();
    
    // 3. Загрузка instance-функций
    volkLoadInstance(instance);
    
    // 4. Создание device
    VkDevice device = createVulkanDevice(instance);
    
    // 5. Загрузка device-функций (оптимизация производительности)
    volkLoadDevice(device);
    
    // Теперь можно использовать Vulkan
    return 0;
}
```

### 3.2. Проверка состояния

```cpp
// Проверка загруженных instance и device
VkInstance loadedInstance = volkGetLoadedInstance();
VkDevice loadedDevice = volkGetLoadedDevice();

if (loadedInstance != VK_NULL_HANDLE) {
    // instance загружен
}

if (loadedDevice != VK_NULL_HANDLE) {
    // device загружен
}
```

### 3.3. Альтернативные функции

```cpp
// Загрузить только instance-функции (device-функции останутся NULL)
volkLoadInstanceOnly(instance);

// Позже загрузить device-функции
volkLoadDevice(device);

// Или использовать таблицы
VolkDeviceTable table;
volkLoadDeviceTable(&table, device);
// Использовать функции через table.vkCmdDraw(...)
```

---

## 4. Интеграция с другими библиотеками

### 4.1. VMA (Vulkan Memory Allocator)

```cpp
#include "volk.h"
#include "vk_mem_alloc.h"

// После volkLoadDevice(device)
vmaImportVulkanFunctionsFromVolk(&allocator);

// Или вручную
VmaVulkanFunctions vkFuncs = {};
vkFuncs.vkGetInstanceProcAddr = volkGetInstanceProcAddr;
vkFuncs.vkGetDeviceProcAddr = volkGetDeviceProcAddr;
vmaAllocatorCreateInfo(instance, device, &vkFuncs, &allocator);
```

### 4.2. SDL3

```cpp
#include "SDL3/SDL_vulkan.h"
#include "volk.h"

// SDL загружает Vulkan loader самостоятельно
SDL_Init(SDL_INIT_VIDEO);

// Используйте volkInitializeCustom с SDL_vkGetInstanceProcAddr
volkInitializeCustom(SDL_vkGetInstanceProcAddr);

// Дальнейшая инициализация как обычно
```

### 4.3. Tracy (профилирование)

```cpp
#include "volk.h"
#include "TracyVulkan.hpp"

// После volkLoadDevice(device)
TracyVkCtx TracyVkContext = TracyVkContextCalibrated(
    physicalDevice,
    device,
    queue,
    commandBuffer,
    volkGetInstanceProcAddr,  // Используем указатели volk
    volkGetDeviceProcAddr
);
```

### 4.4. GLFW

```cpp
#include "volk.h"
#include "GLFW/glfw3.h"

// GLFW не предоставляет vkGetInstanceProcAddr
// Используйте стандартную инициализацию volk
volkInitialize();

// GLFW создаст surface
GLFWwindow* window = glfwCreateWindow(...);
VkSurfaceKHR surface;
glfwCreateWindowSurface(instance, window, nullptr, &surface);
```

---

## 5. Конфигурация и оптимизация

### 5.1. VOLK_NAMESPACE

**Проблема:** Конфликты символов при смешивании volk с прямым использованием Vulkan.

**Решение:**

```cmake
set(VOLK_NAMESPACE ON)
add_subdirectory(external/volk)
```

```cpp
// В коде используйте namespace
volk::volkInitialize();
volk::volkLoadInstance(instance);
```

### 5.2. VOLK_NO_DEVICE_PROTOTYPES

**Назначение:** Скрывает прототипы device-функций, заставляя использовать таблицы.

```cpp
#define VOLK_NO_DEVICE_PROTOTYPES
#include "volk.h"

// Теперь device-функции недоступны напрямую
// Нужно использовать таблицы
VolkDeviceTable table;
volkLoadDeviceTable(&table, device);
table.vkCmdDraw(...);
```

### 5.3. Dispatch Overhead Оптимизация

**Без volk:** Приложение → Loader → Validation Layers → Driver (7% overhead)
**С volk:** Приложение → Driver (прямой доступ)

```cpp
// До оптимизации (через loader)
vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);

// После volkLoadDevice (прямой доступ к драйверу)
// Та же функция, но указатель ведёт напрямую в драйвер
vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
```

**Измерение выигрыша:** Используйте Tracy для профилирования до и после `volkLoadDevice`.

---

## 6. Многопоточность и несколько устройств

### 6.1. Несколько VkDevice

```cpp
// Для каждого устройства создайте отдельную таблицу
VolkDeviceTable graphicsTable;
VolkDeviceTable computeTable;

volkLoadDeviceTable(&graphicsTable, graphicsDevice);
volkLoadDeviceTable(&computeTable, computeDevice);

// Используйте функции через соответствующие таблицы
graphicsTable.vkCmdDraw(...);
computeTable.vkDispatch(...);
```

### 6.2. Многопоточный рендеринг

```cpp
// В каждом потоке храните свою таблицу
thread_local VolkDeviceTable threadTable;

// При создании потока
void initThread(VkDevice device) {
    volkLoadDeviceTable(&threadTable, device);
}

// В рендеринге
void renderThread() {
    threadTable.vkCmdDraw(...);
}
```

### 6.3. Несколько GPU

```cpp
// Для каждого физического устройства
std::vector<VolkDeviceTable> deviceTables;

for (VkDevice device : devices) {
    VolkDeviceTable table;
    volkLoadDeviceTable(&table, device);
    deviceTables.push_back(table);
}

// Используйте таблицу соответствующего GPU
deviceTables[gpuIndex].vkCmdDraw(...);
```

---

## См. также

- [Решение проблем](troubleshooting.md) — диагностика ошибок инициализации
- [Глоссарий](glossary.md) — определения терминов volk
- [Практические сценарии](use-cases.md) — примеры использования в реальных проектах
- [Производительность](performance.md) — бенчмарки и оптимизации
- [Специализированные паттерны интеграции](projectv-integration.md) — продвинутые сценарии для воксельных движков
