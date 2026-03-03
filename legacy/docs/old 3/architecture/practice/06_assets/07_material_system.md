# Material System

**🟡 Уровень 2: Средний** — Система материалов для воксельного рендеринга.

---

## Концепция

### Что такое Material?

Material — это набор параметров, определяющих внешний вид поверхности при рендеринге:

| Параметр           | Описание                           |
|--------------------|------------------------------------|
| **Albedo/Diffuse** | Базовый цвет поверхности           |
| **Normal Map**     | Имитация мелких деталей            |
| **Roughness**      | Степень шероховатости (PBR)        |
| **Metallic**       | Металлический или диэлектрик (PBR) |
| **Emissive**       | Свечение                           |
| **Shader**         | Как эти параметры комбинируются    |

### Архитектура системы

```
Material File (.mat) → Material Compiler → GPU Material Buffer
       ↓                                           ↓
   Slang Shader → Slang Compiler → SPIR-V → Pipeline
```

---

## Material File Format

### JSON Format (.mat)

```json
{
    "name": "stone_bricks",
    "shader": "pbr_static",
    "parameters": {
        "albedo": {
            "type": "texture",
            "value": "textures/stone_bricks_diffuse"
        },
        "normal": {
            "type": "texture",
            "value": "textures/stone_bricks_normal"
        },
        "roughness": {
            "type": "float",
            "value": 0.85
        },
        "metallic": {
            "type": "float",
            "value": 0.0
        },
        "albedoColor": {
            "type": "color",
            "value": [1.0, 1.0, 1.0, 1.0]
        }
    },
    "renderState": {
        "cullMode": "back",
        "depthTest": true,
        "depthWrite": true,
        "blend": "opaque"
    }
}
```

### Примеры материалов

**Glass (прозрачный):**

```json
{
    "name": "glass",
    "shader": "pbr_transparent",
    "parameters": {
        "albedo": {
            "type": "color",
            "value": [0.8, 0.9, 1.0, 0.3]
        },
        "roughness": 0.05,
        "metallic": 0.0
    },
    "renderState": {
        "cullMode": "none",
        "depthTest": true,
        "depthWrite": false,
        "blend": "alpha"
    }
}
```

**Emissive (светящийся):**

```json
{
    "name": "lava",
    "shader": "unlit_emissive",
    "parameters": {
        "emissiveColor": {
            "type": "color",
            "value": [1.0, 0.3, 0.1, 1.0]
        },
        "emissiveIntensity": 5.0
    },
    "renderState": {
        "cullMode": "back",
        "depthTest": true,
        "depthWrite": true,
        "blend": "additive"
    }
}
```

---

## Material Runtime

### Структуры данных

```cpp
// Параметры материала для GPU
struct GPUMaterialData {
    glm::vec4 albedoColor;
    glm::vec4 emissiveColor;
    float roughness;
    float metallic;
    float emissiveIntensity;
    uint32_t flags;          // Bit flags для текстур

    uint32_t albedoTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t roughnessTextureIndex;
    uint32_t metallicTextureIndex;
};

// Материал на CPU
class Material {
public:
    Material(const std::string& name, const std::string& shaderName);
    ~Material();

    // Загрузка из файла
    static std::shared_ptr<Material> load(const std::filesystem::path& path);

    // Параметры
    void setTexture(const std::string& name, TextureHandle texture);
    void setFloat(const std::string& name, float value);
    void setColor(const std::string& name, const glm::vec4& value);

    // GPU данные
    GPUMaterialData getGPUData() const;

    // Render state
    VkPipeline getPipeline() const { return pipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }

private:
    std::string name_;
    std::string shaderName_;

    // Текстуры
    std::unordered_map<std::string, TextureHandle> textures_;

    // Параметры
    std::unordered_map<std::string, float> floatParams_;
    std::unordered_map<std::string, glm::vec4> colorParams_;

    // Render state
    VkPipeline pipeline_;
    VkPipelineLayout pipelineLayout_;
    VkDescriptorSet descriptorSet_;
};
```

### Material Manager

