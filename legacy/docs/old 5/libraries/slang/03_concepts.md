# Основные понятия Slang

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
