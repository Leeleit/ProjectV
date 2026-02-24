## Интеграция

<!-- anchor: 02_integration -->


Подключение Draco к проекту через CMake.

## CMake интеграция

### Через add_subdirectory

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.12)
project(MyProject)

# Добавление Draco как подмодуля
add_subdirectory(external/draco)

# Ваша цель
add_executable(my_app src/main.cpp)

# Линковка
target_link_libraries(my_app PRIVATE draco::draco)

# Include directories добавляются автоматически
```

### Через find_package (установленный Draco)

```cmake
# После установки Draco в систему
find_package(draco CONFIG REQUIRED)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE draco::draco)
```

### FetchContent (CMake 3.14+)

```cmake
include(FetchContent)

FetchContent_Declare(
    draco
    GIT_REPOSITORY https://github.com/google/draco.git
    GIT_TAG 1.5.7
)

FetchContent_MakeAvailable(draco)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE draco::draco)
```

## CMake опции

| Опция                           | Default | Описание                 |
|---------------------------------|---------|--------------------------|
| `BUILD_SHARED_LIBS`             | OFF     | Сборка shared library    |
| `DRACO_TRANSCODER_SUPPORTED`    | OFF     | Включить glTF transcoder |
| `DRACO_ANIMATION_ENCODING`      | OFF     | Поддержка анимаций       |
| `DRACO_POINT_CLOUD_COMPRESSION` | ON      | Сжатие point cloud       |
| `DRACO_MESH_COMPRESSION`        | ON      | Сжатие mesh              |
| `DRACO_BUILD_EXECUTABLES`       | ON      | CLI инструменты          |
| `DRACO_TESTS`                   | OFF     | Сборка тестов            |

### Пример с опциями

```cmake
set(DRACO_TRANSCODER_SUPPORTED ON CACHE BOOL "" FORCE)
set(DRACO_ANIMATION_ENCODING ON CACHE BOOL "" FORCE)
set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(DRACO_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/draco)
```

## DRACO_TRANSCODER_SUPPORTED

Опция включает расширенные возможности:

- glTF загрузка/сохранение
- Materials и textures
- Animations
- Skins
- Scene graph
- EXT_mesh_features / EXT_structural_metadata

> **Примечание:** Требует C++17 и дополнительные зависимости (tinygltf).

### Зависимости для transcoder

```cmake
# При DRACO_TRANSCODER_SUPPORTED=ON
# Draco автоматически подтянет:
# - tinygltf (встроен)
# - libpng (опционально, для текстур)
# - libjpeg (опционально, для текстур)
```

## Минимальная конфигурация

Для декодирования только mesh:

```cmake
set(DRACO_TRANSCODER_SUPPORTED OFF CACHE BOOL "" FORCE)
set(DRACO_ANIMATION_ENCODING OFF CACHE BOOL "" FORCE)
set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(DRACO_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/draco)

# Линкуется только decoder functionality
target_link_libraries(my_app PRIVATE draco::draco)
```

## Структура библиотеки

При сборке создаётся одна библиотека `draco`:

```
libdraco.a / draco.lib
├── Core (decoder_buffer, encoder_buffer)
├── Attributes (geometry_attribute, point_attribute)
├── Compression
│   ├── decode, encode
│   ├── mesh (edgebreaker, sequential)
│   └── point_cloud (kd_tree, sequential)
├── Mesh (mesh, corner_table)
├── PointCloud (point_cloud)
└── IO (obj, ply, stl) - при DRACO_TRANSCODER_SUPPORTED
```

## Заголовочные файлы

Основные include пути:

```cpp
// Декодирование
#include <draco/compression/decode.h>

// Кодирование
#include <draco/compression/encode.h>
#include <draco/compression/expert_encode.h>

// Типы данных
#include <draco/mesh/mesh.h>
#include <draco/point_cloud/point_cloud.h>
#include <draco/attributes/geometry_attribute.h>
#include <draco/attributes/point_attribute.h>

