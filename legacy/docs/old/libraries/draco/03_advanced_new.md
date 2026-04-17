## Производительность

<!-- anchor: 08_performance -->

🟡 **Уровень 2: Средний**

Оптимизация compression ratio и decode speed.

## Метрики

### Compression Ratio

```
Compression Ratio = Original Size / Compressed Size
```

| Модель               | Original | Draco  | Ratio |
|----------------------|----------|--------|-------|
| Character (50K tris) | 2.8 MB   | 180 KB | 15.5x |
| Building (200K tris) | 12 MB    | 600 KB | 20x   |
| Terrain (1M tris)    | 58 MB    | 2.5 MB | 23x   |

### Decode Time

Время декодирования на CPU (ms):

| Mesh size | Edgebreaker | Sequential |
|-----------|-------------|------------|
| 10K tris  | 5-10        | 2-5        |
| 100K tris | 50-100      | 20-40      |
| 1M tris   | 500-1000    | 200-400    |

### Encode Time

Время кодирования на CPU (ms):

| Mesh size | Edgebreaker (cl=0) | Edgebreaker (cl=10) |
|-----------|--------------------|---------------------|
| 10K tris  | 50-100             | 10-20               |
| 100K tris | 500-1000           | 100-200             |
| 1M tris   | 5000-10000         | 1000-2000           |

## Факторы влияния

### Quantization

Больше бит = больше размер файла:

| Position bits | Размер       | Точность |
|---------------|--------------|----------|
| 8             | Минимальный  | ~0.4%    |
| 11            | Базовый      | ~0.05%   |
| 14            | Большой      | ~0.006%  |
| 16            | Максимальный | ~0.0015% |

```cpp
// Компромисс: качество vs размер
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);  // Баланс
```

### Compression Level

| Level | Размер       | Encode   | Decode |
|-------|--------------|----------|--------|
| 0     | Минимальный  | Медленно | Средне |
| 5     | Средний      | Средне   | Быстро |
| 10    | Максимальный | Быстро   | Быстро |

```cpp
// Для offline preprocessing
encoder.SetSpeedOptions(0, 0);

// Для runtime encoding
encoder.SetSpeedOptions(10, 10);

// Для быстрого декодирования
encoder.SetSpeedOptions(5, 10);
```

### Encoding Method

| Method      | Compression | Decode Speed |
|-------------|-------------|--------------|
| Edgebreaker | Лучший      | Средняя      |
| Sequential  | Хороший     | Быстрая      |

### Prediction Schemes

Влияние на compression ratio:

| Scheme              | Improvement |
|---------------------|-------------|
| NONE                | Baseline    |
| DIFFERENCE          | +10-20%     |
| PARALLELOGRAM       | +20-35%     |
| MULTI_PARALLELOGRAM | +25-40%     |

## Профилирование

### Измерение размера

```cpp
#include <draco/compression/encode.h>

draco::Encoder encoder;
encoder.SetSpeedOptions(0, 0);

draco::EncoderBuffer buffer;
encoder.EncodeMeshToBuffer(mesh, &buffer);

std::cout << "Original vertices: " << mesh.num_points() << "\n";
std::cout << "Compressed size: " << buffer.size() << " bytes\n";
std::cout << "Bytes per vertex: "
          << static_cast<float>(buffer.size()) / mesh.num_points() << "\n";
```

### Измерение времени

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

draco::Decoder decoder;
auto result = decoder.DecodeMeshFromBuffer(&buffer);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

std::cout << "Decode time: " << duration.count() << " ms\n";
```

### С Tracy

```cpp
#include <tracy/Tracy.hpp>

