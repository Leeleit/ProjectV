# Решение проблем volk

**🟡 Уровень 2: Средний**

Диагностика и исправление ошибок.

---

## Ошибки компиляции

### VK_NO_PROTOTYPES не определён

**Сообщение:**

```
To use volk, you need to define VK_NO_PROTOTYPES before including vulkan.h
```

**Причина:** `vulkan.h` включён раньше `volk.h` без макроса `VK_NO_PROTOTYPES`.

**Решение:**

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
```

Или в CMake:

```cmake
target_compile_definitions(YourApp PRIVATE VK_NO_PROTOTYPES)
```

---

### Платформенная функция не объявлена

**Сообщение:**

```
'vkCreateWin32SurfaceKHR': undeclared identifier
```

**Причина:** Не определён платформенный макрос.

**Решение:**

```cmake
if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
elseif(APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif()

add_subdirectory(external/volk)
```

---

### VOLK_NAMESPACE требует C++

**Сообщение:**

```
VOLK_NAMESPACE is only supported in C++
```

**Причина:** `VOLK_NAMESPACE` включён, но volk.c компилируется как C.

**Решение:** CMake автоматически переключает на C++ при `VOLK_NAMESPACE ON`. Проверьте конфигурацию.

---

## Ошибки линковки

### Конфликт символов с vulkan-1

**Сообщение:**

```
multiple definition of 'vkCreateInstance'
```

**Причина:** Приложение линкуется с `Vulkan::Vulkan` или `vulkan-1.dll`.

**Решение:**

```cmake
# Неправильно:
target_link_libraries(YourApp PRIVATE Vulkan::Vulkan volk)

# Правильно:
target_link_libraries(YourApp PRIVATE volk)
```

volk сам загружает loader через `volkInitialize()`.

---

### Неразрешённые символы Vulkan

**Сообщение:**

```
undefined reference to 'vkCreateInstance'
```

**Причина:** `VK_NO_PROTOTYPES` определён, но volk не используется.

**Решение:** Добавьте volk или уберите `VK_NO_PROTOTYPES`.

---

### VOLK_IMPLEMENTATION не определён

**Сообщение:**

```
undefined reference to 'volkInitialize'
```

**Причина:** Header-only режим, но `VOLK_IMPLEMENTATION` не определён.

**Решение:**

```cpp
// В одном .cpp файле
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "volk.h"
```

---

## Ошибки во время выполнения

### volkInitialize возвращает ошибку

**Возвращает:** `VK_ERROR_INITIALIZATION_FAILED`

**Причины:**

1. Vulkan не установлен
2. Драйвер не поддерживает Vulkan
3. Повреждён loader

**Решение:**

1. Установите [Vulkan SDK](https://vulkan.lunarg.com/)
2. Обновите драйвер GPU
3. Проверьте наличие `vulkan-1.dll` (Windows) или `libvulkan.so` (Linux)

---

### Крах при вызове vkCmdDraw

**Причина:** Device-функции не загружены.

**Решение:**

```cpp
// После vkCreateDevice
volkLoadDevice(device);

// Теперь vkCmdDraw работает
vkCmdDraw(cmd, ...);
```

---

### NULL указатель функции

**Проверка:**

```cpp
if (vkCmdDraw == nullptr) {
    // Функция не загружена
    // Возможные причины:
    // 1. volkLoadDevice не вызван
    // 2. Расширение не включено при создании device
}
```

**Решение:**

1. Вызовите `volkLoadDevice(device)` после создания device
2. Для расширений — включите их при создании device

---

### Функции расширения не работают

**Причина:** Расширение не включено при создании device.

**Решение:**

```cpp
// Проверьте поддержку расширения
uint32_t count;
vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
std::vector<VkExtensionProperties> extensions(count);
vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, extensions.data());

// Включите при создании device
const char* enabledExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    // ... другие расширения
};

VkDeviceCreateInfo deviceInfo = {};
deviceInfo.enabledExtensionCount = 1;
deviceInfo.ppEnabledExtensionNames = enabledExtensions;
```

---

## Проблемы с несколькими устройствами

### Конфликт глобальных указателей

**Причина:** `volkLoadDevice` перезаписывает глобальные указатели.

**Решение:** Используйте таблицы:

```cpp
VolkDeviceTable tables[2];
volkLoadDeviceTable(&tables[0], device0);
volkLoadDeviceTable(&tables[1], device1);

// Вызовы через таблицы
tables[0].vkCmdDraw(...);
tables[1].vkCmdDraw(...);
```

---

## Диагностика

### Проверка инициализации

```cpp
bool isVolkInitialized() {
    // Проверка версии
    if (volkGetInstanceVersion() == 0) {
        return false;
    }
    return true;
}

bool isDeviceLoaded() {
    return volkGetLoadedDevice() != VK_NULL_HANDLE;
}

bool isInstanceLoaded() {
    return volkGetLoadedInstance() != VK_NULL_HANDLE;
}
```

### Проверка функций

```cpp
void checkFunctions() {
    printf("vkCreateInstance: %s\n", vkCreateInstance ? "OK" : "NULL");
    printf("vkCreateDevice: %s\n", vkCreateDevice ? "OK" : "NULL");
    printf("vkCmdDraw: %s\n", vkCmdDraw ? "OK" : "NULL");
    printf("vkQueueSubmit: %s\n", vkQueueSubmit ? "OK" : "NULL");
}
```

### Проверка расширений

```cpp
bool checkExtension(VkPhysicalDevice physicalDevice, const char* name) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, extensions.data());

    for (const auto& ext : extensions) {
        if (strcmp(ext.extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}
```

---

## Чек-лист диагностики

1. `VK_NO_PROTOTYPES` определён перед `volk.h`?
2. Приложение НЕ линкуется с `Vulkan::Vulkan`?
3. `volkInitialize()` вызван и вернул `VK_SUCCESS`?
4. `volkLoadInstance(instance)` вызван после `vkCreateInstance`?
5. `volkLoadDevice(device)` вызван после `vkCreateDevice`?
6. Платформенные defines переданы в `VOLK_STATIC_DEFINES`?
7. Для header-only: `VOLK_IMPLEMENTATION` определён в одном файле?

---

## Отладка с validation layers

volk совместим с validation layers:

```cpp
const char* layers[] = {"VK_LAYER_KHRONOS_validation"};

VkInstanceCreateInfo createInfo = {};
createInfo.enabledLayerCount = 1;
createInfo.ppEnabledLayerNames = layers;

vkCreateInstance(&createInfo, nullptr, &instance);
volkLoadInstance(instance);
```

Layers перехватят вызовы и выведут диагностику.