// Буферы
#include <draco/core/decoder_buffer.h>
#include <draco/core/encoder_buffer.h>

// Опции
#include <draco/compression/config/encoder_options.h>
#include <draco/compression/config/decoder_options.h>
```

## CLI инструменты

При `DRACO_BUILD_EXECUTABLES=ON`:

| Инструмент         | Назначение                                    |
|--------------------|-----------------------------------------------|
| `draco_encoder`    | Кодирование OBJ/PLY/STL в .drc                |
| `draco_decoder`    | Декодирование .drc в OBJ/PLY                  |
| `draco_transcoder` | glTF transcoding (DRACO_TRANSCODER_SUPPORTED) |

### Примеры команд

```bash
# Кодирование
draco_encoder -i model.obj -o model.drc -qp 14

# С параметрами сжатия
draco_encoder -i model.ply -o model.drc -cl 8 -qp 12 -qt 10

# Декодирование
draco_decoder -i model.drc -o model.obj

# glTF transcoding
draco_transcoder -i scene.glb -o compressed.glb -qp 12
```

## Кросс-компиляция

### Android NDK

```cmake
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM android-24)

set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/draco)
```

### iOS

```cmake
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_ARCHITECTURES arm64)

set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/draco)
```

### Emscripten (WebAssembly)

```bash
emcmake cmake -B build \
    -DDRACO_BUILD_EXECUTABLES=OFF \
    -DDRACO_TESTS=OFF

emmake make -C build
```

## Sanitizers

```cmake
set(DRACO_SANITIZE address CACHE STRING "" FORCE)
# Доступные значения: address, memory, thread, undefined

add_subdirectory(external/draco)
```

## Сборка с SIMD оптимизациями

Draco автоматически определяет SIMD возможности:

```cmake
# Автоопределение при конфигурации
# Включает AVX/SSE на x86, NEON на ARM
# Явное управление не требуется
```

## Отключение ненужных компонентов

Для минимального бинарника только decoder:

```cmake
# Минимальный decoder-only build
set(DRACO_TRANSCODER_SUPPORTED OFF CACHE BOOL "" FORCE)
set(DRACO_MESH_COMPRESSION ON CACHE BOOL "" FORCE)
set(DRACO_POINT_CLOUD_COMPRESSION OFF CACHE BOOL "" FORCE)
set(DRACO_ANIMATION_ENCODING OFF CACHE BOOL "" FORCE)
set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(DRACO_TESTS OFF CACHE BOOL "" FORCE)
```

## Размер библиотеки

Примерные размеры (Release, x64):

| Конфигурация             | Размер  |
|--------------------------|---------|
| Decoder only             | ~800 KB |
| Full (encoder + decoder) | ~1.2 MB |
| With transcoder          | ~2.5 MB |

## Совместимость ABI

Draco гарантирует обратную совместимость decoder:

- Новый decoder читает старые bitstreams
- Старый decoder НЕ читает новые bitstreams

Версия bitstream в заголовке:

```cpp
// Mesh bitstream version: 2.2
// Point cloud bitstream version: 2.3

---

## glTF Transcoding

<!-- anchor: 07_gltf-transcoding -->


Интеграция Draco с glTF через EXT_mesh_draco и draco_transcoder.

## EXT_mesh_draco

Официальное расширение glTF 2.0 для сжатия геометрии Draco.

### Структура

```json
{
  "meshes": [{
    "primitives": [{
      "attributes": {
        "POSITION": 0,
        "NORMAL": 1
      },
      "indices": 2,
      "extensions": {
        "KHR_draco_mesh_compression": {
          "bufferView": 0,
          "attributes": {
            "POSITION": 0,
            "NORMAL": 1
          }
        }
      }
    }]
  }],
  "extensionsUsed": ["KHR_draco_mesh_compression"],
  "extensionsRequired": ["KHR_draco_mesh_compression"]
}
```

### Ключевые отличия

