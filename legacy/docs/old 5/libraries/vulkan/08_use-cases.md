# Vulkan Use Cases

**🟢🟡 Уровень 1-2: Базовый + Средний** — Практические сценарии использования Vulkan: PBR материалы, тени,
post-processing, compute shaders.

---

## Содержание

1. [Deferred Rendering](#deferred-rendering)
2. [PBR Materials](#pbr-materials)
3. [Shadow Mapping](#shadow-mapping)
4. [Post-Processing](#post-processing)
5. [Compute Shaders](#compute-shaders)
6. [Particle Systems](#particle-systems)

---

## Deferred Rendering

### Концепция

Deferred Rendering разделяет геометрию и освещение на два этапа:

```
┌─────────────────────────────────────────────────────────┐
│                  Geometry Pass                           │
│  G-Buffer: Position | Normal | Albedo | MetallicRoughAO │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                  Lighting Pass                           │
│  Чтение G-Buffer → Вычисление освещения → Color Output  │
└─────────────────────────────────────────────────────────┘
```

### G-Buffer Setup

```cpp
struct GBuffer {
    VkImage position;
    VkImage normal;
    VkImage albedo;
    VkImage metallicRoughAO;
    VkImage depth;
    VkImageView positionView;
    VkImageView normalView;
    VkImageView albedoView;
    VkImageView metallicRoughAOView;
    VkImageView depthView;
};

void createGBuffer(GBuffer& gbuffer, uint32_t width, uint32_t height) {
    // Position: RGBA16F
    gbuffer.position = createImage(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

    // Normal: RGB10A2 (pack normal + something)
    gbuffer.normal = createImage(width, height, VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

    // Albedo: RGBA8
    gbuffer.albedo = createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

    // Metallic/Roughness/AO: RGBA8
    gbuffer.metallicRoughAO = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

    // Depth: D32_SFLOAT
    gbuffer.depth = createImage(width, height, VK_FORMAT_D32_SFLOAT,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
```

### Geometry Pass Render Pass

```cpp
VkAttachmentDescription attachments[] = {
    // Position
    {0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
     VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
     VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // Normal, Albedo, MetallicRoughAO - аналогично...
    // Depth
    {0, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
     VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
     VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
};

VkAttachmentRef colorRefs[] = {
    {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
};

VkAttachmentRef depthRef = {4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

VkSubpassDescription geometrySubpass = {};
geometrySubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
geometrySubpass.colorAttachmentCount = 4;
geometrySubpass.pColorAttachments = colorRefs;
geometrySubpass.pDepthStencilAttachment = &depthRef;
```

### Lighting Pass

```glsl
#version 460

// Input attachments from G-Buffer
layout(input_attachment_index = 0, binding = 0) uniform subpassInput gPosition;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput gNormal;
layout(input_attachment_index = 2, binding = 2) uniform subpassInput gAlbedo;
layout(input_attachment_index = 3, binding = 3) uniform subpassInput gMetallicRoughAO;

struct Light {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

layout(set = 0, binding = 4) uniform LightData {
    Light lights[MAX_LIGHTS];
    uint lightCount;
    vec3 viewPos;
} lightData;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 position = subpassLoad(gPosition).rgb;
    vec3 normal = normalize(subpassLoad(gNormal).rgb * 2.0 - 1.0);
    vec4 albedo = subpassLoad(gAlbedo);
    vec3 metallicRoughAO = subpassLoad(gMetallicRoughAO).rgb;

    float metallic = metallicRoughAO.r;
    float roughness = metallicRoughAO.g;
    float ao = metallicRoughAO.b;

    vec3 N = normal;
    vec3 V = normalize(lightData.viewPos - position);

    vec3 Lo = vec3(0.0);

    for (uint i = 0; i < lightData.lightCount; i++) {
        vec3 L = normalize(lights[i].position - position);
        vec3 H = normalize(V + L);

        float distance = length(lights[i].position - position);
        if (distance > lights[i].radius) continue;

        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lights[i].color * lights[i].intensity * attenuation;

        // PBR calculation...
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo.rgb * ao;
    vec3 color = ambient + Lo;

    // Tone mapping
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    fragColor = vec4(color, 1.0);
}
```

---

## PBR Materials

### Material System

```cpp
struct Material {
    uint32_t albedoTexture;
    uint32_t normalTexture;
    uint32_t metallicRoughnessTexture;
    uint32_t aoTexture;
    vec4 albedoColor;
    float metallic;
    float roughness;
    float ao;
    float pad;
};

// Bindless textures
layout(set = 0, binding = 0) uniform sampler2D textures[];

// Material buffer
layout(std430, set = 0, binding = 1) readonly buffer MaterialBuffer {
    Material materials[];
};
```

### PBR Shader Functions

```glsl
const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
```

### Normal Mapping

```glsl
vec3 getNormalFromMap(vec3 normal, vec2 uv, mat3 TBN) {
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
    return normalize(TBN * tangentNormal);
}

// In vertex shader:
vec3 T = normalize(mat3(model) * tangent);
vec3 B = normalize(mat3(model) * bitangent);
vec3 N = normalize(mat3(model) * normal);
mat3 TBN = mat3(T, B, N);
```

---

## Shadow Mapping

### Shadow Map Creation

```cpp
struct ShadowMap {
    VkImage depthImage;
    VkImageView depthView;
    VkFramebuffer framebuffer;
    VkSampler sampler;
    mat4 lightSpaceMatrix;
};

ShadowMap createShadowMap(uint32_t size) {
    ShadowMap shadow;

    shadow.depthImage = createImage(size, size, VK_FORMAT_D32_SFLOAT,
                                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    shadow.depthView = createImageView(shadow.depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Create framebuffer without color attachments
    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = shadowRenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &shadow.depthView;
    fbInfo.width = size;
    fbInfo.height = size;
    fbInfo.layers = 1;

    vkCreateFramebuffer(device, &fbInfo, nullptr, &shadow.framebuffer);

    // Depth bias for shadow acne
    VkPipelineRasterizationStateCreateInfo rasterInfo = {};
    rasterInfo.depthBiasEnable = VK_TRUE;
    rasterInfo.depthBiasConstantFactor = 1.25f;
    rasterInfo.depthBiasSlopeFactor = 1.75f;

    return shadow;
}
```

### Shadow Pass Shader

```glsl
// Vertex shader
#version 460

layout(push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = pc.lightSpaceMatrix * pc.model * vec4(inPosition, 1.0);
}
```

### Shadow Sampling

```glsl
float shadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;

    // PCF (Percentage Closer Filtering)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    // Bias based on surface angle
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}
```

### Cascaded Shadow Maps

```cpp
struct CascadedShadows {
    static constexpr uint32_t CASCADE_COUNT = 4;
    ShadowMap cascades[CASCADE_COUNT];
    float splitDistances[CASCADE_COUNT + 1];
    mat4 lightSpaceMatrices[CASCADE_COUNT];
};

void calculateCascadeSplits(CascadedShadows& shadows, float nearPlane, float farPlane) {
    float lambda = 0.95f;  // Split weight

    for (uint32_t i = 0; i < CascadedShadows::CASCADE_COUNT; i++) {
        float p = (i + 1) / float(CascadedShadows::CASCADE_COUNT);
        float log = nearPlane * pow(farPlane / nearPlane, p);
        float uniform = nearPlane + (farPlane - nearPlane) * p;

        shadows.splitDistances[i] = lambda * log + (1.0f - lambda) * uniform;
    }
    shadows.splitDistances[CascadedShadows::CASCADE_COUNT] = farPlane;
}
```

---

## Post-Processing

### Post-Processing Pipeline

```cpp
struct PostProcess {
    VkImage resultImage;
    VkImageView resultView;
    VkFramebuffer framebuffer;

    // Effect parameters
    float bloomThreshold;
    float bloomIntensity;
    float exposure;
    float gamma;
};
```

### Bloom

```glsl
// Bright pass
#version 460

layout(set = 0, binding = 0) uniform sampler2D sceneColor;

layout(push_constant) uniform PushConstants {
    float threshold;
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 color = texture(sceneColor, inUV).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    if (brightness > pc.threshold) {
        fragColor = vec4(color, 1.0);
    } else {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}

// Gaussian blur (horizontal + vertical passes)
vec3 blur(sampler2D image, vec2 uv, vec2 direction) {
    vec3 color = vec3(0.0);
    vec2 texelSize = 1.0 / textureSize(image, 0);

    float weights[] = {0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216};

    color += texture(image, uv).rgb * weights[0];

    for (int i = 1; i < 5; i++) {
        color += texture(image, uv + direction * texelSize * float(i)).rgb * weights[i];
        color += texture(image, uv - direction * texelSize * float(i)).rgb * weights[i];
    }

    return color;
}
```

### Tone Mapping

```glsl
// Reinhard
vec3 reinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

// ACES Filmic
vec3 aces(vec3 color) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;

    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Exposure
vec3 applyExposure(vec3 color, float exposure) {
    return vec3(1.0) - exp(-color * exposure);
}
```

### FXAA

```glsl
// Simplified FXAA
vec3 fxaa(sampler2D tex, vec2 uv, vec2 texelSize) {
    vec3 rgbN = textureOffset(tex, uv, ivec2( 0, -1)).rgb;
    vec3 rgbS = textureOffset(tex, uv, ivec2( 0,  1)).rgb;
    vec3 rgbW = textureOffset(tex, uv, ivec2(-1,  0)).rgb;
    vec3 rgbE = textureOffset(tex, uv, ivec2( 1,  0)).rgb;
    vec3 rgbM = texture(tex, uv).rgb;

    float lumaN = dot(rgbN, vec3(0.299, 0.587, 0.114));
    float lumaS = dot(rgbS, vec3(0.299, 0.587, 0.114));
    float lumaW = dot(rgbW, vec3(0.299, 0.587, 0.114));
    float lumaE = dot(rgbE, vec3(0.299, 0.587, 0.114));
    float lumaM = dot(rgbM, vec3(0.299, 0.587, 0.114));

    float rangeMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float rangeMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));

    float range = rangeMax - rangeMin;
    if (range < max(0.0312, rangeMax * 0.125)) {
        return rgbM;
    }

    // Edge detection and blending...
    return rgbM;
}
```

---

## Compute Shaders

### Image Processing

```glsl
#version 460

layout(local_size_x = 16, local_size_y = 16) in;

layout(set = 0, binding = 0, rgba8) uniform image2D inputImage;
layout(set = 0, binding = 1, rgba8) uniform image2D outputImage;

// Grayscale conversion
void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    if (coords.x >= imageSize(inputImage).x || coords.y >= imageSize(inputImage).y) return;

    vec4 color = imageLoad(inputImage, coords);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    imageStore(outputImage, coords, vec4(vec3(gray), color.a));
}
```

### Prefix Sum (Scan)

```glsl
// Parallel prefix sum using Blelloch algorithm
layout(local_size_x = 512) in;

shared float sharedData[1024];

void main() {
    uint globalIdx = gl_GlobalInvocationID.x;
    uint localIdx = gl_LocalInvocationIndex.x;

    sharedData[localIdx] = inputData[globalIdx];
    barrier();

    // Up-sweep (reduce)
    for (uint stride = 1; stride < 512; stride *= 2) {
        uint index = (localIdx + 1) * stride * 2 - 1;
        if (index < 1024) {
            sharedData[index] += sharedData[index - stride];
        }
        barrier();
    }

    // Down-sweep
    if (localIdx == 0) {
        sharedData[1023] = 0;
    }
    barrier();

    for (uint stride = 512; stride > 0; stride /= 2) {
        uint index = (localIdx + 1) * stride * 2 - 1;
        if (index < 1024) {
            float temp = sharedData[index];
            sharedData[index] += sharedData[index - stride];
            sharedData[index - stride] = temp;
        }
        barrier();
    }

    outputData[globalIdx] = sharedData[localIdx];
}
```

---

## Particle Systems

### GPU Particle System

```cpp
struct Particle {
    vec3 position;
    float life;
    vec3 velocity;
    float size;
    vec4 color;
};

struct ParticleSystem {
    VkBuffer particleBuffer;
    VkDeviceSize bufferSize;
    uint32_t maxParticles;

    VkBuffer indirectBuffer;  // For vkCmdDrawIndirect
};
```

### Particle Update Compute Shader

```glsl
#version 460

layout(local_size_x = 256) in;

struct Particle {
    vec3 position;
    float life;
    vec3 velocity;
    float size;
    vec4 color;
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

layout(std430, binding = 1) buffer IndirectCommand {
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
} indirect;

uniform float deltaTime;
uniform vec3 gravity;
uniform float lifetime;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= particles.length()) return;

    Particle p = particles[idx];

    if (p.life > 0.0) {
        // Update position
        p.position += p.velocity * deltaTime;
        p.velocity += gravity * deltaTime;
        p.life -= deltaTime;

        // Fade out
        p.color.a = p.life / lifetime;

        particles[idx] = p;
    } else {
        // Reset particle (emit from source)
        p.position = vec3(0.0);
        p.velocity = vec3(
            (float(rand()) / RAND_MAX - 0.5) * 2.0,
            (float(rand()) / RAND_MAX) * 5.0 + 2.0,
            (float(rand()) / RAND_MAX - 0.5) * 2.0
        );
        p.life = lifetime;
        p.color = vec4(1.0);
    }
}

// Count alive particles (parallel reduction)
// Update indirect.instanceCount
```

### Particle Rendering

```glsl
// Vertex shader: billboard particles
#version 460

struct Particle {
    vec3 position;
    float life;
    vec3 velocity;
    float size;
    vec4 color;
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraRight;
    vec3 cameraUp;
} pc;

layout(location = 0) in vec2 inUV;  // Quad vertices

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

void main() {
    Particle p = particles[gl_InstanceIndex];

    // Billboard quad
    vec3 position = p.position +
        pc.cameraRight * inUV.x * p.size +
        pc.cameraUp * inUV.y * p.size;

    gl_Position = pc.viewProj * vec4(position, 1.0);
    outUV = inUV * 0.5 + 0.5;
    outColor = p.color;
}
```

---

## Краткая сводка

| Техника            | Уровень | Использование          |
|--------------------|---------|------------------------|
| Deferred Rendering | 🟡      | Много источников света |
| PBR Materials      | 🟢      | Реалистичные материалы |
| Shadow Mapping     | 🟢      | Базовые тени           |
| Cascaded Shadows   | 🟡      | Большие открытые сцены |
| Bloom              | 🟢      | HDR эффекты            |
| Tone Mapping       | 🟢      | HDR → SDR              |
| FXAA               | 🟢      | Anti-aliasing          |
| Compute Shaders    | 🟡      | Обработка изображений  |
| GPU Particles      | 🟡      | Много частиц           |
