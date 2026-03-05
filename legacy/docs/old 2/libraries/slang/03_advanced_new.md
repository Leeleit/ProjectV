## Производительность Slang

<!-- anchor: 07_performance -->

**🟡 Уровень 2: Средний** — Оптимизации производительности компиляции и выполнения шейдеров Slang.

---

## Оптимизация времени компиляции

### Модульная компиляция

Модульная система Slang позволяет компилировать только изменённые модули:

```
Изменённый модуль → Перекомпиляция только этого модуля
                         ↓
Кэшированные модули → Линковка → SPIR-V
```

При изменении одного модуля в большой кодобазе (100+ файлов):

- **GLSL**: полная перекомпиляция (10-30 секунд)
- **Slang**: инкрементальная перекомпиляция (1-5 секунд)

### Параметры компиляции

```bash
# Для разработки: быстрая компиляция
slangc shader.slang -o shader.spv -target spirv -O1

# Для production: максимальная оптимизация
slangc shader.slang -o shader.spv -target spirv -O3

# Параллельная компиляция (Slang API)
# session->setEnableParallelCompilation(true);
```

### Кэширование модулей

```cpp
// Включение кэша на диске через Slang API
slang::SessionDesc sessionDesc{};
sessionDesc.enableEffectCache = true;
sessionDesc.effectCachePath = "build/.slang_cache";
```

### Структура модулей для кэширования

```
shaders/
├── core/           # Редко меняется → кэшируется надолго
│   ├── types.slang
│   └── math.slang
├── materials/      # Иногда меняется
│   └── pbr.slang
└── render/         # Часто меняется → перекомпилируется
    └── main.slang
```

---

## Оптимизация времени выполнения

### Специализация generics

```slang
// Generic шейдер
generic<T>
struct Processor
{
    T process(T input) { return input * 2.0; }
};

// Явная специализация для конкретного типа
specialized<float>
struct Processor<float>
{
    float process(float input)
    {
        // Компилятор может применить специфичные оптимизации
        return fma(input, 2.0, 0.0);  // FMA инструкция
    }
};
```

### Инлайнинг функций

```slang
// Принудительный инлайн для критичных функций
[ForceInline]
float criticalCalculation(float x)
{
    return x * 2.0 + 1.0;
}

// Запрет инлайна для больших функций
[NoInline]
float complexNoise(float3 p)
{
    // Сложные вычисления, инлайн ухудшит производительность
    return fractalNoise(p, 8);
}
```

### Группировка данных

```slang
// SOA (Structure of Arrays) для лучшей cache locality
struct VoxelDataSOA
{
    float* densities;   // Все densities подряд
    float3* colors;     // Все colors подряд
    uint* materials;    // Все materials подряд
};

// Вместо AOS (Array of Structures)
struct VoxelDataAOS
{
    float density;
    float3 color;
    uint material;
};
// Плохо для cache locality при обработке одного поля
```

### Shared memory в compute shaders

```slang
[numthreads(32, 32, 1)]
void csMain(uint3 id : SV_DispatchThreadID, uint3 localId : SV_GroupThreadID)
{
    // Shared memory для данных, используемых в группе
    groupshared float sharedData[32][32];

    // Загрузка в shared memory
    sharedData[localId.x][localId.y] = input[id.xy];

    // Синхронизация перед использованием
    GroupMemoryBarrierWithGroupSync();

    // Обработка с быстрым доступом к shared memory
    float result = process(sharedData, localId);
    output[id.xy] = result;
}
```

---

## Сравнение с GLSL/HLSL

### Время компиляции

| Сценарий                       | GLSL               | Slang (первая компиляция) | Slang (с кэшем) |
|--------------------------------|--------------------|---------------------------|-----------------|
| Большая кодобаза (100+ файлов) | 10-30 сек          | 15-45 сек                 | 1-5 сек         |
| Изменение одного файла         | 10-30 сек (полная) | 1-3 сек (инкремент)       | 0.5-2 сек       |
| Маленький проект (1-5 файлов)  | 0.1-0.5 сек        | 0.2-0.8 сек               | 0.1-0.3 сек     |

### Размер SPIR-V

| Метрика               | GLSL    | Slang  |
|-----------------------|---------|--------|
| Размер бинарника      | Базовый | +0-10% |
| Количество инструкций | Базовое | +0-5%  |
| Время выполнения      | Базовое | +0-5%  |