void decodeWithProfiling(const void* data, size_t size) {
    ZoneScopedN("DracoDecode");

    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data), size);

    draco::Decoder decoder;

    {
        ZoneScopedN("DecodeMesh");
        auto result = decoder.DecodeMeshFromBuffer(&buffer);
    }

    TracyPlot("MeshSize", static_cast<int64_t>(buffer.decoded_size()));
    FrameMark;
}
```

## Оптимизация decode speed

### Выбор метода

```cpp
// Быстрое декодирование
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);  // Быстрее
encoder.SetSpeedOptions(5, 10);  // Оптимизация под decode
```

### Пропуск деквантования

```cpp
// Если квантование приемлемо для вашего use case
draco::Decoder decoder;
decoder.SetSkipAttributeTransform(draco::GeometryAttribute::POSITION);
// Деквантование будет пропущено, decode быстрее
```

### Предварительное выделение памяти

```cpp
// Выделение памяти заранее для больших mesh
draco::EncoderBuffer buffer;
buffer.Reserve(estimated_size);  // Уменьшает реаллокации
```

## Оптимизация compression ratio

### Максимальное сжатие

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetSpeedOptions(0, 0);

// Минимальное квантование для вашего use case
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 10);
encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 6);
encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 8);
```

### Deduplication перед кодированием

```cpp
// Устранение дубликатов улучшает сжатие
mesh.DeduplicatePointIds();
mesh.DeduplicateAttributeValues();

draco::Encoder encoder;
// ...
```

### Удаление неиспользуемых атрибутов

```cpp
// Удалите ненужные атрибуты перед кодированием
for (int i = mesh.num_attributes() - 1; i >= 0; --i) {
    const auto* attr = mesh.attribute(i);
    if (!isAttributeNeeded(attr->attribute_type())) {
        mesh.DeleteAttribute(i);
    }
}
```

## Memory usage

### Decoder

```cpp
// Пиковое использование памяти при декодировании
// ~2-3x от размера сжатых данных

// Для контроля памяти используйте:
// - Stream processing для больших файлов
// - Chunked loading
```

### Encoder

```cpp
// Пиковое использование памяти при кодировании
// ~3-5x от размера исходных данных

// Edgebreaker требует больше памяти чем Sequential
```

## Benchmarks

### Тестовые модели

| Модель | Vertices | Triangles | Original | Compressed | Ratio |
|--------|----------|-----------|----------|------------|-------|
| Cube   | 8        | 12        | 1 KB     | 200 B      | 5x    |
| Sphere | 482      | 960       | 20 KB    | 3 KB       | 6.7x  |
| Bunny  | 35947    | 69451     | 4 MB     | 280 KB     | 14x   |
| Dragon | 50161    | 100000    | 5 MB     | 350 KB     | 14x   |
| Buddha | 543652   | 1087716   | 55 MB    | 2.8 MB     | 19x   |

### Decode speed по платформам

| Platform      | 100K tris  | 1M tris      |
|---------------|------------|--------------|
| x86-64 (AVX2) | 20-40 ms   | 200-400 ms   |
| x86-64 (SSE)  | 30-60 ms   | 300-600 ms   |
| ARM64 (NEON)  | 25-50 ms   | 250-500 ms   |
| ARM32         | 50-100 ms  | 500-1000 ms  |
| WebAssembly   | 100-200 ms | 1000-2000 ms |

## Практические рекомендации

### Для web

```cpp
// Оптимизация под web loading
encoder.SetSpeedOptions(0, 5);  // Хороший compression, разумный decode
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
```

### Для mobile

```cpp
// Оптимизация под мобильные устройства
encoder.SetSpeedOptions(5, 10);  // Быстрый decode
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
```

### Для desktop

```cpp
// Баланс для desktop
encoder.SetSpeedOptions(3, 5);
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);
```

### Для streaming

```cpp
// Реальное время, минимальная задержка
encoder.SetSpeedOptions(10, 10);
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
```

## Trade-offs summary

| Use case  | Method            | Speed    | Quantization          |
|-----------|-------------------|----------|-----------------------|
| Archive   | Edgebreaker, cl=0 | (0, 0)   | Минимальная           |
| Download  | Edgebreaker, cl=5 | (0, 5)   | Средняя               |
| Streaming | Sequential, cl=10 | (10, 10) | Зависит от требований |
| Real-time | Sequential, cl=10 | (10, 10) | Высокая               |

---

## Продвинутые возможности

<!-- anchor: 09_advanced -->

🔴 **Уровень 3: Продвинутый**

Metadata, animations и transcoder features.

## Metadata

Draco поддерживает хранение пользовательских данных в сжатом файле.

