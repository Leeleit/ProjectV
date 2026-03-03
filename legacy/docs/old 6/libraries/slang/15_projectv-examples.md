# Slang в ProjectV: Продвинутые примеры

**🔴 Уровень 3: Продвинутый** — Демонстрация уникальных возможностей Slang.

## Почему Slang, а не GLSL/HLSL?

Slang предоставляет возможности, которых **нет** в традиционных шейдерных языках:

| Фича                   | GLSL | HLSL     | Slang |
|------------------------|------|----------|-------|
| Interface наследование | ❌    | ❌        | ✅     |
| Generic типы           | ❌    | ❌        | ✅     |
| Модули и импорт        | ❌    | Частично | ✅     |
| Structs с методами     | ❌    | ❌        | ✅     |
| Атрибуты-параметры     | ❌    | ❌        | ✅     |
| Cross-compilation      | ✅    | ✅        | ✅     |

---

## 1. Interface и наследование

### 1.1 Материальный интерфейс

```slang
// materials/interfaces.slang
module MaterialInterfaces;

// Базовый интерфейс для всех материалов
interface IMaterial
{
    // Свойства поверхности
    float3 getBaseColor(float2 uv);
    float getMetallic(float2 uv);
    float getRoughness(float2 uv);
    float3 getEmissive(float2 uv);
    float3 getNormal(float2 uv, float3 geometricNormal);

    // Вычисление BRDF
    float3 evaluate(
        float3 viewDir,
        float3 lightDir,
        float3 normal,
        float2 uv);
}

// Интерфейс для материалов с подповерхностным рассеиванием
interface ISubsurfaceMaterial : IMaterial
{
    float3 getSubsurfaceColor();
    float getSubsurfaceRadius();
    float3 evaluateSubsurface(
        float3 viewDir,
        float3 lightDir,
        float3 normal,
        float thickness);
}
```

### 1.2 Конкретные реализации

```slang
// materials/pbr.slang
module PBRMaterials;

import MaterialInterfaces;

// PBR материал с текстурами
struct PBRMaterial : IMaterial
{
    // Текстуры
    Texture2D<float4> baseColorMap;
    Texture2D<float> metallicMap;
    Texture2D<float> roughnessMap;
    Texture2D<float3> normalMap;
    Texture2D<float3> emissiveMap;

    // Параметры
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float3 emissiveFactor;

    // Sampler
    SamplerState samplerState;

    // Реализация интерфейса
    float3 getBaseColor(float2 uv)
    {
        return baseColorMap.Sample(samplerState, uv).rgb * baseColorFactor.rgb;
    }

    float getMetallic(float2 uv)
    {
        return metallicMap.Sample(samplerState, uv).r * metallicFactor;
    }

    float getRoughness(float2 uv)
    {
        return roughnessMap.Sample(samplerState, uv).r * roughnessFactor;
    }

    float3 getEmissive(float2 uv)
    {
        return emissiveMap.Sample(samplerState, uv).rgb * emissiveFactor;
    }

    float3 getNormal(float2 uv, float3 geometricNormal)
    {
        float3 tangentNormal = normalMap.Sample(samplerState, uv) * 2.0 - 1.0;
        // TBN matrix computation...
        return normalize(tangentToWorld(tangentNormal, geometricNormal));
    }

    float3 evaluate(
        float3 viewDir,
        float3 lightDir,
        float3 normal,
        float2 uv)
    {
        float3 baseColor = getBaseColor(uv);
        float metallic = getMetallic(uv);
        float roughness = getRoughness(uv);

        // GGX BRDF
        float3 H = normalize(viewDir + lightDir);
        float NdotH = max(dot(normal, H), 0.0);
        float NdotV = max(dot(normal, viewDir), 0.0);
        float NdotL = max(dot(normal, lightDir), 0.0);

        // Fresnel
        float3 F0 = lerp(float3(0.04), baseColor, metallic);
        float3 F = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);

        // GGX Distribution
        float D = distributionGGX(NdotH, roughness);

        // Geometry
        float G = geometrySmith(NdotV, NdotL, roughness);

        // Specular
        float3 numerator = D * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.0001;
        float3 specular = numerator / denominator;

        // Diffuse
        float3 kD = (1.0 - F) * (1.0 - metallic);
        float3 diffuse = kD * baseColor / PI;

        return (diffuse + specular) * NdotL;
    }

    // Вспомогательные методы в struct!
    static float distributionGGX(float NdotH, float roughness)
    {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH2 = NdotH * NdotH;

        float num = a2;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;

        return num / denom;
    }

    static float geometrySmith(float NdotV, float NdotL, float roughness)
    {
        float r = roughness + 1.0;
        float k = (r * r) / 8.0;

        float ggx1 = NdotL / (NdotL * (1.0 - k) + k);
        float ggx2 = NdotV / (NdotV * (1.0 - k) + k);

        return ggx1 * ggx2;
    }

    static float3 fresnelSchlick(float cosTheta, float3 F0)
    {
        return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    }
};
```