Разница в размере и производительности выполнения минимальна, так как оба компилируются в один и тот же SPIR-V.

---

## Профилирование

### Измерение времени компиляции

```bash
# Встроенное измерение времени фаз
slangc shader.slang -target spirv -o shader.spv -time-phases
```

### Slang API: измерение

```cpp
auto start = std::chrono::high_resolution_clock::now();

Slang::ComPtr<slang::IBlob> spirvCode;
linkedProgram->getEntryPointCode(0, 0, spirvCode.writeRef(), nullptr);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

std::cout << "Compilation time: " << duration.count() << "ms\n";
std::cout << "SPIR-V size: " << spirvCode->getBufferSize() << " bytes\n";
```

### Анализ SPIR-V

```bash
# Генерация SPIR-V ассемблера для анализа
slangc shader.slang -target spirv-asm -o shader.spvasm

# Подсчёт инструкций
grep -c "Op" shader.spvasm
```

---

## Рекомендации

### Организация кода

1. **Выносить стабильный код в модули**: типы данных, математика, константы
2. **Избегать циклических зависимостей**: модули должны иметь чёткую иерархию
3. **Минимизировать зависимости между модулями**: меньше зависимостей — быстрее компиляция

### Выбор стратегии компиляции

| Сценарий       | Стратегия                              |
|----------------|----------------------------------------|
| Финальный билд | Offline компиляция через CMake, -O3    |
| Разработка     | Runtime компиляция с кэшированием, -O1 |
| CI/CD          | Offline компиляция, проверка ошибок    |

### Память компилятора

```cpp
// Освобождение ресурсов после компиляции
session = nullptr;
globalSession = nullptr;
// Сборка мусора Slang
```

---

## Типичные проблемы

### Медленная компиляция

**Причины:**

- Большое количество модулей с перекрёстными зависимостями
- Глубокая вложенность `#include` или `import`
- Сложные generic-конструкции без специализации

**Решения:**

- Реорганизовать структуру модулей
- Использовать кэширование
- Явно специализировать generics для часто используемых типов

### Большой размер SPIR-V

**Причины:**

- Отладочная информация (`-g`)
- Неоптимизированный код
- Дублирование кода через copy-paste

**Решения:**

- Использовать `-O` или `-O2`
- Убрать отладочную информацию для production
- Вынести общий код в функции или модули

---

## Решение проблем Slang

<!-- anchor: 08_troubleshooting -->

**🟡 Уровень 2: Средний** — Диагностика и устранение ошибок компиляции и интеграции с Vulkan.

---

## Ошибки компиляции (slangc)

### `error: no module named 'X' found`

**Симптом:**

```
error: no module named 'math_utils' found
```

**Причины и решения:**

1. Файл не в пути поиска:

```bash
slangc shader.slang -I shaders/common -I shaders/utils -o shader.spv -target spirv
```

2. Неверный синтаксис импорта:

```slang
// Неверно — использование путей
import "shaders/common/math_utils";

// Верно — только имя модуля
import math_utils;
```

3. Неверное имя файла: файл должен называться `math_utils.slang`.

---

### `error: expected ';'` / синтаксические ошибки

**Частые причины:**

```slang
// Ошибка: атрибут без скобок
[numthreads 8, 8, 8]
void csMain() { }

// Верно
[numthreads(8, 8, 8)]
void csMain() { }

// Ошибка: неверный generic синтаксис
generic T myFunc(T x) { }

// Верно
generic<T> T myFunc(T x) { return x; }
```

---

### `error: type 'X' does not conform to interface 'Y'`

**Симптом:**

```
error: type 'MyMaterial' does not conform to interface 'IMaterial'
```

**Решение:** Проверьте, что реализованы все методы интерфейса:

```slang
interface IMaterial
{
    float3 albedo;
    float roughness;
    float3 evaluate(float3 viewDir, float3 lightDir);
};

// Неполная реализация — ошибка
struct MyMaterial : IMaterial
{
    float3 albedo;
    float roughness;
    // evaluate() отсутствует
};

// Полная реализация
struct MyMaterial : IMaterial
{
    float3 albedo;
    float roughness;

    float3 evaluate(float3 viewDir, float3 lightDir)
    {
        return albedo * max(dot(viewDir, lightDir), 0.0);
    }
};
```

---

### `error: cannot specialize generic with non-type argument`

