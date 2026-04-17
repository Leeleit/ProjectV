## Интеграция GLM

<!-- anchor: 03_integration -->

🟡 **Уровень 2: Средний**

Подключение и настройка GLM: CMake, заголовки, макросы конфигурации.

## CMake

### Добавление как подпроекта

```cmake
add_subdirectory(external/glm)
target_link_libraries(YourApp PRIVATE glm::glm)
```

GLM — header-only библиотека: линкуется только интерфейс (include-пути).

### CMake-опции

| Опция                        | По умолчанию | Описание                                            |
|------------------------------|--------------|-----------------------------------------------------|
| `GLM_BUILD_LIBRARY`          | `ON`         | Сборка библиотеки (для header-only обычно не нужна) |
| `GLM_BUILD_TESTS`            | `OFF`        | Сборка тестов GLM                                   |
| `GLM_ENABLE_CXX_17`          | `OFF`        | Принудительно C++17                                 |
| `GLM_ENABLE_SIMD_SSE2`       | `OFF`        | SIMD-оптимизации (требуют `GLM_FORCE_INTRINSICS`)   |
| `GLM_DISABLE_AUTO_DETECTION` | `OFF`        | Отключить автоопределение платформы                 |

---

## Способы подключения заголовков

### Глобальные заголовки (удобно, но медленная компиляция)

```cpp
#include <glm/glm.hpp>  // Ядро GLM (vec2/3/4, mat2/3/4, базовые функции)
#include <glm/ext.hpp>  // Все расширения (translate, rotate, perspective и др.)
```

### Разделённые заголовки (баланс)

```cpp
// Ядро GLSL
#include <glm/vec2.hpp>               // vec2
#include <glm/vec3.hpp>               // vec3
#include <glm/mat4x4.hpp>             // mat4

// Расширения
#include <glm/ext/matrix_transform.hpp>     // translate, rotate, scale
#include <glm/ext/matrix_clip_space.hpp>    // perspective
```

### Гранулярные заголовки (максимальная скорость компиляции)

```cpp
#include <glm/ext/vector_float3.hpp>          // vec3
#include <glm/ext/matrix_float4x4.hpp>        // mat4
#include <glm/ext/matrix_transform.hpp>       // translate, rotate
```

---

## Модули GLM

### Ядро (GLSL совместимость)

| Заголовок                           | Содержание                               |
|-------------------------------------|------------------------------------------|
| `glm/vec2.hpp`...`glm/vec4.hpp`     | Векторы (float, double, int, uint, bool) |
| `glm/mat2x2.hpp`...`glm/mat4x4.hpp` | Матрицы всех размеров                    |
| `glm/trigonometric.hpp`             | `radians`, `cos`, `sin`, `asin` и др.    |
| `glm/exponential.hpp`               | `pow`, `log`, `exp2`, `sqrt`             |
| `glm/geometric.hpp`                 | `dot`, `cross`, `normalize`, `reflect`   |
| `glm/matrix.hpp`                    | `transpose`, `inverse`, `determinant`    |
| `glm/common.hpp`                    | `min`, `max`, `mix`, `abs`, `sign`       |
| `glm/packing.hpp`                   | `packUnorm4x8`, `unpackHalf2x16`         |
| `glm/integer.hpp`                   | `findMSB`, `bitfieldExtract`             |

### Стабильные расширения (GTC — рекомендованные)

| Заголовок                      | Содержание                                              |
|--------------------------------|---------------------------------------------------------|
| `glm/gtc/matrix_transform.hpp` | `translate`, `rotate`, `scale`, `perspective`, `lookAt` |
| `glm/gtc/type_ptr.hpp`         | `value_ptr`, `make_mat4`, `make_vec3`                   |
| `glm/gtc/quaternion.hpp`       | `quat`, `slerp`, `mat4_cast`, `eulerAngles`             |
| `glm/gtc/constants.hpp`        | `pi`, `epsilon`, `infinity`                             |
| `glm/gtc/random.hpp`           | `linearRand`, `gaussRand`, `diskRand`                   |

