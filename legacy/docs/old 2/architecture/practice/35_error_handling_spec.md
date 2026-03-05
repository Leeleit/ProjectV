# Спецификация системы логирования и обработки ошибок [🟢 Уровень 1]

**Статус:** Technical Specification
**Уровень:** 🟢 Фундаментальный
**Дата:** 2026-02-23
**Версия:** 1.0

---

## Обзор

Документ описывает архитектуру **Error Handling & Telemetry** для ProjectV. Движок использует политику **No Exceptions
** — вместо `throw/catch` используется `std::expected<T, E>`. Критически важно иметь строгую систему телеметрии для
понимания причин ошибок.

---

## 1. Философия обработки ошибок

### 1.1 Принципы

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Error Handling Philosophy                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. NO EXCEPTIONS                                                       │
│     - std::expected<T, E> вместо throw                                  │
│     - Явная обработка ошибок на каждом вызове                           │
│     - Предсказуемый control flow                                        │
│                                                                          │
│  2. FAIL FAST (для programming errors)                                  │
│     - Preconditions violations → assert → std::abort()                  │
│     - Invariants violations → fatal log → std::abort()                  │
│                                                                          │
│  3. RECOVER GRACEFULLY (для runtime errors)                             │
│     - File not found → fallback default → continue                      │
│     - GPU error → reset state → continue                                │
│                                                                          │
│  4. TELEMETRY FIRST                                                     │
│     - Каждая ошибка логируется с контекстом                             │
│     - Stack trace доступен всегда (где поддерживается)                  │
│     - Tracy integration для профилирования                              │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Категории ошибок

| Категория    | Примеры                                    | Действие                    |
|--------------|--------------------------------------------|-----------------------------|
| **Fatal**    | Out of Memory, Vulkan Device Lost          | `std::abort()`              |
| **Critical** | Shader compilation failed, Asset corrupted | Recovery attempt → fallback |
| **Warning**  | Missing optional resource, Deprecated API  | Log + continue              |
| **Info**     | Asset loaded, System initialized           | Log (debug builds)          |

---

## 2. Типы ошибок и Error Codes

### 2.1 Базовый тип ошибки

```cpp
// ProjectV.Core.Error.cppm
export module ProjectV.Core.Error;

import std;
import std.source_location;

export namespace projectv::core {

/// Категория ошибки.
export enum class ErrorCategory : uint8_t {
    Unknown = 0,
    Memory,         ///< Аллокация, VMA
    Vulkan,         ///< Vulkan API
    FileIO,         ///< Чтение/запись файлов
    Asset,          ///< Загрузка ассетов
    Physics,        ///< Jolt Physics
    ECS,            ///< Flecs ECS
    JobSystem,      ///< std::execution
    Network,        ///< Сеть
    Logic           ///< Программная ошибка (должна быть fatal)
};

/// Код ошибки.
export struct ErrorCode {
    ErrorCategory category{ErrorCategory::Unknown};
    uint16_t code{0};
    uint8_t severity{0};  // 0 = info, 1 = warning, 2 = critical, 3 = fatal

    [[nodiscard]] constexpr auto is_fatal() const noexcept -> bool {
        return severity >= 3;
    }

    [[nodiscard]] constexpr auto is_recoverable() const noexcept -> bool {
        return severity < 3;
    }
};

/// Полная информация об ошибке.
export struct ErrorInfo {
    ErrorCode error_code;
    std::string message;
    std::source_location location;
    std::stacktrace stacktrace;  // C++23, если поддерживается

    /// Timestamp в наносекундах от epoch.
    uint64_t timestamp{0};

    /// Thread ID.
    std::thread::id thread_id;

    /// Дополнительный контекст (key-value).
    std::vector<std::pair<std::string, std::string>> context;
};

/// Результат операции с ошибкой.
export template<typename T>
using Result = std::expected<T, ErrorInfo>;

/// Вспомогательный тип для void-результатов.
export using VoidResult = std::expected<void, ErrorInfo>;

} // namespace projectv::core
```

### 2.2 Категоризированные ошибки