```slang
// Ошибка: передача значения вместо типа
Processor<32> proc;  // 32 — значение

// Верно: передача типа
Processor<MyData> proc;

// Для параметров-значений используйте специализационные константы
[[vk::constant_id(0)]]
const uint SIZE = 32;
```

---

### `error: 'spirv_1_6' profile requires Vulkan 1.3`

**Решение:** Подберите соответствующий профиль:

| Vulkan | SPIR-V профиль |
|--------|----------------|
| 1.1    | spirv_1_3      |
| 1.2    | spirv_1_5      |
| 1.3    | spirv_1_6      |

---

## Ошибки Vulkan интеграции

### Validation layer: `VUID-VkShaderModuleCreateInfo-pCode-01379`

**Симптом:** Ошибка при `vkCreateShaderModule` — «pCode must be aligned to 4 bytes».

**Решение:** Читать SPIR-V в `uint32_t`-выровненный буфер:

```cpp
// Неверно
std::vector<char> code = readFile("shader.spv");
createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

// Верно
std::vector<uint32_t> readSPIRV(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    size_t size = file.tellg();
    assert(size % sizeof(uint32_t) == 0);

    std::vector<uint32_t> buffer(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}
```

---

### Validation layer: неверные binding-номера

**Симптом:** `Descriptor binding X in set Y is not bound`.

**Причина:** Несоответствие binding в шейдере и `VkDescriptorSetLayout`.

```slang
// Шейдер
[[vk::binding(0, 0)]] cbuffer Uniforms { float4x4 mvp; };
[[vk::binding(1, 0)]] Texture2D albedo;
[[vk::binding(2, 0)]] SamplerState sampler;
```

```cpp
// C++ — должно точно совпадать
VkDescriptorSetLayoutBinding bindings[] = {
    { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
    { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    { 2, VK_DESCRIPTOR_TYPE_SAMPLER,        1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
};
```

**Рекомендация:** Используйте Slang Reflection API для автоматического получения layout.

---

### Entry point не найден

**Симптом:**

```
VkError: entry point 'main' not found in SPIR-V module
```

**Причина:** В Slang имена entry points сохраняются как есть.

```cpp
// Неверно
shaderStageInfo.pName = "main";

// Верно — имя из шейдера
shaderStageInfo.pName = "vsMain";
```

Или переименуйте при компиляции:

```bash
slangc shader.slang -entry vsMain -stage vertex \
    -rename-entry-point vsMain main \
    -o vertex.spv -target spirv
```

---

### Push constants: неверный offset или size

**Симптом:** Validation layer — `Push constant range does not match shader`.

**Диагностика:**

```cpp
auto layout = linkedProgram->getLayout();
auto pushType = layout->getPushConstantsByIndex(0);
if (pushType) {
    size_t expectedSize = pushType->getSizeInBytes();
    // Сравните с вашим VkPushConstantRange.size
}
```

**Решение:** Убедитесь, что размер совпадает:

```slang
[[vk::push_constant]]
struct PushConstants
{
    float4x4 model;      // 64 байта
    float4x4 viewProj;   // 64 байта
    uint     index;      // 4 байта
    float3   _pad;       // 12 байт для выравнивания
} pc;                    // Итого: 144 байта
```

```cpp
VkPushConstantRange range{};
range.size = 144;  // Должно совпадать
```

---

## Проблемы с матрицами

### Неверный порядок умножения

**Симптом:** Объекты рендерятся неверно.

**Решение:**

```bash
# Флаг компилятора
slangc shader.slang -enable-slang-matrix-layout-row-major -target spirv
```

```slang
// Явное указание в шейдере
struct Transform
{
    row_major float4x4 model;
    row_major float4x4 viewProj;
};
```

---

## Отладка шейдеров

### Просмотр промежуточного кода

```bash
# SPIR-V ассемблер
slangc shader.slang -target spirv-asm -o shader.spvasm

# GLSL для сравнения
slangc shader.slang -target glsl -o shader.glsl

# Дамп IR (с -target и -o)
slangc shader.slang -dump-ir -target spirv -o shader.spv
```

### Отладочная информация

```bash
# Компиляция с debug info
slangc shader.slang -g -O0 -target spirv -o shader.spv
```

### RenderDoc

1. Компилируйте с `-g -O0`
2. Захватите кадр в RenderDoc
3. В Pipeline State → Shaders → View Source увидите исходный Slang код

---

## Чеклист диагностики

