# СТВ-CPP-001: Стандарт языка C++

**Идентификатор документа:** СТВ-CPP-001
**Версия:** 1.0.0
**Статус:** Утверждён
**Дата введения:** 22.02.2026
**Классификация:** Технический стандарт

---

## 1. Область применения

Настоящий стандарт определяет требования к использованию языка C++ в проекте ProjectV. Весь код на C++ ДОЛЖЕН
соответствовать данной спецификации.

---

## 2. Нормативные ссылки

- ISO/IEC 14882:2026 (C++26)
- C++ Core Guidelines (ISO C++ Foundation)
- СТВ-CMAKE-001: Спецификация системы сборки CMake

---

## 3. Версия стандарта

### 3.1 Обязательный стандарт

ProjectV устанавливает **C++26** в качестве обязательного стандарта языка. Использование более ранних стандартов НЕ
допускается.

### 3.2 Конфигурация компилятора

```cmake
# ОБЯЗАТЕЛЬНО: Стандарт C++26
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Строгий ISO C++, без расширений
```

### 3.3 Требуемые возможности C++26

| Возможность          | Статус        | Применение                |
|----------------------|---------------|---------------------------|
| Модули (`import`)    | Обязательно   | Организация кода          |
| Концепты (`concept`) | Обязательно   | Ограничения шаблонов      |
| `std::expected`      | Обязательно   | Обработка ошибок          |
| `std::optional`      | Обязательно   | Опциональные значения     |
| `std::span`          | Обязательно   | Невладеющие представления |
| `std::simd`          | Обязательно   | Векторизация              |
| Рефлексия (`^`)      | Рекомендуется | Метапрограммирование      |
| `std::format`        | Обязательно   | Форматирование строк      |

---

## 4. Модули C++26

### 4.1 Структура модуля

Все модули ДОЛЖНЫ следовать единой структуре именования:

```cpp
// Имя модуля: ProjectV.<Подсистема>[.<Компонент>]
export module ProjectV.Core.Memory;

import std;  // Стандартная библиотека как модуль

// Экспортируемые объявления
export namespace projectv::core::memory {

class Allocator;
class PoolAllocator;
class StackAllocator;

} // namespace projectv::core::memory
```

### 4.2 Именование модулей

| Уровень    | Пример                 | Назначение              |
|------------|------------------------|-------------------------|
| Корневой   | `ProjectV`             | Корневой модуль проекта |
| Подсистема | `ProjectV.Core`        | Основные подсистемы     |
| Компонент  | `ProjectV.Core.Memory` | Конкретные компоненты   |

### 4.3 Разделение интерфейса и реализации

**Интерфейсный файл (`.cppm`):**

```cpp
// ProjectV.Core.Memory.cppm
export module ProjectV.Core.Memory;

import std;

export namespace projectv::core::memory {

/// Интерфейс аллокатора
export class IAllocator {
public:
    virtual ~IAllocator() = default;

    [[nodiscard]] virtual auto allocate(std::size_t size, std::size_t alignment)
        -> void* = 0;
    virtual auto deallocate(void* ptr, std::size_t size) noexcept -> void = 0;
};

/// Фабрика аллокаторов
export class AllocatorFactory {
public:
    [[nodiscard]] static auto create_pool(std::size_t pool_size)
        -> std::unique_ptr<IAllocator>;
};

} // namespace projectv::core::memory
```

**Файл реализации (`.cpp`):**

```cpp
// ProjectV.Core.Memory.cpp
module ProjectV.Core.Memory;

import std;

namespace projectv::core::memory {

class PoolAllocator final : public IAllocator {
public:
    explicit PoolAllocator(std::size_t pool_size);
    ~PoolAllocator() override;

    [[nodiscard]] auto allocate(std::size_t size, std::size_t alignment)
        -> void* override;
    auto deallocate(void* ptr, std::size_t size) noexcept -> void override;

private:
    std::byte* pool_;
    std::size_t pool_size_;
    std::size_t offset_;
};

// Реализация методов...

auto AllocatorFactory::create_pool(std::size_t pool_size)
    -> std::unique_ptr<IAllocator> {
    return std::make_unique<PoolAllocator>(pool_size);
}

} // namespace projectv::core::memory
```