### Экспериментальные расширения (GTX — нестабильные)

Требуют макрос `GLM_ENABLE_EXPERIMENTAL`:

```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>    // Дополнительные функции для кватернионов
#include <glm/gtx/euler_angles.hpp>  // Преобразования углов Эйлера
```

---

## Макросы конфигурации

Определять **до** первого `#include` GLM.

### Системы координат и углы

| Макрос                        | Описание                             | Рекомендация для Vulkan |
|-------------------------------|--------------------------------------|-------------------------|
| `GLM_FORCE_RADIANS`           | Использовать радианы для всех углов  | Рекомендуется           |
| `GLM_FORCE_DEPTH_ZERO_TO_ONE` | NDC по оси Z в [0, 1] (а не [-1, 1]) | Обязательно для Vulkan  |
| `GLM_FORCE_LEFT_HANDED`       | Левая система координат              | Для DirectX             |

### Отладка и сообщения

| Макрос                      | Описание                                 |
|-----------------------------|------------------------------------------|
| `GLM_FORCE_MESSAGES`        | Выводить конфигурацию в лог сборки       |
| `GLM_FORCE_SILENT_WARNINGS` | Отключить warnings о language extensions |

### Контроль точности

| Макрос                              | Тип     | Описание                        |
|-------------------------------------|---------|---------------------------------|
| `GLM_FORCE_PRECISION_HIGHP_FLOAT`   | `float` | Высокая точность (по умолчанию) |
| `GLM_FORCE_PRECISION_MEDIUMP_FLOAT` | `float` | Средняя точность                |
| `GLM_FORCE_PRECISION_LOWP_FLOAT`    | `float` | Низкая точность                 |

### Другие важные макросы

| Макрос                           | Описание                                                   |
|----------------------------------|------------------------------------------------------------|
| `GLM_FORCE_EXPLICIT_CTOR`        | Запретить неявные преобразования типов                     |
| `GLM_FORCE_SIZE_T_LENGTH`        | `.length()` возвращает `size_t` вместо `int`               |
| `GLM_FORCE_UNRESTRICTED_GENTYPE` | Разрешить смешанные типы в функциях                        |
| `GLM_FORCE_SWIZZLE`              | Включить swizzle-операторы (сильно замедляет компиляцию)   |
| `GLM_FORCE_XYZW_ONLY`            | Только компоненты x, y, z, w (без r, g, b, a и s, t, p, q) |

---

## Пример конфигурации для Vulkan

```cpp
// Конфигурация GLM для Vulkan (до включения заголовков)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_MESSAGES  // Опционально: вывод конфигурации при сборке

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
```

---

## Передача данных в GPU

### value_ptr для uniform-буферов

```cpp
#include <glm/gtc/type_ptr.hpp>

glm::mat4 model = glm::mat4(1.0f);
glm::vec3 position = glm::vec3(1.0f, 2.0f, 3.0f);

// Копирование в отображённую память
memcpy(mappedData, glm::value_ptr(model), sizeof(glm::mat4));
memcpy(mappedData + sizeof(glm::mat4), glm::value_ptr(position), sizeof(glm::vec3));
```

### make_* для чтения из GPU

```cpp
float* gpuData = ...;  // Указатель на отображённую память

glm::mat4 model = glm::make_mat4(gpuData);
glm::vec3 position = glm::make_vec3(gpuData + 16);  // Пропустить mat4 (16 floats)
```

---

## Выравнивание для std140

Стандарт `std140` имеет строгие правила выравнивания:

- `vec3` выравнивается как `vec4` (16 байт)
- Матрицы выравниваются по столбцам (каждый столбец как `vec4`)

