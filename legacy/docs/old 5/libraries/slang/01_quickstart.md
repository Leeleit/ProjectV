# Быстрый старт с Slang

**🟢 Уровень 1: Начинающий** — Минимальный пример компиляции шейдера Slang в SPIR-V.

Это руководство поможет скомпилировать первый шейдер Slang и подготовить его для использования с Vulkan.

---

## Создание первого шейдера

Создайте файл `simple_triangle.slang`:

```slang
// simple_triangle.slang

// Структура вершинных данных
struct Vertex
{
    float3 position : POSITION;
    float3 color : COLOR;
};

// Структура для передачи в фрагментный шейдер
struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

// Константный буфер с матрицей преобразования
cbuffer Transform : register(b0)
{
    float4x4 modelViewProjection;
}

// Вершинный шейдер
VertexOutput vsMain(Vertex input)
{
    VertexOutput output;
    output.position = mul(modelViewProjection, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}

// Фрагментный шейдер
float4 fsMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
```

---

## Компиляция в SPIR-V

Используйте `slangc` для компиляции шейдера:

```bash
# Базовая компиляция
slangc simple_triangle.slang -o simple_triangle.spv -target spirv

# Компиляция с оптимизациями
slangc simple_triangle.slang -o simple_triangle_opt.spv -target spirv -O

# Компиляция отдельных entry points
slangc simple_triangle.slang -entry vsMain -stage vertex -o vertex.spv -target spirv
slangc simple_triangle.slang -entry fsMain -stage fragment -o fragment.spv -target spirv

# Компиляция с указанием профиля SPIR-V
slangc simple_triangle.slang -o simple_triangle.spv -target spirv -profile spirv_1_5
```

---

## Проверка результата

Просмотр сгенерированного SPIR-V в текстовом виде:

```bash
# Транспиляция в GLSL для проверки
slangc simple_triangle.slang -target glsl -o simple_triangle.glsl

# Генерация SPIR-V ассемблера
slangc simple_triangle.slang -target spirv-asm -o simple_triangle.spvasm
```

---

## Compute shader пример

Создайте файл `simple_compute.slang`:

```slang
// simple_compute.slang

// Выходной буфер
RWStructuredBuffer<float> outputBuffer : register(u0);

// Конфигурация
cbuffer Config : register(b0)
{
    uint outputSize;
    float scaleFactor;
}

// Compute shader
[numthreads(64, 1, 1)]
void csMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint id = dispatchThreadId.x;
    if (id < outputSize)
    {
        outputBuffer[id] = sin(float(id) * scaleFactor);
    }
}
```

Компиляция compute shader:

```bash
slangc simple_compute.slang \
    -entry csMain \
    -stage compute \
    -o simple_compute.spv \
    -target spirv \
    -profile spirv_1_5 \
    -O
```

---

## Модульная организация

Slang поддерживает модули для организации кода:

```slang
// math_utils.slang
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
// main_shader.slang
import MathUtils;

struct Vertex
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

VertexOutput vsMain(Vertex input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.normal = input.normal;
    return output;
}
```

Компиляция с модулями:

```bash
# Указать путь поиска модулей
slangc main_shader.slang \
    -I . \
    -entry vsMain \
    -stage vertex \
    -o main_vertex.spv \
    -target spirv
```

---

## Типичные параметры компиляции

| Параметр             | Описание              | Пример               |
|----------------------|-----------------------|----------------------|
| `-o <file>`          | Выходной файл         | `-o shader.spv`      |
| `-target <format>`   | Целевой формат        | `-target spirv`      |
| `-profile <profile>` | Профиль SPIR-V        | `-profile spirv_1_5` |
| `-entry <name>`      | Имя entry point       | `-entry vsMain`      |
| `-stage <stage>`     | Шейдерная стадия      | `-stage vertex`      |
| `-O`                 | Оптимизация           | `-O` или `-O2`       |
| `-g`                 | Отладочная информация | `-g`                 |
| `-I <path>`          | Путь поиска модулей   | `-I shaders/`        |
| `-D <name>=<value>`  | Определение макроса   | `-D DEBUG=1`         |

---

## Дальнейшие шаги

- **02. Установка** — настройка окружения разработки
- **03. Основные понятия** — модули, generics, интерфейсы
- **05. Справочник API** — полный список команд slangc