| Аспект     | Без Draco                      | С Draco                  |
|------------|--------------------------------|--------------------------|
| BufferView | Отдельные для каждого атрибута | Один для всего primitive |
| Размер     | Полный                         | Сжатый 10-20x            |
| Загрузка   | Прямой доступ                  | Требуется декодирование  |

## draco_transcoder

CLI инструмент для добавления Draco в glTF.

### Установка

```bash
# Сборка с DRACO_TRANSCODER_SUPPORTED
cmake -DDRACO_TRANSCODER_SUPPORTED=ON ..
make
```

### Базовое использование

```bash
# Добавление Draco сжатия
draco_transcoder -i input.glb -o output.glb

# С настройками квантования
draco_transcoder -i input.glb -o output.glb \
    -qp 14 \   # Position quantization
    -qn 10 \   # Normal quantization
    -qt 12     # Tex coord quantization
```

### Параметры командной строки

| Параметр | Описание                        | Default |
|----------|---------------------------------|---------|
| `-i`     | Входной файл                    | —       |
| `-o`     | Выходной файл                   | —       |
| `-qp`    | Position quantization bits      | 11      |
| `-qn`    | Normal quantization bits        | 8       |
| `-qt`    | Tex coord quantization bits     | 10      |
| `-qc`    | Color quantization bits         | 8       |
| `-qj`    | Joint indices quantization bits | 8       |
| `-qw`    | Joint weights quantization bits | 8       |
| `-cl`    | Compression level (0-10)        | 7       |

### Примеры

```bash
# Высокое качество
draco_transcoder -i model.glb -o model_hq.glb -qp 16 -qn 12 -cl 10

# Максимальное сжатие
draco_transcoder -i model.glb -o model_small.glb -qp 10 -qn 6 -cl 0

# Баланс для web
draco_transcoder -i model.glb -o model_web.glb -qp 12 -qn 8 -cl 5
```

## Программный transcoding

### Загрузка glTF с Draco

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/io/gltf_decoder.h>
#include <draco/io/scene_io.h>

draco::GltfDecoder decoder;
auto scene = decoder.DecodeFromFile("model.glb");
if (!scene.ok()) {
    std::cerr << "Error: " << scene.status().error_msg() << "\n";
    return;
}

// Доступ к сцене
for (int i = 0; i < scene.value()->NumMeshes(); ++i) {
    const draco::Mesh& mesh = scene.value()->GetMesh(i);
    // Обработка mesh
}
#endif
```

### Сохранение с Draco сжатием

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/io/gltf_encoder.h>
#include <draco/compression/draco_compression_options.h>

draco::GltfEncoder encoder;

// Настройка сжатия
draco::DracoCompressionOptions options;
options.quantization_bits_position = 14;
options.quantization_bits_normal = 10;
options.quantization_bits_tex_coord = 12;
options.compression_level = 7;

encoder.SetCompressionOptions(options);

// Сохранение
auto status = encoder.EncodeToFile(*scene, "output.glb");
#endif
```

## DracoCompressionOptions

```cpp
struct DracoCompressionOptions {
    // Quantization bits
    int quantization_bits_position = 11;
    int quantization_bits_normal = 8;
    int quantization_bits_tex_coord = 10;
    int quantization_bits_color = 8;
    int quantization_bits_generic = 8;
    int quantization_bits_joint_index = 8;
    int quantization_bits_joint_weight = 8;

    // Encoding
    int compression_level = 7;  // 0-10
    bool use_expert_encoder = false;

    // Method
    draco::MeshEncoderMethod mesh_encoding = draco::MESH_EDGEBREAKER_ENCODING;
};
```

## Обработка glTF primitives

### Структура после декодирования

```cpp
// glTF primitive содержит Draco-compressed данные
// После декодирования:

draco::Mesh mesh = decodeGltfPrimitive(primitive);

// Атрибуты доступны по glTF semantic
const auto* position = mesh.GetNamedAttribute(draco::GeometryAttribute::POSITION);
const auto* normal = mesh.GetNamedAttribute(draco::GeometryAttribute::NORMAL);
const auto* texcoord = mesh.GetNamedAttribute(draco::GeometryAttribute::TEX_COORD);
```

