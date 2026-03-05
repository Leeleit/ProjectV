# Slang: Чистый справочник для студентов

**Slang** — высокопроизводительный язык и компилятор шейдеров, разработанный для создания масштабируемых шейдерных
кодобаз. Slang предоставляет возможности современных языков программирования: модули, generics, интерфейсы,
автоматическое дифференцирование — при этом генерирует оптимизированный код для Vulkan (SPIR-V), DirectX (HLSL/DXIL),
Metal и других API.

> **Для понимания:** Slang — это как "швейцарский нож" для шейдеров. Вместо того чтобы писать один шейдер под каждую
> платформу (GLSL для Vulkan, HLSL для DirectX, MSL для Metal), ты пишешь на Slang, а компилятор сам генерирует нужный
> код. Это как иметь одного переводчика, который владеет всеми языками.

## Назначение

Slang решает главные проблемы традиционных шейдерных языков:

| Проблема GLSL/HLSL                   | Как решает Slang                           |
|--------------------------------------|--------------------------------------------|
| Нет модульной системы                | Модули с явным import/export               |
| Дублирование кода через copy-paste   | Generics и интерфейсы                      |
| Медленная инкрементальная компиляция | Кэширование скомпилированных модулей       |
| Привязка к одному API                | Кросс-компиляция в SPIR-V, HLSL, MSL, WGSL |

Slang **не предоставляет** runtime для рендеринга — это компилятор, который генерирует код шейдеров для целевого
графического API.

## Архитектура компилятора

### Модульная система

В отличие от GLSL, где каждый файл — изолированная единица компиляции, Slang использует полноценную модульную систему:

```slang
// math_utils.slang — модуль математических утилит
module MathUtils;

export float3 calculateNormal(float3 v0, float3 v1, float3 v2)
{
    return normalize(cross(v1 - v0, v2 - v0));
}

export float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}
```

```slang
// types.slang — модуль типов данных
module Types;

export struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
};
```

```slang
// main_shader.slang — основной шейдер с зависимостями
module MainShader;

import MathUtils;
import Types;

VertexOutput vsMain(Vertex input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.normal = calculateNormal(input.position, input.position, input.position);
    return output;
}
```

> **Для понимания:** Модульная система — как библиотека в университете. Ты не читаешь все книги сразу, а берёшь только
> нужные (import). Каждая книга (модуль) уже написана и скомпилирована, библиотека (Slang) просто выдаёт ссылки на них.

### Модель компиляции

```
Исходный код Slang
       ↓
    Парсер → AST
       ↓
Модульная компиляция → IR (Intermediate Representation)
       ↓
Кэшированные модули → Линковка → Целевой код (SPIR-V/HLSL/MSL)
```

**Преимущества:**

- **Кэширование**: Скомпилированные модули переиспользуются
- **Инкрементальность**: Изменение одного модуля = перекомпиляция только его
- **Кросс-компиляция**: Один исходник → multiple target formats

## Generics и интерфейсы

### Интерфейсы

Интерфейс определяет контракт — набор методов и свойств, которые должен реализовать тип:

```slang
// Интерфейс для материалов
interface IMaterial
{
    float3 albedo;
    float roughness;
    float metallic;

    float3 evaluate(float3 viewDir, float3 lightDir, float3 normal);
}
```

### Generic структуры

```slang
// Generic структура для обработки разных типов данных
generic<T : IMaterial>
struct MaterialProcessor
{
    T material;

    float3 process(float3 viewDir, float3 lightDir, float3 normal)
    {
        return material.evaluate(viewDir, lightDir, normal);
    }
};
```

> **Для понимания:** Generics — как рецепт блюда, а не само блюдо. Рецепт "Омлет" (generic) описывает, что нужно сделать
> с яйцами (параметр T), но конкретный омлет с сыром или омлет с грибами — это специализации.

### Generic функции

```slang
// Generic функция с ограничением типа
generic<T : __BuiltinFloatingPointType>
T lerp(T a, T b, T t)
{
    return a + (b - a) * t;
}

// Использование с разными типами
float v1 = lerp<float>(0.0f, 1.0f, 0.5f);
float3 v2 = lerp<float3>(float3(0), float3(1), float3(0.5));
```

## Типы данных

### Скалярные типы

```slang
bool    // Булев тип (true/false)
int     // 32-битное знаковое целое
uint    // 32-битное беззнаковое целое
float   // 32-битное число с плавающей точкой
double  // 64-битное число с плавающей точкой
half    // 16-битное число с плавающей точкой (если поддерживается)
```

### Векторные типы

