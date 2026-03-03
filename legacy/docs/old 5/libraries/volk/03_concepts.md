# Концепции volk

**🟡 Уровень 2: Средний**

Архитектура мета-загрузчика и принципы работы.

---

## Vulkan Loader

Vulkan использует динамическую загрузку функций через loader — системную библиотеку `vulkan-1.dll` (Windows) или
`libvulkan.so` (Linux).

### Традиционный подход

```
Приложение
    ↓
vulkan-1.dll (loader)
    ↓
Validation Layers
    ↓
Драйвер GPU
```

Каждый вызов функции проходит через dispatch цепочку loader'а.

### Проблемы

1. **Жёсткая зависимость** — приложение не запустится без loader
2. **Dispatch overhead** — до 7% для device-intensive workloads
3. **Конфликты версий** — разные версии loader в системе

---

## Мета-загрузчик volk

volk загружает Vulkan loader динамически во время выполнения и предоставляет прямые указатели на функции.

### Архитектура с volk

```
volkInitialize()
    ↓
Загрузка vulkan-1.dll/libvulkan.so
    ↓
Получение vkGetInstanceProcAddr
    ↓
volkLoadInstance(instance)
    ↓
Загрузка instance-функций
    ↓
volkLoadDevice(device)
    ↓
Прямые указатели на драйвер
```

### Преимущества

| Преимущество     | Описание                                  |
|------------------|-------------------------------------------|
| Runtime загрузка | Проверка наличия Vulkan при запуске       |
| Прямые вызовы    | Устранение dispatch overhead              |
| Контроль         | Приложение решает, когда загружать Vulkan |

---

## Entrypoints

Vulkan функции загружаются по имени через `vkGetInstanceProcAddr`:

```cpp
PFN_vkCreateInstance vkCreateInstance =
    (PFN_vkCreateInstance)vkGetInstanceProcAddr(NULL, "vkCreateInstance");
```

volk автоматизирует этот процесс для всех функций.

### Типы entrypoints

| Тип      | Загрузка                               | Примеры                                                      |
|----------|----------------------------------------|--------------------------------------------------------------|
| Global   | `vkGetInstanceProcAddr(NULL, ...)`     | `vkCreateInstance`, `vkEnumerateInstanceExtensionProperties` |
| Instance | `vkGetInstanceProcAddr(instance, ...)` | `vkCreateDevice`, `vkDestroyInstance`                        |
| Device   | `vkGetDeviceProcAddr(device, ...)`     | `vkCmdDraw`, `vkQueueSubmit`                                 |

---

## Dispatch цепочка

### Без volk

```cpp
// Глобальная функция через loader
vkCmdDraw(cmd, vertexCount, 1, 0, 0);

// Путь: Приложение → Loader → Layers → Драйвер
```

### С volk после volkLoadDevice

```cpp
// Указатель на функцию драйвера
vkCmdDraw(cmd, vertexCount, 1, 0, 0);

// Путь: Приложение → Драйвер (напрямую)
```

---

## Глобальные указатели

volk объявляет все Vulkan функции как extern-указатели:

```c
// В volk.h
extern PFN_vkCreateInstance vkCreateInstance;
extern PFN_vkCmdDraw vkCmdDraw;
extern PFN_vkQueueSubmit vkQueueSubmit;
// ... и так далее для всех функций
```

### Заполнение указателей

```cpp
// После volkInitialize()
vkCreateInstance != nullptr  // Глобальные функции

// После volkLoadInstance(instance)
vkCreateDevice != nullptr    // Instance функции
vkDestroyInstance != nullptr

// После volkLoadDevice(device)
vkCmdDraw != nullptr         // Device функции (прямые)
vkQueueSubmit != nullptr
```

---

## VolkDeviceTable

Структура для хранения указателей device-функций. Используется при работе с несколькими `VkDevice`.

### Объявление

```cpp
struct VolkDeviceTable {
    PFN_vkCmdDraw vkCmdDraw;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkCreateBuffer vkCreateBuffer;
    // ... все device-функции
};
```

### Использование

```cpp
VolkDeviceTable table;
volkLoadDeviceTable(&table, device);

// Вызов через таблицу
table.vkCmdDraw(cmd, vertexCount, 1, 0, 0);
```

---

## VK_NO_PROTOTYPES

Макрос Vulkan, запрещающий объявления прототипов функций в `vulkan.h`.

### Без макроса

```cpp
// vulkan.h объявляет:
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance);

// Линкер ожидает реализацию в vulkan-1.dll
```

### С макросом

```cpp
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// vulkan.h объявляет только типы:
typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance)(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance);

// Нет прототипов — линкер не ищет реализации
```

volk заполняет указатели во время выполнения, поэтому прототипы не нужны.

---

## Validation Layers

volk полностью совместим с validation layers.

### Как это работает

```cpp
const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
createInfo.ppEnabledLayerNames = layers;
createInfo.enabledLayerCount = 1;

vkCreateInstance(&createInfo, nullptr, &instance);
volkLoadInstance(instance);
```

### Dispatch с layers

```
Приложение
    ↓
volk указатель
    ↓
Validation Layer (перехват)
    ↓
Драйвер
```

---

## Выбор стратегии

### Одно устройство

```cpp
volkInitialize();
vkCreateInstance(...);
volkLoadInstance(instance);
vkCreateDevice(...);
volkLoadDevice(device);  // Глобальные указатели → драйвер
```

### Несколько устройств

```cpp
volkInitialize();
vkCreateInstance(...);
volkLoadInstanceOnly(instance);  // Только instance-функции

for (auto& device : devices) {
    vkCreateDevice(...);
    volkLoadDeviceTable(&device.table, device.handle);
}

// Вызовы через таблицы
devices[0].table.vkCmdDraw(...);
devices[1].table.vkCmdDraw(...);
