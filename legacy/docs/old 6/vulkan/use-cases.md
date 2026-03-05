# Сценарии использования Vulkan [🟡 Уровень 2]

**🟡 Уровень 2: Средний** — Практические применения Vulkan для различных задач графики и вычислений.

## Оглавление

- [Рендеринг треугольников](#рендеринг-треугольников)
- [Текстурирование и материалы](#текстурирование-и-материалы)
- [Освещение (PBR, shadow mapping)](#освещение-pbr-shadow-mapping)
- [Post-processing эффекты](#post-processing-эффекты)
- [Compute shaders для различных задач](#compute-shaders-для-различных-задач)
- [Геометрические шейдеры](#геометрические-шейдеры)
- [Тесселяция](#тесселяция)
- [Стереоскопический рендеринг](#стереоскопический-рендеринг)
- [Ray tracing](#ray-tracing)
- [Машинное обучение на GPU](#машинное-обучение-на-gpu)

---

## Рендеринг треугольников

### Базовый треугольник

Самый простой сценарий — отрисовка цветного треугольника. Используется для проверки работоспособности Vulkan конвейера.

```cpp
// Шейдеры
const char* vertShader = R"glsl(
#version 450
layout(location = 0) out vec3 fragColor;
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);
void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
)glsl";

const char* fragShader = R"glsl(
#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(fragColor, 1.0);
}
)glsl";
```

### Индексированный рендеринг

Использование индексных буферов для повторного использования вершин.

```cpp
// Вершины
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

// Индексы (два треугольника для квада)
std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

// Рендеринг
vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
```

### Инстансинг

Отрисовка множества копий одного меша с небольшими вариациями.

```cpp
// Буфер инстансов
struct InstanceData {
    glm::mat4 model;
    glm::vec3 color;
};

std::vector<InstanceData> instances;
for (int i = 0; i < 1000; ++i) {
    instances.push_back({
        glm::translate(glm::mat4(1.0f), 
            glm::vec3(i % 10 - 5.0f, i / 10 - 5.0f, 0.0f)),
        glm::vec3(rand() / float(RAND_MAX), 
                 rand() / float(RAND_MAX), 
                 rand() / float(RAND_MAX))
    });
}

// Шейдер
#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in mat4 instanceModel; // Использует location 2, 3, 4, 5
layout(location = 6) in vec3 instanceColor;

layout(push_constant) uniform Push {
    mat4 viewProj;
} push;

void main() {
    gl_Position = push.viewProj * instanceModel * vec4(inPosition, 1.0);
    fragColor = inColor * instanceColor;
}
```

---

## Текстурирование и материалы

### Базовое текстурирование

Загрузка и применение 2D текстур.

```cpp
// Создание изображения текстуры
VkImageCreateInfo imageInfo = {};
imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
imageInfo.imageType = VK_IMAGE_TYPE_2D;
imageInfo.extent = {width, height, 1};
imageInfo.mipLevels = 1;
imageInfo.arrayLayers = 1;
imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

vkCreateImage(device, &imageInfo, nullptr, &textureImage);

// Создание image view
VkImageViewCreateInfo viewInfo = {};
viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
viewInfo.image = textureImage;
viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
viewInfo.subresourceRange.baseMipLevel = 0;
viewInfo.subresourceRange.levelCount = 1;
viewInfo.subresourceRange.baseArrayLayer = 0;
viewInfo.subresourceRange.layerCount = 1;

vkCreateImageView(device, &viewInfo, nullptr, &textureImageView);

// Создание сэмплера
VkSamplerCreateInfo samplerInfo = {};
samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
samplerInfo.magFilter = VK_FILTER_LINEAR;
samplerInfo.minFilter = VK_FILTER_LINEAR;
samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
samplerInfo.anisotropyEnable = VK_TRUE;
samplerInfo.maxAnisotropy = 16.0f;
samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
samplerInfo.unnormalizedCoordinates = VK_FALSE;
samplerInfo.compareEnable = VK_FALSE;
samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);
```

### Материалы PBR (Physically Based Rendering)

Реализация современных материалов с металличностью и шероховатостью.

```cpp
// Структура материала в шейдере
struct Material {
    vec4 baseColor;
    float metallic;
    float roughness;
    float alphaCutoff;
    float padding;
};

// Текстуры для PBR материала
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;
layout(set = 1, binding = 3) uniform sampler2D occlusionMap;
layout(set = 1, binding = 4) uniform sampler2D emissiveMap;

// Уравнение рендеринга PBR
vec3 calculatePBR(
    vec3 albedo,
    float metallic,
    float roughness,
    vec3 normal,
    vec3 viewDir,
    vec3 lightDir,
    vec3 lightColor
) {
    vec3 N = normalize(normal);
    vec3 V = normalize(viewDir);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);
    
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    
    // Диэлектрик или металл
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Fresnel (Schlick approximation)
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
    
    // Normal distribution (GGX/Trowbridge-Reitz)
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom = NdotH * NdotH * (alphaSq - 1.0) + 1.0;
    float D = alphaSq / (PI * denom * denom);
    
    // Geometry (Smith's method with GGX)
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G1 = NdotV / (NdotV * (1.0 - k) + k);
    float G2 = NdotL / (NdotL * (1.0 - k) + k);
    float G = G1 * G2;
    
    // Cook-Torrance BRDF
    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.001);
    
    // Lambertian diffuse
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;
    
    return (diffuse + specular) * lightColor * NdotL;
}
```

### Массив текстур (Texture Array)

Использование массивов текстур для эффективного переключения между множеством текстур.

```cpp
// Создание texture array
VkImageCreateInfo arrayInfo = {};
arrayInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
arrayInfo.imageType = VK_IMAGE_TYPE_2D;
arrayInfo.extent = {1024, 1024, 1};
arrayInfo.mipLevels = 1;
arrayInfo.arrayLayers = 256; // 256 текстур в массиве
arrayInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
arrayInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

vkCreateImage(device, &arrayInfo, nullptr, &textureArray);

// Image view для массива
VkImageViewCreateInfo arrayViewInfo = {};
arrayViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
arrayViewInfo.image = textureArray;
arrayViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
arrayViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
arrayViewInfo.subresourceRange.layerCount = 256;

vkCreateImageView(device, &arrayViewInfo, nullptr, &textureArrayView);

// Использование в шейдере
layout(set = 0, binding = 0) uniform sampler2DArray textureArray;
layout(location = 0) in vec2 texCoord;
layout(location = 1) in float textureIndex;

void main() {
    vec4 color = texture(textureArray, vec3(texCoord, textureIndex));
    outColor = color;
}
```

---

## Освещение (PBR, shadow mapping)

### Shadow Mapping

Создание теней с помощью карт глубины.

```cpp
// Создание shadow map
VkImageCreateInfo shadowImageInfo = {};
shadowImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
shadowImageInfo.imageType = VK_IMAGE_TYPE_2D;
shadowImageInfo.extent = {shadowMapSize, shadowMapSize, 1};
shadowImageInfo.mipLevels = 1;
shadowImageInfo.arrayLayers = 1;
shadowImageInfo.format = VK_FORMAT_D32_SFLOAT;
shadowImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
                        VK_IMAGE_USAGE_SAMPLED_BIT;

vkCreateImage(device, &shadowImageInfo, nullptr, &shadowImage);

// Render pass для shadow map
VkAttachmentDescription depthAttachment = {};
depthAttachment.format = VK_FORMAT_D32_SFLOAT;
depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

// Шейдер для shadow map (depth-only pass)
#version 450
layout(push_constant) uniform Push {
    mat4 lightSpaceMatrix;
    mat4 model;
} push;

void main() {
    gl_Position = push.lightSpaceMatrix * push.model * vec4(inPosition, 1.0);
}
```

### Каскадные shadow maps (CSM)

Улучшение качества теней на больших расстояниях.

```cpp
// Разделение frustum на каскады
std::vector<float> cascadeSplits = {0.0f, 0.05f, 0.2f, 1.0f};
std::vector<glm::mat4> lightSpaceMatrices;

for (size_t i = 0; i < cascadeSplits.size() - 1; ++i) {
    float nearSplit = cascadeSplits[i];
    float farSplit = cascadeSplits[i + 1];
    
    // Вычисление frustum для каскада
    glm::vec3 frustumCorners[8] = {
        // near plane
        glm::vec3(-1, -1, nearSplit),
        glm::vec3(1, -1, nearSplit),
        glm::vec3(1, 1, nearSplit),
        glm::vec3(-1, 1, nearSplit),
        // far plane
        glm::vec3(-1, -1, farSplit),
        glm::vec3(1, -1, farSplit),
        glm::vec3(1, 1, farSplit),
        glm::vec3(-1, 1, farSplit)
    };
    
    // Преобразование в мировые координаты
    glm::mat4 invViewProj = glm::inverse(viewProjMatrix);
    for (auto& corner : frustumCorners) {
        glm::vec4 worldCorner = invViewProj * glm::vec4(corner, 1.0f);
        corner = glm::vec3(worldCorner) / worldCorner.w;
    }
    
    // Вычисление bounding box и матрицы света
    glm::vec3 center(0.0f);
    for (const auto& corner : frustumCorners) {
        center += corner;
    }
    center /= 8.0f;
    
    glm::mat4 lightView = glm::lookAt(
        center - lightDir * 50.0f,
        center,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    
    // Вычисление bounding box в пространстве света
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    
    for (const auto& corner : frustumCorners) {
        glm::vec4 lightSpaceCorner = lightView * glm::vec4(corner, 1.0f);
        minX = std::min(minX, lightSpaceCorner.x);
        maxX = std::max(maxX, lightSpaceCorner.x);
        minY = std::min(minY, lightSpaceCorner.y);
        maxY = std::max(maxY, lightSpaceCorner.y);
        minZ = std::min(minZ, lightSpaceCorner.z);
        maxZ = std::max(maxZ, lightSpaceCorner.z);
    }
    
    // Orthographic projection с tight bounds
    glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    lightSpaceMatrices.push_back(lightProj * lightView);
}
```

### Screen Space Ambient Occlusion (SSAO)

Добавление ambient occlusion в screen space.

```cpp
// Шейдер SSAO
#version 450
layout(binding = 0) uniform sampler2D positionTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D noiseTexture;
layout(location = 0) out float fragColor;

const int kernelSize = 64;
uniform vec3 samples[kernelSize];
uniform vec2 noiseScale;

void main() {
    vec2 texCoord = gl_FragCoord.xy / textureSize(positionTexture, 0);
    vec3 fragPos = texture(positionTexture, texCoord).xyz;
    vec3 normal = normalize(texture(normalTexture, texCoord).xyz);
    vec3 randomVec = normalize(texture(noiseTexture, texCoord * noiseScale).xyz);
    
    // Create TBN matrix
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    float radius = 0.5;
    
    for (int i = 0; i < kernelSize; ++i) {
        // Sample position in view space
        vec3 samplePos = TBN * samples[i];
        samplePos = fragPos + samplePos * radius;
        
        // Project to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        
        // Sample depth at offset position
        float sampleDepth = texture(positionTexture, offset.xy).z;
        
        // Range check and accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + 0.025 ? 1.0 : 0.0) * rangeCheck;
    }
    
    fragColor = 1.0 - (occlusion / kernelSize);
}
```

---

## Post-processing эффекты

### Bloom

Создание эффекта свечения.

```cpp
// Двухпроходный bloom
// 1. Выделение ярких областей
#version 450
layout(binding = 0) uniform sampler2D sceneTexture;
layout(location = 0) out vec4 brightColor;

void main() {
    vec3 color = texture(sceneTexture, texCoord).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0) {
        brightColor = vec4(color, 1.0);
    } else {
        brightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}

// 2. Gaussian blur (двухпроходный: horizontal + vertical)
#version 450
layout(binding = 0) uniform sampler2D inputTexture;
uniform bool horizontal;
const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 tex_offset = 1.0 / textureSize(inputTexture, 0);
    vec3 result = texture(inputTexture, texCoord).rgb * weight[0];
    
    if (horizontal) {
        for (int i = 1; i < 5; ++i) {
            result += texture(inputTexture, texCoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(inputTexture, texCoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            result += texture(inputTexture, texCoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(inputTexture, texCoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }
    
    outColor = vec4(result, 1.0);
}

// 3. Композиция
#version 450
layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D bloomTexture;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 scene = texture(sceneTexture, texCoord).rgb;
    vec3 bloom = texture(bloomTexture, texCoord).rgb;
    outColor = vec4(scene + bloom, 1.0);
}
```

### Tone Mapping

Преобразование HDR в LDR для вывода на экран.

```cpp
// Различные алгоритмы tone mapping
#version 450
layout(binding = 0) uniform sampler2D hdrTexture;

// Reinhard tone mapping
vec3 reinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

// Reinhard extended (with white point)
vec3 reinhardExtended(vec3 color, float white) {
    vec3 numerator = color * (1.0 + (color / vec3(white * white)));
    vec3 denominator = 1.0 + color;
    return numerator / denominator;
}

// ACES tone mapping (approximation)
vec3 aces(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Uncharted 2 tone mapping
vec3 uncharted2(vec3 color) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    
    vec3 x = color;
    x = ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
    
    const float W = 11.2;
    vec3 whiteScale = 1.0 / uncharted2(vec3(W));
    return x * whiteScale;
}

void main() {
    vec3 hdrColor = texture(hdrTexture, texCoord).rgb;
    vec3 mapped = aces(hdrColor); // Выбор алгоритма
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));
    outColor = vec4(mapped, 1.0);
}
```

### Motion Blur

Эффект размытия в движении.

```cpp
// Velocity-based motion blur
#version 450
layout(binding = 0) uniform sampler2D colorTexture;
layout(binding = 1) uniform sampler2D velocityTexture;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 texCoord = gl_FragCoord.xy / textureSize(colorTexture, 0);
    vec2 velocity = texture(velocityTexture, texCoord).rg;
    
    const int samples = 16;
    vec3 color = texture(colorTexture, texCoord).rgb;
    
    for (int i = 1; i < samples; ++i) {
        vec2 offset = velocity * (float(i) / float(samples - 1) - 0.5);
        color += texture(colorTexture, texCoord + offset).rgb;
    }
    
    color /= float(samples);
    outColor = vec4(color, 1.0);
}

// Вычисление velocity buffer
#version 450
layout(push_constant) uniform Push {
    mat4 currentViewProj;
    mat4 previousViewProj;
} push;

void main() {
    vec4 currentPos = push.currentViewProj * vec4(inPosition, 1.0);
    vec4 previousPos = push.previousViewProj * vec4(inPosition, 1.0);
    
    // Convert to NDC
    vec2 currentNDC = currentPos.xy / currentPos.w;
    vec2 previousNDC = previousPos.xy / previousPos.w;
    
    // Calculate velocity in screen space
    vec2 velocity = (currentNDC - previousNDC) * 0.5;
    
    outVelocity = vec4(velocity, 0.0, 1.0);
}
```

---

## Compute shaders для различных задач

### Генерация геометрии для вокселей

Compute shader для создания мешей из воксельных данных.

```cpp
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(set = 0, binding = 0) buffer VoxelData {
    uint data[];
} voxels;

layout(set = 0, binding = 1) buffer VertexBuffer {
    vec4 vertices[];
} vertexBuffer;

layout(set = 0, binding = 2) buffer IndexBuffer {
    uint indices[];
} indexBuffer;

layout(set = 0, binding = 3) buffer AtomicCounter {
    uint vertexCount;
    uint indexCount;
} counter;

// Marching Cubes таблицы
const ivec3 edgeVertices[12] = ivec3[12](...);
const int edgeTable[256] = int[256](...);
const int triTable[256][16] = int[256][16](...);

void main() {
    ivec3 voxelPos = ivec3(gl_GlobalInvocationID);
    
    // Получение значений вокселей для куба
    float cube[8];
    for (int i = 0; i < 8; ++i) {
        ivec3 corner = voxelPos + ivec3((i & 1), ((i >> 1) & 1), ((i >> 2) & 1));
        cube[i] = float(voxels.data[corner.x + corner.y * size + corner.z * size * size]) / 255.0;
    }
    
    // Вычисление индекса в таблице Marching Cubes
    int cubeIndex = 0;
    for (int i = 0; i < 8; ++i) {
        if (cube[i] > 0.5) cubeIndex |= 1 << i;
    }
    
    // Создание треугольников
    int edgeMask = edgeTable[cubeIndex];
    if (edgeMask == 0) return;
    
    // Вычисление вершин на рёбрах
    vec3 edgeVertices[12];
    for (int i = 0; i < 12; ++i) {
        if ((edgeMask & (1 << i)) != 0) {
            int a = edgeVertices[i][0];
            int b = edgeVertices[i][1];
            float t = (0.5 - cube[a]) / (cube[b] - cube[a]);
            edgeVertices[i] = mix(vec3(a & 1, (a >> 1) & 1, (a >> 2) & 1),
                                 vec3(b & 1, (b >> 1) & 1, (b >> 2) & 1), t);
        }
    }
    
    // Добавление треугольников в буферы
    for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
        uint vertexIdx = atomicAdd(counter.vertexCount, 3);
        uint indexIdx = atomicAdd(counter.indexCount, 3);
        
        vertexBuffer.vertices[vertexIdx] = vec4(edgeVertices[triTable[cubeIndex][i]] + vec3(voxelPos), 1.0);
        vertexBuffer.vertices[vertexIdx + 1] = vec4(edgeVertices[triTable[cubeIndex][i + 1]] + vec3(voxelPos), 1.0);
        vertexBuffer.vertices[vertexIdx + 2] = vec4(edgeVertices[triTable[cubeIndex][i + 2]] + vec3(voxelPos), 1.0);
        
        indexBuffer.indices[indexIdx] = vertexIdx;
        indexBuffer.indices[indexIdx + 1] = vertexIdx + 1;
        indexBuffer.indices[indexIdx + 2] = vertexIdx + 2;
    }
}
```

### Particle system

Высокопроизводительная система частиц на compute shaders.

```cpp
#version 450
layout(local_size_x = 256) in;

struct Particle {
    vec3 position;
    vec3 velocity;
    vec4 color;
    float life;
    float size;
};

layout(set = 0, binding = 0) buffer Particles {
    Particle particles[];
};

layout(set = 0, binding = 1) uniform Emitter {
    vec3 emitterPosition;
    vec3 emitterVelocity;
    float spawnRate;
    float time;
    float deltaTime;
} emitter;

layout(set = 0, binding = 2, std430) buffer AtomicIndex {
    uint count;
} particleCount;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    if (idx >= particles.length()) return;
    
    Particle particle = particles[idx];
    
    // Обновление существующих частиц
    if (particle.life > 0.0) {
        particle.velocity += vec3(0.0, -9.8, 0.0) * emitter.deltaTime;
        particle.position += particle.velocity * emitter.deltaTime;
        particle.life -= emitter.deltaTime;
        
        // Изменение цвета и размера
        particle.color.a = particle.life;
        particle.size *= 0.99;
        
        particles[idx] = particle;
    }
    // Создание новых частиц
    else if (emitter.spawnRate > 0.0 && idx < atomicAdd(particleCount.count, 1)) {
        particle.position = emitter.emitterPosition;
        particle.velocity = emitter.emitterVelocity + 
                           vec3(random() * 2.0 - 1.0,
                                random() * 2.0 - 1.0,
                                random() * 2.0 - 1.0) * 5.0;
        particle.color = vec4(random(), random(), random(), 1.0);
        particle.life = 1.0 + random() * 2.0;
        particle.size = 0.1 + random() * 0.2;
        
        particles[idx] = particle;
    }
}

// Функция случайных чисел
float random() {
    uint state = idx * 1103515245u + 12345u;
    state = (state >> 16u) ^ (state & 0xFFFFu);
    return float(state) / 65535.0;
}
```

### Image processing

Обработка изображений на GPU.

```cpp
#version 450
layout(local_size_x = 16, local_size_y = 16) in;
layout(set = 0, binding = 0, rgba8) uniform image2D inputImage;
layout(set = 0, binding = 1, rgba8) uniform image2D outputImage;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(inputImage);
    
    if (pixel.x >= size.x || pixel.y >= size.y) return;
    
    // Gaussian blur
    const float kernel[5][5] = float[5][5](
        {1.0/273, 4.0/273, 7.0/273, 4.0/273, 1.0/273},
        {4.0/273, 16.0/273, 26.0/273, 16.0/273, 4.0/273},
        {7.0/273, 26.0/273, 41.0/273, 26.0/273, 7.0/273},
        {4.0/273, 16.0/273, 26.0/273, 16.0/273, 4.0/273},
        {1.0/273, 4.0/273, 7.0/273, 4.0/273, 1.0/273}
    );
    
    vec4 color = vec4(0.0);
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            ivec2 samplePos = clamp(pixel + ivec2(x, y), ivec2(0), size - ivec2(1));
            color += imageLoad(inputImage, samplePos) * kernel[y + 2][x + 2];
        }
    }
    
    imageStore(outputImage, pixel, color);
}
```

---

## Геометрические шейдеры

### Нормали визуализация

Геометрический шейдер для отрисовки нормалей.

```cpp
#version 450
layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

in vec3 normal[];
in vec3 position[];

uniform mat4 viewProj;
uniform float normalLength = 0.1;

void main() {
    for (int i = 0; i < 3; ++i) {
        // Вершина треугольника
        gl_Position = viewProj * vec4(position[i], 1.0);
        EmitVertex();
        
        // Нормаль
        gl_Position = viewProj * vec4(position[i] + normal[i] * normalLength, 1.0);
        EmitVertex();
        
        EndPrimitive();
    }
}
```

### Триангуляция в геометрическом шейдере

Генерация дополнительной геометрии.

```cpp
#version 450
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 viewProj;
uniform float size = 0.5;

void main() {
    vec4 center = gl_in[0].gl_Position;
    
    // Генерация квада
    gl_Position = viewProj * (center + vec4(-size, -size, 0.0, 0.0));
    EmitVertex();
    
    gl_Position = viewProj * (center + vec4(size, -size, 0.0, 0.0));
    EmitVertex();
    
    gl_Position = viewProj * (center + vec4(-size, size, 0.0, 0.0));
    EmitVertex();
    
    gl_Position = viewProj * (center + vec4(size, size, 0.0, 0.0));
    EmitVertex();
    
    EndPrimitive();
}
```

---

## Тесселяция

### Динамическая тесселяция

Адаптивная тесселяция в зависимости от расстояния.

```cpp
#version 450
layout(vertices = 3) out;

uniform mat4 view;
uniform vec3 cameraPos;

void main() {
    if (gl_InvocationID == 0) {
        // Вычисление уровня тесселяции на основе расстояния
        vec3 center = (gl_in[0].gl_Position.xyz +
                      gl_in[1].gl_Position.xyz +
                      gl_in[2].gl_Position.xyz) / 3.0;
        
        float distance = length(cameraPos - center);
        float tessLevel = clamp(64.0 / (distance + 1.0), 1.0, 64.0);
        
        gl_TessLevelInner[0] = tessLevel;
        gl_TessLevelOuter[0] = tessLevel;
        gl_TessLevelOuter[1] = tessLevel;
        gl_TessLevelOuter[2] = tessLevel;
    }
    
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}

// Evaluation shader
#version 450
layout(triangles, equal_spacing, cw) in;

uniform mat4 viewProj;

void main() {
    // Barycentric coordinates
    vec3 barycentric = gl_TessCoord;
    
    // Interpolate position
    vec4 position = barycentric.x * gl_in[0].gl_Position +
                   barycentric.y * gl_in[1].gl_Position +
                   barycentric.z * gl_in[2].gl_Position;
    
    // Apply displacement mapping
    float height = texture(heightMap, gl_TessCoord.xy).r;
    position.y += height * 10.0;
    
    gl_Position = viewProj * position;
}
```

---

## Стереоскопический рендеринг

### VR рендеринг

Рендеринг для виртуальной реальности.

```cpp
// Создание двух viewport'ов
VkViewport viewports[2] = {
    {0.0f, 0.0f, width / 2.0f, height, 0.0f, 1.0f},
    {width / 2.0f, 0.0f, width / 2.0f, height, 0.0f, 1.0f}
};

VkRect2D scissors[2] = {
    {{0, 0}, {width / 2, height}},
    {{width / 2, 0}, {width / 2, height}}
};

vkCmdSetViewport(cmd, 0, 2, viewports);
vkCmdSetScissor(cmd, 0, 2, scissors);

// Матрицы для левого и правого глаза
struct EyeMatrices {
    glm::mat4 viewProjLeft;
    glm::mat4 viewProjRight;
};

// Шейдер для стерео рендеринга
#version 450
layout(push_constant) uniform Push {
    mat4 viewProjLeft;
    mat4 viewProjRight;
    int eyeIndex;
} push;

void main() {
    mat4 viewProj = push.eyeIndex == 0 ? push.viewProjLeft : push.viewProjRight;
    gl_Position = viewProj * vec4(inPosition, 1.0);
}
```

---

## Ray tracing

### Простой ray tracing

Использование расширения ray tracing.

```cpp
// Создание acceleration structures
VkAccelerationStructureGeometryKHR geometry = {};
geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
geometry.geometry.triangles.sType = 
    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
geometry.geometry.triangles.vertexData.deviceAddress = vertexBufferAddress;
geometry.geometry.triangles.maxVertex = vertexCount;
geometry.geometry.triangles.vertexStride = sizeof(Vertex);
geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
geometry.geometry.triangles.indexData.deviceAddress = indexBufferAddress;

// Ray generation shader
#version 460
#extension GL_EXT_ray_tracing : require

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, rgba8) uniform image2D outputImage;

layout(push_constant) uniform Push {
    mat4 viewInverse;
    mat4 projInverse;
} push;

void main() {
    vec2 pixel = vec2(gl_LaunchIDEXT.xy);
    vec2 resolution = vec2(gl_LaunchSizeEXT.xy);
    
    vec2 ndc = (pixel + 0.5) / resolution * 2.0 - 1.0;
    
    vec4 origin = push.viewInverse * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 target = push.projInverse * vec4(ndc, 1.0, 1.0);
    vec4 direction = push.viewInverse * vec4(normalize(target.xyz), 0.0);
    
    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.001;
    float tMax = 1000.0;
    
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, topLevelAS, rayFlags, 0xFF,
                         origin.xyz, tMin, direction.xyz, tMax);
    
    while (rayQueryProceedEXT(rayQuery)) {
        if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == 
            gl_RayQueryCandidateIntersectionTriangleEXT) {
            // Обработка пересечения с треугольником
            vec3 barycentric = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
            uint primitiveId = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
            
            // Вычисление цвета
            vec3 color = calculateColor(primitiveId, barycentric);
            imageStore(outputImage, ivec2(pixel), vec4(color, 1.0));
            
            rayQueryConfirmIntersectionEXT(rayQuery);
            break;
        }
    }
}
```

---

## Машинное обучение на GPU

### Матричные операции

Использование compute shaders для линейной алгебры.

```cpp
#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(set = 0, binding = 0) buffer MatrixA {
    float data[];
} matrixA;

layout(set = 0, binding = 1) buffer MatrixB {
    float data[];
} matrixB;

layout(set = 0, binding = 2) buffer MatrixC {
    float data[];
} matrixC;

layout(push_constant) uniform Push {
    uint M;
    uint N;
    uint K;
} sizes;

void main() {
    uint i = gl_GlobalInvocationID.x;
    uint j = gl_GlobalInvocationID.y;
    
    if (i >= sizes.M || j >= sizes.N) return;
    
    float sum = 0.0;
    for (uint k = 0; k < sizes.K; ++k) {
        float a = matrixA.data[i * sizes.K + k];
        float b = matrixB.data[k * sizes.N + j];
        sum += a * b;
    }
    
    matrixC.data[i * sizes.N + j] = sum;
}
```

### Свёрточные нейронные сети

Реализация слоёв CNN.

```cpp
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;

layout(set = 0, binding = 0, rgba32f) uniform image3D inputImage;
layout(set = 0, binding = 1, rgba32f) uniform image3D outputImage;
layout(set = 0, binding = 2) buffer Weights {
    float data[];
} weights;

layout(push_constant) uniform Push {
    ivec3 inputSize;
    ivec3 outputSize;
    ivec3 kernelSize;
    ivec3 stride;
    ivec3 padding;
} params;

void main() {
    ivec3 outputPos = ivec3(gl_GlobalInvocationID);
    if (any(greaterThanEqual(outputPos, params.outputSize))) return;
    
    vec4 sum = vec4(0.0);
    
    ivec3 inputStart = outputPos * params.stride - params.padding;
    
    for (int kz = 0; kz < params.kernelSize.z; ++kz) {
        for (int ky = 0; ky < params.kernelSize.y; ++ky) {
            for (int kx = 0; kx < params.kernelSize.x; ++kx) {
                ivec3 inputPos = inputStart + ivec3(kx, ky, kz);
                
                if (all(greaterThanEqual(inputPos, ivec3(0))) &&
                    all(lessThan(inputPos, params.inputSize))) {
                    
                    vec4 inputVal = imageLoad(inputImage, inputPos);
                    float weight = weights.data[
                        kx + ky * params.kernelSize.x + 
                        kz * params.kernelSize.x * params.kernelSize.y];
                    
                    sum += inputVal * weight;
                }
            }
        }
    }
    
    // ReLU activation
    sum = max(sum, vec4(0.0));
    
    imageStore(outputImage, outputPos, sum);
}
```

---

## 🧭 Навигация

### Следующие шаги

🟢 **[Быстрый старт Vulkan](quickstart.md)** — Практическое создание треугольника  
🟡 **[Производительность Vulkan](performance.md)** — Оптимизация и продвинутые техники  
🔴 **[ProjectV Integration](projectv-integration.md)** — Специфичные для ProjectV подходы

### Связанные разделы

🔗 **[Деревья решений Vulkan](decision-trees.md)** — Выбор правильных подходов  
🔗 **[Интеграция Vulkan](integration.md)** — Настройка с SDL3, volk, VMA  
🔗 **[Решение проблем Vulkan](troubleshooting.md)** — Отладка и типичные ошибки

← **[Назад к основной документации Vulkan](README.md)**