### 1.3 Использование интерфейса в шейдере

```slang
// shaders/pbr_deferred.slang
module PBRDeferred;

import MaterialInterfaces;
import PBRMaterials;

// Uniform buffer с материалом
[[vk::binding(0, 0)]]
ConstantBuffer<PBRMaterial> gMaterial;

// G-Buffer textures
[[vk::binding(1, 0)]]
Texture2D<float4> gGBuffer0;  // albedo + metallic

[[vk::binding(2, 0)]]
Texture2D<float4> gGBuffer1;  // normal + roughness

[[vk::binding(3, 0)]]
Texture2D<float> gDepth;

// Output
[[vk::binding(4, 0)]]
RWTexture2D<float4> gOutput;

// Функция, принимающая любой IMaterial
float3 shadeWithMaterial(
    IMaterial material,
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float2 uv)
{
    float3 result = float3(0, 0, 0);

    // Directional light
    float3 lightDir = normalize(float3(1, 2, 1));
    float3 lightColor = float3(1, 1, 1) * 3.0;

    result += material.evaluate(viewDir, lightDir, normal, uv) * lightColor;

    // Emissive
    result += material.getEmissive(uv);

    return result;
}

[shader("fragment")]
float4 fsMain(float4 position : SV_Position,
              float2 uv : TEXCOORD0) : SV_Target
{
    // Sample G-Buffer
    float4 gbuffer0 = gGBuffer0[uint2(position.xy)];
    float4 gbuffer1 = gGBuffer1[uint2(position.xy)];
    float depth = gDepth[uint2(position.xy)];

    // Reconstruct world position
    float3 worldPos = reconstructWorldPosition(position.xy, depth);
    float3 normal = gbuffer1.xyz * 2.0 - 1.0;
    float3 viewDir = normalize(gCameraPos - worldPos);

    // Shade using interface
    float3 color = shadeWithMaterial(gMaterial, worldPos, normal, viewDir, uv);

    return float4(color, 1.0);
}
```

---

## 2. Generic типы

### 2.1 Обобщённый шейдер для вокселей

```slang
// shaders/voxel_generic.slang
module VoxelGeneric;

// Generic параметр для типа воксельных данных
generic<T>
struct VoxelShader
{
    StructuredBuffer<T> voxelData;
    uint3 volumeSize;
    float voxelScale;

    // Generic метод для выборки вокселя
    T sampleVoxel(float3 worldPos)
    {
        int3 coord = int3(floor(worldPos / voxelScale));

        if (any(coord < 0) || any(coord >= volumeSize))
        {
            return T();  // Default constructed
        }

        uint index = coord.x + coord.y * volumeSize.x +
                     coord.z * volumeSize.x * volumeSize.y;
        return voxelData[index];
    }

    // Generic метод для трассировки луча
    RayHit<T> traceRay(float3 origin, float3 direction, float maxDistance)
    {
        RayHit<T> hit;
        hit.hit = false;

        // DDA algorithm
        float3 pos = origin / voxelScale;
        float3 dir = direction;
        float3 step = sign(dir);
        float3 tMax = (floor(pos) + 0.5 + step * 0.5 - pos) / dir;
        float3 tDelta = step / dir;

        for (int i = 0; i < int(maxDistance / voxelScale); i++)
        {
            T voxel = sampleVoxel(pos * voxelScale);

            if (!isEmpty(voxel))  // Requires isEmpty(T) function
            {
                hit.hit = true;
                hit.voxel = voxel;
                hit.position = pos * voxelScale;
                hit.normal = computeNormal(pos);
                return hit;
            }

            // Step to next voxel
            if (tMax.x < tMax.y)
            {
                if (tMax.x < tMax.z)
                {
                    pos.x += step.x;
                    tMax.x += tDelta.x;
                }
                else
                {
                    pos.z += step.z;
                    tMax.z += tDelta.z;
                }
            }
            else
            {
                if (tMax.y < tMax.z)
                {
                    pos.y += step.y;
                    tMax.y += tDelta.y;
                }
                else
                {
                    pos.z += step.z;
                    tMax.z += tDelta.z;
                }
            }
        }

        return hit;
    }
};

// Результат трассировки луча
struct RayHit<T>
{
    bool hit;
    T voxel;
    float3 position;
    float3 normal;
};
```

