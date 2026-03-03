# Интеграция fastgltf

🟡 **Уровень 2: Средний**

Подключение fastgltf в C++ проект.

## CMake

### Базовая интеграция

```cmake
add_subdirectory(external/fastgltf)

add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE fastgltf::fastgltf)
```

simdjson подгружается автоматически через FetchContent или `find_package`.

### Проект уже использует simdjson

```cmake
# Добавьте simdjson ДО fastgltf
find_package(simdjson REQUIRED)  # или add_subdirectory

# Fastgltf обнаружит системный simdjson
add_subdirectory(external/fastgltf)
```

### Конфликт версий simdjson

```cmake
include(FetchContent)
FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG v3.6.2  # Конкретная версия
  OVERRIDE_FIND_PACKAGE
)

add_subdirectory(external/fastgltf)
```

## CMake-опции fastgltf

| Опция                                      | По умолчанию | Описание                                      |
|--------------------------------------------|--------------|-----------------------------------------------|
| `FASTGLTF_USE_64BIT_FLOAT`                 | OFF          | Использовать `double` вместо `float`          |
| `FASTGLTF_ENABLE_DEPRECATED_EXT`           | OFF          | Включить устаревшие расширения                |
| `FASTGLTF_USE_CUSTOM_SMALLVECTOR`          | OFF          | Использовать SmallVector в большем числе мест |
| `FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL`      | OFF          | Отключить pmr-аллокатор                       |
| `FASTGLTF_COMPILE_AS_CPP20`                | OFF          | Компилировать как C++20                       |
| `FASTGLTF_ENABLE_CPP_MODULES`              | OFF          | Включить поддержку C++20 modules              |
| `FASTGLTF_USE_STD_MODULE`                  | OFF          | Использовать std module (C++23)               |
| `FASTGLTF_ENABLE_TESTS`                    | OFF          | Сборка тестов                                 |
| `FASTGLTF_ENABLE_EXAMPLES`                 | OFF          | Сборка примеров                               |
| `FASTGLTF_ENABLE_DOCS`                     | OFF          | Сборка документации                           |
| `FASTGLTF_ENABLE_IMPLICIT_SHAPES`          | OFF          | Draft extension KHR_implicit_shapes           |
| `FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES` | OFF          | Draft extension KHR_physics_rigid_bodies      |

Пример:

```cmake
set(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL ON CACHE BOOL "" FORCE)
add_subdirectory(external/fastgltf)
```

## Заголовки

| Заголовок                            | Содержание                                   |
|--------------------------------------|----------------------------------------------|
| `fastgltf/core.hpp`                  | Parser, Exporter, Error — основной заголовок |
| `fastgltf/types.hpp`                 | POD типы и перечисления                      |
| `fastgltf/tools.hpp`                 | Accessor tools, node transform utilities     |
| `fastgltf/math.hpp`                  | Встроенная математика (векторы, матрицы)     |
| `fastgltf/base64.hpp`                | SIMD-оптимизированный base64 декодер         |
| `fastgltf/glm_element_traits.hpp`    | Интеграция с glm для accessor tools          |
| `fastgltf/dxmath_element_traits.hpp` | Интеграция с DirectXMath                     |

`core.hpp` уже включает `types.hpp`.

## Options

Флаги для `loadGltf` (побитовое ИЛИ):

| Опция                         | Описание                                                |
|-------------------------------|---------------------------------------------------------|
| `None`                        | Только парсинг JSON. GLB-буферы загружаются в память.   |
| `LoadExternalBuffers`         | Загрузить внешние .bin в `sources::Vector`              |
| `LoadExternalImages`          | Загрузить внешние изображения                           |
| `DecomposeNodeMatrices`       | Разложить матрицы узлов на TRS                          |
| `GenerateMeshIndices`         | Сгенерировать индексы для примитивов без index accessor |
| `AllowDouble`                 | Разрешить component type `GL_DOUBLE` (5130)             |
| `DontRequireValidAssetMember` | Не проверять поле `asset` в JSON                        |

```cpp
auto options = fastgltf::Options::LoadExternalBuffers
             | fastgltf::Options::LoadExternalImages
             | fastgltf::Options::DecomposeNodeMatrices;
```

Примечание: `LoadGLBBuffers` deprecated — GLB-буферы загружаются по умолчанию.

## Category

Маска того, что парсить:

| Значение         | Описание                                           |
|------------------|----------------------------------------------------|
| `All`            | Всё (по умолчанию)                                 |
| `OnlyRenderable` | Всё, кроме Animations и Skins                      |
| `OnlyAnimations` | Только Animations, Accessors, BufferViews, Buffers |

