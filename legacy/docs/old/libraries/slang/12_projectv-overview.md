# Slang в ProjectV: Обзор интеграции

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
