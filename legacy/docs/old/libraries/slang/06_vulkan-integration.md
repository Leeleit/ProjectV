# Интеграция Slang с Vulkan

**🟡 Уровень 2: Средний** — Интеграция Slang с Vulkan API: SPIR-V, bindings, push constants, buffer device address.

---

## Генерация SPIR-V

### Базовая компиляция

```bash
slangc shader.slang -o shader.spv -target spirv -profile spirv_1_5
```

### Профили SPIR-V и версии Vulkan

| Профиль SPIR-V | Vulkan версия         |
|----------------|-----------------------|
| spirv_1_3      | Vulkan 1.1            |
| spirv_1_4      | Vulkan 1.2 (частично) |
| spirv_1_5      | Vulkan 1.2+           |
| spirv_1_6      | Vulkan 1.3+           |

### Оптимизации

```bash
# Оптимизация для production
slangc shader.slang -o shader.spv -target spirv -O

# Без оптимизаций для отладки
slangc shader.slang -o shader.spv -target spirv -O0 -g
```

---

## Bindings

### Объявление ресурсов в шейдере

```slang
// Uniform buffer (constant buffer)
[[vk::binding(0, 0)]]
cbuffer Uniforms
{
    float4x4 viewProj;
    float3 cameraPos;
};

// Combined image sampler
[[vk::binding(1, 0)]]
Texture2D albedoTexture;

[[vk::binding(2, 0)]]
SamplerState linearSampler;

// Storage buffer (SSBO)
[[vk::binding(3, 0)]]
StructuredBuffer<float> inputData;

[[vk::binding(4, 0)]]
RWStructuredBuffer<float> outputData;

// Storage image
[[vk::binding(5, 0)]]
RWTexture2D<float4> outputImage;
```

### Соответствие VkDescriptorSetLayoutBinding

```slang
// Шейдер
[[vk::binding(0, 0)]] cbuffer Uniforms { float4x4 viewProj; };
[[vk::binding(1, 0)]] Texture2D albedo;
[[vk::binding(2, 0)]] SamplerState sampler;
```

```cpp
// C++
VkDescriptorSetLayoutBinding bindings[] = {
    { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
    { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    { 2, VK_DESCRIPTOR_TYPE_SAMPLER,        1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
};
```

### Binding shifts

Для совместимости с HLSL-стилем объявлений:

```bash
# Сдвиги для register(b#, t#, s#, u#)
slangc shader.slang -target spirv \
    -fvk-b-shift 0 0   # b-registers → set 0, binding с 0
    -fvk-t-shift 0 1   # t-registers → set 1, binding с 0
    -fvk-s-shift 0 2   # s-registers → set 2, binding с 0
    -fvk-u-shift 0 3   # u-registers → set 3, binding с 0
```

---

## Push Constants

### Объявление в шейдере

```slang
[[vk::push_constant]]
struct PushConstants
{
    float4x4 model;
    float4x4 viewProj;
    float3   cameraPos;
    uint     frameIndex;
} pc;

// Использование
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(pc.viewProj, mul(pc.model, float4(input.position, 1.0)));
    return output;
}
```

### Создание VkPushConstantRange

```cpp
// Получение размера через reflection
size_t pushConstantsSize = layout->getPushConstantsByIndex(0)->getSizeInBytes();

// Или вручную
struct PushConstants
{
    glm::mat4 model;
    glm::mat4 viewProj;
    glm::vec3 cameraPos;
    uint32_t  frameIndex;
};

VkPushConstantRange range{};
range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
range.offset = 0;
range.size = sizeof(PushConstants);  // Должно совпадать с шейдером
```

### Обновление push constants

```cpp
PushConstants pc{};
pc.model = glm::mat4(1.0f);
pc.viewProj = camera.getViewProj();
pc.cameraPos = camera.getPosition();
pc.frameIndex = frame;

vkCmdPushConstants(
    commandBuffer,
    pipelineLayout,
    VK_SHADER_STAGE_VERTEX_BIT,
    0,
    sizeof(PushConstants),
    &pc
);
```