### Geometry Metadata

```cpp
#include <draco/metadata/geometry_metadata.h>

// Создание mesh
draco::Mesh mesh;
// ... заполнение mesh ...

// Добавление metadata уровня геометрии
auto metadata = std::make_unique<draco::GeometryMetadata>();
metadata->AddEntryString("name", "VoxelChunk_001");
metadata->AddEntryString("author", "ProjectV");
metadata->AddEntryInt("version", 1);
metadata->AddEntryDouble("scale", 1.0);

mesh.AddMetadata(std::move(metadata));
```

### Attribute Metadata

```cpp
// Metadata для конкретного атрибута
int posAttrId = 0;  // ID атрибута позиций

auto attrMetadata = std::make_unique<draco::AttributeMetadata>(posAttrId);
attrMetadata->AddEntryString("semantic", "vertex_position");
attrMetadata->AddEntryString("coordinate_system", "Y-up");
attrMetadata->AddEntryInt("precision_bits", 14);

mesh.AddAttributeMetadata(posAttrId, std::move(attrMetadata));
```

### Чтение Metadata

```cpp
// Чтение metadata уровня геометрии
const draco::GeometryMetadata* meta = mesh.GetMetadata();
if (meta) {
    std::string name;
    if (meta->GetEntryString("name", &name)) {
        std::cout << "Mesh name: " << name << "\n";
    }

    int version;
    if (meta->GetEntryInt("version", &version)) {
        std::cout << "Version: " << version << "\n";
    }
}

// Чтение metadata атрибута
const draco::AttributeMetadata* attrMeta =
    mesh.GetAttributeMetadataByAttributeId(0);
if (attrMeta) {
    std::string semantic;
    if (attrMeta->GetEntryString("semantic", &semantic)) {
        std::cout << "Attribute semantic: " << semantic << "\n";
    }
}
```

### Поиск по metadata

```cpp
// Поиск атрибута по записи в metadata
int attrId = mesh.GetAttributeIdByMetadataEntry("semantic", "vertex_position");
if (attrId >= 0) {
    const draco::PointAttribute* attr = mesh.attribute(attrId);
    // Работаем с атрибутом
}
```

### Типы данных metadata

```cpp
// Доступные типы
metadata->AddEntryString("key_string", "value");
metadata->AddEntryInt("key_int", 42);
metadata->AddEntryDouble("key_double", 3.14159);

// Массивы
std::vector<int32_t> intArray = {1, 2, 3, 4, 5};
metadata->AddEntryIntArray("key_array", intArray);

// Чтение массива
std::vector<int32_t> outArray;
if (metadata->GetEntryIntArray("key_array", &outArray)) {
    // Работаем с массивом
}
```

## Animations

Поддержка анимаций требует `DRACO_TRANSCODER_SUPPORTED`.

### Keyframe Animations

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/animation/keyframe_animation.h>

draco::KeyframeAnimation animation;

// Настройка количества кадров
animation.set_num_frames(100);

// Добавление данных анимации как атрибутов
draco::GeometryAttribute timeAttr;
timeAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 1,
              draco::DT_FLOAT32, false, 4, 0);
animation.AddAttribute(timeAttr, true, 100);

// Данные для каждого кадра
draco::GeometryAttribute transformAttr;
transformAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 16,
                   draco::DT_FLOAT32, false, 64, 0);
animation.AddAttribute(transformAttr, true, 100);
#endif
```

### Animation Encoding

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/animation/keyframe_animation_encoder.h>

draco::KeyframeAnimationEncoder encoder;

draco::EncoderBuffer buffer;
auto status = encoder.EncodeKeyframeAnimation(animation, &buffer);

if (status.ok()) {
    // Сохранение animation data
}
#endif
```

### Animation Decoding

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/animation/keyframe_animation_decoder.h>

draco::DecoderBuffer buffer;
buffer.Init(data, size);

draco::KeyframeAnimationDecoder decoder;
draco::KeyframeAnimation animation;

