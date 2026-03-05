# Будущее C++ (C++23/26 Features)

**🟢🟡🔴 Уровни сложности:**

- 🟢 Уровень 1: Модули, `std::print`, базовые улучшения
- 🟡 Уровень 2: Pattern matching, улучшенные ranges, контракты
- 🔴 Уровень 3: Рефлексия, метаклассы, специализированные фичи для вокселей

> **Связь с философией:** Новые стандарты C++ — это инструменты, а не цели.
> См. [02_zero-cost-abstractions.md](../../philosophy/02_zero-cost-abstractions.md). Используйте фичи, которые дают
> реальную пользу, а не просто "модные".

## ⚠️ Реалистичный взгляд на поддержку компиляторов

**Важно**: Документация ниже оптимистична. В реальности поддержка C++23/26 фич варьируется между компиляторами. Будьте
готовы к откату на C++20 если сборка начинает падать не из-за вашего кода.

### Текущий статус компиляторов (Февраль 2026)

| Компилятор      | Версия                    | C++23 Поддержка | Модули (`import std;`)              | Pattern Matching                            | Контракты | `std::print` | `std::expected` |
|-----------------|---------------------------|-----------------|-------------------------------------|---------------------------------------------|-----------|--------------|-----------------|
| **MSVC**        | Visual Studio 2022 17.8+  | ~85%            | Экспериментально (`/std:c++latest`) | Нет                                         | Нет       | ✅ 19.32+     | ✅ 19.33+        |
| **MSVC**        | Visual Studio 2022 17.10+ | ~90%            | Экспериментально (`/std:c++latest`) | Нет                                         | Нет       | ✅            | ✅               |
| **Clang**       | 18.0+                     | ~90%            | Частично (`-std=c++2b -fmodules`)   | Экспериментально (`-fexperimental-library`) | Нет       | ✅            | ✅               |
| **Clang**       | 19.0+                     | ~95%            | Частично (`-std=c++2b -fmodules`)   | Экспериментально                            | Нет       | ✅            | ✅               |
| **GCC**         | 13.0+                     | ~80%            | Частично (`-std=c++2b -fmodules`)   | Нет                                         | Нет       | ✅            | ✅               |
| **GCC**         | 14.0+                     | ~85%            | Частично (`-std=c++2b -fmodules`)   | Нет                                         | Нет       | ✅            | ✅               |
| **Apple Clang** | 15.0+ (Xcode 15)          | ~70%            | Нет                                 | Нет                                         | Нет       | ❌            | ❌               |
| **Apple Clang** | 16.0+ (Xcode 16)          | ~75%            | Нет                                 | Нет                                         | Нет       | ❌            | ❌               |

### Детальная таблица поддержки фич C++23/26

| Фича                                  | Стандарт        | MSVC                | Clang               | GCC         | Apple Clang | Статус ProjectV               |
|---------------------------------------|-----------------|---------------------|---------------------|-------------|-------------|-------------------------------|
| **`std::print` / `std::format`**      | C++23           | ✅ 19.32+            | ✅ 14+               | ✅ 13+       | ❌           | ✅ Рекомендуется               |
| **`std::expected`**                   | C++23           | ✅ 19.33+            | ✅ 16+               | ✅ 13+       | ❌           | ✅ Рекомендуется               |
| **Модули (`import std;`)**            | C++20/23        | 🟡 Экспериментально | 🟡 Частично         | 🟡 Частично | ❌           | ⚠️ Только для экспериментов   |
| **Pattern Matching (`inspect`)**      | C++26           | ❌                   | 🟡 Экспериментально | ❌           | ❌           | ❌ Не использовать             |
| **Контракты**                         | C++23 (удалено) | ❌                   | ❌                   | ❌           | ❌           | ❌ Удалено из стандарта        |
| **`std::mdspan`**                     | C++23           | ✅ 19.33+            | ✅ 16+               | ✅ 13+       | ❌           | ✅ Для матричных операций      |
| **`std::flat_map` / `std::flat_set`** | C++23           | ✅ 19.33+            | ✅ 18+               | ✅ 13+       | ❌           | ✅ Для cache-friendly структур |
| **`std::generator`**                  | C++23           | ✅ 19.33+            | ✅ 17+               | ✅ 13+       | ❌           | ✅ Для корутин                 |
| **`std::stacktrace`**                 | C++23           | ✅ 19.33+            | ✅ 14+               | ✅ 13+       | ❌           | ✅ Для отладки                 |
| **`std::byteswap`**                   | C++23           | ✅ 19.33+            | ✅ 18+               | ✅ 13+       | ❌           | ✅ Для сетевого кода           |
| **`std::is_scoped_enum`**             | C++23           | ✅ 19.33+            | ✅ 14+               | ✅ 13+       | ❌           | ✅ Для рефлексии               |
| **`std::to_underlying`**              | C++23           | ✅ 19.33+            | ✅ 18+               | ✅ 13+       | ❌           | ✅ Для enum преобразований     |

