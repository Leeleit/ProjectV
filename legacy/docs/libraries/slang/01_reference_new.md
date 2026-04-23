## Обзор Slang

<!-- anchor: 00_overview -->

**🟢 Уровень 1: Начинающий** — Введение в шейдерный язык Slang, его возможности и области применения.

Slang — шейдерный язык и компилятор, разработанный для создания и поддержки больших шейдерных кодобаз в модульном и
расширяемом стиле. Slang предоставляет современные возможности языков программирования для шейдеров: модули, generics,
автоматическое дифференцирование, при этом сохраняя максимальную производительность на современных GPU и графических
API.

---

## Ключевые возможности

### Модульная компиляция

В отличие от традиционных шейдерных языков, Slang использует систему модулей, позволяющую компилировать части кода
независимо и кэшировать результат. Это критически важно для больших шейдерных кодобаз.

### Generics и интерфейсы

Slang поддерживает обобщённое программирование (generics) и интерфейсы, аналогичные шаблонам и концептам C++. Это
позволяет создавать параметризованные шейдеры без дублирования кода.

### Автоматическое дифференцирование

Встроенная поддержка автоматического дифференцирования (automatic differentiation) для задач машинного обучения,
нейронной графики и оптимизации параметров.

### Кроссплатформенная компиляция

Один исходный код компилируется в различные целевые форматы: SPIR-V (Vulkan), HLSL/DXIL (Direct3D), MSL (Metal), WGSL (
WebGPU), CUDA, C++ (CPU).

---

## Поддерживаемые платформы

| Платформа   | Архитектуры           | Статус           |
|-------------|-----------------------|------------------|
| Windows     | x86_64, aarch64       | Полная поддержка |
| Linux       | x86_64, aarch64       | Полная поддержка |
| macOS       | x86_64, Apple Silicon | Полная поддержка |
| WebAssembly | —                     | Экспериментально |

---

## Поддерживаемые цели компиляции

| Цель        | Формат вывода | Статус           |
|-------------|---------------|------------------|
| Vulkan      | SPIR-V, GLSL  | Полная поддержка |
| Direct3D 12 | HLSL, DXIL    | Полная поддержка |
| Direct3D 11 | HLSL          | Полная поддержка |
| Metal       | MSL           | Экспериментально |
| WebGPU      | WGSL          | Экспериментально |
| CUDA        | C++ (compute) | Полная поддержка |
| CPU         | C++           | Экспериментально |

---

## Требования

### Минимальные

- **C++11** или новее для использования Slang API
- **Vulkan SDK 1.3.296+** (Slang включён в SDK начиная с этой версии)
- **Совместимый GPU** с поддержкой Vulkan 1.0+ для базовых функций

### Для расширенных возможностей

- **Vulkan 1.2+** для Buffer Device Address, Descriptor Indexing
- **Vulkan 1.3+** для Mesh Shaders, Ray Tracing
- **C++17/C++20** для современных паттернов интеграции

---

## Содержание документации

### Базовая документация

| Раздел                                              | Описание                               | Уровень |
|-----------------------------------------------------|----------------------------------------|---------|
| [01. Быстрый старт](01_quickstart.md)               | Компиляция первого шейдера Slang       | 🟢      |
| [02. Установка](02_installation.md)                 | Установка slangc, сборка из исходников | 🟢      |
| [03. Основные понятия](03_concepts.md)              | Модули, generics, интерфейсы           | 🟡      |
| [04. Справочник языка](04_language-reference.md)    | Синтаксис, атрибуты, типы данных       | 🟡      |
| [05. Справочник API](05_api-reference.md)           | slangc CLI, Slang C API                | 🟡      |
| [06. Интеграция с Vulkan](06_vulkan-integration.md) | SPIR-V, bindings, push constants       | 🟡      |
| [07. Производительность](07_performance.md)         | Оптимизация компиляции                 | 🟡      |
| [08. Решение проблем](08_troubleshooting.md)        | Диагностика ошибок                     | 🟡      |
| [09. Глоссарий](09_glossary.md)                     | Термины и определения                  | 🟢      |
| [10. Сценарии использования](10_use-cases.md)       | Практические паттерны                  | 🔴      |
| [11. Decision Trees](11_decision-trees.md)          | Выбор стратегий                        | 🟡      |

### Интеграция в ProjectV

| Раздел                                                | Описание                                  | Уровень |
|-------------------------------------------------------|-------------------------------------------|---------|
| [12. Обзор интеграции](12_projectv-overview.md)       | Архитектура шейдерной системы             | 🟡      |
| [13. Интеграция в проект](13_projectv-integration.md) | CMake, SlangShaderManager                 | 🟡      |
| [14. Паттерны ProjectV](14_projectv-patterns.md)      | Generic chunk, GPU culling, SVO traversal | 🔴      |
| [15. Примеры кода](15_projectv-examples.md)           | Готовые примеры шейдеров                  | 🟡      |

---

## Рекомендуемый порядок чтения

1. **01. Быстрый старт** — скомпилировать первый шейдер
2. **02. Установка** — настроить окружение
3. **03. Основные понятия** — понять архитектуру Slang
4. **05. Справочник API** — изучить команды и API
5. **06. Интеграция с Vulkan** — подключить к графическому API

Далее — по необходимости в зависимости от задач.

---

## Основные понятия Slang

<!-- anchor: 03_concepts -->

**🟢 Уровень 1: Начинающий** — Фундаментальные концепции языка Slang: модули, generics, интерфейсы, модель компиляции.

---

## Архитектура Slang

### Модульная система

