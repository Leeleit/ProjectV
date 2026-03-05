# Паттерны использования в ProjectV

🔴 **Уровень 3: Продвинутый**

Типичные паттерны использования meshoptimizer в воксельном движке.

---

## LOD Chain Generation

### Последовательное упрощение

Генерация цепочки LOD-ов от детального к грубому:

```cpp
struct LODChain {
    std::vector<std::vector<uint32_t>> lod_indices;
    std::vector<float> lod_errors;
    size_t base_vertex_count;
};

LODChain generateLODChain(
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& base_indices,
    const std::vector<size_t>& target_percentages  // {75, 50, 25, 10}
) {
    LODChain chain;
    chain.base_vertex_count = vertices.size();
    chain.lod_indices.push_back(base_indices);
    chain.lod_errors.push_back(0.0f);
    
    std::vector<uint32_t> prev_indices = base_indices;
    
    for (size_t pct : target_percentages) {
        size_t target = (base_indices.size() * pct) / 100;
        target = std::max(target, size_t(3));  // Минимум 1 треугольник
        
        std::vector<uint32_t> lod_indices(prev_indices.size());
        float error = 0.0f;
        
        size_t result = meshopt_simplify(
            lod_indices.data(),
            prev_indices.data(),
            prev_indices.size(),
            &vertices[0].position[0],
            vertices.size(),
            sizeof(MeshVertex),
            target,
            FLT_MAX,  // Без ограничения ошибки
            meshopt_SimplifyLockBorder,
            &error
        );
        
        if (result > 3) {
            lod_indices.resize(result);
            
            // Оптимизировать для vertex cache
            meshopt_optimizeVertexCache(
                lod_indices.data(),
                lod_indices.data(),
                result,
                vertices.size()
            );
            
            chain.lod_indices.push_back(std::move(lod_indices));
            chain.lod_errors.push_back(error);
            prev_indices = chain.lod_indices.back();
        }
    }
    
    return chain;
}
```

### LOD Selection

Выбор LOD на основе расстояния и ошибки:

```cpp
size_t selectLOD(const LODChain& chain, float distance, float screen_height, float fov) {
    // Экранный порог в пикселях
    const float pixel_threshold = 2.0f;
    
    // Преобразование ошибки в экранное пространство
    // error_screen = error * screen_height / (distance * tan(fov/2))
    float scale = screen_height / (distance * std::tan(fov / 2));
    
    for (size_t i = 1; i < chain.lod_errors.size(); ++i) {
        float screen_error = chain.lod_errors[i] * scale;
        
        if (screen_error < pixel_threshold) {
            return i;
        }
    }
    
    return chain.lod_indices.size() - 1;
}
```

### LOD с shared vertex buffer

Использование одного vertex buffer для всех LOD-ов:

```cpp
struct SharedLODMesh {
    VkBuffer vertex_buffer;
    VmaAllocation vertex_allocation;
    
    std::vector<VkBuffer> index_buffers;
    std::vector<VmaAllocation> index_allocations;
    std::vector<uint32_t> index_counts;
    std::vector<float> errors;
};

SharedLODMesh createSharedLODMesh(
    VmaAllocator allocator,
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& base_indices
) {
    SharedLODMesh result;
    
    // Создаём vertex buffer один раз
    createVertexBuffer(allocator, vertices, result.vertex_buffer, result.vertex_allocation);
    
    // Генерируем LOD-ы
    LODChain chain = generateLODChain(vertices, base_indices, {75, 50, 25, 10});
    
    // Оптимизируем vertex order для всех LOD-ов сразу
    std::vector<uint32_t> combined_remap;
    size_t total_vertices = optimizeVertexFetchForAllLODs(
        vertices, chain.lod_indices, combined_remap
    );
    
    // Создаём index buffers для каждого LOD
    for (size_t i = 0; i < chain.lod_indices.size(); ++i) {
        VkBuffer ib;
        VmaAllocation ib_alloc;
        createIndexBuffer(allocator, chain.lod_indices[i], ib, ib_alloc);
        
        result.index_buffers.push_back(ib);
        result.index_allocations.push_back(ib_alloc);
        result.index_counts.push_back(chain.lod_indices[i].size());
        result.errors.push_back(chain.lod_errors[i]);
    }
    
    return result;
}
```

