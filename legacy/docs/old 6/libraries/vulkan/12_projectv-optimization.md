# Vulkan ProjectV Optimization

**🔴 Уровень 3: Продвинутый** — "Злые хаки" для максимальной производительности воксельного рендеринга в ProjectV.

> ⚠️ **Предупреждение:** Эти техники нарушают "безопасные" паттерны Vulkan ради производительности. Используйте только в
> hot paths после профилирования.

---

## Shader Objects вместо Pipelines

### Проблема с VkPipeline

| Проблема            | Влияние                     |
|---------------------|-----------------------------|
| Компиляция шейдеров | 10-100ms stall              |
| Permutations        | Взрывной рост комбинаций    |
| State management    | Огромные switch-конструкции |

### VK_EXT_shader_object

```cpp
// Создание шейдера напрямую
VkShaderCreateInfoEXT shaderInfo = {};
shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
shaderInfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
shaderInfo.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
shaderInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
shaderInfo.pCode = spirvCode.data();
shaderInfo.pName = "main";
shaderInfo.setLayoutCount = 1;
shaderInfo.pSetLayouts = &descriptorSetLayout;

VkShaderEXT meshShader;
vkCreateShadersEXT(device, 1, &shaderInfo, nullptr, &meshShader);
```

### Использование

```cpp
void render(VkCommandBuffer cmd) {
    // Bind shaders (вместо pipeline)
    VkShaderEXT shaders[] = {meshShader_, fragShader_};
    VkShaderStageFlagBits stages[] = {
        VK_SHADER_STAGE_MESH_BIT_EXT,
        VK_SHADER_STAGE_FRAGMENT_BIT
    };

    vkCmdBindShadersEXT(cmd, 2, stages, shaders);

    // Dynamic state
    vkCmdSetViewportWithCountEXT(cmd, 1, &viewport);
    vkCmdSetScissorWithCountEXT(cmd, 1, &scissor);
    vkCmdSetCullModeEXT(cmd, VK_CULL_MODE_BACK_BIT);

    // Draw
    vkCmdDrawMeshTasksEXT(cmd, groupCount, 1, 1);
}
```

### Hot Reloading

```cpp
class HotReloadableShaders {
    std::unordered_map<std::string, VkShaderEXT> shaders_;
    VkDevice device_;

public:
    void reload(const std::string& name, const std::vector<uint32_t>& spirv) {
        // Уничтожаем старый
        if (shaders_.count(name)) {
            vkDestroyShaderEXT(device_, shaders_[name], nullptr);
        }

        // Создаём новый — мгновенно!
        VkShaderCreateInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
        info.stage = getStage(name);
        info.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
        info.codeSize = spirv.size() * sizeof(uint32_t);
        info.pCode = spirv.data();
        info.pName = "main";

        vkCreateShadersEXT(device_, 1, &info, nullptr, &shaders_[name]);
    }
};
```

---

## Buffer Device Address Everywhere

### Отказ от Descriptor Sets

```
Традиционно:
CPU: vkCmdBindDescriptorSets() → запись в command buffer
GPU: Декодирование descriptor → чтение адреса → доступ к памяти

BDA:
CPU: vkCmdPushConstants() → запись 8 байт
GPU: Прямое чтение адреса → доступ к памяти
```

### Push Constants с адресами

```cpp
struct PushConstants {
    VkDeviceAddress voxelData;    // 8 байт
    VkDeviceAddress chunkInfo;    // 8 байт
    VkDeviceAddress indirectCmd;  // 8 байт
    uint32_t chunkCount;          // 4 байт
    uint32_t frameIndex;          // 4 байт
};  // Итого: 32 байта

void render(VkCommandBuffer cmd) {
    PushConstants pc = {};
    pc.voxelData = voxelBufferAddr_;
    pc.chunkInfo = chunkBufferAddr_;
    pc.indirectCmd = indirectBufferAddr_;
    pc.chunkCount = visibleChunks_;

    vkCmdPushConstants(cmd, pipelineLayout_,
                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);
}
```

### Шейдер с BDA

```glsl
#version 460
#extension GL_EXT_buffer_reference2 : require

layout(buffer_reference, buffer_reference_align = 4) buffer VoxelData {
    uint packed_data;
};

layout(buffer_reference, buffer_reference_align = 16) buffer ChunkInfo {
    vec3 position;
    uint lod;
    uint voxel_count;
};

layout(push_constant) uniform PushConstants {
    VoxelData voxelData;
    ChunkInfo chunkInfo;
    uint chunkCount;
} pc;

void main() {
    // Прямой доступ без дескрипторов!
    uint voxel = pc.voxelData[gl_VertexIndex].packed_data;
    vec3 chunkPos = pc.chunkInfo[gl_InstanceIndex].position;
}
```

