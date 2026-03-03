## Установка Slang

<!-- anchor: 02_installation -->

**🟢 Уровень 1: Начинающий** — Способы установки Slang: Vulkan SDK, бинарные пакеты, сборка из исходников.

---

## Вариант A: Vulkan SDK (рекомендуется)

Slang включён в Vulkan SDK начиная с версии 1.3.296.0. Это рекомендуемый способ для большинства пользователей.

### Проверка установки

```bash
# Проверьте наличие slangc в PATH
slangc --version

# Если команда не найдена, убедитесь, что Vulkan SDK установлен
# и переменная VULKAN_SDK указывает на правильный путь
```

### Установка Vulkan SDK

Скачайте последнюю версию с официального сайта:

```
https://vulkan.lunarg.com/sdk/home
```

После установки проверьте переменную окружения:

```bash
# Linux/macOS
echo $VULKAN_SDK
# Ожидаемый вывод: /path/to/VulkanSDK/1.3.xxx.0/... или аналогичный

# Windows (PowerShell)
echo $env:VULKAN_SDK
```

---

## Вариант B: Бинарные пакеты

Если Vulkan SDK недоступен или нужна последняя версия Slang, скачайте бинарный пакет с GitHub Releases.

### Windows

```bash
# Скачивание
curl -LO https://github.com/shader-slang/slang/releases/latest/download/slang-win64-release.zip

# Распаковка
unzip slang-win64-release.zip -d C:/slang

# Добавление в PATH (PowerShell)
$env:PATH += ";C:\slang\bin"
```

### Linux

```bash
# Скачивание
curl -LO https://github.com/shader-slang/slang/releases/latest/download/slang-linux-x86_64-release.tar.gz

# Распаковка
tar -xzf slang-linux-x86_64-release.tar.gz -C /usr/local

# Обновление кэша библиотек
sudo ldconfig
```

### macOS

```bash
# Скачивание
curl -LO https://github.com/shader-slang/slang/releases/latest/download/slang-macos-release.zip

# Распаковка
unzip slang-macos-release.zip -d /usr/local
```

---

## Вариант C: Сборка из исходников

Для разработки или использования последних изменений.

### Требования для сборки

- **CMake 3.25+**
- **C++20 совместимый компилятор** (MSVC 2022, GCC 11+, Clang 14+)
- **Git** (для клонирования подмодулей)

### Клонирование репозитория

```bash
git clone https://github.com/shader-slang/slang.git
cd slang

# Инициализация подмодулей
git submodule update --init --recursive
```

### Конфигурация и сборка

```bash
# Создание директории сборки
mkdir build && cd build

# Конфигурация (Linux/macOS)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Конфигурация (Windows, Visual Studio 2022)
cmake .. -G "Visual Studio 17 2022" -A x64

# Сборка (параллельная, использует все ядра)
cmake --build . --config Release --parallel

# Установка (опционально)
cmake --install . --prefix /usr/local
```

### Сборка конкретных целей

```bash
# Только компилятор slangc
cmake --build . --config Release --target slangc

# Только библиотека Slang
cmake --build . --config Release --target slang

# Тесты
cmake --build . --config Release --target slang-test
```

---

## Интеграция с CMake

### find_package

```cmake
# Поиск установленного Slang
find_package(Slang REQUIRED)

# Использование
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE slang::slang)
```

### add_subdirectory

```cmake
# Если Slang включён как подмодуль проекта
add_subdirectory(external/slang)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE slang)
```

### Поиск slangc исполняемого файла

```cmake
# Найти slangc для компиляции шейдеров
find_program(SLANGC_EXECUTABLE slangc
    HINTS
        $ENV{VULKAN_SDK}/bin
        ${CMAKE_SOURCE_DIR}/external/slang/build/bin
)

if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc not found")
endif()
```

---

## Проверка установки

### Командная строка

```bash
# Проверка версии
slangc --version

# Пример вывода:
# slangc version 2025.x.x

# Тестовая компиляция
echo "[numthreads(1,1,1)] void main() {}" > test.slang
slangc test.slang -target spirv -o test.spv
rm test.slang test.spv
```

