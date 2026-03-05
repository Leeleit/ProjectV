# glTF Transcoding

🟡 **Уровень 2: Средний**

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
