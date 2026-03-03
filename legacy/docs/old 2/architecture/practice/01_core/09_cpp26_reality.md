# Спецификация C++26 Reality Check [🟡 Уровень 2]

**Статус:** Technical Specification
**Уровень:** 🟡 Средний
**Дата:** 2026-02-23
**Версия:** 1.0

---

## Обзор

Документ описывает прагматичные решения для работы с **C++26 features** в условиях неполной поддержки компиляторами.
Основной фокус: сериализация (Glaze/PFR), PIMPL для stdexec, модульные workarounds.

---

## 1. Проблема: Неполная поддержка C++26

### 1.1 Матрица поддержки компиляторов (2026 Q1)

| Feature                   | GCC 14 | GCC 15          | Clang 18   | Clang 19   | MSVC 19.4 |
|---------------------------|--------|-----------------|------------|------------|-----------|
| `import std;`             | ✅      | ✅               | ⚠️ Partial | ✅          | ✅         |
| `std::execution` (P2300)  | ❌      | ⚠️ Partial      | ❌          | ⚠️ Partial | ❌         |
| `std::expected`           | ✅      | ✅               | ✅          | ✅          | ✅         |
| `std::format`             | ✅      | ✅               | ✅          | ✅          | ✅         |
| Static Reflection (P2996) | ❌      | ❌               | ❌          | ❌          | ❌         |
| `std::linalg`             | ❌      | ⚠️ Partial      | ❌          | ❌          | ❌         |
| Contracts                 | ❌      | ❌               | ❌          | ❌          | ❌         |
| `std::execution::task`    | ❌      | ⚠️ Experimental | ❌          | ❌          | ❌         |

**Legend:** ✅ Full support | ⚠️ Partial/Experimental | ❌ Not supported

### 1.2 Стратегия

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    C++26 Reality Check Strategy                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Feature Availability                          │    │
│  ├─────────────────────────────────────────────────────────────────┤    │
│  │                                                                 │    │
│  │   ✅ Available           ⚠️ Partial            ❌ Not Available │    │
│  │   ┌─────────────┐       ┌─────────────┐       ┌─────────────┐  │    │
│  │   │ std::format │       │ Modules     │       │ Reflection  │  │    │
│  │   │ std::expected│       │ stdexec     │       │ std::linalg │  │    │
│  │   │ std::span   │       │             │       │ Contracts   │  │    │
│  │   └─────────────┘       └─────────────┘       └─────────────┘  │    │
│  │         │                     │                     │          │    │
│  │         ▼                     ▼                     ▼          │    │
│  │   Use directly         Workarounds         Alternatives       │    │
│  │                         PIMPL              Glaze/PFR           │    │
│  │                         Polyfills          Boost::describe     │    │
│  │                                                                 │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  Key Principles:                                                         │
│  1. Use what works reliably                                              │
│  2. Abstract what doesn't (PIMPL)                                        │
│  3. Document known issues                                                │
│  4. Plan migration paths                                                 │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Сериализация: Glaze + PFR

### 2.1 Почему не Static Reflection?

Static Reflection (P2996) не будет доступен до C++29+. Пока используем комбинацию:

- **Boost.PFR** — для reflection агрегатных типов (без макросов)
- **Glaze** — для JSON/binary сериализации (high performance)

### 2.2 Базовая сериализация