### Рекомендации для ProjectV с конкретными версиями

```cpp
// Безопасные фичи C++23 для ProjectV (поддерживаются всеми компиляторами)
#if __cpp_lib_print >= 202207L
    #include <print>  // Безопасно: MSVC 19.32+, Clang 14+, GCC 13+
#else
    #include <iostream>
    #include <format>
#endif

#if __cpp_lib_expected >= 202202L
    #include <expected>  // Безопасно: MSVC 19.33+, Clang 16+, GCC 13+
#endif

// Опасные фичи (требуют проверки версий)
#if defined(_MSC_VER) && _MSC_VER >= 1937  // VS 2022 17.7+
    // Можно использовать std::mdspan, std::flat_map
#elif defined(__clang__) && __clang_major__ >= 16
    // Можно использовать std::mdspan, std::flat_map
#elif defined(__GNUC__) && __GNUC__ >= 13
    // Можно использовать std::mdspan, std::flat_map
#endif

// Модули - только для экспериментов
#ifdef PROJECTV_EXPERIMENTAL_MODULES
    #if defined(_MSC_VER) && _MSC_VER >= 1933 && defined(_MSVC_LANG) && _MSVC_LANG >= 202302L
        // MSVC с /std:c++latest
        import std;
    #elif defined(__clang__) && __clang_major__ >= 18 && __cplusplus >= 202302L
        // Clang с -std=c++2b -fmodules
        import std;
    #else
        #include <iostream>
        #include <vector>
        // ... другие заголовки
    #endif
#else
    // Классические заголовки
    #include <iostream>
    #include <vector>
    // ...
#endif
```

### Минимальные версии компиляторов для ProjectV

ProjectV устанавливает следующие минимальные требования для стабильной работы:

| Компилятор      | Минимальная версия      | Рекомендуемая версия      | Примечания                       |
|-----------------|-------------------------|---------------------------|----------------------------------|
| **MSVC**        | Visual Studio 2022 17.8 | Visual Studio 2022 17.10+ | Для полной поддержки C++23       |
| **Clang**       | 16.0                    | 18.0+                     | Для стабильной поддержки модулей |
| **GCC**         | 13.0                    | 14.0+                     | Для лучшей поддержки C++23       |
| **Apple Clang** | 15.0 (Xcode 15)         | 16.0+ (Xcode 16)          | Ограниченная поддержка C++23     |

### CMake конфигурация для поддержки компиляторов

```cmake
# Проверка версии компилятора
if (MSVC)
  if (MSVC_VERSION LESS 1937)  # До VS 2022 17.7
    message(WARNING "MSVC version ${MSVC_VERSION} may have limited C++23 support")
    set(PROJECTV_USE_EXPERIMENTAL OFF)
  else ()
    set(PROJECTV_USE_EXPERIMENTAL ON)
  endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.0)
    message(WARNING "Clang version ${CMAKE_CXX_COMPILER_VERSION} has limited C++23 support")
    set(PROJECTV_USE_EXPERIMENTAL OFF)
  else ()
    set(PROJECTV_USE_EXPERIMENTAL ON)
  endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0)
    message(FATAL_ERROR "GCC version ${CMAKE_CXX_COMPILER_VERSION} doesn't support required C++23 features")
  else ()
    set(PROJECTV_USE_EXPERIMENTAL ON)
  endif ()
endif ()

# Настройка флагов компиляции
if (PROJECTV_USE_EXPERIMENTAL)
  target_compile_features(projectv PUBLIC cxx_std_23)

  if (MSVC)
    target_compile_options(projectv PRIVATE "/std:c++latest")
  else ()
    target_compile_options(projectv PRIVATE "-std=c++2b")
  endif ()
else ()
  # Откат на C++20
  target_compile_features(projectv PUBLIC cxx_std_20)

  if (MSVC)
    target_compile_options(projectv PRIVATE "/std:c++20")
  else ()
    target_compile_options(projectv PRIVATE "-std=c++20")
  endif ()
endif ()

# Предупреждения о неподдерживаемых фичах
add_custom_target(check_cpp23_support
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_cpp23.cmake
  COMMENT "Checking C++23 compiler support..."
)
```

