# Vulkan Concepts

**🟢 Уровень 1: Базовый** — Основные понятия и архитектура Vulkan.

---

## Содержание

1. [Host и Device](#host-и-device)
2. [Execution Model](#execution-model)
3. [Очереди и Queue Families](#очереди-и-queue-families)
4. [Командные буферы](#командные-буферы)
5. [Синхронизация](#синхронизация)
6. [Ресурсы и память](#ресурсы-и-память)
7. [Render Pass](#render-pass)
8. [Pipeline](#pipeline)
9. [Descriptor Sets](#descriptor-sets)

---

## Host и Device

### Определения

| Термин                  | Описание                                      |
|-------------------------|-----------------------------------------------|
| **Host**                | CPU и системная память приложения             |
| **Device**              | GPU и видеопамять                             |
| **Host-visible memory** | Память GPU, доступная для чтения/записи CPU   |
| **Device-local memory** | Память GPU, оптимизированная для GPU-операций |

### Разделение ответственности

```
┌─────────────────────┐          ┌─────────────────────┐
│       HOST (CPU)     │          │     DEVICE (GPU)    │
├─────────────────────┤          ├─────────────────────┤
│ • Application logic  │          │ • Rendering         │
│ • Resource creation  │          │ • Compute shaders   │
│ • Command recording  │  ──────► │ • Memory access     │
│ • Synchronization    │          │ • Parallel exec     │
│ • Window management  │          │                     │
└─────────────────────┘          └─────────────────────┘
        │                                │
        │    vkQueueSubmit()             │
        │    vkQueuePresentKHR()         │
        └────────────────────────────────┘
```

### Передача данных

| Направление     | Метод           | Типичное использование                   |
|-----------------|-----------------|------------------------------------------|
| Host → Device   | Staging buffer  | Загрузка текстур, mesh-данных            |
| Device → Host   | Readback buffer | GPU → CPU результаты (occlusion queries) |
| Device ↔ Device | GPU-only        | Рендеринг, compute                       |

---

## Execution Model

### Команды и очереди

Vulkan использует **командные буферы** для записи команд, которые затем отправляются в **очереди**:

```
┌──────────────────────────────────────────────────────────┐
│                    Command Buffer                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │ vkCmdBind│ │ vkCmdSet │ │ vkCmdDraw│ │ vkCmdBind│     │
│  │ Pipeline │ │ Viewport │ │          │ │ DescSet  │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
└──────────────────────────────────────────────────────────┘
                          │
                          │ vkQueueSubmit()
                          ▼
┌──────────────────────────────────────────────────────────┐
│                       Queue                               │
│  [Cmd1] → [Cmd2] → [Cmd3] → [Cmd4] → ... → GPU execution │
└──────────────────────────────────────────────────────────┘
```

### Порядок выполнения

**Важно:** Команды в одном command buffer выполняются **последовательно**, но:

- Разные command buffers в одном submit — **последовательно**
- Разные submits — **последовательно** (если не указано иное)
- Разные очереди — **параллельно** (требует синхронизации)

---

## Очереди и Queue Families

### Queue Family

Группа очередей с одинаковыми возможностями:

```cpp
typedef struct VkQueueFamilyProperties {
    VkQueueFlags    queueFlags;           // capabilities
    uint32_t        queueCount;           // number of queues
    uint32_t        timestampValidBits;
    VkExtent3D      minImageTransferGranularity;
} VkQueueFamilyProperties;

// Queue flags
VK_QUEUE_GRAPHICS_BIT       // Rendering, tessellation, geometry
VK_QUEUE_COMPUTE_BIT        // Compute shaders (graphics not guaranteed)
VK_QUEUE_TRANSFER_BIT       // Copy operations (usually subset of graphics/compute)
VK_QUEUE_SPARSE_BINDING_BIT // Sparse memory binding
```

### Типичная конфигурация GPU

| Queue Family Index | Flags                           | Count | Purpose                 |
|--------------------|---------------------------------|-------|-------------------------|
| 0                  | GRAPHICS \| COMPUTE \| TRANSFER | 16    | Основная очередь        |
| 1                  | TRANSFER                        | 2     | Асинхронные копирования |
| 2                  | COMPUTE                         | 8     | Async compute           |

### Выбор очереди

```cpp
uint32_t graphicsFamily = UINT32_MAX;
uint32_t computeFamily = UINT32_MAX;
uint32_t transferFamily = UINT32_MAX;

for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        graphicsFamily = i;
    }
    if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
        !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        computeFamily = i;  // Dedicated compute queue
    }
    if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
        !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
        !(queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
        transferFamily = i;  // Dedicated transfer queue
    }
}
```

---

## Командные буферы

### Уровни командных буферов

| Level         | Описание                                            | Использование                         |
|---------------|-----------------------------------------------------|---------------------------------------|
| **Primary**   | Может быть отправлен в очередь                      | Основные команды рендеринга           |
| **Secondary** | Выполняется из primary через `vkCmdExecuteCommands` | Многопоточная запись, reusable passes |

### Состояния командного буфера

```
┌───────────┐   vkBeginCommandBuffer()   ┌───────────┐
│  Invalid  │ ─────────────────────────► │ Recording │
└───────────┘                            └───────────┘
                                               │
                              vkEndCommandBuffer()
                                               │
                                               ▼
┌───────────┐   vkResetCommandBuffer()   ┌───────────┐
│  Invalid  │ ◄───────────────────────── │ Executable│
└───────────┘                            └───────────┘
                                               │
                              vkQueueSubmit()
                                               │
                                               ▼
                                         ┌───────────┐
                                         │  Pending  │
                                         └───────────┘
                                               │
                              GPU completes execution
                                               │
                                               ▼
                                         ┌───────────┐
                                         │  Invalid  │
                                         └───────────┘
```

### Пулы командных буферов

```cpp
VkCommandPoolCreateInfo poolInfo = {};
poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
poolInfo.queueFamilyIndex = graphicsFamily;
poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
// TRANSIENT_BIT — для краткоживущих буферов
// RESET_COMMAND_BUFFER_BIT — позволяет сброс отдельного буфера
```

---

## Синхронизация

### Типы примитивов синхронизации

| Примитив      | Область               | Назначение                               |
|---------------|-----------------------|------------------------------------------|
| **Fence**     | Host ↔ Device         | CPU ждёт завершения GPU                  |
| **Semaphore** | Device ↔ Device       | Синхронизация между очередями/операциями |
| **Barrier**   | Внутри command buffer | Упорядочивание стадий pipeline           |

### Fence

```cpp
// Создание
VkFenceCreateInfo fenceInfo = {};
fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Изначально signalized
vkCreateFence(device, &fenceInfo, nullptr, &fence);

// Ожидание на CPU
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
vkResetFences(device, 1, &fence);

// Signal при submit
vkQueueSubmit(queue, 1, &submitInfo, fence);
```

### Semaphore

```cpp
// Создание
VkSemaphoreCreateInfo semInfo = {};
semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
vkCreateSemaphore(device, &semInfo, nullptr, &semaphore);

// Использование в submit
VkSubmitInfo submitInfo = {};
submitInfo.waitSemaphoreCount = 1;
submitInfo.pWaitSemaphores = &waitSemaphore;
submitInfo.pWaitDstStageMask = &waitStage;
submitInfo.signalSemaphoreCount = 1;
submitInfo.pSignalSemaphores = &signalSemaphore;
```

### Pipeline Barrier

```cpp
vkCmdPipelineBarrier(
    commandBuffer,
    srcStageMask,    // src pipeline stage
    dstStageMask,    // dst pipeline stage
    dependencyFlags,
    memoryBarrierCount, pMemoryBarriers,
    bufferMemoryBarrierCount, pBufferMemoryBarriers,
    imageMemoryBarrierCount, pImageMemoryBarriers
);
```

### Типичный сценарий синхронизации

```
Frame N:
  Acquire Image (wait: imageAvailable semaphore)
       ↓
  Submit Commands (wait: imageAvailable, signal: renderFinished)
       ↓
  Present (wait: renderFinished semaphore)
       ↓
  Wait Fence (CPU block until GPU done with frame N-MAX_FRAMES_IN_FLIGHT)
```

---

## Ресурсы и память

### Типы ресурсов

| Ресурс                         | Описание               | Типичное содержимое                     |
|--------------------------------|------------------------|-----------------------------------------|
| **VkBuffer**                   | Линейный массив данных | Vertices, indices, uniforms             |
| **VkImage**                    | Многомерные данные     | Textures, render targets, depth buffers |
| **VkAccelerationStructureKHR** | Для ray tracing        | BVH структуры                           |

### Memory Types

GPU имеет несколько типов памяти с разными характеристиками:

| Тип               | Характеристики               | Использование                                       |
|-------------------|------------------------------|-----------------------------------------------------|
| **DEVICE_LOCAL**  | Быстрая, GPU-only            | Текстуры, render targets, часто используемые буферы |
| **HOST_VISIBLE**  | Доступна CPU                 | Staging, readback, dynamic uniforms                 |
| **HOST_COHERENT** | Автоматическая когерентность | Упрощает синхронизацию                              |
| **HOST_CACHED**   | Кэшируется CPU               | Быстрый readback                                    |

### Memory Heaps

```cpp
VkPhysicalDeviceMemoryProperties memProps;
vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
    // memoryHeaps[i].size — размер кучи в байтах
    // memoryHeaps[i].flags — DEVICE_LOCAL или нет
}

for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
    // memoryTypes[i].propertyFlags — комбинация флагов выше
    // memoryTypes[i].heapIndex — индекс кучи
}
```

### Binding памяти

```cpp
// Создание буфера
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = size;
bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

// Требования к памяти
VkMemoryRequirements memReqs;
vkGetBufferMemoryRequirements(device, buffer, &memReqs);

// Выбор типа памяти
uint32_t memoryTypeIndex = findMemoryType(
    memReqs.memoryTypeBits,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
);

// Выделение памяти
VkMemoryAllocateInfo allocInfo = {};
allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
allocInfo.allocationSize = memReqs.size;
allocInfo.memoryTypeIndex = memoryTypeIndex;
vkAllocateMemory(device, &allocInfo, nullptr, &memory);

// Binding
vkBindBufferMemory(device, buffer, memory, 0);
```

---

## Render Pass

### Назначение

Render Pass описывает:

- Attachments (color, depth, input)
- Load/store operations
- Layout transitions
- Subpasses и их зависимости

### Структура

```cpp
VkAttachmentDescription attachments[] = {
    // Color attachment
    {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,      // При входе
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,    // При выходе
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    },
    // Depth attachment
    {
        .format = VK_FORMAT_D32_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    }
};

VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

VkSubpassDescription subpass = {};
subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
subpass.colorAttachmentCount = 1;
subpass.pColorAttachments = &colorRef;
subpass.pDepthStencilAttachment = &depthRef;
```

### Subpass Dependencies

Определяют зависимости между subpass или между render pass и внешними операциями:

```cpp
VkSubpassDependency dependencies[] = {
    {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    }
};
```

---

## Pipeline

### Graphics Pipeline

Объединяет все state для рендеринга:

```
┌─────────────────────────────────────────────────────────┐
│                 Graphics Pipeline                        │
├─────────────────────────────────────────────────────────┤
│  Shader Stages        │ Vertex, Fragment, Geometry...   │
│  Vertex Input         │ Bindings, attributes            │
│  Input Assembly       │ Topology, primitive restart     │
│  Tessellation         │ Patches, control/eval shaders   │
│  Viewport             │ Viewports, scissors             │
│  Rasterization        │ Polygon mode, cull mode, depth  │
│  Multisample          │ MSAA settings                   │
│  Depth/Stencil        │ Depth test, stencil ops         │
│  Color Blend          │ Blend factors, logic op         │
│  Dynamic State        │ Dynamically changeable states   │
│  Layout               │ Descriptor sets, push constants │
│  Render Pass          │ Compatibility                   │
└─────────────────────────────────────────────────────────┘
```

### Compute Pipeline

Упрощённый pipeline для compute shaders:

```cpp
VkComputePipelineCreateInfo computeInfo = {};
computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
computeInfo.stage = shaderStage;  // Только один shader stage
computeInfo.layout = pipelineLayout;

vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computeInfo, nullptr, &computePipeline);
```

### Pipeline Cache

```cpp
VkPipelineCacheCreateInfo cacheInfo = {};
cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipelineCache);

// Использование при создании pipeline
vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline);

// Сохранение на диск для повторного использования
size_t cacheSize;
vkGetPipelineCacheData(device, pipelineCache, &cacheSize, nullptr);
std::vector<char> cacheData(cacheSize);
vkGetPipelineCacheData(device, pipelineCache, &cacheSize, cacheData.data());
// Запись cacheData в файл...
```

---

## Descriptor Sets

### Назначение

Descriptor sets связывают ресурсы (buffers, images) с шейдерами:

```cpp
// Layout binding
VkDescriptorSetLayoutBinding uboBinding = {};
uboBinding.binding = 0;
uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
uboBinding.descriptorCount = 1;
uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

// Descriptor set layout
VkDescriptorSetLayoutCreateInfo layoutInfo = {};
layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
layoutInfo.bindingCount = 1;
layoutInfo.pBindings = &uboBinding;
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

// Descriptor pool
VkDescriptorPoolSize poolSize = {};
poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
poolSize.descriptorCount = maxFrames;

VkDescriptorPoolCreateInfo poolInfo = {};
poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
poolInfo.maxSets = maxFrames;
poolInfo.poolSizeCount = 1;
poolInfo.pPoolSizes = &poolSize;
vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

// Allocate descriptor set
VkDescriptorSetAllocateInfo allocInfo = {};
allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
allocInfo.descriptorPool = descriptorPool;
allocInfo.descriptorSetCount = 1;
allocInfo.pSetLayouts = &descriptorSetLayout;
vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

// Update descriptor set
VkDescriptorBufferInfo bufferInfo = {};
bufferInfo.buffer = uniformBuffer;
bufferInfo.offset = 0;
bufferInfo.range = sizeof(UniformData);

VkWriteDescriptorSet descriptorWrite = {};
descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
descriptorWrite.dstSet = descriptorSet;
descriptorWrite.dstBinding = 0;
descriptorWrite.descriptorCount = 1;
descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
descriptorWrite.pBufferInfo = &bufferInfo;
vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
```

### Типы дескрипторов

| Тип                    | Ресурс              | Использование                |
|------------------------|---------------------|------------------------------|
| UNIFORM_BUFFER         | VkBuffer            | Матрицы, константы шейдера   |
| STORAGE_BUFFER         | VkBuffer            | Read/write данные из шейдера |
| COMBINED_IMAGE_SAMPLER | VkImage + VkSampler | Текстуры                     |
| STORAGE_IMAGE          | VkImage             | Image load/store             |
| INPUT_ATTACHMENT       | VkImage             | Subpass input                |

---

## Ключевые принципы

1. **Явность** — всё указывается явно, нет скрытого состояния
2. **Immutable pipelines** — pipeline state фиксируется при создании
3. **Command buffers** — команды записываются заранее
4. **Синхронизация** — приложение отвечает за все барьеры
5. **Memory management** — явное выделение и освобождение памяти
