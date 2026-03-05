# Глоссарий Vulkan 1.4

Словарь терминов Vulkan с фокусом на ProjectV и воксельном рендеринге. Подробные примеры —
в [Основных понятиях](concepts.md), [Быстром старте](quickstart.md) и [ProjectV Integration](projectv-integration.md).

**Уровни сложности:** 🟢 База Vulkan • 🟡 Оптимизации для вокселей • 🔴 Продвинутые фичи Vulkan 1.4

## Оглавление

- [Vulkan 1.4 и ProjectV](#vulkan-14-и-projectv)
- [Уровни сложности терминов](#уровни-сложности-терминов)
- [volk](#volk)
- [SDL3 Vulkan](#sdl3-vulkan)
- [Структуры Vulkan (sType, pNext)](#структуры-vulkan-stype-pnext)
- [Инициализация](#инициализация)
- [Surface и Swapchain](#surface-и-swapchain)
- [Командные буферы](#командные-буферы)
- [Синхронизация](#синхронизация)
- [Рендеринг](#рендеринг)
- [Ресурсы](#ресурсы)
- [Расширения и слои](#расширения-и-слои)
- [Compute и GPU-Driven Rendering](#compute-и-gpu-driven-rendering)
- [Воксельный рендеринг](#воксельный-рендеринг)
- [См. также](#см-также)

---

## Vulkan 1.4 и ProjectV

| Термин                   | Уровень | Объяснение                                                                   | ProjectV использование                                   |
|--------------------------|---------|------------------------------------------------------------------------------|----------------------------------------------------------|
| **Vulkan 1.4**           | 🟢      | Версия API с timeline semaphores, descriptor indexing, buffer device address | Базовая версия для ProjectV                              |
| **GPU Driven Rendering** | 🟡      | Архитектура, где GPU сам определяет что рисовать через compute shaders       | Multi-Draw Indirect для вокселей                         |
| **Async Compute**        | 🟡      | Параллельное выполнение compute и graphics очередей                          | Генерация воксельной геометрии параллельно с рендерингом |
| **Bindless Rendering**   | 🟡      | Доступ к текстурам и буферам через индексы, а не фиксированные binding       | Тысячи текстур вокселей без пересоздания descriptor sets |
| **Mesh Shading**         | 🔴      | Пайплайн с task и mesh шейдерами вместо vertex shader                        | Генерация сложной геометрии вокселей на GPU              |
| **Ray Tracing**          | 🔴      | Трассировка лучей для глобального освещения и теней                          | SVO (Sparse Voxel Octree) трассировка                    |
| **SVO**                  | 🔴      | Sparse Voxel Octree — разреженное октодерево вокселей                        | Эффективное хранение и трассировка воксельного мира      |
| **VMA**                  | 🟢      | Vulkan Memory Allocator — библиотека для управления памятью GPU              | Выделение буферов для воксельных данных                  |
| **Tracy GPU Profiling**  | 🟡      | Инструмент профилирования GPU команд                                         | Оптимизация воксельного рендерера                        |

---

## Уровни сложности терминов

### 🟢 Уровень 1: База Vulkan

- Основные объекты (Instance, Device, Queue, Swapchain)
- Простой рендеринг треугольника
- Базовая синхронизация
- Управление памятью через VMA

### 🟡 Уровень 2: Оптимизации для вокселей

- GPU Driven Rendering с Multi-Draw Indirect
- Async Compute с Timeline Semaphores
- Bindless Rendering с Descriptor Indexing
- Compute Shader Culling и LOD
- Memory Aliasing и Sparse Memory

### 🔴 Уровень 3: Продвинутые фичи Vulkan 1.4

- Mesh Shading для генерации геометрии
- Ray Tracing для SVO и ambient occlusion
- Device Groups (multi-GPU)
- Synchronization2
- Pipeline Library

---

## volk

| Термин                                  | Уровень | Объяснение                                                                                                                                     | ProjectV специфика                      |
|-----------------------------------------|---------|------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------|
| **VOLK_HEADER_VERSION**                 | 🟢      | Версия volk (343 в [volk.h](../../external/volk/volk.h)). Макрос для проверки при компиляции.                                                  | Используется для проверки совместимости |
| **volkLoadInstanceOnly**                | 🟢      | Загрузка только instance-функций, без device. Требует затем `volkLoadDevice` или `volkLoadDeviceTable`. Для multi-device — использовать table. | Полезно при раздельной инициализации    |
| **volkGetLoadedInstance**               | 🟢      | Возвращает последний `VkInstance`, переданный в `volkLoadInstance`, или `VK_NULL_HANDLE`.                                                      | Отладка и проверка состояния            |
| **volkGetLoadedDevice**                 | 🟢      | Возвращает последний `VkDevice`, переданный в `volkLoadDevice`, или `VK_NULL_HANDLE`.                                                          | Отладка                                 |
| **VolkInstanceTable / VolkDeviceTable** | 🟡      | Таблицы указателей на функции. Для нескольких `VkDevice` — `volkLoadDeviceTable(&table, device)` и вызовы через `table.vkCmdDraw(...)`.        | Multi-GPU архитектура                   |

---

## SDL3 Vulkan

| Термин                                  | Уровень | Объяснение                                                                                                                                                       | ProjectV использование                              |
|-----------------------------------------|---------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------|
| **SDL_Vulkan_GetInstanceExtensions**    | 🟢      | Возвращает `const char* const*` — массив имён расширений. Владелец — SDL, не копировать. В SDL3 **не требует** window (в отличие от SDL2).                       | Автоматическое получение расширений для поверхности |
| **SDL_Vulkan_LoadLibrary**              | 🟢      | Явная загрузка Vulkan loader. `path == nullptr` — системный loader. Вызывать после `SDL_Init`, до создания Vulkan-окна.                                          | Контроль над временем загрузки Vulkan               |
| **SDL_Vulkan_UnloadLibrary**            | 🟢      | Выгрузка loader. Вызывать после уничтожения всех Vulkan-ресурсов и окон.                                                                                         | Очистка ресурсов при выходе                         |
| **SDL_Vulkan_DestroySurface**           | 🟢      | Уничтожает surface. Сигнатура: `(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* allocator)`. Вызывать до `SDL_DestroyWindow`.           | Правильный порядок уничтожения                      |
| **SDL_Vulkan_GetPresentationSupport**   | 🟢      | Проверяет поддержку present для данной queue family. Параметры: `(instance, physicalDevice, queueFamilyIndex)`. Возвращает `true` если можно present на surface. | Выбор очереди для present                           |
| **SDL_Vulkan_GetVkGetInstanceProcAddr** | 🟢      | Возвращает указатель на `vkGetInstanceProcAddr`. Передать в `volkInitializeCustom` если SDL уже загрузил loader.                                                 | Интеграция volk + SDL3                              |

---

## Структуры Vulkan (sType, pNext)

| Термин     | Уровень | Объяснение                                                                                                                                                                                                                                          | Важно для             |
|------------|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------|
| **sType**  | 🟢      | Поле в каждой структуре Vulkan — идентификатор типа (`VK_STRUCTURE_TYPE_*`). Обязательно для всех структур, передаваемых в API. Пример: `info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO`.                                                      | Все структуры         |
| **pNext**  | 🟢      | Указатель на цепочку расширений. `nullptr` — базовая структура. Для дополнительных параметров (debug, feature) передают указатель на следующую структуру с собственным `sType`.                                                                     | Расширения Vulkan 1.4 |
| **Handle** | 🟢      | Непрозрачный указатель или число, идентифицирующее объект Vulkan (`VkInstance`, `VkDevice`, `VkSwapchainKHR` и т.д.). В C++ ведёт себя как указатель, но **не разыменовывается** и **не освобождается** через `delete` — только через `vkDestroy*`. | Все объекты Vulkan    |

---

## Инициализация

| Термин                | Уровень | Объяснение                                                                                                                                                                                     | ProjectV пример                            |
|-----------------------|---------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------|
| **VkInstance**        | 🟢      | Объект, представляющий подключение приложения к Vulkan. Создаётся первым через `vkCreateInstance`. Хранит информацию о слоях и расширениях. Нужен для создания устройств и поверхностей.       | `vkCreateInstance` с Vulkan 1.4            |
| **VkApplicationInfo** | 🟢      | Структура с информацией о приложении: `pApplicationName`, `applicationVersion`, `pEngineName`, `engineVersion`, `apiVersion`. Передаётся в `VkInstanceCreateInfo`.                             | Указание `apiVersion = VK_API_VERSION_1_4` |
| **vkCreateInstance**  | 🟢      | Создаёт `VkInstance`. Принимает `VkInstanceCreateInfo` с расширениями и слоями. Вызывать после `volkInitialize`.                                                                               | Создание instance с validation layers      |
| **VkPhysicalDevice**  | 🟢      | «Физическое» GPU — видеокарта или интегрированный чип. Перечисляется через `vkEnumeratePhysicalDevices`. Через него узнают возможности железа: память, очереди, форматы. Не создаётся вручную. | Выбор GPU с поддержкой Vulkan 1.4          |
| **VkDevice**          | 🟢      | Логическое устройство. Создаётся из `VkPhysicalDevice` через `vkCreateDevice`. Через него создают буферы, изображения, пайплайны, очереди. Большинство вызовов Vulkan работают с device.       | Создание device с async compute очередями  |
| **vkCreateDevice**    | 🟢      | Создаёт `VkDevice` из `VkPhysicalDevice`. Принимает `VkDeviceCreateInfo` с очередями и расширениями device.                                                                                    | Включение `VK_KHR_timeline_semaphore`      |
| **VkQueue**           | 🟢      | Очередь команд GPU. Получается через `vkGetDeviceQueue` после создания device. В неё отправляют command buffers для выполнения (graphics, compute, transfer, present).                         | Graphics, compute, present очереди         |
| **Queue family**      | 🟢      | Группа очередей с одинаковыми возможностями. `VkQueueFamilyProperties` описывает возможности (graphics, compute, transfer, present). У физического устройства несколько queue families.        | Поиск отдельных семейств для async compute |

---

## Surface и Swapchain

| Термин                   | Уровень | Объяснение                                                                                                                                                                            | Особенности                                            |
|--------------------------|---------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------|
| **VkSurfaceKHR**         | 🟢      | Поверхность для вывода — окно или область экрана. Связывает Vulkan с оконной системой (SDL, Win32, X11 и т.д.). Создаётся через `SDL_Vulkan_CreateSurface` или `vkCreate*SurfaceKHR`. | В ProjectV — через SDL3                                |
| **VK_KHR_surface**       | 🟢      | Расширение instance. Необходимо для создания surfaces. Требуется поверх `VK_KHR_win32_surface` (Windows) или `VK_KHR_xlib_surface` (Linux) и т.д.                                     | Автоматически включается SDL3                          |
| **VK_KHR_win32_surface** | 🟢      | Расширение для создания surface на Windows. Включить в `VkInstanceCreateInfo::ppEnabledExtensionNames`.                                                                               | Windows-specific                                       |
| **VkSwapchainKHR**       | 🟢      | Цепочка изображений для вывода на экран. Создаётся через `vkCreateSwapchainKHR`. Содержит несколько изображений (обычно 2–3); приложение рисует в одно, пока другое показывается.     | Double/triple buffering                                |
| **VkPresentModeKHR**     | 🟢      | Режим показа кадров: `VK_PRESENT_MODE_FIFO` (vsync), `VK_PRESENT_MODE_IMMEDIATE` (без vsync), `VK_PRESENT_MODE_MAILBOX` (triple buffering) и др.                                      | `VK_PRESENT_MODE_MAILBOX_KHR` для минимальной задержки |
| **Swapchain image**      | 🟢      | Одно из изображений в swapchain. Получается через `vkGetSwapchainImagesKHR`. Каждое изображение — render target для одного кадра.                                                     | Image views для framebuffer                            |
| **VK_KHR_swapchain**     | 🟢      | Расширение device. Требуется для создания swapchain и present.                                                                                                                        | Обязательно для вывода на экран                        |

---

## Командные буферы

| Термин                                 | Уровень | Объяснение                                                                                                                                                                                      | Использование                                                                |
|----------------------------------------|---------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------|
| **VkCommandPool**                      | 🟢      | Пул для выделения command buffers. Создаётся через `vkCreateCommandPool`. Команды из одного пула обычно отправляются в одну queue family.                                                       | `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT` для частой перезаписи                 |
| **VkCommandBuffer**                    | 🟢      | Буфер с записанными командами GPU. Выделяется через `vkAllocateCommandBuffers`. Запись: `vkBeginCommandBuffer` → `vkCmd*` → `vkEndCommandBuffer`. Отправляется в очередь через `vkQueueSubmit`. | Primary для graphics, secondary для reuse                                    |
| **vkAllocateCommandBuffers**           | 🟢      | Выделяет один или несколько command buffers из пула.                                                                                                                                            | Множественное выделение для параллельной записи                              |
| **vkBeginCommandBuffer**               | 🟢      | Начинает запись команд. Буфер переходит в состояние recording.                                                                                                                                  | `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT` для однократного использования |
| **vkEndCommandBuffer**                 | 🟢      | Завершает запись. Буфер готов к отправке.                                                                                                                                                       | Проверка `VK_SUCCESS`                                                        |
| **vkCmdDraw**                          | 🟢      | Команда отрисовки примитивов (без индексов). Указывает `vertexCount`, `instanceCount`.                                                                                                          | Простой рендеринг                                                            |
| **vkCmdDrawIndexed**                   | 🟢      | Команда отрисовки с индексным буфером. Указывает `indexCount`, `instanceCount`, `firstIndex`, `vertexOffset`.                                                                                   | Меши с индексами                                                             |
| **Primary / Secondary command buffer** | 🟡      | Primary — может быть отправлен в очередь и вызывать secondary. Secondary — выполняется только из primary через `vkCmdExecuteCommands`.                                                          | Оптимизация повторяющихся команд                                             |
| **vkCmdDrawIndirect**                  | 🟡      | Indirect drawing — параметры рисования читаются из буфера.                                                                                                                                      | GPU Driven Rendering                                                         |
| **vkCmdDrawIndexedIndirect**           | 🟡      | Indirect indexed drawing — индексированное рисование с параметрами из буфера.                                                                                                                   | GPU Driven Rendering с индексами                                             |
| **vkCmdDispatch**                      | 🟡      | Выполнение compute shader. Указывает размер work groups.                                                                                                                                        | Compute шейдеры для вокселей                                                 |
| **vkCmdDispatchIndirect**              | 🟡      | Indirect dispatch — параметры dispatch читаются из буфера.                                                                                                                                      | GPU-driven compute                                                           |

---

## Синхронизация

| Термин                    | Уровень | Объяснение                                                                                                                                                                                                         | ProjectV применение                            |
|---------------------------|---------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------|
| **VkFence**               | 🟢      | Синхронизация host (CPU) и device (GPU). GPU сигнализирует fence при завершении работы. CPU ждёт через `vkWaitForFences`. Типично: один fence на кадр, ждём перед повторным использованием ресурсов кадра.         | Ожидание завершения кадра                      |
| **VkSemaphore**           | 🟢      | Синхронизация между очередью и операциями (acquire image, submit, present). Сигнализирует «операция завершена». Используется в `vkQueueSubmit` и `vkQueuePresentKHR` для ordering.                                 | Синхронизация acquire → render → present       |
| **VkPipelineStageFlags**  | 🟢      | Стадия пайплайна, на которой срабатывает барьер или semaphore. Например, `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`, `VK_PIPELINE_STAGE_TRANSFER_BIT`.                                                        | Указание стадий ожидания                       |
| **vkWaitForFences**       | 🟢      | Блокирует CPU до сигнализации fence. Перед повторным использованием command buffer или изображения кадра.                                                                                                          | Ожидание перед началом нового кадра            |
| **VkPipelineBarrier**     | 🟡      | Синхронизация внутри command buffer. Обеспечивает порядок выполнения и видимость памяти. Используется для переходов layout изображений (`VK_IMAGE_LAYOUT_UNDEFINED` → `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`). | Барьеры между compute и graphics               |
| **VkImageLayout**         | 🟢      | Состояние изображения в пайплайне. Должно соответствовать использованию: `COLOR_ATTACHMENT_OPTIMAL` для render target, `SHADER_READ_ONLY_OPTIMAL` для текстуры, `TRANSFER_DST_OPTIMAL` для копирования.            | Transition layouts                             |
| **VkSubpassDependency**   | 🟡      | Зависимость между subpass в render pass или между внешним миром и render pass. Определяет pipeline barriers автоматически.                                                                                         | Оптимизация render pass                        |
| **Timeline Semaphore**    | 🟡      | Расширение `VK_KHR_timeline_semaphore`. Semaphore со значением (counter). Позволяет более тонкую синхронизацию между очередями.                                                                                    | Async compute в ProjectV                       |
| **vkWaitSemaphores**      | 🟡      | Ожидание timeline semaphore на host. Параметры: `VkSemaphoreWaitInfo` с значениями.                                                                                                                                | Ожидание завершения compute шейдера            |
| **vkSignalSemaphore**     | 🟡      | Сигнализация timeline semaphore с host.                                                                                                                                                                            | Ручное управление синхронизацией               |
| **VkMemoryBarrier**       | 🟡      | Барьер для обеспечения видимости памяти между стадиями пайплайна.                                                                                                                                                  | Синхронизация доступа к буферам                |
| **VkBufferMemoryBarrier** | 🟡      | Барьер для конкретного буфера. Используется при передаче владения между queue families.                                                                                                                            | Передача indirect buffer от compute к graphics |
| **VkImageMemoryBarrier**  | 🟡      | Барьер для конкретного изображения. Используется для transition layout и передачи владения.                                                                                                                        | Transition swapchain image layout              |

---

## Рендеринг

| Термин                      | Уровень | Объяснение                                                                                                                                                                            | ProjectV использование                                |
|-----------------------------|---------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------|
| **VkRenderPass**            | 🟢      | Описание прохода рендеринга: attachments (color, depth), subpasses, load/store operations. Создаётся через `vkCreateRenderPass`. Задаёт формат и образ использования framebuffer.     | Render pass для воксельного рендеринга                |
| **VkFramebuffer**           | 🟢      | Набор image views (обычно swapchain image views), привязанных к render pass. Создаётся через `vkCreateFramebuffer`. Один framebuffer — один «кадр» render pass.                       | Framebuffer для каждого swapchain image               |
| **VkPipeline**              | 🟢      | Конфигурация GPU для отрисовки или compute: шейдеры, vertex input, rasterization, blend и т.д. Graphics pipeline — `vkCreateGraphicsPipelines`, compute — `vkCreateComputePipelines`. | Graphics pipeline для вокселей, compute для генерации |
| **VkPipelineLayout**        | 🟢      | Описание layout ресурсов (descriptor sets, push constants), используемых пайплайном. Создаётся через `vkCreatePipelineLayout`.                                                        | Layout с push constants для трансформаций             |
| **VkShaderModule**          | 🟢      | Скомпилированный шейдер (SPIR-V). Создаётся через `vkCreateShaderModule` из байтов SPIR-V. Указывается в `VkPipelineShaderStageCreateInfo`.                                           | Vertex/fragment shaders, compute shaders              |
| **VkAttachmentDescription** | 🟢      | Описание одного attachment в render pass: format, samples, loadOp, storeOp, initialLayout, finalLayout.                                                                               | Color и depth attachments                             |
| **VkSubpassDescription**    | 🟢      | Подпроход внутри render pass. Определяет, какие attachments используются как color, depth, input.                                                                                     | Single subpass для простого рендеринга                |
| **vkCmdBeginRenderPass**    | 🟢      | Начинает render pass в command buffer. Указывает framebuffer и render pass.                                                                                                           | Начало рендеринга кадра                               |
| **vkCmdEndRenderPass**      | 🟢      | Завершает render pass.                                                                                                                                                                | Конец рендеринга кадра                                |
| **vkCmdBindPipeline**       | 🟢      | Привязывает pipeline к command buffer. Все последующие draw — с этим пайплайном.                                                                                                      | Переключение между graphics/compute                   |
| **Render Pass 2**           | 🟡      | `vkCreateRenderPass2` — улучшенная версия с более гибким контролем над attachments и dependencies.                                                                                    | Использовать для сложных рендер пассов                |

---

## Ресурсы

| Термин                    | Уровень | Объяснение                                                                                                                                                                    | ProjectV альтернатива                                   |
|---------------------------|---------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------|
| **VkBuffer**              | 🟢      | Линейный буфер данных. Вершины, индексы, uniform, staging. Создаётся через `vkCreateBuffer`. Требует выделения памяти (`vkAllocateMemory`) и привязки (`vkBindBufferMemory`). | **VMA** — `vmaCreateBuffer`                             |
| **VkImage**               | 🟢      | Двумерное/трёхмерное изображение. Текстуры, render targets, depth buffer. Создаётся через `vkCreateImage`. Аналогично буферу — память и bind.                                 | **VMA** — `vmaCreateImage`                              |
| **VkImageView**           | 🟢      | Представление изображения для шейдеров или framebuffer. Создаётся через `vkCreateImageView`. Указывает format, mip levels, array layers.                                      | Создание view для текстур и render targets              |
| **VkDeviceMemory**        | 🟢      | Выделенный блок памяти на device. Используется с `vkBindBufferMemory` / `vkBindImageMemory`.                                                                                  | **VMA** — автоматическое управление                     |
| **VkDescriptorSet**       | 🟢      | Набор дескрипторов (uniform buffer, image+sampler и т.д.) для шейдеров. Привязывается через `vkCmdBindDescriptorSets`.                                                        | Bindless rendering для многих текстур                   |
| **VkDescriptorSetLayout** | 🟢      | Layout набора: какие binding'и, типы, stages. Создаётся через `vkCreateDescriptorSetLayout`.                                                                                  | Bindless layout с `VK_DESCRIPTOR_BINDING_*` флагами     |
| **VkDescriptorPool**      | 🟢      | Пул для выделения descriptor sets. Создаётся через `vkCreateDescriptorPool`.                                                                                                  | Пул с `VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT` |
| **VkSampler**             | 🟢      | Параметры выборки текстур (filter, address mode). Создаётся через `vkCreateSampler`. Передаётся в descriptor set.                                                             | Sampler для текстур вокселей                            |
| **VK_FORMAT_***           | 🟢      | Формат пикселей: `VK_FORMAT_R8G8B8A8_UNORM`, `VK_FORMAT_D32_SFLOAT` (depth) и т.д.                                                                                            | Выбор формата для текстур и render targets              |
| **Sparse Memory**         | 🔴      | Разреженная память — выделение только используемых регионов больших изображений.                                                                                              | Огромные воксельные текстуры                            |
| **Memory Aliasing**       | 🟡      | Разные ресурсы используют одну и ту же память в разное время.                                                                                                                 | Оптимизация использования памяти                        |

---

## Расширения и слои

| Термин                           | Уровень | Объяснение                                                                                                                                                                                                                                                           | ProjectV использование                                    |
|----------------------------------|---------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------|
| **Extension**                    | 🟢      | Дополнительные возможности Vulkan, не входящие в базовую спецификацию. Подключаются по имени (например, `VK_KHR_swapchain`). Instance extensions — для instance, device extensions — для device. volk автоматически загружает entrypoints для включённых расширений. | `VK_KHR_timeline_semaphore`, `VK_EXT_descriptor_indexing` |
| **Layer**                        | 🟢      | Промежуточный слой между приложением и драйвером. Validation layer проверяет вызовы на ошибки. Debug layer добавляет логирование. Включаются в `VkInstanceCreateInfo::ppEnabledLayerNames`.                                                                          | `VK_LAYER_KHRONOS_validation` в debug                     |
| **VK_EXT_debug_utils**           | 🟢      | Расширение для отладки: `vkCreateDebugUtilsMessengerEXT`, сообщения validation layer.                                                                                                                                                                                | Отладка и логирование                                     |
| **VK_LAYER_KHRONOS_validation**  | 🟢      | Стандартный validation layer. Проверяет параметры вызовов, выводит ошибки и предупреждения. Включать в debug-сборках.                                                                                                                                                | Обязательно для разработки                                |
| **VK_KHR_timeline_semaphore**    | 🟡      | Расширение для timeline semaphores. Позволяет более тонкую синхронизацию между очередями.                                                                                                                                                                            | Async compute в ProjectV                                  |
| **VK_EXT_descriptor_indexing**   | 🟡      | Расширение для bindless rendering. Позволяет частично bound descriptor sets и update after bind.                                                                                                                                                                     | Тысячи текстур вокселей                                   |
| **VK_KHR_buffer_device_address** | 🟡      | Расширение для получения GPU адресов буферов. Позволяет шейдерам напрямую обращаться к буферам по адресу.                                                                                                                                                            | GPU-driven индексация данных                              |
| **VK_EXT_mesh_shader**           | 🔴      | Расширение для mesh shading. Заменяет vertex shader на task + mesh шейдеры.                                                                                                                                                                                          | Генерация геометрии вокселей                              |
| **VK_KHR_ray_tracing_pipeline**  | 🔴      | Расширение для ray tracing. Позволяет создавать ray tracing пайплайны.                                                                                                                                                                                               | SVO трассировка лучей                                     |
| **VK_EXT_memory_budget**         | 🟡      | Расширение для запроса бюджета памяти устройства.                                                                                                                                                                                                                    | Мониторинг использования памяти VMA                       |

---

## Compute и GPU-Driven Rendering

| Термин                    | Уровень | Объяснение                                                                                                 | ProjectV применение                          |
|---------------------------|---------|------------------------------------------------------------------------------------------------------------|----------------------------------------------|
| **Compute Shader**        | 🟡      | Шейдер для общих вычислений на GPU. Не имеет доступа к графическому пайплайну.                             | Генерация воксельной геометрии, culling, LOD |
| **Work Group**            | 🟡      | Группа инvocations в compute shader. Определяется в шейдере (`layout(local_size_x = ...)`).                | Оптимизация под конкретный GPU               |
| **Dispatch**              | 🟡      | Вызов compute shader с указанием количества work groups.                                                   | `vkCmdDispatch` или `vkCmdDispatchIndirect`  |
| **Indirect Drawing**      | 🟡      | Рисование с параметрами из GPU буфера (`VkDrawIndirectCommand`). Позволяет GPU самому решать что рисовать. | Multi-Draw Indirect для вокселей             |
| **Frustum Culling**       | 🟡      | Отсечение объектов вне frustum камеры. Выполняется compute shader'ом на GPU.                               | Culling миллионов вокселей                   |
| **LOD (Level of Detail)** | 🟡      | Уровень детализации — уменьшение детализации для дальних объектов.                                         | Compute shader выбирает LOD для вокселей     |
| **Occlusion Culling**     | 🟡      | Отсечение объектов, закрытых другими объектами.                                                            | Сложная оптимизация для плотных сцен         |
| **GPU-Driven Pipeline**   | 🟡      | Архитектура, где compute шейдеры готовят данные для graphics пайплайна.                                    | Стандартная архитектура ProjectV             |
| **Wavefront**             | 🔴      | Группа инvocations, выполняющихся вместе на GPU (обычно 32 или 64).                                        | Оптимизация compute shader                   |

---

## Воксельный рендеринг

| Термин                        | Уровень | Объяснение                                                                         | Реализация в ProjectV                 |
|-------------------------------|---------|------------------------------------------------------------------------------------|---------------------------------------|
| **Воксель**                   | 🟢      | Трёхмерный пиксель — элементарный куб в воксельном мире.                           | Хранится в сжатом формате в буфере    |
| **Чанк**                      | 🟢      | Блок вокселей (обычно 16×16×16 или 32×32×32). Базовая единица LOD и culling.       | Отдельный объект рендеринга           |
| **SVO (Sparse Voxel Octree)** | 🔴      | Разреженное октодерево вокселей. Эффективная структура для хранения и трассировки. | Ray tracing acceleration structure    |
| **Mesh Extraction**           | 🟡      | Генерация полигональной меши из вокселей (Marching Cubes, Surface Nets).           | Compute shader генерирует вершины     |
| **Texture Atlas**             | 🟡      | Большая текстура, содержащая множество маленьких текстур.                          | Хранение текстур вокселей             |
| **Mipmapping**                | 🟢      | Иерархия текстур с уменьшенными версиями для дальних объектов.                     | Автоматическая генерация mip levels   |
| **Anisotropic Filtering**     | 🟡      | Улучшенная фильтрация текстур под углом.                                           | Включать для качественного рендеринга |
| **Global Illumination**       | 🔴      | Освещение, учитывающее отражение света между поверхностями.                        | Ray tracing через SVO                 |
| **Ambient Occlusion**         | 🟡      | Затенение углов и щелей.                                                           | Screen-space или ray traced           |
| **Voxel Cone Tracing**        | 🔴      | Упрощённая трассировка для глобального освещения.                                  | Альтернатива full ray tracing         |

---

## См. также

- [Основные понятия](concepts.md) 🟢 — host/device, execution model, жизненный цикл, swapchain, render pass,
  синхронизация.
- [ProjectV Integration](projectv-integration.md) 🟡🔴 — архитектура воксельного рендерера, GPU-driven rendering, async
  compute.
- [Performance](performance.md) 🟡 — оптимизации GPU-driven rendering, memory management, compute shader tuning.
- [Decision Trees](decision-trees.md) 🟢🟡 — выбор API, архитектуры рендеринга, синхронизации.
- [Use Cases](use-cases.md) 🟢🟡🔴 — примеры compute shaders для вокселей, post-processing, ray tracing.
- [Troubleshooting](troubleshooting.md) 🟢🟡🔴 — решение проблем синхронизации, memory, validation errors.
- [API Reference](api-reference.md) 🟢🟡🔴 — полные описания функций и структур Vulkan 1.4.
- [volk — Глоссарий](../volk/glossary.md) 🟢 — loader, entrypoint (пересечение; volk-специфичные термины — выше).
- [VMA Documentation](../vma/api-reference.md) 🟢 — управление памятью GPU для воксельных буферов.