### Рекомендации для ProjectV

1. **Модули (`import std;`)**:

- ❌ **Не используйте в продакшене**
- Проблемы: CMake + Ninja/MSBuild нестабильны с модулями
- Решение: Используйте классические `#include` до стабилизации

1. **Pattern Matching (`inspect`)**:

- ❌ **Только для экспериментов**
- Проблемы: Синтаксис может измениться до C++26
- Решение: Используйте `std::visit` с `std::variant`

1. **Контракты**:

- ❌ **Не поддерживается ни одним компилятором**
- Проблемы: Удалены из C++23, будущее неясно
- Решение: Используйте `assert()` и runtime проверки

1. **`std::print` / `std::format`**:

- ✅ **Используйте смело**
- Поддержка: MSVC 19.32+, Clang 16+, GCC 13+
- Преимущества: Быстрее `std::cout`, локализация

### Безопасный подход для ProjectV

```cpp
// Вместо рискованных фич C++23/26:
#ifdef PROJECTV_USE_EXPERIMENTAL
    import std;  // Модули - нестабильно
    // ... экспериментальный код
#else
    #include <iostream>  // Классический подход
    #include <vector>
    #include <string>
    // ... стабильный код
#endif

// Конфигурация CMake:
option(PROJECTV_USE_EXPERIMENTAL "Use experimental C++23/26 features" OFF)
if(PROJECTV_USE_EXPERIMENTAL)
    target_compile_definitions(projectv PRIVATE PROJECTV_USE_EXPERIMENTAL)
endif()
```

### Миграционный план с откатом

```cpp
// Всегда имейте план отката:
#if __cpp_modules >= 202207L && defined(__clang__)
    // Clang с поддержкой модулей
    import std;
#elif __has_include(<print>)
    // Компилятор с std::print
    #include <print>
#else
    // Откат на C++17/C++20
    #include <iostream>
    #include <format>

    namespace std {
        // Эмуляция std::print для старых компиляторов
        template<typename... Args>
        void print(std::string_view fmt, Args&&... args) {
            std::cout << std::vformat(fmt, std::make_format_args(args...));
        }

        template<typename... Args>
        void println(std::string_view fmt, Args&&... args) {
            std::cout << std::vformat(fmt, std::make_format_args(args...)) << '\n';
        }
    }
#endif
```

ProjectV ориентирован на использование самых современных возможностей языка, но с реалистичным подходом. Мы смотрим на
стандарты C++23 и будущее C++26, чтобы писать код, который будет актуален еще долго и максимально эффективен для
воксельного движка, но всегда имеем план отката на C++20.

## Уровень 1: Модули и базовые улучшения

### Модули (Modules) — Смерть `#include`

> **⚠️ Warning для ProjectV: `#include` — основной путь**
>
> Несмотря на потенциальные преимущества модулей, в ProjectV мы используем классические `#include` как основной
> способ организации кода. Причины:
> - **CMake поддержка:** Нестабильная, требует экспериментальных флагов
> - **IDE поддержка:** IntelliSense, clangd, code navigation ограничены
> - **Кросс-компиляция:** Разные флаги для MSVC/Clang/GCC
> - **Отладка:** Сложности с breakpoint-ами в модулях
>
> **Модули можно использовать** в изолированных экспериментах, но **не внедряйте** их в основной кодовой базе.

Вместо медленного текстового включения файлов (`#include`), модули (`import`) используют предкомпилированный бинарный
формат.

