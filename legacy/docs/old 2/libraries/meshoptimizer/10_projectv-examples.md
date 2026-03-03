# Примеры кода для ProjectV

🔴 **Уровень 3: Продвинутый**

Полные примеры использования meshoptimizer в ProjectV.

---

## Пример 1: Полный pipeline оптимизации

```cpp
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdio>

// Структура вершины для ProjectV
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Результат оптимизации
struct OptimizedMesh {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = VK_NULL_HANDLE;
    VmaAllocation index_allocation = VK_NULL_HANDLE;
    uint32_t index_count = 0;
    uint32_t vertex_count = 0;
    float acmr = 0.0f;
    float atvr = 0.0f;
};

class MeshOptimizer {
public:
    MeshOptimizer(VmaAllocator allocator, VkDevice device, VkQueue queue, VkCommandPool cmd_pool)
        : allocator_(allocator), device_(device), queue_(queue), cmd_pool_(cmd_pool) {}
    
    OptimizedMesh optimizeAndUpload(
        const std::vector<Vertex>& src_vertices,
        const std::vector<uint32_t>& src_indices
    ) {
        OptimizedMesh result;
        
        // ========== 1. INDEXING ==========
        printf("[MeshOptimizer] Step 1: Indexing...\n");
        
        std::vector<uint32_t> remap(src_vertices.size());
        size_t unique_count = meshopt_generateVertexRemap(
            remap.data(),
            src_indices.data(),
            src_indices.size(),
            src_vertices.data(),
            src_vertices.size(),
            sizeof(Vertex)
        );
        
        printf("  Unique vertices: %zu -> %zu\n", src_vertices.size(), unique_count);
        
        // Применяем remap
        std::vector<uint32_t> indices(src_indices.size());
        meshopt_remapIndexBuffer(indices.data(), src_indices.data(), src_indices.size(), remap.data());
        
        std::vector<Vertex> vertices(unique_count);
        meshopt_remapVertexBuffer(vertices.data(), src_vertices.data(), src_vertices.size(),
                                  sizeof(Vertex), remap.data());
        
        // ========== 2. VERTEX CACHE OPTIMIZATION ==========
        printf("[MeshOptimizer] Step 2: Vertex Cache Optimization...\n");
        
        auto before_vcache = meshopt_analyzeVertexCache(
            indices.data(), indices.size(), unique_count, 16, 0, 0);
        
        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), unique_count);
        
        auto after_vcache = meshopt_analyzeVertexCache(
            indices.data(), indices.size(), unique_count, 16, 0, 0);
        
        printf("  ACMR: %.3f -> %.3f\n", before_vcache.acmr, after_vcache.acmr);
        printf("  ATVR: %.3f -> %.3f\n", before_vcache.atvr, after_vcache.atvr);
        
        result.acmr = after_vcache.acmr;
        result.atvr = after_vcache.atvr;
        
        // ========== 3. VERTEX FETCH OPTIMIZATION ==========
        printf("[MeshOptimizer] Step 3: Vertex Fetch Optimization...\n");
        
        auto before_vfetch = meshopt_analyzeVertexFetch(
            indices.data(), indices.size(), unique_count, sizeof(Vertex));
        
        meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(),
                                    vertices.data(), unique_count, sizeof(Vertex));
        
        auto after_vfetch = meshopt_analyzeVertexFetch(
            indices.data(), indices.size(), unique_count, sizeof(Vertex));
        
        printf("  Overfetch: %.3f -> %.3f\n", before_vfetch.overfetch, after_vfetch.overfetch);
        
        // ========== 4. UPLOAD TO GPU ==========
        printf("[MeshOptimizer] Step 4: Uploading to GPU...\n");
        
        result.vertex_count = static_cast<uint32_t>(unique_count);
        result.index_count = static_cast<uint32_t>(indices.size());
        
        // Vertex buffer
        VkDeviceSize vb_size = vertices.size() * sizeof(Vertex);
        createDeviceLocalBuffer(vb_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vertices.data(), result.vertex_buffer, result.vertex_allocation);
        
        // Index buffer
        VkDeviceSize ib_size = indices.size() * sizeof(uint32_t);
        createDeviceLocalBuffer(ib_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            indices.data(), result.index_buffer, result.index_allocation);
        
        printf("  Vertex buffer: %zu bytes\n", (size_t)vb_size);
        printf("  Index buffer: %zu bytes\n", (size_t)ib_size);
        printf("[MeshOptimizer] Done!\n");
        
        return result;
    }
    
    void destroy(OptimizedMesh& mesh) {
        if (mesh.vertex_buffer) {
            vmaDestroyBuffer(allocator_, mesh.vertex_buffer, mesh.vertex_allocation);
            mesh.vertex_buffer = VK_NULL_HANDLE;
        }
        if (mesh.index_buffer) {
            vmaDestroyBuffer(allocator_, mesh.index_buffer, mesh.index_allocation);
            mesh.index_buffer = VK_NULL_HANDLE;
        }
    }

private:
    VmaAllocator allocator_;
    VkDevice device_;
    VkQueue queue_;
    VkCommandPool cmd_pool_;
    
    void createDeviceLocalBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        const void* data,
        VkBuffer& buffer,
        VmaAllocation& allocation
    ) {
        // Staging buffer
        VkBufferCreateInfo staging_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        staging_info.size = size;
        staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        
        VmaAllocationCreateInfo staging_alloc = {};
        staging_alloc.usage = VMA_MEMORY_USAGE_AUTO;
        staging_alloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | 
                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        
        VkBuffer staging_buffer;
        VmaAllocation staging_allocation;
        VmaAllocationInfo staging_info_result;
        
        vmaCreateBuffer(allocator_, &staging_info, &staging_alloc,
            &staging_buffer, &staging_allocation, &staging_info_result);
        
        memcpy(staging_info_result.pMappedData, data, size);
        
        // Device local buffer
        VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer_info.size = size;
        buffer_info.usage = usage;
        
        VmaAllocationCreateInfo buffer_alloc = {};
        buffer_alloc.usage = VMA_MEMORY_USAGE_AUTO;
        buffer_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        
        vmaCreateBuffer(allocator_, &buffer_info, &buffer_alloc,
            &buffer, &allocation, nullptr);
        
        // Copy command
        VkCommandBuffer cmd = beginSingleTimeCommands();
        
        VkBufferCopy copy = {};
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = size;
        vkCmdCopyBuffer(cmd, staging_buffer, buffer, 1, &copy);
        
        endSingleTimeCommands(cmd);
        
        // Cleanup staging
        vmaDestroyBuffer(allocator_, staging_buffer, staging_allocation);
    }
    
    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc_info.commandPool = cmd_pool_;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device_, &alloc_info, &cmd);
        
        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(cmd, &begin_info);
        return cmd;
    }
    
    void endSingleTimeCommands(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;
        
        vkQueueSubmit(queue_, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue_);
        
        vkFreeCommandBuffers(device_, cmd_pool_, 1, &cmd);
    }
};
```

