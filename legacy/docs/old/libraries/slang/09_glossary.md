# Глоссарий Slang

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