```cpp
class MaterialManager {
public:
    MaterialManager(VkDevice device, VmaAllocator allocator);
    ~MaterialManager();

    // Загрузка материалов
    MaterialHandle loadMaterial(const std::string& name);
    MaterialHandle getMaterial(const std::string& name);

    // GPU буфер материалов
    VkBuffer getMaterialBuffer() const { return materialBuffer_; }
    uint32_t getMaterialIndex(MaterialHandle handle) const;

    // Обновление (вызывать при изменении материалов)
    void updateMaterialBuffer();

    // Текстуры
    TextureHandle loadTexture(const std::string& path);

private:
    VkDevice device_;
    VmaAllocator allocator_;

    // Кэш материалов
    std::unordered_map<std::string, std::shared_ptr<Material>> materials_;

    // GPU буфер для всех материалов
    VkBuffer materialBuffer_;
    VmaAllocation materialAllocation_;
    GPUMaterialData* mappedMaterialData_;
    uint32_t nextMaterialIndex_ = 0;

    // Текстуры (bindless)
    std::unordered_map<std::string, TextureHandle> textureCache_;
    std::vector<VkImageView> textureViews_;
    VkDescriptorSet textureDescriptorSet_;
};
```

---

## Shader System (Slang)

### Почему Slang?

| Критерий                 | GLSL | HLSL | Slang |
|--------------------------|------|------|-------|
| Cross-compilation        | ✅    | ⚠️   | ✅     |
| Modules/Imports          | ❌    | ✅    | ✅     |
| Specialization Constants | ⚠️   | ❌    | ✅     |
| Reflection API           | ⚠️   | ⚠️   | ✅     |
| Modern syntax            | ❌    | ✅    | ✅     |

Slang — современный шейдерный язык с поддержкой модулей, удобной рефлексией и компиляцией в SPIR-V.

### Базовый PBR Shader (Slang)

```slang
// shaders/common/pbr_common.slang
module pbr_common;

import vulkan;

// Константы
static const float PI = 3.14159265359;

// Структуры
struct MaterialData {
    float4 albedoColor;
    float4 emissiveColor;
    float roughness;
    float metallic;
    float emissiveIntensity;
    uint flags;
    uint albedoTextureIndex;
    uint normalTextureIndex;
    uint roughnessTextureIndex;
    uint metallicTextureIndex;
};

// PBR функции
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
```

```slang
// shaders/pbr_static.slang
import pbr_common;

// Push Constants
[push_constant]
cbuffer PushConstants {
    float4x4 viewProj;
    float3 cameraPos;
    uint materialIndex;
};

// Bindless textures
[[vk::binding(0, 0)]]
Texture2D textures[];

[[vk::binding(1, 0)]]
SamplerState sampler_;

[[vk::binding(2, 0)]]
StructuredBuffer<MaterialData> materials;

// Vertex input
struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 texCoord : TEXCOORD0;
};

// Vertex output
struct VSOutput {
    float4 position : SV_Position;
    float3 worldPos : WorldPos;
    float3 normal : Normal;
    float2 texCoord : TexCoord;
};

VSOutput vertexMain(VSInput input) {
    VSOutput output;
    output.position = mul(viewProj, float4(input.position, 1.0));
    output.worldPos = input.position;
    output.normal = input.normal;
    output.texCoord = input.texCoord;
    return output;
}

float4 fragmentMain(VSOutput input) : SV_Target {
    MaterialData mat = materials[materialIndex];

    // Albedo
    float3 albedo = mat.albedoColor.rgb;
    if (mat.flags & 0x01) {  // Has albedo texture
        albedo *= textures[mat.albedoTextureIndex].Sample(sampler_, input.texCoord).rgb;
    }

    // Normal
    float3 N = normalize(input.normal);
    if (mat.flags & 0x02) {  // Has normal texture
        float3 normalMap = textures[mat.normalTextureIndex].Sample(sampler_, input.texCoord).rgb;
        normalMap = normalMap * 2.0 - 1.0;
        N = normalize(N + normalMap * 0.5);
    }

    // Roughness/Metallic
    float roughness = mat.roughness;
    float metallic = mat.metallic;

    // PBR lighting
    float3 V = normalize(cameraPos - input.worldPos);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // Directional light (пример)
    float3 L = normalize(float3(1.0, 1.0, 1.0));
    float3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
              GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 diffuse = kD * albedo / PI;

    float NdotL = max(dot(N, L), 0.0);
    float3 Lo = (diffuse + specular) * NdotL * float3(1.0, 0.98, 0.95);  // Light color

    // Ambient (упрощённый)
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo;

    // Emissive
    float3 emissive = mat.emissiveColor.rgb * mat.emissiveIntensity;

    return float4(ambient + Lo + emissive, 1.0);
}
```

### Компиляция Slang → SPIR-V

