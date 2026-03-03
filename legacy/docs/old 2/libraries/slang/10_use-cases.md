# Сценарии использования Slang

**🔴 Уровень 3: Продвинутый** — Практические паттерны применения Slang для различных задач рендеринга.

---

## 1. Deferred Shading G-Buffer Pass

Рендер pass, записывающий геометрическую информацию в G-buffer для последующего deferred shading.

```slang
module gbuffer;

struct GBufferOutput
{
    float4 albedoMetallic   : SV_Target0;  // RGB: albedo, A: metallic
    float4 normalRoughness  : SV_Target1;  // RGB: world normal, A: roughness
    float4 emissiveAO       : SV_Target2;  // RGB: emissive, A: AO
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    uint materialId : TEXCOORD1;
};

[[vk::binding(0, 0)]]
cbuffer Uniforms
{
    float4x4 viewProj;
    float3 cameraPos;
};

[[vk::push_constant]]
struct PC
{
    float4x4 model;
    uint materialId;
} pc;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    float3 worldPos = mul(pc.model, float4(input.position, 1.0)).xyz;
    output.position = mul(viewProj, float4(worldPos, 1.0));
    output.normal = mul(pc.model, float4(input.normal, 0.0)).xyz;
    output.uv = input.uv;
    output.materialId = pc.materialId;
    return output;
}

[shader("fragment")]
GBufferOutput fsMain(VertexOutput input)
{
    GBufferOutput gbuf;
    float3 N = normalize(input.normal);

    gbuf.albedoMetallic = float4(0.8, 0.8, 0.8, 0.0);  // Default material
    gbuf.normalRoughness = float4(N * 0.5 + 0.5, 0.5);
    gbuf.emissiveAO = float4(0.0, 0.0, 0.0, 1.0);

    return gbuf;
}
```

---

## 2. GPU Culling с Compute Shader

Отсечение невидимых объектов на GPU без участия CPU.

```slang
module culling;

struct Frustum
{
    float4 planes[6];
};

struct AABB
{
    float3 min;
    float3 max;
};

[[vk::push_constant]]
struct PC
{
    Frustum frustum;
    uint objectCount;
} pc;

[[vk::binding(0, 0)]]
StructuredBuffer<AABB> objectAABBs;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> visibleObjects;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> visibleCount;

bool isAABBInFrustum(AABB aabb, Frustum frustum)
{
    for (int i = 0; i < 6; i++)
    {
        float3 n = frustum.planes[i].xyz;
        float d = frustum.planes[i].w;
        float3 pos = n >= 0 ? aabb.max : aabb.min;

        if (dot(n, pos) + d < 0.0)
            return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    uint objectId = tid.x;
    if (objectId >= pc.objectCount) return;

    AABB aabb = objectAABBs[objectId];

    if (isAABBInFrustum(aabb, pc.frustum))
    {
        uint slot;
        InterlockedAdd(visibleCount[0], 1, slot);
        visibleObjects[slot] = objectId;
    }
}
```

---

## 3. Ray Marching для SDF

Трассировка лучей через signed distance field.

```slang
module ray_march;

[[vk::binding(0, 0)]]
StructuredBuffer<float> sdfVolume;

[[vk::binding(1, 0)]]
RWTexture2D<float4> outputImage;

[[vk::push_constant]]
struct PC
{
    float4x4 invViewProj;
    float3 cameraPos;
    float maxDist;
    uint2 resolution;
    uint maxSteps;
} pc;

float sampleSDF(float3 worldPos)
{
    int3 coord = int3(worldPos);
    // Bounds checking and sampling
    return sdfVolume[coord.x + coord.y * 64 + coord.z * 64 * 64];
}

struct RayMarchResult
{
    bool hit;
    float t;
    float3 hitPos;
    float3 normal;
};

RayMarchResult sphereTrace(float3 ro, float3 rd)
{
    RayMarchResult result;
    result.hit = false;

    float t = 0.0;
    for (uint i = 0; i < pc.maxSteps; i++)
    {
        float3 p = ro + rd * t;
        float dist = sampleSDF(p);

        if (dist < 0.001)
        {
            result.hit = true;
            result.t = t;
            result.hitPos = p;

            // Normal via central differences
            float eps = 0.1;
            result.normal = normalize(float3(
                sampleSDF(p + float3(eps, 0, 0)) - sampleSDF(p - float3(eps, 0, 0)),
                sampleSDF(p + float3(0, eps, 0)) - sampleSDF(p - float3(0, eps, 0)),
                sampleSDF(p + float3(0, 0, eps)) - sampleSDF(p - float3(0, 0, eps))
            ));
            return result;
        }

        t += dist;
        if (t > pc.maxDist) break;
    }
    return result;
}

[numthreads(8, 8, 1)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid.xy >= pc.resolution)) return;

    float2 uv = (float2(tid.xy) + 0.5) / float2(pc.resolution);
    float2 ndc = uv * 2.0 - 1.0;

    float4 clipPos = float4(ndc, 1.0, 1.0);
    float4 worldPos = mul(pc.invViewProj, clipPos);
    worldPos /= worldPos.w;

    float3 rd = normalize(worldPos.xyz - pc.cameraPos);

    RayMarchResult result = sphereTrace(pc.cameraPos, rd);

    float4 color;
    if (result.hit)
    {
        float3 lightDir = normalize(float3(1, 2, 1));
        float NdotL = max(dot(result.normal, lightDir), 0.0);
        color = float4(float3(0.8, 0.6, 0.4) * NdotL + 0.1, 1.0);
    }
    else
    {
        color = float4(0.1, 0.2, 0.4, 1.0);
    }

    outputImage[tid.xy] = color;
}
```

