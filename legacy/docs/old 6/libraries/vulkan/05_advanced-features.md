# Vulkan Advanced Features

**🟡🔴 Уровень 2-3: Средний + Продвинутый** — Расширенные возможности Vulkan 1.4: timeline semaphores, descriptor
indexing, dynamic rendering, mesh shaders.

---

## Содержание

1. [Timeline Semaphores](#timeline-semaphores)
2. [Descriptor Indexing (Bindless)](#descriptor-indexing-bindless)
3. [Dynamic Rendering](#dynamic-rendering)
4. [Mesh Shaders](#mesh-shaders)
5. [Buffer Device Address](#buffer-device-address)
6. [Synchronization2](#synchronization2)
7. [Pipeline Library](#pipeline-library)

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

## Краткая сводка

| Фича                  | Расширение                   | Уровень | Назначение              |
|-----------------------|------------------------------|---------|-------------------------|
| Timeline Semaphores   | VK_KHR_timeline_semaphore    | 🟡      | Async queue sync        |
| Descriptor Indexing   | VK_EXT_descriptor_indexing   | 🟡      | Bindless resources      |
| Dynamic Rendering     | VK_KHR_dynamic_rendering     | 🟡      | No render pass objects  |
| Mesh Shaders          | VK_EXT_mesh_shader           | 🔴      | GPU geometry generation |
| Buffer Device Address | VK_KHR_buffer_device_address | 🟡      | Direct GPU pointers     |
| Synchronization2      | VK_KHR_synchronization2      | 🟡      | Improved sync API       |
| Pipeline Library      | VK_EXT_pipeline_library      | 🔴      | Pipeline reuse          |
