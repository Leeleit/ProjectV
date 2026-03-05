# Интеграция GLM с ProjectV

🔴 **Уровень 3: Продвинутый**

Специфика использования GLM в ProjectV: Vulkan, VMA, инстансинг.

## Конфигурация для Vulkan

```cpp
// Конфигурация GLM для ProjectV (Vulkan)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // Z в [0, 1] для Vulkan
#define GLM_FORCE_INTRINSICS         // SIMD оптимизации
#define GLM_FORCE_SSE2               // Минимум SSE2

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
```

---

## Uniform-буферы для Vulkan

### Структура UBO с правильным выравниванием

```cpp
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPos;  // 16 байт (не 12!)
    alignas(4)  float time;
};
```

### Обновление UBO

```cpp
void updateUniformBuffer(VmaAllocation allocation, void* mappedData,
                        const glm::mat4& model, const glm::mat4& view,
                        const glm::mat4& proj, const glm::vec3& cameraPos) {
    UniformBufferObject ubo;
    ubo.model = model;
    ubo.view = view;
    ubo.proj = proj;
    ubo.cameraPos = cameraPos;
    ubo.time = static_cast<float>(glfwGetTime());

    memcpy(mappedData, &ubo, sizeof(ubo));
}
```

---

## Push Constants

Для данных, которые меняются часто (например, трансформация объекта):

```cpp
struct PushConstants {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec4 color;
};

// Запись push constants
PushConstants pc;
pc.model = glm::translate(glm::mat4(1.0f), position);
pc.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

vkCmdPushConstants(
    commandBuffer,
    pipelineLayout,
    VK_SHADER_STAGE_VERTEX_BIT,
    0,
    sizeof(PushConstants),
    &pc
);
```

---

## Инстансинг

### Массив матриц для инстансинга

```cpp
struct InstanceData {
    std::vector<glm::mat4> modelMatrices;
    std::vector<glm::vec4> colors;
};

// Создание данных для сетки чанков
InstanceData createChunkInstances(int gridSize, float chunkSpacing) {
    InstanceData data;
    size_t count = gridSize * gridSize;
    data.modelMatrices.reserve(count);
    data.colors.reserve(count);

    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            glm::mat4 model = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(x * chunkSpacing, 0.0f, z * chunkSpacing)
            );
            data.modelMatrices.push_back(model);
            data.colors.push_back(glm::vec4(1.0f));
        }
    }

    return data;
}
```

### Загрузка в VMA-буфер

```cpp
void uploadInstanceBuffer(
    VmaAllocator allocator,
    const InstanceData& data,
    VkBuffer& buffer,
    VmaAllocation& allocation
) {
    VkDeviceSize matrixSize = data.modelMatrices.size() * sizeof(glm::mat4);
    VkDeviceSize colorSize = data.colors.size() * sizeof(glm::vec4);
    VkDeviceSize totalSize = matrixSize + colorSize;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = totalSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

    // Staging buffer для копирования...
    // memcpy(stagingData, glm::value_ptr(data.modelMatrices[0]), matrixSize);
}
```

---

## Вычисление MVP для кадра

```cpp
class VulkanRenderer {
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
    glm::mat4 viewProjMatrix;
    bool dirty = true;

public:
    void setCamera(const glm::vec3& pos, const glm::vec3& target) {
        viewMatrix = glm::lookAt(pos, target, glm::vec3(0, 1, 0));
        dirty = true;
    }

    void setProjection(float fov, float aspect, float near, float far) {
        projMatrix = glm::perspective(fov, aspect, near, far);
        dirty = true;
    }

    glm::mat4 getViewProj() {
        if (dirty) {
            viewProjMatrix = projMatrix * viewMatrix;
            dirty = false;
        }
        return viewProjMatrix;
    }

    glm::mat4 getMVP(const glm::mat4& model) {
        return getViewProj() * model;
    }
};
```

---

## Чанковая система координат

### Преобразование координат чанков