auto status = decoder.DecodeBufferToAnimation(&buffer, &animation);
if (status.ok()) {
    // Доступ к данным анимации
    int numFrames = animation.num_frames();
    int numAttributes = animation.num_attributes();
}
#endif
```

## Skins

Скиннинг для деформируемых mesh.

### Skinning Data

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
// Joints attribute (bone indices)
draco::GeometryAttribute jointsAttr;
jointsAttr.Init(draco::GeometryAttribute::JOINTS, nullptr, 4,
                draco::DT_UINT16, false, 8, 0);
mesh.AddAttribute(jointsAttr, true, numVertices);

// Weights attribute (bone weights)
draco::GeometryAttribute weightsAttr;
weightsAttr.Init(draco::GeometryAttribute::WEIGHTS, nullptr, 4,
                 draco::DT_FLOAT32, false, 16, 0);
mesh.AddAttribute(weightsAttr, true, numVertices);

// Skin data
draco::Skin skin;
skin.SetName("CharacterSkin");

// Inverse bind matrices
std::vector<glm::mat4> inverseBindMatrices;
for (int i = 0; i < numJoints; ++i) {
    inverseBindMatrices.push_back(computeInverseBindMatrix(joints[i]));
}
skin.SetInverseBindMatrices(inverseBindMatrices);

// Joint nodes
for (int i = 0; i < numJoints; ++i) {
    skin.AddJoint(jointNodeIndices[i]);
}
#endif
```

## Materials (DRACO_TRANSCODER_SUPPORTED)

### Material Library

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/material/material_library.h>

draco::MaterialLibrary& matLib = mesh.GetMaterialLibrary();

// Создание материала
auto material = std::make_unique<draco::Material>();
material->SetName("VoxelMaterial");
material->SetColorFactor({1.0f, 0.5f, 0.2f, 1.0f});
material->SetMetallicFactor(0.8f);
material->SetRoughnessFactor(0.3f);
material->SetEmissiveFactor({0.1f, 0.05f, 0.0f});

matLib.AddMaterial(std::move(material));
#endif
```

### Texture Mapping

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/texture/texture_map.h>
#include <draco/texture/texture.h>

// Создание текстуры
auto texture = std::make_unique<draco::Texture>();
texture->SetSourceImage(loadImage("diffuse.png"));

// Texture map
auto textureMap = std::make_unique<draco::TextureMap>();
textureMap->SetTexture(std::move(texture));
textureMap->SetTexCoordIndex(0);
textureMap->SetWrappingMode(draco::TextureMap::WRAP, draco::TextureMap::WRAP);

// Привязка к материалу
material->SetTextureMap(draco::Material::DIFFUSE, std::move(textureMap));
#endif
```

## Mesh Features

EXT_mesh_features для воксельных данных.

### Feature IDs

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/mesh/mesh_features.h>

// Создание mesh features
auto meshFeatures = std::make_unique<draco::MeshFeatures>();
meshFeatures->SetFeatureCount(100);  // 100 unique voxel types

// Привязка к атрибуту
meshFeatures->SetAttributeIndex(3);  // GENERIC атрибут с ID вокселей

mesh.AddMeshFeatures(std::move(meshFeatures));
#endif
```

### Feature IDs из текстуры

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
// Texture-based feature IDs
auto meshFeatures = std::make_unique<draco::MeshFeatures>();

// Texture с feature ID
auto featureTexture = std::make_unique<draco::Texture>();
featureTexture->SetSourceImage(loadImage("voxel_ids.png"));

draco::MeshFeatureTextureSet textureSet;
textureSet.texture = featureTexture.get();
textureSet.texCoordIndex = 1;

meshFeatures->SetFeatureTextureSet(&textureSet);
meshFeatures->SetFeatureCount(256);  // 256 voxel types
#endif
```

## Structural Metadata

EXT_structural_metadata для структурированных данных.

### Property Tables

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/metadata/structural_metadata.h>

draco::StructuralMetadata structuralMeta;

// Определение схемы
draco::PropertyTableSchema schema;
schema.name = "VoxelProperties";

// Свойство: тип вокселя
draco::PropertyTableProperty voxelTypeProp;
voxelTypeProp.name = "voxel_type";
voxelTypeProp.type = draco::DT_UINT8;

