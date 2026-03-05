# Vulkan Advanced Features

**🟡🔴 Уровень 2-3: Средний + Продвинутый** — Расширенные возможности Vulkan 1.4: timeline semaphores, descriptor
indexing, dynamic rendering, mesh shaders.

---

## Содержание

1. [Vulkan 1.4 Core Features](#vulkan-14-core-features)
2. [Timeline Semaphores](#timeline-semaphores)
3. [Descriptor Indexing (Bindless)](#descriptor-indexing-bindless)
4. [Dynamic Rendering](#dynamic-rendering)
5. [Mesh Shaders](#mesh-shaders)
6. [Buffer Device Address](#buffer-device-address)
7. [Synchronization2](#synchronization2)
8. [Pipeline Library](#pipeline-library)
9. [Memory Layout Validation](#memory-layout-validation)

---

## Vulkan 1.4 Core Features

### Обзор

**Vulkan 1.4** (декабрь 2024) включил в core множество расширений, которые ранее требовали явного включения. Это
упрощает код и повышает переносимость.

### Promoted Extensions → Core

| Было расширением                | Стало в Vulkan 1.4 | Назначение                     |
|---------------------------------|--------------------|--------------------------------|
| `VK_KHR_dynamic_rendering`      | Core               | Рендеринг без Render Pass      |
| `VK_KHR_timeline_semaphore`     | Core               | Тонкая синхронизация           |
| `VK_KHR_synchronization2`       | Core               | Улучшенный API синхронизации   |
| `VK_EXT_descriptor_indexing`    | Core               | Bindless ресурсы               |
| `VK_KHR_buffer_device_address`  | Core               | GPU-указатели на буферы        |
| `VK_KHR_push_descriptor`        | Core               | Дескрипторы без пула           |
| `VK_KHR_shader_draw_parameters` | Core               | Встроенные переменные шейдеров |
| `VK_KHR_maintenance4`           | Core               | Исправления API                |
| `VK_KHR_maintenance5`           | Core               | Дополнительные улучшения       |
| `VK_KHR_maintenance6`           | Core               | Битовые операции, memcpy       |

### Проверка поддержки Vulkan 1.4

```cpp
VkApplicationInfo appInfo{};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.apiVersion = VK_API_VERSION_1_4;  // Запрашиваем Vulkan 1.4

VkInstanceCreateInfo instanceInfo{};
instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
instanceInfo.pApplicationInfo = &appInfo;

VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
    // Driver doesn't support Vulkan 1.4, fallback to 1.3
    appInfo.apiVersion = VK_API_VERSION_1_3;
    vkCreateInstance(&instanceInfo, nullptr, &instance);
}
```

### Device Creation (упрощённое)

```cpp
// Vulkan 1.4: многие фичи включены по умолчанию
// Дополнительные features запрашиваются через pNext цепочку

VkPhysicalDeviceVulkan14Features v14Features{};
v14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

// Vulkan 1.4 core features
v14Features.dynamicRendering = VK_TRUE;              // Was VK_KHR_dynamic_rendering
v14Features.timelineSemaphore = VK_TRUE;             // Was VK_KHR_timeline_semaphore
v14Features.synchronization2 = VK_TRUE;              // Was VK_KHR_synchronization2
v14Features.descriptorIndexing = VK_TRUE;            // Was VK_EXT_descriptor_indexing
v14Features.bufferDeviceAddress = VK_TRUE;           // Was VK_KHR_buffer_device_address
v14Features.pushDescriptor = VK_TRUE;                // Was VK_KHR_push_descriptor

VkDeviceCreateInfo deviceInfo{};
deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
deviceInfo.pNext = &v14Features;

VkDevice device;
vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
```

### ProjectV: Vulkan 1.4 Requirements

```cpp
// ProjectV требует Vulkan 1.4 как минимум
// Все core features из 1.4 доступны без расширений

namespace projectv::render {

struct Vulkan14Context {
    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkPhysicalDeviceVulkan14Features features{};

    auto initialize() noexcept -> std::expected<void, VulkanError> {
        // Verify Vulkan 1.4 support
        uint32_t apiVersion;
        vkEnumerateInstanceVersion(&apiVersion);

        if (apiVersion < VK_API_VERSION_1_4) {
            return std::unexpected(VulkanError::Vulkan14NotSupported);
        }

        // Create instance with 1.4
        VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_API_VERSION_1_4
        };

        // ... create instance, select device ...

        // Query Vulkan 1.4 features
        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &features
        };

        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

        // Verify required features
        if (!features.dynamicRendering || !features.timelineSemaphore) {
            return std::unexpected(VulkanError::RequiredFeaturesMissing);
        }

        return {};
    }
};

} // namespace projectv::render
```

### Benefits for ProjectV

| Feature               | Benefit                                             |
|-----------------------|-----------------------------------------------------|
| `dynamicRendering`    | Упрощённый код, нет RenderPass/Framebuffer объектов |
| `timelineSemaphore`   | Эффективная синхронизация GPU↔CPU                   |
| `synchronization2`    | `vkCmdPipelineBarrier2`, `vkQueueSubmit2`           |
| `descriptorIndexing`  | Bindless текстуры, массивы дескрипторов             |
| `bufferDeviceAddress` | GPU pointers для SVO traversal                      |
| `pushDescriptor`      | Быстрое обновление дескрипторов                     |

### Backward Compatibility Note

```cpp
// Для совместимости с Vulkan 1.3, можно проверять фичи динамически:

#if VK_API_VERSION >= VK_API_VERSION_1_4
    // Use Vulkan 1.4 core features directly
    v14Features.dynamicRendering = VK_TRUE;
#else
    // Fallback to extension
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicFeatures{};
    dynamicFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicFeatures.dynamicRendering = VK_TRUE;
    // Add to pNext chain
#endif
```

### Maintenance Extensions (Vulkan 1.4 Core)

**VK_KHR_maintenance5:**

- `vkCmdBindIndexBuffer2KHR` — смещение индексов
- `vkGetRenderingAreaGranularityKHR` — оптимизация render area
- New bit operations in SPIR-V

**VK_KHR_maintenance6:**

- `vkCmdBindDescriptorSets2KHR` — расширенный bind
- `VK_WHOLE_SIZE` improvements
- Null descriptor support

```cpp
// Maintenance5 example: bind index buffer with offset
vkCmdBindIndexBuffer2KHR(
    cmdBuffer,
    indexBuffer,
    0,                    // offset
    bufferSize,           // size (was VK_WHOLE_SIZE)
    VK_INDEX_TYPE_UINT32
);
```

---

## Timeline Semaphores

### Обзор

Timeline semaphores — это семафоры со значением (counter), позволяющие более тонкую синхронизацию между очередями без
создания множества бинарных семафоров.

### Преимущества перед бинарными семафорами

| Аспект                     | Бинарные семафоры | Timeline semaphores    |
|----------------------------|-------------------|------------------------|
| Количество                 | Один на операцию  | Один на много операций |
| Значение                   | 0 или 1           | Любое uint64           |
| Wait на CPU                | Нет               | `vkWaitSemaphores`     |
| Signal с CPU               | Нет               | `vkSignalSemaphore`    |
| Многократное использование | Нет               | Да                     |

### Включение

```cpp
VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures = {};
timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
timelineFeatures.timelineSemaphore = VK_TRUE;

VkDeviceCreateInfo deviceInfo = {};
deviceInfo.pNext = &timelineFeatures;
```

### Создание

```cpp
VkSemaphoreTypeCreateInfoKHR timelineInfo = {};
timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
timelineInfo.initialValue = 0;

VkSemaphoreCreateInfo semInfo = {};
semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
semInfo.pNext = &timelineInfo;

VkSemaphore timelineSemaphore;
vkCreateSemaphore(device, &semInfo, nullptr, &timelineSemaphore);
```

### Использование в submit

```cpp
VkTimelineSemaphoreSubmitInfoKHR timelineSubmit = {};
timelineSubmit.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
timelineSubmit.waitSemaphoreValueCount = 1;
timelineSubmit.pWaitSemaphoreValues = &waitValue;     // Ждать value >= waitValue
timelineSubmit.signalSemaphoreValueCount = 1;
timelineSubmit.pSignalSemaphoreValues = &signalValue; // Signal: value = signalValue

VkSubmitInfo submitInfo = {};
submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
submitInfo.pNext = &timelineSubmit;
submitInfo.waitSemaphoreCount = 1;
submitInfo.pWaitSemaphores = &timelineSemaphore;
submitInfo.signalSemaphoreCount = 1;
submitInfo.pSignalSemaphores = &timelineSemaphore;
```

### Wait и Signal на CPU

```cpp
// Wait на CPU
VkSemaphoreWaitInfoKHR waitInfo = {};
waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
waitInfo.semaphoreCount = 1;
waitInfo.pSemaphores = &timelineSemaphore;
waitInfo.pValues = &waitValue;

vkWaitSemaphores(device, &waitInfo, UINT64_MAX);

// Signal с CPU
VkSemaphoreSignalInfoKHR signalInfo = {};
signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR;
signalInfo.semaphore = timelineSemaphore;
signalInfo.value = signalValue;

vkSignalSemaphore(device, &signalInfo);
```

### Сценарий: Async Compute

```cpp
// Одна timeline semaphore для синхронизации graphics и compute
// Value 0-99: graphics queue
// Value 100-199: compute queue

// Graphics queue ждёт compute (value >= 100)
uint64_t graphicsWaitValue = 100;
uint64_t graphicsSignalValue = 150;

// Compute queue ждёт graphics (value >= 50)
uint64_t computeWaitValue = 50;
uint64_t computeSignalValue = 100;
```

---

## Descriptor Indexing (Bindless)

### Обзор

Descriptor Indexing позволяет:

- Использовать массивы дескрипторов переменной длины
- Обновлять дескрипторы после binding
- Обращаться к текстурам/буферам по индексу из шейдера

### Включение

```cpp
VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
indexingFeatures.runtimeDescriptorArray = VK_TRUE;
indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
```

### Descriptor Set Layout

```cpp
VkDescriptorSetLayoutBinding textureBinding = {};
textureBinding.binding = 0;
textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
textureBinding.descriptorCount = 1024;  // Массив из 1024 текстур
textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

VkDescriptorBindingFlags bindingFlags =
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |        // Не все элементы должны быть valid
    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | // Переменная длина
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;        // Обновление после bind

VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {};
flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
flagsInfo.bindingCount = 1;
flagsInfo.pBindingFlags = &bindingFlags;

VkDescriptorSetLayoutCreateInfo layoutInfo = {};
layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
layoutInfo.pNext = &flagsInfo;
layoutInfo.bindingCount = 1;
layoutInfo.pBindings = &textureBinding;
layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
```

### Descriptor Pool

```cpp
VkDescriptorPoolSize poolSize = {};
poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
poolSize.descriptorCount = 1024;

VkDescriptorPoolCreateInfo poolInfo = {};
poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
poolInfo.maxSets = 1;
poolInfo.poolSizeCount = 1;
poolInfo.pPoolSizes = &poolSize;
```

### Выделение Descriptor Set

```cpp
uint32_t maxTextures = 1024;
VkDescriptorSetVariableDescriptorCountAllocateInfo variableCount = {};
variableCount.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
variableCount.descriptorSetCount = 1;
variableCount.pDescriptorCounts = &maxTextures;

VkDescriptorSetAllocateInfo allocInfo = {};
allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
allocInfo.pNext = &variableCount;
allocInfo.descriptorPool = pool;
allocInfo.descriptorSetCount = 1;
allocInfo.pSetLayouts = &layout;
```

### Обновление текстур

```cpp
uint32_t textureIndex = 42;

VkDescriptorImageInfo imageInfo = {};
imageInfo.sampler = sampler;
imageInfo.imageView = textureView;
imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

VkWriteDescriptorSet write = {};
write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
write.dstSet = descriptorSet;
write.dstBinding = 0;
write.dstArrayElement = textureIndex;  // Индекс в массиве
write.descriptorCount = 1;
write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
write.pImageInfo = &imageInfo;

vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
```

### Шейдер

```glsl
#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D textures[];

// NonUniform indexing — индекс не константа
vec4 sampleTexture(uint index, vec2 uv) {
    return texture(textures[nonuniformEXT(index)], uv);
}
```

---

## Dynamic Rendering

### Обзор

Dynamic Rendering позволяет рендерить без Render Pass и Framebuffer объектов, упрощая код и добавляя гибкость.

### Включение

```cpp
VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicFeatures = {};
dynamicFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
dynamicFeatures.dynamicRendering = VK_TRUE;
```

### Создание Pipeline без Render Pass

```cpp
VkGraphicsPipelineCreateInfo pipelineInfo = {};
pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
pipelineInfo.renderPass = VK_NULL_HANDLE;  // Без render pass!

// Указание форматов напрямую
VkPipelineRenderingCreateInfoKHR renderingInfo = {};
renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
renderingInfo.colorAttachmentCount = 1;
renderingInfo.pColorAttachmentFormats = &colorFormat;
renderingInfo.depthAttachmentFormat = depthFormat;

pipelineInfo.pNext = &renderingInfo;
```

### Начало рендеринга

```cpp
VkRenderingAttachmentInfoKHR colorAttachment = {};
colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
colorAttachment.imageView = colorImageView;
colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

VkRenderingAttachmentInfoKHR depthAttachment = {};
depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
depthAttachment.imageView = depthImageView;
depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
depthAttachment.clearValue.depthStencil = {1.0f, 0};

VkRenderingInfoKHR renderingInfo = {};
renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
renderingInfo.renderArea = {{0, 0}, {width, height}};
renderingInfo.layerCount = 1;
renderingInfo.colorAttachmentCount = 1;
renderingInfo.pColorAttachments = &colorAttachment;
renderingInfo.pDepthAttachment = &depthAttachment;

vkCmdBeginRenderingKHR(commandBuffer, &renderingInfo);

// Draw calls...

vkCmdEndRenderingKHR(commandBuffer);
```

### Layout Transitions

При Dynamic Rendering нужно явно управлять layout:

```cpp
VkImageMemoryBarrier barrier = {};
barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
barrier.srcAccessMask = 0;
barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
barrier.image = colorImage;
barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    0, 0, nullptr, 0, nullptr, 1, &barrier);
```

---

## Mesh Shaders

### Обзор

Mesh Shaders заменяют vertex shader на task + mesh шейдеры, позволяя генерировать геометрию на GPU.

### Расширения

```cpp
// Cross-vendor (предпочтительно)
VK_EXT_mesh_shader

// NVIDIA-specific (legacy)
VK_NV_mesh_shader
```

### Включение

```cpp
VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures = {};
meshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
meshFeatures.meshShader = VK_TRUE;
meshFeatures.taskShader = VK_TRUE;
```

### Task Shader (GLSL)

```glsl
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;

taskPayloadSharedEXT struct TaskPayload {
    uint meshCount;
    uint meshIndices[32];
} payload;

void main() {
    uint idx = gl_GlobalInvocationID.x;

    // Culling logic
    if (isVisible(idx)) {
        uint meshIdx = atomicAdd(payload.meshCount, 1);
        payload.meshIndices[meshIdx] = idx;
    }

    barrier();

    if (gl_LocalInvocationIndex == 0 && payload.meshCount > 0) {
        EmitMeshTasksEXT(payload.meshCount, 1, 1);
    }
}
```

### Mesh Shader (GLSL)

```glsl
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(triangles) out;
layout(max_vertices = 64, max_primitives = 64) out;

taskPayloadSharedEXT struct TaskPayload {
    uint meshCount;
    uint meshIndices[32];
} payload;

out gl_MeshPerVertexEXT {
    vec4 gl_Position;
} gl_MeshVerticesEXT[];

void main() {
    uint taskIdx = gl_WorkGroupID.x;
    uint meshIdx = payload.meshIndices[taskIdx];

    // Generate geometry
    uint vertexCount = generateVertices(meshIdx, gl_MeshVerticesEXT);
    uint primitiveCount = generatePrimitives(meshIdx, gl_PrimitiveTriangleIndicesEXT);

    SetMeshOutputsEXT(vertexCount, primitiveCount);
}
```

### Отрисовка

```cpp
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);
vkCmdDrawMeshTasksEXT(cmd, groupCountX, groupCountY, groupCountZ);

// Или indirect
vkCmdDrawMeshTasksIndirectEXT(cmd, buffer, offset, drawCount, stride);

// Или indirect count
vkCmdDrawMeshTasksIndirectCountEXT(cmd, buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
```

---

## Buffer Device Address

### Обзор

Buffer Device Address (BDA) позволяет получать 64-битные GPU-адреса буферов и передавать их напрямую в шейдеры.

### Включение

```cpp
VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures = {};
bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
bdaFeatures.bufferDeviceAddress = VK_TRUE;
bdaFeatures.bufferDeviceAddressCaptureReplay = VK_TRUE;  // Для RenderDoc
```

### Создание буфера

```cpp
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = size;
bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;  // Обязательно!
```

### Получение адреса

```cpp
VkBufferDeviceAddressInfo addrInfo = {};
addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
addrInfo.buffer = buffer;

VkDeviceAddress address = vkGetBufferDeviceAddress(device, &addrInfo);
```

### Использование в шейдере

```glsl
#version 460
#extension GL_EXT_buffer_reference2 : require

layout(buffer_reference, buffer_reference_align = 4) buffer DataBuffer {
    uint data[];
};

layout(push_constant) uniform PushConstants {
    DataBuffer buffer;
    uint count;
} pc;

void main() {
    uint value = pc.buffer.data[gl_VertexIndex];
}
```

### Передача через Push Constants

```cpp
struct PushConstants {
    VkDeviceAddress bufferAddress;
    uint32_t count;
};

PushConstants pc;
pc.bufferAddress = vkGetBufferDeviceAddress(device, &addrInfo);
pc.count = elementCount;

vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
```

---

## Synchronization2

### Обзор

Synchronization2 упрощает и уточняет API синхронизации.

### Включение

```cpp
VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features = {};
sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
sync2Features.synchronization2 = VK_TRUE;
```

### vkCmdPipelineBarrier2

```cpp
VkMemoryBarrier2KHR memoryBarrier = {};
memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;
memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR;

VkDependencyInfoKHR dependencyInfo = {};
dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
dependencyInfo.memoryBarrierCount = 1;
dependencyInfo.pMemoryBarriers = &memoryBarrier;

vkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);
```

### vkQueueSubmit2

```cpp
VkCommandBufferSubmitInfoKHR cmdInfo = {};
cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
cmdInfo.commandBuffer = cmdBuffer;

VkSemaphoreSubmitInfoKHR waitInfo = {};
waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
waitInfo.semaphore = semaphore;
waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

VkSubmitInfo2KHR submitInfo = {};
submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
submitInfo.waitSemaphoreInfoCount = 1;
submitInfo.pWaitSemaphoreInfos = &waitInfo;
submitInfo.commandBufferInfoCount = 1;
submitInfo.pCommandBufferInfos = &cmdInfo;

vkQueueSubmit2KHR(queue, 1, &submitInfo, fence);
```

---

## Pipeline Library

### Обзор

Pipeline Library позволяет переиспользовать скомпилированные части pipeline между разными pipelines.

### Включение

```cpp
VkPhysicalDevicePipelineLibraryGroupFeaturesEXT libraryFeatures = {};
libraryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_LIBRARY_GROUP_FEATURES_EXT;
libraryFeatures.pipelineLibraryGroup = VK_TRUE;
```

### Создание библиотеки

```cpp
VkPipelineLibraryCreateInfoKHR libraryInfo = {};
libraryInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
libraryInfo.libraryCount = 1;
libraryInfo.pLibraries = &vertexLibrary;

VkGraphicsPipelineCreateInfo pipelineInfo = {};
pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
pipelineInfo.pNext = &libraryInfo;
pipelineInfo.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;

VkPipeline pipeline;
vkCreateGraphicsPipelines(device, cache, 1, &pipelineInfo, nullptr, &pipeline);
```

### Типичный сценарий

```
Vertex Library    Fragment Library A    Fragment Library B
     │                    │                    │
     └────────────────────┼────────────────────┘
                          │
                    Pipeline A, Pipeline B
                    (reuse vertex library)
```

---

## Memory Layout Validation

### Обзор

При передаче данных между CPU (C++) и GPU (Slang/GLSL/SPIR-V) критически важно обеспечить идентичность layout структур.
Несовпадение приводит к corruption данных и труднодиагностируемым багам.

### std140 vs std430 Layout Rules

| Правило           | std140 (UBO)                  | std430 (SSBO)         |
|-------------------|-------------------------------|-----------------------|
| `alignof(float)`  | 4                             | 4                     |
| `alignof(vec2)`   | 8                             | 8                     |
| `alignof(vec3)`   | 16 (!)                        | 12                    |
| `alignof(vec4)`   | 16                            | 16                    |
| `alignof(struct)` | max(alignof(members), 16)     | max(alignof(members)) |
| Array stride      | round_up(sizeof(element), 16) | sizeof(element)       |
| Nested struct     | 16-byte aligned               | member-aligned        |

### Common Pitfalls

```cpp
// ОШИБКА: vec3 в std140 имеет alignment 16, не 12!
struct alignas(16) Uniforms {
    glm::vec3 position;  // Offset: 0,  Size: 12, Padding: 4 bytes!
    float intensity;     // Offset: 16, OK in std140
};

// ПРАВИЛЬНО: используем vec4 или явный padding
struct alignas(16) Uniforms {
    glm::vec4 position;  // Offset: 0,  Size: 16
    float intensity;     // Offset: 16, OK
};
```

### C++26 Static Assertions Pattern

```cpp
// ProjectV.Render.GPUTypes.cppm
export module ProjectV.Render.GPUTypes;

import std;
import glm;

export namespace projectv::render {

/// GPU-aligned struct for std430 layout
/// MUST match Slang shader layout exactly
export struct alignas(16) CellState {
    float density{0.0f};        ///< Offset: 0
    float velocity_x{0.0f};     ///< Offset: 4
    float velocity_y{0.0f};     ///< Offset: 8
    float velocity_z{0.0f};     ///< Offset: 12
    uint32_t material_type{0};  ///< Offset: 16
    uint32_t flags{0};          ///< Offset: 20
    float temperature{0.0f};    ///< Offset: 24
    float pressure{0.0f};       ///< Offset: 28
};
// Total size: 32 bytes = 2 × vec4 (std430 compatible)

// === Compile-time validation ===
static_assert(sizeof(CellState) == 32,
    "CellState must be 32 bytes to match std430 layout");

static_assert(alignof(CellState) == 16,
    "CellState must be 16-byte aligned for GPU buffers");

static_assert(offsetof(CellState, density) == 0,
    "CellState::density offset mismatch");

static_assert(offsetof(CellState, velocity_x) == 4,
    "CellState::velocity_x offset mismatch");

static_assert(offsetof(CellState, velocity_y) == 8,
    "CellState::velocity_y offset mismatch");

static_assert(offsetof(CellState, velocity_z) == 12,
    "CellState::velocity_z offset mismatch");

static_assert(offsetof(CellState, material_type) == 16,
    "CellState::material_type offset mismatch");

static_assert(offsetof(CellState, flags) == 20,
    "CellState::flags offset mismatch");

static_assert(offsetof(CellState, temperature) == 24,
    "CellState::temperature offset mismatch");

static_assert(offsetof(CellState, pressure) == 28,
    "CellState::pressure offset mismatch");

/// Uniform buffer struct for std140 layout
export struct alignas(16) CameraUniforms {
    glm::mat4 view{1.0f};           ///< Offset: 0,  Size: 64
    glm::mat4 projection{1.0f};     ///< Offset: 64, Size: 64
    glm::vec4 position{0.0f};       ///< Offset: 128, Size: 16
    glm::vec4 direction{0.0f};      ///< Offset: 144, Size: 16
    float near_plane{0.1f};         ///< Offset: 160
    float far_plane{1000.0f};       ///< Offset: 164
    float padding[2];               ///< Offset: 168-175, Align to 176
};
// Total size: 176 bytes = 11 × vec4 (std140 requires 16-byte alignment)

static_assert(sizeof(CameraUniforms) == 176,
    "CameraUniforms must be 176 bytes for std140 layout");

static_assert(offsetof(CameraUniforms, view) == 0,
    "CameraUniforms::view offset mismatch");

static_assert(offsetof(CameraUniforms, projection) == 64,
    "CameraUniforms::projection offset mismatch");

static_assert(offsetof(CameraUniforms, position) == 128,
    "CameraUniforms::position offset mismatch");

static_assert(offsetof(CameraUniforms, direction) == 144,
    "CameraUniforms::direction offset mismatch");

static_assert(offsetof(CameraUniforms, near_plane) == 160,
    "CameraUniforms::near_plane offset mismatch");

static_assert(offsetof(CameraUniforms, far_plane) == 164,
    "CameraUniforms::far_plane offset mismatch");

} // namespace projectv::render
```

### Slang Shader Validation

```slang
// VoxelCA.slang
module VoxelCA;

// Cell state structure - MUST match C++ CellState
struct CellState {
    float density;        // Offset: 0
    float velocity_x;     // Offset: 4
    float velocity_y;     // Offset: 8
    float velocity_z;     // Offset: 12
    uint material_type;   // Offset: 16
    uint flags;           // Offset: 20
    float temperature;    // Offset: 24
    float pressure;       // Offset: 28
};
// Total: 32 bytes (std430 compatible)

// Static assertion in Slang
static_assert(sizeof(CellState) == 32);

// Camera uniforms for std140
struct CameraUniforms {
    float4x4 view;        // Offset: 0,  Size: 64
    float4x4 projection;  // Offset: 64, Size: 64
    float4 position;      // Offset: 128, Size: 16
    float4 direction;     // Offset: 144, Size: 16
    float near_plane;     // Offset: 160
    float far_plane;      // Offset: 164
    float2 padding;       // Offset: 168-175
};
// Total: 176 bytes

static_assert(sizeof(CameraUniforms) == 176);
```

### Runtime Validation Function

```cpp
// ProjectV.Render.LayoutValidation.cppm
export module ProjectV.Render.LayoutValidation;

import std;
import glm;
import ProjectV.Render.GPUTypes;

export namespace projectv::render {

/// Validates GPU struct layouts at runtime (debug builds)
export auto validate_gpu_layouts() noexcept -> bool {
    bool valid = true;

    // CellState validation
    if (sizeof(CellState) != 32) {
        std::println(stderr, "ERROR: CellState size = {}, expected 32",
                    sizeof(CellState));
        valid = false;
    }

    if (alignof(CellState) != 16) {
        std::println(stderr, "ERROR: CellState alignment = {}, expected 16",
                    alignof(CellState));
        valid = false;
    }

    // CameraUniforms validation
    if (sizeof(CameraUniforms) != 176) {
        std::println(stderr, "ERROR: CameraUniforms size = {}, expected 176",
                    sizeof(CameraUniforms));
        valid = false;
    }

    // Print layout info (debug)
    if constexpr (DEBUG_BUILD) {
        std::println("GPU Layout Validation:");
        std::println("  CellState:");
        std::println("    sizeof = {}", sizeof(CellState));
        std::println("    alignof = {}", alignof(CellState));
        std::println("    offsetof(density) = {}", offsetof(CellState, density));
        std::println("    offsetof(velocity_x) = {}", offsetof(CellState, velocity_x));
        std::println("    offsetof(material_type) = {}", offsetof(CellState, material_type));
    }

    return valid;
}

/// Assert at startup
export auto assert_gpu_layouts() noexcept -> void {
    [[maybe_unused]] static bool validated = []{
        if (!validate_gpu_layouts()) {
            std::abort();
        }
        return true;
    }();
}

} // namespace projectv::render
```

### Debug Template Helper (C++26)

```cpp
// ProjectV.Render.LayoutDebug.cppm
export module ProjectV.Render.LayoutDebug;

import std;

export namespace projectv::render {

/// Template helper for printing struct layout
export template<typename T>
auto print_struct_layout(std::string_view name) noexcept -> void {
    std::println("Struct: {}", name);
    std::println("  sizeof = {}", sizeof(T));
    std::println("  alignof = {}", alignof(T));
}

/// Template helper for runtime offset check
export template<typename T, typename Member>
auto check_offset(std::string_view struct_name,
                 std::string_view member_name,
                 size_t expected_offset,
                 size_t actual_offset) noexcept -> bool {
    if (actual_offset != expected_offset) {
        std::println(stderr,
            "ERROR: {}::{} offset = {}, expected {}",
            struct_name, member_name, actual_offset, expected_offset);
        return false;
    }
    return true;
}

/// Macro for compile-time + runtime validation
#define VALIDATE_GPU_STRUCT(Struct, Size, Align) \
    static_assert(sizeof(Struct) == Size, #Struct " size mismatch"); \
    static_assert(alignof(Struct) == Align, #Struct " alignment mismatch")

} // namespace projectv::render
```

### Integration with Vulkan

```cpp
// When creating uniform buffers, always verify alignment

auto create_uniform_buffer(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage
) -> std::expected<Buffer, BufferError> {

    // Verify minimum alignment for uniform buffers
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);

    constexpr VkDeviceSize min_alignment = 16;  // std140 minimum
    if (props.limits.minUniformBufferOffsetAlignment > min_alignment) {
        // Adjust buffer offsets accordingly
    }

    // Create buffer with proper alignment
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo allocInfo{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        .preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    // ... create and return buffer ...
}
```

### Best Practices Summary

| Практика                              | Обоснование                   |
|---------------------------------------|-------------------------------|
| `static_assert` для всех GPU структур | Compile-time проверка         |
| `alignas(16)` для GPU structs         | Гарантирует 16-byte alignment |
| Explicit padding вместо implicit      | Ясность layout                |
| Runtime validation в debug builds     | Проверка на разных платформах |
| Slang `static_assert(sizeof(T))`      | Двусторонняя валидация        |
| Unit tests для layout                 | CI/CD проверка                |

---

## Краткая сводка

| Фича                     | Расширение                   | Уровень | Назначение              |
|--------------------------|------------------------------|---------|-------------------------|
| Vulkan 1.4 Core          | (Core in 1.4)                | 🟡      | Упрощённый API          |
| Timeline Semaphores      | VK_KHR_timeline_semaphore    | 🟡      | Async queue sync        |
| Descriptor Indexing      | VK_EXT_descriptor_indexing   | 🟡      | Bindless resources      |
| Dynamic Rendering        | VK_KHR_dynamic_rendering     | 🟡      | No render pass objects  |
| Mesh Shaders             | VK_EXT_mesh_shader           | 🔴      | GPU geometry generation |
| Buffer Device Address    | VK_KHR_buffer_device_address | 🟡      | Direct GPU pointers     |
| Synchronization2         | VK_KHR_synchronization2      | 🟡      | Improved sync API       |
| Pipeline Library         | VK_EXT_pipeline_library      | 🔴      | Pipeline reuse          |
| Memory Layout Validation | C++26 static_assert          | 🟡      | CPU-GPU data integrity  |