```cpp
struct UBO {
    alignas(16) glm::mat4 mvp;       // 64 байта, выравнивание 16
    alignas(16) glm::vec3 cameraPos; // 16 байт (не 12!)
    alignas(4)  float time;          // 4 байта
};
```

**Альтернатива:** Используйте `glm::vec4` вместо `glm::vec3` для избежания проблем.

---

## Интеграция GLM с ProjectV

<!-- anchor: 09_projectv-integration -->

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

---

## Паттерны GLM для воксельного движка

<!-- anchor: 10_projectv-patterns -->

🔴 **Уровень 3: Продвинутый**

Специализированные паттерны использования GLM для воксельного рендеринга в ProjectV.

## Математика для Greedy Meshing

Greedy Meshing — алгоритм объединения соседних вокселей в большие плоскости для уменьшения количества треугольников.

### Структура плоскости

```cpp
struct VoxelFace {
    glm::ivec3 start;      // Начальная позиция плоскости
    glm::uvec3 size;       // Размеры плоскости (width, height, depth = 1)
    uint32_t materialId;   // ID материала
    glm::vec3 normal;      // Нормаль плоскости
};
```

### Поиск плоскостей

```cpp
std::vector<VoxelFace> findGreedyMeshingPlanes(
    const std::vector<uint32_t>& materials,
    const std::vector<bool>& opacity,
    uint32_t chunkSize
) {
    std::vector<VoxelFace> faces;

    // Поиск в направлении X
    for (uint32_t z = 0; z < chunkSize; ++z) {
        for (uint32_t y = 0; y < chunkSize; ++y) {
            uint32_t startX = 0;
            uint32_t currentMaterial = 0;
            bool inPlane = false;

            for (uint32_t x = 0; x < chunkSize; ++x) {
                uint32_t idx = x + y * chunkSize + z * chunkSize * chunkSize;
                uint32_t material = materials[idx];
                bool opaque = opacity[idx];

                if (opaque && (!inPlane || material == currentMaterial)) {
                    if (!inPlane) {
                        startX = x;
                        currentMaterial = material;
                        inPlane = true;
                    }
                } else {
                    if (inPlane) {
                        faces.push_back({
                            {static_cast<int>(startX), static_cast<int>(y), static_cast<int>(z)},
                            {x - startX, 1, 1},
                            currentMaterial,
                            {1, 0, 0}
                        });
                        inPlane = false;
                    }
                }
            }

            if (inPlane) {
                faces.push_back({
                    {static_cast<int>(startX), static_cast<int>(y), static_cast<int>(z)},
                    {chunkSize - startX, 1, 1},
                    currentMaterial,
                    {1, 0, 0}
                });
            }
        }
    }

    return faces;
}
```

### Генерация вершин для плоскости

```cpp
std::vector<glm::vec3> generateFaceVertices(
    const VoxelFace& face,
    const glm::mat4& chunkTransform
) {
    std::vector<glm::vec3> vertices;

    // Базовые вершины куба (0..1)
    const std::array<glm::vec3, 8> cubeVertices = {{
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
    }};

    // Индексы для грани (зависит от нормали)
    std::array<uint32_t, 4> faceIndices;
    if (face.normal == glm::vec3(1, 0, 0)) {       // +X
        faceIndices = {1, 5, 6, 2};
    } else if (face.normal == glm::vec3(-1, 0, 0)) { // -X
        faceIndices = {0, 3, 7, 4};
    } else if (face.normal == glm::vec3(0, 1, 0)) {   // +Y
        faceIndices = {2, 6, 7, 3};
    } else if (face.normal == glm::vec3(0, -1, 0)) {  // -Y
        faceIndices = {0, 4, 5, 1};
    } else if (face.normal == glm::vec3(0, 0, 1)) {    // +Z
        faceIndices = {4, 7, 6, 5};
    } else {                                           // -Z
        faceIndices = {0, 1, 2, 3};
    }

    // Трансформация
    glm::mat4 faceTransform = chunkTransform;
    faceTransform = glm::translate(faceTransform, glm::vec3(face.start));
    faceTransform = glm::scale(faceTransform, glm::vec3(face.size));

    for (uint32_t idx : faceIndices) {
        glm::vec4 vertex = faceTransform * glm::vec4(cubeVertices[idx], 1.0f);
        vertices.push_back(glm::vec3(vertex));
    }

    return vertices;
}
```