```cpp
// Модульная архитектура для ProjectV
// renderer.ixx (модуль интерфейса)
export module ProjectV.Renderer;

import std;
import glm;

export namespace ProjectV::Renderer {
    class VulkanRenderer {
    public:
        void render(const std::vector<glm::vec3>& vertices);
        void setClearColor(const glm::vec4& color);
    };

    struct RenderStats {
        uint64_t drawCalls;
        uint64_t verticesRendered;
        float frameTime;
    };
}

// main.cpp
import std;
import ProjectV.Renderer;
import ProjectV.ECS;

int main() {
    using namespace ProjectV;

    // std::print/println быстрее std::cout
    std::println("🚀 ProjectV запущен с C++23 модулями!");

    Renderer::VulkanRenderer renderer;
    ECS::World world;

    // Инициализация
    renderer.setClearColor({0.1f, 0.2f, 0.3f, 1.0f});

    return 0;
}
```

**Преимущества модулей для ProjectV:**

1. **Ускорение сборки в десятки раз**: Критично для больших проектов
2. **Изоляция макросов**: Макросы из одного файла не "просачиваются" в другой
3. **Гигиена имен**: Не нужно возиться с `Header Guards`
4. **Чёткие зависимости**: Компилятор видит все зависимости явно

### `std::print` и `std::format`

```cpp
// Современный вывод для отладки
import std;

void debugVoxelChunk(const VoxelChunk& chunk) {
    // Форматированный вывод с локализацией
    std::print("Чанк [{}x{}x{}]:\n", CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);

    int solidCount = 0;
    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                if (chunk.getVoxel(x, y, z).type != VoxelType::AIR) {
                    ++solidCount;
                }
            }
        }
    }

    float fillPercentage = (solidCount * 100.0f) / CHUNK_VOLUME;
    std::println("  Твёрдых вокселей: {} ({:.1f}%)", solidCount, fillPercentage);

    // Форматирование для логов
    auto logMessage = std::format("[{}] Чанк обновлён: {} вокселей",
                                 std::chrono::system_clock::now(),
                                 solidCount);
    TracyLog(logMessage.c_str(), logMessage.size());
}
```

### Улучшенные умные указатели

```cpp
// C++23: make_unique_for_overwrite для массивов
import std;
import glm;

class VoxelMesh {
    std::unique_ptr<glm::vec3[]> vertices_;
    std::unique_ptr<glm::vec3[]> normals_;
    size_t vertexCount_;

public:
    VoxelMesh(size_t maxVertices)
        : vertexCount_(0) {
        // Без инициализации нулями - быстрее для больших массивов
        vertices_ = std::make_unique_for_overwrite<glm::vec3[]>(maxVertices);
        normals_ = std::make_unique_for_overwrite<glm::vec3[]>(maxVertices);
    }

    void addVertex(const glm::vec3& position, const glm::vec3& normal) {
        if (vertexCount_ < maxVertices_) {
            vertices_[vertexCount_] = position;
            normals_[vertexCount_] = normal;
            ++vertexCount_;
        }
    }
};
```

## Уровень 2: Pattern matching и улучшенные ranges

### Pattern Matching (C++26)

Оператор `inspect` заменит громоздкие `if-else` и `switch`. Это сделает код более декларативным.

```cpp
// Pattern matching для обработки событий ECS
import std;

enum class VoxelEventType {
    Placed,
    Destroyed,
    Modified,
    LiquidFlow
};

struct VoxelEvent {
    VoxelEventType type;
    glm::ivec3 position;
    VoxelType oldType;
    VoxelType newType;
};

void handleVoxelEvent(const VoxelEvent& event) {
    inspect (event) {
        // Деструктуризация + проверка типа
        <VoxelEventType::Placed> [pos, old, new] => {
            std::println("Воксель размещён в {}: {} -> {}", pos, old, new);
            playSound("place.wav");
            updateLighting(pos);
        }
        <VoxelEventType::Destroyed> [pos, old, _] => {
            std::println("Воксель уничтожен в {}: {}", pos, old);
            playSound("break.wav");
            spawnParticles(pos, old);
        }
        <VoxelEventType::LiquidFlow> [pos, _, new] => {
            std::println("Жидкость течёт в {}: {}", pos, new);
            updateFluidSimulation(pos);
        }
        _ => {
            std::println("Неизвестное событие вокселя");
        }
    };
}

// Pattern matching для компонентов
void processComponent(auto&& component) {
    inspect (component) {
        // Проверка типа и деструктуризация
        <Transform> [pos, rot, scale] => {
            // Обработка трансформации
            updateTransform(pos, rot, scale);
        }
        <Velocity> [linear, angular] => {
            // Обработка скорости
            applyPhysics(linear, angular);
        }
        <Health> [current, max] => {
            // Обработка здоровья
            checkDeath(current, max);
        }
        // Рекурсивный pattern matching
        <std::variant<Transform, Velocity, Health>> var => {
            processComponent(var);  // Рекурсивная обработка
        }
    };
}
```