// Свойство: твёрдость
draco::PropertyTableProperty hardnessProp;
hardnessProp.name = "hardness";
hardnessProp.type = draco::DT_FLOAT32;

structuralMeta.AddPropertyTableSchema(schema);
#endif
```

## Scene Graph

Управление иерархией объектов.

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/scene/scene.h>
#include <draco/scene/scene_node.h>

draco::Scene scene;

// Создание узлов
auto rootNode = std::make_unique<draco::SceneNode>();
rootNode->SetName("Root");

auto chunkNode = std::make_unique<draco::SceneNode>();
chunkNode->SetName("VoxelChunk_001");
chunkNode->SetTranslation({100.0f, 0.0f, 100.0f});

// Иерархия
chunkNode->SetParentIndex(0);  // Root

scene.AddNode(std::move(rootNode));
scene.AddNode(std::move(chunkNode));

// Привязка mesh к узлу
scene.AssignMeshToNode(meshIndex, nodeIndex);
#endif
```

## Custom Attribute Processing

### Создание кастомного атрибута

```cpp
// GENERIC атрибут для пользовательских данных
draco::GeometryAttribute customAttr;
customAttr.Init(
    draco::GeometryAttribute::GENERIC,
    nullptr,
    4,                  // 4 компонента
    draco::DT_FLOAT32,  // float
    false,
    sizeof(float) * 4,
    0
);

int customAttrId = mesh.AddAttribute(customAttr, true, numVertices);

// Заполнение данных
draco::PointAttribute* attr = mesh.attribute(customAttrId);
for (draco::AttributeValueIndex i(0); i < numVertices; ++i) {
    float customData[4] = {
        computeCustomValue0(i),
        computeCustomValue1(i),
        computeCustomValue2(i),
        computeCustomValue3(i)
    };
    attr->SetAttributeValue(i, customData);
}
```

### Named attributes

```cpp
// Добавление имени через metadata
auto attrMeta = std::make_unique<draco::AttributeMetadata>(customAttrId);
attrMeta->AddEntryString("name", "occlusion_data");
attrMeta->AddEntryString("description", "Ambient occlusion factors per vertex");
mesh.AddAttributeMetadata(customAttrId, std::move(attrMeta));

// Поиск по имени
const draco::AttributeMetadata* foundMeta =
    mesh.GetAttributeMetadataByStringEntry("name", "occlusion_data");
if (foundMeta) {
    int attrId = mesh.GetAttributeIdByUniqueId(foundMeta->att_unique_id());
    const draco::PointAttribute* attr = mesh.attribute(attrId);
}
```

## Expert Encoding Patterns

### Динамическое определение квантования

```cpp
draco::ExpertEncoder encoder(mesh);

for (int i = 0; i < mesh.num_attributes(); ++i) {
    const draco::PointAttribute* attr = mesh.attribute(i);

    int quantBits = 0;  // 0 = без квантования

    switch (attr->attribute_type()) {
        case draco::GeometryAttribute::POSITION:
            quantBits = determinePositionQuantization(attr);
            break;
        case draco::GeometryAttribute::NORMAL:
            quantBits = determineNormalQuantization(attr);
            break;
        case draco::GeometryAttribute::TEX_COORD:
            quantBits = 10;
            break;
        default:
            // GENERIC атрибуты без квантования
            break;
    }

    if (quantBits > 0) {
        encoder.SetQuantizationBitsForAttribute(i, quantBits);
    }
}
```

### Адаптивный prediction scheme

```cpp
draco::ExpertEncoder encoder(mesh);

for (int i = 0; i < mesh.num_attributes(); ++i) {
    const draco::PointAttribute* attr = mesh.attribute(i);

    int predictionScheme = draco::PREDICTION_DIFFERENCE;

    if (attr->attribute_type() == draco::GeometryAttribute::POSITION) {
        // Анализируем геометрию
        if (isSmoothMesh(mesh)) {
            predictionScheme = draco::MESH_PREDICTION_MULTI_PARALLELOGRAM;
        } else {
            predictionScheme = draco::MESH_PREDICTION_PARALLELOGRAM;
        }
    } else if (attr->attribute_type() == draco::GeometryAttribute::NORMAL) {
        predictionScheme = draco::MESH_PREDICTION_GEOMETRIC_NORMAL;
    }

    encoder.SetPredictionSchemeForAttribute(i, predictionScheme);
}
```