В отличие от GLSL, где каждый файл компилируется независимо, Slang использует систему модулей:

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
// main_shader.slang — основной шейдер, импортирующий зависимости
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

**Преимущества модульной системы:**

- **Отдельная компиляция**: Модули компилируются один раз и кэшируются
- **Инкрементальная компиляция**: Изменения в одном модуле не требуют перекомпиляции всей кодобазы
- **Повторное использование**: Модули могут использоваться в разных шейдерах
- **Явные зависимости**: `import` чётко определяет зависимости между модулями

---

## Generics и интерфейсы

Slang поддерживает обобщённое программирование (generics) и интерфейсы, аналогичные C++ шаблонам и концептам.

### Интерфейсы

Интерфейс определяет контракт, который должен реализовать тип:

```slang
// Интерфейс для материалов
interface IMaterial
{
    float3 albedo;
    float roughness;
    float metallic;

    float3 evaluate(float3 viewDir, float3 lightDir, float3 normal);
};
```

### Реализация интерфейса

```slang
// Конкретная реализация материала
struct PBRMaterial : IMaterial
{
    float3 albedo;
    float roughness;
    float metallic;
    float3 emissive;

    float3 evaluate(float3 viewDir, float3 lightDir, float3 normal)
    {
        // PBR реализация
        return calculatePBR(viewDir, lightDir, normal, albedo, roughness, metallic);
    }
};
```

### Generic структуры

```slang
// Generic структура для работы с разными типами
generic<T : IMaterial>
struct MaterialProcessor
{
    T material;

    float3 process(float3 viewDir, float3 lightDir, float3 normal)
    {
        return material.evaluate(viewDir, lightDir, normal);
    }
};

// Использование
MaterialProcessor<PBRMaterial> processor;
float3 result = processor.process(viewDir, lightDir, normal);
```

### Generic функции

```slang
// Generic функция с ограничением
generic<T : __BuiltinFloatingPointType>
T lerp(T a, T b, T t)
{
    return a + (b - a) * t;
}

// Использование
float v1 = lerp<float>(0.0, 1.0, 0.5);
float3 v2 = lerp<float3>(float3(0), float3(1), float3(0.5));
```

**Преимущества generics:**

- **Параметризация шейдеров**: Один шейдер для разных типов данных
- **Компиляция времени**: Специализация под конкретные типы
- **Безопасность типов**: Проверка типов во время компиляции
- **Отсутствие дублирования**: Код пишется один раз

---

## Автоматическое дифференцирование

Одна из мощных возможностей Slang — автоматическое дифференцирование для ML и оптимизации.

### Объявление дифференцируемой функции

```slang
// Функция с автоматическим дифференцированием
[Differentiable]
float complexFunction(float x, float y)
{
    if (x > 0) {
        return sin(x) * cos(y);
    } else {
        return exp(x) * log(y + 1.0);
    }
}
```

### Вычисление градиентов

```slang
// Прямой режим (forward mode) — эффективен при малом числе входов
float3 gradient = fwd_diff(complexFunction)(
    DifferentialPair<float>(x, float3(1, 0, 0)),
    DifferentialPair<float>(y, float3(0, 1, 0))
).d;

// Обратный режим (backward mode) — эффективен при многих входах
bwd_diff(complexFunction)(x, y, 1.0f);
```

### Применение

- **Нейронная графика**: Оптимизация параметров рендеринга через градиенты
- **Авто-тюнинг**: Автоматическая настройка шейдерных параметров
- **Процедурная генерация**: Градиент-based генерация контента
- **Дифференцируемый рендеринг**: Оптимизация геометрии и материалов

---

## Модель компиляции

### Традиционная модель (GLSL)

```
Исходный код GLSL → Компилятор GLSL → SPIR-V → Загрузка в GPU
```

При любом изменении — полная перекомпиляция.

### Модель Slang

```
Исходный код Slang → Парсер → AST → Модульная компиляция → IR
       ↓
Кэшированные модули → Линковка → Целевая генерация → SPIR-V/HLSL/MSL
       ↓
Загрузка в GPU с оптимизациями
```

**Ключевые преимущества:**

1. **Кэширование модулей**: Повторное использование скомпилированных модулей
2. **Инкрементальная компиляция**: Только изменённые модули перекомпилируются
3. **Кросскомпиляция**: Один исходный код → несколько целевых форматов
4. **Оптимизации на уровне IR**: Межмодульные оптимизации

---

## Ключевые отличия от GLSL/HLSL

### Сравнение возможностей

| Особенность            | GLSL         | HLSL                 | Slang                     |
|------------------------|--------------|----------------------|---------------------------|
| Модули                 | Нет          | Ограниченные         | Полная поддержка          |
| Generics               | Нет          | Ограниченные шаблоны | Полная поддержка          |
| Интерфейсы             | Нет          | Нет                  | Полная поддержка          |
| Авт. дифференцирование | Нет          | Нет                  | Поддержка                 |
| Указатели              | Ограниченные | Ограниченные         | Полная поддержка (SPIR-V) |

### Поддержка современных GPU возможностей

| Возможность        | GLSL         | Slang                             |
|--------------------|--------------|-----------------------------------|
| Ray Tracing        | Экстеншены   | Нативная поддержка                |
| Mesh Shading       | Экстеншены   | Нативная поддержка                |
| Compute Shaders    | Базовая      | Расширенная (указатели, generics) |
| Bindless Texturing | Ограниченная | Полная поддержка                  |

---

## Типы данных

### Скалярные типы

```slang
bool    // булев тип
int     // 32-битное знаковое целое
uint    // 32-битное беззнаковое целое
float   // 32-битное float
double  // 64-битное float (если поддерживается)
half    // 16-битное float (если поддерживается)
```