### Material information

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
// При DRACO_TRANSCODER_SUPPORTED mesh содержит materials
const draco::MaterialLibrary& materials = mesh.GetMaterialLibrary();

for (int i = 0; i < materials.NumMaterials(); ++i) {
    const draco::Material& mat = materials.GetMaterial(i);
    // PBR properties
    mat.GetColorFactor();
    mat.GetMetallicFactor();
    mat.GetRoughnessFactor();
}
#endif
```

## Animations

### Skinning data

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
// Joints и weights
const auto* joints = mesh.GetNamedAttribute(draco::GeometryAttribute::JOINTS);
const auto* weights = mesh.GetNamedAttribute(draco::GeometryAttribute::WEIGHTS);

// Skins
const draco::Scene& scene = *scene_result.value();
for (int i = 0; i < scene.NumSkins(); ++i) {
    const draco::Skin& skin = scene.GetSkin(i);
    // Inverse bind matrices
    // Joint nodes
}
#endif
```

### Animation data

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
for (int i = 0; i < scene.NumAnimations(); ++i) {
    const draco::Animation& anim = scene.GetAnimation(i);

    // Channels
    for (int c = 0; c < anim.NumChannels(); ++c) {
        const auto& channel = anim.GetChannel(c);
        // Target node, path (translation, rotation, scale)
    }

    // Samplers
    for (int s = 0; s < anim.NumSamplers(); ++s) {
        const auto& sampler = anim.GetSampler(s);
        // Input (time), output (values)
    }
}
#endif
```

## EXT_mesh_features

Draco поддерживает EXT_mesh_features для воксельных данных.

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
// Mesh features для воксельных идентификаторов
for (int i = 0; i < mesh.NumMeshFeatures(); ++i) {
    const draco::MeshFeatures& features = mesh.GetMeshFeatures(draco::MeshFeaturesIndex(i));

    // Feature IDs
    // Attribute association
    // Texture features
}
#endif
```

## EXT_structural_metadata

Структурные метаданные для воксельных сцен.

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
const draco::StructuralMetadata& metadata = scene.GetStructuralMetadata();

// Property tables
// Property attributes
// Schema definitions
#endif
```

## Интеграция с glTF loaders

### Порядок загрузки

```
1. Загрузка glTF JSON
2. Проверка extensionsUsed
3. Если KHR_draco_mesh_compression:
   a. Чтение bufferView с Draco данными
   b. Декодирование Draco
   c. Создание vertex/index buffers
4. Инициализация материалов и текстур
```

### Пример loader интеграции

```cpp
class GltfDracoLoader {
public:
    bool load(const std::string& path) {
        // 1. Парсинг glTF JSON
        if (!parseGltf(path)) return false;

        // 2. Проверка Draco
        bool hasDraco = checkExtension("KHR_draco_mesh_compression");

        // 3. Загрузка meshes
        for (auto& primitive : primitives_) {
            if (hasDraco && primitive.hasDracoExtension) {
                if (!decodeDracoPrimitive(primitive)) return false;
            } else {
                if (!loadStandardPrimitive(primitive)) return false;
            }
        }

        return true;
    }

private:
    bool decodeDracoPrimitive(Primitive& prim) {
        // Чтение Draco buffer
        auto data = readBufferView(prim.dracoBufferView);

        // Декодирование
        draco::DecoderBuffer buffer;
        buffer.Init(data.data(), data.size());

        draco::Decoder decoder;
        auto result = decoder.DecodeMeshFromBuffer(&buffer);

        if (!result.ok()) return false;

        prim.mesh = std::move(result).value();
        return true;
    }
};
```

## Best practices

### Квантование

| Тип контента | Position | Normal | UV |
|--------------|----------|--------|----|
| Characters   | 14-16    | 10-12  | 12 |
| Props        | 12-14    | 8-10   | 10 |
| Architecture | 11-12    | 8      | 10 |
| Terrain      | 10-11    | 6-8    | 8  |

### Compression level

| Use case  | Level | Trade-off                |
|-----------|-------|--------------------------|
| Download  | 0-3   | Лучшее сжатие, медленное |
| Streaming | 5-7   | Баланс                   |
| Runtime   | 8-10  | Быстрое, хуже сжатие     |

### Тестирование

```bash
# Проверка размера
ls -la original.glb compressed.glb

