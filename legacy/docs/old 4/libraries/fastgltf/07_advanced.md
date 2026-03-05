# Продвинутые темы fastgltf

🔴 **Уровень 3: Продвинутый**

Sparse accessors, morph targets, анимации, скиннинг и GPU-driven загрузка.

## Sparse Accessors

### Концепция

Sparse accessors позволяют хранить только изменённые значения вместо полного дублирования буфера.

```
Базовый буфер (или нули)
        ↓
    Инициализация
        ↓
Sparse indices (какие элементы заменяются)
        ↓
Sparse values (новые значения)
        ↓
    Итоговый accessor
```

### Применение

- Partial updates вершинных данных
- Сжатие анимационных ключевых кадров
- Динамические изменения ландшафта

### Структура

```cpp
struct SparseAccessor {
    size_t count;                    // Количество изменённых элементов
    size_t indicesAccessor;          // Accessor с индексами
    size_t valuesAccessor;           // Accessor с новыми значениями
    ComponentType indicesComponentType;
    // ...
};
```

### Использование

Accessor tools автоматически обрабатывают sparse:

```cpp
// Автоматическая обработка
std::vector<glm::vec3> vertices(accessor.count);
fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, vertices.data());
// vertices содержит итоговые данные с применёнными sparse значениями
```

### Ручная обработка

```cpp
if (accessor.sparse.has_value()) {
    const auto& sparse = *accessor.sparse;

    // 1. Базовые данные
    if (accessor.bufferViewIndex.has_value()) {
        fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, output.data());
    } else {
        std::fill(output.begin(), output.end(), glm::vec3(0.0f));
    }

    // 2. Индексы
    std::vector<uint32_t> indices(sparse.count);
    fastgltf::copyFromAccessor<uint32_t>(
        asset, asset.accessors[sparse.indicesAccessor], indices.data());

    // 3. Новые значения
    std::vector<glm::vec3> values(sparse.count);
    fastgltf::copyFromAccessor<glm::vec3>(
        asset, asset.accessors[sparse.valuesAccessor], values.data());

    // 4. Применение
    for (size_t i = 0; i < sparse.count; ++i) {
        output[indices[i]] = values[i];
    }
}
```

---

## Morph Targets (Blend Shapes)

### Концепция

Morph targets позволяют деформировать меш путём интерполяции между базовой формой и целевыми формами.

```
vertex_final = vertex_base + Σ(weight[i] * target_offset[i])
```

### Структура в glTF

```
Mesh
├── primitives[]
│   └── Primitive
│       ├── attributes (POSITION, NORMAL, ...)
│       └── targets[] (MorphTarget)
│           ├── POSITION delta
│           ├── NORMAL delta
│           └── TANGENT delta
└── weights[] (веса по умолчанию)
```

### Извлечение morph targets

```cpp
std::vector<glm::vec3> extractMorphDeltas(
    const fastgltf::Asset& asset,
    const fastgltf::Primitive& primitive,
    size_t targetIndex) {

    std::vector<glm::vec3> deltas;

    if (targetIndex >= primitive.targets.size()) {
        return deltas;
    }

    const auto& target = primitive.targets[targetIndex];
    auto it = target.find("POSITION");

    if (it != target.end()) {
        const auto& accessor = asset.accessors[it->second];
        deltas.resize(accessor.count);
        fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, deltas.data());
    }

    return deltas;
}
```

### Применение morph targets

```cpp
void applyMorphTargets(
    std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& basePositions,
    const std::vector<std::vector<glm::vec3>>& targetDeltas,
    const std::vector<float>& weights) {

    positions = basePositions;

    for (size_t t = 0; t < targetDeltas.size() && t < weights.size(); ++t) {
        float weight = weights[t];
        const auto& deltas = targetDeltas[t];

        for (size_t v = 0; v < positions.size() && v < deltas.size(); ++v) {
            positions[v] += deltas[v] * weight;
        }
    }
}
```

### GLSL шейдер