---

## LOD-система для чанков

### Вычисление уровня LOD

```cpp
class VoxelLODSystem {
public:
    static constexpr std::array<float, 5> LOD_DISTANCES = {
        50.0f,   // LOD 0: 0-50 единиц
        100.0f,  // LOD 1: 50-100 единиц
        200.0f,  // LOD 2: 100-200 единиц
        400.0f,  // LOD 3: 200-400 единиц
        800.0f   // LOD 4: 400+ единиц
    };

    static uint32_t calculateLODLevel(glm::vec3 cameraPos, glm::vec3 chunkCenter) {
        float distance = glm::distance(cameraPos, chunkCenter);

        for (uint32_t lod = 0; lod < LOD_DISTANCES.size(); ++lod) {
            if (distance < LOD_DISTANCES[lod]) {
                return lod;
            }
        }

        return static_cast<uint32_t>(LOD_DISTANCES.size() - 1);
    }

    static float calculateMorphFactor(
        glm::vec3 cameraPos,
        glm::vec3 chunkCenter,
        uint32_t currentLOD
    ) {
        float distance = glm::distance(cameraPos, chunkCenter);
        float lowerBound = currentLOD > 0 ? LOD_DISTANCES[currentLOD - 1] : 0.0f;
        float upperBound = LOD_DISTANCES[currentLOD];

        float t = (distance - lowerBound) / (upperBound - lowerBound);
        return glm::clamp(t, 0.0f, 1.0f);
    }
};
```

### Упрощение геометрии для LOD

```cpp
std::vector<uint32_t> simplifyVoxelData(
    const std::vector<uint32_t>& originalMaterials,
    uint32_t chunkSize,
    uint32_t lodLevel
) {
    uint32_t factor = 1 << lodLevel;  // 1, 2, 4, 8, 16...
    uint32_t newSize = chunkSize / factor;

    std::vector<uint32_t> simplified(newSize * newSize * newSize, 0);

    for (uint32_t z = 0; z < newSize; ++z) {
        for (uint32_t y = 0; y < newSize; ++y) {
            for (uint32_t x = 0; x < newSize; ++x) {
                uint32_t materialSum = 0;
                uint32_t voxelCount = 0;

                // Усреднение блока вокселей
                for (uint32_t dz = 0; dz < factor; ++dz) {
                    for (uint32_t dy = 0; dy < factor; ++dy) {
                        for (uint32_t dx = 0; dx < factor; ++dx) {
                            uint32_t srcX = x * factor + dx;
                            uint32_t srcY = y * factor + dy;
                            uint32_t srcZ = z * factor + dz;

                            if (srcX < chunkSize && srcY < chunkSize && srcZ < chunkSize) {
                                uint32_t idx = srcX + srcY * chunkSize + srcZ * chunkSize * chunkSize;
                                if (originalMaterials[idx] != 0) {
                                    materialSum += originalMaterials[idx];
                                    voxelCount++;
                                }
                            }
                        }
                    }
                }

                uint32_t dstIdx = x + y * newSize + z * newSize * newSize;
                if (voxelCount > 0) {
                    simplified[dstIdx] = materialSum / voxelCount;
                }
            }
        }
    }

    return simplified;
}
```

---

## SIMD для массовых трансформаций

### Пакетная трансформация позиций