При возникновении проблем проверьте по порядку:

- [ ] `slangc --version` — убедитесь, что slangc доступен
- [ ] Путь поиска модулей (`-I`) настроен верно
- [ ] Профиль SPIR-V соответствует версии Vulkan
- [ ] Имя entry point в `VkPipelineShaderStageCreateInfo` совпадает с шейдером
- [ ] Все binding-номера совпадают с `VkDescriptorSetLayout`
- [ ] Размер push constants совпадает в шейдере и `VkPushConstantRange`
- [ ] Матричный порядок согласован между C++ и шейдером
- [ ] Validation layers Vulkan включены

---

## Полезные команды

```bash
# Проверка синтаксиса без генерации вывода
slangc shader.slang -dump-ir > /dev/null

# Валидация SPIR-V
export SLANG_RUN_SPIRV_VALIDATION=1
slangc shader.slang -target spirv -o shader.spv

# Подробный вывод
slangc shader.slang -verbose -target spirv -o shader.spv

# Время компиляции по фазам
slangc shader.slang -time-phases -target spirv -o shader.spv

---

## Сценарии использования Slang

<!-- anchor: 10_use-cases -->

**🔴 Уровень 3: Продвинутый** — Практические паттерны применения Slang для различных задач рендеринга.

---

## 1. Deferred Shading G-Buffer Pass

Рендер pass, записывающий геометрическую информацию в G-buffer для последующего deferred shading.

```slang
module gbuffer;

struct GBufferOutput
{
    float4 albedoMetallic   : SV_Target0;  // RGB: albedo, A: metallic
    float4 normalRoughness  : SV_Target1;  // RGB: world normal, A: roughness
    float4 emissiveAO       : SV_Target2;  // RGB: emissive, A: AO
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    uint materialId : TEXCOORD1;
};

[[vk::binding(0, 0)]]
cbuffer Uniforms
{
    float4x4 viewProj;
    float3 cameraPos;
};

[[vk::push_constant]]
struct PC
{
    float4x4 model;
    uint materialId;
} pc;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    float3 worldPos = mul(pc.model, float4(input.position, 1.0)).xyz;
    output.position = mul(viewProj, float4(worldPos, 1.0));
    output.normal = mul(pc.model, float4(input.normal, 0.0)).xyz;
    output.uv = input.uv;
    output.materialId = pc.materialId;
    return output;
}

[shader("fragment")]
GBufferOutput fsMain(VertexOutput input)
{
    GBufferOutput gbuf;
    float3 N = normalize(input.normal);

    gbuf.albedoMetallic = float4(0.8, 0.8, 0.8, 0.0);  // Default material
    gbuf.normalRoughness = float4(N * 0.5 + 0.5, 0.5);
    gbuf.emissiveAO = float4(0.0, 0.0, 0.0, 1.0);

    return gbuf;
}
```

---

## 2. GPU Culling с Compute Shader

Отсечение невидимых объектов на GPU без участия CPU.

```slang
module culling;

struct Frustum
{
    float4 planes[6];
};

struct AABB
{
    float3 min;
    float3 max;
};

[[vk::push_constant]]
struct PC
{
    Frustum frustum;
    uint objectCount;
} pc;

[[vk::binding(0, 0)]]
StructuredBuffer<AABB> objectAABBs;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> visibleObjects;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> visibleCount;