```cpp
#include <slang.h>

class ShaderCompiler {
public:
    std::vector<uint32_t> compileToSPIRV(const std::filesystem::path& path) {
        // Создание сессии Slang
        slang::SessionDesc sessionDesc = {};
        slang::TargetDesc targetDesc = {};

        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = session_->findProfile("spirv_1_5");

        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;

        slang::ISession* session;
        session_->createSession(sessionDesc, &session);

        // Загрузка модуля
        slang::IModule* module;
        Slang::ComPtr<slang::IBlob> diagnostics;
        module = session->loadModule(path.string().c_str(), diagnostics.writeRef());

        if (diagnostics) {
            std::cerr << "Shader compilation error: "
                      << (const char*)diagnostics->getBufferPointer() << std::endl;
            return {};
        }

        // Поиск entry points
        Slang::ComPtr<slang::IEntryPoint> vertexEntry;
        Slang::ComPtr<slang::IEntryPoint> fragmentEntry;
        module->findEntryPointByName("vertexMain", vertexEntry.writeRef());
        module->findEntryPointByName("fragmentMain", fragmentEntry.writeRef());

        // Компиляция
        Slang::ComPtr<slang::IComponentType> program;
        slang::IComponentType* components[] = {module, vertexEntry, fragmentEntry};
        session->createCompositeType(components, 3, program.writeRef(), diagnostics.writeRef());

        Slang::ComPtr<slang::IBlob> spirvCode;
        program->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

        // Конвертация в uint32_t vector
        const uint32_t* data = static_cast<const uint32_t*>(spirvCode->getBufferPointer());
        size_t size = spirvCode->getBufferSize() / sizeof(uint32_t);
        return std::vector<uint32_t>(data, data + size);
    }

private:
    slang::IGlobalSession* session_;
};
```

---

## Material Variants

### Переключение шейдеров

Для разных материалов нужны разные шейдеры:

| Shader            | Для чего                            |
|-------------------|-------------------------------------|
| `pbr_static`      | Статические объекты                 |
| `pbr_animated`    | Анимированные модели (skinning)     |
| `pbr_transparent` | Стекло, вода                        |
| `unlit_emissive`  | Лава, светящиеся объекты            |
| `foliage`         | Растительность (wind animation)     |
| `voxel_block`     | Воксельные блоки (оптимизированный) |

### Генерация Pipeline

```cpp
class PipelineCache {
public:
    VkPipeline getOrCreatePipeline(const Material& material) {
        size_t hash = computePipelineHash(material);

        auto it = cache_.find(hash);
        if (it != cache_.end()) {
            return it->second;
        }

        VkPipeline pipeline = createPipeline(material);
        cache_[hash] = pipeline;
        return pipeline;
    }

private:
    size_t computePipelineHash(const Material& material) {
        size_t hash = 0;
        hashCombine(hash, std::hash<std::string>{}(material.getShaderName()));
        hashCombine(hash, std::hash<int>{}(material.getCullMode()));
        hashCombine(hash, std::hash<int>{}(material.getBlendMode()));
        hashCombine(hash, std::hash<bool>{}(material.getDepthWrite()));
        return hash;
    }

    VkPipeline createPipeline(const Material& material) {
        // Загрузка шейдера
        auto vertexSpirv = shaderCache_.getSPIRV(material.getShaderName(), "vertexMain");
        auto fragmentSpirv = shaderCache_.getSPIRV(material.getShaderName(), "fragmentMain");

        // Создание shader modules
        VkShaderModule vertexModule = createShaderModule(vertexSpirv);
        VkShaderModule fragmentModule = createShaderModule(fragmentSpirv);

        // Настройка pipeline
        VkGraphicsPipelineCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            // ... настройка stages, vertex input, rasterization, etc.
        };

        // Render state из материала
        VkPipelineRasterizationStateCreateInfo rasterization = {
            .cullMode = vkCullMode(material.getCullMode()),
            // ...
        };

        VkPipelineColorBlendStateCreateInfo blend = {
            .attachmentCount = 1,
            .pAttachments = &blendAttachment(material.getBlendMode()),
        };

        VkPipeline pipeline;
        vkCreateGraphicsPipelines(device_, pipelineCache_, 1, &createInfo, nullptr, &pipeline);

        return pipeline;
    }

    std::unordered_map<size_t, VkPipeline> cache_;
    VkPipelineCache pipelineCache_;
};
```

---

## Интеграция с вокселями

### Материал для воксельного блока

