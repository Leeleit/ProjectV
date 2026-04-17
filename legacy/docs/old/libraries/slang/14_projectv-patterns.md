# Slang в ProjectV: Паттерны

**🔴 Уровень 3: Продвинутый** — Специфичные паттерны шейдеров для воксельного движка ProjectV.

---

## Паттерн 1: Generic Voxel Chunk

Параметризованный шейдер для работы с разными типами вокселей.

```slang
// shaders/voxel/data/chunk.slang
module VoxelChunk;

import Types;

// Интерфейс вокселя
interface IVoxel
{
    float getDensity();
    float3 getColor();
    uint getMaterialId();
    bool isEmpty();
};

// Generic чанк
generic<TVoxel>
struct VoxelChunk where TVoxel : IVoxel
{
    static const uint SIZE = 32;
    static const uint VOLUME = SIZE * SIZE * SIZE;

    [[vk::buffer_reference]]
    TVoxel* voxels;

    float3 worldOrigin;
    float voxelSize;
    uint lodLevel;

    TVoxel sample(uint3 localPos)
    {
        uint index = localPos.z * SIZE * SIZE + localPos.y * SIZE + localPos.x;
        return voxels[index];
    }

    TVoxel sampleLinear(float3 localPos)
    {
        float3 p = localPos - 0.5;
        uint3 base = uint3(floor(p));
        float3 f = fract(p);

        TVoxel v000 = sample(base + uint3(0, 0, 0));
        TVoxel v001 = sample(base + uint3(0, 0, 1));
        TVoxel v010 = sample(base + uint3(0, 1, 0));
        TVoxel v011 = sample(base + uint3(0, 1, 1));
        TVoxel v100 = sample(base + uint3(1, 0, 0));
        TVoxel v101 = sample(base + uint3(1, 0, 1));
        TVoxel v110 = sample(base + uint3(1, 1, 0));
        TVoxel v111 = sample(base + uint3(1, 1, 1));

        // Trilinear interpolation
        return trilerp(v000, v001, v010, v011,
                       v100, v101, v110, v111, f);
    }

    float3 localToWorld(uint3 localPos)
    {
        return worldOrigin + float3(localPos) * voxelSize;
    }

    uint3 worldToLocal(float3 worldPos)
    {
        return uint3((worldPos - worldOrigin) / voxelSize);
    }
};
```

---

## Паттерн 2: GPU Voxel Culling

Многоуровневое отсечение воксельных чанков.

```slang
// shaders/voxel/compute/culling.slang
module VoxelCulling;

import Types;
import VoxelChunk;

struct CullingData
{
    float4 frustumPlanes[6];
    float3 cameraPos;
    float maxDistance;
    uint chunkCount;
};

[[vk::push_constant]]
struct PC
{
    CullingData culling;
} pc;

[[vk::binding(0, 0)]]
StructuredBuffer<ChunkHeader> chunkHeaders;

[[vk::binding(1, 0)]]
StructuredBuffer<AABB> chunkAABBs;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> visibleChunks;

[[vk::binding(3, 0)]]
RWStructuredBuffer<uint> visibleCount;

[[vk::binding(4, 0)]]
RWStructuredBuffer<float> chunkDistances;

// Frustum culling
bool isInFrustum(AABB aabb, float4 planes[6])
{
    for (int i = 0; i < 6; i++)
    {
        float3 n = planes[i].xyz;
        float d = planes[i].w;
        float3 pos = n >= 0 ? aabb.max : aabb.min;

        if (dot(n, pos) + d < 0.0)
            return false;
    }
    return true;
}

// Distance culling
bool isWithinDistance(float3 chunkCenter, float3 cameraPos, float maxDist)
{
    return distance(chunkCenter, cameraPos) < maxDist;
}

// Occlusion culling (Hi-Z)
bool isOccluded(float3 chunkCenter, float chunkSize, Texture2D<float> hiZ)
{
    // Упрощённая версия — нужна интеграция с Hi-Z буфером
    return false;
}

[numthreads(64, 1, 1)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    uint chunkId = tid.x;
    if (chunkId >= pc.culling.chunkCount) return;

    AABB aabb = chunkAABBs[chunkId];
    float3 center = (aabb.min + aabb.max) * 0.5;

    // 1. Frustum culling
    if (!isInFrustum(aabb, pc.culling.frustumPlanes)) return;

    // 2. Distance culling
    float dist = distance(center, pc.culling.cameraPos);
    if (dist > pc.culling.maxDistance) return;

    // 3. LOD selection на основе расстояния
    uint lodLevel = 0;
    if (dist > 64.0) lodLevel = 1;
    if (dist > 128.0) lodLevel = 2;
    if (dist > 256.0) lodLevel = 3;

    // 4. Запись результата
    uint slot;
    InterlockedAdd(visibleCount[0], 1, slot);

    visibleChunks[slot] = chunkId;
    chunkDistances[chunkId] = dist;
}
```