# Проверка качества (визуально)
# Загрузите в glTF viewer

# Benchmark декодирования
draco_decoder -i compressed.glb -o test.obj
time draco_decoder ...

---

## Интеграция Draco в ProjectV

<!-- anchor: 12_projectv-integration -->


Специфика интеграции Draco в воксельный движок ProjectV: работа с SVO, sparse data, интеграция с VMA и flecs.

## Роль Draco в ProjectV

Draco используется для:

1. **Сжатие воксельных чанков** — уменьшение размера при хранении и передаче
2. **Загрузка glTF моделей** — через KHR_draco_mesh_compression
3. **Сетевая синхронизация** — минимальный bandwidth для multiplayer

### Жизненный цикл данных

```

Воксельный чанк (SoA)
↓
VoxelMeshBuilder → draco::Mesh
↓
Draco Encoder
↓
Compressed Chunk (.drc)
↓
Network / Disk
↓
Draco Decoder
↓
draco::Mesh
↓
VMA / Vulkan Buffers
↓
ECS Components (flecs)
↓
Rendering System

```

---

## Воксельные чанки

### Структура данных чанка

```cpp
#include <draco/mesh/mesh.h>
#include <draco/attributes/geometry_attribute.h>

// SoA данные воксельного чанка
struct VoxelChunkData {
    std::vector<uint8_t> voxelTypes;      // Тип вокселя
    std::vector<uint8_t> occlusion;       // Ambient occlusion
    std::vector<uint16_t> blockLight;     // Уровень света
    // ... другие данные
};

// Конвертация в Draco Mesh
std::unique_ptr<draco::Mesh> voxelChunkToMesh(const VoxelChunkData& chunk) {
    auto mesh = std::make_unique<draco::Mesh>();

    // Для вокселей используем Point Cloud (без connectivity)
    // Или mesh с упрощённой геометрией для greedy meshing

    // Voxel type attribute
    draco::GeometryAttribute voxelTypeAttr;
    voxelTypeAttr.Init(
        draco::GeometryAttribute::GENERIC,
        nullptr, 1, draco::DT_UINT8, false, 1, 0
    );

    int voxelTypeAttrId = mesh->AddAttribute(voxelTypeAttr, true, chunk.voxelTypes.size());

    draco::PointAttribute* attr = mesh->attribute(voxelTypeAttrId);
    for (size_t i = 0; i < chunk.voxelTypes.size(); ++i) {
        attr->SetAttributeValue(draco::AttributeValueIndex(i), &chunk.voxelTypes[i]);
    }

    mesh->set_num_points(chunk.voxelTypes.size());

    return mesh;
}
```

### Квантование для вокселей

Воксели уже дискретны, но атрибуты могут требовать разной точности:

```cpp
draco::ExpertEncoder encoder(*mesh);

// Voxel type — 8 бит достаточно (256 типов)
encoder.SetQuantizationBitsForAttribute(voxelTypeAttrId, 8);

// Occlusion — 4 бита (16 уровней)
encoder.SetQuantizationBitsForAttribute(occlusionAttrId, 4);

// Light — 8 бит для точности
encoder.SetQuantizationBitsForAttribute(lightAttrId, 8);

// Скорость важна для real-time chunk loading
encoder.SetSpeedOptions(5, 10);
encoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);
```

---

## Интеграция с VMA

### Прямая запись в GPU buffer