---

## Buffer Device Address

Требует Vulkan 1.2+ и расширения `VK_KHR_buffer_device_address`.

### Объявление в шейдере

```slang
// Тип указателя на буфер
[[vk::buffer_reference]]
struct DeviceBuffer
{
    float data[];
};

// С выравниванием
[[vk::buffer_reference, vk::buffer_reference_align(16)]]
struct AlignedBuffer
{
    float4 data[];
};
```

### Использование

```slang
[[vk::push_constant]]
struct PC
{
    DeviceBuffer inputBuffer;
    DeviceBuffer outputBuffer;
    uint count;
} pc;

[numthreads(64, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x < pc.count)
    {
        float value = pc.inputBuffer.data[id.x];
        pc.outputBuffer.data[id.x] = value * 2.0;
    }
}
```

### Получение адреса буфера

```cpp
VkBufferDeviceAddressInfo addressInfo{};
addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
addressInfo.buffer = buffer;

VkDeviceAddress address = vkGetBufferDeviceAddress(device, &addressInfo);

// Передача в push constants
struct PC
{
    VkDeviceAddress inputBuffer;
    VkDeviceAddress outputBuffer;
    uint32_t count;
};

PC pc{};
pc.inputBuffer = inputAddress;
pc.outputBuffer = outputAddress;
pc.count = elementCount;
```

---

## Descriptor Indexing (Bindless)

Требует Vulkan 1.2+ и расширения `VK_EXT_descriptor_indexing`.

### Объявление массива текстур

```slang
// Массив текстур
[[vk::binding(0, 0)]]
Texture2D materialTextures[];

[[vk::binding(1, 0)]]
SamplerState linearSampler;

// Сэмплинг с динамическим индексом
float4 sampleMaterial(uint textureIndex, float2 uv)
{
    return materialTextures[NonUniformResourceIndex(textureIndex)].Sample(
        linearSampler,
        uv
    );
}
```

### Настройка VkDescriptorSetLayoutBinding

```cpp
VkDescriptorSetLayoutBinding binding{};
binding.binding = 0;
binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
binding.descriptorCount = MAX_TEXTURES;  // Большой массив
binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

// Включить partial binding и update after bind
VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
VkDescriptorBindingFlags bindingFlags =
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
flagsInfo.bindingCount = 1;
flagsInfo.pBindingFlags = &bindingFlags;
```

---

## Специализационные константы

### Объявление в шейдере

```slang
[[vk::constant_id(0)]]
const uint WORKGROUP_SIZE = 64;

[[vk::constant_id(1)]]
const bool ENABLE_SHADOWS = true;

[[vk::constant_id(2)]]
const float SHADOW_BIAS = 0.001;

[numthreads(WORKGROUP_SIZE, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    if (ENABLE_SHADOWS) {
        // Логика с тенями
    }
}
```

### Настройка VkSpecializationInfo

```cpp
struct SpecializationData
{
    uint32_t workgroupSize;
    VkBool32 enableShadows;
    float shadowBias;
};

SpecializationData specData{};
specData.workgroupSize = 128;
specData.enableShadows = VK_TRUE;
specData.shadowBias = 0.0005f;

VkSpecializationMapEntry entries[] = {
    { 0, offsetof(SpecializationData, workgroupSize), sizeof(uint32_t) },
    { 1, offsetof(SpecializationData, enableShadows), sizeof(VkBool32) },
    { 2, offsetof(SpecializationData, shadowBias), sizeof(float) },
};

VkSpecializationInfo specInfo{};
specInfo.mapEntryCount = 3;
specInfo.pMapEntries = entries;
specInfo.dataSize = sizeof(SpecializationData);
specInfo.pData = &specData;

// Применение при создании pipeline
VkPipelineShaderStageCreateInfo shaderStage{};
shaderStage.pSpecializationInfo = &specInfo;
```

---