---

## Пример 2: Генерация LOD цепочки

```cpp
#include <meshoptimizer.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>

struct LODMesh {
    VkBuffer index_buffer;
    VmaAllocation index_allocation;
    uint32_t index_count;
    float error;
};

struct LODChainMesh {
    // Shared vertex buffer
    VkBuffer vertex_buffer;
    VmaAllocation vertex_allocation;
    uint32_t vertex_count;
    
    // Per-LOD index buffers
    std::array<LODMesh, 4> lods;
    size_t lod_count;
};

class LODGenerator {
public:
    LODGenerator(VmaAllocator allocator) : allocator_(allocator) {}
    
    LODChainMesh generateLODs(
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& base_indices,
        const std::vector<float>& lod_ratios = {0.75f, 0.5f, 0.25f, 0.1f}
    ) {
        LODChainMesh result;
        result.vertex_count = static_cast<uint32_t>(vertices.size());
        result.lod_count = 0;
        
        // Создаём общий vertex buffer
        createVertexBuffer(vertices, result.vertex_buffer, result.vertex_allocation);
        
        // LOD 0: исходный меш
        result.lods[0].index_count = static_cast<uint32_t>(base_indices.size());
        result.lods[0].error = 0.0f;
        createIndexBuffer(base_indices, result.lods[0].index_buffer, result.lods[0].index_allocation);
        result.lod_count = 1;
        
        // Генерируем последующие LOD-ы
        std::vector<uint32_t> prev_indices = base_indices;
        float accumulated_error = 0.0f;
        
        for (size_t i = 1; i < lod_ratios.size(); ++i) {
            size_t target_count = base_indices.size() * lod_ratios[i];
            target_count = std::max(target_count, size_t(12));  // Минимум 4 треугольника
            
            std::vector<uint32_t> lod_indices(prev_indices.size());
            float lod_error = 0.0f;
            
            size_t result_count = meshopt_simplify(
                lod_indices.data(),
                prev_indices.data(),
                prev_indices.size(),
                &vertices[0].position.x,
                vertices.size(),
                sizeof(Vertex),
                target_count,
                1e-2f,  // 1% максимальная ошибка
                meshopt_SimplifyLockBorder,
                &lod_error
            );
            
            if (result_count < 12) break;  // Слишком мало треугольников
            
            lod_indices.resize(result_count);
            
            // Оптимизируем для vertex cache
            meshopt_optimizeVertexCache(
                lod_indices.data(),
                lod_indices.data(),
                result_count,
                vertices.size()
            );
            
            // Создаём index buffer
            result.lods[i].index_count = static_cast<uint32_t>(result_count);
            result.lods[i].error = accumulated_error + lod_error;
            createIndexBuffer(lod_indices, result.lods[i].index_buffer, result.lods[i].index_allocation);
            
            accumulated_error += lod_error;
            prev_indices = std::move(lod_indices);
            result.lod_count++;
        }
        
        return result;
    }
    
    size_t selectLOD(const LODChainMesh& mesh, float distance, float screen_height, float fov) {
        const float pixel_threshold = 1.5f;
        float scale = screen_height / (distance * std::tan(fov / 2));
        
        for (size_t i = 1; i < mesh.lod_count; ++i) {
            float screen_error = mesh.lods[i].error * scale;
            if (screen_error < pixel_threshold) {
                return i;
            }
        }
        return mesh.lod_count - 1;
    }

private:
    VmaAllocator allocator_;
    
    void createVertexBuffer(const std::vector<Vertex>& vertices,
                           VkBuffer& buffer, VmaAllocation& allocation) {
        VkDeviceSize size = vertices.size() * sizeof(Vertex);
        
        VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        
        VmaAllocationCreateInfo alloc = {};
        alloc.usage = VMA_MEMORY_USAGE_AUTO;
        alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        
        vmaCreateBuffer(allocator_, &info, &alloc, &buffer, &allocation, nullptr);
        // Upload data...
    }
    
    void createIndexBuffer(const std::vector<uint32_t>& indices,
                          VkBuffer& buffer, VmaAllocation& allocation) {
        VkDeviceSize size = indices.size() * sizeof(uint32_t);
        
        VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        
        VmaAllocationCreateInfo alloc = {};
        alloc.usage = VMA_MEMORY_USAGE_AUTO;
        alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        
        vmaCreateBuffer(allocator_, &info, &alloc, &buffer, &allocation, nullptr);
        // Upload data...
    }
};
```