### 4.4 Запрещённые практики с модулями

1. `#include` директивы в интерфейсных файлах `.cppm` для C++ библиотек
2. Глобальные определения в файлах `.cppm`
3. Макросы, экспортируемые из модулей
4. Циклические зависимости между модулями

---

## 5. Обработка ошибок

### 5.1 Использование `std::expected`

Все функции, которые могут завершиться неудачей, ДОЛЖНЫ возвращать `std::expected`:

```cpp
export namespace projectv::core {

/// Результат операции создания ресурса
template<typename T>
using Result = std::expected<T, Error>;

/// Код ошибки
export enum class ErrorCode : uint32_t {
    Success = 0,
    InvalidArgument,
    OutOfMemory,
    ResourceNotFound,
    DeviceLost,
};

/// Информация об ошибке
export struct Error {
    ErrorCode code;
    std::string message;

    [[nodiscard]] auto operator bool() const noexcept -> bool {
        return code == ErrorCode::Success;
    }
};

/// Пример функции с обработкой ошибок
export [[nodiscard]] auto create_buffer(std::size_t size)
    -> Result<Buffer> {
    if (size == 0) {
        return std::unexpected(Error{
            .code = ErrorCode::InvalidArgument,
            .message = "Размер буфера должен быть больше нуля"
        });
    }

    auto* ptr = allocate(size);
    if (!ptr) {
        return std::unexpected(Error{
            .code = ErrorCode::OutOfMemory,
            .message = "Не удалось выделить память"
        });
    }

    return Buffer{ptr, size};
}

} // namespace projectv::core
```

### 5.2 Исключения

Исключения разрешены только в исключительных обстоятельствах:

| Ситуация            | Использовать     | Пример                     |
|---------------------|------------------|----------------------------|
| Ожидаемые ошибки    | `std::expected`  | Файловые операции, парсинг |
| Неожиданные ошибки  | `std::expected`  | Ошибки API                 |
| Неисправимые ошибки | `std::terminate` | Нарушение инвариантов      |
| Конструкторы        | Исключения       | Ошибки инициализации       |

### 5.3 Утверждения

```cpp
// Отладочные утверждения (только в Debug)
assert(condition && "Сообщение об ошибке");

// Внутренние проверки (всегда включены)
if (ptr == nullptr) [[unlikely]] {
    std::terminate();
}

// Контрактные проверки (C++26)
// Формально стандартизированы в C++26, но поддержка компилятора ограничена
```

---

## 6. Управление памятью

### 6.1 Запрещённые практики

1. `new` и `delete` — использовать аллокаторы
2. `malloc` и `free` — использовать аллокаторы
3. Сырые указатели для владения — использовать умные указатели
4. Ручное управление временем жизни — использовать RAII

### 6.2 Обязательные практики

```cpp
// ПРАВИЛЬНО: Умные указатели
auto ptr = std::make_unique<Resource>();
auto shared = std::make_shared<SharedResource>();

// ПРАВИЛЬНО: Пользовательские аллокаторы
auto buffer = allocator.allocate<byte>(size);

// ПРАВИЛЬНО: RAII
class FileHandle {
public:
    explicit FileHandle(std::string_view path);
    ~FileHandle() noexcept;  // Автоматическое закрытие

    // Запрет копирования
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    // Разрешение перемещения
    FileHandle(FileHandle&&) noexcept = default;
    FileHandle& operator=(FileHandle&&) noexcept = default;
};
```

### 6.3 Выравнивание памяти

Все типы данных ДОЛЖНЫ быть правильно выровнены:

```cpp
// Выравнивание для SIMD-операций
struct alignas(16) Vec4 {
    float x, y, z, w;
};

// Выравнивание для cache-line
struct alignas(64) CacheLineAligned {
    std::atomic<uint32_t> counter;
    char padding[60];  // Дополнение до 64 байт
};

// Проверка выравнивания
static_assert(alignof(Vec4) == 16);
static_assert(alignof(CacheLineAligned) == 64);
```