---

## Meshlet Culling

### Frustum Culling

```cpp
struct Frustum {
    glm::vec4 planes[6];  // left, right, bottom, top, near, far
};

bool isMeshletVisible(const meshopt_Bounds& bounds, const Frustum& frustum) {
    // Bounding sphere test
    for (int i = 0; i < 6; ++i) {
        float distance = glm::dot(glm::vec3(frustum.planes[i]), 
                                  glm::vec3(bounds.center[0], bounds.center[1], bounds.center[2]))
                        + frustum.planes[i].w;
        
        if (distance < -bounds.radius) {
            return false;  // Полностью вне frustum
        }
    }
    return true;
}
```

### Cone Culling (Backface)

```cpp
bool isMeshletBackfacing(
    const meshopt_Bounds& bounds,
    const glm::vec3& camera_position
) {
    glm::vec3 center(bounds.center[0], bounds.center[1], bounds.center[2]);
    glm::vec3 cone_axis(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]);
    
    // Формула для perspective projection
    glm::vec3 view_dir = glm::normalize(center - camera_position);
    float d = glm::dot(view_dir, cone_axis);
    
    return d >= bounds.cone_cutoff;
}
```

### Occlusion Culling (GPU-driven)

```cpp
// CPU: подготовка indirect draw commands
void prepareMeshletDraws(
    const std::vector<meshopt_Bounds>& bounds,
    const Frustum& frustum,
    const glm::vec3& camera_position,
    std::vector<VkDrawMeshTasksIndirectCommandEXT>& commands
) {
    commands.clear();
    
    for (size_t i = 0; i < bounds.size(); ++i) {
        if (!isMeshletVisible(bounds[i], frustum)) continue;
        if (isMeshletBackfacing(bounds[i], camera_position)) continue;
        
        commands.push_back({
            .groupCountX = 1,
            .groupCountY = 1,
            .groupCountZ = 1
        });
    }
}

// GPU: hierarchical z-buffer occlusion culling
// Выполняется в task/amplification shader
```

---

## Streaming Decompression

### Потоковая загрузка сжатых данных

```cpp
class StreamingMeshLoader {
public:
    struct MeshPart {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
    };
    
    StreamingMeshLoader(size_t max_memory) : max_memory_(max_memory) {}
    
    std::future<MeshPart> loadCompressed(const std::string& path) {
        return std::async(std::launch::async, [this, path] {
            // 1. Загрузка сжатых данных с диска
            auto compressed = loadCompressedData(path);
            
            // 2. Декомпрессия
            MeshPart part;
            part.vertices.resize(compressed.vertex_count);
            part.indices.resize(compressed.index_count);
            
            // Декодирование vertex buffer
            int vres = meshopt_decodeVertexBuffer(
                part.vertices.data(),
                compressed.vertex_count,
                sizeof(MeshVertex),
                compressed.vertex_data.data(),
                compressed.vertex_data.size()
            );
            
            if (vres != 0) {
                throw std::runtime_error("Vertex decode failed");
            }
            
            // Декодирование index buffer
            int ires = meshopt_decodeIndexBuffer(
                part.indices.data(),
                compressed.index_count,
                sizeof(uint32_t),
                compressed.index_data.data(),
                compressed.index_data.size()
            );
            
            if (ires != 0) {
                throw std::runtime_error("Index decode failed");
            }
            
            return part;
        });
    }

private:
    size_t max_memory_;
    std::mutex mutex_;
};
```

### Приоритетная загрузка