---

## 4. Mesh Shader для процедурной геометрии

Генерация геометрии на GPU без vertex buffer.

```slang
module mesh_shader;

struct MeshVertex
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

[[vk::push_constant]]
struct PC
{
    float4x4 viewProj;
    float3 chunkOrigin;
    uint voxelCount;
} pc;

[shader("amplification")]
[numthreads(32, 1, 1)]
void asMain(uint3 tid : SV_DispatchThreadID)
{
    // Determine which voxels need mesh generation
    bool needsMesh = checkVoxelNeedsMesh(tid.x);

    if (needsMesh)
    {
        DispatchMesh(1, 1, 1);
    }
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(32, 1, 1)]
void msMain(
    uint gid : SV_GroupIndex,
    out vertices MeshVertex verts[24],
    out indices uint3 tris[12])
{
    uint voxelId = gid;

    // Generate cube vertices (6 faces * 4 vertices)
    SetMeshOutputCounts(24, 12);

    float3 origin = getVoxelPosition(voxelId) + pc.chunkOrigin;

    // Generate 6 faces of the cube
    for (uint face = 0; face < 6; face++)
    {
        uint base = face * 4;
        float3 n = getFaceNormal(face);

        for (uint v = 0; v < 4; v++)
        {
            verts[base + v].position = mul(pc.viewProj, float4(origin + getFaceVertex(face, v), 1.0));
            verts[base + v].normal = n;
        }

        tris[face * 2 + 0] = uint3(base, base + 1, base + 2);
        tris[face * 2 + 1] = uint3(base, base + 2, base + 3);
    }
}
```

---

## 5. Автоматическое дифференцирование

Оптимизация параметров через градиенты.

```slang
module neural_sdf;

[Differentiable]
float neuralNetwork(float3 pos, float4 weights[16])
{
    float h[16];
    for (int i = 0; i < 16; i++)
    {
        h[i] = pos.x * weights[i].x + pos.y * weights[i].y + pos.z * weights[i].z + weights[i].w;
        h[i] = max(h[i], 0.0);  // ReLU
    }

    float result = 0.0;
    for (int j = 0; j < 16; j++)
    {
        result += h[j] * weights[j].x;
    }
    return result;
}

[Differentiable]
float sdfFunction(float3 pos)
{
    float4 weights[16];  // Would be loaded from buffer
    return neuralNetwork(pos, weights);
}

// Compute gradient via forward mode
float3 computeGradient(float3 pos)
{
    float eps = 0.001;
    float3 grad;

    grad.x = (sdfFunction(pos + float3(eps, 0, 0)) - sdfFunction(pos - float3(eps, 0, 0))) / (2 * eps);
    grad.y = (sdfFunction(pos + float3(0, eps, 0)) - sdfFunction(pos - float3(0, eps, 0))) / (2 * eps);
    grad.z = (sdfFunction(pos + float3(0, 0, eps)) - sdfFunction(pos - float3(0, 0, eps))) / (2 * eps);

    return grad;
}
```

---

## 6. Post-Processing: Tonemapping

Финальный пасс с ACES tonemapping.

```slang
module tonemap;

[[vk::binding(0, 0)]]
Texture2D<float4> hdrInput;

[[vk::binding(1, 0)]]
RWTexture2D<float4> ldrOutput;

[[vk::binding(2, 0)]]
SamplerState linearSampler;

[[vk::push_constant]]
struct PC
{
    float exposure;
    float gamma;
    uint2 resolution;
} pc;

float3 acesFilmic(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[numthreads(8, 8, 1)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid.xy >= pc.resolution)) return;

    float2 uv = (float2(tid.xy) + 0.5) / float2(pc.resolution);
    float3 hdr = hdrInput.SampleLevel(linearSampler, uv, 0).rgb;

    hdr *= pc.exposure;
    float3 ldr = acesFilmic(hdr);
    ldr = pow(ldr, 1.0 / pc.gamma);

    ldrOutput[tid.xy] = float4(ldr, 1.0);
}
```

---

## 7. Bindless Rendering

Динамический доступ к массиву текстур.

```slang
module bindless;

[[vk::binding(0, 0)]]
Texture2D materialTextures[];

[[vk::binding(1, 0)]]
SamplerState linearSampler;

[[vk::push_constant]]
struct PC
{
    float4x4 viewProj;
    uint baseTextureIndex;
} pc;

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    uint materialId : TEXCOORD1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation uint materialId : TEXCOORD1;
};

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(pc.viewProj, float4(input.position, 1.0));
    output.uv = input.uv;
    output.materialId = input.materialId;
    return output;
}

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target
{
    uint textureIndex = pc.baseTextureIndex + input.materialId;

    float4 color = materialTextures[NonUniformResourceIndex(textureIndex)]
        .Sample(linearSampler, input.uv);

    return color;
}