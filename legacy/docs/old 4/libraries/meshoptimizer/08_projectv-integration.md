# Интеграция с ProjectV

🔴 **Уровень 3: Продвинутый**

Детали интеграции meshoptimizer с Vulkan, VMA, fastgltf, flecs.

---

## Интеграция с Vulkan

### Создание оптимизированных буферов

```cpp
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>

struct MeshVertex {
    float position[3];
    float normal[3];
    float uv[2];
};

struct OptimizedMesh {
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    VmaAllocation vertex_allocation;
    VmaAllocation index_allocation;
    size_t index_count;
    size_t vertex_count;
};

OptimizedMesh createOptimizedMesh(
    VmaAllocator allocator,
    const std::vector<MeshVertex>& src_vertices,
    const std::vector<uint32_t>& src_indices
) {
    // 1. Indexing
    std::vector<uint32_t> remap(src_vertices.size());
    size_t unique_count = meshopt_generateVertexRemap(
        remap.data(),
        src_indices.data(),
        src_indices.size(),
        src_vertices.data(),
        src_vertices.size(),
        sizeof(MeshVertex)
    );
    
    std::vector<uint32_t> indices(src_indices.size());
    meshopt_remapIndexBuffer(indices.data(), src_indices.data(), src_indices.size(), remap.data());
    
    std::vector<MeshVertex> vertices(unique_count);
    meshopt_remapVertexBuffer(vertices.data(), src_vertices.data(), src_vertices.size(),
                              sizeof(MeshVertex), remap.data());
    
    // 2. Vertex Cache Optimization
    meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), unique_count);
    
    // 3. Vertex Fetch Optimization
    meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(),
                                vertices.data(), unique_count, sizeof(MeshVertex));
    
    // 4. Create GPU buffers
    OptimizedMesh result;
    result.index_count = indices.size();
    result.vertex_count = unique_count;
    
    // Vertex buffer
    VkBufferCreateInfo vb_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    vb_info.size = vertices.size() * sizeof(MeshVertex);
    vb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo vb_alloc = {};
    vb_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    vb_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    vmaCreateBuffer(allocator, &vb_info, &vb_alloc,
        &result.vertex_buffer, &result.vertex_allocation, nullptr);
    
    // Index buffer
    VkBufferCreateInfo ib_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ib_info.size = indices.size() * sizeof(uint32_t);
    ib_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo ib_alloc = {};
    ib_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    ib_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    vmaCreateBuffer(allocator, &ib_info, &ib_alloc,
        &result.index_buffer, &result.index_allocation, nullptr);
    
    // Upload data (через staging buffer)
    uploadToDevice(allocator, result.vertex_buffer, vertices.data(), vb_info.size);
    uploadToDevice(allocator, result.index_buffer, indices.data(), ib_info.size);
    
    return result;
}
```

### Mesh Shading Pipeline

```cpp
struct MeshletGPU {
    uint32_t vertex_offset;
    uint32_t triangle_offset;
    uint32_t vertex_count;
    uint32_t triangle_count;
};

struct MeshletData {
    VkBuffer meshlets_buffer;
    VkBuffer vertices_buffer;
    VkBuffer triangles_buffer;
    VkBuffer bounds_buffer;
    
    std::vector<meshopt_Bounds> bounds;
    size_t meshlet_count;
};

MeshletData createMeshlets(
    VmaAllocator allocator,
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& indices
) {
    const size_t max_vertices = 64;
    const size_t max_triangles = 126;
    const float cone_weight = 0.25f;
    
    size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);
    
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<uint32_t> meshlet_vertices(max_meshlets * max_vertices);
    std::vector<uint8_t> meshlet_triangles(max_meshlets * max_triangles * 3);
    
    size_t meshlet_count = meshopt_buildMeshlets(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_triangles.data(),
        indices.data(),
        indices.size(),
        &vertices[0].position[0],
        vertices.size(),
        sizeof(MeshVertex),
        max_vertices,
        max_triangles,
        cone_weight
    );
    
    // Trim to actual size
    meshlets.resize(meshlet_count);
    
    // Compute bounds for each meshlet
    std::vector<meshopt_Bounds> bounds(meshlet_count);
    for (size_t i = 0; i < meshlet_count; ++i) {
        const auto& m = meshlets[i];
        bounds[i] = meshopt_computeMeshletBounds(
            &meshlet_vertices[m.vertex_offset],
            &meshlet_triangles[m.triangle_offset],
            m.triangle_count,
            &vertices[0].position[0],
            vertices.size(),
            sizeof(MeshVertex)
        );
    }
    
    // Create GPU buffers
    MeshletData result;
    result.bounds = std::move(bounds);
    result.meshlet_count = meshlet_count;
    
    // Upload meshlets, vertices, triangles, bounds to GPU
    // ...
    
    return result;
}
```