```cpp
class PriorityMeshLoader {
public:
    using MeshID = uint64_t;
    using Priority = float;
    
    void requestMesh(MeshID id, Priority priority, std::function<void(OptimizedMesh)> callback) {
        std::lock_guard lock(mutex_);
        
        auto it = std::lower_bound(queue_.begin(), queue_.end(),
            std::make_pair(priority, id),
            [](auto& a, auto& b) { return a.first > b.first; });
        
        queue_.insert(it, {priority, id, callback});
        
        if (!loading_) {
            loading_ = true;
            worker_cv_.notify_one();
        }
    }
    
    void updatePriorities(const std::vector<std::pair<MeshID, Priority>>& updates) {
        std::lock_guard lock(mutex_);
        
        for (const auto& [id, priority] : updates) {
            // Найти и обновить приоритет
            for (auto& item : queue_) {
                if (std::get<1>(item) == id) {
                    item = {priority, id, std::get<2>(item)};
                    break;
                }
            }
        }
        
        // Пересортировать
        std::sort(queue_.begin(), queue_.end(),
            [](auto& a, auto& b) { return std::get<0>(a) > std::get<0>(b); });
    }

private:
    std::mutex mutex_;
    std::condition_variable worker_cv_;
    std::vector<std::tuple<Priority, MeshID, std::function<void(OptimizedMesh)>>> queue_;
    bool loading_ = false;
};
```

---

## Chunk Optimization

### Оптимизация воксельных чанков

```cpp
struct VoxelChunkMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    
    // Оптимизированные данные
    OptimizedMesh gpu_mesh;
    std::vector<meshopt_Bounds> meshlet_bounds;
};

VoxelChunkMesh createOptimizedChunkMesh(
    const std::vector<VoxelVertex>& raw_vertices,
    const std::vector<uint32_t>& raw_indices
) {
    VoxelChunkMesh result;
    
    // 1. Конвертация вершин
    result.vertices.resize(raw_vertices.size());
    for (size_t i = 0; i < raw_vertices.size(); ++i) {
        result.vertices[i].position[0] = static_cast<float>(raw_vertices[i].x);
        result.vertices[i].position[1] = static_cast<float>(raw_vertices[i].y);
        result.vertices[i].position[2] = static_cast<float>(raw_vertices[i].z);
        // ... normals, uvs, etc.
    }
    
    // 2. Indexing (удаление дубликатов от marching cubes)
    std::vector<uint32_t> remap(result.vertices.size());
    size_t unique_count = meshopt_generateVertexRemap(
        remap.data(),
        raw_indices.data(),
        raw_indices.size(),
        result.vertices.data(),
        result.vertices.size(),
        sizeof(MeshVertex)
    );
    
    result.indices.resize(raw_indices.size());
    meshopt_remapIndexBuffer(result.indices.data(), raw_indices.data(),
                             raw_indices.size(), remap.data());
    
    result.vertices.resize(unique_count);
    meshopt_remapVertexBuffer(result.vertices.data(), result.vertices.data(),
                              unique_count, sizeof(MeshVertex), remap.data());
    
    // 3. Optimization
    meshopt_optimizeVertexCache(result.indices.data(), result.indices.data(),
                                result.indices.size(), unique_count);
    meshopt_optimizeVertexFetch(result.vertices.data(), result.indices.data(),
                                result.indices.size(), result.vertices.data(),
                                unique_count, sizeof(MeshVertex));
    
    return result;
}
```

### Параллельная генерация чанков

```cpp
class ChunkMeshGenerator {
public:
    ChunkMeshGenerator(size_t thread_count) {
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }
    
    ~ChunkMeshGenerator() {
        stop_ = true;
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }
    
    std::future<VoxelChunkMesh> generateAsync(ChunkCoord coord) {
        std::promise<VoxelChunkMesh> promise;
        auto future = promise.get_future();
        
        {
            std::lock_guard lock(mutex_);
            queue_.push({coord, std::move(promise)});
        }
        cv_.notify_one();
        
        return future;
    }

private:
    struct WorkItem {
        ChunkCoord coord;
        std::promise<VoxelChunkMesh> promise;
    };
    
    void workerLoop() {
        while (true) {
            WorkItem item;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_) return;
                
                item = std::move(queue_.front());
                queue_.pop();
            }
            
            try {
                // Генерация меши чанка
                auto [vertices, indices] = generateChunkGeometry(item.coord);
                
                // Оптимизация
                auto mesh = createOptimizedChunkMesh(vertices, indices);
                
                item.promise.set_value(std::move(mesh));
            } catch (...) {
                item.promise.set_exception(std::current_exception());
            }
        }
    }
    
    std::vector<std::thread> workers_;
    std::queue<WorkItem> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};
```