```cpp
#include <draco/compression/decode.h>
#include <vk_mem_alloc.h>

struct VulkanMeshData {
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;
    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    uint32_t vertexCount;
    uint32_t indexCount;
};

class DracoVmaDecoder {
public:
    DracoVmaDecoder(VmaAllocator allocator) : allocator_(allocator) {}

    std::optional<VulkanMeshData> decodeToVulkan(
        const void* data, size_t size) {

        // Декодирование в CPU
        draco::DecoderBuffer buffer;
        buffer.Init(reinterpret_cast<const char*>(data), size);

        draco::Decoder decoder;
        auto result = decoder.DecodeMeshFromBuffer(&buffer);

        if (!result.ok()) return std::nullopt;

        auto mesh = std::move(result).value();

        VulkanMeshData vulkanData = {};

        // Создание vertex buffer
        if (!createVertexBuffer(*mesh, vulkanData)) return std::nullopt;

        // Создание index buffer
        if (!createIndexBuffer(*mesh, vulkanData)) return std::nullopt;

        return vulkanData;
    }

private:
    VmaAllocator allocator_;

    bool createVertexBuffer(const draco::Mesh& mesh, VulkanMeshData& out) {
        const auto* posAttr = mesh.GetNamedAttribute(draco::GeometryAttribute::POSITION);
        if (!posAttr) return false;

        const size_t vertexCount = mesh.num_points();
        const size_t vertexSize = sizeof(float) * 3;  // xyz
        const size_t bufferSize = vertexCount * vertexSize;

        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfo;
        VkResult result = vmaCreateBuffer(allocator_, &bufferInfo, &allocCreateInfo,
            &out.vertexBuffer, &out.vertexAllocation, &allocInfo);

        if (result != VK_SUCCESS) return false;

        // Прямая запись в mapped memory
        float* dst = static_cast<float*>(allocInfo.pMappedData);
        for (draco::PointIndex i(0); i < vertexCount; ++i) {
            std::array<float, 3> pos;
            posAttr->GetValue(posAttr->mapped_index(i), &pos);
            *dst++ = pos[0];
            *dst++ = pos[1];
            *dst++ = pos[2];
        }

        out.vertexCount = static_cast<uint32_t>(vertexCount);
        return true;
    }

    bool createIndexBuffer(const draco::Mesh& mesh, VulkanMeshData& out) {
        const size_t indexCount = mesh.num_faces() * 3;
        const size_t bufferSize = indexCount * sizeof(uint32_t);

        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfo;
        VkResult result = vmaCreateBuffer(allocator_, &bufferInfo, &allocCreateInfo,
            &out.indexBuffer, &out.indexAllocation, &allocInfo);

        if (result != VK_SUCCESS) return false;

        uint32_t* dst = static_cast<uint32_t*>(allocInfo.pMappedData);
        for (draco::FaceIndex f(0); f < mesh.num_faces(); ++f) {
            const auto& face = mesh.face(f);
            *dst++ = face[0].value();
            *dst++ = face[1].value();
            *dst++ = face[2].value();
        }

        out.indexCount = static_cast<uint32_t>(indexCount);
        return true;
    }
};
```

### Staging buffer для больших mesh

```cpp
// Для больших mesh используйте staging buffer
// См. docs/guides/cpp/02_memory-management.md для деталей RAII

class StagedMeshLoader {
public:
    void loadMeshWithStaging(VkDevice device, VkCommandPool cmdPool,
                             VkQueue queue, const draco::Mesh& mesh) {
        // 1. Создание staging buffer (HOST_VISIBLE)
        // 2. Копирование данных из Draco в staging
        // 3. Создание device buffer (DEVICE_LOCAL)
        // 4. Copy command buffer
        // 5. Submit и wait
    }
};
```

---

## Интеграция с ECS (flecs)

### Компоненты для геометрии