### Векторные типы

```slang
float2, float3, float4    // векторы float
int2, int3, int4          // векторы int
uint2, uint3, uint4       // векторы uint
bool2, bool3, bool4       // векторы bool

// Обобщённый синтаксис
vector<float, 3>          // эквивалент float3
vector<int, 2>            // эквивалент int2
```

### Матричные типы

```slang
float2x2, float2x3, float2x4
float3x2, float3x3, float3x4
float4x2, float4x3, float4x4

// Обобщённый синтаксис
matrix<float, 3, 4>       // матрица 3x4
```

### Текстуры и сэмплеры

```slang
Texture1D<float4>         // 1D текстура
Texture2D<float4>         // 2D текстура
Texture3D<float4>         // 3D текстура
TextureCube<float4>       // Cube map

SamplerState              // сэмплер
SamplerComparisonState    // comparison sampler
```

### Буферы

```slang
// Constant buffer (uniform buffer)
cbuffer MyConstants : register(b0)
{
    float4x4 viewProj;
    float3 cameraPos;
}

// Structured buffer (SSBO)
StructuredBuffer<Vertex> vertices : register(t0);

// Read-write structured buffer
RWStructuredBuffer<float> output : register(u0);
```

---

## Система атрибутов

Slang использует атрибуты для указания метаданных компилятору:

```slang
// Compute shader — размер группы потоков
[numthreads(8, 8, 1)]
void csMain(uint3 id : SV_DispatchThreadID) { }

// Принудительный инлайнинг
[ForceInline]
float fastFunction(float x) { return x * 2.0; }

// Запрет инлайнинга
[NoInline]
float complexFunction(float x) { /* ... */ }

// Оптимизация для кэша
[OptimizeForCache]
struct PackedData { /* ... */ };
```

---

## Пространства имён

Slang поддерживает пространства имён для организации кода:

```slang
namespace Math
{
    float PI = 3.14159265;

    float toRadians(float degrees)
    {
        return degrees * PI / 180.0;
    }
}

// Использование
float angle = Math::toRadians(90.0);
```

---

## Специализация модулей

Generics можно специализировать явно для оптимизации:

```slang
// Базовый generic
generic<T>
struct Processor
{
    T process(T input) { return input * 2.0; }
};

// Явная специализация для float
specialized<float>
struct Processor<float>
{
    float process(float input)
    {
        // Оптимизированная версия для float
        return fma(input, 2.0, 0.0);
    }
};

---

## Справочник языка Slang

<!-- anchor: 04_language-reference -->

**🟡 Уровень 2: Средний** — Синтаксис, атрибуты, ключевые слова и конструкции языка Slang.

---

## Структура модуля

### Объявление модуля

```slang
// Объявление модуля (обычно совпадает с именем файла)
module MyModule;

// Экспорт функций и типов
export float myFunction(float x) { return x * 2.0; }

export struct MyStruct { float3 value; };
```

### Импорт модулей

```slang
// Импорт модуля по имени
import MathUtils;
import Types;

// Использование импортированных сущностей
float3 normal = MathUtils::calculateNormal(p0, p1, p2);
Types::Vertex v;
```

---

## Функции

### Объявление функций

```slang
// Простая функция
float square(float x)
{
    return x * x;
}

// Функция с несколькими параметрами
float3 add(float3 a, float3 b)
{
    return a + b;
}

// Функция с параметрами по умолчанию
float power(float x, float exp = 2.0)
{
    return pow(x, exp);
}
```

### Generic функции

```slang
// Generic функция
generic<T>
T maxValue(T a, T b)
{
    return a > b ? a : b;
}

// С ограничением типа
generic<T : __BuiltinFloatingPointType>
T sqrtSafe(T x)
{
    return x >= 0 ? sqrt(x) : T(0);
}
```

### Перегрузка функций

```slang
float process(float x) { return x * 2.0; }
float2 process(float2 x) { return x * 2.0; }
float3 process(float3 x) { return x * 2.0; }
```

---

## Структуры

### Объявление структур

```slang
// Простая структура
struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

// Структура с методами
struct Transform
{
    float4x4 matrix;

    float3 transformPoint(float3 p)
    {
        return mul(matrix, float4(p, 1.0)).xyz;
    }

    float3 transformDirection(float3 d)
    {
        return mul(matrix, float4(d, 0.0)).xyz;
    }
};
```

### Структуры с семантикой

```slang
// Вершинный ввод
struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

// Вершинный вывод
struct VertexOutput
{
    float4 position : SV_Position;    // Обязательная семантика для позиции
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};
```

### Generic структуры

```slang
generic<T>
struct Buffer
{
    T* data;
    uint size;

    T get(uint index)
    {
        return data[index];
    }

    void set(uint index, T value)
    {
        data[index] = value;
    }
};
```

---

## Интерфейсы

### Объявление интерфейса

```slang
// Интерфейс с свойствами и методами
interface IShape
{
    float3 getCenter();
    float getRadius();
    bool contains(float3 point);
};
```

### Реализация интерфейса

```slang
struct Sphere : IShape
{
    float3 center;
    float radius;

    float3 getCenter() { return center; }
    float getRadius() { return radius; }

    bool contains(float3 point)
    {
        return length(point - center) <= radius;
    }
};
```

### Использование интерфейса

```slang
// Функция, принимающая любой тип, реализующий интерфейс
generic<T : IShape>
bool isInShape(T shape, float3 point)
{
    return shape.contains(point);
}
```

---

## Атрибуты

### Шейдерные стадии

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
// Размер группы потоков (x, y, z)
[numthreads(32, 32, 1)]
void csMain(uint3 id : SV_DispatchThreadID) { }

// Альтернативный синтаксис
[numthreads(X=32, Y=32, Z=1)]
void csMain(uint3 id : SV_DispatchThreadID) { }
```

