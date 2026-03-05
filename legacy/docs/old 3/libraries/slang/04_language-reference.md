# Справочник языка Slang

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