```cpp
// Воксельный материал (оптимизированный)
struct VoxelMaterial {
    uint8_t textureIndex;      // Индекс в texture atlas
    uint8_t roughness;         // 0-255 → 0.0-1.0
    uint8_t metallic;          // 0-255 → 0.0-1.0
    uint8_t flags;             // Битовые флаги

    // Расшифровка flags:
    // bit 0: hasDiffuseTexture
    // bit 1: hasNormalTexture
    // bit 2: isTransparent
    // bit 3: isEmissive
};

// Воксельный блок
struct VoxelBlock {
    VoxelMaterial material;
    uint8_t health;            // Для разрушаемых блоков
    uint8_t light;             // Освещение
    uint8_t variant;           // Вариант текстуры
};

// Texture Atlas для вокселей
class VoxelTextureAtlas {
public:
    void addTexture(const std::string& name, const std::filesystem::path& path) {
        // Добавление текстуры в atlas
        textures_.push_back({name, path});
        dirty_ = true;
    }

    void build() {
        if (!dirty_) return;

        // Генерация atlas
        constexpr int ATLAS_SIZE = 4096;
        constexpr int TILE_SIZE = 16;

        int tilesPerRow = ATLAS_SIZE / TILE_SIZE;

        for (size_t i = 0; i < textures_.size(); i++) {
            int x = (i % tilesPerRow) * TILE_SIZE;
            int y = (i / tilesPerRow) * TILE_SIZE;

            // Копирование текстуры в atlas
            copyTextureToAtlas(textures_[i].path, x, y, TILE_SIZE);
        }

        // Загрузка atlas в GPU
        uploadToGPU();

        dirty_ = false;
    }

    glm::vec4 getUVRect(uint8_t textureIndex) const {
        constexpr int ATLAS_SIZE = 4096;
        constexpr int TILE_SIZE = 16;
        int tilesPerRow = ATLAS_SIZE / TILE_SIZE;

        float u = (textureIndex % tilesPerRow) * TILE_SIZE / float(ATLAS_SIZE);
        float v = (textureIndex / tilesPerRow) * TILE_SIZE / float(ATLAS_SIZE);
        float s = TILE_SIZE / float(ATLAS_SIZE);

        return glm::vec4(u, v, u + s, v + s);
    }

private:
    struct TextureEntry {
        std::string name;
        std::filesystem::path path;
    };

    std::vector<TextureEntry> textures_;
    VkImage atlasImage_;
    VkImageView atlasView_;
    bool dirty_ = false;
};
```

---

## Material Hot-Reload

```cpp
class MaterialHotReloader {
public:
    void watchDirectory(const std::filesystem::path& path) {
        watchPath_ = path;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.path().extension() == ".mat") {
                lastWriteTimes_[entry.path()] =
                    std::filesystem::last_write_time(entry.path());
            }
        }
    }

    void checkForChanges() {
        for (const auto& [path, lastTime] : lastWriteTimes_) {
            auto currentTime = std::filesystem::last_write_time(path);
            if (currentTime != lastTime) {
                reloadMaterial(path);
                lastWriteTimes_[path] = currentTime;
            }
        }
    }

private:
    void reloadMaterial(const std::filesystem::path& path) {
        // Разбор имени материала из пути
        std::string materialName = path.stem().string();

        // Перезагрузка материала
        materialManager_->reloadMaterial(materialName);

        SDL_Log("Material reloaded: %s", materialName.c_str());
    }

    std::filesystem::path watchPath_;
    std::unordered_map<std::filesystem::path,
                       std::filesystem::file_time_type> lastWriteTimes_;
    MaterialManager* materialManager_;
};
```

---

## Рекомендации

### Performance

1. **Bindless textures:** Используйте bindless доступ к текстурам
2. **Material buffer:** Один буфер для всех материалов
3. **Pipeline caching:** Кэшируйте pipelines по хэшу render state
4. **Texture atlas:** Для вокселей — один atlas вместо тысяч текстур

### Organization

```
materials/
├── stone_bricks.mat
├── dirt.mat
├── glass.mat
└── lava.mat

shaders/
├── common/
│   └── pbr_common.slang
├── pbr_static.slang
├── pbr_transparent.slang
└── unlit_emissive.slang

textures/
├── stone_bricks_diffuse.png
├── stone_bricks_normal.png
└── ...
```

---

## Резюме

**Material = Shader + Parameters + Render State**

**Компоненты системы:**

- `.mat` JSON файлы для определения материалов
- Slang shaders для GPU кода
- Material Buffer для bindless доступа
- Pipeline Cache для эффективного переключения

**Выгода:**

- Декларативное определение материалов
- Быстрое переключение между материалами
- Hot-reload для итераций
- Оптимизация под воксельный рендеринг
