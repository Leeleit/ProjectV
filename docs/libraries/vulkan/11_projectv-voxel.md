# Vulkan ProjectV Voxel Rendering

Воксельный рендеринг в ProjectV: Mesh Shaders как main path, SVO, GPU-driven rendering.

---

## Почему Mesh Shaders — main path

### Сравнение подходов

| Метрика             | Traditional (VS+FS) | Compute + Indirect  | Mesh Shaders |
|---------------------|---------------------|---------------------|--------------|
| Draw calls          | 1000+               | 1 (indirect)        | 1            |
| GPU-CPU sync        | Нет                 | Да (indirect count) | Нет          |
| Geometry generation | CPU                 | Compute shader      | Mesh Shader  |
| Culling             | CPU/Compute         | Compute             | Task Shader  |
| Memory bandwidth    | Высокая             | Средняя             | Низкая       |
| LOD switching       | CPU                 | Compute             | Task Shader  |

### Преимущества для вокселей

1. **GPU-only pipeline** — геометрия генерируется и потребляется на GPU
2. **Amplification** — Task Shader спавнит Mesh Shaders по необходимости
3. **Culling до rasterization** — Task Shader отсекает невидимые чанки
4. **LOD на GPU** — выбор уровня детализации без CPU involvement

---

## Архитектура воксельного рендерера

### Общая схема

```
┌─────────────────────────────────────────────────────────────┐
│                    Voxel World Data                          │
│  Chunks: 32x32x32 voxels each                                │
│  Materials: bindless texture array                           │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                   GPU Culling Pass                           │
│  Compute: Frustum + Occlusion culling                        │
│  Output: Visible chunk list, LOD levels                      │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                   Mesh Shader Pass                           │
│  Task Shader: Per-chunk culling, LOD selection               │
│  Mesh Shader: Greedy meshing, vertex output                  │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                   Fragment Shader                            │
│  Material lookup, lighting, fog                              │
└─────────────────────────────────────────────────────────────┘
```

### Данные чанка

```cpp
struct VoxelChunk {
    // Позиция в мире
    glm::ivec3 worldPos;
    uint32_t lodLevel;

    // GPU буфер с вокселями
    VkBuffer voxelBuffer;
    VkDeviceAddress voxelAddress;  // BDA

    // Метаданные
    uint32_t voxelCount;
    uint32_t flags;  // Empty, full, modified
    float boundingRadius;
};

struct ChunkData {
    glm::vec4 bounds;      // xyz = center, w = radius
    uint32_t voxelBuffer;  // Index in bindless array
    uint32_t lodLevel;
    uint32_t flags;
    uint32_t padding;
};
```

---

## Chunk System

### Хранение вокселей

```cpp
class VoxelChunkManager {
    static constexpr uint32_t CHUNK_SIZE = 32;
    static constexpr uint32_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

    struct Chunk {
        std::array<uint32_t, CHUNK_VOLUME / 4> packedVoxels;  // 4 voxels per uint32
        glm::ivec3 position;
        uint8_t lod;
        bool dirty;
    };

    std::unordered_map<glm::ivec3, Chunk, IVec3Hash> chunks_;

public:
    // Упаковка вокселя: 8-bit type + 4-bit light + 4-bit flags
    uint32_t packVoxel(uint8_t type, uint4 light, uint4 flags) {
        return type | (light << 8) | (flags << 12);
    }

    // Upload на GPU
    GPUBuffer uploadChunk(VmaAllocator allocator, const Chunk& chunk) {
        GPUBuffer buffer;

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = chunk.packedVoxels.size() * sizeof(uint32_t);
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                       &buffer.buffer, &buffer.allocation, nullptr);

        // Staging upload...

        // Get BDA
        VkBufferDeviceAddressInfo addrInfo = {};
        addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addrInfo.buffer = buffer.buffer;
        buffer.address = vkGetBufferDeviceAddress(device, &addrInfo);

        return buffer;
    }
};
```

### Bindless Voxel Buffers

```cpp
// Все чанки доступны через один дескриптор
layout(std430, set = 0, binding = 0) readonly buffer VoxelBuffers {
    uint voxelData[];  // Все чанки подряд
};

layout(std430, set = 0, binding = 1) readonly buffer ChunkMetadata {
    ChunkInfo chunks[];
};

struct ChunkInfo {
    uint32_t dataOffset;    // Offset в voxelData
    uint32_t dataSize;
    vec3 worldPos;
    uint32_t lodLevel;
    vec4 bounds;  // xyz = center, w = radius
};
```

---

## GPU-Driven Culling

### Compute Shader Culling