### CMake тест

```cmake
cmake_minimum_required(VERSION 3.25)
project(slang_test)

find_package(Slang REQUIRED)

add_executable(slang_test main.cpp)
target_link_libraries(slang_test PRIVATE slang::slang)
```

```cpp
// main.cpp
#include <slang.h>
#include <iostream>

int main() {
    Slang::ComPtr<slang::IGlobalSession> globalSession;
    SlangResult result = slang::createGlobalSession(globalSession.writeRef());

    if (SLANG_SUCCEEDED(result)) {
        std::cout << "Slang initialized successfully" << std::endl;
        return 0;
    } else {
        std::cerr << "Failed to initialize Slang" << std::endl;
        return 1;
    }
}
```

---

## Структура установки

После установки или сборки структура директорий:

```
slang/
├── bin/
│   ├── slangc           # Компилятор командной строки
│   ├── slangd           # Language daemon (LSP)
│   └── slang-test       # Тестовый раннер
├── lib/
│   ├── slang.so / slang.dll    # Основная библиотека
│   ├── slang-rt.so / slang-rt.dll  # Runtime библиотека
│   └── cmake/
│       └── slang/       # CMake config файлы
└── include/
    ├── slang.h          # Основной заголовок
    ├── slang-com-ptr.h  # ComPtr helper
    └── slang-gfx.h      # Graphics layer (опционально)
```

---

## Устранение неполадок

### slangc не найден

```bash
# Проверьте PATH
which slangc        # Linux/macOS
where slangc        # Windows

# Добавьте в PATH временно
export PATH=$PATH:/path/to/slang/bin  # Linux/macOS
$env:PATH += ";C:\path\to\slang\bin"  # Windows PowerShell
```

### Ошибка загрузки библиотеки

```bash
# Linux: проверьте кэш библиотек
ldconfig -p | grep slang

# Обновите кэш
sudo ldconfig

# Или установите LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Конфликт версий

Если установлены несколько версий (Vulkan SDK + локальная сборка):

```bash
# Проверьте, какая версия используется
slangc --version

# Явно укажите путь в CMake
set(Slang_DIR "/path/to/slang/lib/cmake/slang")
find_package(Slang REQUIRED)

---

## Интеграция Slang с Vulkan

<!-- anchor: 06_vulkan-integration -->

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

---

## Slang в ProjectV: Обзор интеграции

<!-- anchor: 12_projectv-overview -->

**🟡 Уровень 2: Средний** — Почему Slang выбран для воксельного движка ProjectV и как он интегрируется в архитектуру.

---

## Почему Slang для ProjectV

### Ключевые причины выбора

1. **Модульная организация** — воксельный движок имеет сложную шейдерную кодобазу с множеством типов вокселей,
   материалов и техник рендеринга. Модульная система Slang позволяет организовать код без дублирования.

2. **Generics для вокселей** — различные типы вокселей (SimpleVoxel, MaterialVoxel, SDFVoxel) требуют параметризованных
   шейдеров. Slang generics решают эту задачу без copy-paste.

3. **Инкрементальная компиляция** — быстрая итерация при разработке сложных шейдеров для воксельного рендеринга.

4. **Vulkan-ориентированность** — первоклассная поддержка SPIR-V и современных Vulkan-расширений (Buffer Device Address,
   Descriptor Indexing).

### Связь с другими компонентами

| Компонент     | Интеграция с Slang                                      |
|---------------|---------------------------------------------------------|
| **Vulkan**    | Основной API рендеринга, Slang генерирует SPIR-V        |
| **volk**      | Загрузка Vulkan-функций, совместим с Slang SPIR-V       |
| **VMA**       | Выделение памяти для буферов, используемых в шейдерах   |
| **flecs ECS** | Шейдеры могут использовать данные ECS-компонентов       |
| **Tracy**     | Профилирование времени компиляции и выполнения шейдеров |

---

## Архитектура шейдерной системы

### Структура директорий

