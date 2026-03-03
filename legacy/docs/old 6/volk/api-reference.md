# Справочник API volk

**🟡 Уровень 2: Средний** — Полный справочник функций и структур volk.

Полные объявления функций и структур находятся в файле [volk.h](../../external/volk/volk.h). Этот справочник
предоставляет подробное описание каждой функции volk, их параметров, возвращаемых значений и рекомендаций по
использованию.

## Обзор функций volk

volk предоставляет следующие функции для управления Vulkan loader и загрузкой функций:

### Основные функции инициализации

| Функция                         | Описание                               | Когда использовать                    |
|---------------------------------|----------------------------------------|---------------------------------------|
| `volkInitialize()`              | Загружает Vulkan loader из системы     | Старт приложения, загрузка Vulkan     |
| `volkInitializeCustom(handler)` | Инициализация с кастомным handler      | Loader уже загружен (например, SDL)   |
| `volkGetInstanceVersion()`      | Возвращает версию Vulkan loader        | Проверить версию до создания instance |
| `volkFinalize()`                | Выгружает loader, сбрасывает указатели | Реинициализация volk                  |

### Функции загрузки функций Vulkan

| Функция                                   | Описание                                      | Когда использовать                      |
|-------------------------------------------|-----------------------------------------------|-----------------------------------------|
| `volkLoadInstance(instance)`              | Загружает все instance- и device-функции      | После `vkCreateInstance`, один device   |
| `volkLoadInstanceOnly(instance)`          | Загружает только instance-функции             | При использовании `volkLoadDeviceTable` |
| `volkLoadDevice(device)`                  | Загружает device-функции напрямую из драйвера | Один device, нужна производительность   |
| `volkLoadDeviceTable(&table, device)`     | Заполняет таблицу device-функциями            | Несколько `VkDevice`                    |
| `volkLoadInstanceTable(&table, instance)` | Заполняет таблицу instance-функциями          | Table-based интерфейс                   |

### Вспомогательные функции

| Функция                   | Описание                                      | Когда использовать |
|---------------------------|-----------------------------------------------|--------------------|
| `volkGetLoadedInstance()` | Возвращает последний загруженный `VkInstance` | Отладка, проверки  |
| `volkGetLoadedDevice()`   | Возвращает последний загруженный `VkDevice`   | Отладка, проверки  |