bool isAABBInFrustum(AABB aabb, Frustum frustum)
{
    for (int i = 0; i < 6; i++)
    {
        float3 n = frustum.planes[i].xyz;
        float d = frustum.planes[i].w;
        float3 pos = n >= 0 ? aabb.max : aabb.min;

        if (dot(n, pos) + d < 0.0)
            return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    uint objectId = tid.x;
    if (objectId >= pc.objectCount) return;

    AABB aabb = objectAABBs[objectId];

    if (isAABBInFrustum(aabb, pc.frustum))
    {
        uint slot;
        InterlockedAdd(visibleCount[0], 1, slot);
        visibleObjects[slot] = objectId;
    }
}
```

---

## 3. Ray Marching для SDF

Трассировка лучей через signed distance field.

```slang
module ray_march;

[[vk::binding(0, 0)]]
StructuredBuffer<float> sdfVolume;

[[vk::binding(1, 0)]]
RWTexture2D<float4> outputImage;

[[vk::push_constant]]
struct PC
{
    float4x4 invViewProj;
    float3 cameraPos;
    float maxDist;
    uint2 resolution;
    uint maxSteps;
} pc;

float sampleSDF(float3 worldPos)
{
    int3 coord = int3(worldPos);
    // Bounds checking and sampling
    return sdfVolume[coord.x + coord.y * 64 + coord.z * 64 * 64];
}

struct RayMarchResult
{
    bool hit;
    float t;
    float3 hitPos;
    float3 normal;
};

RayMarchResult sphereTrace(float3 ro, float3 rd)
{
    RayMarchResult result;
    result.hit = false;

    float t = 0.0;
    for (uint i = 0; i < pc.maxSteps; i++)
    {
        float3 p = ro + rd * t;
        float dist = sampleSDF(p);

        if (dist < 0.001)
        {
            result.hit = true;
            result.t = t;
            result.hitPos = p;

            // Normal via central differences
            float eps = 0.1;
            result.normal = normalize(float3(
                sampleSDF(p + float3(eps, 0, 0)) - sampleSDF(p - float3(eps, 0, 0)),
                sampleSDF(p + float3(0, eps, 0)) - sampleSDF(p - float3(0, eps, 0)),
                sampleSDF(p + float3(0, 0, eps)) - sampleSDF(p - float3(0, 0, eps))
            ));
            return result;
        }

        t += dist;
        if (t > pc.maxDist) break;
    }
    return result;
}

[numthreads(8, 8, 1)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid.xy >= pc.resolution)) return;

    float2 uv = (float2(tid.xy) + 0.5) / float2(pc.resolution);
    float2 ndc = uv * 2.0 - 1.0;

    float4 clipPos = float4(ndc, 1.0, 1.0);
    float4 worldPos = mul(pc.invViewProj, clipPos);
    worldPos /= worldPos.w;

    float3 rd = normalize(worldPos.xyz - pc.cameraPos);

    RayMarchResult result = sphereTrace(pc.cameraPos, rd);

    float4 color;
    if (result.hit)
    {
        float3 lightDir = normalize(float3(1, 2, 1));
        float NdotL = max(dot(result.normal, lightDir), 0.0);
        color = float4(float3(0.8, 0.6, 0.4) * NdotL + 0.1, 1.0);
    }
    else
    {
        color = float4(0.1, 0.2, 0.4, 1.0);
    }

    outputImage[tid.xy] = color;
}
```

---

## 4. Mesh Shader для процедурной геометрии

Генерация геометрии на GPU без vertex buffer.

```slang
module mesh_shader;

struct MeshVertex
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

[[vk::push_constant]]
struct PC
{
    float4x4 viewProj;
    float3 chunkOrigin;
    uint voxelCount;
} pc;

[shader("amplification")]
[numthreads(32, 1, 1)]
void asMain(uint3 tid : SV_DispatchThreadID)
{
    // Determine which voxels need mesh generation
    bool needsMesh = checkVoxelNeedsMesh(tid.x);

    if (needsMesh)
    {
        DispatchMesh(1, 1, 1);
    }
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(32, 1, 1)]
void msMain(
    uint gid : SV_GroupIndex,
    out vertices MeshVertex verts[24],
    out indices uint3 tris[12])
{
    uint voxelId = gid;

    // Generate cube vertices (6 faces * 4 vertices)
    SetMeshOutputCounts(24, 12);

    float3 origin = getVoxelPosition(voxelId) + pc.chunkOrigin;

    // Generate 6 faces of the cube
    for (uint face = 0; face < 6; face++)
    {
        uint base = face * 4;
        float3 n = getFaceNormal(face);

        for (uint v = 0; v < 4; v++)
        {
            verts[base + v].position = mul(pc.viewProj, float4(origin + getFaceVertex(face, v), 1.0));
            verts[base + v].normal = n;
        }

        tris[face * 2 + 0] = uint3(base, base + 1, base + 2);
        tris[face * 2 + 1] = uint3(base, base + 2, base + 3);
    }
}
```

---

## 5. Автоматическое дифференцирование

Оптимизация параметров через градиенты.

```slang
module neural_sdf;

[Differentiable]
float neuralNetwork(float3 pos, float4 weights[16])
{
    float h[16];
    for (int i = 0; i < 16; i++)
    {
        h[i] = pos.x * weights[i].x + pos.y * weights[i].y + pos.z * weights[i].z + weights[i].w;
        h[i] = max(h[i], 0.0);  // ReLU
    }

    float result = 0.0;
    for (int j = 0; j < 16; j++)
    {
        result += h[j] * weights[j].x;
    }
    return result;
}