```
ProjectV/
├── shaders/                    # Исходники Slang шейдеров
│   ├── core/                   # Базовые модули (редко меняются)
│   │   ├── types.slang         # VoxelData, ChunkHeader, MaterialData
│   │   ├── math.slang          # Математические утилиты
│   │   └── constants.slang     # CHUNK_SIZE, MAX_LOD, INVALID_INDEX
│   │
│   ├── voxel/                  # Воксельные шейдеры
│   │   ├── data/               # Структуры данных
│   │   │   ├── chunk.slang     # VoxelChunk generic
│   │   │   ├── octree.slang    # SVO структуры
│   │   │   └── sparse.slang    # Sparse voxel представления
│   │   │
│   │   ├── rendering/          # Рендеринг
│   │   │   ├── gbuffer.slang   # G-buffer pass
│   │   │   ├── lighting.slang  # Deferred lighting
│   │   │   └── raymarch.slang  # Ray marching для SDF
│   │   │
│   │   └── compute/            # Compute шейдеры
│   │       ├── generation.slang # Генерация вокселей
│   │       ├── culling.slang   # GPU culling
│   │       └── lod.slang       # LOD selection
│   │
│   └── materials/              # Материалы
│       ├── interface.slang     # IMaterial интерфейс
│       ├── pbr.slang           # PBR материал
│       └── voxel_material.slang # Воксельный материал
│
└── build/shaders/              # Скомпилированные SPIR-V
    ├── core/
    ├── voxel/
    └── materials/
```

### Поток данных

```
Исходники .slang
       ↓
slangc (CMake add_custom_command)
       ↓
SPIR-V .spv файлы
       ↓
VkShaderModule (vkCreateShaderModule)
       ↓
VkPipeline (Graphics/Compute)
       ↓
Рендеринг вокселей
```

---

## Стратегия компиляции в ProjectV

### Build-time компиляция (по умолчанию)

```cmake
# cmake/slang_utils.cmake
function(compile_projectv_shader SOURCE ENTRY STAGE OUTPUT)
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND slangc
            ${SOURCE}
            -o ${OUTPUT}
            -target spirv
            -profile spirv_1_5
            -entry ${ENTRY}
            -stage ${STAGE}
            -O2
            -enable-slang-matrix-layout-row-major
            -I ${PROJECT_SOURCE_DIR}/shaders/core
            -I ${PROJECT_SOURCE_DIR}/shaders/voxel
        DEPENDS ${SOURCE}
        COMMENT "Slang: ${SOURCE} -> ${OUTPUT}"
        VERBATIM
    )
endfunction()
```

### Runtime компиляция (режим разработки)

```cpp
// src/renderer/slang_shader_manager.hpp
class SlangShaderManager
{
public:
    SlangShaderManager(VkDevice device);
    ~SlangShaderManager();

    // Загрузка precompiled SPIR-V
    VkShaderModule loadSPIRV(const std::string& path);

#ifdef PROJECTV_SHADER_HOT_RELOAD
    // Runtime компиляция для разработки
    VkShaderModule compile(
        const std::string& slangPath,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    void hotReload();
#endif

private:
    VkDevice device_;
    Slang::ComPtr<slang::IGlobalSession> globalSession_;
    Slang::ComPtr<slang::ISession> session_;
    std::unordered_map<std::string, VkShaderModule> cache_;
};
```

---

## Генерация шейдеров

### Воксельные типы данных

```slang
// shaders/core/types.slang
module Types;

// Базовый тип вокселя
struct SimpleVoxel
{
    float density;
    float3 color;
};

// Воксель с материалом
struct MaterialVoxel
{
    float density;
    float3 albedo;
    float roughness;
    float metallic;
    float emission;
    uint materialId;
};

// SDF воксель
struct SDFVoxel
{
    float distance;
    uint materialId;
};

// Заголовок чанка
struct ChunkHeader
{
    float3 worldOrigin;
    uint lodLevel;
    uint voxelCount;
    uint firstVoxelIndex;
};
```

### Generic шейдер для вокселей

