# Vulkan Glossary

Словарь терминов Vulkan с определениями и контекстом использования.

---

## A

### API Version

Версия Vulkan API, указывается при создании instance. Формат: `VK_API_VERSION_1_3`. Определяет доступные функции и
расширения.

### Async Compute

Параллельное выполнение compute shaders на отдельной очереди одновременно с graphics работой. Требует синхронизации
через timeline semaphores.

### Attachment

Буфер (color, depth, stencil), привязанный к render pass. Описывается VkAttachmentDescription.

---

## B

### Barrier

Механизм синхронизации внутри command buffer. Обеспечивает видимость памяти и порядок выполнения между стадиями
pipeline. См. `vkCmdPipelineBarrier`.

### Bindless

Техника доступа к ресурсам (текстурам, буферам) по индексу вместо отдельных descriptor bindings. Требует
`VK_EXT_descriptor_indexing`.

### Buffer

Линейный массив данных в GPU памяти. Используется для вершин, индексов, uniform данных, storage.

### Buffer Device Address (BDA)

64-битный GPU-адрес буфера, получаемый через `vkGetBufferDeviceAddress`. Позволяет шейдерам напрямую обращаться к
памяти.

---

## C

### Command Buffer

Объект, содержащий записанные команды для GPU. Создаётся из command pool, записывается, отправляется в queue.

### Command Pool

Пул для выделения command buffers. Привязан к queue family.

### Compute Shader

Шейдер для общих вычислений на GPU. Не имеет доступа к графическому pipeline. Запускается через `vkCmdDispatch`.

### Concurrent Sharing

Режим sharing ресурсов между несколькими queue families. Альтернатива exclusive sharing с явной передачей владения.

### CreateInfo

Паттерн именования структур для создания объектов: `VkInstanceCreateInfo`, `VkDeviceCreateInfo` и т.д.

---

## D

### Deferred Rendering

Техника рендеринга, разделяющая геометрию и освещение. G-Buffer содержит position, normal, albedo и др.

### Descriptor

"Указатель" на ресурс (buffer, image, sampler) для шейдера. Объединяются в descriptor sets.

### Descriptor Pool

Пул для выделения descriptor sets.

### Descriptor Set

Набор дескрипторов, привязываемых к pipeline. Создаётся из descriptor pool по descriptor set layout.

### Descriptor Set Layout

Описание структуры descriptor set: bindings, типы, стадии.

### Device (VkDevice)

Логическое устройство — интерфейс к GPU. Создаётся из physical device.

### Device Memory (VkDeviceMemory)

Выделенный блок памяти на GPU. Привязывается к buffers/images.

### Dynamic State

Состояние pipeline, которое можно изменять динамически через `vkCmdSet*` команды.

---

## E

### Extension

Дополнительная функциональность Vulkan, не входящая в core. Instance extensions и device extensions.

---

## F

### Fence

Объект синхронизации CPU ↔ GPU. CPU ждёт через `vkWaitForFences`, GPU signal при завершении работы.

### Framebuffer

Набор image views, привязанных к render pass. Используется при рендеринге.

---

## G

### GPU-Driven Rendering

Архитектура, где GPU принимает решения о рендеринге: culling, LOD, indirect draw. Минимизирует CPU involvement.

### Graphics Pipeline

Объект, объединяющий все state для рендеринга: шейдеры, vertex input, rasterization, blend, etc.

---

## H

### Host

CPU и системная память приложения.

### Host Visible Memory

GPU память, доступная для чтения/записи CPU. Используется для staging, uniform buffers.

---

## I

### Image

Многомерный массив данных: 1D, 2D, 3D, cube map. Используется для текстур, render targets.

### Image Layout

Состояние image в pipeline: `UNDEFINED`, `GENERAL`, `COLOR_ATTACHMENT_OPTIMAL`, `SHADER_READ_ONLY_OPTIMAL`, etc.
Требует явных transitions.

### Image View

Представление image для определённого использования: формат, mip levels, array layers.

### Instance (VkInstance)

Корневой объект Vulkan. Представляет подключение приложения к Vulkan loader.

### Indirect Drawing

Рисование с параметрами из GPU буфера. Позволяет GPU контролировать что и сколько рисовать.

---

## L

### Layer

Промежуточный слой между приложением и драйвером. Validation layers для отладки.

### Layout Transition

Изменение image layout через pipeline barrier или render pass.

### LOD (Level of Detail)

Уровень детализации — уменьшение сложности для дальних объектов.

---

## M

### Memory Heap

Физический блок памяти GPU. Устройство имеет несколько heaps с разными свойствами.

### Memory Type

Тип памяти внутри heap: DEVICE_LOCAL, HOST_VISIBLE, HOST_COHERENT, HOST_CACHED.

### Mesh Shader

