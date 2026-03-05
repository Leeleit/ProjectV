# СТВ-CPP-002: Стандарт структуры кода

---

## 1. Область применения

Настоящий стандарт определяет требования к структуре и организации кода C++ в ProjectV. Все модули, классы и функции
ДОЛЖНЫ соответствовать данной спецификации.

---

## 2. Нормативные ссылки

- ISO/IEC 14882:2026 (C++26)
- C++ Core Guidelines
- СТВ-CPP-001: Стандарт языка C++

---

## 3. Организация файлов

### 3.1 Структура директорий

```
src/
├── core/                    # Ядро движка
│   ├── ProjectV.Core.cppm          # Главный интерфейс модуля
│   ├── ProjectV.Core.Memory.cppm   # Подмодуль памяти
│   ├── ProjectV.Core.Memory.cpp    # Реализация
│   ├── ProjectV.Core.Containers.cppm
│   ├── ProjectV.Core.Containers.cpp
│   └── ProjectV.Core.Math.cppm
├── render/                  # Подсистема рендеринга
│   ├── ProjectV.Render.cppm
│   ├── ProjectV.Render.Vulkan.cppm
│   └── ProjectV.Render.Vulkan.cpp
├── physics/                 # Физика
│   ├── ProjectV.Physics.cppm
│   └── ProjectV.Physics.cpp
├── ecs/                     # Entity Component System
│   ├── ProjectV.ECS.cppm
│   └── ProjectV.ECS.cpp
├── ui/                      # Пользовательский интерфейс
│   ├── ProjectV.UI.cppm
│   └── ProjectV.UI.cpp
└── app/                     # Приложение
    ├── ProjectV.App.cppm
    └── main.cpp             # Точка входа
```

### 3.2 Именование файлов

| Тип файла          | Расширение | Шаблон имени                               |
|--------------------|------------|--------------------------------------------|
| Интерфейс модуля   | `.cppm`    | `ProjectV.<Подсистема>[.<Компонент>].cppm` |
| Реализация модуля  | `.cpp`     | `ProjectV.<Подсистема>[.<Компонент>].cpp`  |
| Заголовок (legacy) | `.h`       | `<имя>.h` (только для C-совместимости)     |

---

## 4. Структура модуля

### 4.1 Главный файл модуля

```cpp
// ProjectV.Core.cppm
export module ProjectV.Core;

// Экспорт подмодулей
export module ProjectV.Core.Memory;
export module ProjectV.Core.Containers;
export module ProjectV.Core.Math;

// Экспорт общего пространства имён
export namespace projectv::core {
    // Общие типы и константы
}
```

### 4.2 Файл подмодуля

```cpp
// ProjectV.Core.Memory.cppm
export module ProjectV.Core.Memory;

import std;

// Предварительные объявления
export namespace projectv::core::memory {
    class IAllocator;
    class PoolAllocator;
    class StackAllocator;
}

// Экспорт реализации
export namespace projectv::core::memory {

/// Интерфейс аллокатора памяти
export class IAllocator
{
public:
    virtual ~IAllocator() = default;

    /// Выделить память
    [[nodiscard]] virtual auto allocate(
        std::size_t size,
        std::size_t alignment = alignof(std::max_align_t)
    ) -> void* = 0;

    /// Освободить память
    virtual auto deallocate(
        void* ptr,
        std::size_t size
    ) noexcept -> void = 0;
};

} // namespace projectv::core::memory
```

### 4.3 Файл реализации

```cpp
// ProjectV.Core.Memory.cpp
module ProjectV.Core.Memory;

import std;

// Внутренние вспомогательные функции
namespace {

constexpr std::size_t DEFAULT_ALIGNMENT = alignof(std::max_align_t);

auto align_up(std::size_t size, std::size_t alignment) -> std::size_t {
    return (size + alignment - 1) & ~(alignment - 1);
}

} // anonymous namespace

namespace projectv::core::memory {

// Реализация PoolAllocator
class PoolAllocator final : public IAllocator
{
public:
    explicit PoolAllocator(std::size_t pool_size)
        : pool_(static_cast<std::byte*>(std::aligned_alloc(64, pool_size)))
        , pool_size_(pool_size)
        , offset_(0)
    {
        if (!pool_) {
            throw std::bad_alloc();
        }
    }

    ~PoolAllocator() noexcept override {
        std::free(pool_);
    }

    [[nodiscard]] auto allocate(std::size_t size, std::size_t alignment)
        -> void* override
    {
        auto aligned_offset = align_up(offset_, alignment);
        auto new_offset = aligned_offset + size;

        if (new_offset > pool_size_) {
            return nullptr;
        }

        offset_ = new_offset;
        return pool_ + aligned_offset;
    }

    auto deallocate(void* ptr, std::size_t size) noexcept -> void override
    {
        // Stack allocator: dealloc не освобождает память
    }

private:
    std::byte* pool_;
    std::size_t pool_size_;
    std::size_t offset_;
};

} // namespace projectv::core::memory
```

---

## 5. Структура класса

### 5.1 Порядок разделов

```cpp
export class ExampleClass
{
    // 1. Публичные типы и константы
public:
    using value_type = int;
    static constexpr std::size_t DEFAULT_SIZE = 64;

    // 2. Конструкторы и деструктор
    explicit ExampleClass(std::size_t size = DEFAULT_SIZE);
    ExampleClass(const ExampleClass& other);
    ExampleClass(ExampleClass&& other) noexcept;
    ~ExampleClass() noexcept;

    // 3. Операторы присваивания
    auto operator=(const ExampleClass& other) -> ExampleClass&;
    auto operator=(ExampleClass&& other) noexcept -> ExampleClass&;

    // 4. Публичные методы
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto data() noexcept -> value_type*;
    [[nodiscard]] auto data() const noexcept -> const value_type*;

    // 5. Защищённые методы
protected:
    void reset() noexcept;

    // 6. Приватные методы
private:
    void initialize();
    void cleanup() noexcept;

    // 7. Члены данных (в конце)
    std::unique_ptr<value_type[]> data_;
    std::size_t size_;
};
```