```slang
// shaders/voxel/data/chunk.slang
module VoxelChunk;

import Types;

generic<TVoxel>
struct VoxelChunk
{
    static const uint SIZE = 32;

    [[vk::buffer_reference]]
    TVoxel* voxels;

    ChunkHeader header;

    TVoxel getVoxel(uint3 localPos)
    {
        uint index = localPos.z * SIZE * SIZE +
                    localPos.y * SIZE +
                    localPos.x;
        return voxels[index];
    }

    float3 localToWorld(uint3 localPos)
    {
        return header.worldOrigin + float3(localPos) * getVoxelSize(header.lodLevel);
    }
};

// Специализации
typedef VoxelChunk<SimpleVoxel> SimpleChunk;
typedef VoxelChunk<MaterialVoxel> MaterialChunk;
```

---

## Интеграция с ECS

### Передача ECS-данных в шейдер

```slang
// shaders/voxel/ecs_integration.slang
module ECSIntegration;

import Types;

// Данные, синхронизированные с ECS
struct ECSChunkData
{
    uint entityId;
    float3 worldPosition;
    uint lodLevel;
    uint isDirty;
    uint materialIndex;
};

[[vk::binding(0, 0)]]
StructuredBuffer<ECSChunkData> chunkEntities;

[[vk::push_constant]]
struct ECS PC
{
    float4x4 viewProj;
    float3 cameraPos;
    uint frameIndex;
} pc;
```

```cpp
// C++ сторона: синхронизация с flecs
void syncChunkEntities(flecs::world& world, VkBuffer buffer)
{
    auto query = world.each<ChunkComponent, TransformComponent>();

    std::vector<ECSChunkData> data;
    data.reserve(query.count());

    query.each([&](flecs::entity e, ChunkComponent& chunk, TransformComponent& transform) {
        ECSChunkData ecsData;
        ecsData.entityId = e.id();
        ecsData.worldPosition = transform.position;
        ecsData.lodLevel = chunk.lodLevel;
        ecsData.isDirty = chunk.needsRebuild ? 1 : 0;
        ecsData.materialIndex = chunk.materialIndex;
        data.push_back(ecsData);
    });

    // Копирование в GPU буфер
    vkMapMemory(device, bufferMemory, 0, data.size() * sizeof(ECSChunkData), 0, &mapped);
    memcpy(mapped, data.data(), data.size() * sizeof(ECSChunkData));
    vkUnmapMemory(device, bufferMemory);
}
```

---

## Профилирование с Tracy

```cpp
#include <tracy/Tracy.hpp>

void SlangShaderManager::compile(const std::string& path, ...)
{
    ZoneScopedN("Slang Compile");

    auto start = std::chrono::high_resolution_clock::now();

    // ... компиляция

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start
    );

    TracyPlot("Slang/CompileTime", duration.count());
    TracyPlot("Slang/ShaderSize", spirvCode->getBufferSize());
}
```

---

## Следующие разделы

- **13. Интеграция в проект** — детали CMake и SlangShaderManager
- **14. Паттерны ProjectV** — специфичные решения для вокселей
- **15. Примеры кода** — готовые примеры

---

## Slang в ProjectV: Интеграция

<!-- anchor: 13_projectv-integration -->

**🟡 Уровень 2: Средний** — Детали интеграции Slang в сборку ProjectV и реализация SlangShaderManager.

---

## CMake интеграция

### Поиск slangc

```cmake
# cmake/FindSlang.cmake
find_program(SLANGC_EXECUTABLE
    NAMES slangc
    HINTS
        $ENV{VULKAN_SDK}/bin
        ${CMAKE_SOURCE_DIR}/external/slang/build/bin
        ${CMAKE_SOURCE_DIR}/external/slang/build/Release/bin
)

find_path(SLANG_INCLUDE_DIR
    NAMES slang.h
    HINTS
        $ENV{VULKAN_SDK}/include
        ${CMAKE_SOURCE_DIR}/external/slang/include
)

find_library(SLANG_LIBRARY
    NAMES slang slangd
    HINTS
        $ENV{VULKAN_SDK}/lib
        ${CMAKE_SOURCE_DIR}/external/slang/build/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Slang
    REQUIRED_VARS SLANGC_EXECUTABLE SLANG_INCLUDE_DIR SLANG_LIBRARY
)

if(Slang_FOUND)
    add_library(Slang::slang UNKNOWN IMPORTED)
    set_target_properties(Slang::slang PROPERTIES
        IMPORTED_LOCATION "${SLANG_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
    )
endif()
```

