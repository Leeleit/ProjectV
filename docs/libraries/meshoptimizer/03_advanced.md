# DOD Оптимизации и Хардкор

> Продвинутые техники для maximum performance. Zero-copy, Job System, SoA, alignas(64).

---

## Data-Oriented Design для Mesh Данных

### AoS vs SoA

Традиционный Array of Structures (AoS) — это плохо для кэша. SoA — наш выбор.

```cpp
// ❌ AoS — плохо для cache line utilization
struct VertexAoS {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float tx, ty, tz, tw;  // tangent
};
std::vector<VertexAoS> vertices_aos;

// ✅ SoA — данные одного типа лежат рядом
struct VertexSoA {
    alignas(64) std::vector<float> px;  // 64 bytes = cache line
    alignas(64) std::vector<float> py;
    alignas(64) std::vector<float> pz;
    alignas(64) std::vector<float> nx;
    alignas(64) std::vector<float> ny;
    alignas(64) std::vector<float> nz;
    alignas(64) std::vector<float> u;
    alignas(64) std::vector<float> v;
};

class MeshSoA {
public:
    size_t vertex_count = 0;

    // Hot data — accessed every frame
    alignas(64) std::vector<float> position_x;
    alignas(64) std::vector<float> position_y;
    alignas(64) std::vector<float> position_z;

    // Cold data — accessed less frequently
    alignas(64) std::vector<float> normal_x;
    alignas(64) std::vector<float> normal_y;
    alignas(64) std::vector<float> normal_z;
    alignas(64) std::vector<float> uv_u;
    alignas(64) std::vector<float> uv_v;

    void resize(size_t count) {
        vertex_count = count;
        position_x.resize(count);
        position_y.resize(count);
        position_z.resize(count);
        normal_x.resize(count);
        normal_y.resize(count);
        normal_z.resize(count);
        uv_u.resize(count);
        uv_v.resize(count);
    }
};
```

> **Для понимания:** AoS — это как если бы в шкафу лежала вся одежда вперемешку: носки с рубашками, штаны с куртками.
> SoA — это как отдельные полки: все носки вместе, все рубашки вместе. Когда нужен один тип — открываешь одну полку, не
> роешься во всём шкафу.

---

## Job System Интеграция

### Параллельная оптимизация мешей

Никаких `std::thread` внутри игрового кадра. Используй Job System.

```cpp
#include <job_system.h>  // Твой Job System

class ParallelMeshOptimizer {
public:
    struct OptimizeJob {
        uint32_t mesh_id;
        std::span<const VertexSoA::value_type> vertices;
        std::span<uint32_t> indices;
        std::span<uint32_t> remap;
        size_t unique_count;
    };

    // Job для CPU optimization (фоновые потоки)
    struct MeshOptimizationJob : JobSystem::IJob {
        OptimizeJob* job_data = nullptr;

        void execute() override {
            auto& j = *job_data;

            // Indexing phase
            meshopt_generateVertexRemap(
                j.remap.data(),
                nullptr,  // no source index buffer
                j.indices.size(),
                j.vertices.data(),
                j.vertices.size(),
                sizeof(VertexAoS)  // treats as array of bytes
            );

            // Remap
            meshopt_remapIndexBuffer(
                j.indices.data(),
                j.indices.data(),
                j.indices.size(),
                j.remap.data()
            );

            // Vertex cache
            meshopt_optimizeVertexCache(
                j.indices.data(),
                j.indices.data(),
                j.indices.size(),
                j.unique_count
            );
        }
    };

    // Интерфейс для очереди задач
    void queueOptimizations(
        JobSystem& js,
        std::span<MeshData> meshes
    ) {
        std::vector<OptimizeJob> jobs(meshes.size());
        std::vector<MeshOptimizationJob> job_impls(meshes.size());

        for (size_t i = 0; i < meshes.size(); ++i) {
            jobs[i].mesh_id = meshes[i].id;
            jobs[i].vertices = meshes[i].raw_vertices;
            jobs[i].indices = meshes[i].raw_indices;
            jobs[i].remap.resize(meshes[i].raw_vertices.size());

            job_impls[i].job_data = &jobs[i];

            js.enqueue(&job_impls[i], JobSystem::Priority::Normal);
        }
    }
};
```

### Параллельная LOD генерация