---

## Subgroup Operations для вокселей

### Frustum Culling в один Pass

```glsl
layout(local_size_x = 64) in;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= chunkCount) return;

    // Проверка frustum для одного чанка
    bool visible = isChunkVisible(idx);

    // Subgroup ballot — все 64 результата в одной операции!
    uvec4 ballot = subgroupBallot(visible);

    // Только первый invocation записывает
    if (subgroupElect()) {
        uint visibleCount = subgroupBallotBitCount(ballot);

        // Запись результатов одним burst
        for (uint i = 0; i < 64; i++) {
            uint chunkIdx = gl_WorkGroupID.x * 64 + i;
            if (chunkIdx < chunkCount) {
                chunks[chunkIdx].visible = (ballot[i / 32] >> (i % 32)) & 1;
            }
        }
    }
}
```

### Mesh Generation в Shared Memory

```glsl
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

shared uint sharedVoxels[512];  // 8³ = 512

void main() {
    uvec3 localPos = gl_LocalInvocationID;
    uint localIdx = localPos.x + localPos.y * 8 + localPos.z * 64;

    // Загрузка в shared memory
    sharedVoxels[localIdx] = input.data[globalIdx];

    subgroupBarrier();

    // Проверка граней в shared memory (быстро!)
    uint voxel = sharedVoxels[localIdx];
    if (voxel == 0) return;

    uint faceCount = 0;

    // Проверка соседей
    if (localPos.x == 0 || sharedVoxels[localIdx - 1] == 0) faceCount++;
    if (localPos.x == 7 || sharedVoxels[localIdx + 1] == 0) faceCount++;
    // ...

    // Subgroup reduction для подсчёта вершин
    uint totalVertices = subgroupAdd(faceCount * 4);

    // Выделение места
    uint vertexOffset;
    if (gl_SubgroupInvocationID == 0) {
        vertexOffset = atomicAdd(output.vertexCount, totalVertices);
    }
    vertexOffset = subgroupShuffle(vertexOffset, 0);
}
```

### Subgroup Shuffle для LOD

```glsl
void selectLOD() {
    float distance = calculateDistance(chunkPosition);
    uint desiredLOD = uint(log2(distance / lodBaseDistance));
    desiredLOD = clamp(desiredLOD, 0, maxLOD);

    // Subgroup min/max
    uint minLOD = subgroupMin(desiredLOD);
    uint maxLOD = subgroupMax(desiredLOD);

    if (minLOD == maxLOD) {
        // Все чанки в subgroup имеют одинаковый LOD — оптимизация!
        processUniformLOD(minLOD);
    } else {
        // Разные LOD — shuffle для обмена
        for (uint lod = minLOD; lod <= maxLOD; lod++) {
            uvec4 ballot = subgroupBallot(desiredLOD == lod);
            uint count = subgroupBallotBitCount(ballot);

            if (count > 0) {
                processLODGroup(lod, ballot);
            }
        }
    }
}
```

---

## Memory Aliasing

### Проблема

SVO требует много памяти: ~16 GB для 2048³ вокселей при полном SVO.

### Решение

Одна память для разных целей в разное время:

```cpp
struct AliasedVoxelMemory {
    VkBuffer buffer;
    VmaAllocation allocation;

    // Вариант 1: SVO nodes
    struct SVONode {
        uint32_t children[8];
        uint32_t brickPtr;
        uint32_t flags;
    };

    // Вариант 2: Dense voxel grid
    // uint32_t voxels[512][512][512];

    // Вариант 3: Render textures
    VkImageView colorTarget;
    VkImageView depthTarget;

    void init(VkDevice device, VmaAllocator allocator, VkDeviceSize size) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.flags = VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT;

        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                        &buffer, &allocation, nullptr);
    }
};
```

### Aliasing Image + Buffer

```cpp
struct AliasedImageBuffer {
    VkDeviceMemory memory;
    VkBuffer svoBuffer;
    VkImage renderTarget;

    void init(VkDevice device, VkPhysicalDevice physDevice,
              VkDeviceSize bufferSize, VkExtent2D imageSize) {
        // 1. Аллокация памяти под оба ресурса
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = bufferSize + imageSizeBytes;

        vkAllocateMemory(device, &allocInfo, nullptr, &memory);

        // 2. Создание буфера
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        vkCreateBuffer(device, &bufferInfo, nullptr, &svoBuffer);
        vkBindBufferMemory(device, svoBuffer, memory, 0);

        // 3. Создание image с offset!
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        imageInfo.extent = {imageSize.width, imageSize.height, 1};
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_STORAGE_BIT;
        imageInfo.flags = VK_IMAGE_CREATE_ALIAS_BIT;

        vkCreateImage(device, &imageInfo, nullptr, &renderTarget);
        vkBindImageMemory(device, renderTarget, memory, bufferSize);
    }
};
```