```cpp
// ProjectV.Serialization.cppm
export module ProjectV.Serialization;

import std;
import glaze;       // https://github.com/stephenberry/glaze
import boost.pfr;   // Boost.PFR2

export namespace projectv::serialization {

// ============================================================================
// Концепт: Serializable Aggregate
// ============================================================================

/// Тип, который можно сериализовать через PFR (aggregate without bases).
export template<typename T>
concept SerializableAggregate =
    std::is_aggregate_v<T> &&
    !std::is_polymorphic_v<T> &&
    requires { boost::pfr::tuple_size_v<T>; };

// ============================================================================
// Glaze Metadata (для кастомных имён полей)
// ============================================================================

/// Макрос для определения Glaze metadata (опционально).
/// Использовать только если нужны кастомные имена или исключения.
#define PROJECTV_GLAZE_METADATA(Type, ...) \
    template<> \
    struct glz::meta<Type> { \
        using T = Type; \
        static constexpr auto value = __VA_ARGS__; \
    };

// ============================================================================
// JSON Serialization
// ============================================================================

/// Сериализует объект в JSON.
export template<SerializableAggregate T>
[[nodiscard]] auto to_json(T const& obj) noexcept
    -> std::expected<std::string, glz::error_ctx> {

    std::string buffer;
    auto err = glz::write_json(obj, buffer);

    if (err) {
        return std::unexpected(err);
    }
    return buffer;
}

/// Десериализует объект из JSON.
export template<SerializableAggregate T>
[[nodiscard]] auto from_json(std::string_view json) noexcept
    -> std::expected<T, glz::error_ctx> {

    T obj;
    auto err = glz::read_json(obj, json);

    if (err) {
        return std::unexpected(err);
    }
    return obj;
}

// ============================================================================
// Binary Serialization (более компактный)
// ============================================================================

/// Сериализует объект в бинарный формат Glaze.
export template<SerializableAggregate T>
[[nodiscard]] auto to_binary(T const& obj) noexcept
    -> std::expected<std::vector<std::byte>, glz::error_ctx> {

    std::vector<std::byte> buffer;
    auto err = glz::write_binary(obj, buffer);

    if (err) {
        return std::unexpected(err);
    }
    return buffer;
}

/// Десериализует объект из бинарного формата.
export template<SerializableAggregate T>
[[nodiscard]] auto from_binary(std::span<std::byte const> data) noexcept
    -> std::expected<T, glz::error_ctx> {

    T obj;
    auto err = glz::read_binary(obj, data);

    if (err) {
        return std::unexpected(err);
    }
    return obj;
}

// ============================================================================
// Field Counting и Introspection
// ============================================================================

/// Возвращает количество полей в aggregate типе.
export template<SerializableAggregate T>
constexpr auto field_count() noexcept -> size_t {
    return boost::pfr::tuple_size_v<T>;
}

/// Возвращает имя поля по индексу (compile-time через Glaze).
export template<SerializableAggregate T, size_t I>
constexpr auto field_name() noexcept -> std::string_view {
    // Glaze предоставляет имена полей автоматически
    return glz::name_v<T, I>;
}

/// Применяет функцию ко всем полям (compile-time iteration).
export template<SerializableAggregate T, typename F>
constexpr auto for_each_field(T const& obj, F&& func) noexcept -> void {
    boost::pfr::for_each_field(obj, std::forward<F>(func));
}

// ============================================================================
// Versioned Serialization
// ============================================================================

/// Заголовок сериализованных данных.
export struct alignas(8) SerializedHeader {
    uint32_t magic{0x50564D44};  // "PVMD"
    uint32_t version{1};
    uint32_t type_hash{0};       // typeid(T).hash_code()
    uint32_t data_size{0};
    uint64_t timestamp{0};
    uint64_t checksum{0};        // CRC64
};

/// Сериализует объект с версией.
export template<SerializableAggregate T>
[[nodiscard]] auto to_binary_versioned(
    T const& obj,
    uint32_t version = 1
) noexcept -> std::expected<std::vector<std::byte>, glz::error_ctx> {

    // Serialize data
    auto data = to_binary(obj);
    if (!data) {
        return std::unexpected(data.error());
    }

    // Build header
    SerializedHeader header{
        .magic = 0x50564D44,
        .version = version,
        .type_hash = static_cast<uint32_t>(typeid(T).hash_code()),
        .data_size = static_cast<uint32_t>(data->size()),
        .timestamp = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count()
        ),
        .checksum = 0  // TODO: compute CRC64
    };

    // Combine header + data
    std::vector<std::byte> result(sizeof(header) + data->size());
    std::memcpy(result.data(), &header, sizeof(header));
    std::memcpy(result.data() + sizeof(header), data->data(), data->size());

    return result;
}

} // namespace projectv::serialization
```

### 2.3 Примеры использования