```cpp
// ProjectV.Core.ErrorCodes.cppm
export module ProjectV.Core.ErrorCodes;

import std;
import ProjectV.Core.Error;

export namespace projectv::core::errors {

// === Memory Errors ===
export constexpr ErrorCode OUT_OF_MEMORY{ErrorCategory::Memory, 1, 3};  // Fatal
export constexpr ErrorCode ALLOCATION_FAILED{ErrorCategory::Memory, 2, 2};
export constexpr ErrorCode POOL_EXHAUSTED{ErrorCategory::Memory, 3, 2};

// === Vulkan Errors ===
export constexpr ErrorCode VULKAN_INSTANCE_FAILED{ErrorCategory::Vulkan, 1, 3};
export constexpr ErrorCode VULKAN_DEVICE_FAILED{ErrorCategory::Vulkan, 2, 3};
export constexpr ErrorCode VULKAN_OUT_OF_MEMORY{ErrorCategory::Vulkan, 3, 3};
export constexpr ErrorCode VULKAN_DEVICE_LOST{ErrorCategory::Vulkan, 4, 3};
export constexpr ErrorCode VULKAN_SWAPCHAIN_FAILED{ErrorCategory::Vulkan, 5, 2};
export constexpr ErrorCode VULKAN_SHADER_COMPILE_FAILED{ErrorCategory::Vulkan, 6, 2};
export constexpr ErrorCode VULKAN_PIPELINE_FAILED{ErrorCategory::Vulkan, 7, 2};

// === FileIO Errors ===
export constexpr ErrorCode FILE_NOT_FOUND{ErrorCategory::FileIO, 1, 1};
export constexpr ErrorCode FILE_READ_ERROR{ErrorCategory::FileIO, 2, 2};
export constexpr ErrorCode FILE_WRITE_ERROR{ErrorCategory::FileIO, 3, 2};
export constexpr ErrorCode FILE_CORRUPTED{ErrorCategory::FileIO, 4, 2};

// === Asset Errors ===
export constexpr ErrorCode ASSET_LOAD_FAILED{ErrorCategory::Asset, 1, 2};
export constexpr ErrorCode ASSET_PARSE_FAILED{ErrorCategory::Asset, 2, 2};
export constexpr ErrorCode ASSET_VERSION_MISMATCH{ErrorCategory::Asset, 3, 1};

// === Job System Errors ===
export constexpr ErrorCode JOB_SCHEDULE_FAILED{ErrorCategory::JobSystem, 1, 2};
export constexpr ErrorCode JOB_TIMEOUT{ErrorCategory::JobSystem, 2, 1};
export constexpr ErrorCode JOB_CANCELLED{ErrorCategory::JobSystem, 3, 0};

// === Logic Errors (Fatal) ===
export constexpr ErrorCode PRECONDITION_VIOLATION{ErrorCategory::Logic, 1, 3};
export constexpr ErrorCode INVARIANT_VIOLATION{ErrorCategory::Logic, 2, 3};
export constexpr ErrorCode NULL_POINTER{ErrorCategory::Logic, 3, 3};
export constexpr ErrorCode OUT_OF_BOUNDS{ErrorCategory::Logic, 4, 3};

} // namespace projectv::core::errors
```

---

## 3. Макросы и функции логирования

### 3.1 Основные макросы

```cpp
// ProjectV.Core.Diagnostics.cppm
export module ProjectV.Core.Diagnostics;

import std;
import std.source_location;
import ProjectV.Core.Error;

export namespace projectv::core {

/// Создаёт ErrorInfo из кода ошибки и текущего контекста.
export auto make_error(
    ErrorCode code,
    std::string_view message,
    std::source_location loc = std::source_location::current()
) noexcept -> ErrorInfo;

/// Логирует ошибку.
export auto log_error(ErrorInfo const& error) noexcept -> void;

/// Логирует warning.
export auto log_warning(
    std::string_view message,
    std::source_location loc = std::source_location::current()
) noexcept -> void;

/// Логирует info (только в debug builds).
export auto log_info(
    std::string_view message,
    std::source_location loc = std::source_location::current()
) noexcept -> void;

/// Логирует fatal error и вызывает std::abort().
[[noreturn]] export auto log_fatal(
    ErrorInfo const& error
) noexcept -> void;

} // namespace projectv::core
```

### 3.2 Удобные макросы