### 5.2 Правило пяти/нуля

```cpp
// Правило нуля: класс не управляет ресурсами
class ValueObject
{
public:
    ValueObject() = default;

    // Компилятор автоматически генерирует:
    // - Конструктор копирования
    // - Оператор присваивания копированием
    // - Конструктор перемещения
    // - Оператор присваивания перемещением
    // - Деструктор

private:
    std::string name_;
    int value_{0};
};

// Правило пяти: класс управляет ресурсами
class ResourceOwner
{
public:
    explicit ResourceOwner(std::size_t size);
    ~ResourceOwner() noexcept;

    ResourceOwner(const ResourceOwner& other);
    auto operator=(const ResourceOwner& other) -> ResourceOwner&;

    ResourceOwner(ResourceOwner&& other) noexcept;
    auto operator=(ResourceOwner&& other) noexcept -> ResourceOwner&;

private:
    void* resource_;
    std::size_t size_;
};
```

---

## 6. Структура функции

### 6.1 Сигнатура функции

```cpp
/// Краткое описание функции
///
/// @param param1 Описание параметра 1
/// @param param2 Описание параметра 2
/// @return Описание возвращаемого значения
/// @throws Описание исключений (если есть)
[[nodiscard]] auto function_name(
    Type1 param1,
    Type2 param2
) -> ResultType;
```

### 6.2 Примеры функций

```cpp
/// Создать буфер вершин
///
/// @param size Размер буфера в байтах
/// @param usage Флаги использования буфера
/// @return Результат создания буфера или ошибку
[[nodiscard]] auto create_vertex_buffer(
    std::size_t size,
    BufferUsage usage
) -> std::expected<VertexBuffer, Error>;

/// Обработать данные
///
/// @param data Входные данные
/// @param output Выходной буфер
/// @return Количество обработанных элементов
auto process_data(
    std::span<const float> data,
    std::span<float> output
) -> std::size_t;
```

---

## 7. Пространства имён

### 7.1 Иерархия пространств имён

```
projectv::                    // Корневое пространство
├── core::                    // Ядро
│   ├── memory::             // Управление памятью
│   ├── containers::         // Контейнеры
│   └── math::               // Математика
├── render::                  // Рендеринг
│   └── vulkan::             // Vulkan-специфичное
├── physics::                 // Физика
├── ecs::                     // Entity Component System
└── ui::                      // Пользовательский интерфейс
```

### 7.2 Использование пространств имён

```cpp
// ПРАВИЛЬНО: Полное имя типа
auto allocator = projectv::core::memory::PoolAllocator{1024};

// ПРАВИЛЬНО: Сокращение в файле реализации
namespace pcm = projectv::core::memory;

auto allocator = pcm::PoolAllocator{1024};

// ЗАПРЕЩЕНО: using namespace в заголовках
// using namespace projectv::core::memory;  // ЗАПРЕЩЕНО

// ЗАПРЕЩЕНО: using namespace std
// using namespace std;  // ЗАПРЕЩЕНО
```

---

## 8. Заголовочные файлы

### 8.1 Структура заголовка

```cpp
// 1. Защита от повторного включения (для legacy-заголовков)
#pragma once

// 2. Включение стандартной библиотеки
#include <cstdint>
#include <memory>

// 3. Включение внешних библиотек
#include <vulkan/vulkan.h>

// 4. Включение внутренних заголовков
#include "projectv/core/export.h"

// 5. Пространство имён
namespace projectv::core {

// Объявления

} // namespace projectv::core
```

### 8.2 Заголовки для C-совместимости

```cpp
// projectv/core/export.h
#pragma once

#ifdef _WIN32
    #ifdef PROJECTV_CORE_EXPORTS
        #define PROJECTV_CORE_API __declspec(dllexport)
    #else
        #define PROJECTV_CORE_API __declspec(dllimport)
    #endif
#else
    #define PROJECTV_CORE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

PROJECTV_CORE_API void projectv_init();
PROJECTV_CORE_API void projectv_shutdown();

#ifdef __cplusplus
}
#endif
```

---

## 9. Требования соответствия

### 9.1 Обязательные требования

1. Все модули ДОЛЖНЫ следовать структуре `ProjectV.<Подсистема>[.<Компонент>]`
2. Все файлы реализации ДОЛЖНЫ иметь соответствующие интерфейсные файлы
3. Все классы ДОЛЖНЫ следовать правилу пяти или нулю
4. Все функции ДОЛЖНЫ иметь документацию в формате Doxygen

### 9.2 Запрещённые практики

1. Определения функций в заголовочных файлах (кроме шаблонов)
2. `using namespace` в заголовочных файлах
3. Глобальные `using`-объявления
4. Включение `<bits/*.h>` или других внутренних заголовков

---

## 10. История редакций

| Версия | Дата       | Автор                 | Изменения                   |
|--------|------------|-----------------------|-----------------------------|
| 1.0.0  | 22.02.2026 | Архитектурная команда | Первоначальная спецификация |

---

## 11. Связанные документы

- [СТВ-CPP-001: Стандарт языка C++](00_language-standard.md)
- [СТВ-CPP-003: Стандарт управления памятью](02_memory-management.md)