```glsl
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

// Morph target data
layout (binding = 0) buffer MorphPositions { vec3 morphPositions[]; };
layout (binding = 1) buffer MorphNormals { vec3 morphNormals[]; };

uniform float morphWeights[MAX_MORPH_TARGETS];
uniform int morphTargetCount;
uniform int vertexCount;

void main() {
    vec3 position = inPosition;
    vec3 normal = inNormal;

    for (int t = 0; t < morphTargetCount; ++t) {
        int offset = t * vertexCount + gl_VertexIndex;
        position += morphPositions[offset] * morphWeights[t];
        normal += morphNormals[offset] * morphWeights[t];
    }

    normal = normalize(normal);
    // ...
}
```

---

## Анимации

### Структура

```
Animation
├── channels[]
│   └── AnimationChannel
│       ├── target.node
│       ├── target.path (translation/rotation/scale/weights)
│       └── samplerIndex
└── samplers[]
    └── AnimationSampler
        ├── inputAccessor (время ключевых кадров)
        ├── outputAccessor (значения)
        └── interpolation (LINEAR/STEP/CUBICSPLINE)
```

### Интерполяция

| Тип           | Описание              | Использование                  |
|---------------|-----------------------|--------------------------------|
| `LINEAR`      | Линейная интерполяция | Большинство анимаций           |
| `STEP`        | Без интерполяции      | Visibility, discrete           |
| `CUBICSPLINE` | Кубический сплайн     | Camera paths, плавные движения |

### Обработка анимации

```cpp
void processAnimation(
    const fastgltf::Asset& asset,
    const fastgltf::Animation& animation,
    float time,
    std::map<size_t, glm::mat4>& nodeTransforms) {

    for (const auto& channel : animation.channels) {
        if (!channel.nodeIndex.has_value()) continue;

        const auto& sampler = animation.samplers[channel.samplerIndex];
        const auto& inputAccessor = asset.accessors[sampler.inputAccessor];
        const auto& outputAccessor = asset.accessors[sampler.outputAccessor];

        // Временные метки
        std::vector<float> keyframeTimes;
        fastgltf::iterateAccessor<float>(asset, inputAccessor,
            [&](float t) { keyframeTimes.push_back(t); });

        // Поиск текущего ключевого кадра
        size_t keyframeIndex = 0;
        for (size_t i = 0; i < keyframeTimes.size() - 1; ++i) {
            if (time >= keyframeTimes[i] && time < keyframeTimes[i + 1]) {
                keyframeIndex = i;
                break;
            }
        }

        float t0 = keyframeTimes[keyframeIndex];
        float t1 = keyframeTimes[keyframeIndex + 1];
        float alpha = (time - t0) / (t1 - t0);

        size_t nodeIndex = *channel.nodeIndex;

        switch (channel.path) {
            case fastgltf::AnimationPath::Translation: {
                std::vector<glm::vec3> values;
                fastgltf::iterateAccessor<glm::vec3>(asset, outputAccessor,
                    [&](glm::vec3 v) { values.push_back(v); });
                glm::vec3 pos = glm::mix(values[keyframeIndex],
                                         values[keyframeIndex + 1], alpha);
                nodeTransforms[nodeIndex] = glm::translate(
                    nodeTransforms[nodeIndex], pos);
                break;
            }
            case fastgltf::AnimationPath::Rotation: {
                std::vector<glm::quat> values;
                fastgltf::iterateAccessor<glm::quat>(asset, outputAccessor,
                    [&](glm::quat q) { values.push_back(q); });
                glm::quat rot = glm::slerp(values[keyframeIndex],
                                           values[keyframeIndex + 1], alpha);
                nodeTransforms[nodeIndex] *= glm::mat4_cast(rot);
                break;
            }
            case fastgltf::AnimationPath::Scale: {
                std::vector<glm::vec3> values;
                fastgltf::iterateAccessor<glm::vec3>(asset, outputAccessor,
                    [&](glm::vec3 v) { values.push_back(v); });
                glm::vec3 scale = glm::mix(values[keyframeIndex],
                                           values[keyframeIndex + 1], alpha);
                nodeTransforms[nodeIndex] = glm::scale(
                    nodeTransforms[nodeIndex], scale);
                break;
            }
            case fastgltf::AnimationPath::Weights: {
                // Morph target weights
                break;
            }
        }
    }
}
```