---

## Интеграция с fastgltf

### Загрузка и оптимизация glTF

```cpp
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

struct GLTFMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

GLTFMesh loadAndOptimizeGLTFMesh(
    const fastgltf::Asset& asset,
    const fastgltf::Mesh& mesh,
    size_t primitive_index
) {
    const auto& primitive = mesh.primitives[primitive_index];
    
    // Load positions
    auto* position_attr = primitive.findAttribute("POSITION");
    auto& position_accessor = asset.accessors[position_attr->second];
    
    std::vector<glm::vec3> positions(position_accessor.count);
    fastgltf::copyFromAccessor(asset, position_accessor, positions.data());
    
    // Load normals (optional)
    std::vector<glm::vec3> normals;
    if (auto* normal_attr = primitive.findAttribute("NORMAL")) {
        normals.resize(position_accessor.count);
        fastgltf::copyFromAccessor(asset, asset.accessors[normal_attr->second], normals.data());
    }
    
    // Load UVs (optional)
    std::vector<glm::vec2> uvs;
    if (auto* uv_attr = primitive.findAttribute("TEXCOORD_0")) {
        uvs.resize(position_accessor.count);
        fastgltf::copyFromAccessor(asset, asset.accessors[uv_attr->second], uvs.data());
    }
    
    // Build vertices
    GLTFMesh result;
    result.vertices.resize(position_accessor.count);
    
    for (size_t i = 0; i < position_accessor.count; ++i) {
        result.vertices[i].position[0] = positions[i].x;
        result.vertices[i].position[1] = positions[i].y;
        result.vertices[i].position[2] = positions[i].z;
        
        if (!normals.empty()) {
            result.vertices[i].normal[0] = normals[i].x;
            result.vertices[i].normal[1] = normals[i].y;
            result.vertices[i].normal[2] = normals[i].z;
        }
        
        if (!uvs.empty()) {
            result.vertices[i].uv[0] = uvs[i].x;
            result.vertices[i].uv[1] = uvs[i].y;
        }
    }
    
    // Load indices
    if (primitive.indicesAccessor) {
        auto& index_accessor = asset.accessors[primitive.indicesAccessor.value()];
        result.indices.resize(index_accessor.count);
        fastgltf::copyFromAccessor(asset, index_accessor, result.indices.data());
    } else {
        // Generate indices for non-indexed geometry
        result.indices.resize(position_accessor.count);
        for (size_t i = 0; i < position_accessor.count; ++i) {
            result.indices[i] = static_cast<uint32_t>(i);
        }
    }
    
    // Optimize for GPU
    optimizeMeshInPlace(result.vertices, result.indices);
    
    return result;
}

void optimizeMeshInPlace(
    std::vector<MeshVertex>& vertices,
    std::vector<uint32_t>& indices
) {
    size_t index_count = indices.size();
    size_t vertex_count = vertices.size();
    
    // Indexing
    std::vector<uint32_t> remap(vertex_count);
    size_t unique_count = meshopt_generateVertexRemap(
        remap.data(), indices.data(), index_count,
        vertices.data(), vertex_count, sizeof(MeshVertex)
    );
    
    std::vector<uint32_t> new_indices(index_count);
    meshopt_remapIndexBuffer(new_indices.data(), indices.data(), index_count, remap.data());
    
    std::vector<MeshVertex> new_vertices(unique_count);
    meshopt_remapVertexBuffer(new_vertices.data(), vertices.data(), vertex_count,
                              sizeof(MeshVertex), remap.data());
    
    // Optimize
    meshopt_optimizeVertexCache(new_indices.data(), new_indices.data(), index_count, unique_count);
    meshopt_optimizeVertexFetch(new_vertices.data(), new_indices.data(), index_count,
                                new_vertices.data(), unique_count, sizeof(MeshVertex));
    
    vertices = std::move(new_vertices);
    indices = std::move(new_indices);
}
```