Шейдер, заменяющий vertex shader. Генерирует геометрию на GPU. Часть mesh shading pipeline.

### Multi-Draw Indirect

Один вызов `vkCmdDraw*Indirect` для множества объектов. Параметры из GPU буфера.

---

## N

### Non-Uniform Indexing

Индексация массива дескрипторов значением, не известным во время компиляции. Требует `nonuniformEXT` в GLSL.

---

## O

### Out of Date

Состояние swapchain, требующее пересоздания: resize, DPI change.

---

## P

### Physical Device (VkPhysicalDevice)

Физический GPU. Перечисляется через `vkEnumeratePhysicalDevices`. Не создаётся, только выбирается.

### Pipeline

Объект, содержащий все state для выполнения работы: graphics pipeline, compute pipeline.

### Pipeline Barrier

Команда синхронизации внутри command buffer. Обеспечивает порядок и видимость памяти.

### Pipeline Cache

Кэш скомпилированных pipelines. Можно сохранять на диск для ускорения последующих запусков.

### Pipeline Layout

Описание ресурсов pipeline: descriptor set layouts, push constants.

### Present

Отображение изображения на экран. Выполняется через `vkQueuePresentKHR`.

### Push Constants

Маленький блок данных (до 128 байт), передаваемый напрямую в command buffer. Быстрее descriptor sets.

---

## Q

### Queue

Очередь команд GPU. Graphics, compute, transfer, present очереди.

### Queue Family

Группа очередей с одинаковыми возможностями. Graphics, compute, transfer, sparse binding.

---

## R

### Render Pass

Описание прохода рендеринга: attachments, subpasses, dependencies.

### Render Target

Изображение, в которое происходит рендеринг. Color attachment, depth attachment.

---

## S

### Sampler

Объект, описывающий параметры выборки текстуры: filter, address mode, mip mapping, anisotropy.

### Semaphore

Объект синхронизации GPU ↔ GPU. Используется между queue submissions и present.

### Shader Module

Скомпилированный SPIR-V код шейдера. Используется при создании pipeline.

### SPIR-V

Бинарный формат шейдеров Vulkan. Компилируется из GLSL, HLSL или других языков.

### Staging Buffer

Промежуточный буфер для передачи данных Host → Device. Обычно в HOST_VISIBLE памяти.

### Subpass

Часть render pass. Позволяет эффективно использовать input attachments.

### Subgroup

Группа потоков, выполняющихся синхронно на одном SIMD-юните. NVIDIA warp = 32, AMD wavefront = 64.

### Surface (VkSurfaceKHR)

Абстракция вывода — окно или область экрана. Создаётся через SDL или platform-specific API.

### Swapchain (VkSwapchainKHR)

Цепочка изображений для вывода на экран. Double/triple buffering.

### Synchronization2

Расширение с улучшенным API синхронизации: `vkCmdPipelineBarrier2`, `vkQueueSubmit2`.

---

## T

### Task Shader

Шейдер перед mesh shader. Выполняет culling, LOD selection, спавнит mesh shader workgroups.

### Timeline Semaphore

Semaphore со значением (counter). Позволяет более тонкую синхронизацию, wait/signal на CPU.

### Transfer

Операции копирования данных: buffer-to-buffer, buffer-to-image, image-to-image.

---

## U

### Uniform Buffer

Буфер с константами для шейдера. Обычно small, read-only в шейдере.

### Update After Bind

Флаг дескриптора, позволяющий обновление после привязки к pipeline.

---

## V

### Validation Layers

Слои для проверки корректности API вызовов. Обязательны при разработке.

### volk

Заголовочный загрузчик Vulkan функций. Упрощает работу с extensions.

### VMA (Vulkan Memory Allocator)

Библиотека для управления памятью GPU. Упрощает аллокацию и дефрагментацию.

### VkResult

Тип возвращаемого значения для большинства Vulkan функций. Коды успеха и ошибок.

---

## W

### Workgroup

Группа invocations в compute shader. Определяется `layout(local_size_x, local_size_y, local_size_z)`.

---

## Z

### Zero-Descriptor Rendering

Техника рендеринга без descriptor sets. Все данные через push constants и buffer device address.

---

## Краткий справочник по типам

| Тип        | Описание               | Пример                                 |
|------------|------------------------|----------------------------------------|
| Handle     | Непрозрачный указатель | `VkInstance`, `VkDevice`, `VkBuffer`   |
| CreateInfo | Структура создания     | `VkInstanceCreateInfo`                 |
| Flags      | Битовые флаги          | `VkQueueFlags`, `VkPipelineStageFlags` |
| Struct     | Структура данных       | `VkExtent2D`, `VkViewport`             |
| Enum       | Перечисление           | `VkFormat`, `VkPresentModeKHR`         |