```cpp
// ProjectV.Core.Macros.cppm
export module ProjectV.Core.Macros;

import std;
import ProjectV.Core.Error;
import ProjectV.Core.Diagnostics;

// === Проверка предусловий ===

/// Проверяет предусловие. При нарушении → fatal.
#define PV_PRECONDITION(cond, message)                                          \
    do {                                                                        \
        if (!(cond)) [[unlikely]] {                                             \
            ::projectv::core::log_fatal(                                        \
                ::projectv::core::make_error(                                   \
                    ::projectv::core::errors::PRECONDITION_VIOLATION,           \
                    message,                                                    \
                    std::source_location::current()                             \
                )                                                               \
            );                                                                  \
        }                                                                       \
    } while (0)

/// Проверяет инвариант. При нарушении → fatal.
#define PV_INVARIANT(cond, message)                                             \
    do {                                                                        \
        if (!(cond)) [[unlikely]] {                                             \
            ::projectv::core::log_fatal(                                        \
                ::projectv::core::make_error(                                   \
                    ::projectv::core::errors::INVARIANT_VIOLATION,              \
                    message,                                                    \
                    std::source_location::current()                             \
                )                                                               \
            );                                                                  \
        }                                                                       \
    } while (0)

// === Обработка std::expected ===

/// Пытается извлечь значение из expected. При ошибке → возвращает ошибку.
#define PV_TRY(expr)                                                            \
    ({                                                                          \
        auto&& _result = (expr);                                                \
        if (!_result) [[unlikely]] {                                            \
            return std::unexpected(std::move(_result.error()));                 \
        }                                                                       \
        std::move(*_result);                                                    \
    })

/// Пытается извлечь значение из expected. При ошибке → возвращает дефолт.
#define PV_TRY_OR(expr, default_value)                                          \
    ({                                                                          \
        auto&& _result = (expr);                                                \
        if (!_result) [[unlikely]] {                                            \
            ::projectv::core::log_error(_result.error());                       \
            return (default_value);                                             \
        }                                                                       \
        std::move(*_result);                                                    \
    })

/// Пытается извлечь значение из expected. При ошибке → игнорирует.
#define PV_TRY_IGNORE(expr)                                                     \
    ({                                                                          \
        auto&& _result = (expr);                                                \
        if (!_result) [[unlikely]] {                                            \
            ::projectv::core::log_error(_result.error());                       \
        }                                                                       \
        _result.has_value() ? std::optional{std::move(*_result)} : std::nullopt;\
    })

// === Логирование ===

/// Логирует ошибку с контекстом.
#define PV_LOG_ERROR(code, message)                                             \
    ::projectv::core::log_error(                                                \
        ::projectv::core::make_error(                                           \
            code,                                                               \
            message,                                                            \
            std::source_location::current()                                     \
        )                                                                       \
    )

/// Логирует warning.
#define PV_LOG_WARNING(message)                                                 \
    ::projectv::core::log_warning(message, std::source_location::current())

/// Логирует info (debug only).
#ifndef NDEBUG
#define PV_LOG_INFO(message)                                                    \
    ::projectv::core::log_info(message, std::source_location::current())
#else
#define PV_LOG_INFO(message) ((void)0)
#endif

/// Фатальная ошибка с abort.
#define PV_FATAL(code, message)                                                 \
    ::projectv::core::log_fatal(                                                \
        ::projectv::core::make_error(                                           \
            code,                                                               \
            message,                                                            \
            std::source_location::current()                                     \
        )                                                                       \
    )
```

### 3.3 Примеры использования

```cpp
// Пример 1: Функция с возвращаемой ошибкой
auto load_texture(std::string_view path) noexcept
    -> Result<Texture> {

    auto file = PV_TRY(read_file(path));

    if (!is_valid_texture(file)) {
        return std::unexpected(make_error(
            errors::ASSET_PARSE_FAILED,
            std::format("Invalid texture format: {}", path)
        ));
    }

    return parse_texture(file);
}

// Пример 2: Предусловия
auto get_chunk(int32_t x, int32_t y, int32_t z) noexcept
    -> Chunk* {

    PV_PRECONDITION(
        x >= 0 && y >= 0 && z >= 0,
        std::format("Invalid chunk coordinates: ({}, {}, {})", x, y, z)
    );

    PV_INVARIANT(
        chunks_.contains(key(x, y, z)),
        "Chunk must exist in map"
    );

    return &chunks_.at(key(x, y, z));
}

// Пример 3: Обработка Vulkan ошибок
auto create_buffer(VkDeviceSize size) noexcept
    -> Result<VkBuffer> {

    VkBuffer buffer;
    VkResult result = vkCreateBuffer(device_, &info, nullptr, &buffer);

    if (result != VK_SUCCESS) {
        PV_LOG_ERROR(
            errors::VULKAN_OUT_OF_MEMORY,
            std::format("vkCreateBuffer failed: {}", vk_result_to_string(result))
        );
        return std::unexpected(make_error(
            errors::VULKAN_OUT_OF_MEMORY,
            "Buffer creation failed"
        ));
    }

    return buffer;
}
```