---

## Интеграция с flecs ECS

### Компоненты

```cpp
// ECS компоненты для меши
struct StaticMesh {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = VK_NULL_HANDLE;
    VmaAllocation index_allocation = VK_NULL_HANDLE;
    uint32_t index_count = 0;
    uint32_t vertex_count = 0;
};

struct MeshLODs {
    static constexpr size_t MAX_LODS = 4;
    
    StaticMesh lods[MAX_LODS];
    float errors[MAX_LODS] = {};
    size_t lod_count = 0;
};

struct MeshletGeometry {
    VkBuffer meshlets_buffer;
    VkBuffer vertices_buffer;
    VkBuffer triangles_buffer;
    VkBuffer bounds_buffer;
    
    std::vector<meshopt_Bounds> cpu_bounds;
    uint32_t meshlet_count;
};

struct MeshMetrics {
    float acmr = 0.0f;
    float atvr = 0.0f;
    float overdraw = 0.0f;
};
```

### Система оптимизации

```cpp
class MeshOptimizationSystem {
public:
    MeshOptimizationSystem(flecs::world& world, VmaAllocator allocator)
        : allocator_(allocator)
    {
        // Система для создания LOD-ов
        world.system<MeshLODs, const StaticMesh>("CreateLODs")
            .kind(flecs::OnLoad)
            .each([this](flecs::entity e, MeshLODs& lods, const StaticMesh& base_mesh) {
                if (lods.lod_count == 0 && base_mesh.index_count > 300) {
                    generateLODs(e, base_mesh);
                }
            });
        
        // Система для создания meshlets
        world.system<MeshletGeometry, const StaticMesh>("CreateMeshlets")
            .kind(flecs::OnLoad)
            .each([this](flecs::entity e, MeshletGeometry& meshlet, const StaticMesh& mesh) {
                if (meshlet.meshlet_count == 0 && mesh.index_count > 0) {
                    generateMeshlets(e, mesh);
                }
            });
    }

private:
    VmaAllocator allocator_;
    
    void generateLODs(flecs::entity e, const StaticMesh& base_mesh) {
        // Загрузка данных из GPU (или использование кэша)
        // ...
        
        // Генерация LOD-ов
        MeshLODs lods;
        lods.lods[0] = base_mesh;
        lods.lod_count = 1;
        
        // Последовательное упрощение
        std::vector<uint32_t> prev_indices = loadIndices(base_mesh);
        std::vector<MeshVertex> vertices = loadVertices(base_mesh);
        
        size_t target_counts[] = {
            prev_indices.size() * 3 / 4,   // 75%
            prev_indices.size() * 1 / 2,   // 50%
            prev_indices.size() * 1 / 4    // 25%
        };
        
        for (size_t lod = 0; lod < 3; ++lod) {
            std::vector<uint32_t> lod_indices(prev_indices.size());
            float error = 0.0f;
            
            size_t result_count = meshopt_simplify(
                lod_indices.data(),
                prev_indices.data(),
                prev_indices.size(),
                &vertices[0].position[0],
                vertices.size(),
                sizeof(MeshVertex),
                target_counts[lod],
                1e-2f,
                0,
                &error
            );
            
            if (result_count > 0) {
                lod_indices.resize(result_count);
                
                // Оптимизировать и загрузить на GPU
                meshopt_optimizeVertexCache(lod_indices.data(), lod_indices.data(),
                                           result_count, vertices.size());
                
                lods.lods[lods.lod_count] = createMesh(vertices, lod_indices);
                lods.errors[lods.lod_count] = error;
                lods.lod_count++;
                
                prev_indices = lod_indices;
            }
        }
        
        e.set<MeshLODs>(std::move(lods));
    }
    
    void generateMeshlets(flecs::entity e, const StaticMesh& mesh) {
        // Аналогично примеру выше
        // ...
    }
};
```