### 2.2 Специализации для разных типов вокселей

```slang
// voxel_types.slang
module VoxelTypes;

// Простой воксель (один байт материала)
struct SimpleVoxel
{
    uint8_t materialId;

    bool isEmpty() { return materialId == 0; }
};

// PBR воксель с нормалью
struct PBRVoxel
{
    uint16_t materialId;
    uint8_t normalX;  // Octahedral encoding
    uint8_t normalY;
    uint16_t materialData;

    bool isEmpty() { return materialId == 0; }

    float3 decodeNormal()
    {
        float2 oct = float2(normalX, normalY) / 127.0 - 1.0;
        return octahedralDecode(oct);
    }
};

// SDF воксель для signed distance field
struct SDFVoxel
{
    float distance;
    uint16_t materialId;

    bool isEmpty() { return distance > 0.5; }
};

// Специализации VoxelShader
alias SimpleVoxelShader = VoxelShader<SimpleVoxel>;
alias PBRVoxelShader = VoxelShader<PBRVoxel>;
alias SDFVoxelShader = VoxelShader<SDFVoxel>;
```

---

## 3. Модули и импорт

### 3.1 Модульная архитектура шейдеров

```slang
// modules/common/math.slang
module Common.Math;

public const float PI = 3.14159265359;
public const float INV_PI = 1.0 / PI;
public const float EPSILON = 1e-6;

public float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

public float3 saturate(float3 x)
{
    return clamp(x, float3(0), float3(1));
}

public float luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

public float3 reinhardToneMap(float3 color, float exposure)
{
    color *= exposure;
    return color / (color + float3(1));
}

public float3 acesToneMap(float3 color)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}
```

```slang
// modules/common/sampling.slang
module Common.Sampling;

import Common.Math;

// Hammersley sequence
public float2 hammersley(uint i, uint N)
{
    uint bits = i;
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float radicalInverseVDC = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) / float(N), radicalInverseVDC);
}

// Importance sample GGX
public float3 importanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Spherical to cartesian
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Tangent to world space
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// Prefiltered environment mapping
public float3 prefilterEnvMap(
    TextureCube<float3> envMap,
    SamplerState samplerState,
    float3 R,
    float roughness,
    uint sampleCount)
{
    float3 N = R;
    float3 V = R;

    float3 prefilteredColor = float3(0);
    float totalWeight = 0.0;

    for (uint i = 0; i < sampleCount; i++)
    {
        float2 Xi = hammersley(i, sampleCount);
        float3 H = importanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            prefilteredColor += envMap.SampleLevel(samplerState, L, 0).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    return prefilteredColor / totalWeight;
}
```

### 3.2 Композиция модулей

```slang
// shaders/pbr_deferred_full.slang
module PBRDeferredFull;

import Common.Math;
import Common.Sampling;
import MaterialInterfaces;
import PBRMaterials;

// Теперь доступны все публичные функции из импортированных модулей
// - saturate, luminance, acesToneMap из Common.Math
// - hammersley, importanceSampleGGX из Common.Sampling
// - IMaterial, PBRMaterial из MaterialInterfaces

[[vk::binding(0, 0)]]
ConstantBuffer<PBRMaterial> gMaterial;

[[vk::binding(1, 0)]]
TextureCube<float3> gEnvMap;

[[vk::binding(2, 0)]]
Texture2D<float2> gBRDFLUT;

[[vk::binding(3, 0)]]
SamplerState gSampler;

// Image-Based Lighting
float3 evaluateIBL(
    IMaterial material,
    float3 N,
    float3 V,
    float2 uv)
{
    float3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);

    float roughness = material.getRoughness(uv);
    float metallic = material.getMetallic(uv);
    float3 baseColor = material.getBaseColor(uv);

    // Prefiltered specular
    float3 F0 = lerp(float3(0.04), baseColor, metallic);
    float3 F = fresnelSchlickRoughness(NdotV, F0, roughness);

    float3 prefiltered = prefilterEnvMap(gEnvMap, gSampler, R, roughness, 32);
    float2 brdf = gBRDFLUT.Sample(gSampler, float2(NdotV, roughness)).rg;

    float3 specular = prefiltered * (F * brdf.x + brdf.y);

    // Diffuse IBL
    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 irradiance = gEnvMap.SampleLevel(gSampler, N, 10).rgb;  // Diffuse convolved
    float3 diffuse = kD * baseColor * irradiance;

    return diffuse + specular;
}

[shader("fragment")]
float4 fsMain(float4 position : SV_Position,
              float2 uv : TEXCOORD0,
              float3 normal : NORMAL,
              float3 worldPos : WORLD_POS) : SV_Target
{
    float3 N = normalize(normal);
    float3 V = normalize(gCameraPos - worldPos);

    // Direct lighting
    float3 directLight = shadeWithMaterial(gMaterial, worldPos, N, V, uv);

    // Indirect lighting (IBL)
    float3 indirectLight = evaluateIBL(gMaterial, N, V, uv);

    // Combine
    float3 color = directLight + indirectLight;

    // Tone mapping
    color = acesToneMap(color);

    // Gamma correction
    color = pow(color, float3(1.0 / 2.2));

    return float4(color, 1.0);
}
```