```cpp
#include <flecs.h>

// Компонент для рендеринга
struct DracoMeshComponent {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexAllocation = VK_NULL_HANDLE;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

// Компонент для сжатых данных
struct CompressedChunkComponent {
    std::vector<char> compressedData;
    uint32_t chunkX, chunkY, chunkZ;
    bool needsDecompression = true;
};

// Система загрузки чанков
void RegisterDracoSystems(flecs::world& world) {
    // Система декомпрессии (на worker thread)
    world.system<CompressedChunkComponent, DracoMeshComponent>("DracoDecompress")
        .kind(flecs::OnUpdate)
        .each([](flecs::entity e, CompressedChunkComponent& compressed,
                 DracoMeshComponent& mesh) {
            if (!compressed.needsDecompression) return;

            // Декомпрессия в фоновом потоке
            draco::DecoderBuffer buffer;
            buffer.Init(compressed.compressedData.data(),
                       compressed.compressedData.size());

            draco::Decoder decoder;
            auto result = decoder.DecodeMeshFromBuffer(&buffer);

            if (result.ok()) {
                // Создание Vulkan buffers
                // ...
                compressed.needsDecompression = false;
            }
        });
}
```

### Асинхронная загрузка

```cpp
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

class AsyncDracoLoader {
public:
    struct LoadRequest {
        std::vector<char> compressedData;
        std::function<void(std::unique_ptr<draco::Mesh>)> callback;
    };

    void start() {
        worker_ = std::thread([this]() { workerLoop(); });
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_all();
        worker_.join();
    }

    void submit(const std::vector<char>& data,
                std::function<void(std::unique_ptr<draco::Mesh>)> callback) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push({data, callback});
        }
        cv_.notify_one();
    }

private:
    void workerLoop() {
        while (running_) {
            LoadRequest request;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return !queue_.empty() || !running_; });

                if (!running_) break;

                request = std::move(queue_.front());
                queue_.pop();
            }

            draco::DecoderBuffer buffer;
            buffer.Init(request.compressedData.data(),
                       request.compressedData.size());

            draco::Decoder decoder;
            auto result = decoder.DecodeMeshFromBuffer(&buffer);

            if (result.ok()) {
                request.callback(std::move(result).value());
            } else {
                request.callback(nullptr);
            }
        }
    }

    std::thread worker_;
    std::queue<LoadRequest> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = true;
};
```

---

## SVO и Sparse Data

### Адаптация Draco для SVO

```cpp
// SVO node data
struct SVONodeData {
    uint32_t childMask;      // 8-bit child existence mask
    uint32_t firstChild;     // Index of first child
    uint8_t voxelType;       // Leaf voxel type
};

// Encoding SVO as Draco point cloud
std::unique_ptr<draco::PointCloud> svoToPointCloud(const std::vector<SVONodeData>& nodes) {
    auto pc = std::make_unique<draco::PointCloud>();
    pc->set_num_points(nodes.size());

    // Child mask attribute
    draco::GeometryAttribute childMaskAttr;
    childMaskAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 1,
                       draco::DT_UINT32, false, 4, 0);
    int childMaskId = pc->AddAttribute(childMaskAttr, true, nodes.size());

    // First child attribute
    draco::GeometryAttribute firstChildAttr;
    firstChildAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 1,
                        draco::DT_UINT32, false, 4, 0);
    int firstChildId = pc->AddAttribute(firstChildAttr, true, nodes.size());

    // Voxel type attribute
    draco::GeometryAttribute voxelTypeAttr;
    voxelTypeAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 1,
                       draco::DT_UINT8, false, 1, 0);
    int voxelTypeId = pc->AddAttribute(voxelTypeAttr, true, nodes.size());

    // Fill attributes
    for (size_t i = 0; i < nodes.size(); ++i) {
        pc->attribute(childMaskId)->SetAttributeValue(
            draco::AttributeValueIndex(i), &nodes[i].childMask);
        pc->attribute(firstChildId)->SetAttributeValue(
            draco::AttributeValueIndex(i), &nodes[i].firstChild);
        pc->attribute(voxelTypeId)->SetAttributeValue(
            draco::AttributeValueIndex(i), &nodes[i].voxelType);
    }

    return pc;
}
```