---

## Паттерн 3: Marching Cubes на GPU

Генерация мешей из воксельных данных.

```slang
// shaders/voxel/compute/marching_cubes.slang
module MarchingCubes;

import Types;

// Таблица edge table (256 entries)
[[vk::binding(0, 0)]]
StructuredBuffer<int> edgeTable;

// Таблица triangle table (256 * 16 entries)
[[vk::binding(1, 0)]]
StructuredBuffer<int> triangleTable;

[[vk::binding(2, 0)]]
StructuredBuffer<float> densityVolume;

[[vk::binding(3, 0)]]
RWStructuredBuffer<float3> outputVertices;

[[vk::binding(4, 0)]]
RWStructuredBuffer<uint> vertexCount;

[[vk::push_constant]]
struct PC
{
    uint3 volumeSize;
    float isoLevel;
    float3 worldOffset;
    float voxelSize;
} pc;

float sampleDensity(int3 pos)
{
    if (any(pos < 0) || any(pos >= int3(pc.volumeSize)))
        return 0.0;

    uint index = pos.z * pc.volumeSize.x * pc.volumeSize.y +
                pos.y * pc.volumeSize.x +
                pos.x;
    return densityVolume[index];
}

float3 interpolate(float3 p1, float3 p2, float d1, float d2)
{
    if (abs(pc.isoLevel - d1) < 0.00001) return p1;
    if (abs(pc.isoLevel - d2) < 0.00001) return p2;
    if (abs(d1 - d2) < 0.00001) return p1;

    float t = (pc.isoLevel - d1) / (d2 - d1);
    return p1 + t * (p2 - p1);
}

[numthreads(8, 8, 8)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= pc.volumeSize - 1)) return;

    int3 pos = int3(tid);

    // 8 corners of the cube
    float3 corners[8] = {
        float3(pos.x, pos.y, pos.z),
        float3(pos.x + 1, pos.y, pos.z),
        float3(pos.x + 1, pos.y, pos.z + 1),
        float3(pos.x, pos.y, pos.z + 1),
        float3(pos.x, pos.y + 1, pos.z),
        float3(pos.x + 1, pos.y + 1, pos.z),
        float3(pos.x + 1, pos.y + 1, pos.z + 1),
        float3(pos.x, pos.y + 1, pos.z + 1)
    };

    // Densities at corners
    float densities[8] = {
        sampleDensity(int3(pos.x, pos.y, pos.z)),
        sampleDensity(int3(pos.x + 1, pos.y, pos.z)),
        sampleDensity(int3(pos.x + 1, pos.y, pos.z + 1)),
        sampleDensity(int3(pos.x, pos.y, pos.z + 1)),
        sampleDensity(int3(pos.x, pos.y + 1, pos.z)),
        sampleDensity(int3(pos.x + 1, pos.y + 1, pos.z)),
        sampleDensity(int3(pos.x + 1, pos.y + 1, pos.z + 1)),
        sampleDensity(int3(pos.x, pos.y + 1, pos.z + 1))
    };

    // Calculate cube index
    int cubeIndex = 0;
    for (int i = 0; i < 8; i++)
    {
        if (densities[i] < pc.isoLevel)
            cubeIndex |= (1 << i);
    }

    if (edgeTable[cubeIndex] == 0) return;

    // Calculate vertices
    float3 vertices[12];
    if (edgeTable[cubeIndex] & 1)    vertices[0]  = interpolate(corners[0], corners[1], densities[0], densities[1]);
    if (edgeTable[cubeIndex] & 2)    vertices[1]  = interpolate(corners[1], corners[2], densities[1], densities[2]);
    if (edgeTable[cubeIndex] & 4)    vertices[2]  = interpolate(corners[2], corners[3], densities[2], densities[3]);
    if (edgeTable[cubeIndex] & 8)    vertices[3]  = interpolate(corners[3], corners[0], densities[3], densities[0]);
    if (edgeTable[cubeIndex] & 16)   vertices[4]  = interpolate(corners[4], corners[5], densities[4], densities[5]);
    if (edgeTable[cubeIndex] & 32)   vertices[5]  = interpolate(corners[5], corners[6], densities[5], densities[6]);
    if (edgeTable[cubeIndex] & 64)   vertices[6]  = interpolate(corners[6], corners[7], densities[6], densities[7]);
    if (edgeTable[cubeIndex] & 128)  vertices[7]  = interpolate(corners[7], corners[4], densities[7], densities[4]);
    if (edgeTable[cubeIndex] & 256)  vertices[8]  = interpolate(corners[0], corners[4], densities[0], densities[4]);
    if (edgeTable[cubeIndex] & 512)  vertices[9]  = interpolate(corners[1], corners[5], densities[1], densities[5]);
    if (edgeTable[cubeIndex] & 1024) vertices[10] = interpolate(corners[2], corners[6], densities[2], densities[6]);
    if (edgeTable[cubeIndex] & 2048) vertices[11] = interpolate(corners[3], corners[7], densities[3], densities[7]);

    // Output triangles
    for (int i = 0; triangleTable[cubeIndex * 16 + i] != -1; i += 3)
    {
        uint slot;
        InterlockedAdd(vertexCount[0], 3, slot);

        float3 worldOffset = pc.worldOffset;
        float vs = pc.voxelSize;

        outputVertices[slot + 0] = vertices[triangleTable[cubeIndex * 16 + i + 0]] * vs + worldOffset;
        outputVertices[slot + 1] = vertices[triangleTable[cubeIndex * 16 + i + 1]] * vs + worldOffset;
        outputVertices[slot + 2] = vertices[triangleTable[cubeIndex * 16 + i + 2]] * vs + worldOffset;
    }
}
```

