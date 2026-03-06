# 🚧 Устранение неполадок Vulkan для ProjectV

Распространённые проблемы, ошибки и их решения при работе с Vulkan в ProjectV. Разделы организованы по уровням
сложности:

🟢 **Начинающий**: Ошибки инициализации, SDL, поверхностные проблемы
🟡 **Средний**: Синхронизация, memory, validation layers, производительность  
🔴 **Продвинутый**: Воксельный рендеринг, async compute, GPU-driven rendering, оптимизации

## Оглавление

- [🟢 Коды ошибок VK_ERROR_* — быстрая справка](#-коды-ошибок-vk_error---быстрая-справка)
- [🟢 Ошибки инициализации и SDL](#-ошибки-инициализации-и-sdl)
- [🟡 Ошибки расширений, слоёв и validation](#-ошибки-расширений-слоёв-и-validation)
- [🟡 Синхронизация и поверхностные ошибки](#-синхронизация-и-поверхностные-ошибки)
- [🟡 Ошибки памяти и производительности](#-ошибки-памяти-и-производительности)
- [🔴 Воксельный рендеринг — специфичные проблемы](#-воксельный-рендеринг--специфичные-проблемы)
- [🔴 Async Compute и GPU-driven rendering](#-async-compute-и-gpu-driven-rendering)
- [🔴 Оптимизации и продвинутые сценарии](#-оптимизации-и-продвинутые-сценарии)
- [📚 Ссылки и дополнительные ресурсы](#-ссылки-и-дополнительные-ресурсы)

---

## 🟢 Коды ошибок VK_ERROR_* — быстрая справка

| Код                              | Типичная причина                                        | Уровень | Решение                                            |
|----------------------------------|---------------------------------------------------------|---------|----------------------------------------------------|
| `VK_ERROR_INITIALIZATION_FAILED` | Loader не найден (vulkan-1.dll, libvulkan.so)           | 🟢      | Установить Vulkan SDK или обновить драйвер         |
| `VK_ERROR_INCOMPATIBLE_DRIVER`   | Запрошена версия API, которую драйвер не поддерживает   | 🟢      | Уменьшить `apiVersion` или обновить драйвер        |
| `VK_ERROR_EXTENSION_NOT_PRESENT` | Расширение не поддерживается                            | 🟡      | Проверить через `vkEnumerate*ExtensionProperties`  |
| `VK_ERROR_LAYER_NOT_PRESENT`     | Слой (например, validation) не установлен               | 🟢      | Установить Vulkan SDK или отключить слой           |
| `VK_ERROR_SURFACE_LOST_KHR`      | Surface недействителен (окно свёрнуто, режим изменился) | 🟡      | Пересоздать swapchain после восстановления         |
| `VK_SUBOPTIMAL_KHR`              | Swapchain технически работает, но не оптимален (resize) | 🟡      | Продолжить рендер; пересоздать при возможности     |
| `VK_ERROR_OUT_OF_DEVICE_MEMORY`  | Недостаточно видеопамяти                                | 🟡      | Уменьшить ресурсы или проверить утечки             |
| `VK_ERROR_OUT_OF_HOST_MEMORY`    | Недостаточно системной памяти                           | 🟡      | Проверить потребление памяти, освободить утечки    |
| `VK_ERROR_DEVICE_LOST`           | GPU перестал отвечать                                   | 🔴      | Включить validation layer, проверить синхронизацию |

Подробности по каждому — в соответствующих разделах ниже.

---

## 🟢 Ошибки инициализации и SDL

### `VK_ERROR_INITIALIZATION_FAILED` при `volkInitialize()` или `vkCreateInstance`

**Причина:** Vulkan loader не найден или не может загрузиться. На Windows — отсутствует `vulkan-1.dll`; на Linux —
`libvulkan.so.1`.

**Решение:**

1. Установите [Vulkan SDK](https://vulkan.lunarg.com/) — включает loader и validation layers
2. Обновите драйвер видеокарты — NVIDIA, AMD, Intel поставляют Vulkan loader
3. На Linux: `sudo apt install libvulkan1` (Debian/Ubuntu) или аналог

**Для ProjectV:** Проверьте, что в CMakeLists.txt подключен `volk` как подмодуль.
См. [volk — Решение проблем](../volk/troubleshooting.md#volkinitialize-возвращает-vk_error_initialization_failed).

---

### `VK_ERROR_INCOMPATIBLE_DRIVER` при `vkCreateInstance`

**Причина:** Драйвер не поддерживает запрашиваемую версию API (`apiVersion` в `VkApplicationInfo`).

**Решение:** Уменьшите `apiVersion` до `VK_API_VERSION_1_0` или `VK_MAKE_VERSION(1, 0, 0)`; либо обновите драйвер.

**Для ProjectV:** Используйте `VK_API_VERSION_1_2` как минимальную цель. Проверьте поддержку через
`vkGetPhysicalDeviceProperties(physicalDevice, &props)`.

---

### SDL_Vulkan_GetInstanceExtensions — неверный API (SDL2 vs SDL3)

**Проблема:** Двухшаговый вызов как в SDL2 — сначала с `nullptr` для подсчёта, затем копирование в свой массив — **не
подходит для SDL3**.

**SDL3 API** ([SDL_vulkan.h](../../external/SDL/include/SDL3/SDL_vulkan.h)):

```c
char const * const * SDL_Vulkan_GetInstanceExtensions(Uint32 *count);
```

Функция возвращает указатель на массив, принадлежащий SDL. **Не копировать**, не освобождать, использовать напрямую в
`ppEnabledExtensionNames`:

```cpp
Uint32 extCount = 0;
const char* const* extNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
if (!extNames) { /* SDL_GetError() */ }
createInfo.enabledExtensionCount = extCount;
createInfo.ppEnabledExtensionNames = extNames;
```

**Для ProjectV:** Эта функция вызывается **до** создания окна. SDL автоматически загружает Vulkan loader при первом
вызове.

---

## 🟡 Ошибки расширений, слоёв и validation

### `VK_ERROR_EXTENSION_NOT_PRESENT` при `vkCreateInstance` или `vkCreateDevice`

**Причина:** Запрошено расширение, которое устройство или loader не поддерживает.

**Решение:**

1. Для instance extensions: проверьте через `vkEnumerateInstanceExtensionProperties`, что имя расширения есть в списке
2. Для device extensions: `vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, ...)`
3. Убедитесь, что имена расширений написаны точно: `VK_KHR_SWAPCHAIN_EXTENSION_NAME`, `VK_KHR_SURFACE_EXTENSION_NAME` и
   т.д.
4. SDL возвращает расширения для surface — используйте именно их в `VkInstanceCreateInfo`

**Для ProjectV:** Обязательные расширения: `VK_KHR_surface`, `VK_KHR_win32_surface` (Windows), `VK_KHR_swapchain`.

---

### Слой validation не найден (`VK_ERROR_LAYER_NOT_PRESENT`)

**Причина:** Включён слой (например, `VK_LAYER_KHRONOS_validation`), но он не установлен.

**Решение:**

1. Установите Vulkan SDK — в него входят validation layers
2. Либо не включайте validation в release-сборке — используйте только в debug
3. Проверьте доступные слои через `vkEnumerateInstanceLayerProperties`

**Для ProjectV:** Рекомендуется всегда включать validation layers в debug-сборках для отлова ошибок раннего Vulkan-кода.

---

### Частые VUID (Validation Error ID)

Validation layer выводит сообщения с VUID. Типичные для ProjectV:

| VUID / Сообщение                                        | Причина                                        | Решение                                                                                                                                           |
|---------------------------------------------------------|------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------|
| `VUID-VkSubmitInfo-pWaitSemaphores-03238`               | Semaphore не в сигнальном состоянии при submit | Убедитесь, что `vkAcquireNextImageKHR` успешно вернул image; semaphore будет сигнализирован после acquire                                         |
| `VUID-vkCmdDraw-None-02699`                             | Draw при несоответствующем pipeline state      | Проверьте, что pipeline создан для того же render pass, vertex input и т.д.                                                                       |
| `VUID-VkRenderPassBeginInfo-renderPass-00904`           | Framebuffer не совместим с render pass         | Framebuffer должен быть создан для этого render pass; совпадение attachment count, format                                                         |
| `VUID-VkImageMemoryBarrier-oldLayout-01197`             | Неверный oldLayout при barrier                 | Layout должен соответствовать предыдущему использованию изображения. Для swapchain image после acquire — обычно `UNDEFINED` или `PRESENT_SRC_KHR` |
| `object of type VkImageView ... has not been destroyed` | Утечка ресурсов при выходе                     | Уничтожайте все объекты в обратном порядке создания; `vkDeviceWaitIdle` перед cleanup                                                             |

**Где искать VUID:** Сообщения validation содержат ссылки на
спецификацию. [Vulkan Validation Errors](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#errorVUID) —
полный список. Локально — в `validation_error_database.html` Vulkan SDK.

---

## 🟡 Синхронизация и поверхностные ошибки

### `VK_ERROR_SURFACE_LOST_KHR` при `vkAcquireNextImageKHR` или `vkQueuePresentKHR`

**Причина:** Surface больше недействителен — окно свёрнуто, переключение режима, отключение монитора и т.п. Рендер
невозможен до восстановления surface.

**Решение:**

1. При `vkAcquireNextImageKHR`: выйти из цикла рендеринга; ждать события (например, `SDL_EVENT_WINDOW_FOCUS_GAINED`,
   восстановление размера)
2. При `vkQueuePresentKHR`: не передавать этот кадр; обработать как surface lost
3. Пересоздать swapchain и framebuffers после восстановления окна; проверить `vkGetPhysicalDeviceSurfaceSupportKHR` или
   `SDL_Vulkan_GetPresentationSupport` перед повторным созданием

**Для ProjectV:** Обрабатывайте `SDL_EVENT_WINDOW_MINIMIZED` и `SDL_EVENT_WINDOW_RESTORED` для паузы/возобновления
рендеринга.

---

### `VK_SUBOPTIMAL_KHR` при `vkAcquireNextImageKHR` или `vkQueuePresentKHR`

**Причина:** Swapchain технически работает, но не оптимален (размер окна изменился, поворот и т.д.). Рендер можно
продолжить, но желательно пересоздать swapchain.

**Решение:**

1. Обрабатывать как `VK_SUCCESS` — дождаться конца кадра, отрисовать и показать
2. Установить флаг «нужно пересоздать swapchain» и при следующей итерации (после present, до acquire) выполнить
   swapchain recreation
3. Не паниковать — это не критичная ошибка

---

### Чёрный экран (Vulkan работает, но ничего не видно)

**Типичные причины:**

1. **Неверный image layout.** Render target должен быть в `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` во время render
   pass
2. **Неверный viewport/scissor.** Viewport `width/height` = 0 или scissor обрезает всё
3. **Очистка (clear) с цветом (0,0,0,0)** — прозрачный фон. Установите `clearValue.color` с альфа = 1
4. **Шейдер ничего не пишет.** Проверьте, что fragment shader записывает в `layout(location = 0) out`
5. **Синхронизация.** Present выполняется до завершения рендеринга — дождитесь semaphore
6. **Неправильный формат swapchain.** Убедитесь, что формат в shader и swapchain совпадают (R8G8B8A8 vs B8G8R8A8 и т.д.)

**Отладка:** Включите validation layer; проверьте VUID. Добавьте `vkQueueWaitIdle` после submit для синхронной отладки (
медленно, но проще ловить).

---

### Swapchain recreation при resize

При изменении размера окна swapchain нужно пересоздать: старые image views и framebuffers становятся невалидными.

**Порядок:**

1. `vkDeviceWaitIdle(device)` — дождаться завершения всех операций
2. Уничтожить старые framebuffers: `vkDestroyFramebuffer` для каждого
3. Уничтожить старые image views: `vkDestroyImageView` для каждого swapchain image
4. Уничтожить swapchain: `vkDestroySwapchainKHR`
5. Получить новый размер: `SDL_GetWindowSizeInPixels` или из события `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` (
   `event.window.data1`, `event.window.data2`)
6. Создать новый swapchain: `vkCreateSwapchainKHR` с новым `imageExtent`
7. Получить новые изображения: `vkGetSwapchainImagesKHR`
8. Создать новые image views и framebuffers

Render pass и pipeline можно не пересоздавать, если формат и количество attachments не изменились. При смене extent
viewport и scissor задаются динамически через `vkCmdSetViewport` / `vkCmdSetScissor`.

---

## 🟡 Ошибки памяти и производительности

### `VK_ERROR_OUT_OF_DEVICE_MEMORY`

**Причина:** Недостаточно видеопамяти на GPU.

**Решение:**

1. Уменьшите размеры текстур, буферов, количества объектов
2. Освободите неиспользуемые ресурсы
3. При VMA: `vmaGetHeapBudgets` — проверьте `usage` и `budget` по heap'ам
4. Убедитесь, что буферы/изображения уничтожаются при пересоздании (например, swapchain при resize)

**Для ProjectV:** Используйте VMA для управления памятью. Проверьте утечки через `vmaCalculateStats`.

---

### `VK_ERROR_OUT_OF_HOST_MEMORY`

**Причина:** Недостаточно системной памяти или лимит процесса исчерпан.

**Решение:** Проверьте потребление памяти; уменьшите число одновременных аллокаций; освободите утечки.

---

### `VK_ERROR_DEVICE_LOST` при submit, present или wait

**Причина:** GPU перестал отвечать — TDR (Timeout Detection and Recovery) на Windows, сбой драйвера, ошибка в
приложении (invalid use).

**Решение:**

1. Включите validation layer — часто device lost следствие нарушения VUID (invalid draw, memory corruption)
2. Проверьте синхронизацию: fence/semaphore используются правильно; нет использования ресурсов до готовности
3. Обновите драйвер
4. При TDR — уменьшите нагрузку, упростите сцену для отладки

**Для ProjectV:** Используйте Tracy для профилирования GPU, чтобы определить, какой этап рендеринга вызывает TDR.

---

## 🔴 Воксельный рендеринг — специфичные проблемы

### Проблемы производительности при рендеринге миллионов вокселей

**Проблема:** Низкий FPS при отрисовке 1M+ вокселей, высокое потребление памяти, долгая загрузка чанков.

**Решение:**

1. **GPU-driven rendering:** Используйте `vkCmdDrawIndirect` для минимизации draw calls. Генерируйте indirect команды в
   compute shaders
2. **Frustum culling на GPU:** Выполняйте отсечение невидимых чанков в compute shader перед отправкой на рендеринг
3. **LOD (Level of Detail):** Автоматически генерируйте упрощённые меши для дальних вокселей
4. **Batch rendering:** Объединяйте воксели с одинаковыми материалами в один draw call
5. **Memory optimization:** Используйте sparse textures для миров >4GB

**Для ProjectV:** См. [Интеграция воксельного рендеринга](integration.md#7-интеграция-воксельного-рендеринга)
и [Производительность Vulkan](performance.md).

---

### Ошибки compute shaders для генерации геометрии

**Проблема:** Compute shader не генерирует геометрию или создаёт артефакты.

**Решение:**

1. **Проверка work group size:** Убедитесь, что `gl_WorkGroupSize` соответствует диспатчу (`vkCmdDispatch`)
2. **Memory barriers:** Добавьте `VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT` барьеры между compute и graphics queue
3. **Buffer bounds:** Проверьте, что indirect buffer имеет достаточный размер для максимального количества команд
4. **Atomic operations:** При параллельной записи в буфер используйте `atomicAdd` для индексов

**Пример для ProjectV:**

```glsl
// В compute shader для генерации мешей
layout (std430, binding = 0) buffer MeshBuffer {
    Vertex vertices[];
    uint indices[];
    uint vertexCount;
    uint indexCount;
};

void main() {
    uint idx = atomicAdd(vertexCount, 4); // Атомарное добавление
    if (idx + 3 < vertices.length()) {
        // Генерация 4 вершин
    }
}
```

---

### Bindless rendering ошибки

**Проблема:** Текстуры не отображаются или появляются артефакты при использовании descriptor indexing.

**Решение:**

1. **Включение расширений:** Убедитесь, что `VK_EXT_descriptor_indexing` и `VK_KHR_maintenance3` включены
2. **Descriptor set limits:** Проверьте `maxDescriptorSetSamplers` и `maxPerStageDescriptorSamplers` через
   `vkGetPhysicalDeviceProperties2`
3. **Texture array bounds:** В шейдере проверяйте индекс текстуры перед доступом: `if (textureIndex < textureCount)`
4. **Sampler compatibility:** Используйте immutable samplers для consistency

**Для ProjectV:** Bindless rendering критически важен для воксельного рендеринга с тысячами текстур.

---

### Sparse memory allocation failures

**Проблема:** `vkQueueBindSparse` возвращает `VK_ERROR_OUT_OF_DEVICE_MEMORY` или `VK_ERROR_FEATURE_NOT_PRESENT`.

**Решение:**

1. **Проверка поддержки:** Вызовите `vkGetPhysicalDeviceSparseImageProperties` для проверки возможностей
2. **Page size:** Используйте правильный размер страницы (обычно 64KB или 256KB)
3. **Memory type:** Убедитесь, что memory type поддерживает `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`
4. **Gradual allocation:** Аллоцируйте sparse memory постепенно по мере необходимости

---

## 🔴 Async Compute и GPU-driven rendering

### Async compute synchronization issues

**Проблема:** Compute и graphics очереди конфликтуют, приводя к артефактам или падению производительности.

**Решение:**

1. **Timeline semaphores:** Используйте `VK_SEMAPHORE_TYPE_TIMELINE` для точной синхронизации
2. **Queue family индекс:** Убедитесь, что compute и graphics очереди принадлежат разным family (если поддерживается)
3. **Resource ownership transfer:** При передаче ресурсов между очередями используйте `VK_ACCESS_MEMORY_READ_BIT`/
   `WRITE_BIT` барьеры
4. **Pipeline barriers:** Устанавливайте барьеры между compute dispatch и graphics draw

**Пример синхронизации для ProjectV:**

```cpp
// Compute queue dispatch
VkSubmitInfo computeSubmit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &computeCmdBuffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &computeFinishedSemaphore,
};
vkQueueSubmit(computeQueue, 1, &computeSubmit, VK_NULL_HANDLE);

// Graphics queue wait for compute
VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
VkSubmitInfo graphicsSubmit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &computeFinishedSemaphore,
    .pWaitDstStageMask = &waitStage,
    .commandBufferCount = 1,
    .pCommandBuffers = &graphicsCmdBuffer,
};
vkQueueSubmit(graphicsQueue, 1, &graphicsSubmit, VK_NULL_HANDLE);
```

---

### Multi-Draw Indirect проблемы

**Проблема:** `vkCmdDrawIndirect` не отрисовывает или отрисовывает неверное количество примитивов.

**Решение:**

1. **Buffer alignment:** Убедитесь, что indirect buffer выровнен по `VkDrawIndirectCommand` (обычно 16 байт)
2. **Command count:** Проверьте, что `drawCount` в `vkCmdDrawIndirect` не превышает количество команд в буфере
3. **Buffer usage flags:** У indirect buffer должны быть флаги `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`
4. **Memory visibility:** Убедитесь, что compute shader записал данные до того, как graphics queue начнёт читать

**Для ProjectV:** Используйте compute shader для обновления indirect команд каждый кадр с frustum culling.

---

## 🔴 Оптимизации и продвинутые сценарии

### Оптимизация для воксельного рендеринга

| Проблема         | Решение                                 | Производительность    | Уровень |
|------------------|-----------------------------------------|-----------------------|---------|
| High draw calls  | GPU-driven rendering с indirect drawing | 1000x reduction       | 🔴      |
| Memory overhead  | Sparse textures + compression           | 70% reduction         | 🔴      |
| Texture binding  | Bindless descriptor arrays              | 10x faster            | 🔴      |
| LOD popping      | Геометрические мипмапы + dithering      | Smooth transitions    | 🔴      |
| Loading stutter  | Async streaming + prefetching           | No frame drops        | 🔴      |
| Shadow artifacts | Variance Shadow Maps + filtering        | Clean shadows         | 🟡      |
| Reflection noise | Screen Space Reflections + denoising    | Realistic reflections | 🔴      |

### Tracy GPU profiling проблемы

**Проблема:** Tracy не показывает GPU timing или показывает неверные значения.

**Решение:**

1. **Включение расширения:** Убедитесь, что `VK_EXT_calibrated_timestamps` включено
2. **Query pool:** Создайте `VkQueryPool` с типом `VK_QUERY_TYPE_TIMESTAMP`
3. **Timestamp valid bits:** Проверьте `timestampValidBits` через `vkGetPhysicalDeviceProperties`
4. **Синхронизация:** Используйте `vkCmdWriteTimestamp` в command buffer

**Для ProjectV:** Tracy интегрирован в примеры кода. См. [Tracy документацию](../tracy/quickstart.md).

---

## 📚 Ссылки и дополнительные ресурсы

- [volk — Решение проблем](../volk/troubleshooting.md) — VK_NO_PROTOTYPES, loader, volkInitialize
- [VMA — Решение проблем](../vma/troubleshooting.md) — память, map, flush/invalidate
- [Интеграция Vulkan](integration.md) — порядок инициализации и очистки
- [Основные понятия](concepts.md) — синхронизация, layout, swapchain
- [Производительность Vulkan](performance.md) — оптимизации для вокселей
- [Сценарии использования](use-cases.md) — практические примеры
- [ProjectV интеграция](projectv-integration.md) — специфичные для ProjectV техники

**Официальные ресурсы:**

- [Vulkan Specification](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html)
- [Validation Layers Database](https://vulkan.lunarg.com/doc/view/latest/windows/validation_error_database.html)
- [Vulkan SDK](https://vulkan.lunarg.com/)

---

*Последнее обновление: февраль 2026*  
*Уровни сложности: 🟢 Начинающий, 🟡 Средний, 🔴 Продвинутый*