```glsl
#version 460

layout(local_size_x = 64) in;

struct ChunkInfo {
    uint32_t dataOffset;
    uint32_t dataSize;
    vec3 worldPos;
    uint32_t lodLevel;
    vec4 bounds;
};

layout(std430, binding = 0) readonly buffer InputChunks {
    ChunkInfo inputChunks[];
};

layout(std430, binding = 1) buffer OutputChunks {
    uint32_t visibleChunkIndices[];
};

layout(std430, binding = 2) buffer Counters {
    uint32_t visibleCount;
    uint32_t totalCount;
};

uniform mat4 viewProjection;
uniform vec3 cameraPosition;
uniform float lodDistances[4];

bool frustumCull(vec3 center, float radius, mat4 vp) {
    vec4 clip = vp * vec4(center, 1.0);
    float w = clip.w;

    return clip.x > -w - radius && clip.x < w + radius &&
           clip.y > -w - radius && clip.y < w + radius &&
           clip.z > -w - radius && clip.z < w + radius;
}

uint selectLOD(vec3 chunkPos, vec3 cameraPos, uint currentLOD) {
    float dist = distance(chunkPos, cameraPos);

    uint newLOD = 0;
    for (uint i = 0; i < 4; i++) {
        if (dist < lodDistances[i]) {
            newLOD = i;
            break;
        }
        newLOD = 3;
    }

    // Don't increase LOD beyond available
    return min(newLOD, currentLOD);
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= totalCount) return;

    ChunkInfo chunk = inputChunks[idx];

    // Skip empty chunks
    if (chunk.dataSize == 0) return;

    // Frustum culling
    if (!frustumCull(chunk.bounds.xyz, chunk.bounds.w, viewProjection)) {
        return;
    }

    // LOD selection
    uint lod = selectLOD(chunk.worldPos, cameraPosition, chunk.lodLevel);

    // Add to visible list
    uint visibleIdx = atomicAdd(visibleCount, 1);
    visibleChunkIndices[visibleIdx] = idx;
}
```

---

## Mesh Shader Pipeline

### Task Shader

```
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;

taskPayloadSharedEXT struct TaskPayload {
    uint chunkCount;
    uint chunkIndices[32];
    uint lodLevels[32];
} payload;

layout(std430, binding = 0) readonly buffer VisibleChunks {
    uint visibleChunkIndices[];
};

layout(std430, binding = 1) readonly buffer ChunkMetadata {
    ChunkInfo chunks[];
};

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec3 cameraPosition;
    uint visibleCount;
    float lodDistances[4];
} pc;

bool frustumCull(vec3 center, float radius) {
    vec4 clip = pc.viewProjection * vec4(center, 1.0);
    float w = clip.w;
    return clip.x > -w - radius && clip.x < w + radius &&
           clip.y > -w - radius && clip.y < w + radius &&
           clip.z > -w - radius && clip.z < w + radius;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;

    if (idx >= pc.visibleCount) return;

    uint chunkIdx = visibleChunkIndices[idx];
    ChunkInfo chunk = chunks[chunkIdx];

    // Additional culling (occlusion, distance)
    if (!frustumCull(chunk.bounds.xyz, chunk.bounds.w)) {
        return;
    }

    // LOD selection
    float dist = distance(chunk.worldPos, pc.cameraPosition);
    uint lod = 0;
    for (uint i = 0; i < 4; i++) {
        if (dist < pc.lodDistances[i]) {
            lod = i;
            break;
        }
        lod = 3;
    }
    lod = min(lod, chunk.lodLevel);

    // Add to payload
    uint payloadIdx = atomicAdd(payload.chunkCount, 1);
    payload.chunkIndices[payloadIdx] = chunkIdx;
    payload.lodLevels[payloadIdx] = lod;

    barrier();

    // First invocation emits mesh tasks
    if (gl_LocalInvocationIndex == 0 && payload.chunkCount > 0) {
        EmitMeshTasksEXT(payload.chunkCount, 1, 1);
    }
}
```

### Mesh Shader