---

## Пример 3: Meshlet Pipeline

```cpp
#include <meshoptimizer.h>
#include <glm/glm.hpp>
#include <vector>

struct MeshletData {
    // CPU data
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<uint32_t> meshlet_vertices;
    std::vector<uint8_t> meshlet_triangles;
    std::vector<meshopt_Bounds> bounds;
    
    // GPU buffers
    VkBuffer meshlets_buffer;
    VkBuffer vertices_buffer;
    VkBuffer triangles_buffer;
    VkBuffer bounds_buffer;
    VmaAllocation allocations[4];
    uint32_t meshlet_count;
};

class MeshletBuilder {
public:
    MeshletBuilder(VmaAllocator allocator) : allocator_(allocator) {}
    
    MeshletData buildMeshlets(
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        bool for_raytracing = false
    ) {
        const size_t max_vertices = 64;
        const size_t max_triangles = for_raytracing ? 64 : 126;
        const float cone_weight = 0.25f;
        
        MeshletData result;
        
        size_t max_meshlets = meshopt_buildMeshletsBound(
            indices.size(), max_vertices, max_triangles);
        
        result.meshlets.resize(max_meshlets);
        result.meshlet_vertices.resize(max_meshlets * max_vertices);
        result.meshlet_triangles.resize(max_meshlets * max_triangles * 3);
        
        size_t meshlet_count;
        
        if (for_raytracing) {
            meshlet_count = meshopt_buildMeshletsSpatial(
                result.meshlets.data(),
                result.meshlet_vertices.data(),
                result.meshlet_triangles.data(),
                indices.data(),
                indices.size(),
                &vertices[0].position.x,
                vertices.size(),
                sizeof(Vertex),
                max_vertices,
                16,  // min_triangles
                max_triangles,
                0.5f  // fill_weight
            );
        } else {
            meshlet_count = meshopt_buildMeshlets(
                result.meshlets.data(),
                result.meshlet_vertices.data(),
                result.meshlet_triangles.data(),
                indices.data(),
                indices.size(),
                &vertices[0].position.x,
                vertices.size(),
                sizeof(Vertex),
                max_vertices,
                max_triangles,
                cone_weight
            );
        }
        
        result.meshlets.resize(meshlet_count);
        result.meshlet_count = static_cast<uint32_t>(meshlet_count);
        
        // Trim arrays
        const auto& last = result.meshlets.back();
        result.meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
        result.meshlet_triangles.resize(last.triangle_offset + last.triangle_count * 3);
        
        // Optimize each meshlet
        for (auto& m : result.meshlets) {
            meshopt_optimizeMeshlet(
                &result.meshlet_vertices[m.vertex_offset],
                &result.meshlet_triangles[m.triangle_offset],
                m.triangle_count,
                m.vertex_count
            );
        }
        
        // Compute bounds
        result.bounds.resize(meshlet_count);
        for (size_t i = 0; i < meshlet_count; ++i) {
            const auto& m = result.meshlets[i];
            result.bounds[i] = meshopt_computeMeshletBounds(
                &result.meshlet_vertices[m.vertex_offset],
                &result.meshlet_triangles[m.triangle_offset],
                m.triangle_count,
                &vertices[0].position.x,
                vertices.size(),
                sizeof(Vertex)
            );
        }
        
        // Upload to GPU
        uploadToGPU(result);
        
        return result;
    }
    
    void destroy(MeshletData& data) {
        for (int i = 0; i < 4; ++i) {
            if (data.allocations[i]) {
                vmaDestroyBuffer(allocator_, 
                    i == 0 ? data.meshlets_buffer :
                    i == 1 ? data.vertices_buffer :
                    i == 2 ? data.triangles_buffer : data.bounds_buffer,
                    data.allocations[i]);
            }
        }
    }

private:
    VmaAllocator allocator_;
    
    void uploadToGPU(MeshletData& data) {
        // Создание буферов для meshlets, vertices, triangles, bounds
        // ...
    }
};

// Culling функции
namespace MeshletCulling {

bool frustumCull(const meshopt_Bounds& bounds, const glm::vec4 frustum_planes[6]) {
    glm::vec3 center(bounds.center[0], bounds.center[1], bounds.center[2]);
    
    for (int i = 0; i < 6; ++i) {
        float distance = glm::dot(glm::vec3(frustum_planes[i]), center) + frustum_planes[i].w;
        if (distance < -bounds.radius) {
            return true;  // Culled
        }
    }
    return false;
}

bool backfaceCull(const meshopt_Bounds& bounds, const glm::vec3& camera_pos) {
    glm::vec3 center(bounds.center[0], bounds.center[1], bounds.center[2]);
    glm::vec3 axis(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]);
    
    glm::vec3 view_dir = glm::normalize(center - camera_pos);
    float d = glm::dot(view_dir, axis);
    
    return d >= bounds.cone_cutoff;
}

std::vector<uint32_t> getVisibleMeshlets(
    const MeshletData& data,
    const glm::vec4 frustum_planes[6],
    const glm::vec3& camera_pos
) {
    std::vector<uint32_t> visible;
    visible.reserve(data.meshlet_count);
    
    for (uint32_t i = 0; i < data.meshlet_count; ++i) {
        if (frustumCull(data.bounds[i], frustum_planes)) continue;
        if (backfaceCull(data.bounds[i], camera_pos)) continue;
        
        visible.push_back(i);
    }
    
    return visible;
}

}  // namespace MeshletCulling
```

