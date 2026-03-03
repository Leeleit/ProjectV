# Продвинутые возможности

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