```cpp
// Для статических моделей — быстрее на 30-40%
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyRenderable);

// Для извлечения только анимаций
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyAnimations);
```

## Extensions

Битовая маска расширений передаётся в конструктор Parser:

```cpp
fastgltf::Extensions extensions =
    fastgltf::Extensions::KHR_texture_basisu
    | fastgltf::Extensions::KHR_materials_unlit
    | fastgltf::Extensions::KHR_mesh_quantization
    | fastgltf::Extensions::EXT_mesh_gpu_instancing;

fastgltf::Parser parser(extensions);
```

### Проверка расширений модели

```cpp
auto asset = parser.loadGltf(data.get(), basePath, options);

// Расширения, которые модель использует
for (const auto& ext : asset->extensionsUsed) {
    std::cout << "Uses: " << ext << "\n";
}

// Обязательные расширения (без которых модель не работает)
for (const auto& ext : asset->extensionsRequired) {
    std::cout << "Requires: " << ext << "\n";
}
```

### Утилиты для отладки

```cpp
// Имя первого установленного бита
std::string_view name = fastgltf::stringifyExtension(extensions);

// Список всех имён установленных битов
std::vector<std::string> names = fastgltf::stringifyExtensionBits(extensions);
```

## Связь с glm

Для использования glm с accessor tools подключите traits:

```cpp
#include <glm/glm.hpp>
#include <fastgltf/glm_element_traits.hpp>

// Теперь iterateAccessor<glm::vec3> работает
std::vector<glm::vec3> positions;
fastgltf::iterateAccessor<glm::vec3>(asset, accessor,
    [&](glm::vec3 pos) { positions.push_back(pos); });
```

## Android

Загрузка glTF из APK assets:

```cpp
#include <android/asset_manager_jni.h>

// Инициализация (вызывается один раз)
AAssetManager* manager = AAssetManager_fromJava(env, assetManager);
fastgltf::setAndroidAssetManager(manager);

// Загрузка из assets
auto data = fastgltf::AndroidGltfDataBuffer::FromAsset("models/scene.gltf");
if (data.error() != fastgltf::Error::None) {
    return false;
}

// LoadExternalBuffers и LoadExternalImages работают с APK
auto asset = parser.loadGltf(data.get(), "",
                              fastgltf::Options::LoadExternalBuffers);
```

## Callbacks

### BufferAllocationCallback

Для прямой записи в GPU-память:

```cpp
struct Context {
    // Ваш GPU context
};

Context ctx;
parser.setUserPointer(&ctx);

parser.setBufferAllocationCallback(
    [](void* userPointer, size_t bufferSize, fastgltf::BufferAllocateFlags flags)
    -> fastgltf::BufferInfo {

        auto* ctx = static_cast<Context*>(userPointer);

        // Выделение GPU буфера
        void* mappedPtr = allocateGPUBuffer(ctx, bufferSize);

        return fastgltf::BufferInfo{
            .mappedMemory = mappedPtr,
            .customId = /* ваш ID буфера */
        };
    },
    nullptr,  // unmap callback (опционально)
    nullptr   // deallocate callback (опционально)
);
```

### ExtrasParseCallback

Для чтения extras из JSON:

```cpp
auto extrasCallback = [](simdjson::dom::object* extras,
                         std::size_t objectIndex,
                         fastgltf::Category category,
                         void* userPointer) {
    if (category != fastgltf::Category::Nodes)
        return;

    std::string_view customName;
    if ((*extras)["customName"].get_string().get(customName) == simdjson::SUCCESS) {
        // Сохраните customName
    }
};

parser.setExtrasParseCallback(extrasCallback);
parser.setUserPointer(&yourDataStorage);
```

### Base64DecodeCallback

Для кастомного base64-декодера:

```cpp
parser.setBase64DecodeCallback([](std::string_view encoded, void* userPointer)
    -> std::vector<std::byte> {
    // Ваш декодер
});
```

## C++20 Modules

При включении `FASTGLTF_ENABLE_CPP_MODULES`:

```cmake
set(FASTGLTF_ENABLE_CPP_MODULES ON CACHE BOOL "" FORCE)
add_subdirectory(external/fastgltf)

# Использование
target_link_libraries(YourApp PRIVATE fastgltf::module)
```

```cpp
// В коде
import fastgltf;
```

Примечание: поддержка модулей зависит от компилятора и генератора CMake.