---

## 7. Стандартная библиотека

### 7.1 Обязательные контейнеры

| Контейнер       | Применение            | Альтернатива                         |
|-----------------|-----------------------|--------------------------------------|
| `std::vector`   | Динамические массивы  | `std::span` для представлений        |
| `std::array`    | Фиксированные массивы | —                                    |
| `std::string`   | Строки                | `std::string_view` для представлений |
| `std::optional` | Опциональные значения | —                                    |
| `std::expected` | Результаты с ошибками | —                                    |

### 7.2 Использование `std::span`

```cpp
// Функция, принимающая любое непрерывное представление
void process_data(std::span<const float> data);

// Вызов
std::vector<float> vec{1.0f, 2.0f, 3.0f};
std::array<float, 3> arr{1.0f, 2.0f, 3.0f};
float c_arr[3] = {1.0f, 2.0f, 3.0f};

process_data(vec);   // OK
process_data(arr);   // OK
process_data(c_arr); // OK
```

---

## 8. Стиль кодирования

### 8.1 Именование

| Элемент           | Стиль                  | Пример                   |
|-------------------|------------------------|--------------------------|
| Пространства имён | `snake_case`           | `projectv::core::memory` |
| Классы/структуры  | `PascalCase`           | `PoolAllocator`          |
| Функции           | `snake_case`           | `allocate_memory`        |
| Переменные        | `snake_case`           | `buffer_size`            |
| Константы         | `SCREAMING_SNAKE_CASE` | `MAX_BUFFER_SIZE`        |
| Члены классов     | `snake_case_`          | `pool_size_`             |

### 8.2 Форматирование

```cpp
// Максимальная длина строки: 100 символов
// Отступ: 4 пробела (без табуляции)

// Фигурные скобки на отдельной строке (Allman)
class Example
{
public:
    explicit Example(std::string_view name);

    [[nodiscard]] auto get_name() const noexcept
        -> std::string_view;

private:
    std::string name_;
};

// Длинные списки аргументов
auto result = create_buffer(
    size,
    alignment,
    BufferUsage::Vertex | BufferUsage::TransferDst
);
```

### 8.3 Автоматическое форматирование

Использовать `.clang-format`:

```yaml
# .clang-format
BasedOnStyle: LLVM
Language: Cpp
Standard: c++26
IndentWidth: 4
ColumnLimit: 100
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
NamespaceIndentation: None
PointerAlignment: Left
ReferenceAlignment: Left
SortIncludes: CaseSensitive
SortUsingDeclarations: true
```

---

## 9. Требования соответствия

### 9.1 Обязательные требования

1. Весь код ДОЛЖЕН компилироваться с `-std=c++26`
2. Все модули ДОЛЖНЫ использовать единое именование `ProjectV.*`
3. Все функции, которые могут завершиться неудачей, ДОЛЖНЫ возвращать `std::expected`
4. Все типы данных ДОЛЖНЫ быть правильно выровнены
5. Все файлы ДОЛЖНЫ проходить проверку `clang-format`

### 9.2 Запрещённые практики

1. `using namespace std;` в заголовочных файлах
2. Сырые `new`/`delete` для управления памятью
3. Си-стиль приведений `(type)expr`
4. `goto` (кроме обоснованных случаев)
5. Глобальные переменные с внешней связью

---

## 10. История редакций

| Версия | Дата       | Автор                 | Изменения                   |
|--------|------------|-----------------------|-----------------------------|
| 1.0.0  | 22.02.2026 | Архитектурная команда | Первоначальная спецификация |

---

## 11. Связанные документы

- [СТВ-CPP-002: Стандарт структуры кода](01_code-structure.md)
- [СТВ-CPP-003: Стандарт управления памятью](02_memory-management.md)
- [СТВ-CMAKE-001: Спецификация системы сборки CMake](../cmake/00_specification.md)