### CUBICSPLINE интерполяция

```cpp
glm::vec3 cubicSplineInterpolate(
    const std::vector<glm::vec3>& data,
    size_t keyframeIndex,
    float alpha,
    float deltaTime) {

    // [in-tangent, value, out-tangent] для каждого кадра
    size_t idx = keyframeIndex * 3;
    glm::vec3 inTangent = data[idx];
    glm::vec3 value = data[idx + 1];
    glm::vec3 outTangent = data[idx + 2];

    glm::vec3 nextValue = data[(keyframeIndex + 1) * 3 + 1];
    glm::vec3 nextInTangent = data[(keyframeIndex + 1) * 3];

    float t = alpha;
    float t2 = t * t;
    float t3 = t2 * t;

    return (2.0f * t3 - 3.0f * t2 + 1.0f) * value +
           (t3 - 2.0f * t2 + t) * outTangent * deltaTime +
           (-2.0f * t3 + 3.0f * t2) * nextValue +
           (t3 - t2) * nextInTangent * deltaTime;
}
```

---

## Скиннинг (Skeletal Animation)

### Структура

```
Skin
├── joints[] (индексы Node)
├── inverseBindMatrices (Accessor с матрицами)
└── skeleton (опционально, root node)

Primitive
├── JOINTS_0 (uvec4 — индексы костей)
└── WEIGHTS_0 (vec4 — веса)
```

### Извлечение данных скиннинга

```cpp
struct SkeletonData {
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> jointMatrices;
    std::vector<size_t> jointNodeIndices;
};

SkeletonData extractSkinData(const fastgltf::Asset& asset,
                             const fastgltf::Skin& skin) {
    SkeletonData data;
    size_t jointCount = skin.joints.size();

    data.jointNodeIndices = skin.joints;
    data.jointMatrices.resize(jointCount, glm::mat4(1.0f));

    if (skin.inverseBindMatrices.has_value()) {
        const auto& accessor = asset.accessors[*skin.inverseBindMatrices];
        data.inverseBindMatrices.resize(jointCount);
        fastgltf::copyFromAccessor<glm::mat4>(
            asset, accessor, data.inverseBindMatrices.data());
    } else {
        data.inverseBindMatrices.resize(jointCount, glm::mat4(1.0f));
    }

    return data;
}
```

### Обновление joint matrices

```cpp
void updateJointMatrices(
    SkeletonData& skeleton,
    const std::vector<glm::mat4>& nodeGlobalTransforms) {

    for (size_t i = 0; i < skeleton.jointNodeIndices.size(); ++i) {
        size_t nodeIndex = skeleton.jointNodeIndices[i];
        skeleton.jointMatrices[i] =
            nodeGlobalTransforms[nodeIndex] * skeleton.inverseBindMatrices[i];
    }
}
```

### Веса вершин

