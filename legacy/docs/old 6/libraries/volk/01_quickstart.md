# Быстрый старт volk

**🟢 Уровень 1: Базовый**

Минимальный пример инициализации и использования.

---

## Минимальный пример

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include <stdio.h>

int main() {
    // 1. Инициализация loader
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
        printf("Vulkan loader not found\n");
        return 1;
    }

    // 2. Создание instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance;
    result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        printf("Failed to create instance\n");
        return 1;
    }

    // 3. Загрузка instance-функций
    volkLoadInstance(instance);

    // 4. Выбор физического устройства
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        printf("No Vulkan devices\n");
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    VkPhysicalDevice physicalDevice;
    vkEnumeratePhysicalDevices(instance, &deviceCount, &physicalDevice);

    // 5. Создание logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = 0;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    VkDevice device;
    result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        printf("Failed to create device\n");
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    // 6. Загрузка device-функций (прямые вызовы драйвера)
    volkLoadDevice(device);

    printf("Volk initialized successfully\n");

    // 7. Очистка
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}
```

---

## Порядок вызовов

```
volkInitialize()
       ↓
vkCreateInstance()
       ↓
volkLoadInstance(instance)
       ↓
vkEnumeratePhysicalDevices()
       ↓
vkCreateDevice()
       ↓
volkLoadDevice(device)
       ↓
Использование Vulkan
       ↓
vkDestroyDevice()
       ↓
vkDestroyInstance()
```

---

## Ключевые моменты

### VK_NO_PROTOTYPES

Обязательный макрос перед включением `volk.h`:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
```

Без этого макроса возникнут конфликты с объявлениями из `vulkan.h`.

### volkInitialize

Загружает Vulkan loader из системы. Возвращает:

- `VK_SUCCESS` — loader найден
- `VK_ERROR_INITIALIZATION_FAILED` — Vulkan не установлен

### volkLoadInstance

Загружает все instance-функции через `vkGetInstanceProcAddr`. После этого можно вызывать:

- `vkEnumeratePhysicalDevices`
- `vkCreateDevice`
- `vkDestroyInstance`
- И все instance-расширения

### volkLoadDevice

Загружает device-функции напрямую из драйвера через `vkGetDeviceProcAddr`. После этого:

- Вызовы `vkCmdDraw`, `vkQueueSubmit` идут напрямую в драйвер
- Dispatch overhead устранён
- Требуется для каждого `VkDevice` отдельно

---

## Проверка версии Vulkan

```cpp
uint32_t version = volkGetInstanceVersion();
if (version == 0) {
    // volkInitialize не вызывался или завершился ошибкой
}

uint32_t major = VK_VERSION_MAJOR(version);
uint32_t minor = VK_VERSION_MINOR(version);
uint32_t patch = VK_VERSION_PATCH(version);

printf("Vulkan %u.%u.%u\n", major, minor, patch);
```

---

## Header-only режим

В одном .cpp файле:

```cpp
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "volk.h"
```

В остальных файлах:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
```

`VOLK_IMPLEMENTATION` должен быть определён только в одной единице трансляции.