```cpp
struct VoxelCoordinateSystem {
    static constexpr uint32_t CHUNK_SIZE = 16;

    // Локальная позиция вокселя → мировая позиция
    static glm::vec3 localToWorld(glm::ivec3 chunkPos, glm::uvec3 localVoxel) {
        glm::vec3 chunkWorld = glm::vec3(chunkPos) * static_cast<float>(CHUNK_SIZE);
        return chunkWorld + glm::vec3(localVoxel);
    }

    // Мировая позиция → координаты чанка
    static glm::ivec3 worldToChunk(glm::vec3 worldPos) {
        return glm::floor(worldPos / static_cast<float>(CHUNK_SIZE));
    }

    // Индекс вокселя в линейном массиве
    static uint32_t voxelIndex(glm::uvec3 localPos) {
        return localPos.x + localPos.y * CHUNK_SIZE + localPos.z * CHUNK_SIZE * CHUNK_SIZE;
    }

    // Обратное преобразование
    static glm::uvec3 indexToVoxel(uint32_t index) {
        uint32_t z = index / (CHUNK_SIZE * CHUNK_SIZE);
        index %= (CHUNK_SIZE * CHUNK_SIZE);
        uint32_t y = index / CHUNK_SIZE;
        uint32_t x = index % CHUNK_SIZE;
        return {x, y, z};
    }
};
```

### Bounding Box для чанка

```cpp
struct ChunkBoundingBox {
    glm::vec3 min;
    glm::vec3 max;

    static ChunkBoundingBox compute(glm::ivec3 chunkPos, uint32_t chunkSize) {
        glm::vec3 base = glm::vec3(chunkPos) * static_cast<float>(chunkSize);
        return {
            base,
            base + glm::vec3(static_cast<float>(chunkSize))
        };
    }

    bool intersectsFrustum(const glm::mat4& vpMatrix) const {
        // Проверка 8 углов bounding box
        for (int i = 0; i < 8; ++i) {
            glm::vec3 corner(
                (i & 1) ? max.x : min.x,
                (i & 2) ? max.y : min.y,
                (i & 4) ? max.z : min.z
            );
            glm::vec4 clip = vpMatrix * glm::vec4(corner, 1.0f);
            if (clip.x >= -clip.w && clip.x <= clip.w &&
                clip.y >= -clip.w && clip.y <= clip.w &&
                clip.z >= 0.0f && clip.z <= clip.w) {
                return true;
            }
        }
        return false;
    }
};
```

---

## Интеграция с flecs ECS

### Компоненты с GLM типами

```cpp
#include <flecs.h>

struct Position { glm::vec3 value; };
struct Rotation { glm::quat value; };
struct Scale { glm::vec3 value; };
struct Transform { glm::mat4 matrix; };

// Система обновления матриц трансформации
void RegisterTransformSystem(flecs::world& ecs) {
    ecs.system<Transform, const Position, const Rotation, const Scale>("UpdateTransform")
        .each([](Transform& t, const Position& p, const Rotation& r, const Scale& s) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), p.value)
                            * glm::mat4_cast(r.value)
                            * glm::scale(glm::mat4(1.0f), s.value);
            t.matrix = model;
        });
}
```

---

## Профилирование с Tracy

```cpp
#include <tracy/Tracy.hpp>

void renderFrame(const std::vector<glm::mat4>& models) {
    ZoneScopedN("Render Frame");

    {
        ZoneScopedN("Update MVP matrices");
        glm::mat4 vp = camera.getViewProj();

        for (const auto& model : models) {
            glm::mat4 mvp = vp * model;
            // ...
        }
    }
}
```

---

## Сводка интеграции

| Элемент                       | Файл            | Назначение                    |
|-------------------------------|-----------------|-------------------------------|
| `GLM_FORCE_DEPTH_ZERO_TO_ONE` | Конфигурация    | Vulkan Z-диапазон             |
| `GLM_FORCE_INTRINSICS`        | Конфигурация    | SIMD оптимизации              |
| `alignas(16)`                 | Структуры UBO   | Выравнивание для GPU          |
| `glm::value_ptr()`            | Передача данных | Указатель для memcpy          |
| Push Constants                | Vulkan          | Часто меняющиеся данные       |
| Instance buffer               | VMA             | Массив матриц для инстансинга |