### Улучшенные Ranges (C++23)

```cpp
// Продвинутые пайплайны для обработки вокселей
import std;
import glm;

class AdvancedVoxelProcessor {
public:
    auto findVisibleVoxels(const VoxelChunk& chunk, const glm::vec3& cameraPos) {
        return chunk.voxels()
            | std::views::enumerate  // C++23: индекс + значение
            | std::views::filter([&](auto&& pair) {
                auto [index, voxel] = pair;
                return voxel.type != VoxelType::AIR
                    && isVisible(index, cameraPos);
            })
            | std::views::transform([&](auto&& pair) {
                auto [index, voxel] = pair;
                auto pos = indexToPosition(index);
                return RenderableVoxel{pos, voxel.type, voxel.light};
            })
            | std::views::chunk_by( {
                // Группировка по материалу для инстанцирования
                return a.type == b.type;
            })
            | std::views::transform( {
                // Создание draw call для каждого материала
                return createDrawCall(chunk);
            });
    }

    // C++23: views::join_with для сложных пайплайнов
    auto processMultipleChunks(const std::vector<VoxelChunk>& chunks) {
        return chunks
            | std::views::transform(&VoxelChunk::voxels)
            | std::views::join_with(std::views::empty<Voxel>)  // Разделитель
            | std::views::filter(&Voxel::isSolid)
            | std::views::take(1'000'000);  // Цифровые разделители
    }
};
```

### Контракты (Contracts) - C++23

```cpp
// Контракты для проверки инвариантов
import std;

class VoxelBuffer {
    std::unique_ptr<Voxel[]> data_;
    size_t size_;

public:
    VoxelBuffer(size_t size)
        : size_(size)
        [[pre: size > 0]]           // Предусловие
        [[post: data_ != nullptr]]  // Постусловие
    {
        data_ = std::make_unique_for_overwrite<Voxel[]>(size);
    }

    Voxel& operator
        [[pre: index < size_]]      // Проверка границ при компиляции
        [[post r: &r == &data_[index]]]  // Гарантия возврата правильной ссылки
    {
        return data_[index];
    }

    void resize(size_t newSize)
        [[pre: newSize > 0]]
        [[post: size_ == newSize]]
        [[post: data_ != nullptr]]
    {
        auto newData = std::make_unique_for_overwrite<Voxel[]>(newSize);
        std::copy_n(data_.get(), std::min(size_, newSize), newData.get());
        data_ = std::move(newData);
        size_ = newSize;
    }
};
```

## Уровень 3: Рефлексия и метаклассы

### Рефлексия (C++26) — Святой грааль

Рефлексия позволяет программе "видеть" свою структуру во время компиляции. Это навсегда изменит GameDev.