```slang
float2, float3, float4    // Векторы float
int2, int3, int4          // Векторы int
uint2, uint3, uint4       // Векторы uint

// Обобщённый синтаксис
vector<float, 3>          // Эквивалент float3
```

### Матричные типы

```slang
float2x2, float2x3, float2x4
float3x2, float3x3, float3x4
float4x2, float4x3, float4x4

// Обобщённый синтаксис
matrix<float, 3, 4>      // Матрица 3x4
```

### Текстуры и сэмплеры

```slang
Texture1D<float4>         // 1D текстура
Texture2D<float4>         // 2D текстура
Texture3D<float4>         // 3D текстура
TextureCube<float4>       // Cube map

SamplerState              // Стандартный сэмплер
SamplerComparisonState    // Comparison sampler для теней
```

### Буферы

```slang
// Constant buffer (uniform buffer)
cbuffer MyConstants : register(b0)
{
    float4x4 viewProj;
    float3 cameraPos;
}

// Structured buffer (SSBO) — только чтение
StructuredBuffer<Vertex> vertices : register(t0);

// Read-write structured buffer
RWStructuredBuffer<float> output : register(u0);
```

## Шейдерные стадии и атрибуты

### Объявление шейдеров

```slang
// Вершинный шейдер
[shader("vertex")]
VertexOutput vsMain(VertexInput input) { /* ... */ }

// Фрагментный шейдер
[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target { /* ... */ }

// Compute шейдер
[shader("compute")]
[numthreads(8, 8, 1)]
void csMain(uint3 id : SV_DispatchThreadID) { /* ... */ }
```

### Compute shader атрибуты

```slang
// Размер группы потоков
[numthreads(8, 8, 1)]        // X=8, Y=8, Z=1
[numthreads(64, 1, 1)]       // Одномерная группа
[numthreads(4, 4, 4)]        // Трёхмерная группа
```

### Атрибуты оптимизации

```slang
// Принудительный инлайнинг критичных функций
[ForceInline]
float fastFunction(float x) { return x * 2.0f; }

// Запрет инлайна для больших функций
[NoInline]
float complexNoise(float3 p) { /* ... */ }

// Порог инлайна по количеству инструкций
[InlineThreshold(32)]
float mediumFunction(float x) { /* ... */ }
```

## Vulkan-специфичные атрибуты

### Привязка дескрипторов

```slang
// binding = 0, set = 0
[[vk::binding(0, 0)]]
Texture2D albedoTexture;

// binding = 1, set = 0
[[vk::binding(1, 0)]]
SamplerState linearSampler;

// binding = 2, set = 0
[[vk::binding(3, 0)]]
RWStructuredBuffer<float> outputBuffer;

// Constant buffer в set = 1
[[vk::binding(0, 1)]]
ConstantBuffer<GlobalUniforms> globals;
```

### Push constants

```slang
[[vk::push_constant]]
struct PushConstants
{
    float4x4 model;
    float4x4 viewProj;
    float3 cameraPos;
    uint frameIndex;
} pc;
```

### Специализационные константы

```slang
[[vk::constant_id(0)]]
const uint WORKGROUP_SIZE = 64;

[[vk::constant_id(1)]]
const bool ENABLE_SHADOWS = true;

[numthreads(WORKGROUP_SIZE, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    if (ENABLE_SHADOWS) {
        // Логика теней
    }
}
```

### Buffer Device Address

```slang
// Тип указателя на буфер
[[vk::buffer_reference]]
struct DeviceBuffer
{
    float data[];
};

// С явным выравниванием
[[vk::buffer_reference, vk::buffer_reference_align(16)]]
struct AlignedBuffer
{
    float4 data[];
};

// Использование в push constants
[[vk::push_constant]]
struct PC
{
    DeviceBuffer buffer;
} pc;

float readValue(uint index)
{
    return pc.buffer.data[index];
}
```

## Системные семантики

### Vertex shader

| Семантика                | Описание                      |
|--------------------------|-------------------------------|
| `POSITION`               | Позиция вершины               |
| `NORMAL`                 | Нормаль                       |
| `TEXCOORD0`, `TEXCOORD1` | Текстурные координаты         |
| `COLOR0`, `COLOR1`       | Цвет вершины                  |
| `SV_Position`            | Позиция в clip space (output) |
| `SV_InstanceID`          | ID инстанса                   |
| `SV_VertexID`            | ID вершины                    |

### Fragment shader

| Семантика                   | Описание                        |
|-----------------------------|---------------------------------|
| `SV_Target`                 | Выходной цвет (Render Target 0) |
| `SV_Target0` - `SV_Target7` | Multiple render targets         |
| `SV_Depth`                  | Выходная глубина                |

### Compute shader