Подробный порядок вызова: [Интеграция, раздел 4–5](integration.md#4-порядок-вызовов).

---

## Подробное описание функций

### volkInitialize

```c
VkResult volkInitialize(void);
```

**Назначение:** Загружает Vulkan loader из системы (`vulkan-1.dll` на Windows, `libvulkan.so` на Linux,
`libvulkan.dylib` на macOS).

**Параметры:** Нет.

**Возвращаемое значение:**

- `VK_SUCCESS`: Успешная загрузка Vulkan loader
- `VK_ERROR_INITIALIZATION_FAILED`: Не удалось загрузить Vulkan loader (Vulkan не установлен или повреждён)

**Когда использовать:**

- Стандартный путь инициализации volk
- Вызывать до создания `VkInstance` с помощью `vkCreateInstance`
- Если приложение не имеет других механизмов загрузки Vulkan

**Пример:**

```c
VkResult result = volkInitialize();
if (result != VK_SUCCESS) {
    fprintf(stderr, "Failed to load Vulkan loader\n");
    return false;
}
```

**См. также:** [Интеграция, шаг 1](integration.md#5-инициализация)

---

### volkInitializeCustom

```c
void volkInitializeCustom(PFN_vkGetInstanceProcAddr handler);
```

Инициализация с кастомным указателем на `vkGetInstanceProcAddr`. Loader не загружается volk — используется переданный
handler.

**Когда использовать:** Loader уже загружен (SDL, другой код). Альтернатива — `volkInitialize()`.

- **handler** — функция загрузки глобальных entrypoints

См. [Интеграция, раздел 9](integration.md#9-дополнительно).

---

### volkFinalize

```c
void volkFinalize(void);
```

Выгружает loader и сбрасывает глобальные указатели в `NULL`.

**Когда использовать:** Редко. Только при реинициализации volk в одном процессе.

---

### volkGetInstanceVersion

```c
uint32_t volkGetInstanceVersion(void);
```

Возвращает версию Vulkan loader (формат `VK_MAKE_VERSION`).

**Когда использовать:** Проверить поддержку Vulkan до создания instance. Удобно для логирования или fallback.

- **Возвращает:** версию или `0`, если `volkInitialize` не вызывался или завершился ошибкой

---

### volkLoadInstance

```c
void volkLoadInstance(VkInstance instance);
```

Загружает все глобальные указатели на instance- и device-функции. Device-функции идут через loader dispatch.

**Когда использовать:** Один device, стандартный путь. Вызывать сразу после `vkCreateInstance`.

См. [Интеграция, шаг 4](integration.md#5-инициализация).

---

### volkLoadInstanceOnly

```c
void volkLoadInstanceOnly(VkInstance instance);
```

Загружает только instance-функции. Device-функции остаются `NULL`.

**Когда использовать:** При `volkLoadDeviceTable` и нескольких device. Device загружайте через `volkLoadDevice` или
`volkLoadDeviceTable`.

См. [Интеграция, раздел 6](integration.md#6-несколько-vkdevice).

---

### volkLoadDevice

```c
void volkLoadDevice(VkDevice device);
```

Загружает device-функции напрямую из драйвера. Уменьшает dispatch overhead (до ~7%).

**Когда использовать:** Один device, нужна производительность. Сразу после `vkCreateDevice`.

См. [Интеграция, шаг 5](integration.md#5-инициализация).

---

### volkLoadInstanceTable

```c
void volkLoadInstanceTable(struct VolkInstanceTable* table, VkInstance instance);
```

Заполняет `VolkInstanceTable` указателями на instance-функции.

**Когда использовать:** Table-based интерфейс вместо глобальных символов. Редко нужно.

---

### volkLoadDeviceTable

```c
void volkLoadDeviceTable(struct VolkDeviceTable* table, VkDevice device);
```

Заполняет `VolkDeviceTable` указателями на device-функции.

**Когда использовать:** Несколько `VkDevice` — каждому устройству своя таблица.

См. [Интеграция, раздел 6](integration.md#6-несколько-vkdevice).

---

### volkGetLoadedInstance

```c
VkInstance volkGetLoadedInstance(void);
```

Последний `VkInstance`, переданный в `volkLoadInstance` или `volkLoadInstanceOnly`.

**Когда использовать:** Отладка, проверки. Редко нужно в прикладном коде.

- **Возвращает:** handle или `VK_NULL_HANDLE`

---

### volkGetLoadedDevice

```c
VkDevice volkGetLoadedDevice(void);
```

Последний `VkDevice`, переданный в `volkLoadDevice`.

**Когда использовать:** Отладка, проверки.

- **Возвращает:** handle или `VK_NULL_HANDLE`

---

## Структуры

### VolkInstanceTable

Указатели на instance-функции: `vkCreateDevice`, `vkDestroyInstance`, `vkEnumeratePhysicalDevices`,
`vkGetPhysicalDeviceProperties` и др., включая расширения. Набор полей зависит от версии Vulkan headers.

### VolkDeviceTable

Указатели на device-функции: `vkCmdDraw`, `vkQueueSubmit`, `vkCreateBuffer` и т.д. Одна таблица на каждый `VkDevice`.

---

## Макросы

| Макрос                      | Описание                                                                                                                           |
|-----------------------------|------------------------------------------------------------------------------------------------------------------------------------|
| `VOLK_HEADER_VERSION`       | Версия заголовка (343)                                                                                                             |
| `VOLK_IMPLEMENTATION`       | Определить в одном .cpp при [header-only](integration.md#8-header-only-режим)                                                      |
| `VOLK_NAMESPACE`            | Символы в namespace `volk::` (CMake-опция)                                                                                         |
| `VOLK_NO_DEVICE_PROTOTYPES` | Скрыть device-прототипы при table-based интерфейсе                                                                                 |
| `VOLK_VULKAN_H_PATH`        | Кастомный путь к vulkan.h: `#define VOLK_VULKAN_H_PATH "path/to/vulkan.h"` — volk включит его вместо `<vulkan/vulkan_core.h>`      |
| `VK_NO_PROTOTYPES`          | Определить до vulkan.h, чтобы избежать конфликтов; см. [Интеграция, раздел 2](integration.md#2-vk_no_prototypes-и-порядок-include) |