### Атрибуты оптимизации

```slang
// Принудительный инлайнинг
[ForceInline]
float fastFunction(float x) { return x * 2.0; }

// Запрет инлайнинга
[NoInline]
float complexFunction(float x) { /* ... */ }

// Порог инлайнинга (количество инструкций)
[InlineThreshold(32)]
float mediumFunction(float x) { /* ... */ }
```

### Атрибуты дифференцирования

```slang
// Дифференцируемая функция
[Differentiable]
float myFunction(float x, float y)
{
    return sin(x) * cos(y);
}

// Дифференцируемый метод
[Differentiable]
float3 MyStruct::compute(float3 input) { /* ... */ }
```

---

## Vulkan-специфичные атрибуты

### Привязка дескрипторов

```slang
// Указание binding и set
[[vk::binding(0, 0)]]
Texture2D albedoTexture;

[[vk::binding(1, 0)]]
SamplerState linearSampler;

[[vk::binding(2, 0)]]
RWStructuredBuffer<float> outputBuffer;

// Constant buffer / Uniform buffer
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
    uint     frameIndex;
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
        // Тень включена
    }
}
```

### Buffer Device Address

```slang
// Объявление типа указателя на буфер
[[vk::buffer_reference]]
struct DeviceBuffer
{
    float data[];
};

// С указанием выравнивания
[[vk::buffer_reference, vk::buffer_reference_align(16)]]
struct AlignedBuffer
{
    float4 data[];
};

// Использование
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

---

## Семантика

### Системные семантики

| Семантика             | Описание                 | Использование      |
|-----------------------|--------------------------|--------------------|
| `SV_Position`         | Позиция в clip space     | Вершинный вывод    |
| `SV_Target`           | Выходной цвет            | Фрагментный шейдер |
| `SV_Depth`            | Глубина                  | Фрагментный шейдер |
| `SV_DispatchThreadID` | Глобальный ID потока     | Compute shader     |
| `SV_GroupID`          | ID группы                | Compute shader     |
| `SV_GroupThreadID`    | Локальный ID в группе    | Compute shader     |
| `SV_GroupIndex`       | Линейный индекс в группе | Compute shader     |
| `SV_InstanceID`       | ID инстанса              | Вершинный шейдер   |
| `SV_VertexID`         | ID вершины               | Вершинный шейдер   |
| `SV_PrimitiveID`      | ID примитива             | Geometry shader    |

### Пользовательская семантика

```slang
struct VertexInput
{
    float3 position : POSITION;      // Пользовательская семантика
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float4 tangent  : TANGENT;
};
```

---

## Ключевые слова

### Объявления

| Ключевое слово | Описание              |
|----------------|-----------------------|
| `module`       | Объявление модуля     |
| `import`       | Импорт модуля         |
| `export`       | Экспорт сущности      |
| `generic`      | Объявление generic    |
| `specialized`  | Специализация generic |
| `interface`    | Объявление интерфейса |
| `struct`       | Объявление структуры  |
| `typedef`      | Определение типа      |
| `namespace`    | Пространство имён     |
| `cbuffer`      | Constant buffer       |
| `tbuffer`      | Texture buffer        |

### Модификаторы

| Ключевое слово    | Описание                    |
|-------------------|-----------------------------|
| `static`          | Статический член            |
| `const`           | Константа                   |
| `in`              | Входной параметр            |
| `out`             | Выходной параметр           |
| `inout`           | Входной и выходной параметр |
| `uniform`         | Uniform переменная          |
| `volatile`        | Volatile переменная         |
| `shared`          | Group shared память         |
| `nointerpolation` | Без интерполяции            |
| `row_major`       | Row-major матрица           |
| `column_major`    | Column-major матрица        |

### Управляющие конструкции

| Ключевое слово              | Описание           |
|-----------------------------|--------------------|
| `if`, `else`                | Условие            |
| `switch`, `case`, `default` | Switch             |
| `for`                       | Цикл for           |
| `while`                     | Цикл while         |
| `do`                        | Цикл do-while      |
| `break`                     | Выход из цикла     |
| `continue`                  | Следующая итерация |
| `return`                    | Возврат из функции |
| `discard`                   | Отбросить фрагмент |

---

## Операторы

### Арифметические

```slang
// Скалярные и векторные операции
float a = 1.0 + 2.0;    // Сложение
float b = 3.0 - 1.0;    // Вычитание
float c = 2.0 * 3.0;    // Умножение
float d = 6.0 / 2.0;    // Деление
float e = 5.0 % 3.0;    // Остаток от деления

// Векторные операции
float3 v = float3(1, 2, 3) + float3(4, 5, 6);  // Поэлементное сложение
float3 w = float3(1, 2, 3) * 2.0;              // Умножение на скаляр
```

### Матричные

```slang
float4x4 m1, m2;
float4 v;

float4x4 m3 = m1 * m2;     // Матричное умножение
float4 r = mul(m1, v);     // Умножение матрицы на вектор
float4x4 m4 = m1 + m2;     // Поэлементное сложение
```

### Сравнения

```slang
bool eq = (a == b);    // Равенство
bool ne = (a != b);    // Неравенство
bool lt = (a < b);     // Меньше
bool le = (a <= b);    // Меньше или равно
bool gt = (a > b);     // Больше
bool ge = (a >= b);    // Больше или равно
```

### Логические

```slang
bool and = (a && b);   // Логическое И
bool or = (a || b);    // Логическое ИЛИ
bool not = !a;         // Логическое НЕ