| Семантика             | Описание                      |
|-----------------------|-------------------------------|
| `SV_DispatchThreadID` | Глобальный ID потока          |
| `SV_GroupID`          | ID группы потоков             |
| `SV_GroupThreadID`    | Локальный ID внутри группы    |
| `SV_GroupIndex`       | Линейный индекс внутри группы |

## Встроенные функции

### Математические

```slang
// Тригонометрия
float s = sin(x);
float c = cos(x);
float t = tan(x);

// Степени и логарифмы
float p = pow(x, y);
float e = exp(x);
float l = log(x);
float s = sqrt(x);
float r = rsqrt(x);      // 1/sqrt(x)

// Округление
float a = abs(x);
float c = ceil(x);
float f = floor(x);

// Min/Max
float m = min(a, b);
float m = max(a, b);
float c = clamp(x, minVal, maxVal);
float s = saturate(x);   // clamp(x, 0, 1)
```

### Векторные

```slang
// Скалярное произведение
float d = dot(a, b);

// Векторное произведение
float3 c = cross(a, b);

// Длина
float l = length(v);
float l2 = length_squared(v);  // Без извлечения корня

// Нормализация
float3 n = normalize(v);

// Расстояние
float d = distance(a, b);
```

### Текстурные

```slang
// Сэмплинг с фильтрацией
float4 c = texture.Sample(sampler, uv);
float4 c = texture.SampleLevel(sampler, uv, lod);
float4 c = texture.SampleGrad(sampler, uv, ddx, ddy);

// Загрузка (без фильтрации)
float4 c = texture.Load(int3(coord, lod));

// Gather — выборка 4 соседних текселей
float4 c = texture.Gather(sampler, uv);
```

### Атомарные операции

```slang
// Для RWStructuredBuffer, RWTexture
InterlockedAdd(buffer[index], value);
InterlockedAdd(buffer[index], value, oldValue);

InterlockedMin(buffer[index], value);
InterlockedMax(buffer[index], value);
InterlockedAnd(buffer[index], value);
InterlockedOr(buffer[index], value);
InterlockedXor(buffer[index], value);
InterlockedCompareExchange(buffer[index], compare, value, oldValue);
```

## Командная строка: slangc

### Основные опции

```bash
# Базовая компиляция в SPIR-V
slangc shader.slang -o shader.spv -target spirv -profile spirv_1_5

# Оптимизация
slangc shader.slang -o shader.spv -target spirv -O        # Стандартная
slangc shader.slang -o shader.spv -target spirv -O0       # Без оптимизаций (отладка)
slangc shader.slang -o shader.spv -target spirv -O3       # Максимальная

# С отладочной информацией
slangc shader.slang -o shader.spv -target spirv -g -O0
```

### Опции модулей

```bash
# Пути поиска модулей
slangc main.slang -I shaders/common -I shaders/materials -o output.spv

# Переименование entry point
slangc shader.slang -entry vsMain -stage vertex \
    -rename-entry-point vsMain main -o vertex.spv

# Измерение времени компиляции
slangc shader.slang -time-phases -o shader.spv

# Дамп промежуточного представления
slangc shader.slang -dump-ir -o shader.spv
```

### Транспиляция

```slang
# В GLSL для отладки
slangc shader.slang -target glsl -o shader.glsl

# В HLSL
slangc shader.slang -target hlsl -o shader.hlsl

# В SPIR-V ассемблер
slangc shader.slang -target spirv-asm -o shader.spvasm
```

## Slang C API

### Инициализация

```cpp
#include <slang.h>
#include <slang-com-ptr.h>

// Создание глобальной сессии
Slang::ComPtr<slang::IGlobalSession> globalSession;
SlangResult result = slang::createGlobalSession(globalSession.writeRef());

if (SLANG_FAILED(result)) {
    std::println("Failed to create Slang session");
    return;
}
```

### Компиляция шейдера