### Metadata для SVO структуры

```cpp
void addSVOMetadata(draco::PointCloud& pc, uint32_t depth, uint32_t leafCount) {
    auto metadata = std::make_unique<draco::GeometryMetadata>();
    metadata->AddEntryString("structure", "svo");
    metadata->AddEntryInt("depth", depth);
    metadata->AddEntryInt("leaf_count", leafCount);
    metadata->AddEntryString("version", "1.0");

    pc.AddMetadata(std::move(metadata));
}
```

---

## Профилирование с Tracy

```cpp
#include <tracy/Tracy.hpp>

class ProfiledDracoDecoder {
public:
    std::unique_ptr<draco::Mesh> decode(const void* data, size_t size) {
        ZoneScopedN("DracoDecode");

        draco::DecoderBuffer buffer;
        buffer.Init(reinterpret_cast<const char*>(data), size);

        draco::Decoder decoder;

        auto start = std::chrono::high_resolution_clock::now();
        auto result = decoder.DecodeMeshFromBuffer(&buffer);
        auto end = std::chrono::high_resolution_clock::now();

        if (result.ok()) {
            auto mesh = std::move(result).value();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            TracyPlot("DracoDecodeTime", duration.count());
            TracyPlot("DracoVertexCount", static_cast<int64_t>(mesh->num_points()));
            TracyPlot("DracoFaceCount", static_cast<int64_t>(mesh->num_faces()));

            return mesh;
        }

        return nullptr;
    }
};
```

---

## Сетевая оптимизация

### Delta compression для чанков

```cpp
// Сжатие diff между версиями чанка
std::vector<char> compressChunkDiff(
    const VoxelChunkData& oldChunk,
    const VoxelChunkData& newChunk) {

    // 1. Вычисление diff
    std::vector<uint8_t> diff;
    for (size_t i = 0; i < newChunk.voxelTypes.size(); ++i) {
        if (oldChunk.voxelTypes[i] != newChunk.voxelTypes[i]) {
            // Store index + new value
        }
    }

    // 2. Сжатие diff через Draco
    // ...
}
```

### Приоритет загрузки

```cpp
// Загрузка чанков по расстоянию до игрока
void prioritizeChunkLoading(
    const std::vector<CompressedChunkComponent>& chunks,
    const glm::vec3& playerPos) {

    // Sort chunks by distance to player
    // Load nearest chunks first with higher decode priority
}
```

---

## Best Practices для ProjectV

### Настройки по умолчанию для вокселей

```cpp
draco::Encoder createVoxelChunkEncoder() {
    draco::Encoder encoder;

    // Воксели уже дискретны, достаточно 8 бит
    encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, 8);

    // Быстрое декодирование важно для chunk loading
    encoder.SetSpeedOptions(5, 10);

    // Sequential для point cloud данных
    encoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);

    return encoder;
}
```

### Настройки для glTF моделей

```cpp
draco::Encoder createGltfEncoder() {
    draco::Encoder encoder;

    // Высокое качество для визуальных моделей
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 10);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 12);

    // Хороший compression, разумный decode
    encoder.SetSpeedOptions(3, 5);

    // Edgebreaker для meshes
    encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);

    return encoder;
}
```

### Память и кэширование

```cpp
// Кэширование декодированных mesh
class MeshCache {
public:
    std::shared_ptr<draco::Mesh> getOrDecode(const std::string& key,
                                             const std::vector<char>& compressed) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }

        // Decode and cache
        draco::DecoderBuffer buffer;
        buffer.Init(compressed.data(), compressed.size());

        draco::Decoder decoder;
        auto result = decoder.DecodeMeshFromBuffer(&buffer);

        if (result.ok()) {
            auto mesh = std::make_shared<draco::Mesh>(std::move(result).value());
            cache_[key] = mesh;
            return mesh;
        }

        return nullptr;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<draco::Mesh>> cache_;
    std::mutex mutex_;
};