// Векторные логические операции
bool3 vAnd = bool3(true, false, true) && bool3(true, true, false);
```

### Побитовые

```slang
uint and = a & b;      // Побитовое И
uint or = a | b;       // Побитовое ИЛИ
uint xor = a ^ b;      // Побитовое XOR
uint not = ~a;         // Побитовое НЕ
uint left = a << 2;    // Сдвиг влево
uint right = a >> 2;   // Сдвиг вправо
```

---

## Встроенные функции

### Математические

```slang
// Тригонометрия
float s = sin(x);
float c = cos(x);
float t = tan(x);
float a = asin(x);
float b = acos(x);
float d = atan2(y, x);

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
float r = round(x);
float t = trunc(x);
float fr = frac(x);      // Дробная часть

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
float l2 = length_squared(v);  // Длина в квадрате

// Нормализация
float3 n = normalize(v);

// Расстояние
float d = distance(a, b);

// Рефлект
float3 r = reflect(incident, normal);

// Преломление
float3 r = refract(incident, normal, eta);
```

### Матричные

```slang
// Определитель
float d = determinant(m);

// Транспонирование
float4x4 t = transpose(m);

// Обратная матрица
float4x4 i = inverse(m);
```

### Текстурные

```slang
// Сэмплинг
float4 c = texture.Sample(sampler, uv);
float4 c = texture.SampleLevel(sampler, uv, lod);
float4 c = texture.SampleGrad(sampler, uv, ddx, ddy);

// Загрузка (без фильтрации)
float4 c = texture.Load(int3(coord, lod));

// Gather
float4 c = texture.Gather(sampler, uv);  // 4 сэмпла для PCF
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
InterlockedExchange(buffer[index], value, oldValue);
```

---

## Препроцессор

```slang
// Условная компиляция
#ifdef DEBUG
    // Код для отладки
#endif

#ifndef RELEASE
    // Код не для релиза
#endif

#if defined(FOO) && defined(BAR)
    // Код если оба определены
#endif

// Определение макросов
#define PI 3.14159265
#define MAX_COUNT 100

// Макросы с параметрами
#define SQUARE(x) ((x) * (x))

// Включение файлов
#include "common.slang"
#include <stdlib.slang>

---

## Справочник API Slang

<!-- anchor: 05_api-reference -->

**🟡 Уровень 2: Средний** — Полный справочник команд `slangc`, Slang C API и Reflection API.

---

## Командная строка: slangc

### Базовый синтаксис

```bash
slangc [опции] <входные_файлы...>
```

Slang использует одинарные дефисы для многосимвольных опций: `-help`, `-target spirv`, `-dump-ir`.

---

## Основные опции

### Выходные параметры

| Опция                | Описание              | Пример               |
|----------------------|-----------------------|----------------------|
| `-o <файл>`          | Выходной файл         | `-o shader.spv`      |
| `-target <формат>`   | Целевой формат вывода | `-target spirv`      |
| `-profile <профиль>` | Профиль совместимости | `-profile spirv_1_5` |

### Точки входа и стадии

| Опция             | Описание         | Пример          |
|-------------------|------------------|-----------------|
| `-entry <имя>`    | Имя точки входа  | `-entry vsMain` |
| `-stage <стадия>` | Шейдерная стадия | `-stage vertex` |

**Допустимые значения `-stage`:**

| Значение        | Шейдерная стадия            |
|-----------------|-----------------------------|
| `vertex`        | Вершинный шейдер            |
| `fragment`      | Фрагментный шейдер          |
| `compute`       | Вычислительный шейдер       |
| `geometry`      | Геометрический шейдер       |
| `hull`          | Hull / Tessellation Control |
| `domain`        | Domain / Tessellation Eval  |
| `amplification` | Task / Amplification shader |
| `mesh`          | Mesh shader                 |
| `raygen`        | Ray generation              |
| `closesthit`    | Closest hit                 |
| `anyhit`        | Any hit                     |
| `miss`          | Miss shader                 |
| `callable`      | Callable shader             |
| `intersection`  | Intersection shader         |

### Цели компиляции (-target)

| Значение    | Описание                   |
|-------------|----------------------------|
| `spirv`     | SPIR-V бинарник для Vulkan |
| `spirv-asm` | SPIR-V текстовый ассемблер |
| `glsl`      | Транспиляция в GLSL        |
| `hlsl`      | Транспиляция в HLSL        |
| `dxil`      | DirectX IL (для D3D12)     |
| `metal`     | Metal Shading Language     |
| `cuda`      | CUDA C++                   |
| `cpp`       | CPU C++                    |
| `ptx`       | NVIDIA PTX                 |

### Оптимизация

| Опция | Описание                      |
|-------|-------------------------------|
| `-O`  | Стандартная оптимизация       |
| `-O0` | Без оптимизаций (для отладки) |
| `-O1` | Минимальная оптимизация       |
| `-O2` | Умеренная оптимизация         |
| `-O3` | Максимальная оптимизация      |

---

## Vulkan-специфичные параметры

### Смещения привязок дескрипторов (binding shifts)

```bash
-fvk-b-shift <N> <set>    # cbuffer: сдвинуть на N в set
-fvk-t-shift <N> <set>    # SRV (текстуры): сдвинуть на N в set
-fvk-s-shift <N> <set>    # сэмплеры: сдвинуть на N в set
-fvk-u-shift <N> <set>    # UAV: сдвинуть на N в set
```

Пример:

```bash
slangc shader.slang -target spirv \
    -fvk-b-shift 0 0 \
    -fvk-t-shift 0 1 \
    -fvk-u-shift 0 2
```

### Матричный порядок

```bash
-enable-slang-matrix-layout-row-major    # Row-major матрицы
-enable-slang-matrix-layout-column-major # Column-major матрицы
```

### Профили SPIR-V

```bash
-profile spirv_1_3    # Vulkan 1.1
-profile spirv_1_4    # Vulkan 1.2 (частично)
-profile spirv_1_5    # Vulkan 1.2+
-profile spirv_1_6    # Vulkan 1.3+
```

### Дополнительные опции

```bash
-fvk-use-entrypoint-name        # Сохранять имена entry points
-emit-spirv-directly            # Генерировать SPIR-V напрямую (без GLSL промежутка)
```

---

## Отладка и диагностика

```bash
-g                # Отладочная информация (имена переменных, номера строк)
-line-directive   # Директивы #line в выводе
-dump-ir          # Дамп промежуточного представления
-E                # Только препроцессор
--no-warnings     # Отключить предупреждения
-Werror           # Трактовать предупреждения как ошибки
-time-phases      # Измерение времени каждой фазы компиляции
-verbose          # Детальный вывод
```

---

## Примеры команд

### Базовая компиляция

```bash
# Вершинный шейдер
slangc shaders/triangle.slang \
    -entry vsMain \
    -stage vertex \
    -o build/shaders/triangle_vert.spv \
    -target spirv \
    -profile spirv_1_5 \
    -O

# Compute шейдер
slangc shaders/compute.slang \
    -entry csMain \
    -stage compute \
    -o build/shaders/compute.spv \
    -target spirv \
    -profile spirv_1_6 \
    -O3
```

### Транспиляция

```bash
# В GLSL для отладки
slangc shader.slang -target glsl -o shader_debug.glsl

# В SPIR-V ассемблер
slangc shader.slang -target spirv-asm -o shader.spvasm
```

### С модулями

```bash
# Указать путь поиска модулей
slangc main_shader.slang \
    -I shaders/common \
    -I shaders/materials \
    -entry vsMain \
    -stage vertex \
    -o output.spv \
    -target spirv
```

---

## Slang C API

Для динамической компиляции шейдеров во время выполнения приложения.

### Заголовки

```cpp
#include <slang.h>           // Основной API
#include <slang-com-ptr.h>   // ComPtr helper
```

### Инициализация

```cpp
// Создание глобальной сессии компиляции
Slang::ComPtr<slang::IGlobalSession> globalSession;
SlangResult result = slang::createGlobalSession(globalSession.writeRef());

if (SLANG_FAILED(result)) {
    // Обработка ошибки
}
```

### Создание сессии

```cpp
// Описание цели компиляции
slang::TargetDesc targetDesc{};
targetDesc.format = SLANG_SPIRV;
targetDesc.profile = globalSession->findProfile("spirv_1_5");
targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

// Описание сессии
slang::SessionDesc sessionDesc{};
sessionDesc.targets = &targetDesc;
sessionDesc.targetCount = 1;

// Пути поиска модулей
const char* searchPaths[] = {
    "shaders/",
    "shaders/common/"
};
sessionDesc.searchPaths = searchPaths;
sessionDesc.searchPathCount = 2;

// Создание сессии
Slang::ComPtr<slang::ISession> session;
globalSession->createSession(sessionDesc, session.writeRef());
```

### Загрузка модуля

```cpp
Slang::ComPtr<slang::IModule> module;
Slang::ComPtr<slang::IBlob> diagnosticsBlob;

module = session->loadModule("my_shader", diagnosticsBlob.writeRef());

if (!module) {
    const char* diagnostics = diagnosticsBlob
        ? static_cast<const char*>(diagnosticsBlob->getBufferPointer())
        : "(нет диагностики)";
    // Обработка ошибки
}
```

### Компиляция шейдера

```cpp
// Получение точки входа
Slang::ComPtr<slang::IEntryPoint> entryPoint;
module->findEntryPointByName("vsMain", entryPoint.writeRef());

// Создание программы
slang::IComponentType* components[] = { module, entryPoint };
Slang::ComPtr<slang::IComponentType> program;
Slang::ComPtr<slang::IBlob> diagnosticsBlob;

session->createCompositeComponentType(
    components,
    2,
    program.writeRef(),
    diagnosticsBlob.writeRef()
);

// Линковка
Slang::ComPtr<slang::IComponentType> linkedProgram;
program->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());

// Получение SPIR-V кода
Slang::ComPtr<slang::IBlob> spirvCode;
linkedProgram->getEntryPointCode(
    0,  // Индекс entry point
    0,  // Индекс цели
    spirvCode.writeRef(),
    diagnosticsBlob.writeRef()
);
```

### Использование SPIR-V

```cpp
// Создание VkShaderModule
VkShaderModuleCreateInfo createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
createInfo.codeSize = spirvCode->getBufferSize();
createInfo.pCode = reinterpret_cast<const uint32_t*>(spirvCode->getBufferPointer());

VkShaderModule shaderModule;
VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
```

---

## Reflection API

Получение информации о структуре шейдера для автоматического создания pipeline layout.

### Получение layout

```cpp
slang::ProgramLayout* layout = linkedProgram->getLayout();
```

### Перебор параметров

```cpp
uint32_t paramCount = layout->getParameterCount();
for (uint32_t i = 0; i < paramCount; i++) {
    slang::VariableLayoutReflection* param = layout->getParameterByIndex(i);

    const char* name = param->getName();

    // Получение binding и set
    uint32_t binding = param->getBindingIndex();
    uint32_t set = param->getBindingSpace();

    // Размер в байтах
    size_t size = param->getType()->getSizeInBytes();

    // Использование для создания VkDescriptorSetLayout
}
```