```cpp
// Пример 1: Простая сериализация структуры

struct GameSettings {
    int resolution_x{1920};
    int resolution_y{1080};
    bool fullscreen{true};
    float volume_master{1.0f};
    float volume_music{0.8f};
    std::string language{"en"};
};

// Сериализация автоматически работает (Glaze + PFR)
auto settings = GameSettings{};

auto json = projectv::serialization::to_json(settings).value();
// {"resolution_x":1920,"resolution_y":1080,"fullscreen":true,...}

auto parsed = projectv::serialization::from_json<GameSettings>(json).value();

// Пример 2: Кастомные имена полей

struct ServerConfig {
    std::string host;
    uint16_t port;
    bool use_tls;
};

PROJECTV_GLAZE_METADATA(ServerConfig,
    glz::object(
        "host", &T::host,
        "port", &T::port,
        "useTLS", &T::use_tls  // CamelCase в JSON
    )
)

// Пример 3: Вложенные структуры

struct EntityData {
    uint64_t id;
    std::string name;
    glm::vec3 position;  // Нужна специализация для glm::vec3
};

// Специализация для glm::vec3
template<>
struct glz::meta<glm::vec3> {
    static constexpr auto value = glz::array(&glm::vec3::x, &glm::vec3::y, &glm::vec3::z);
};

// Пример 4: Introspection

struct PlayerState {
    int health;
    int mana;
    float x, y, z;
};

static_assert(projectv::serialization::field_count<PlayerState>() == 5);

// Iterate fields at runtime
PlayerState player{100, 50, 1.0f, 2.0f, 3.0f};
projectv::serialization::for_each_field(player, [](auto const& field, size_t index) {
    std::println("Field {}: {}", index, field);
});
```

---

## 3. PIMPL для stdexec

### 3.1 Проблема с stdexec

`std::execution` (P2300) имеет нестабильный ABI и частичную поддержку. Решение: **PIMPL (Pointer to Implementation)**
для изоляции.

### 3.2 Execution Context PIMPL

```cpp
// ProjectV.Execution.Context.cppm
export module ProjectV.Execution.Context;

import std;

// Forward declare implementation (defined in .cpp)
namespace projectv::execution::impl {
    struct ExecutionContextImpl;
}

export namespace projectv::execution {

// ============================================================================
// ExecutionContext — PIMPL wrapper для stdexec::run_thread_pool
// ============================================================================

/// Контекст выполнения для async операций.
/// Скрывает stdexec implementation details за стабильным ABI.
export class ExecutionContext {
public:
    /// Конфигурация.
    struct Config {
        uint32_t thread_count{std::thread::hardware_concurrency()};
        uint32_t max_tasks_queued{1024};
        bool pin_threads_to_cores{false};
    };

    /// Создаёт контекст.
    [[nodiscard]] static auto create(Config const& config = {}) noexcept
        -> std::expected<ExecutionContext, std::string>;

    /// Destructor (defined in .cpp for PIMPL).
    ~ExecutionContext() noexcept;

    // Move-only (PIMPL)
    ExecutionContext(ExecutionContext&&) noexcept;
    ExecutionContext& operator=(ExecutionContext&&) noexcept;
    ExecutionContext(ExecutionContext const&) = delete;
    ExecutionContext& operator=(ExecutionContext const&) = delete;

    /// Получает scheduler для enqueue задач.
    /// Возвращает opaque handle, который можно использовать с schedule().
    [[nodiscard]] auto scheduler() const noexcept -> void*;

    /// Запускает синхронную задачу.
    template<typename T>
    auto sync_wait(/* sender<T> */ void* sender) const noexcept
        -> std::expected<T, std::exception_ptr>;

    /// Возвращает количество потоков.
    [[nodiscard]] auto thread_count() const noexcept -> uint32_t;

    /// Запрашивает остановку.
    auto request_stop() noexcept -> void;

private:
    ExecutionContext() = default;

    // Opaque pointer to implementation
    std::unique_ptr<impl::ExecutionContextImpl> impl_;
};

// ============================================================================
// Async Operations (type-erased interface)
// ============================================================================

/// Type-erased async operation handle.
export struct AsyncOperation {
    uint64_t id{0};
    void* state{nullptr};

    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return state != nullptr;
    }
};

/// Создаёт async операцию.
export auto make_async_operation(
    ExecutionContext& ctx,
    std::invocable auto&& func
) noexcept -> AsyncOperation;

} // namespace projectv::execution

// ============================================================================
// Implementation (в отдельном .cpp файле)
// ============================================================================

// ProjectV.Execution.Context.cpp
module ProjectV.Execution.Context;

// Conditional include based on stdexec availability
#if __has_include(<execution>)
    #include <execution>
    #define PROJECTV_HAS_STDEXEC 1
#else
    #define PROJECTV_HAS_STDEXEC 0
#endif

namespace projectv::execution::impl {

#if PROJECTV_HAS_STDEXEC

struct ExecutionContextImpl {
    stdexec::static_thread_pool<4> pool;  // Inline buffer for 4 threads

    ExecutionContextImpl(uint32_t thread_count)
        : pool(thread_count) {}
};

#else

// Fallback: simple thread pool
struct ExecutionContextImpl {
    std::vector<std::jthread> threads;
    moodycamel::ConcurrentQueue<std::function<void()>> task_queue;
    std::atomic<bool> running{true};

    ExecutionContextImpl(uint32_t thread_count) {
        for (uint32_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([this] {
                while (running.load(std::memory_order::relaxed)) {
                    std::function<void()> task;
                    if (task_queue.try_dequeue(task)) {
                        task();
                    } else {
                        std::this_thread::yield();
                    }
                }
            });
        }
    }

    ~ExecutionContextImpl() {
        running.store(false);
    }
};

#endif

} // namespace projectv::execution::impl

namespace projectv::execution {

auto ExecutionContext::create(Config const& config) noexcept
    -> std::expected<ExecutionContext, std::string> {

    ExecutionContext ctx;
    try {
        ctx.impl_ = std::make_unique<impl::ExecutionContextImpl>(config.thread_count);
    } catch (std::exception const& e) {
        return std::unexpected(e.what());
    }
    return ctx;
}

ExecutionContext::~ExecutionContext() noexcept = default;
ExecutionContext::ExecutionContext(ExecutionContext&&) noexcept = default;
ExecutionContext& ExecutionContext::operator=(ExecutionContext&&) noexcept = default;

auto ExecutionContext::thread_count() const noexcept -> uint32_t {
    return impl_->pool.available_parallelism();
}

auto ExecutionContext::request_stop() noexcept -> void {
    // Implementation-specific
}

} // namespace projectv::execution
```