### Модуль компиляции шейдеров

```cmake
# cmake/ProjectVShaders.cmake
include_guard()

# Параметры компиляции ProjectV
set(PROJECTV_SLANG_TARGET spirv)
set(PROJECTV_SLANG_PROFILE spirv_1_5)
set(PROJECTV_SLANG_OPTIMIZATION -O2)

# Пути к модулям
set(PROJECTV_SLANG_INCLUDES
    -I ${PROJECT_SOURCE_DIR}/shaders/core
    -I ${PROJECT_SOURCE_DIR}/shaders/voxel
    -I ${PROJECT_SOURCE_DIR}/shaders/materials
)

# Дополнительные флаги
set(PROJECTV_SLANG_FLAGS
    -enable-slang-matrix-layout-row-major
    -fvk-use-dx-position-w
)

# Функция компиляции одного шейдера
function(projectv_compile_shader SOURCE ENTRY STAGE OUTPUT)
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${SLANGC_EXECUTABLE}
            ${SOURCE}
            -o ${OUTPUT}
            -target ${PROJECTV_SLANG_TARGET}
            -profile ${PROJECTV_SLANG_PROFILE}
            -entry ${ENTRY}
            -stage ${STAGE}
            ${PROJECTV_SLANG_OPTIMIZATION}
            ${PROJECTV_SLANG_INCLUDES}
            ${PROJECTV_SLANG_FLAGS}
        DEPENDS ${SOURCE}
        COMMENT "ProjectV Shader: ${SOURCE} -> ${OUTPUT}"
        VERBATIM
    )
endfunction()

# Функция компиляции группы шейдеров
function(projectv_add_shader_target TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    set(ALL_OUTPUTS)

    foreach(SRC IN LISTS ARG_SOURCES)
        get_filename_component(NAME_WE ${SRC} NAME_WE)

        # Вершинный шейдер
        set(VERT_OUT "${CMAKE_BINARY_DIR}/shaders/${NAME_WE}_vert.spv")
        projectv_compile_shader(${SRC} vsMain vertex ${VERT_OUT})
        list(APPEND ALL_OUTPUTS ${VERT_OUT})

        # Фрагментный шейдер
        set(FRAG_OUT "${CMAKE_BINARY_DIR}/shaders/${NAME_WE}_frag.spv")
        projectv_compile_shader(${SRC} fsMain fragment ${FRAG_OUT})
        list(APPEND ALL_OUTPUTS ${FRAG_OUT})
    endforeach()

    add_custom_target(${TARGET_NAME} ALL DEPENDS ${ALL_OUTPUTS})
endfunction()
```

### Использование в CMakeLists.txt

```cmake
# CMakeLists.txt (фрагмент)
find_package(Slang REQUIRED)
include(cmake/ProjectVShaders.cmake)

# Основные шейдеры
projectv_add_shader_target(projectv_shaders
    SOURCES
        shaders/voxel/rendering/gbuffer.slang
        shaders/voxel/rendering/lighting.slang
)

# Compute шейдеры
projectv_compile_shader(
    shaders/voxel/compute/culling.slang
    csMain
    compute
    ${CMAKE_BINARY_DIR}/shaders/culling.spv
)

add_custom_target(projectv_compute_shaders
    DEPENDS ${CMAKE_BINARY_DIR}/shaders/culling.spv
)

add_dependencies(ProjectV projectv_shaders projectv_compute_shaders)
```

---

## SlangShaderManager

### Заголовочный файл