```cpp
#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_SSE2
#include <glm/glm.hpp>

#include <immintrin.h>

void transformPositionsSIMD(
    const std::vector<glm::vec3>& positions,
    const glm::mat4& transform,
    std::vector<glm::vec3>& result
) {
    size_t count = positions.size();
    result.resize(count);

    // Извлекаем строки матрицы
    __m128 row0 = _mm_loadu_ps(&transform[0][0]);
    __m128 row1 = _mm_loadu_ps(&transform[1][0]);
    __m128 row2 = _mm_loadu_ps(&transform[2][0]);
    __m128 row3 = _mm_loadu_ps(&transform[3][0]);

    for (size_t i = 0; i < count; ++i) {
        __m128 pos = _mm_set_ps(1.0f, positions[i].z, positions[i].y, positions[i].x);

        __m128 tx = _mm_mul_ps(row0, _mm_shuffle_ps(pos, pos, 0x00));
        __m128 ty = _mm_mul_ps(row1, _mm_shuffle_ps(pos, pos, 0x55));
        __m128 tz = _mm_mul_ps(row2, _mm_shuffle_ps(pos, pos, 0xAA));

        __m128 transformed = _mm_add_ps(_mm_add_ps(tx, ty), _mm_add_ps(tz, row3));

        alignas(16) float resultArr[4];
        _mm_store_ps(resultArr, transformed);

        result[i] = glm::vec3(resultArr[0], resultArr[1], resultArr[2]);
    }
}
```

### Быстрое вычисление расстояний

```cpp
void computeDistancesSIMD(
    const std::vector<glm::vec3>& positions,
    glm::vec3 reference,
    std::vector<float>& distances
) {
    size_t count = positions.size();
    distances.resize(count);

    __m128 refX = _mm_set1_ps(reference.x);
    __m128 refY = _mm_set1_ps(reference.y);
    __m128 refZ = _mm_set1_ps(reference.z);

    for (size_t i = 0; i < count; i += 4) {
        size_t remaining = std::min(size_t(4), count - i);

        // Загрузка позиций
        alignas(16) float posX[4], posY[4], posZ[4];
        for (size_t j = 0; j < remaining; ++j) {
            posX[j] = positions[i + j].x;
            posY[j] = positions[i + j].y;
            posZ[j] = positions[i + j].z;
        }

        __m128 x = _mm_load_ps(posX);
        __m128 y = _mm_load_ps(posY);
        __m128 z = _mm_load_ps(posZ);

        // diff = pos - ref
        __m128 dx = _mm_sub_ps(x, refX);
        __m128 dy = _mm_sub_ps(y, refY);
        __m128 dz = _mm_sub_ps(z, refZ);

        // distance2 = dx*dx + dy*dy + dz*dz
        __m128 dist2 = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(dx, dx), _mm_mul_ps(dy, dy)),
            _mm_mul_ps(dz, dz)
        );

        // distance = sqrt(distance2)
        __m128 dist = _mm_sqrt_ps(dist2);

        alignas(16) float resultArr[4];
        _mm_store_ps(resultArr, dist);

        for (size_t j = 0; j < remaining; ++j) {
            distances[i + j] = resultArr[j];
        }
    }
}
```

---

## Данные для Compute Shaders

### Структура для GPU

```cpp
struct GPUVoxelChunkData {
    glm::mat4 worldTransform;  // 64 байта
    glm::vec4 chunkInfo;       // 16 байт: xyz = grid pos, w = lod level
    glm::uvec4 dataInfo;       // 16 байт: x = voxel_count, y = flags

    // Конвертация для буфера
    std::array<float, 24> toFloatArray() const {
        std::array<float, 24> result;

        const float* matrixData = glm::value_ptr(worldTransform);
        std::copy(matrixData, matrixData + 16, result.begin());

        result[16] = chunkInfo.x;
        result[17] = chunkInfo.y;
        result[18] = chunkInfo.z;
        result[19] = chunkInfo.w;

        result[20] = static_cast<float>(dataInfo.x);
        result[21] = static_cast<float>(dataInfo.y);
        result[22] = static_cast<float>(dataInfo.z);
        result[23] = static_cast<float>(dataInfo.w);

        return result;
    }
};
```