### 3.3 Sender/Receiver Wrapper

```cpp
// ProjectV.Execution.Sender.cppm
export module ProjectV.Execution.Sender;

import std;
import ProjectV.Execution.Context;

export namespace projectv::execution {

// ============================================================================
// Type-Erased Sender
// ============================================================================

/// Type-erased sender wrapper.
/// Скрывает сложные типы stdexec senders.
export template<typename T>
class Sender {
public:
    /// Концепт для sender-like типов.
    template<typename S>
    concept SenderLike = requires(S s) {
        { stdexec::sync_wait(s) } -> std::same_as<std::optional<T>>;
    };

    /// Создаёт из sender-like объекта.
    template<SenderLike S>
    Sender(S&& sender) {
        // Type-erased storage
        impl_ = std::make_unique<SenderImpl<std::remove_cvref_t<S>>>(
            std::forward<S>(sender)
        );
    }

    /// Синхронно ожидает результат.
    [[nodiscard]] auto sync_wait() const noexcept
        -> std::expected<T, std::exception_ptr> {

        if (!impl_) {
            return std::unexpected(std::make_exception_ptr(
                std::runtime_error("Empty sender")
            ));
        }

        return impl_->sync_wait();
    }

private:
    struct SenderBase {
        virtual ~SenderBase() = default;
        virtual auto sync_wait() const -> std::expected<T, std::exception_ptr> = 0;
    };

    template<typename S>
    struct SenderImpl : SenderBase {
        S sender;

        SenderImpl(S&& s) : sender(std::move(s)) {}

        auto sync_wait() const -> std::expected<T, std::exception_ptr> override {
            try {
#if PROJECTV_HAS_STDEXEC
                auto result = stdexec::sync_wait(sender);
                if (result) {
                    return *result;
                }
                return std::unexpected(std::make_exception_ptr(
                    std::runtime_error("Sync wait returned empty")
                ));
#else
                return std::unexpected(std::make_exception_ptr(
                    std::runtime_error("stdexec not available")
                ));
#endif
            } catch (...) {
                return std::unexpected(std::current_exception());
            }
        }
    };

    std::unique_ptr<SenderBase> impl_;
};

// ============================================================================
// Helper Functions
// ============================================================================

/// Создаёт sender из callable.
export template<std::invocable F>
[[nodiscard]] auto make_sender(F&& func) noexcept -> Sender<std::invoke_result_t<F>> {
#if PROJECTV_HAS_STDEXEC
    return Sender<std::invoke_result_t<F>>(
        stdexec::schedule(stdexec::get_scheduler()) |
        stdexec::then(std::forward<F>(func))
    );
#else
    // Fallback implementation
    // ...
#endif
}

} // namespace projectv::execution
```