```cpp
// src/renderer/slang_shader_manager.hpp
#pragma once

#include <vulkan/vulkan.h>
#include <slang.h>
#include <slang-com-ptr.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace projectv {

class SlangShaderManager
{
public:
    struct ShaderModule
    {
        VkShaderModule module = VK_NULL_HANDLE;
        std::string entryPoint;
        VkShaderStageFlagBits stage;
    };

    explicit SlangShaderManager(VkDevice device);
    ~SlangShaderManager();

    // Запрет копирования
    SlangShaderManager(const SlangShaderManager&) = delete;
    SlangShaderManager& operator=(const SlangShaderManager&) = delete;

    // Инициализация Slang API
    bool initialize(const std::vector<std::string>& searchPaths);

    // Загрузка precompiled SPIR-V
    ShaderModule loadSPIRV(
        const std::string& path,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    // Загрузка с кэшированием
    ShaderModule loadCached(
        const std::string& name,
        const std::string& path,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

#ifdef PROJECTV_SHADER_HOT_RELOAD
    // Runtime компиляция
    ShaderModule compile(
        const std::string& slangSource,
        const std::string& moduleName,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    // Горячая перезагрузка
    void hotReload(const std::string& name);

    // Проверка изменений
    void checkForChanges();
#endif

    // Очистка кэша
    void clearCache();

    // Уничтожение модулей
    void destroyModule(const std::string& name);

private:
    VkDevice device_;

    // Slang API
    Slang::ComPtr<slang::IGlobalSession> globalSession_;
    Slang::ComPtr<slang::ISession> session_;

    // Кэш модулей
    std::unordered_map<std::string, ShaderModule> moduleCache_;

    // Чтение SPIR-V файла
    std::vector<uint32_t> readSPIRVFile(const std::string& path);

    // Создание VkShaderModule
    VkShaderModule createShaderModule(const std::vector<uint32_t>& code);
};

} // namespace projectv
```

### Реализация