---

## Интеграция со сжатием

### Сохранение оптимизированной геометрии

```cpp
#include <zstd.h>

struct CompressedMeshData {
    std::vector<uint8_t> encoded_vertices;
    std::vector<uint8_t> encoded_indices;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t vertex_size;
};

CompressedMeshData compressMesh(
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& indices
) {
    CompressedMeshData result;
    result.vertex_count = vertices.size();
    result.index_count = indices.size();
    result.vertex_size = sizeof(MeshVertex);
    
    // Encode vertex buffer
    size_t vertex_bound = meshopt_encodeVertexBufferBound(vertices.size(), sizeof(MeshVertex));
    result.encoded_vertices.resize(vertex_bound);
    result.encoded_vertices.resize(
        meshopt_encodeVertexBuffer(
            result.encoded_vertices.data(), vertex_bound,
            vertices.data(), vertices.size(), sizeof(MeshVertex)
        )
    );
    
    // Encode index buffer
    size_t index_bound = meshopt_encodeIndexBufferBound(indices.size(), vertices.size());
    result.encoded_indices.resize(index_bound);
    result.encoded_indices.resize(
        meshopt_encodeIndexBuffer(
            result.encoded_indices.data(), index_bound,
            indices.data(), indices.size()
        )
    );
    
    // Optional: zstd compression
    // ...
    
    return result;
}

void decompressMesh(
    const CompressedMeshData& compressed,
    std::vector<MeshVertex>& vertices,
    std::vector<uint32_t>& indices
) {
    vertices.resize(compressed.vertex_count);
    indices.resize(compressed.index_count);
    
    int vres = meshopt_decodeVertexBuffer(
        vertices.data(), compressed.vertex_count, compressed.vertex_size,
        compressed.encoded_vertices.data(), compressed.encoded_vertices.size()
    );
    assert(vres == 0);
    
    int ires = meshopt_decodeIndexBuffer(
        indices.data(), compressed.index_count, sizeof(uint32_t),
        compressed.encoded_indices.data(), compressed.encoded_indices.size()
    );
    assert(ires == 0);
}
```

---

## Асинхронная загрузка

```cpp
class AsyncMeshLoader {
public:
    struct LoadRequest {
        std::string path;
        std::promise<OptimizedMesh> promise;
    };
    
    AsyncMeshLoader(VmaAllocator allocator) : allocator_(allocator) {
        worker_thread_ = std::thread([this] { workerLoop(); });
    }
    
    ~AsyncMeshLoader() {
        stop_ = true;
        cv_.notify_all();
        worker_thread_.join();
    }
    
    std::future<OptimizedMesh> loadAsync(const std::string& path) {
        std::promise<OptimizedMesh> promise;
        auto future = promise.get_future();
        
        {
            std::lock_guard lock(mutex_);
            queue_.push({path, std::move(promise)});
        }
        cv_.notify_one();
        
        return future;
    }

private:
    void workerLoop() {
        while (!stop_) {
            LoadRequest request;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                
                if (stop_) break;
                
                request = std::move(queue_.front());
                queue_.pop();
            }
            
            try {
                // Load file
                auto [vertices, indices] = loadMeshFromFile(request.path);
                
                // Optimize (CPU-intensive, но в фоновом потоке)
                auto mesh = createOptimizedMesh(allocator_, vertices, indices);
                
                request.promise.set_value(std::move(mesh));
            } catch (...) {
                request.promise.set_exception(std::current_exception());
            }
        }
    }
    
    VmaAllocator allocator_;
    std::thread worker_thread_;
    std::queue<LoadRequest> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};