---

## 4. Интеграция с Tracy Profiler

### 4.1 Tracy Logging

```cpp
// ProjectV.Core.Tracy.cppm
export module ProjectV.Core.Tracy;

import std;

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

export namespace projectv::core {

/// Уровень логирования для Tracy.
export enum class TracyLogLevel : uint8_t {
    Info,
    Warning,
    Error,
    Fatal
};

/// Логирует сообщение в Tracy.
export auto tracy_log(
    TracyLogLevel level,
    std::string_view message
) noexcept -> void {
#ifdef TRACY_ENABLE
    switch (level) {
        case TracyLogLevel::Info:
            TracyMessageL(message.data());
            break;
        case TracyLogLevel::Warning:
            TracyMessageLC(message.data(), tracy::Color::Yellow);
            break;
        case TracyLogLevel::Error:
            TracyMessageLC(message.data(), tracy::Color::Red);
            break;
        case TracyLogLevel::Fatal:
            TracyMessageLC(message.data(), tracy::Color::Red4);
            break;
    }
#else
    (void)level;
    (void)message;
#endif
}

/// Named zone для трассировки.
#ifdef TRACY_ENABLE
#define PV_ZONE(name) ZoneScopedN(name)
#define PV_FRAME_MARK() FrameMark
#else
#define PV_ZONE(name) ((void)0)
#define PV_FRAME_MARK() ((void)0)
#endif

} // namespace projectv::core
```

### 4.2 Интеграция логов

```cpp
// ProjectV.Core.Diagnostics.cpp
module ProjectV.Core.Diagnostics;

import std;
import std.source_location;
import ProjectV.Core.Error;
import ProjectV.Core.Tracy;

namespace projectv::core {

auto log_error(ErrorInfo const& error) noexcept -> void {
    // 1. Форматирование сообщения
    std::string formatted = std::format(
        "[ERROR] {}:{}:{}: {} (code: {}:{})",
        error.location.file_name(),
        error.location.line(),
        error.location.function_name(),
        error.message,
        static_cast<int>(error.error_code.category),
        error.error_code.code
    );

    // 2. Console output
    std::fprintf(stderr, "%s\n", formatted.c_str());

    // 3. Tracy output
    tracy_log(TracyLogLevel::Error, formatted);

    // 4. File log (если включён)
    if (log_file_.is_open()) {
        log_file_ << formatted << std::endl;
    }
}

auto log_fatal(ErrorInfo const& error) noexcept -> void {
    // Полная информация перед abort
    std::string formatted = std::format(
        "[FATAL] {}:{}:{}: {}\n"
        "Category: {}\n"
        "Code: {}\n"
        "Thread: {}\n"
        "Stack trace:\n{}",
        error.location.file_name(),
        error.location.line(),
        error.location.function_name(),
        error.message,
        static_cast<int>(error.error_code.category),
        error.error_code.code,
        error.thread_id,
        // Stack trace (если поддерживается)
        #ifdef __cpp_lib_stacktrace
        std::to_string(error.stacktrace)
        #else
        "(not available)"
        #endif
    );

    // Console
    std::fprintf(stderr, "%s\n", formatted.c_str());

    // Tracy
    tracy_log(TracyLogLevel::Fatal, formatted);

    // Flush everything
    std::fflush(stderr);
    if (log_file_.is_open()) {
        log_file_ << formatted << std::endl;
        log_file_.flush();
    }

    // Abort
    std::abort();
}

} // namespace projectv::core
```

---

## 5. Политика Fatal vs Recoverable