### Push constants

```cpp
slang::TypeLayoutReflection* pushConstants = layout->getPushConstantsByIndex(0);
if (pushConstants) {
    size_t size = pushConstants->getSizeInBytes();
    // Создание VkPushConstantRange
}
```

### Специализация

```cpp
// Специализация generic параметров
slang::SpecializationArg args[1];
args[0].kind = slang::SpecializationArg::Kind::Type;
args[0].type = session->getTypeFromString("MyType");

Slang::ComPtr<slang::IComponentType> specialized;
linkedProgram->specialize(
    args,
    1,
    specialized.writeRef(),
    nullptr
);
```

---

## Коды ошибок

### SlangResult

```cpp
SLANG_OK                 // Успех
SLANG_FAIL               // Общая ошибка
SLANG_E_OUT_OF_MEMORY    // Недостаточно памяти
SLANG_E_INVALID_ARG      // Неверный аргумент
SLANG_E_NOT_FOUND        // Не найдено
SLANG_E_NOT_IMPLEMENTED  // Не реализовано

// Проверка результата
if (SLANG_SUCCEEDED(result)) { /* успех */ }
if (SLANG_FAILED(result)) { /* ошибка */ }
```

---

## CMake интеграция

### Вспомогательная функция

```cmake
# Компиляция шейдеров Slang
function(compile_slang_shader TARGET SOURCE ENTRY STAGE OUTPUT)
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND slangc
            ${SOURCE}
            -o ${OUTPUT}
            -target spirv
            -profile spirv_1_5
            -entry ${ENTRY}
            -stage ${STAGE}
            -O
        DEPENDS ${SOURCE}
        COMMENT "Slang: ${SOURCE} -> ${OUTPUT}"
        VERBATIM
    )

    add_custom_target(compile_${ENTRY}
        DEPENDS ${OUTPUT}
    )

    add_dependencies(${TARGET} compile_${ENTRY})
endfunction()

# Использование
compile_slang_shader(MyApp
    "shaders/triangle.slang"
    "vsMain"
    "vertex"
    "${CMAKE_BINARY_DIR}/shaders/triangle_vert.spv"
)
```

### Пакетная компиляция

```cmake
function(target_slang_shaders TARGET)
    cmake_parse_arguments(ARG "" "PROFILE" "SOURCES" ${ARGN})

    set(ARG_PROFILE ${ARG_PROFILE} spirv_1_5)

    foreach(SRC ${ARG_SOURCES})
        get_filename_component(NAME_WE ${SRC} NAME_WE)

        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/shaders/${NAME_WE}.spv
            COMMAND slangc ${SRC}
                -o ${CMAKE_BINARY_DIR}/shaders/${NAME_WE}.spv
                -target spirv
                -profile ${ARG_PROFILE}
                -O
            DEPENDS ${SRC}
            COMMENT "Slang: ${SRC}"
        )

        target_sources(${TARGET} PRIVATE
            ${CMAKE_BINARY_DIR}/shaders/${NAME_WE}.spv
        )
    endforeach()
endfunction()

# Использование
target_slang_shaders(MyApp
    PROFILE spirv_1_5
    SOURCES
        shaders/triangle.slang
        shaders/compute.slang
)

---

## Глоссарий Slang

<!-- anchor: 09_glossary -->

**🟢 Уровень 1: Начинающий** — Термины и понятия языка Slang.

---

## Основные термины

### Модуль (Module)

Базовая единица организации кода в Slang. Модуль содержит определения типов, функций и констант, которые могут быть
импортированы другими модулями.

```slang
module MathUtils;

