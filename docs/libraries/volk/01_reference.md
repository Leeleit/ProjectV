# Volk

**Volk — это мост между вашим кодом и драйвером GPU.** Мета-загрузчик, который превращает статическую зависимость от
Vulkan loader в динамический выбор во время выполнения.

---

## Назначение

Volk — библиотека для динамической загрузки функций Vulkan без статической или динамической линковки с системным Vulkan
loader (`vulkan-1.dll` / `libvulkan.so`).

### Проблема традиционного подхода

При стандартной линковке с Vulkan loader:

1. Приложение зависит от `vulkan-1.dll` / `libvulkan.so` при запуске
2. Каждый вызов функции проходит через dispatch цепочку loader'а
3. Dispatch overhead может достигать 7% для device-intensive workloads

### Решение volk

Volk загружает Vulkan loader динамически во время выполнения и предоставляет прямые указатели на функции:

1. Приложение запускается без установленного Vulkan (проверка в runtime)
2. После `volkLoadDevice()` вызовы идут напрямую в драйвер
3. Dispatch overhead устранён

---

## Архитектура

### Vulkan Loader

Vulkan использует динамическую загрузку функций через loader — системную библиотеку `vulkan-1.dll` (Windows) или
`libvulkan.so` (Linux).

```
Приложение
    ↓
vulkan-1.dll (loader) — диспетчеризация
    ↓
Validation Layers — проверки (если включены)
    ↓
Драйвер GPU — выполнение
```

### Мета-загрузчик volk

Volk загружает Vulkan loader динамически во время выполнения и предоставляет прямые указатели на функции.

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

---

## Возможности

| Возможность               | Описание                                   |
|---------------------------|--------------------------------------------|
| Динамическая загрузка     | Загрузка Vulkan loader во время выполнения |
| Автоматические расширения | Загрузка всех entrypoints расширений       |
| Прямые вызовы драйвера    | Оптимизация производительности до 7%       |
| Кроссплатформенность      | Windows, Linux, macOS, Android             |

---

## Характеристики

| Параметр | Значение  |
|----------|-----------|
| Язык     | C89 / C++ |
| Версия   | 2.0+      |
| Vulkan   | 1.0–1.4   |
| Лицензия | MIT       |

---

## Совместимость

Volk совместим со всеми версиями Vulkan от 1.0 до 1.4 и автоматически загружает функции расширений через
`vkGetInstanceProcAddr`.

### Поддерживаемые платформы

- Windows (Win32, UWP)
- Linux (X11, Wayland)
- macOS (MoltenVK)
- Android
- iOS (MoltenVK)

---

## Entrypoints

Vulkan функции загружаются по имени через `vkGetInstanceProcAddr`:

```cpp
PFN_vkCreateInstance vkCreateInstance =
    (PFN_vkCreateInstance)vkGetInstanceProcAddr(NULL, "vkCreateInstance");
```

Volk автоматизирует этот процесс для всех функций.

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

Volk объявляет все Vulkan функции как extern-указатели:

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

Volk заполняет указатели во время выполнения, поэтому прототипы не нужны.

---

## Validation Layers

Volk полностью совместим с validation layers.

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
```

---

## Глоссарий

| Термин                 | Определение                                                     |
|------------------------|-----------------------------------------------------------------|
| **Volk**               | Мета-загрузчик для Vulkan API.                                  |
| **Vulkan Loader**      | Системная библиотека `vulkan-1.dll` / `libvulkan.so`.           |
| **Dispatch overhead**  | Накладные расходы на диспетчеризацию вызовов через loader.      |
| **Entrypoint**         | Точка входа Vulkan функции.                                     |
| **VK_NO_PROTOTYPES**   | Макрос, запрещающий объявления прототипов функций в `vulkan.h`. |
| **VolkDeviceTable**    | Структура для хранения указателей device-функций.               |
| **Global functions**   | Функции, доступные до создания instance (`vkCreateInstance`).   |
| **Instance functions** | Функции, требующие instance (`vkCreateDevice`).                 |
| **Device functions**   | Функции, требующие device (`vkCmdDraw`).                        |

---

## Конфигурационные макросы

| Макрос                      | Описание                                 |
|-----------------------------|------------------------------------------|
| `VOLK_IMPLEMENTATION`       | Включает реализацию в header-only режиме |
| `VOLK_NAMESPACE`            | Помещает символы в namespace `volk::`    |
| `VOLK_NO_DEVICE_PROTOTYPES` | Скрывает прототипы device-функций        |
| `VOLK_VULKAN_H_PATH`        | Кастомный путь к vulkan.h                |

---

## Функции

| Термин                      | Определение                                             |
|-----------------------------|---------------------------------------------------------|
| **volkInitialize**          | Загружает Vulkan loader и глобальные функции.           |
| **volkLoadInstance**        | Загружает instance-функции.                             |
| **volkLoadDevice**          | Загружает device-функции (прямые указатели на драйвер). |
| **volkLoadInstanceOnly**    | Загружает только instance-функции.                      |
| **volkLoadDeviceTable**     | Загружает device-функции в таблицу.                     |
| **volkGetInstanceVersion**  | Возвращает версию Vulkan loader.                        |
| **volkGetLoadedInstance**   | Возвращает текущий загруженный instance.                |
| **volkGetLoadedDevice**     | Возвращает текущий загруженный device.                  |
| **volkGetInstanceProcAddr** | Возвращает указатель на `vkGetInstanceProcAddr`.        |
| **volkGetDeviceProcAddr**   | Возвращает указатель на `vkGetDeviceProcAddr`.          |

---

## Производительность

### Dispatch Overhead

При каждом вызове Vulkan функции через loader:

```
Приложение
    ↓
vulkan-1.dll (loader) — диспетчеризация
    ↓
Validation Layers — проверки (если включены)
    ↓
Драйвер GPU — выполнение
```

Каждый переход добавляет накладные расходы.

### Величина overhead

| Тип нагрузки                                | Overhead через loader  |
|---------------------------------------------|------------------------|
| Device-intensive (vkCmdDraw, vkCmdDispatch) | До 7%                  |
| Instance-intensive (vkCreateDevice)         | Минимальный            |
| Смешанная                                   | Зависит от соотношения |

### Когда использовать volkLoadDevice

**Обязательно:**

- Частые draw/dispatch вызовы (более 100 на кадр)
- Compute-intensive приложения
- Real-time рендеринг

**Не критично:**

- Редкие Vulkan вызовы
- UI-приложения
- Tools и утилиты

---

## Исходный код

Репозиторий: [github.com/zeux/volk](https://github.com/zeux/volk)