[Differentiable]
float sdfFunction(float3 pos)
{
    float4 weights[16];  // Would be loaded from buffer
    return neuralNetwork(pos, weights);
}

// Compute gradient via forward mode
float3 computeGradient(float3 pos)
{
    float eps = 0.001;
    float3 grad;

    grad.x = (sdfFunction(pos + float3(eps, 0, 0)) - sdfFunction(pos - float3(eps, 0, 0))) / (2 * eps);
    grad.y = (sdfFunction(pos + float3(0, eps, 0)) - sdfFunction(pos - float3(0, eps, 0))) / (2 * eps);
    grad.z = (sdfFunction(pos + float3(0, 0, eps)) - sdfFunction(pos - float3(0, 0, eps))) / (2 * eps);

    return grad;
}
```

---

## 6. Post-Processing: Tonemapping

Финальный пасс с ACES tonemapping.

```slang
module tonemap;

[[vk::binding(0, 0)]]
Texture2D<float4> hdrInput;

[[vk::binding(1, 0)]]
RWTexture2D<float4> ldrOutput;

[[vk::binding(2, 0)]]
SamplerState linearSampler;

[[vk::push_constant]]
struct PC
{
    float exposure;
    float gamma;
    uint2 resolution;
} pc;

float3 acesFilmic(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[numthreads(8, 8, 1)]
void csMain(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid.xy >= pc.resolution)) return;

    float2 uv = (float2(tid.xy) + 0.5) / float2(pc.resolution);
    float3 hdr = hdrInput.SampleLevel(linearSampler, uv, 0).rgb;

    hdr *= pc.exposure;
    float3 ldr = acesFilmic(hdr);
    ldr = pow(ldr, 1.0 / pc.gamma);

    ldrOutput[tid.xy] = float4(ldr, 1.0);
}
```

---

## 7. Bindless Rendering

Динамический доступ к массиву текстур.

```slang
module bindless;

[[vk::binding(0, 0)]]
Texture2D materialTextures[];

[[vk::binding(1, 0)]]
SamplerState linearSampler;

[[vk::push_constant]]
struct PC
{
    float4x4 viewProj;
    uint baseTextureIndex;
} pc;

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    uint materialId : TEXCOORD1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation uint materialId : TEXCOORD1;
};

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(pc.viewProj, float4(input.position, 1.0));
    output.uv = input.uv;
    output.materialId = input.materialId;
    return output;
}

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target
{
    uint textureIndex = pc.baseTextureIndex + input.materialId;

    float4 color = materialTextures[NonUniformResourceIndex(textureIndex)]
        .Sample(linearSampler, input.uv);

    return color;
}

---

## Decision Trees: Slang

<!-- anchor: 11_decision-trees -->

**🟡 Уровень 2: Средний** — Выбор стратегий компиляции, организации шейдеров и интеграции.

---

## Slang или GLSL?

```

Нужен шейдер
│
├── Размер кодобазы?
│ ├── Один файл (< 200 строк)
│ │ └── Нужны generics или модули?
│ │ ├── Нет → GLSL достаточно
│ │ └── Да → Slang
│ │
│ ├── Несколько файлов (200-2000 строк)
│ │ └── Есть повторяющийся код?
│ │ ├── Нет → GLSL + include-файлы
│ │ └── Да → Slang (модули)
│ │
│ └── Большая кодобаза (2000+ строк)
│ └── Slang (модули, generics, инкрементальная компиляция)

```

**Критерии выбора Slang:**

- Модульная организация кода
- Generics для параметризации шейдеров
- Автоматическое дифференцирование
- Инкрементальная компиляция для больших проектов

**Когда GLSL достаточно:**

- Небольшой проект
- Нет потребности в модулях
- Минимум зависимостей

---

## Стратегия компиляции

```

Как компилировать шейдеры?
│
├── Во время сборки (CMake)
│ ├── Преимущества:
│ │ ├── Нет overhead в рантайме
│ │ ├── Ошибки при сборке
│ │ └── Оптимизированный SPIR-V
│ └── Когда: production, CI/CD
│
├── Во время выполнения (Slang API)
│ ├── Преимущества:
│ │ ├── Горячая перезагрузка
│ │ ├── Динамическая специализация
│ │ └── Адаптация под GPU
│ └── Когда: разработка, инструменты
│
└── Гибридный подход
├── Production: offline компиляция
└── Debug: runtime компиляция (ifdef DEBUG)

```