### 5.1 Диаграмма принятия решений

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Error Decision Tree                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                         ┌─────────────┐                                 │
│                         │   Error     │                                 │
│                         └──────┬──────┘                                 │
│                                │                                         │
│                                ▼                                         │
│                    ┌───────────────────────┐                            │
│                    │ Is Precondition/      │                            │
│                    │ Invariant Violation?  │                            │
│                    └───────────┬───────────┘                            │
│                          │           │                                  │
│                         YES          NO                                  │
│                          │           │                                  │
│                          ▼           ▼                                  │
│                   ┌──────────┐  ┌───────────────────┐                   │
│                   │  FATAL   │  │ Is Resource      │                   │
│                   │  abort() │  │ Exhausted?       │                   │
│                   └──────────┘  └─────────┬─────────┘                   │
│                                           │                             │
│                                    YES    │    NO                        │
│                                          │                              │
│                             ┌────────────┴────────────┐                 │
│                             ▼                         ▼                 │
│                    ┌────────────────┐       ┌─────────────────┐         │
│                    │ Can Fallback?  │       │ Log & Continue  │         │
│                    └───────┬────────┘       └─────────────────┘         │
│                            │                                             │
│                     YES    │    NO                                        │
│                            │                                              │
│                 ┌──────────┴──────────┐                                  │
│                 ▼                     ▼                                  │
│        ┌────────────────┐     ┌────────────────┐                        │
│        │ Use Fallback   │     │   FATAL        │                        │
│        │ Log Warning    │     │   abort()      │                        │
│        └────────────────┘     └────────────────┘                        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Классификация по категориям

```cpp
// ProjectV.Core.ErrorPolicy.cppm
export module ProjectV.Core.ErrorPolicy;

import std;
import ProjectV.Core.Error;

export namespace projectv::core {

/// Определяет политику обработки ошибки.
export constexpr auto error_policy(ErrorCode code) noexcept -> enum class Policy {
    Abort,          // Немедленный abort()
    Recover,        // Попытка восстановления
    LogAndContinue, // Лог + продолжение
    Silent          // Тихая обработка
};

/// Таблица политик.
export constexpr auto get_policy(ErrorCode code) noexcept -> Policy {
    // === Всегда FATAL ===
    if (code.category == ErrorCategory::Logic) {
        return Policy::Abort;
    }

    // === Memory ===
    if (code.category == ErrorCategory::Memory) {
        if (code.code == 1) return Policy::Abort;  // OUT_OF_MEMORY
        return Policy::Recover;
    }

    // === Vulkan ===
    if (code.category == ErrorCategory::Vulkan) {
        switch (code.code) {
            case 1:  // INSTANCE_FAILED
            case 2:  // DEVICE_FAILED
            case 3:  // OUT_OF_MEMORY
            case 4:  // DEVICE_LOST
                return Policy::Abort;
            case 5:  // SWAPCHAIN_FAILED
                return Policy::Recover;  // Recreate swapchain
            case 6:  // SHADER_COMPILE_FAILED
            case 7:  // PIPELINE_FAILED
                return Policy::LogAndContinue;  // Fallback shader
        }
    }

    // === FileIO ===
    if (code.category == ErrorCategory::FileIO) {
        if (code.code == 1) return Policy::LogAndContinue;  // NOT_FOUND
        return Policy::Recover;
    }

    // === Asset ===
    if (code.category == ErrorCategory::Asset) {
        return Policy::LogAndContinue;  // Всегда fallback
    }

    // === Job System ===
    if (code.category == ErrorCategory::JobSystem) {
        if (code.code == 3) return Policy::Silent;  // CANCELLED - норма
        return Policy::LogAndContinue;
    }

    return Policy::LogAndContinue;
}

/// Обрабатывает ошибку согласно политике.
/// Возвращает true если можно продолжить, false если нужен abort.
export auto handle_error(ErrorInfo const& error) noexcept -> bool {
    auto policy = get_policy(error.error_code);

    switch (policy) {
        case Policy::Abort:
            log_fatal(error);
            return false;  // Never reached

        case Policy::Recover:
            log_error(error);
            // Попытка recovery выполняется вызывающим кодом
            return true;

        case Policy::LogAndContinue:
            log_error(error);
            return true;

        case Policy::Silent:
            return true;
    }

    return true;
}

} // namespace projectv::core
```

### 5.3 Примеры политик