---

## Raytracing Optimization

### Meshlets для BLAS

```cpp
struct RaytracingMeshletData {
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<uint32_t> meshlet_vertices;
    std::vector<uint8_t> meshlet_triangles;
    std::vector<meshopt_Bounds> bounds;
};

RaytracingMeshletData createRaytracingMeshlets(
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& indices
) {
    const size_t max_vertices = 64;
    const size_t min_triangles = 16;
    const size_t max_triangles = 64;
    const float fill_weight = 0.5f;
    
    size_t max_meshlets = meshopt_buildMeshletsBound(
        indices.size(), max_vertices, min_triangles);
    
    RaytracingMeshletData result;
    result.meshlets.resize(max_meshlets);
    result.meshlet_vertices.resize(max_meshlets * max_vertices);
    result.meshlet_triangles.resize(max_meshlets * max_triangles * 3);
    
    size_t meshlet_count = meshopt_buildMeshletsSpatial(
        result.meshlets.data(),
        result.meshlet_vertices.data(),
        result.meshlet_triangles.data(),
        indices.data(),
        indices.size(),
        &vertices[0].position[0],
        vertices.size(),
        sizeof(MeshVertex),
        max_vertices,
        min_triangles,
        max_triangles,
        fill_weight
    );
    
    result.meshlets.resize(meshlet_count);
    
    // Вычислить bounds
    result.bounds.resize(meshlet_count);
    for (size_t i = 0; i < meshlet_count; ++i) {
        const auto& m = result.meshlets[i];
        result.bounds[i] = meshopt_computeMeshletBounds(
            &result.meshlet_vertices[m.vertex_offset],
            &result.meshlet_triangles[m.triangle_offset],
            m.triangle_count,
            &vertices[0].position[0],
            vertices.size(),
            sizeof(MeshVertex)
        );
    }
    
    return result;
}
```

---

## Memory Budget Management

### Управление памятью при загрузке

```cpp
class MeshMemoryManager {
public:
    MeshMemoryManager(size_t budget) : budget_(budget), used_(0) {}
    
    bool canAllocate(size_t size) const {
        return used_ + size <= budget_;
    }
    
    std::optional<OptimizedMesh> tryLoad(
        const std::string& path,
        VmaAllocator allocator
    ) {
        // Оценка размера
        size_t estimated_size = estimateMeshSize(path);
        
        if (!canAllocate(estimated_size)) {
            // Попробовать освободить память
            evictLRU(estimated_size);
        }
        
        if (!canAllocate(estimated_size)) {
            return std::nullopt;
        }
        
        // Загрузка
        auto mesh = loadAndOptimizeMesh(path, allocator);
        used_ += getMeshSize(mesh);
        
        // Добавить в LRU
        lru_list_.push_front({path, mesh});
        lru_map_[path] = lru_list_.begin();
        
        return mesh;
    }
    
    void evictLRU(size_t needed_space) {
        while (used_ + needed_space > budget_ && !lru_list_.empty()) {
            auto& [path, mesh] = lru_list_.back();
            
            used_ -= getMeshSize(mesh);
            destroyMesh(mesh);
            
            lru_map_.erase(path);
            lru_list_.pop_back();
        }
    }

private:
    size_t budget_;
    size_t used_;
    std::list<std::pair<std::string, OptimizedMesh>> lru_list_;
    std::unordered_map<std::string, decltype(lru_list_)::iterator> lru_map_;
};