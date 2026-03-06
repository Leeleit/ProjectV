# Интеграция glm в ProjectV

**🔴 Уровень 3: Продвинутый**

В ProjectV библиотека glm (версия **1.1.0**) используется не просто как математическая база, но как инструмент для
высокопроизводительных вычислений трансформаций чанков, инстансинга и физики.

## Оглавление

1. [Архитектура: DOD и ECS](#1-архитектура-dod-и-ecs)
2. [Оптимизация для воксельного движка](#2-оптимизация-для-воксельного-движка)
3. [Интеграция с VMA и инстансинг](#3-интеграция-с-vma-и-инстансинг)
4. [Примеры кода](#4-примеры-кода)
5. [Профилирование и отладка](#5-профилирование-и-отладка)

---

## 1. Архитектура: DOD и ECS

В контексте воксельного движка ProjectV с архитектурой Data-Oriented Design (DOD) и Entity Component System (ECS) важно
правильно использовать glm для достижения максимальной производительности.

### Организация данных для кэш-дружественного доступа

1. **Массивы структур (AoS) → Структуры массивов (SoA)**
  * **AoS (медленно):** `std::vector<glm::vec3> positions;`
  * **SoA (быстро):** `std::vector<float> pos_x, pos_y, pos_z;`
  * Для массовых операций SoA улучшает локальность данных.

2. **Пакетная обработка трансформаций**
  * Вычисление матриц для групп объектов за один проход.
  * Минимизация вызовов функций `translate`/`rotate`/`scale`.
  * Использование SIMD оптимизаций через `GLM_ENABLE_SIMD_*`.

### Интеграция с flecs ECS

1. **Компоненты glm типов в flecs:**
   ```cpp
   struct Position { glm::vec3 value; };
   struct Rotation { glm::quat value; };
   struct Scale { glm::vec3 value; };
   ```

2. **Массовые вычисления:** Используйте `flecs::query` для обработки групп компонентов.
3. **Кэширование матриц:** Предвычисляйте матрицы трансформации для статических объектов.

---

## 2. Оптимизация для воксельного движка

### Производительность операций (относительное время)

| Операция                 | Относительная стоимость | Оптимизация                             |
|--------------------------|-------------------------|-----------------------------------------|
| `normalize(v)`           | 1.0×                    | Используйте SIMD `GLM_ENABLE_SIMD_SSE2` |
| `dot(a, b)`              | 0.8×                    | Пакетная обработка через SoA            |
| `cross(a, b)`            | 1.2×                    | Избегайте в горячих путях               |
| `translate/rotate/scale` | 2.0×                    | Кэшируйте результаты                    |
| `perspective/lookAt`     | 3.0×                    | Вычисляйте только при изменении камеры  |

### Рекомендации

1. **Пакетная обработка:** Вычисление трансформаций для массивов вокселей.
2. **Минимизация аллокаций:** Предварительное резервирование памяти для векторов результатов.
3. **Повторное использование матриц:** Одна матрица трансформации для всего чанка, применяемая к локальным позициям
   вокселей.

---

## 3. Воксельная математика для ProjectV

**Специализированные математические функции и оптимизации для воксельного рендеринга.**

### Математика для чанков и мировых координат

Воксельные миры ProjectV используют иерархическую систему координат:

1. **Мир → Чанк → Воксель** трехуровневая трансформация
2. **Локальные координаты вокселя:** (0..15, 0..15, 0..15) в пределах чанка
3. **Координаты чанка:** целочисленные (chunk_x, chunk_y, chunk_z) в сетке мира
4. **Мировые координаты:** float, полученные через трансформацию

```cpp
// Конвертация между системами координат
struct VoxelCoordinateSystem {
    static constexpr uint32_t CHUNK_SIZE = 16;
    static constexpr float VOXEL_SIZE = 1.0f;

    // Локальная позиция вокселя → мировая позиция
    static glm::vec3 local_to_world(glm::ivec3 chunk_pos, glm::uvec3 local_voxel) {
        glm::vec3 chunk_world = glm::vec3(chunk_pos) * float(CHUNK_SIZE) * VOXEL_SIZE;
        glm::vec3 voxel_local = glm::vec3(local_voxel) * VOXEL_SIZE;
        return chunk_world + voxel_local;
    }

    // Мировая позиция → координаты чанка и локальная позиция вокселя
    static std::pair<glm::ivec3, glm::uvec3> world_to_local(glm::vec3 world_pos) {
        glm::vec3 chunk_f = world_pos / (float(CHUNK_SIZE) * VOXEL_SIZE);
        glm::ivec3 chunk_pos = glm::floor(chunk_f);

        glm::vec3 voxel_in_chunk = world_pos - glm::vec3(chunk_pos) * float(CHUNK_SIZE) * VOXEL_SIZE;
        glm::uvec3 local_voxel = glm::floor(voxel_in_chunk / VOXEL_SIZE);

        return {chunk_pos, local_voxel};
    }

    // Быстрое вычисление индекса вокселя в линейном массиве (DOD оптимизация)
    static uint32_t voxel_index(glm::uvec3 local_pos) {
        return local_pos.x + (local_pos.y * CHUNK_SIZE) + (local_pos.z * CHUNK_SIZE * CHUNK_SIZE);
    }

    // Обратное преобразование индекса → локальная позиция
    static glm::uvec3 index_to_voxel(uint32_t index) {
        uint32_t z = index / (CHUNK_SIZE * CHUNK_SIZE);
        index %= (CHUNK_SIZE * CHUNK_SIZE);
        uint32_t y = index / CHUNK_SIZE;
        uint32_t x = index % CHUNK_SIZE;
        return {x, y, z};
    }
};
```

### Математика для Greedy Meshing

Greedy Meshing — алгоритм объединения соседних вокселей в большие плоскости для уменьшения количества треугольников.

```cpp
// Структура для представления плоскости greedy meshing
struct VoxelFace {
    glm::ivec3 start;      // Начальная позиция плоскости
    glm::uvec3 size;       // Размеры плоскости (width, height, depth = 1)
    uint32_t material_id;  // ID материала
    glm::vec3 normal;      // Нормаль плоскости
};

// Функция для поиска плоскостей в чанке (упрощенная версия)
std::vector<VoxelFace> find_greedy_meshing_planes(const VoxelData& chunk_data) {
    std::vector<VoxelFace> faces;

    // Поиск в направлении X
    for (uint32_t z = 0; z < VoxelCoordinateSystem::CHUNK_SIZE; ++z) {
        for (uint32_t y = 0; y < VoxelCoordinateSystem::CHUNK_SIZE; ++y) {
            uint32_t start_x = 0;
            uint32_t current_material = 0;
            bool in_plane = false;

            for (uint32_t x = 0; x < VoxelCoordinateSystem::CHUNK_SIZE; ++x) {
                uint32_t idx = VoxelCoordinateSystem::voxel_index({x, y, z});
                uint32_t material = chunk_data.materials[idx];
                bool opaque = chunk_data.opacity[idx];

                if (opaque && (!in_plane || material == current_material)) {
                    if (!in_plane) {
                        start_x = x;
                        current_material = material;
                        in_plane = true;
                    }
                } else {
                    if (in_plane) {
                        // Завершаем плоскость
                        faces.push_back({
                            {start_x, y, z},
                            {x - start_x, 1, 1},
                            current_material,
                            {1, 0, 0}  // Нормаль по X
                        });
                        in_plane = false;
                    }
                }
            }

            // Завершаем последнюю плоскость в строке
            if (in_plane) {
                faces.push_back({
                    {start_x, y, z},
                    {VoxelCoordinateSystem::CHUNK_SIZE - start_x, 1, 1},
                    current_material,
                    {1, 0, 0}
                });
            }
        }
    }

    return faces;
}

// Генерация вершин для плоскости greedy meshing
std::vector<glm::vec3> generate_face_vertices(const VoxelFace& face, const glm::mat4& chunk_transform) {
    std::vector<glm::vec3> vertices;

    // Базовые вершины куба (0..1)
    const std::array<glm::vec3, 8> cube_vertices = {{
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
    }};

    // Индексы для грани (зависит от нормали)
    std::array<uint32_t, 4> face_indices;
    if (face.normal == glm::vec3(1, 0, 0)) {  // +X
        face_indices = {1, 5, 6, 2};
    } else if (face.normal == glm::vec3(-1, 0, 0)) {  // -X
        face_indices = {0, 3, 7, 4};
    } else if (face.normal == glm::vec3(0, 1, 0)) {  // +Y
        face_indices = {2, 6, 7, 3};
    } else if (face.normal == glm::vec3(0, -1, 0)) {  // -Y
        face_indices = {0, 4, 5, 1};
    } else if (face.normal == glm::vec3(0, 0, 1)) {  // +Z
        face_indices = {4, 7, 6, 5};
    } else {  // -Z
        face_indices = {0, 1, 2, 3};
    }

    // Масштабирование и смещение
    glm::mat4 face_transform = chunk_transform;
    face_transform = glm::translate(face_transform, glm::vec3(face.start));
    face_transform = glm::scale(face_transform, glm::vec3(face.size));

    // Генерация вершин
    for (uint32_t idx : face_indices) {
        glm::vec4 vertex = face_transform * glm::vec4(cube_vertices[idx], 1.0f);
        vertices.push_back(glm::vec3(vertex));
    }

    return vertices;
}
```

### LOD вычисления и морфинг

Уровни детализации (LOD) для воксельных миров требуют специализированной математики:

```cpp
// Класс для управления LOD воксельного мира
class VoxelLODSystem {
public:
    // Расстояния перехода между уровнями LOD
    static constexpr std::array<float, 5> LOD_DISTANCES = {
        0.0f,    // LOD 0: 0-50 единиц
        50.0f,   // LOD 1: 50-100 единиц
        100.0f,  // LOD 2: 100-200 единиц
        200.0f,  // LOD 3: 200-400 единиц
        400.0f   // LOD 4: 400+ единиц
    };

    // Вычисление уровня LOD на основе расстояния
    static uint32_t calculate_lod_level(glm::vec3 camera_pos, glm::vec3 chunk_center) {
        float distance = glm::distance(camera_pos, chunk_center);

        for (uint32_t lod = 0; lod < LOD_DISTANCES.size(); ++lod) {
            if (distance < LOD_DISTANCES[lod]) {
                return lod;
            }
        }

        return LOD_DISTANCES.size() - 1;  // Максимальный LOD
    }

    // Морфинг между уровнями LOD (плавный переход)
    static float calculate_lod_morph_factor(glm::vec3 camera_pos, glm::vec3 chunk_center,
                                           uint32_t current_lod, uint32_t target_lod) {
        float distance = glm::distance(camera_pos, chunk_center);

        if (current_lod == target_lod) {
            return 0.0f;  // Нет морфинга
        }

        // Вычисляем фактор морфинга (0..1)
        float lower_bound = LOD_DISTANCES[std::min(current_lod, target_lod)];
        float upper_bound = LOD_DISTANCES[std::max(current_lod, target_lod)];

        if (upper_bound - lower_bound < 0.001f) {
            return 1.0f;
        }

        float t = (distance - lower_bound) / (upper_bound - lower_bound);
        return glm::clamp(t, 0.0f, 1.0f);
    }

    // Упрощение геометрии для уровня LOD (октодерево сжатие)
    static std::vector<uint32_t> simplify_voxel_data(const VoxelData& original_data,
                                                    uint32_t lod_level) {
        uint32_t simplification_factor = 1 << lod_level;  // 1, 2, 4, 8, 16...
        uint32_t new_size = VoxelCoordinateSystem::CHUNK_SIZE / simplification_factor;

        std::vector<uint32_t> simplified_data(new_size * new_size * new_size, 0);

        // Простое усреднение (можно заменить на более сложные алгоритмы)
        for (uint32_t z = 0; z < new_size; ++z) {
            for (uint32_t y = 0; y < new_size; ++y) {
                for (uint32_t x = 0; x < new_size; ++x) {
                    // Усредняем блок вокселей
                    uint32_t material_sum = 0;
                    uint32_t voxel_count = 0;

                    for (uint32_t dz = 0; dz < simplification_factor; ++dz) {
                        for (uint32_t dy = 0; dy < simplification_factor; ++dy) {
                            for (uint32_t dx = 0; dx < simplification_factor; ++dx) {
                                uint32_t src_x = x * simplification_factor + dx;
                                uint32_t src_y = y * simplification_factor + dy;
                                uint32_t src_z = z * simplification_factor + dz;

                                if (src_x < VoxelCoordinateSystem::CHUNK_SIZE &&
                                    src_y < VoxelCoordinateSystem::CHUNK_SIZE &&
                                    src_z < VoxelCoordinateSystem::CHUNK_SIZE) {

                                    uint32_t idx = VoxelCoordinateSystem::voxel_index({src_x, src_y, src_z});
                                    if (original_data.opacity[idx]) {
                                        material_sum += original_data.materials[idx];
                                        voxel_count++;
                                    }
                                }
                            }
                        }
                    }

                    // Сохраняем усредненное значение
                    uint32_t dst_idx = x + (y * new_size) + (z * new_size * new_size);
                    if (voxel_count > 0) {
                        simplified_data[dst_idx] = material_sum / voxel_count;
                    }
                }
            }
        }

        return simplified_data;
    }
};
```

### SIMD оптимизации для массовых операций

Для обработки миллионов вокселей критически важны SIMD оптимизации:

```cpp
// Включение SIMD оптимизаций в glm
#define GLM_ENABLE_SIMD_SSE2  // Для SSE2
#define GLM_ENABLE_SIMD_AVX   // Для AVX (если доступно)
#define GLM_ENABLE_SIMD_AVX2  // Для AVX2 (если доступно)

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <immintrin.h>  // Для intrinsics

// Пакетная трансформация массива позиций с использованием SIMD
void transform_positions_simd(const std::vector<glm::vec3>& positions,
                             const glm::mat4& transform,
                             std::vector<glm::vec3>& result) {
    size_t count = positions.size();
    result.resize(count);

    // Извлекаем строки матрицы трансформации
    __m128 row0 = _mm_loadu_ps(&transform[0][0]);
    __m128 row1 = _mm_loadu_ps(&transform[1][0]);
    __m128 row2 = _mm_loadu_ps(&transform[2][0]);
    __m128 row3 = _mm_loadu_ps(&transform[3][0]);

    // Обрабатываем по 4 позиции за раз (SSE)
    for (size_t i = 0; i < count; i += 4) {
        size_t remaining = std::min(size_t(4), count - i);

        // Загружаем позиции
        alignas(16) float pos_x[4], pos_y[4], pos_z[4];
        for (size_t j = 0; j < remaining; ++j) {
            pos_x[j] = positions[i + j].x;
            pos_y[j] = positions[i + j].y;
            pos_z[j] = positions[i + j].z;
        }

        // SIMD вычисления
        __m128 x = _mm_load_ps(pos_x);
        __m128 y = _mm_load_ps(pos_y);
        __m128 z = _mm_load_ps(pos_z);

        // Вычисляем transformed = row0*x + row1*y + row2*z + row3
        __m128 tx = _mm_mul_ps(row0, x);
        __m128 ty = _mm_mul_ps(row1, y);
        __m128 tz = _mm_mul_ps(row2, z);

        __m128 transformed = _mm_add_ps(tx, ty);
        transformed = _mm_add_ps(transformed, tz);
        transformed = _mm_add_ps(transformed, row3);

        // Сохраняем результат
        alignas(16) float result_arr[4];
        _mm_store_ps(result_arr, transformed);

        for (size_t j = 0; j < remaining; ++j) {
            result[i + j] = glm::vec3(result_arr[j * 4], result_arr[j * 4 + 1], result_arr[j * 4 + 2]);
        }
    }
}

// Быстрое вычисление bounding box для чанка
struct ChunkBoundingBox {
    glm::vec3 min;
    glm::vec3 max;

    // SIMD оптимизированное вычисление
    static ChunkBoundingBox compute_simd(const glm::vec3& chunk_world_pos, float chunk_size) {
        __m128 chunk_pos = _mm_set_ps(0.0f, chunk_world_pos.z, chunk_world_pos.y, chunk_world_pos.x);
        __m128 half_size = _mm_set1_ps(chunk_size * 0.5f);

        __m128 min_vec = _mm_sub_ps(chunk_pos, half_size);
        __m128 max_vec = _mm_add_ps(chunk_pos, half_size);

        alignas(16) float min_arr[4], max_arr[4];
        _mm_store_ps(min_arr, min_vec);
        _mm_store_ps(max_arr, max_vec);

        return {
            {min_arr[0], min_arr[1], min_arr[2]},
            {max_arr[0], max_arr[1], max_arr[2]}
        };
    }
};
```

### Интеграция с Vulkan Compute Shaders

Передача математических данных в compute shaders для GPU-Driven рендеринга:

```cpp
// Структура для передачи данных чанка в compute shader
struct GPUVoxelChunkData {
    glm::mat4 world_transform;      // 64 байта
    glm::vec4 chunk_info;           // 16 байт: x,y,z = grid pos, w = lod level
    glm::uvec4 data_info;           // 16 байт: x = voxel_count, y = triangle_count, z = flags
    // Итого: 96 байт, выравнивание 16 байт

    // Конвертация для Vulkan буфера
    std::array<float, 24> to_float_array() const {
        std::array<float, 24> result;

        // Копируем матрицу (16 floats)
        const float* matrix_data = glm::value_ptr(world_transform);
        std::copy(matrix_data, matrix_data + 16, result.begin());

        // Копируем chunk_info (4 floats)
        result[16] = chunk_info.x;
        result[17] = chunk_info.y;
        result[18] = chunk_info.z;
        result[19] = chunk_info.w;

        // Копируем data_info (4 floats как uint)
        result[20] = static_cast<float>(data_info.x);
        result[21] = static_cast<float>(data_info.y);
        result[22] = static_cast<float>(data_info.z);
        result[23] = static_cast<float>(data_info.w);

        return result;
    }
};

// Пакетная подготовка данных для compute shader
std::vector<GPUVoxelChunkData> prepare_gpu_chunk_data(
    const std::vector<VoxelChunk>& chunks,
    const std::vector<glm::mat4>& transforms) {

    std::vector<GPUVoxelChunkData> gpu_data;
    gpu_data.reserve(chunks.size());

    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        const auto& transform = transforms[i];

        GPUVoxelChunkData data;
        data.world_transform = transform;
        data.chunk_info = glm::vec4(
            static_cast<float>(chunk.grid_position.x),
            static_cast<float>(chunk.grid_position.y),
            static_cast<float>(chunk.grid_position.z),
            static_cast<float>(chunk.lod_level)
        );
        data.data_info = glm::uvec4(
            chunk.voxel_count,
            chunk.triangle_count,
            chunk.is_dirty ? 1 : 0,
            chunk.is_visible ? 1 : 0
        );

        gpu_data.push_back(data);
    }

    return gpu_data;
}
```

---

## 3. Интеграция с VMA и инстансинг

В ProjectV часто требуется рендерить тысячи объектов (чанков) с минимальным overhead.

### Инстансинг с массивами матриц

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

// Массив матриц трансформации для инстансинга
struct InstanceData {
    std::vector<glm::mat4> model_matrices;
    size_t instance_count;
};

// Создание матриц для сетки чанков (например, 16×16)
InstanceData create_chunk_grid(int grid_size, float chunk_spacing) {
    InstanceData data;
    data.instance_count = grid_size * grid_size;
    data.model_matrices.reserve(data.instance_count);

    for (int z = 0; z < grid_size; ++z) {
        for (int x = 0; x < grid_size; ++x) {
            glm::mat4 model = glm::mat4(1.0f);
            glm::vec3 position(
                x * chunk_spacing,
                0.0f,
                z * chunk_spacing
            );
            model = glm::translate(model, position);
            data.model_matrices.push_back(model);
        }
    }

    return data;
}

// Передача массива матриц в VMA-буфер
void upload_instance_data(VkDevice device, VmaAllocator allocator,
                          const InstanceData& data, VkBuffer& buffer,
                          VmaAllocation& allocation) {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = data.instance_count * sizeof(glm::mat4);
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
                    &buffer, &allocation, nullptr);

    // Копирование данных через staging buffer
    // ... (используйте glm::value_ptr для каждой матрицы)
}
```

### Выравнивание для VMA и Vulkan

При работе с VMA важно учитывать выравнивание:

1. **Матрицы glm::mat4** — выравнивание 16 байт (совместимо с Vulkan).
2. **Векторы glm::vec4** — выравнивание 16 байт.
3. **Структуры с смешанными типами** — используйте `alignas(16)`.

```cpp
struct VoxelInstance {
    alignas(16) glm::mat4 model;      // 16 байт выравнивание
    alignas(16) glm::vec4 color;      // 16 байт выравнивание
    alignas(4)  uint32_t voxel_type;  // 4 байта выравнивание
    // Итого: 36 байт, но Vulkan требует кратность 16 → 48 байт с padding
};
```

---

## 4. Примеры кода

### Трансформация воксельного чанка (16×16×16)

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

// Позиция чанка в мире
constexpr glm::vec3 CHUNK_POSITION = {10.0f, 0.0f, 10.0f};
constexpr glm::vec3 CHUNK_SCALE = {1.0f, 1.0f, 1.0f};
constexpr float CHUNK_ROTATION = 0.0f; // Радианы

// Матрица трансформации для всего чанка
glm::mat4 get_chunk_transform() {
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, CHUNK_POSITION);
    transform = glm::rotate(transform, CHUNK_ROTATION, glm::vec3(0, 1, 0));
    transform = glm::scale(transform, CHUNK_SCALE);
    return transform;
}

// Трансформация массива позиций вокселей (оптимизированно)
std::vector<glm::vec4> transform_voxel_positions(
    const std::vector<glm::vec3>& local_positions,
    const glm::mat4& chunk_transform) {

    std::vector<glm::vec4> world_positions;
    world_positions.reserve(local_positions.size());

    for (const auto& local_pos : local_positions) {
        // Преобразование из локальных координат в мировые
        glm::vec4 world_pos = chunk_transform * glm::vec4(local_pos, 1.0f);
        world_positions.push_back(world_pos);
    }

    return world_positions;
}
```

---

## 5. Профилирование и отладка

### Интеграция с Tracy

Для профилирования математических операций можно использовать Tracy:

```cpp
#include <tracy/Tracy.hpp>

void process_voxel_chunk(const VoxelChunk& chunk) {
    ZoneScopedN("Process voxel chunk");

    {
        ZoneScopedN("Matrix transformations");
        // Тяжёлые вычисления матриц
        glm::mat4 mvp = proj * view * model;
    }
}
```

### Решение проблем производительности

**Проблема:** Низкая производительность массовых трансформаций.
**Причина:** Использование AoS вместо SoA.
**Решение:** Перейдите на SoA (см. раздел 1).

**Проблема:** Неэффективное использование SIMD.
**Причина:** Не включены SIMD оптимизации.
**Решение:** Включите `GLM_ENABLE_SIMD_SSE2` (или AVX) в CMake.

**Проблема:** Высокий overhead при создании матриц.
**Причина:** Частое вычисление одинаковых матриц (проекции, вида).
**Решение:** Кэшируйте результаты (`view * proj`).

---

## Сводка для ProjectV

| Действие                                           | Где                                                    |
|----------------------------------------------------|--------------------------------------------------------|
| `target_link_libraries(ProjectV PRIVATE glm::glm)` | CMakeLists.txt                                         |
| `GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE` | По желанию, до include glm                             |
| `glm::value_ptr(m)`                                | При копировании mat4/vec* в отображённую память Vulkan |
| **Оптимизация инстансинга**                        | Массивы матриц, VMA буферы, push constants             |
| **Выравнивание структур**                          | `alignas(16)` для mat4/vec4, учёт padding              |