```cpp
// Настройка целевой платформы
slang::TargetDesc targetDesc{};
targetDesc.format = SLANG_SPIRV;
targetDesc.profile = globalSession->findProfile("spirv_1_5");
targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

// Настройка сессии
slang::SessionDesc sessionDesc{};
sessionDesc.targets = &targetDesc;
sessionDesc.targetCount = 1;

const char* searchPaths[] = {"shaders/"};
sessionDesc.searchPaths = searchPaths;
sessionDesc.searchPathCount = 1;

// Создание сессии
Slang::ComPtr<slang::ISession> session;
globalSession->createSession(sessionDesc, session.writeRef());

// Загрузка модуля
Slang::ComPtr<slang::IModule> module;
Slang::ComPtr<slang::IBlob> diagnostics;

module = session->loadModule("my_shader", diagnostics.writeRef());

if (!module) {
    const char* diag = diagnostics
        ? static_cast<const char*>(diagnostics->getBufferPointer())
        : "Unknown error";
    std::println("Module load error: {}", diag);
    return;
}

// Получение entry point
Slang::ComPtr<slang::IEntryPoint> entryPoint;
module->findEntryPointByName("vsMain", entryPoint.writeRef());

// Композиция программы
slang::IComponentType* components[] = {module.Get(), entryPoint.Get()};
Slang::ComPtr<slang::IComponentType> program;
session->createCompositeComponentType(components, 2, program.writeRef(), diagnostics.writeRef());

// Линковка
Slang::ComPtr<slang::IComponentType> linkedProgram;
program->link(linkedProgram.writeRef(), diagnostics.writeRef());

// Получение SPIR-V кода
Slang::ComPtr<slang::IBlob> spirvCode;
linkedProgram->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

// Использование SPIR-V
const uint32_t* code = reinterpret_cast<const uint32_t*>(spirvCode->getBufferPointer());
size_t size = spirvCode->getBufferSize();
```

### Reflection API

```cpp
// Получение layout для создания pipeline
slang::ProgramLayout* layout = linkedProgram->getLayout();

uint32_t paramCount = layout->getParameterCount();
for (uint32_t i = 0; i < paramCount; i++) {
    slang::VariableLayoutReflection* param = layout->getParameterByIndex(i);

    const char* name = param->getName();
    uint32_t binding = param->getBindingIndex();
    uint32_t set = param->getBindingSpace();
    size_t size = param->getType()->getSizeInBytes();

    std::println("Param: {} binding={}.{} size={}", name, set, binding, size);
}

// Push constants
slang::TypeLayoutReflection* pushConstants = layout->getPushConstantsByIndex(0);
if (pushConstants) {
    size_t size = pushConstants->getSizeInBytes();
    std::println("Push constants size: {}", size);
}
```

## Коды результатов

```cpp
SLANG_OK                  // Успех
SLANG_FAIL                // Общая ошибка
SLANG_E_OUT_OF_MEMORY     // Недостаточно памяти
SLANG_E_INVALID_ARG       // Неверный аргумент
SLANG_E_NOT_FOUND         // Не найдено
SLANG_E_NOT_IMPLEMENTED   // Не реализовано

// Проверка
if (SLANG_SUCCEEDED(result)) { /* успех */ }
if (SLANG_FAILED(result)) { /* ошибка */ }
```

## Целевые платформы

| Платформа   | Формат вывода  | Статус           |
|-------------|----------------|------------------|
| Vulkan 1.1  | SPIR-V 1.3     | Полная поддержка |
| Vulkan 1.2  | SPIR-V 1.4/1.5 | Полная поддержка |
| Vulkan 1.3  | SPIR-V 1.6     | Полная поддержка |
| Direct3D 12 | HLSL, DXIL     | Полная поддержка |
| Direct3D 11 | HLSL           | Полная поддержка |
| Metal       | MSL            | Экспериментально |
| WebGPU      | WGSL           | Экспериментально |
| CUDA        | C++            | Экспериментально |
| CPU         | C++            | Экспериментально |

## Глоссарий

| Термин                    | Определение                                                                              |
|---------------------------|------------------------------------------------------------------------------------------|
| **Модуль (Module)**       | Единица компиляции с экспортируемыми функциями и типами                                  |
| **Generic**               | Параметризованный тип или функция, аналог шаблонов C++                                   |
| **Интерфейс (Interface)** | Контракт, определяющий набор методов и свойств                                           |
| **SPIR-V**                | Standard Portable Intermediate Representation — бинарный формат шейдеров для Vulkan      |
| **IR**                    | Intermediate Representation — промежуточное представление между исходным кодом и целевым |
| **Descriptor**            | Объект Vulkan, представляющий ресурс (буфер, текстуру, сэмплер)                          |
| **Binding**               | Индекс ресурса внутри descriptor set                                                     |
| **Push Constants**        | Механизм передачи небольших данных в шейдер без создания descriptor set                  |
| **BDA**                   | Buffer Device Address — механизм прямого доступа к памяти GPU через указатели            |

## Ключевые функции slangc

| Задача       | Функция/опция                                    |
|--------------|--------------------------------------------------|
| Компиляция   | `slangc input.slang -o output.spv -target spirv` |
| Entry point  | `-entry vsMain -stage vertex`                    |
| Оптимизация  | `-O`, `-O3`                                      |
| Пути модулей | `-I shaders/common`                              |
| Отладка      | `-g -O0`                                         |
| Транспиляция | `-target glsl` или `-target hlsl`                |