```cpp
struct VertexSkinData {
    glm::uvec4 jointIndices;
    glm::vec4 weights;
};

std::vector<VertexSkinData> extractVertexSkinData(
    const fastgltf::Asset& asset,
    const fastgltf::Primitive& primitive) {

    auto jointsIt = primitive.findAttribute("JOINTS_0");
    auto weightsIt = primitive.findAttribute("WEIGHTS_0");

    if (jointsIt == primitive.attributes.end() ||
        weightsIt == primitive.attributes.end()) {
        return {};
    }

    size_t vertexCount = asset.accessors[jointsIt->accessorIndex].count;
    std::vector<VertexSkinData> skinData(vertexCount);

    // Извлечение индексов костей
    const auto& jointsAccessor = asset.accessors[jointsIt->accessorIndex];
    if (jointsAccessor.componentType == fastgltf::ComponentType::UnsignedByte) {
        std::vector<glm::u8vec4> temp(vertexCount);
        fastgltf::copyFromAccessor<glm::u8vec4>(asset, jointsAccessor, temp.data());
        for (size_t i = 0; i < vertexCount; ++i) {
            skinData[i].jointIndices = glm::uvec4(temp[i]);
        }
    } else if (jointsAccessor.componentType ==
               fastgltf::ComponentType::UnsignedShort) {
        std::vector<glm::u16vec4> temp(vertexCount);
        fastgltf::copyFromAccessor<glm::u16vec4>(asset, jointsAccessor, temp.data());
        for (size_t i = 0; i < vertexCount; ++i) {
            skinData[i].jointIndices = glm::uvec4(temp[i]);
        }
    }

    // Извлечение весов
    const auto& weightsAccessor = asset.accessors[weightsIt->accessorIndex];
    fastgltf::copyFromAccessor<glm::vec4>(asset, weightsAccessor,
        [&](glm::vec4 weight, size_t idx) {
            skinData[idx].weights = weight;
        });

    // Нормализация
    for (auto& sd : skinData) {
        float sum = sd.weights.x + sd.weights.y + sd.weights.z + sd.weights.w;
        if (sum > 0.0f) sd.weights /= sum;
    }

    return skinData;
}
```

### GLSL шейдер

```glsl
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in uvec4 inJoints;
layout (location = 3) in vec4 inWeights;

layout (binding = 0) readonly buffer JointMatrices {
    mat4 jointMatrices[];
};

void main() {
    mat4 skinMatrix =
        inWeights.x * jointMatrices[inJoints.x] +
        inWeights.y * jointMatrices[inJoints.y] +
        inWeights.z * jointMatrices[inJoints.z] +
        inWeights.w * jointMatrices[inJoints.w];

    vec3 position = (skinMatrix * vec4(inPosition, 1.0)).xyz;
    vec3 normal = normalize((skinMatrix * vec4(inNormal, 0.0)).xyz);

    gl_Position = projection * view * model * vec4(position, 1.0);
}
```

---

## GPU-Driven Загрузка

### Zero-copy через BufferAllocationCallback

```cpp
struct GpuContext {
    // Ваш GPU context (VMA, VkDevice, ...)
};

fastgltf::Parser createGpuParser(GpuContext& ctx) {
    fastgltf::Parser parser;
    parser.setUserPointer(&ctx);

    parser.setBufferAllocationCallback(
        [](void* userPointer, size_t bufferSize,
           fastgltf::BufferAllocateFlags flags) -> fastgltf::BufferInfo {

            auto* ctx = static_cast<GpuContext*>(userPointer);

            // Выделение GPU буфера с mapped памятью
            void* mappedPtr = allocateGpuBuffer(ctx, bufferSize);

            return fastgltf::BufferInfo{
                .mappedMemory = mappedPtr,
                .customId = /* ваш ID */
            };
        },
        nullptr,  // unmap
        nullptr   // deallocate
    );

    return parser;
}
```

### Преимущества

- Нет промежуточных CPU буферов
- Данные записываются сразу в GPU память
- Минимальное копирование

### Требования

- GPU буфер должен быть host-visible
- Или использовать staging buffer с последующим transfer

---

## Резюме

| Тема                 | Ключевые концепции                 | Инструменты fastgltf                  |
|----------------------|------------------------------------|---------------------------------------|
| **Sparse Accessors** | Partial updates, экономия памяти   | `Accessor::sparse`, accessor tools    |
| **Morph Targets**    | Blend shapes, интерполяция         | `Primitive::targets`, `Mesh::weights` |
| **Анимации**         | Keyframes, каналы, интерполяция    | `Animation`, `AnimationSampler`       |
| **Скиннинг**         | Кости, веса, inverse bind matrices | `Skin`, JOINTS_0, WEIGHTS_0           |
| **GPU-Driven**       | Zero-copy, callbacks               | `setBufferAllocationCallback`         |