### Transition между использованиями

```cpp
void transitionToSVOBuild(VkCommandBuffer cmd, VkImage image) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    vkCmdPipelineBarrier(cmd, ...);
}

void transitionToRenderTarget(VkCommandBuffer cmd, VkImage image) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, ...);
}
```

---

## Zero-Descriptor Rendering

### Полная архитектура без дескрипторов

```cpp
struct FrameData {
    // GPU addresses (24 байта)
    VkDeviceAddress voxelData;
    VkDeviceAddress chunkData;
    VkDeviceAddress cameraData;

    // Frame info (16 байт)
    uint32_t frameIndex;
    uint32_t chunkCount;
    uint32_t screenWidth;
    uint32_t screenHeight;

    // Timing (8 байт)
    float time;
    float deltaTime;

    // Render params (16 байт)
    glm::vec4 fogParams;

    // LOD params (16 байт)
    float lodDistances[4];
};  // Итого: 80 байт — влезает в push constants!

void render(VkCommandBuffer cmd) {
    FrameData pc = {};
    pc.voxelData = voxelBufferAddr_;
    pc.chunkData = chunkBufferAddr_;
    pc.cameraData = cameraBufferAddr_;
    // ...

    vkCmdPushConstants(cmd, pipelineLayout_,
                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    // Один dispatch для всего рендеринга
    vkCmdDrawMeshTasksEXT(cmd, taskCount, 1, 1);
}
```

### Шейдер без дескрипторов

```glsl
#version 460
#extension GL_EXT_buffer_reference2 : require

layout(buffer_reference, buffer_reference_align = 4) buffer VoxelData {
    uint data[];
};

layout(buffer_reference, buffer_reference_align = 16) buffer ChunkData {
    vec3 position;
    uint lod;
};

layout(buffer_reference, buffer_reference_align = 16) buffer CameraData {
    mat4 viewProj;
    vec3 position;
};

layout(push_constant) uniform FrameData {
    VoxelData voxelData;
    ChunkData chunkData;
    CameraData cameraData;
    uint frameIndex;
    uint chunkCount;
    float time;
    float deltaTime;
    vec4 fogParams;
    float lodDistances[4];
} pc;

void main() {
    // Прямой доступ ко всем данным
    mat4 viewProj = pc.cameraData.viewProj;
    vec3 chunkPos = pc.chunkData[gl_InstanceIndex].position;
    uint voxel = pc.voxelData[calculateVoxelIndex()];
}
```

---

## Сравнение производительности

| Техника               | CPU Time | GPU Time | Memory   | Сложность |
|-----------------------|----------|----------|----------|-----------|
| Traditional Pipelines | 10-50ms  | 15ms     | Baseline | 🟢        |
| Shader Objects        | 0.1-1ms  | 15ms     | Baseline | 🟡        |
| BDA + Push Constants  | 0.1ms    | 12ms     | -10%     | 🟡        |
| Subgroup Culling      | 0.1ms    | 8ms      | -5%      | 🔴        |
| Memory Aliasing       | 0.1ms    | 8ms      | -50%     | 🔴        |
| Zero-Descriptor       | 0.05ms   | 6ms      | -30%     | 🔴        |

---

## Чеклист внедрения

- [ ] Проверка поддержки `VK_EXT_shader_object`
- [ ] Проверка поддержки `VK_KHR_buffer_device_address`
- [ ] Проверка поддержки `VK_KHR_shader_subgroup_*`
- [ ] Включение BDA features при создании device
- [ ] Создание всех буферов с `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`
- [ ] Refactor шейдеров на `GL_EXT_buffer_reference`
- [ ] Замена descriptor bindings на push constants
- [ ] Тестирование на NVIDIA/AMD/Intel
- [ ] Профилирование до/после

---

## Ключевые принципы

1. **Shader Objects** — упрощают hot reload и state management
2. **BDA** — устраняет overhead дескрипторов
3. **Subgroups** — SIMD-оптимизации внутри warp/wavefront
4. **Memory Aliasing** — экономия памяти через переиспользование
5. **Zero-Descriptor** — минимальный CPU overhead