```cpp
class LODGenerationJob : public JobSystem::IJob {
public:
    struct LODJobData {
        std::span<const VertexSoA> vertices;
        std::span<const uint32_t> base_indices;
        std::span<uint32_t> lod_indices;
        float target_error;
        size_t target_count;
        float* out_error;
    };

    LODJobData* data = nullptr;

    void execute() override {
        float error = 0.        const size_t0f;

 result = meshopt_simplify(
            data->lod_indices.data(),
            data->base_indices.data(),
            data->base_indices.size(),
            &data->vertices->position_x[0],  // pointer to float array
            data->vertices->vertex_count,
            sizeof(float) * 3,  // stride for position only
            data->target_count,
            data->target_error,
            meshopt_SimplifyLockBorder,
            &error
        );

        data->lod_indices.resize(result);
        if (data->out_error) {
            *data->out_error = error;
        }
    }
};
```

---

## Zero-Copy Streaming

### Декодирование напрямую в GPU буфер

Декодер meshoptimizer работает быстро. Используй это для стриминга.

```cpp
#include <vk_mem_alloc.h>

class StreamingMeshLoader {
public:
    StreamingMeshLoader(VmaAllocator allocator, VkDevice device)
        : allocator_(allocator), device_(device) {}

    // Декодирование напрямую в pre-allocated GPU буфер
    // Никаких промежуточных CPU аллокаций
    [[nodiscard]]
    auto decodeToGPUBuffer(
        std::span<const uint8_t> compressed_vertex_data,
        std::span<const uint8_t> compressed_index_data,
        size_t vertex_count,
        size_t vertex_size,
        size_t index_count
    ) -> std::expected<VkBuffer, VkResult> {

        // GPU буфер для decoded данных
        // Заранее размеченный пул буферов
        auto vb = allocateVertexBuffer(vertex_count * vertex_size);
        if (!vb) return std::unexpected{vb.error()};

        auto ib = allocateIndexBuffer(index_count * sizeof(uint32_t));
        if (!ib) {
            deallocate(*vb);
            return std::unexpected{ib.error()};
        }

        // Декодирование vertex buffer
        // Пишем сразу в отображаемую память (mapped memory)
        void* vb_mapped = mapBuffer(*vb);
        if (!vb_mapped) return std::unexpected{VK_ERROR_MEMORY_MAP_FAILED};

        const int vresult = meshopt_decodeVertexBuffer(
            vb_mapped,
            vertex_count,
            vertex_size,
            compressed_vertex_data.data(),
            compressed_vertex_data.size()
        );

        unmapBuffer(*vb);

        if (vresult != 0) {
            return std::unexpected{VK_ERROR_UNKNOWN};
        }

        // Аналогично для index buffer
        void* ib_mapped = mapBuffer(*ib);
        if (!ib_mapped) return std::unexpected{VK_ERROR_MEMORY_MAP_FAILED};

        const int iresult = meshopt_decodeIndexBuffer(
            ib_mapped,
            index_count,
            compressed_index_data.data(),
            compressed_index_data.size()
        );

        unmapBuffer(*ib);

        if (iresult != 0) {
            return std::unexpected{VK_ERROR_UNKNOWN};
        }

        // Возвращаем кастомный result с обоими буферами
        // (упрощённо - вернём один compound тип)
        return *vb;
    }

private:
    VmaAllocator allocator_;
    VkDevice device_;

    struct GPUBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

    [[nodiscard]]
    std::expected<GPUBuffer, VkResult> allocateVertexBuffer(VkDeviceSize size) {
        GPUBuffer result;

        VkBufferCreateInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo alloc = {};
        alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        const VkResult r = vmaCreateBuffer(allocator_, &info, &alloc,
                                          &result.buffer, &result.allocation, nullptr);

        if (r != VK_SUCCESS) return std::unexpected{r};

        return result;
    }

    [[nodiscard]]
    std::expected<GPUBuffer, VkResult> allocateIndexBuffer(VkDeviceSize size) {
        GPUBuffer result;

        VkBufferCreateInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        VmaAllocationCreateInfo alloc = {};
        alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        const VkResult r = vmaCreateBuffer(allocator_, &info, &alloc,
                                          &result.buffer, &result.allocation, nullptr);

        if (r != VK_SUCCESS) return std::unexpected{r};

        return result;
    }

    void* mapBuffer(const GPUBuffer& buf) {
        void* mapped = nullptr;
        vmaMapMemory(allocator_, buf.allocation, &mapped);
        return mapped;
    }

    void unmapBuffer(const GPUBuffer& buf) {
        vmaUnmapMemory(allocator_, buf.allocation);
    }

    void deallocate(const GPUBuffer& buf) {
        vmaDestroyBuffer(allocator_, buf.buffer, buf.allocation);
    }
};
```