## Parallel Processing

### Многопоточное кодирование

```cpp
#include <thread>
#include <vector>

void encodeMeshesParallel(const std::vector<draco::Mesh>& meshes,
                          std::vector<std::vector<char>>& outputs) {
    outputs.resize(meshes.size());

    std::vector<std::thread> threads;
    const int numThreads = std::thread::hardware_concurrency();

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = t; i < meshes.size(); i += numThreads) {
                draco::Encoder encoder;
                encoder.SetSpeedOptions(5, 5);

                draco::EncoderBuffer buffer;
                encoder.EncodeMeshToBuffer(meshes[i], &buffer);

                outputs[i].assign(buffer.data(), buffer.data() + buffer.size());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}
```

> **Примечание:** Draco encoder не thread-safe. Каждый поток должен иметь свой Encoder.

---

## Решение проблем

<!-- anchor: 10_troubleshooting -->

🟡 **Уровень 2: Средний**

Типичные ошибки и их решения.

## Ошибки декодирования

### Invalid geometry type

**Симптом:**

```cpp
auto geomType = draco::Decoder::GetEncodedGeometryType(&buffer);
// geomType.status().code() != OK
```

**Причины:**

- Данные не являются Draco bitstream
- Повреждённый заголовок
- Неверная версия Draco

**Решения:**

```cpp
// Проверка заголовка
bool isDracoData(const void* data, size_t size) {
    if (size < 5) return false;
    const char* header = static_cast<const char*>(data);
    return strncmp(header, "DRACO", 5) == 0;
}

// Проверка перед декодированием
if (!isDracoData(data, size)) {
    std::cerr << "Not a valid Draco file\n";
    return;
}
```

### DecodeMeshFromBuffer failed

**Симптом:**

```cpp
auto result = decoder.DecodeMeshFromBuffer(&buffer);
// result.ok() == false
```

**Причины:**

- Point cloud вместо mesh
- Несовместимая версия bitstream
- Недостаточно данных

**Решения:**

```cpp
// Проверка типа геометрии
auto geomType = draco::Decoder::GetEncodedGeometryType(&buffer);
if (!geomType.ok()) {
    std::cerr << "Cannot determine geometry type\n";
    return;
}

draco::Decoder decoder;

if (geomType.value() == draco::TRIANGULAR_MESH) {
    auto result = decoder.DecodeMeshFromBuffer(&buffer);
} else if (geomType.value() == draco::POINT_CLOUD) {
    auto result = decoder.DecodePointCloudFromBuffer(&buffer);
} else {
    std::cerr << "Invalid geometry type\n";
}
```

### Attribute not found

**Симптом:**

```cpp
const auto* attr = mesh.GetNamedAttribute(draco::GeometryAttribute::NORMAL);
// attr == nullptr
```

**Причины:**

- Атрибут не был закодирован
- Неправильный тип атрибута

**Решения:**

```cpp
// Проверка наличия атрибута
const auto* attr = mesh.GetNamedAttribute(draco::GeometryAttribute::NORMAL);
if (!attr) {
    std::cout << "Warning: Normal attribute not found\n";
    // Создать default normals или продолжить без них
}

// Перебор всех атрибутов
for (int i = 0; i < mesh.num_attributes(); ++i) {
    const auto* attr = mesh.attribute(i);
    std::cout << "Attribute " << i << ": "
              << draco::GeometryAttribute::TypeToString(attr->attribute_type())
              << "\n";
}
```

## Ошибки кодирования

### Encoding failed

**Симптом:**

```cpp
auto status = encoder.EncodeMeshToBuffer(mesh, &buffer);
// status.ok() == false
```

**Причины:**

- Некорректный mesh (нет граней, нет точек)
- Отсутствует position attribute
- Несовместимые настройки

**Решения:**