```cpp
// src/renderer/slang_shader_manager.cpp
#include "slang_shader_manager.hpp"
#include <fstream>
#include <iostream>

namespace projectv {

SlangShaderManager::SlangShaderManager(VkDevice device)
    : device_(device)
{
}

SlangShaderManager::~SlangShaderManager()
{
    clearCache();
}

bool SlangShaderManager::initialize(const std::vector<std::string>& searchPaths)
{
    // Создание глобальной сессии
    SlangResult result = slang::createGlobalSession(globalSession_.writeRef());
    if (SLANG_FAILED(result))
    {
        std::cerr << "Failed to create Slang global session\n";
        return false;
    }

    // Настройка цели компиляции
    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession_->findProfile("spirv_1_5");
    targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    // Настройка сессии
    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    // Пути поиска
    std::vector<const char*> pathPtrs;
    for (const auto& path : searchPaths)
    {
        pathPtrs.push_back(path.c_str());
    }
    sessionDesc.searchPaths = pathPtrs.data();
    sessionDesc.searchPathCount = static_cast<int>(pathPtrs.size());

    // Создание сессии
    result = globalSession_->createSession(sessionDesc, session_.writeRef());
    if (SLANG_FAILED(result))
    {
        std::cerr << "Failed to create Slang session\n";
        return false;
    }

    return true;
}

ShaderModule SlangShaderManager::loadSPIRV(
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ShaderModule shaderModule;
    shaderModule.entryPoint = entryPoint;
    shaderModule.stage = stage;

    auto spirvCode = readSPIRVFile(path);
    if (spirvCode.empty())
    {
        std::cerr << "Failed to read SPIR-V: " << path << "\n";
        return shaderModule;
    }

    shaderModule.module = createShaderModule(spirvCode);
    return shaderModule;
}

ShaderModule SlangShaderManager::loadCached(
    const std::string& name,
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    auto it = moduleCache_.find(name);
    if (it != moduleCache_.end())
    {
        return it->second;
    }

    auto module = loadSPIRV(path, entryPoint, stage);
    moduleCache_[name] = module;
    return module;
}

void SlangShaderManager::clearCache()
{
    for (auto& [name, module] : moduleCache_)
    {
        if (module.module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device_, module.module, nullptr);
        }
    }
    moduleCache_.clear();
}

void SlangShaderManager::destroyModule(const std::string& name)
{
    auto it = moduleCache_.find(name);
    if (it != moduleCache_.end())
    {
        if (it->second.module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device_, it->second.module, nullptr);
        }
        moduleCache_.erase(it);
    }
}

std::vector<uint32_t> SlangShaderManager::readSPIRVFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return {};
    }

    size_t size = file.tellg();
    if (size % sizeof(uint32_t) != 0)
    {
        return {};
    }

    file.seekg(0);

    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), size);

    return code;
}

VkShaderModule SlangShaderManager::createShaderModule(const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(device_, &createInfo, nullptr, &module);

    if (result != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    return module;
}

#ifdef PROJECTV_SHADER_HOT_RELOAD
ShaderModule SlangShaderManager::compile(
    const std::string& slangSource,
    const std::string& moduleName,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ShaderModule shaderModule;
    shaderModule.entryPoint = entryPoint;
    shaderModule.stage = stage;

    // Загрузка модуля
    Slang::ComPtr<slang::IBlob> diagnostics;
    auto module = session_->loadModuleFromSourceString(
        moduleName.c_str(),
        moduleName.c_str(),
        slangSource.c_str(),
        diagnostics.writeRef()
    );

    if (!module)
    {
        const char* diag = diagnostics
            ? static_cast<const char*>(diagnostics->getBufferPointer())
            : "Unknown error";
        std::cerr << "Slang compile error: " << diag << "\n";
        return shaderModule;
    }

    // Получение entry point
    Slang::ComPtr<slang::IEntryPoint> entry;
    module->findEntryPointByName(entryPoint.c_str(), entry.writeRef());

    if (!entry)
    {
        std::cerr << "Entry point not found: " << entryPoint << "\n";
        return shaderModule;
    }

    // Линковка
    slang::IComponentType* components[] = { module, entry };
    Slang::ComPtr<slang::IComponentType> program;
    session_->createCompositeComponentType(
        components, 2,
        program.writeRef(),
        diagnostics.writeRef()
    );

    Slang::ComPtr<slang::IComponentType> linked;
    program->link(linked.writeRef(), diagnostics.writeRef());

    // Получение SPIR-V
    Slang::ComPtr<slang::IBlob> spirvCode;
    linked->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

    if (!spirvCode)
    {
        std::cerr << "Failed to generate SPIR-V\n";
        return shaderModule;
    }

    // Создание VkShaderModule
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode->getBufferSize();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(spirvCode->getBufferPointer());

    vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule.module);

    return shaderModule;
}
#endif

} // namespace projectv
```

---

## Использование в рендерере

```cpp
// src/renderer/vulkan_renderer.cpp
#include "slang_shader_manager.hpp"

class VulkanRenderer
{
public:
    void initialize(VkDevice device)
    {
        shaderManager_ = std::make_unique<SlangShaderManager>(device);

        shaderManager_->initialize({
            "shaders/core",
            "shaders/voxel",
            "shaders/materials"
        });

        loadShaders();
    }

    void loadShaders()
    {
        // G-buffer шейдеры
        auto vertShader = shaderManager_->loadCached(
            "gbuffer_vert",
            "build/shaders/gbuffer_vert.spv",
            "vsMain",
            VK_SHADER_STAGE_VERTEX_BIT
        );

        auto fragShader = shaderManager_->loadCached(
            "gbuffer_frag",
            "build/shaders/gbuffer_frag.spv",
            "fsMain",
            VK_SHADER_STAGE_FRAGMENT_BIT
        );

        // Создание pipeline
        createGBufferPipeline(vertShader, fragShader);
    }

private:
    std::unique_ptr<SlangShaderManager> shaderManager_;
};
```

---

## Следующий раздел

- **14. Паттерны ProjectV** — специфичные решения для воксельного рендеринга

---

## Slang в ProjectV: Паттерны

<!-- anchor: 14_projectv-patterns -->

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

---

## Slang в ProjectV: Продвинутые примеры

<!-- anchor: 15_projectv-examples -->

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