---

## 4. Модульные Workarounds

### 4.1 Проблема с Modules

Проблемы:

1. Разные компиляторы имеют разные баги с modules
2. Build systems (CMake) имеют ограниченную поддержку
3. IDE indexing работает плохо

### 4.2 Гибридный подход

```cpp
// ProjectV.Config.hpp (header для IDE и cross-compiler)

#pragma once

// Feature detection
#if __cpp_modules >= 202207L && defined(__GNUC__)
    #define PROJECTV_USE_MODULES 1
#else
    #define PROJECTV_USE_MODULES 0
#endif

// Fallback includes для non-module builds
#if !PROJECTV_USE_MODULES
    #include <string>
    #include <vector>
    #include <span>
    #include <expected>
    #include <format>
    #include <chrono>
#endif

// ============================================================================
// Module Declaration
// ============================================================================

#if PROJECTV_USE_MODULES

// Module version
export module ProjectV.Config;

export import std;

#else

// Header version
// Всё уже включено выше

#endif

// ============================================================================
// Common Types (доступны в обоих режимах)
// ============================================================================

namespace projectv::config {

/// Версия движка.
struct Version {
    uint16_t major{0};
    uint16_t minor{1};
    uint16_t patch{0};
    uint16_t build{0};

    [[nodiscard]] auto to_string() const -> std::string {
        return std::format("{}.{}.{}.{}", major, minor, patch, build);
    }
};

/// Конфигурация сборки.
struct BuildConfig {
    bool debug{false};
    bool enable_tracy{false};
    bool enable_validation{false};
    Version version;
};

} // namespace projectv::config

// ============================================================================
// Export (для modules)
// ============================================================================

#if PROJECTV_USE_MODULES
export namespace projectv::config {
    // Уже экспортировано выше
}
#endif
```

### 4.3 CMake Integration

```cmake
# CMakeLists.txt

cmake_minimum_required(VERSION 3.28)

project(ProjectV LANGUAGES CXX)

# C++26 с fallback на C++23
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

# Проверка поддержки modules
include(CheckCXXSourceCompiles)

check_cxx_source_compiles("
    export module test;
    import std;
" PROJECTV_CXX_MODULES_SUPPORTED)

if(PROJECTV_CXX_MODULES_SUPPORTED)
    message(STATUS "C++26 Modules: Supported")
    set(PROJECTV_USE_MODULES ON)
else()
    message(STATUS "C++26 Modules: Not supported, using headers")
    set(PROJECTV_USE_MODULES OFF)
endif()

# ============================================================================
# Library
# ============================================================================

add_library(ProjectV)

if(PROJECTV_USE_MODULES)
    # Module sources
    define_source_files(ProjectV GLOB_RECURSE "src/*.cppm")

    # CMake 3.28+ module support
    target_sources(ProjectV PUBLIC FILE_SET CXX_MODULES FILES
        src/ProjectV.Config.cppm
        src/ProjectV.Serialization.cppm
        src/ProjectV.Execution.Context.cppm
        # ... more modules
    )
else()
    # Header sources
    target_sources(ProjectV PUBLIC FILE_SET HEADERS FILES
        src/ProjectV.Config.hpp
        src/ProjectV.Serialization.hpp
        src/ProjectV.Execution.Context.hpp
        # ... more headers
    )
endif()

# ============================================================================
# Dependencies
# ============================================================================

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)

# Glaze (header-only)
FetchContent_Declare(glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze
    GIT_TAG main
)
FetchContent_MakeAvailable(glaze)

# Boost.PFR (header-only)
FetchContent_Declare(boost_pfr
    GIT_REPOSITORY https://github.com/boostorg/pfr
    GIT_TAG develop
)
FetchContent_MakeAvailable(boost_pfr)

target_link_libraries(ProjectV PUBLIC
    Vulkan::Vulkan
    glm::glm
    glaze::glaze
)
```

---

## 5. Feature Detection Matrix

### 5.1 Runtime Detection