---

## Cache Line Alignment

### Структуры с выравниванием

```cpp
#include <cstddef>

// Выравнивание на 64 байта = размер cache line
// Исключает false sharing между потоками
struct alignas(64) CacheLinePaddedVertexData {
    // Позиции
    float px[128];  // 512 bytes total
    float py[128];
    float pz[128];

    // Потокозащищённый счётчик обработанных вершин
    alignas(64) uint32_t processed_count = 0;
};

// Для meshlet bounds — SIMD-friendly layout
struct alignas(16) AlignedMeshletBounds {
    float center_x;
    float center_y;
    float center_z;
    float radius;

    float cone_apex_x;
    float cone_apex_y;
    float cone_apex_z;
    float cone_cutoff;

    float cone_axis_x;
    float cone_axis_y;
    float cone_axis_z;
    float padding;  // fill to 16 bytes

    // SIMD-friendly frustum test
    bool isVisibleInFrustum(std::span<const float, 24> planes) const {
        // 6 planes × 4 components = 24 floats
        // Load 4 floats at once for SIMD
        for (size_t i = 0; i < 6; ++i) {
            const float4 plane = {
                planes[i * 4 + 0],
                planes[i * 4 + 1],
                planes[i * 4 + 2],
                planes[i * 4 + 3]
            };

            const float dist = center_x * plane[0] +
                             center_y * plane[1] +
                             center_z * plane[2] + plane[3];

            if (dist < -radius) return false;
        }
        return true;
    }
};
```

---

## GPU-Driven Паттерны

### Indirect Drawing с Meshlets

```cpp
#include <vk_ext_mesh_shader.h>  // VK_EXT_mesh_shader

class MeshletRenderer {
public:
    struct DrawData {
        VkBuffer meshlets;
        VkBuffer meshlet_vertices;
        VkBuffer meshlet_triangles;
        VkBuffer bounds;
        VkBuffer indirect_commands;

        uint32_t meshlet_count;
    };

    // CPU: подготовка indirect commands на основе frustum culling
    void prepareIndirectDraws(
        DrawData& draw_data,
        std::span<const AlignedMeshletBounds> cpu_bounds,
        const Frustum& frustum,
        const glm::vec3& camera_position
    ) {
        std::vector<VkDrawMeshTasksIndirectCommandEXT> commands;
        commands.reserve(cpu_bounds.size());

        for (size_t i = 0; i < cpu_bounds.size(); ++i) {
            // Frustum culling
            if (!cpu_bounds[i].isVisibleInFrustum(frustum.planes)) {
                continue;
            }

            // Backface culling через cone
            if (isBackfacing(cpu_bounds[i], camera_position)) {
                continue;
            }

            commands.push_back({
                .groupCountX = 1,
                .groupCountY = 1,
                .groupCountZ = 1
            });
        }

        // Upload indirect commands
        uploadToBuffer(draw_data.indirect_commands, commands);
        draw_data.meshlet_count = static_cast<uint32_t>(commands.size());
    }

private:
    bool isBackfacing(
        const AlignedMeshletBounds& bounds,
        const glm::vec3& camera_pos
    ) {
        const glm::vec3 view_dir = glm::normalize(
            glm::vec3(bounds.center_x, bounds.center_y, bounds.center_z) - camera_pos
        );

        const glm::vec3 cone_axis = {
            bounds.cone_axis_x,
            bounds.cone_axis_y,
            bounds.cone_axis_z
        };

        return glm::dot(view_dir, cone_axis) >= bounds.cone_cutoff;
    }
};
```

### Frustum Culling с SIMD