| Категория | Код | Ошибка                 | Политика       | Действие            |
|-----------|-----|------------------------|----------------|---------------------|
| Logic     | 1   | PRECONDITION_VIOLATION | Abort          | `std::abort()`      |
| Memory    | 1   | OUT_OF_MEMORY          | Abort          | `std::abort()`      |
| Vulkan    | 3   | VULKAN_OUT_OF_MEMORY   | Abort          | `std::abort()`      |
| Vulkan    | 4   | VULKAN_DEVICE_LOST     | Abort          | `std::abort()`      |
| Vulkan    | 5   | SWAPCHAIN_FAILED       | Recover        | Recreate swapchain  |
| Vulkan    | 6   | SHADER_COMPILE_FAILED  | LogAndContinue | Fallback shader     |
| FileIO    | 1   | FILE_NOT_FOUND         | LogAndContinue | Use default asset   |
| Asset     | 1   | ASSET_LOAD_FAILED      | LogAndContinue | Use placeholder     |
| JobSystem | 3   | JOB_CANCELLED          | Silent         | Normal cancellation |

---

## 6. Конфигурация логирования

### 6.1 LogLevel

```cpp
// ProjectV.Core.LogConfig.cppm
export module ProjectV.Core.LogConfig;

import std;

export namespace projectv::core {

/// Уровень логирования.
export enum class LogLevel : uint8_t {
    None = 0,      // Отключено
    Fatal = 1,     // Только фатальные
    Error = 2,     // Ошибки и фатальные
    Warning = 3,   // Warnings + ошибки + фатальные
    Info = 4,      // Все кроме debug
    Debug = 5,     // Всё
    Trace = 6      // Всё + trace
};

/// Конфигурация логирования.
export struct LogConfig {
    LogLevel min_level{LogLevel::Info};
    bool log_to_console{true};
    bool log_to_file{false};
    bool log_to_tracy{true};
    std::filesystem::path log_file_path{"projectv.log"};
    bool include_timestamp{true};
    bool include_thread_id{true};
    bool include_source_location{true};
    bool include_stack_trace{false};  // Только для fatal
};

/// Глобальная конфигурация.
export auto configure_logging(LogConfig const& config) noexcept -> void;
export auto current_log_config() noexcept -> LogConfig const&;

} // namespace projectv::core
```

---

## 7. Пример полного использования

```cpp
// Пример функции с полной обработкой ошибок
auto render_frame(VulkanRenderer& renderer) noexcept
    -> VoidResult {

    PV_ZONE("render_frame");

    // 1. Begin frame
    auto begin_result = renderer.begin_frame();
    if (!begin_result) {
        auto error = begin_result.error();

        // Определяем политику
        if (!handle_error(error)) {
            // Fatal - не достигнется, handle_error уже вызвал abort
            return std::unexpected(error);
        }

        // Recover - пробуем продолжить
        return {};  // Skip frame
    }

    // 2. Render chunks
    for (auto& chunk : visible_chunks_) {
        auto render_result = render_chunk(renderer, chunk);
        PV_TRY_IGNORE(render_result);  // Log error, continue
    }

    // 3. End frame
    auto end_result = renderer.end_frame();
    if (!end_result) {
        PV_LOG_ERROR(
            errors::VULKAN_SWAPCHAIN_FAILED,
            "Failed to present frame"
        );
        // Попытка recreate swapchain
        renderer.recreate_swapchain();
    }

    PV_FRAME_MARK();
    return {};
}
```

---

## Статус

| Компонент                      | Статус         | Приоритет |
|--------------------------------|----------------|-----------|
| ErrorInfo/ErrorCode            | Специфицирован | P0        |
| Макросы PV_TRY/PV_PRECONDITION | Специфицирован | P0        |
| Tracy Integration              | Специфицирован | P0        |
| Error Policy                   | Специфицирован | P1        |
| LogConfig                      | Специфицирован | P2        |

---

## Ссылки

- [09_testing-philosophy.md](../../philosophy/09_testing-philosophy.md)
- [37_shutdown_sequence.md](./37_shutdown_sequence.md)
- [Tracy Profiler](https://github.com/wolfpld/tracy)
- [C++23 std::expected](https://en.cppreference.com/w/cpp/utility/expected)
- [C++23 std::stacktrace](https://en.cppreference.com/w/cpp/utility/stacktrace)