```
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(triangles) out;
layout(max_vertices = 256, max_primitives = 128) out;

taskPayloadSharedEXT struct TaskPayload {
    uint chunkCount;
    uint chunkIndices[32];
    uint lodLevels[32];
} payload;

out gl_MeshPerVertexEXT {
    vec4 gl_Position;
} gl_MeshVerticesEXT[];

layout(location = 0) out vec2 outUV[];
layout(location = 1) out vec3 outNormal[];
layout(location = 2) out flat uint outMaterial[];
layout(location = 3) out vec3 outWorldPos[];

layout(std430, binding = 2) readonly buffer VoxelData {
    uint packedVoxels[];
};

layout(std430, binding = 3) readonly buffer ChunkMetadata {
    ChunkInfo chunks[];
};

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    mat4 invViewProjection;
    vec3 cameraPosition;
} pc;

const uint CHUNK_SIZE = 32;

uint getVoxel(uint chunkIdx, uint x, uint y, uint z) {
    ChunkInfo chunk = chunks[chunkIdx];
    uint localIdx = x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;

    // Bounds check
    if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE) return 0;

    // Read from packed data
    uint packedIdx = chunk.dataOffset + localIdx / 4;
    uint packed = packedVoxels[packedIdx];

    // Extract voxel
    uint shift = (localIdx % 4) * 8;
    return (packed >> shift) & 0xFF;
}

bool isFaceVisible(uint chunkIdx, uint x, uint y, uint z, int dx, int dy, int dz) {
    // Boundary check
    if (x + dx < 0 || x + dx >= CHUNK_SIZE ||
        y + dy < 0 || y + dy >= CHUNK_SIZE ||
        z + dz < 0 || z + dz >= CHUNK_SIZE) {
        // Check neighbor chunk or assume visible
        return true;
    }

    uint neighbor = getVoxel(chunkIdx, x + dx, y + dy, z + dz);
    return neighbor == 0;  // Air = visible face
}

void emitQuad(inout uint vertexIdx, inout uint primIdx,
              vec3 pos, vec3 normal, uint material, uint size) {
    vec3 tangent, bitangent;

    if (abs(normal.x) > 0.5) {
        tangent = vec3(0, 1, 0);
        bitangent = vec3(0, 0, 1);
    } else if (abs(normal.y) > 0.5) {
        tangent = vec3(1, 0, 0);
        bitangent = vec3(0, 0, 1);
    } else {
        tangent = vec3(1, 0, 0);
        bitangent = vec3(0, 1, 0);
    }

    // 4 vertices
    vec3 positions[4] = {
        pos,
        pos + tangent * float(size),
        pos + tangent * float(size) + bitangent * float(size),
        pos + bitangent * float(size)
    };

    vec2 uvs[4] = {
        vec2(0, 0),
        vec2(float(size), 0),
        vec2(float(size), float(size)),
        vec2(0, float(size))
    };

    ChunkInfo chunk = chunks[payload.chunkIndices[gl_WorkGroupID.x]];
    vec3 chunkWorldPos = chunk.worldPos;

    for (uint i = 0; i < 4; i++) {
        vec3 worldPos = chunkWorldPos + positions[i];
        gl_MeshVerticesEXT[vertexIdx + i].gl_Position =
            pc.viewProjection * vec4(worldPos, 1.0);
        outUV[vertexIdx + i] = uvs[i];
        outNormal[vertexIdx + i] = normal;
        outMaterial[vertexIdx + i] = material;
        outWorldPos[vertexIdx + i] = worldPos;
    }

    // 2 triangles
    gl_PrimitiveTriangleIndicesEXT[primIdx] =
        uvec3(vertexIdx, vertexIdx + 1, vertexIdx + 2);
    gl_PrimitiveTriangleIndicesEXT[primIdx + 1] =
        uvec3(vertexIdx, vertexIdx + 2, vertexIdx + 3);

    vertexIdx += 4;
    primIdx += 2;
}

void main() {
    uint taskIdx = gl_WorkGroupID.x;
    uint chunkIdx = payload.chunkIndices[taskIdx];
    uint lod = payload.lodLevels[taskIdx];

    uint vertexIdx = 0;
    uint primIdx = 0;

    uint step = 1u << lod;  // LOD step: 1, 2, 4, 8

    // Iterate voxels
    for (uint z = 0; z < CHUNK_SIZE; z += step) {
        for (uint y = 0; y < CHUNK_SIZE; y += step) {
            for (uint x = 0; x < CHUNK_SIZE; x += step) {
                uint voxel = getVoxel(chunkIdx, x, y, z);
                if (voxel == 0) continue;  // Air

                vec3 pos = vec3(x, y, z);

                // Check 6 faces
                if (isFaceVisible(chunkIdx, x, y, z, -1, 0, 0))
                    emitQuad(vertexIdx, primIdx, pos, vec3(-1, 0, 0), voxel, step);
                if (isFaceVisible(chunkIdx, x, y, z, 1, 0, 0))
                    emitQuad(vertexIdx, primIdx, pos + vec3(step, 0, 0), vec3(1, 0, 0), voxel, step);
                if (isFaceVisible(chunkIdx, x, y, z, 0, -1, 0))
                    emitQuad(vertexIdx, primIdx, pos, vec3(0, -1, 0), voxel, step);
                if (isFaceVisible(chunkIdx, x, y, z, 0, 1, 0))
                    emitQuad(vertexIdx, primIdx, pos + vec3(0, step, 0), vec3(0, 1, 0), voxel, step);
                if (isFaceVisible(chunkIdx, x, y, z, 0, 0, -1))
                    emitQuad(vertexIdx, primIdx, pos, vec3(0, 0, -1), voxel, step);
                if (isFaceVisible(chunkIdx, x, y, z, 0, 0, 1))
                    emitQuad(vertexIdx, primIdx, pos + vec3(0, 0, step), vec3(0, 0, 1), voxel, step);
            }
        }
    }

    SetMeshOutputsEXT(vertexIdx, primIdx);
}
```