---

## 4. Structs с методами для воксельных операций

```slang
// shaders/voxel_operations.slang
module VoxelOperations;

import VoxelTypes;

// Struct с методами для операций над воксельным чанком
struct VoxelChunkOps
{
    StructuredBuffer<uint32_t> packedData;  // RLE compressed
    uint32_t uncompressedSize;

    // Метод для декомпрессии RLE
    uint32_t getVoxelId(uint3 localCoord, uint chunkSize)
    {
        uint index = localCoord.x +
                     localCoord.y * chunkSize +
                     localCoord.z * chunkSize * chunkSize;

        // RLE decompression
        uint currentPos = 0;
        uint dataIdx = 0;

        while (dataIdx < packedData.Length())
        {
            uint runLength = packedData[dataIdx] >> 16;
            uint voxelId = packedData[dataIdx] & 0xFFFF;

            if (index >= currentPos && index < currentPos + runLength)
            {
                return voxelId;
            }

            currentPos += runLength;
            dataIdx++;
        }

        return 0;  // Air
    }

    // Метод для трассировки внутри чанка
    bool traceLocalRay(
        float3 localOrigin,
        float3 localDir,
        out float3 hitPos,
        out float3 hitNormal)
    {
        // Amanatides-Woo algorithm
        float3 pos = localOrigin;
        float3 step = sign(localDir);
        float3 tMax = (floor(pos) + 0.5 + step * 0.5 - pos) / localDir;
        float3 tDelta = step / localDir;

        for (int i = 0; i < 32; i++)  // Max iterations
        {
            if (getVoxelId(uint3(pos), 16) != 0)
            {
                hitPos = pos;

                // Determine normal from last step
                if (tMax.x < tMax.y)
                {
                    hitNormal = tMax.x < tMax.z ?
                        float3(-step.x, 0, 0) : float3(0, 0, -step.z);
                }
                else
                {
                    hitNormal = tMax.y < tMax.z ?
                        float3(0, -step.y, 0) : float3(0, 0, -step.z);
                }

                return true;
            }

            // Step
            if (tMax.x < tMax.y)
            {
                if (tMax.x < tMax.z)
                {
                    pos.x += step.x;
                    tMax.x += tDelta.x;
                }
                else
                {
                    pos.z += step.z;
                    tMax.z += tDelta.z;
                }
            }
            else
            {
                if (tMax.y < tMax.z)
                {
                    pos.y += step.y;
                    tMax.y += tDelta.y;
                }
                else
                {
                    pos.z += step.z;
                    tMax.z += tDelta.z;
                }
            }
        }

        return false;
    }

    // Метод для вычисления AO
    float computeAmbientOcclusion(uint3 localCoord, uint chunkSize)
    {
        float ao = 0.0;
        int samples = 0;

        for (int dz = -1; dz <= 1; dz++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0 && dz == 0) continue;

                    uint3 neighborCoord = uint3(
                        int(localCoord.x) + dx,
                        int(localCoord.y) + dy,
                        int(localCoord.z) + dz
                    );

                    if (any(int3(neighborCoord) < 0) ||
                        any(neighborCoord >= chunkSize))
                        continue;

                    if (getVoxelId(neighborCoord, chunkSize) != 0)
                    {
                        float weight = 1.0 / (dx*dx + dy*dy + dz*dz);
                        ao += weight;
                    }

                    samples++;
                }
            }
        }

        return 1.0 - saturate(ao / float(samples));
    }
};
```

---

## 5. Полный пример: Voxel Renderer