```cpp
// Автоматическая сериализация компонентов ECS
import std;
import glm;

// Аннотация для рефлексии
[[reflect]]
struct Transform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
};

[[reflect]]
struct Velocity {
    glm::vec3 linear;
    glm::vec3 angular;
};

// Генератор сериализации через рефлексию
template<typename T>
consteval auto generateSerializer() {
    return []<typename... Fields>(std::meta::type<T>, Fields... fields) {
        return [=](const T& obj, std::vector<uint8_t>& buffer) {
            // Компиляция времени компиляции: генерация кода сериализации
            (serializeField(obj.[fields], buffer), ...);
        };
    }(std::meta::type<T>, std::meta::fields_of<T>...);
}

// Использование в ProjectV
class ComponentSerializer {
public:
    template<typename T>
    std::vector<uint8_t> serialize(const T& component) {
        std::vector<uint8_t> buffer;

        // Автоматически сгенерированная сериализация
        constexpr auto serializer = generateSerializer<T>();
        serializer(component, buffer);

        return buffer;
    }

    template<typename T>
    T deserialize(const std::vector<uint8_t>& buffer) {
        T component;

        // Автоматически сгенерированная десериализация
        constexpr auto deserializer = generateDeserializer<T>();
        deserializer(component, buffer);

        return component;
    }
};

// Автоматическая генерация UI инспектора
template<typename T>
void generateInspectorUI(const T& obj) {
    for... (const auto& field : std::meta::fields_of<T>) {
        std::string label = std::string(field.name);

        if constexpr (std::is_same_v<decltype(obj.[field]), glm::vec3>) {
            // Генерация ползунков для векторов
            ImGui::DragFloat3(label.c_str(), &obj.[field].x, 0.1f);
        } else if constexpr (std::is_same_v<decltype(obj.[field]), float>) {
            // Генерация ползунка для float
            ImGui::DragFloat(label.c_str(), &obj.[field], 0.1f);
        } else if constexpr (std::is_enum_v<decltype(obj.[field])>) {
            // Генерация выпадающего списка для enum
            generateEnumCombo(label, obj.[field]);
        }
    }
}
```

### Метаклассы (C++26) для кодогенерации

```cpp
// Метакласс для автоматической генерации компонентов ECS
$class component {
    // Правила для метакласса
    constexpr {
        // Все поля становятся public
        for... (auto f : $class.member_variables()) {
            f.make_public();
        }

        // Генерация конструктора по умолчанию
        compiler.generate_default_constructor();

        // Генерация операторов сравнения
        compiler.require(compiler.generate_equality_operators());

        // Генерация сериализации
        generate_serialization($class);

        // Добавление метаданных для рефлексии
        $class.add_attribute("Component");
    }
};

// Использование метакласса
component Transform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
};

component Velocity {
    glm::vec3 linear;
    glm::vec3 angular;
};

// Компилятор автоматически генерирует:
// - Конструктор по умолчанию
// - operator==, operator!=
// - Методы сериализации
// - Метаданные для рефлексии
```

### Специализированные фичи для вокселей

```cpp
// C++26: Customization points для воксельных алгоритмов
namespace voxel_traits {
    template<typename T>
    concept VoxelType = requires(T voxel) {
        { voxel.type } -> std::same_as<uint8_t>;
        { voxel.light } -> std::same_as<uint8_t>;
        { voxel.isTransparent() } -> std::same_as<bool>;
    };

    // Настройка алгоритмов через customization points
    template<VoxelType T>
    void greedyMeshing(const T* chunk, MeshData& mesh) {
        if constexpr (requires { T::customGreedyMeshing(chunk, mesh); }) {
            // Использование специализированной реализации
            T::customGreedyMeshing(chunk, mesh);
        } else {
            // Использование реализации по умолчанию
            defaultGreedyMeshing(chunk, mesh);
        }
    }
}

// C++26: Compile-time hash maps для быстрого поиска материалов
consteval auto generateMaterialTable() {
    std::array<std::pair<std::string_view, MaterialProperties>, 256> table{};

    // Заполнение таблицы при компиляции
    table[VoxelType::STONE] = {"Stone", {0.8f, 0.7f, 0.6f, 1.0f}};
    table[VoxelType::DIRT] = {"Dirt", {0.6f, 0.5f, 0.4f, 1.0f}};
    table[VoxelType::GRASS] = {"Grass", {0.2f, 0.8f, 0.2f, 1.0f}};
    // ... остальные материалы

    return table;
}

constexpr auto MATERIAL_TABLE = generateMaterialTable();
```

## Для ProjectV

### Архитектура с использованием C++23/26