```cpp
#include <immintrin.h>

struct alignas(32) SimdFrustum {
    __m256 planes_x;  // 8 planes (extended frustum)
    __m256 planes_y;
    __m256 planes_z;
    __m256 planes_w;
};

bool cullMeshletsSIMD(
    std::span<const AlignedMeshletBounds> bounds,
    const SimdFrustum& frustum,
    std::span<uint32_t> visible_indices
) {
    __m256 radius_broadcast = _mm256_set1_ps(0.0f);

    for (size_t i = 0; i < bounds.size(); i += 8) {
        // Load 8 centers
        __m256 cx = _mm256_set_ps(
            bounds[i+7].center_x, bounds[i+6].center_x,
            bounds[i+5].center_x, bounds[i+4].center_x,
            bounds[i+3].center_x, bounds[i+2].center_x,
            bounds[i+1].center_x, bounds[i+0].center_x
        );

        __m256 cy = _mm256_set_ps(
            bounds[i+7].center_y, bounds[i+6].center_y,
            bounds[i+5].center_y, bounds[i+4].center_y,
            bounds[i+3].center_y, bounds[i+2].center_y,
            bounds[i+1].center_y, bounds[i+0].center_y
        );

        __m256 cz = _mm256_set_ps(
            bounds[i+7].center_z, bounds[i+6].center_z,
            bounds[i+5].center_z, bounds[i+4].center_z,
            bounds[i+3].center_z, bounds[i+2].center_z,
            bounds[i+1].center_z, bounds[i+0].center_z
        );

        __m256 r = _mm256_set_ps(
            bounds[i+7].radius, bounds[i+6].radius,
            bounds[i+5].radius, bounds[i+4].radius,
            bounds[i+3].radius, bounds[i+2].radius,
            bounds[i+1].radius, bounds[i+0].radius
        );

        // Test against all 8 planes (or fewer if less remain)
        __m256i mask = _mm256_set1_epi32(-1);  // all bits set

        for (int p = 0; p < 6; ++p) {
            __m256 px = _mm256_load_ps(&frustum.planes_x);
            __m256 py = _mm256_load_ps(&frustum.planes_y);
            __m256 pz = _mm256_load_ps(&frustum.planes_z);
            __m256 pw = _mm256_load_ps(&frustum.planes_w);

            // dot product: x*px + y*py + z*pz + w
            __m256 dist = _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(cx, px), _mm256_mul_ps(cy, py)),
                _mm256_add_ps(_mm256_mul_ps(cz, pz), pw)
            );

            // dist < -radius → outside
            __m256 dist_minus_r = _mm256_sub_ps(dist, r);
            __m256 outside = _mm256_cmp_ps(dist_minus_r, _mm256_set1_ps(0.0f), _CMP_LT_OQ);

            // If outside any plane, clear the bit
            mask = _mm256_castps_si256(_mm256_and_ps(
                _mm256_castsi256_ps(mask),
                _mm256_castsi256_ps(_mm256_castps_si256(outside))
            ));
        }

        // Extract results
        int m = _mm256_movemask_ps(_mm256_castsi256_ps(mask));

        // Add visible indices
        for (int j = 0; j < 8 && (i + j) < bounds.size(); ++j) {
            if (m & (1 << j)) {
                visible_indices.push_back(static_cast<uint32_t>(i + j));
            }
        }
    }

    return !visible_indices.empty();
}
```

---

## Memory Pool для Частой Аллокации

### Пул для meshlet данных

```cpp
#include <memory>
#include <vector>

class MeshletMemoryPool {
public:
    struct PoolChunk {
        static constexpr size_t SIZE = 1024 * 1024;  // 1 MB
        alignas(64) std::byte data[SIZE];
        size_t used = 0;
    };

    MeshletMemoryPool() {
        chunks_.push_back(std::make_unique<PoolChunk>());
    }

    template<typename T>
    [[nodiscard]]
    std::span<T> allocate(size_t count) {
        const size_t bytes_needed = count * sizeof(T);
        const size_t alignment = alignof(T);

        // Align
        size_t aligned_used = (currentChunk()->used + alignment - 1) & ~(alignment - 1);

        // Need new chunk?
        if (aligned_used + bytes_needed > PoolChunk::SIZE) {
            chunks_.push_back(std::make_unique<PoolChunk>());
            aligned_used = 0;
        }

        auto* ptr = currentChunk()->data + aligned_used;
        currentChunk()->used = aligned_used + bytes_needed;

        return {reinterpret_cast<T*>(ptr), count};
    }

    void reset() {
        for (auto& chunk : chunks_) {
            chunk->used = 0;
        }
    }

private:
    std::vector<std::unique_ptr<PoolChunk>> chunks_;

    PoolChunk* currentChunk() {
        return chunks_.back().get();
    }
};

// Использование с meshoptimizer
auto buildMeshletsWithPool(
    MeshletMemoryPool& pool,
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) -> MeshletData {
    const size_t max_meshlets = meshopt_buildMeshletsBound(
        indices.size(), 64, 126
    );

    // Allocate из пула
    auto meshlets = pool.allocate<meshopt_Meshlet>(max_meshlets);
    auto mv = pool.allocate<uint32_t>(max_meshlets * 64);
    auto mt = pool.allocate<uint8_t>(max_meshlets * 126 * 3);

    const size_t count = meshopt_buildMeshlets(
        meshlets.data(),
        mv.data(),
        mt.data(),
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex),
        64, 126, 0.0f
    );

    return {
        .meshlets = meshlets.subspan(0, count),
        .vertices = mv,
        .triangles = mt
    };
}
```