### Fragment Shader

```glsl
#version 460

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in flat uint inMaterial;
layout(location = 3) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 4) uniform sampler2D textures[];

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec3 cameraPosition;
    vec3 lightDirection;
    float time;
} pc;

void main() {
    // Material lookup (bindless)
    uint albedoIndex = inMaterial * 4 + 0;
    uint normalIndex = inMaterial * 4 + 1;

    vec4 albedo = texture(textures[nonuniformEXT(albedoIndex)], inUV);

    // Simple directional lighting
    vec3 N = normalize(inNormal);
    vec3 L = normalize(pc.lightDirection);
    float NdotL = max(dot(N, L), 0.0);

    // Ambient
    vec3 ambient = albedo.rgb * 0.3;

    // Diffuse
    vec3 diffuse = albedo.rgb * NdotL;

    // Distance fog
    float dist = length(inWorldPos - pc.cameraPosition);
    float fogFactor = 1.0 - exp(-dist * 0.001);
    vec3 fogColor = vec3(0.5, 0.6, 0.7);

    vec3 color = ambient + diffuse;
    color = mix(color, fogColor, fogFactor);

    outColor = vec4(color, albedo.a);
}
```

---

## LOD System

### LOD Distances

```cpp
struct LODConfig {
    float distances[4];  // Distance thresholds
    uint32_t chunkSizes[4];  // Effective size at each LOD

    static LODConfig createDefault(float baseDistance = 64.0f) {
        LODConfig config;
        for (int i = 0; i < 4; i++) {
            config.distances[i] = baseDistance * (1 << i);
            config.chunkSizes[i] = CHUNK_SIZE >> i;
        }
        return config;
    }
};
```

### LOD Transition

```
// Smooth LOD transition via dithering
float ditherThreshold = interleavedGradientNoise(gl_FragCoord.xy);

if (lodBlend > 0.0 && lodBlend < 1.0) {
    float dither = interleavedGradientNoise(gl_FragCoord.xy + pc.time);
    if (dither < lodBlend) {
        // Use higher LOD
    }
}
```

---

## Fallback Strategy

### Иерархия рендеринга

```cpp
enum class VoxelRenderPath {
    MeshShaders,        // Preferred: VK_EXT_mesh_shader
    ComputeIndirect,    // Fallback: Compute + vkCmdDrawIndexedIndirect
    Traditional,        // Last resort: CPU mesh generation
};

VoxelRenderPath selectRenderPath(VkPhysicalDevice device) {
    // Check mesh shader support
    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures = {};
    meshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &meshFeatures;

    vkGetPhysicalDeviceFeatures2(device, &features2);

    if (meshFeatures.meshShader && meshFeatures.taskShader) {
        return VoxelRenderPath::MeshShaders;
    }

    // Fallback to compute
    return VoxelRenderPath::ComputeIndirect;
}
```

### Compute Indirect Fallback

```cpp
class ComputeIndirectRenderer {
    // Compute shader generates vertices
    // vkCmdDrawIndexedIndirect draws result

    void render(VkCommandBuffer cmd, const VoxelWorld& world) {
        // 1. Compute: Generate mesh
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, meshGenPipeline_);
        vkCmdDispatch(cmd, workgroups, 1, 1);

        // 2. Barrier
        VkMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);

        // 3. Draw
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipeline_);
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
        vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, indirectBuffer_, 0, drawCount, sizeof(VkDrawIndexedIndirectCommand));
    }
};
```

---

## Ключевые принципы

1. **Mesh Shaders — main path** — минимальный CPU involvement
2. **BDA для voxel data** — прямой доступ к памяти чанков
3. **GPU culling** — frustum + occlusion в compute/task shader
4. **LOD на GPU** — Task Shader выбирает уровень детализации
5. **Fallback обязателен** — Compute Indirect для старого железа