---

## Паттерн 4: Sparse Voxel Octree (SVO) Traversal

Трассировка лучей через SVO.

```slang
// shaders/voxel/rendering/svo_raymarch.slang
module SVORayMarch;

import Types;

struct SVONode
{
    uint childMask;      // 8 bits for children
    uint childPtr;       // Index to first child
    uint voxelData;      // Packed voxel data
    uint padding;
};

[[vk::binding(0, 0)]]
StructuredBuffer<SVONode> svoNodes;

[[vk::binding(1, 0)]]
StructuredBuffer<VoxelData> voxelData;

[[vk::push_constant]]
struct PC
{
    float4x4 invViewProj;
    float3 cameraPos;
    uint maxSteps;
    float maxDistance;
    uint rootIndex;
} pc;

struct Ray
{
    float3 origin;
    float3 direction;
    float tMin;
    float tMax;
};

struct Hit
{
    bool hit;
    float t;
    float3 normal;
    uint voxelIndex;
};

// DDA-based SVO traversal
Hit traverseSVO(Ray ray, uint rootIndex, uint maxSteps)
{
    Hit result;
    result.hit = false;

    float t = 0.0;
    float3 pos = ray.origin;

    for (uint step = 0; step < maxSteps; step++)
    {
        // Determine octant
        uint3 octant = uint3(pos > 0.5);

        // Navigate SVO
        uint nodeIndex = rootIndex;
        uint depth = 0;
        uint maxDepth = 8;

        while (depth < maxDepth)
        {
            SVONode node = svoNodes[nodeIndex];
            uint childBit = 1 << (octant.z * 4 + octant.y * 2 + octant.x);

            if (!(node.childMask & childBit))
            {
                // Empty space — advance ray
                break;
            }

            // Navigate to child
            uint childIndex = node.childPtr + popcount(node.childMask & (childBit - 1));
            nodeIndex = childIndex;
            depth++;
        }

        // Check for voxel at leaf
        SVONode leaf = svoNodes[nodeIndex];
        if (leaf.voxelData != 0xFFFFFFFF)
        {
            result.hit = true;
            result.t = t;
            result.voxelIndex = leaf.voxelData;
            return result;
        }

        // Advance ray
        float stepSize = 1.0 / 256.0;  // Based on voxel size
        t += stepSize;
        pos = ray.origin + ray.direction * t;

        if (t > ray.tMax || t > pc.maxDistance) break;
    }

    return result;
}

[numthreads(8, 8, 1)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    // Generate ray from pixel
    float2 uv = (float2(tid.xy) + 0.5) / float2(1024.0, 768.0);
    float2 ndc = uv * 2.0 - 1.0;

    float4 worldPos = mul(pc.invViewProj, float4(ndc, 1.0, 1.0));
    worldPos /= worldPos.w;

    Ray ray;
    ray.origin = pc.cameraPos;
    ray.direction = normalize(worldPos.xyz - pc.cameraPos);
    ray.tMin = 0.0;
    ray.tMax = pc.maxDistance;

    Hit hit = traverseSVO(ray, pc.rootIndex, pc.maxSteps);

    // Output
    // ...
}
```