## Матричный порядок

### Row-major vs Column-major

```bash
# Для совместимости с C/C++ (row-major)
slangc shader.slang -target spirv -enable-slang-matrix-layout-row-major
```

```slang
// Явное указание в шейдере
struct Transform
{
    row_major float4x4 model;
    row_major float4x4 viewProj;
};
```

### Соответствие с glm

```cpp
// glm использует column-major хранение, но row-major умножение
glm::mat4 mvp = proj * view * model;

// Если шейдер ожидает row-major
glm::mat4 mvpRowMajor = glm::transpose(mvp);

// Или использовать флаг компилятора
// -enable-slang-matrix-layout-row-major
```

---

## Entry points

### Именование entry points

```slang
// Имена entry points сохраняются
VertexOutput vsMain(VertexInput input) { /* ... */ }
float4 fsMain(VertexOutput input) : SV_Target { /* ... */ }
```

```cpp
// VkPipelineShaderStageCreateInfo
VkPipelineShaderStageCreateInfo vertStage{};
vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
vertStage.pName = "vsMain";  // Имя из шейдера

VkPipelineShaderStageCreateInfo fragStage{};
fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
fragStage.pName = "fsMain";
```

### Переименование entry point

```bash
# Переименовать vsMain в main
slangc shader.slang -entry vsMain -stage vertex \
    -rename-entry-point vsMain main \
    -o vertex.spv -target spirv
```

---

## Создание Vulkan Pipeline

### Загрузка SPIR-V

```cpp
std::vector<uint32_t> readSPIRV(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    size_t size = file.tellg();
    file.seekg(0);

    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), size);
    return code;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule module;
    vkCreateShaderModule(device, &createInfo, nullptr, &module);
    return module;
}
```

### Создание graphics pipeline

```cpp
// Загрузка шейдеров
VkShaderModule vertModule = createShaderModule(device, readSPIRV("vertex.spv"));
VkShaderModule fragModule = createShaderModule(device, readSPIRV("fragment.spv"));

VkPipelineShaderStageCreateInfo stages[] = {
    { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_VERTEX_BIT, vertModule, "vsMain", nullptr },
    { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "fsMain", nullptr },
};

// Создание pipeline
VkGraphicsPipelineCreateInfo pipelineInfo{};
pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
pipelineInfo.stageCount = 2;
pipelineInfo.pStages = stages;
// ... остальные настройки

VkPipeline pipeline;
vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
```

### Создание compute pipeline

```cpp
VkShaderModule computeModule = createShaderModule(device, readSPIRV("compute.spv"));

VkPipelineShaderStageCreateInfo computeStage{};
computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
computeStage.module = computeModule;
computeStage.pName = "csMain";

VkComputePipelineCreateInfo pipelineInfo{};
pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
pipelineInfo.stage = computeStage;
pipelineInfo.layout = pipelineLayout;

VkPipeline pipeline;
vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
```

---

## Vertex Input

### Соответствие структуры шейдера

```slang
struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};
```

```cpp
VkVertexInputBindingDescription binding{};
binding.binding = 0;
binding.stride = sizeof(float) * 8;  // 3 + 3 + 2
binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

VkVertexInputAttributeDescription attributes[] = {
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },                  // position
    { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3 },  // normal
    { 2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6 },     // uv
};
```

---

## Отладка

### Валидация SPIR-V

```bash
# Установить переменную для валидации
export SLANG_RUN_SPIRV_VALIDATION=1
slangc shader.slang -target spirv -o shader.spv
```

### Просмотр SPIR-V

```bash
# Текстовый формат SPIR-V
slangc shader.slang -target spirv-asm -o shader.spvasm

# Транспиляция в GLSL
slangc shader.slang -target glsl -o shader.glsl
```

### RenderDoc

```bash
# Компиляция с отладочной информацией
slangc shader.slang -target spirv -o shader.spv -g -O0
```

RenderDoc покажет исходный код Slang при наличии debug info.