```cpp
// ProjectV.FeatureDetection.cppm
export module ProjectV.FeatureDetection;

import std;

export namespace projectv::features {

/// Результат detection.
struct FeatureStatus {
    bool available{false};
    bool experimental{false};
    std::string compiler_version;
    std::string message;
};

/// Проверяет поддержку features.
export auto check_features() -> std::unordered_map<std::string, FeatureStatus> {
    std::unordered_map<std::string, FeatureStatus> status;

    // Modules
    status["modules"] = {
#if PROJECTV_USE_MODULES
        .available = true,
#else
        .available = false,
#endif
        .experimental = false,
        .message = "C++26 modules"
    };

    // std::execution
    status["std_execution"] = {
#if PROJECTV_HAS_STDEXEC
        .available = true,
        .experimental = true,
        .message = "P2300 std::execution (partial)"
#else
        .available = false,
        .experimental = false,
        .message = "P2300 std::execution (not available)"
#endif
    };

    // std::expected
    status["std_expected"] = {
#if __cpp_expected >= 202211L
        .available = true,
#else
        .available = false,
#endif
        .experimental = false,
        .message = "std::expected"
    };

    // std::format
    status["std_format"] = {
#if __cpp_lib_format >= 202207L
        .available = true,
#else
        .available = false,
#endif
        .experimental = false,
        .message = "std::format"
    };

    return status;
}

} // namespace projectv::features
```

### 5.2 Compile-time Checks

```cpp
// Compile-time feature checks для conditional compilation

// Modules
#if __cpp_modules >= 202207L
    #define PROJECTV_MODULES_AVAILABLE 1
#else
    #define PROJECTV_MODULES_AVAILABLE 0
#endif

// std::execution (P2300)
#if __has_include(<execution>) && defined(__cpp_lib_execution)
    #define PROJECTV_STDEXEC_AVAILABLE 1
#else
    #define PROJECTV_STDEXEC_AVAILABLE 0
#endif

// std::expected
#if __cpp_lib_expected >= 202211L
    #define PROJECTV_EXPECTED_AVAILABLE 1
#else
    #define PROJECTV_EXPECTED_AVAILABLE 0
#endif

// std::format
#if __cpp_lib_format >= 202207L
    #define PROJECTV_FORMAT_AVAILABLE 1
#else
    #define PROJECTV_FORMAT_AVAILABLE 0
#endif

// Static Reflection (future)
#if __cpp_reflection >= 202400L
    #define PROJECTV_REFLECTION_AVAILABLE 1
#else
    #define PROJECTV_REFLECTION_AVAILABLE 0
#endif
```

---

## 6. Migration Path

### 6.1 Планы миграции

| Feature        | Current State | Migration Target      | Timeline           |
|----------------|---------------|-----------------------|--------------------|
| Serialization  | Glaze + PFR   | Static Reflection     | C++29              |
| Async Runtime  | PIMPL stdexec | Native std::execution | GCC 15+            |
| Modules        | Hybrid        | Full modules          | Clang 19+          |
| Linear Algebra | glm           | std::linalg           | C++26 full support |

### 6.2 Deprecation Strategy

```cpp
// Пример deprecation macro

#define PROJECTV_DEPRECATED(msg) [[deprecated(msg)]]

// Пример: помечаем старый API для удаления в будущем

namespace projectv::legacy {

/// @deprecated Use serialization::to_json instead
PROJECTV_DEPRECATED("Use serialization::to_json")
template<typename T>
auto serialize(T const& obj) -> std::string {
    return serialization::to_json(obj).value_or("");
}

} // namespace projectv::legacy
```

---

## Статус

| Компонент               | Статус         | Приоритет |
|-------------------------|----------------|-----------|
| Glaze + PFR Integration | Специфицирован | P0        |
| PIMPL for stdexec       | Специфицирован | P0        |
| Module Workarounds      | Специфицирован | P1        |
| Feature Detection       | Специфицирован | P1        |
| Migration Path          | Документирован | P2        |

---

## Ссылки

- [Glaze](https://github.com/stephenberry/glaze) — JSON/Binary serialization
- [Boost.PFR](https://github.com/boostorg/pfr) — Reflection for aggregates
- [P2300](https://wg21.link/p2300) — std::execution
- [P2996](https://wg21.link/p2996) — Static Reflection
- [C++26 Status](https://en.cppreference.com/w/cpp/compiler_support/26)