### Подготовка данных для GPU

```cpp
std::vector<GPUVoxelChunkData> prepareGPUChunkData(
    const std::vector<glm::ivec3>& chunkPositions,
    const std::vector<uint32_t>& lodLevels,
    const std::vector<uint32_t>& voxelCounts
) {
    std::vector<GPUVoxelChunkData> gpuData;
    gpuData.reserve(chunkPositions.size());

    for (size_t i = 0; i < chunkPositions.size(); ++i) {
        GPUVoxelChunkData data;
        data.worldTransform = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(chunkPositions[i]) * 16.0f
        );
        data.chunkInfo = glm::vec4(
            static_cast<float>(chunkPositions[i].x),
            static_cast<float>(chunkPositions[i].y),
            static_cast<float>(chunkPositions[i].z),
            static_cast<float>(lodLevels[i])
        );
        data.dataInfo = glm::uvec4(voxelCounts[i], 0, 0, 0);

        gpuData.push_back(data);
    }

    return gpuData;
}
```

---

## SoA для воксельных данных

### Оптимизированная структура чанка

```cpp
struct VoxelChunkSoA {
    // SoA для максимальной производительности
    alignas(64) uint8_t materials[16 * 16 * 16];
    alignas(64) uint8_t opacity[16 * 16 * 16];
    alignas(64) uint8_t lightLevels[16 * 16 * 16];

    // Метаданные
    glm::ivec3 position;
    uint32_t lodLevel;
    bool isDirty;
    bool isVisible;

    static constexpr uint32_t SIZE = 16;
    static constexpr uint32_t VOLUME = SIZE * SIZE * SIZE;

    // Индексация
    static uint32_t index(uint32_t x, uint32_t y, uint32_t z) {
        return x + y * SIZE + z * SIZE * SIZE;
    }

    // Доступ к материалам
    uint8_t getMaterial(uint32_t x, uint32_t y, uint32_t z) const {
        return materials[index(x, y, z)];
    }

    void setMaterial(uint32_t x, uint32_t y, uint32_t z, uint8_t value) {
        materials[index(x, y, z)] = value;
        isDirty = true;
    }
};
```

### Массовая обработка

```cpp
void processChunkBatch(
    const std::vector<VoxelChunkSoA>& chunks,
    std::vector<glm::mat4>& transforms,
    glm::vec3 cameraPos
) {
    // Фильтрация видимых чанков
    std::vector<size_t> visibleIndices;
    for (size_t i = 0; i < chunks.size(); ++i) {
        glm::vec3 chunkCenter = glm::vec3(chunks[i].position) * 16.0f + 8.0f;
        float dist = glm::distance(cameraPos, chunkCenter);
        if (dist < 400.0f) {  // Render distance
            visibleIndices.push_back(i);
        }
    }

    // Обновление матриц только для видимых
    transforms.resize(visibleIndices.size());
    for (size_t i = 0; i < visibleIndices.size(); ++i) {
        const auto& chunk = chunks[visibleIndices[i]];
        transforms[i] = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(chunk.position) * 16.0f
        );
    }
}
```

---

## Сводка паттернов

| Паттерн              | Применение           | Выигрыш                            |
|----------------------|----------------------|------------------------------------|
| Greedy Meshing       | Генерация мешей      | Уменьшение треугольников в 10-100x |
| LOD System           | Дальность прорисовки | Уменьшение геометрии в 2-16x       |
| SIMD трансформации   | Массовые вычисления  | Ускорение в 2-4x                   |
| SoA данные           | Кэш-локальность      | Ускорение обхода в 2-3x            |
| Batch processing     | Отсечение невидимых  | Уменьшение работы GPU              |
| GPU data preparation | Compute shaders      | Ускорение передачи данных          |