---

## Пример 4: Сжатие и загрузка

```cpp
#include <meshoptimizer.h>
#include <vector>
#include <fstream>

struct CompressedMeshFile {
    // Header
    uint32_t magic = 0x4D455348;  // "MESH"
    uint32_t version = 1;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t vertex_size;
    uint32_t vertex_data_size;
    uint32_t index_data_size;
    
    // Data follows after header
};

class MeshCompressor {
public:
    bool saveCompressed(
        const std::string& path,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices
    ) {
        // Encode vertex buffer
        size_t vb_bound = meshopt_encodeVertexBufferBound(vertices.size(), sizeof(Vertex));
        std::vector<uint8_t> encoded_vertices(vb_bound);
        size_t vb_size = meshopt_encodeVertexBuffer(
            encoded_vertices.data(), vb_bound,
            vertices.data(), vertices.size(), sizeof(Vertex)
        );
        encoded_vertices.resize(vb_size);
        
        // Encode index buffer
        size_t ib_bound = meshopt_encodeIndexBufferBound(indices.size(), vertices.size());
        std::vector<uint8_t> encoded_indices(ib_bound);
        size_t ib_size = meshopt_encodeIndexBuffer(
            encoded_indices.data(), ib_bound,
            indices.data(), indices.size()
        );
        encoded_indices.resize(ib_size);
        
        // Write file
        CompressedMeshFile header;
        header.vertex_count = vertices.size();
        header.index_count = indices.size();
        header.vertex_size = sizeof(Vertex);
        header.vertex_data_size = vb_size;
        header.index_data_size = ib_size;
        
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(encoded_vertices.data()), vb_size);
        file.write(reinterpret_cast<const char*>(encoded_indices.data()), ib_size);
        
        printf("Saved: %u vertices, %u indices\n", header.vertex_count, header.index_count);
        printf("Compressed: %zu bytes (vertices) + %zu bytes (indices)\n", vb_size, ib_size);
        printf("Original: %zu bytes (vertices) + %zu bytes (indices)\n",
            vertices.size() * sizeof(Vertex), indices.size() * sizeof(uint32_t));
        
        return true;
    }
    
    bool loadCompressed(
        const std::string& path,
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices
    ) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        
        CompressedMeshFile header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        if (header.magic != 0x4D455348 || header.version != 1) {
            return false;
        }
        
        // Read encoded data
        std::vector<uint8_t> encoded_vertices(header.vertex_data_size);
        std::vector<uint8_t> encoded_indices(header.index_data_size);
        
        file.read(reinterpret_cast<char*>(encoded_vertices.data()), header.vertex_data_size);
        file.read(reinterpret_cast<char*>(encoded_indices.data()), header.index_data_size);
        
        // Decode
        vertices.resize(header.vertex_count);
        indices.resize(header.index_count);
        
        int vres = meshopt_decodeVertexBuffer(
            vertices.data(), header.vertex_count, header.vertex_size,
            encoded_vertices.data(), encoded_vertices.size()
        );
        
        if (vres != 0) {
            printf("Vertex decode error: %d\n", vres);
            return false;
        }
        
        int ires = meshopt_decodeIndexBuffer(
            indices.data(), header.index_count, sizeof(uint32_t),
            encoded_indices.data(), encoded_indices.size()
        );
        
        if (ires != 0) {
            printf("Index decode error: %d\n", ires);
            return false;
        }
        
        return true;
    }
    
    void printCompressionStats(
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices
    ) {
        size_t original_vb = vertices.size() * sizeof(Vertex);
        size_t original_ib = indices.size() * sizeof(uint32_t);
        
        size_t vb_bound = meshopt_encodeVertexBufferBound(vertices.size(), sizeof(Vertex));
        size_t ib_bound = meshopt_encodeIndexBufferBound(indices.size(), vertices.size());
        
        std::vector<uint8_t> encoded_vb(vb_bound);
        std::vector<uint8_t> encoded_ib(ib_bound);
        
        size_t vb_size = meshopt_encodeVertexBuffer(
            encoded_vb.data(), vb_bound, vertices.data(), vertices.size(), sizeof(Vertex));
        size_t ib_size = meshopt_encodeIndexBuffer(
            encoded_ib.data(), ib_bound, indices.data(), indices.size());
        
        printf("=== Compression Stats ===\n");
        printf("Vertices: %zu (%zu bytes)\n", vertices.size(), original_vb);
        printf("Triangles: %zu (%zu bytes)\n", indices.size() / 3, original_ib);
        printf("\n");
        printf("Vertex compression: %zu -> %zu bytes (%.1f%%)\n",
            original_vb, vb_size, 100.0f * vb_size / original_vb);
        printf("Index compression: %zu -> %zu bytes (%.1f%%)\n",
            original_ib, ib_size, 100.0f * ib_size / original_ib);
        printf("Total: %zu -> %zu bytes (%.1f%%)\n",
            original_vb + original_ib, vb_size + ib_size,
            100.0f * (vb_size + ib_size) / (original_vb + original_ib));
    }
};
```