```slang
// shaders/voxel_renderer.slang
module VoxelRenderer;

import Common.Math;
import Common.Sampling;
import VoxelTypes;
import VoxelOperations;

// Push constants
[[vk::push_constant]]
struct VoxelRendererParams
{
    float4x4 viewProj;
    float4x4 invViewProj;
    float3 cameraPos;
    float time;
    uint frameIndex;
    float maxDistance;
    float voxelScale;
    uint chunkCount;
} params;

// SVO data
[[vk::binding(0, 0)]]
StructuredBuffer<uint64_t> svoNodes;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> svoVoxelData;

[[vk::binding(2, 0)]]
StructuredBuffer<VoxelMaterial> materials;

[[vk::binding(3, 0)]]
TextureCube<float3> envMap;

[[vk::binding(4, 0)]]
SamplerState envSampler;

[[vk::binding(5, 0)]]
RWTexture2D<float4> outputImage;

[[vk::binding(6, 0)]]
RWTexture2D<float> depthImage;

// SVO traversal
float3 traverseSVO(
    float3 origin,
    float3 direction,
    out uint32_t materialId,
    out float3 normal)
{
    // Stack for iterative traversal
    uint stack[24];
    int stackPtr = 0;
    stack[0] = 0;  // Root node

    float tMin = 0.0;
    float tMax = params.maxDistance;

    while (stackPtr >= 0 && tMin < tMax)
    {
        uint nodeIdx = stack[stackPtr--];
        uint64_t nodeData = svoNodes[nodeIdx];

        uint8_t childMask = (nodeData >> 56) & 0xFF;
        uint32_t childPtr = (nodeData >> 28) & 0x0FFFFFFF;
        uint32_t voxelPtr = nodeData & 0x0FFFFFFF;

        // Leaf node
        if (childMask == 0 && voxelPtr != 0)
        {
            materialId = svoVoxelData[voxelPtr] >> 16;
            uint normalData = svoVoxelData[voxelPtr] & 0xFFFF;

            // Decode normal
            float2 oct = float2(
                (normalData >> 8) / 127.0 - 1.0,
                (normalData & 0xFF) / 127.0 - 1.0
            );
            normal = octahedralDecode(oct);

            return origin + direction * tMin;
        }

        // Internal node - push children
        if (childMask != 0)
        {
            for (int i = 7; i >= 0; i--)
            {
                if (childMask & (1 << i))
                {
                    stack[++stackPtr] = childPtr + i;
                }
            }
        }
    }

    materialId = 0;
    normal = float3(0, 1, 0);
    return origin + direction * params.maxDistance;
}

// Main compute shader
[numthreads(8, 8, 1)]
[shader("compute")]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    uint2 resolution;
    outputImage.GetDimensions(resolution.x, resolution.y);

    if (any(tid.xy >= resolution)) return;

    // Generate ray
    float2 uv = (float2(tid.xy) + 0.5) / float2(resolution);
    float2 ndc = uv * 2.0 - 1.0;

    float4 worldPos = mul(params.invViewProj, float4(ndc, 1.0, 1.0));
    worldPos /= worldPos.w;

    float3 ro = params.cameraPos;
    float3 rd = normalize(worldPos.xyz - ro);

    // Trace SVO
    uint32_t materialId;
    float3 normal;
    float3 hitPos = traverseSVO(ro, rd, materialId, normal);

    float3 color;

    if (materialId != 0)
    {
        // Get material
        VoxelMaterial mat = materials[materialId];

        // Lighting
        float3 V = -rd;
        float3 N = normal;

        // Direct light
        float3 L = normalize(float3(1, 2, 1));
        float NdotL = max(dot(N, L), 0.0);

        // Diffuse
        float3 diffuse = mat.baseColor.rgb * NdotL;

        // Specular
        float3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);
        float spec = pow(NdotH, 32.0 * (1.0 - mat.roughness));
        float3 specular = spec * (1.0 - mat.metallic);

        // Environment reflection
        float3 R = reflect(-V, N);
        float3 envColor = envMap.SampleLevel(envSampler, R, mat.roughness * 8).rgb;

        // Combine
        color = (diffuse + specular) * 0.8 + envColor * mat.metallic * 0.2;
        color += mat.emissive.rgb;

        // Ambient
        color += mat.baseColor.rgb * 0.1;
    }
    else
    {
        // Sky
        float t = 0.5 * (rd.y + 1.0);
        color = lerp(float3(1, 1, 1), float3(0.5, 0.7, 1.0), t);
    }

    // Tone mapping and gamma
    color = acesToneMap(color);
    color = pow(color, float3(1.0 / 2.2));

    outputImage[tid.xy] = float4(color, 1.0);
}
```

---

## Ссылки

- [Slang Overview](./00_overview.md)
- [Slang Quickstart](./01_quickstart.md)
- [SVO Architecture](../../architecture/practice/00_svo-architecture.md)
- [Material System](../../architecture/practice/20_material-system.md)