### Реализация гибридного подхода

```cpp
class ShaderManager
{
#ifdef DEBUG_SHADER_HOT_RELOAD
    // Runtime компиляция
    VkShaderModule compile(const char* slangFile, const char* entry);
#else
    // Загрузка precompiled .spv
    VkShaderModule load(const char* spvPath);
#endif
};
```

---

## Организация модулей

```
Как организовать шейдерные модули?
    │
    ├── По частоте изменений
    │   ├── core/ — редко меняется
    │   │   ├── types.slang
    │   │   ├── math.slang
    │   │   └── constants.slang
    │   │
    │   ├── features/ — иногда меняется
    │   │   ├── materials.slang
    │   │   ├── lighting.slang
    │   │   └── shadows.slang
    │   │
    │   └── render/ — часто меняется
    │       ├── main.slang
    │       └── postprocess.slang
    │
    └── Иерархия зависимостей
        core/ ← features/ ← render/
```

**Правила:**

- Модули нижнего уровня не зависят от верхнего
- Минимизировать перекрёстные зависимости
- Экспортировать только публичный API

---

## Offline vs Runtime специализация

```
Generic шейдер нужно специализировать
    │
    ├── Типы известны при сборке?
    │   ├── Да → Offline специализация
    │   │   ├── slangc shader.slang -D TYPE=A -o a.spv
    │   │   └── Нет overhead в рантайме
    │   │
    │   └── Нет → Runtime специализация
    │       ├── linkedProgram->specialize(args, ...)
    │       └── Гибкость, но задержка при первой компиляции
```

---

## Buffer Device Address vs Descriptor Sets

```
Как передать данные в шейдер?
    │
    ├── Глобальные данные кадра
    │   └── Push Constants или Uniform Buffer
    │       └── Быстро, ограниченный размер
    │
    ├── Большие буферы данных
    │   ├── Vulkan 1.2+ с BDA?
    │   │   ├── Да → Buffer Device Address
    │   │   │   └── Нет overhead дескрипторов
    │   │   │
    │   │   └── Нет → SSBO через Descriptor Sets
    │   │
    │   └── Много текстур (100+)
    │       └── Bindless Descriptor Indexing
```

---

## Forward vs Deferred Rendering

```
Архитектура рендеринга?
    │
    ├── Количество источников света?
    │   ├── 1-4 → Forward Rendering
    │   │   └── Проще, меньше памяти
    │   │
    │   └── 5+ → Deferred Rendering
    │       ├── G-buffer pass
    │       ├── Lighting pass
    │       └── Опционально: Clustered shading
```

---

## Когда использовать Automatic Differentiation

```
Нужно ли Automatic Differentiation?
    │
    ├── Neural SDF / NeRF
    │   └── Да — критично для обучения
    │
    ├── Автотюнинг процедурной генерации
    │   └── Полезно — градиенты по параметрам
    │
    └── Стандартный рендеринг
        └── Нет — overhead без пользы
```

---

## Чеклист выбора стратегии

**На старте проекта:**

- [ ] Оценить размер шейдерной кодобазы
- [ ] Определить потребность в модулях и generics
- [ ] Выбрать: GLSL или Slang
- [ ] Настроить build-time компиляцию через CMake

**При масштабировании:**

- [ ] Выделить core-модули с базовыми типами
- [ ] Создать интерфейсы для параметризации
- [ ] Настроить кэширование модулей
- [ ] Реализовать инкрементальную компиляцию

**Продвинутые техники:**

- [ ] Bindless rendering для множества материалов
- [ ] GPU-driven culling через compute shaders
- [ ] Mesh shaders для процедурной геометрии
- [ ] Automatic differentiation для ML-задач

---

## Краткая сводка

| Ситуация            | Рекомендация                      |
|---------------------|-----------------------------------|
| Маленький проект    | GLSL                              |
| Большая кодобаза    | Slang (модули)                    |
| Production сборка   | Offline компиляция                |
| Активная разработка | Runtime компиляция + кэш          |
| Много материалов    | Bindless + Descriptor Indexing    |
| Много света         | Deferred или Clustered            |
| Neural rendering    | Slang + Automatic Differentiation |
