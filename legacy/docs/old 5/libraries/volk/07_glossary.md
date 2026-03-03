# Глоссарий volk

**🟢 Уровень 1: Базовый**

Термины и определения.

---

## A

### API

Application Programming Interface. В контексте Vulkan — набор функций для работы с GPU.

---

## D

### Device

`VkDevice` — логическое устройство, представляющее GPU. Создаётся из `VkPhysicalDevice`.

### Dispatch

Процесс маршрутизации вызова функции к правильному обработчику (layer, драйвер).

### Dispatch overhead

Накладные расходы на маршрутизацию вызовов через loader. Может достигать 7% для device-intensive workloads.

---

## E

### Entrypoint

Функция Vulkan, полученная через `vkGetInstanceProcAddr` или `vkGetDeviceProcAddr`.

---

## G

### Global functions

Функции Vulkan, не требующие instance: `vkCreateInstance`, `vkEnumerateInstanceExtensionProperties`.

---

## H

### Header-only

Режим библиотеки, когда весь код находится в заголовочном файле. Для volk требуется макрос `VOLK_IMPLEMENTATION`.

---

## I

### Instance

`VkInstance` — корневой объект Vulkan. Представляет подключение приложения к Vulkan runtime.

---

## L

### Loader

Системная библиотека (`vulkan-1.dll` / `libvulkan.so`), обеспечивающая:

- Загрузку драйверов GPU
- Управление слоями (layers)
- Диспетчеризацию вызовов

---

## M

### Meta-loader

Библиотека (как volk), которая загружает loader во время выполнения и предоставляет прямые указатели на функции.

---

## P

### Physical Device

`VkPhysicalDevice` — физическое устройство GPU. Получается через `vkEnumeratePhysicalDevices`.

### Platform defines

Макросы для включения платформенных расширений:

- `VK_USE_PLATFORM_WIN32_KHR` — Windows
- `VK_USE_PLATFORM_XLIB_KHR` — Linux X11
- `VK_USE_PLATFORM_WAYLAND_KHR` — Linux Wayland
- `VK_USE_PLATFORM_MACOS_MVK` — macOS (MoltenVK)

---

## V

### Validation Layer

Слой для отладки, проверяющий корректность вызовов Vulkan API.

### VK_NO_PROTOTYPES

Макрос Vulkan, запрещающий объявления прототипов функций в `vulkan.h`.

### volk

Мета-загрузчик Vulkan для динамической загрузки функций.

### volkInitialize

Функция загрузки Vulkan loader из системы.

### volkLoadDevice

Функция загрузки device-функций напрямую из драйвера.

### volkLoadInstance

Функция загрузки instance-функций через `vkGetInstanceProcAddr`.

### VolkDeviceTable

Структура для хранения указателей device-функций. Используется при работе с несколькими `VkDevice`.

### VolkInstanceTable

Структура для хранения указателей instance-функций.

---

## Сокращения

| Сокращение | Полное название            |
|------------|----------------------------|
| GPU        | Graphics Processing Unit   |
| KHR        | Khronos Group (расширения) |
| MVK        | MoltenVK                   |
| PFN        | Pointer to Function        |
| SDK        | Software Development Kit   |

---

## Связанные термины Vulkan

| Термин         | Описание                            |
|----------------|-------------------------------------|
| Queue          | Очередь команд GPU                  |
| Command Buffer | Буфер команд для GPU                |
| Pipeline       | Конфигурация рендеринга             |
| Swapchain      | Буфер для отображения на экран      |
| Descriptor     | Описание ресурсов (buffers, images) |
| Shader         | Программа для GPU                   |