```cpp
// Проверка mesh перед кодированием
bool validateMesh(const draco::Mesh& mesh) {
    if (mesh.num_points() == 0) {
        std::cerr << "Mesh has no points\n";
        return false;
    }

    if (mesh.num_faces() == 0) {
        std::cerr << "Mesh has no faces\n";
        return false;
    }

    const auto* pos = mesh.GetNamedAttribute(draco::GeometryAttribute::POSITION);
    if (!pos) {
        std::cerr << "Mesh has no position attribute\n";
        return false;
    }

    return true;
}

if (!validateMesh(mesh)) {
    return;
}
```

### Quantization out of range

**Симптом:**

```cpp
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 20);
// Ошибка или неожиданный результат
```

**Причина:** Квантование > 16 бит не поддерживается для float атрибутов.

**Решение:**

```cpp
// Допустимые значения: 1-16 для position/normal/texcoord
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);  // OK
encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 10);    // OK
encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 12); // OK
```

### Prediction scheme failed

**Симптом:**

```cpp
auto status = encoder.SetAttributePredictionScheme(type, scheme);
// status.ok() == false
```

**Причина:** Prediction scheme несовместим с типом атрибута или геометрии.

**Решение:**

```cpp
// Используйте автоматический выбор
draco::Encoder encoder;
encoder.SetSpeedOptions(0, 0);  // Автоматический выбор prediction schemes

// Или проверяйте совместимость
// MESH_PREDICTION_PARALLELOGRAM — только для mesh positions
// MESH_PREDICTION_GEOMETRIC_NORMAL — только для normals
// PREDICTION_DIFFERENCE — универсальный
```

## Проблемы производительности

### Медленное декодирование

**Причина:** Высокий compression level или Edgebreaker.

**Решения:**

```cpp
// При кодировании оптимизируйте под decode speed
encoder.SetSpeedOptions(5, 10);  // encoding_speed=5, decoding_speed=10

// Используйте Sequential вместо Edgebreaker
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
```

### Большой размер файла

**Причина:** Низкое квантование или неоптимальные настройки.

**Решения:**

```cpp
// Увеличьте compression level
encoder.SetSpeedOptions(0, 0);  // Максимальный compression

// Уменьшите quantization bits
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 10);
encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 6);

// Используйте Edgebreaker
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);

// Deduplication перед кодированием
mesh.DeduplicatePointIds();
mesh.DeduplicateAttributeValues();
```

### Высокое потребление памяти

**Причина:** Большие mesh или множественное кодирование.

**Решения:**

```cpp
// Обработка по частям
// Разбейте большие модели на chunks

// Освобождение промежуточных данных
{
    draco::EncoderBuffer buffer;
    encoder.EncodeMeshToBuffer(mesh, &buffer);
    // Сохранение buffer
}  // buffer уничтожен

// Переиспользование encoder
draco::Encoder encoder;  // Создаётся один раз
for (const auto& mesh : meshes) {
    draco::EncoderBuffer buffer;
    encoder.EncodeMeshToBuffer(mesh, &buffer);
    // ...
    encoder.Reset();  // Сброс для следующего mesh
}
```

## Проблемы совместимости

### Версия bitstream

**Симптом:** Старый decoder не может прочитать новые файлы.

**Решение:**

```cpp
// Проверка версии при кодировании
// Draco гарантирует обратную совместимость decoder

// Mesh bitstream: 2.2
// Point cloud bitstream: 2.3

// Для максимальной совместимости используйте стандартные настройки
draco::Encoder encoder;
encoder.SetSpeedOptions(7, 7);  // Default compression level
```

### Кроссплатформенность

**Проблема:** Разные результаты на разных платформах.

**Решения:**

```cpp
// Избегайте платформенно-зависимых типов
// Используйте явные типы размеров
static_assert(sizeof(float) == 4, "float must be 32-bit");

// Проверяйте endianness при передаче данных между системами
```

## Проблемы glTF

### EXT_mesh_draco не распознаётся

**Причина:** glTF loader не поддерживает расширение.

**Решение:**

```cpp
// Проверьте extensionsUsed
// "KHR_draco_mesh_compression" должен быть в списке

// Убедитесь, что loader поддерживает Draco
// three.js: используйте DRACOLoader
// Filament: встроенная поддержка
// Babylon.js: встроенная поддержка
```