---

## Profile-Guided Optimization

### Tracy интеграция

```cpp
#include <tracy/Tracy.hpp>

class ProfiledMeshOptimizer {
public:
    void optimizeWithProfiling(
        std::span<Vertex> vertices,
        std::span<uint32_t> indices
    ) {
        ZoneScopedN("MeshOptimization");

        // Phase 1: Indexing
        {
            ZoneScopedN("Indexing");
            std::vector<uint32_t> remap(vertices.size());

            const ZoneTimer t1("generateVertexRemap");
            meshopt_generateVertexRemap(
                remap.data(), nullptr, indices.size(),
                vertices.data(), vertices.size(), sizeof(Vertex)
            );
        }

        // Phase 2: Vertex Cache
        {
            ZoneScopedN("VertexCache");
            const ZoneTimer t2("optimizeVertexCache");

            meshopt_optimizeVertexCache(
                indices.data(), indices.data(),
                indices.size(), vertices.size()
            );
        }

        // Phase 3: Vertex Fetch
        {
            ZoneScopedN("VertexFetch");
            const ZoneTimer t3("optimizeVertexFetch");

            meshopt_optimizeVertexFetch(
                vertices.data(), indices.data(), indices.size(),
                vertices.data(), vertices.size(), sizeof(Vertex)
            );
        }

        // Phase 4: Analysis
        {
            ZoneScopedN("Analysis");
            const ZoneTimer t4("analyzeVertexCache");

            const auto stats = meshopt_analyzeVertexCache(
                indices.data(), indices.size(),
                vertices.size(), 16, 0, 0
            );

            TracyPlot("ACMR", stats.acmr);
            TracyPlot("ATVR", stats.atvr);
        }
    }
};
```

---

## Продвинутые Паттерны

### Ring Buffer для Streaming LOD

```cpp
class LODStreamingRingBuffer {
public:
    static constexpr size_t MAX_PENDING = 8;

    struct PendingLOD {
        uint32_t mesh_id;
        size_t lod_level;
        std::vector<uint8_t> compressed_data;
        VkFence fence;
    };

    void requestLOD(uint32_t mesh_id, size_t lod_level) {
        if (pending_count >= MAX_PENDING) {
            waitForOldest();
        }

        // Асинхронная декомпрессия
        auto compressed = loadCompressedLOD(mesh_id, lod_level);

        PendingLOD p;
        p.mesh_id = mesh_id;
        p.lod_level = lod_level;
        p.compressed_data = std::move(compressed);

        // Queue async decode на transfer queue
        scheduleAsyncDecode(p);

        pending_[write_pos_] = std::move(p);
        write_pos_ = (write_pos_ + 1) % MAX_PENDING;
        pending_count++;
    }

    void update() {
        // Проверяем готовые LOD
        for (size_t i = 0; i < pending_count; ++i) {
            const size_t idx = (read_pos_ + i) % MAX_PENDING;

            if (pending_[idx].fence && isFenceReady(pending_[idx].fence)) {
                // LOD готов, обновляем ECS компонент
                uploadToGPUAndUpdateECS(pending_[idx]);

                // Освобождаем слот
                pending_[idx] = {};
                completed_count++;
            }
        }

        // Compact pending array
        // ...
    }

private:
    std::array<PendingLOD, MAX_PENDING> pending_;
    size_t write_pos_ = 0;
    size_t read_pos_ = 0;
    size_t pending_count = 0;
    size_t completed_count = 0;
};
```

---

## Итоговый Чеклист Оптимизаций

| Техника              | Винрейт               | Сложность |
|----------------------|-----------------------|-----------|
| SoA layout           | 2–5x                  | Низкая    |
| Job System           | 4–8x (CPU cores)      | Средняя   |
| Zero-copy decode     | 2x (memory bandwidth) | Средняя   |
| Cache line alignment | 1.1–1.3x (MT)         | Низкая    |
| Indirect drawing     | 1.5–3x (draw calls)   | Высокая   |
| SIMD culling         | 4–8x (culling speed)  | Высокая   |
| Memory pooling       | 1.2–1.5x (allocator)  | Средняя   |

> **Важно:** Не все техники нужны сразу. Начинай с профилирования, затем добавляй оптимизации по необходимости.
> Premature optimization — зло.
