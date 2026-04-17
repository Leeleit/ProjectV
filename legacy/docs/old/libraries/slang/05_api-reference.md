# Справочник API Slang

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