---

## Пример 5: Интеграция с flecs

```cpp
#include <flecs.h>
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>

// Components
ECS_COMPONENT_DECLARE(StaticMesh);
ECS_COMPONENT_DECLARE(MeshLODs);
ECS_COMPONENT_DECLARE(MeshletGeometry);
ECS_COMPONENT_DECLARE(MeshMetrics);

// Systems
void OptimizeMeshSystem(ecs_iter_t* it) {
    StaticMesh* mesh = ecs_field(it, StaticMesh, 1);
    MeshMetrics* metrics = ecs_field(it, MeshMetrics, 2);
    
    for (int i = 0; i < it->count; i++) {
        if (mesh[i].vertex_buffer == VK_NULL_HANDLE) continue;
        if (metrics[i].acmr > 0) continue;  // Already optimized
        
        // Get mesh data from GPU (simplified)
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        downloadMeshFromGPU(mesh[i], vertices, indices);
        
        // Optimize
        meshopt_optimizeVertexCache(
            indices.data(), indices.data(), indices.size(), vertices.size());
        meshopt_optimizeVertexFetch(
            vertices.data(), indices.data(), indices.size(),
            vertices.data(), vertices.size(), sizeof(Vertex));
        
        // Update metrics
        auto stats = meshopt_analyzeVertexCache(
            indices.data(), indices.size(), vertices.size(), 16, 0, 0);
        metrics[i].acmr = stats.acmr;
        metrics[i].atvr = stats.atvr;
        
        // Re-upload to GPU
        updateMeshOnGPU(mesh[i], vertices, indices);
    }
}

void GenerateLODSystem(ecs_iter_t* it) {
    StaticMesh* mesh = ecs_field(it, StaticMesh, 1);
    MeshLODs* lods = ecs_field(it, MeshLODs, 2);
    
    for (int i = 0; i < it->count; i++) {
        if (mesh[i].index_count < 300) continue;
        if (lods[i].lod_count > 0) continue;
        
        // Generate LODs...
        lods[i].lod_count = 4;
        // ...
    }
}

void MeshletCullingSystem(ecs_iter_t* it) {
    MeshletGeometry* meshlet = ecs_field(it, MeshletGeometry, 1);
    
    // Get camera data from singleton
    const CameraData* camera = ecs_singleton_get(it->world, CameraData);
    
    for (int i = 0; i < it->count; i++) {
        // Cull meshlets
        auto visible = MeshletCulling::getVisibleMeshlets(
            meshlet[i],
            camera->frustum_planes,
            camera->position
        );
        
        // Update draw commands
        meshlet[i].visible_count = visible.size();
    }
}

// Module initialization
void MeshOptimizerModuleImport(ecs_world_t* world) {
    ECS_MODULE(world, MeshOptimizerModule);
    
    ECS_COMPONENT_DEFINE(world, StaticMesh);
    ECS_COMPONENT_DEFINE(world, MeshLODs);
    ECS_COMPONENT_DEFINE(world, MeshletGeometry);
    ECS_COMPONENT_DEFINE(world, MeshMetrics);
    
    // Systems
    ECS_SYSTEM(world, OptimizeMeshSystem, EcsOnLoad, StaticMesh, MeshMetrics);
    ECS_SYSTEM(world, GenerateLODSystem, EcsOnLoad, StaticMesh, MeshLODs);
    ECS_SYSTEM(world, MeshletCullingSystem, EcsPreUpdate, MeshletGeometry);
}