```cpp
// Современная архитектура ProjectV с модулями
export module ProjectV.Core;

import std;
import glm;
import vulkan;  // Гипотетический модуль Vulkan

export namespace ProjectV {
    // Использование pattern matching для обработки событий
    class EventSystem {
    public:
        void processEvent(const auto& event) {
            inspect (event) {
                <WindowEvent> [type, data] => handleWindowEvent(type, data),
                <InputEvent> [key, action] => handleInputEvent(key, action),
                <VoxelEvent> [pos, type, action] => handleVoxelEvent(pos, type, action),
                _ => logUnknownEvent(event)
            };
        }
    };

    // Использование контрактов для безопасности
    class VoxelWorld {
        std::vector<VoxelChunk> chunks_;

    public:
        Voxel& getVoxel(const glm::ivec3& worldPos)
            [[pre: isValidPosition(worldPos)]]
            [[post r: r.type != VoxelType::INVALID]]
        {
            auto [chunkPos, localPos] = worldToChunkPos(worldPos);
            return chunks_[chunkIndex(chunkPos)].getVoxel(localPos);
        }

        void setVoxel(const glm::ivec3& worldPos, VoxelType type)
            [[pre: isValidPosition(worldPos)]]
            [[pre: type != VoxelType::INVALID]]
            [[post: getVoxel(worldPos).type == type]]
        {
            auto [chunkPos, localPos] = worldToChunkPos(worldPos);
            chunks_[chunkIndex(chunkPos)].setVoxel(localPos, type);

            // Автоматическое обновление соседей
            updateNeighbors(worldPos);
        }
    };

    // Использование рефлексии для редактора
    class EntityInspector {
    public:
        template<typename T>
        void drawComponentUI(flecs::entity entity, T& component) {
            ImGui::PushID(typeid(T).name());

            // Автоматическая генерация UI через рефлексию
            generateInspectorUI(component);

            // Кнопки действий через метаданные
            if constexpr (requires { T::inspectorActions(); }) {
                T::inspectorActions(entity, component);
            }

            ImGui::PopID();
        }
    };
}
```

### Миграционный план для ProjectV

```cpp
// Поэтапное внедрение современных фич
class MigrationPlan {
public:
    // Этап 1: C++20 (текущий)
    void stage1() {
        // - Концепты для шаблонов
        // - Ranges для обработки данных
        // - Coroutines для асинхронных операций
        // - Modules для ускорения сборки
    }

    // Этап 2: C++23 (ближайшее будущее)
    void stage2() {
        // - std::print для логирования
        // - std::expected для обработки ошибок
        // - Контракты для проверки инвариантов
        // - Улучшенные умные указатели
    }

    // Этап 3: C++26 (долгосрочное будущее)
    void stage3() {
        // - Pattern matching для обработки событий
        // - Рефлексия для автоматической сериализации
        // - Метаклассы для кодогенерации
        // - Специализированные фичи для вокселей
    }
};
```

## Распространённые ошибки и решения

1. **Слишком раннее внедрение экспериментальных фич**

- ❌ Использование нестабильного синтаксиса C++26 в продакшене
- ✅ Используйте фичи только после их стандартизации и стабилизации в компиляторах

1. **Игнорирование обратной совместимости**

- ❌ Требование C++26 для всей кодовой базы
- ✅ Поэтапное внедрение с поддержкой старых стандартов

1. **Сложность отладки современных фич**

- ❌ Непонятные ошибки компиляции с концептами и constraints
- ✅ Пишите понятные концепты с ясными сообщениями об ошибках

1. **Избыточное использование метапрограммирования**

- ❌ Слишком сложные шаблоны и constexpr вычисления
- ✅ Используйте метапрограммирование только там, где это действительно нужно

1. **Недостаточное тестирование новых возможностей**

- ❌ Внедрение фич без тестирования на разных компиляторах
- ✅ Тестируйте на GCC, Clang и MSVC перед внедрением

## Быстрые ссылки по задачам

- **Ускорить сборку проекта**: Перейдите на модули (C++20/23)
- **Улучшить обработку ошибок**: Используйте `std::expected` (C++23)
- **Сделать код более декларативным**: Используйте pattern matching (C++26)
- **Автоматизировать сериализацию**: Используйте рефлексию (C++26)
- **Улучшить безопасность кода**: Используйте контракты (C++23)
- **Оптимизировать обработку данных**: Используйте улучшенные ranges (C++23)

