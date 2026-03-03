# Паттерны GLM для воксельного движка

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