---

## Паттерн 5: Material System

Интерфейс-ориентированная система материалов.

```slang
// shaders/materials/interface.slang
module Materials;

interface IMaterial
{
    float3 albedo;
    float roughness;
    float metallic;
    float emission;

    float3 evaluate(float3 viewDir, float3 lightDir, float3 normal);
    float3 sample(float2 uv);
};

// PBR материал
struct PBRMaterial : IMaterial
{
    float3 albedo_;
    float roughness_;
    float metallic_;
    float emission_;

    [[vk::binding(0, 1)]] Texture2D albedoMap;
    [[vk::binding(1, 1)]] Texture2D normalMap;
    [[vk::binding(2, 1)]] Texture2D roughnessMap;
    [[vk::binding(3, 1)]] SamplerState sampler_;

    property float3 albedo { get { return albedo_; } }
    property float roughness { get { return roughness_; } }
    property float metallic { get { return metallic_; } }
    property float emission { get { return emission_; } }

    float3 evaluate(float3 viewDir, float3 lightDir, float3 normal)
    {
        float3 H = normalize(viewDir + lightDir);
        float NdotL = max(dot(normal, lightDir), 0.0);
        float NdotV = max(dot(normal, viewDir), 0.0);
        float NdotH = max(dot(normal, H), 0.0);

        // GGX distribution
        float a = roughness_ * roughness_;
        float a2 = a * a;
        float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
        float D = a2 / (3.14159 * denom * denom);

        // Fresnel
        float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo_, metallic_);
        float3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

        // Geometry
        float k = (roughness_ + 1.0) * (roughness_ + 1.0) / 8.0;
        float G1L = NdotL / (NdotL * (1.0 - k) + k);
        float G1V = NdotV / (NdotV * (1.0 - k) + k);
        float G = G1L * G1V;

        float3 specular = D * F * G / (4.0 * NdotL * NdotV + 0.001);
        float3 diffuse = (1.0 - metallic_) * albedo_ / 3.14159;

        return (diffuse + specular) * NdotL;
    }

    float3 sample(float2 uv)
    {
        return albedoMap.Sample(sampler_, uv).rgb;
    }
};
```

---

## Паттерн 6: ECS Integration Pattern

Шаблон синхронизации ECS-данных с GPU.

```slang
// shaders/voxel/ecs_data.slang
module ECSData;

// Структуры, зеркалирующие ECS компоненты
struct TransformComponent
{
    float3 position;
    float4 rotation;  // Quaternion
    float3 scale;
};

struct ChunkComponent
{
    uint3 chunkCoord;
    uint lodLevel;
    uint materialIndex;
    uint flags;
};

struct PhysicsComponent
{
    float3 velocity;
    float mass;
    float3 angularVelocity;
    float inertia;
};

// GPU-представление ECS мира
struct ECSWorld
{
    uint entityCount;
    uint chunkCount;
    uint dynamicObjectCount;
    uint padding;

    // Массивы компонентов (SOA для cache locality)
    [[vk::buffer_reference]] TransformComponent* transforms;
    [[vk::buffer_reference]] ChunkComponent* chunks;
    [[vk::buffer_reference]] PhysicsComponent* physics;
};

[[vk::binding(0, 0)]]
StructuredBuffer<ECSWorld> ecsWorlds;

// Утилиты для работы с ECS данными в шейдере
float3 getWorldPosition(uint entityId, ECSWorld world)
{
    return world.transforms[entityId].position;
}

float4x4 getModelMatrix(uint entityId, ECSWorld world)
{
    TransformComponent t = world.transforms[entityId];
    // Construct matrix from position, rotation, scale
    // ...
    return float4x4(1.0);
}
```

---

## Следующий раздел

- **15. Примеры кода** — готовые примеры для интеграции