### Текстуры потеряны после transcoding

**Причина:** Текстуры не включены в Draco-сжатие.

**Решение:**

```bash
# Draco сжимает только геометрию
# Текстуры обрабатываются отдельно

# При использовании draco_transcoder:
draco_transcoder -i input.glb -o output.glb
# Текстуры сохраняются в output.glb
```

## Диагностика

### Вывод информации о mesh

```cpp
void diagnoseMesh(const draco::Mesh& mesh) {
    std::cout << "=== Mesh Diagnostics ===\n";
    std::cout << "Points: " << mesh.num_points() << "\n";
    std::cout << "Faces: " << mesh.num_faces() << "\n";
    std::cout << "Attributes: " << mesh.num_attributes() << "\n";

    for (int i = 0; i < mesh.num_attributes(); ++i) {
        const auto* attr = mesh.attribute(i);
        std::cout << "  [" << i << "] "
                  << draco::GeometryAttribute::TypeToString(attr->attribute_type())
                  << " (components: " << static_cast<int>(attr->num_components())
                  << ", values: " << attr->size() << ")\n";
    }

    auto bbox = mesh.ComputeBoundingBox();
    std::cout << "Bounding box: ["
              << bbox.GetMinPoint()[0] << ", " << bbox.GetMinPoint()[1] << ", " << bbox.GetMinPoint()[2]
              << "] - ["
              << bbox.GetMaxPoint()[0] << ", " << bbox.GetMaxPoint()[1] << ", " << bbox.GetMaxPoint()[2]
              << "]\n";
}
```

### Проверка целостности

```cpp
bool validateMeshIntegrity(const draco::Mesh& mesh) {
    // Проверка граней
    for (draco::FaceIndex f(0); f < mesh.num_faces(); ++f) {
        const auto& face = mesh.face(f);
        for (int i = 0; i < 3; ++i) {
            if (face[i].value() >= mesh.num_points()) {
                std::cerr << "Face " << f.value() << " references invalid point\n";
                return false;
            }
        }
    }

    // Проверка атрибутов
    for (int i = 0; i < mesh.num_attributes(); ++i) {
        const auto* attr = mesh.attribute(i);
        if (attr->size() == 0) {
            std::cerr << "Attribute " << i << " is empty\n";
            return false;
        }
    }

    return true;
}
```

### Отладка квантования

```cpp
void analyzeQuantization(const draco::Mesh& original, const draco::Mesh& decoded) {
    const auto* origPos = original.GetNamedAttribute(draco::GeometryAttribute::POSITION);
    const auto* decodedPos = decoded.GetNamedAttribute(draco::GeometryAttribute::POSITION);

    if (!origPos || !decodedPos) return;

    double maxError = 0.0;
    double avgError = 0.0;

    for (draco::PointIndex i(0); i < original.num_points(); ++i) {
        std::array<float, 3> orig, dec;
        origPos->GetValue(origPos->mapped_index(i), &orig);
        decodedPos->GetValue(decodedPos->mapped_index(i), &dec);

        double error = 0.0;
        for (int c = 0; c < 3; ++c) {
            error += std::abs(orig[c] - dec[c]);
        }
        error /= 3.0;

        maxError = std::max(maxError, error);
        avgError += error;
    }
    avgError /= original.num_points();

    std::cout << "Quantization error: max=" << maxError << ", avg=" << avgError << "\n";
}
```

## Частые вопросы

### Как уменьшить размер файла?

1. Уменьшите quantization bits
2. Увеличьте compression level (lower speed)
3. Используйте Edgebreaker
4. Выполните deduplication
5. Удалите неиспользуемые атрибуты

### Как ускорить загрузку?

1. Используйте Sequential encoding
2. Увеличьте decoding_speed в SetSpeedOptions
3. Предзагрузка в фоновом потоке
4. Кэширование декодированных mesh

### Можно ли использовать Draco для real-time streaming?

Да, с правильными настройками:

```cpp
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);  // Fastest
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
```

### Поддерживает ли Draco morph targets?

Через DRACO_TRANSCODER_SUPPORTED и glTF EXT_mesh_features.