export float square(float x) { return x * x; }
```

### Generic (Обобщённый тип)

Параметризованный тип или функция, работающая с разными типами данных. Аналогичен шаблонам в C++.

```slang
generic<T>
struct Buffer
{
    T* data;
    uint size;
};
```

### Интерфейс (Interface)

Контракт, определяющий набор методов и свойств, которые должен реализовать тип. Аналогичен интерфейсам в Java или
концептам в C++20.

```slang
interface IMaterial
{
    float3 albedo;
    float roughness;
    float3 evaluate(float3 viewDir, float3 lightDir);
};
```

### Автоматическое дифференцирование (Automatic Differentiation)

Техника автоматического вычисления производных функций для ML и оптимизации.

```slang
[Differentiable]
float myFunction(float x, float y)
{
    return sin(x) * cos(y);
}
```

---

## Термины компиляции

### Модульная компиляция (Modular Compilation)

Процесс компиляции, при котором каждый модуль компилируется отдельно и может быть переиспользован. Позволяет
инкрементальную компиляцию.

### Инкрементальная компиляция (Incremental Compilation)

Компиляция только изменённых частей кода. В Slang достигается через модульную систему и кэширование.

### Промежуточное представление (Intermediate Representation, IR)

Абстрактное представление кода между исходным кодом и целевым форматом. Slang использует собственный SSA-based IR.

### SPIR-V

Standard Portable Intermediate Representation — бинарный формат шейдеров для Vulkan. Основной целевой формат при
компиляции Slang для Vulkan.

---

## Типы данных

### Скалярные типы

| Тип      | Описание                           |
|----------|------------------------------------|
| `bool`   | Булев тип                          |
| `int`    | 32-битное знаковое целое           |
| `uint`   | 32-битное беззнаковое целое        |
| `float`  | 32-битное число с плавающей точкой |
| `double` | 64-битное число с плавающей точкой |
| `half`   | 16-битное число с плавающей точкой |

### Векторные типы

| Тип                          | Описание      |
|------------------------------|---------------|
| `float2`, `float3`, `float4` | Векторы float |
| `int2`, `int3`, `int4`       | Векторы int   |
| `uint2`, `uint3`, `uint4`    | Векторы uint  |
| `bool2`, `bool3`, `bool4`    | Векторы bool  |

### Матричные типы

| Тип        | Описание    |
|------------|-------------|
| `float2x2` | Матрица 2x2 |
| `float3x3` | Матрица 3x3 |
| `float4x4` | Матрица 4x4 |

### Текстуры

| Тип              | Описание            |
|------------------|---------------------|
| `Texture1D<T>`   | 1D текстура         |
| `Texture2D<T>`   | 2D текстура         |
| `Texture3D<T>`   | 3D текстура         |
| `TextureCube<T>` | Cube map            |
| `RWTexture2D<T>` | Read-write текстура |

### Буферы

| Тип                     | Описание                           |
|-------------------------|------------------------------------|
| `cbuffer`               | Constant buffer (uniform buffer)   |
| `StructuredBuffer<T>`   | Read-only structured buffer (SSBO) |
| `RWStructuredBuffer<T>` | Read-write structured buffer       |

---

## Vulkan-специфичные термины

### Descriptor

Объект Vulkan, представляющий ресурс (буфер, текстуру, сэмплер), доступный шейдеру.

### Descriptor Set

Набор дескрипторов, привязываемых к pipeline как единая группа.

### Binding

Индекс ресурса внутри descriptor set. Указывается в шейдере через `[[vk::binding(N, M)]]`.

### Push Constants

Механизм передачи небольших данных в шейдер без создания descriptor set. Ограничен 128 байтами.

### Buffer Device Address (BDA)

Механизм прямого доступа к памяти GPU через указатели. Требует Vulkan 1.2+.

### Descriptor Indexing

Возможность динамически индексировать массивы дескрипторов в шейдере. Основа bindless rendering.

---

## Шейдерные стадии

| Стадия        | Имя в Slang     | Описание                  |
|---------------|-----------------|---------------------------|
| Vertex        | `vertex`        | Обработка вершин          |
| Fragment      | `fragment`      | Вычисление цвета пикселей |
| Compute       | `compute`       | Общие вычисления на GPU   |
| Geometry      | `geometry`      | Генерация примитивов      |
| Hull          | `hull`          | Tessellation control      |
| Domain        | `domain`        | Tessellation evaluation   |
| Mesh          | `mesh`          | Mesh shader               |
| Amplification | `amplification` | Task shader               |
| Raygen        | `raygen`        | Ray generation            |
| Closest hit   | `closesthit`    | Ближайшее попадание луча  |
| Any hit       | `anyhit`        | Любое попадание луча      |
| Miss          | `miss`          | Промах луча               |
| Callable      | `callable`      | Вызываемый шейдер         |

---

## Семантика

### Системные семантики

| Семантика             | Описание                        |
|-----------------------|---------------------------------|
| `SV_Position`         | Позиция в clip space            |
| `SV_Target`           | Выходной цвет пикселя           |
| `SV_Depth`            | Глубина пикселя                 |
| `SV_DispatchThreadID` | Глобальный ID потока (compute)  |
| `SV_GroupID`          | ID группы потоков (compute)     |
| `SV_GroupThreadID`    | Локальный ID в группе (compute) |
| `SV_GroupIndex`       | Линейный индекс в группе        |
| `SV_InstanceID`       | ID инстанса                     |
| `SV_VertexID`         | ID вершины                      |

---

## Атрибуты

### Шейдерные атрибуты

| Атрибут                 | Описание                        |
|-------------------------|---------------------------------|
| `[numthreads(x, y, z)]` | Размер группы потоков (compute) |
| `[shader("type")]`      | Тип шейдера                     |
| `[shader("vertex")]`    | Вершинный шейдер                |
| `[shader("fragment")]`  | Фрагментный шейдер              |
| `[shader("compute")]`   | Compute шейдер                  |

### Атрибуты оптимизации

| Атрибут                | Описание                 |
|------------------------|--------------------------|
| `[ForceInline]`        | Принудительный инлайнинг |
| `[NoInline]`           | Запрет инлайнинга        |
| `[InlineThreshold(N)]` | Порог инлайнинга         |

### Vulkan атрибуты

| Атрибут                    | Описание                    |
|----------------------------|-----------------------------|
| `[[vk::binding(N, M)]]`    | Binding N в set M           |
| `[[vk::push_constant]]`    | Push constant блок          |
| `[[vk::constant_id(N)]]`   | Специализационная константа |
| `[[vk::buffer_reference]]` | Тип указателя на буфер      |

---

## Инструменты

### slangc

Командный компилятор Slang для компиляции шейдеров из командной строки.

### Slang API

Программный интерфейс для динамической компиляции шейдеров во время выполнения.

### Reflection API

API для получения информации о структуре шейдера: bindings, push constants, типы параметров.

---

## Сокращения

| Сокращение | Полное название                                        |
|------------|--------------------------------------------------------|
| SPIR-V     | Standard Portable Intermediate Representation - Vulkan |
| IR         | Intermediate Representation                            |
| SSBO       | Shader Storage Buffer Object                           |
| UBO        | Uniform Buffer Object                                  |
| BDA        | Buffer Device Address                                  |
| FMA        | Fused Multiply-Add                                     |
| LOD        | Level of Detail                                        |
| GPU        | Graphics Processing Unit                               |
| CPU        | Central Processing Unit                                |