# Решение проблем Slang

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
