# СТВ-CPP-003: Стандарт управления памятью

---

## 1. Область применения

Настоящий стандарт определяет требования к управлению памятью в ProjectV. Все операции выделения и освобождения памяти
ДОЛЖНЫ соответствовать данной спецификации.

---

## 3. Принципы управления памятью

### 3.1 Основные принципы

1. **RAII (Resource Acquisition Is Initialization)**: Время жизни ресурса привязано к времени жизни объекта
2. **Единственная ответственность**: Каждый ресурс имеет единственного владельца
3. **Явное владение**: Владение памятью выражается через типы
4. **Безопасность исключений**: Освобождение ресурсов гарантируется при исключениях

### 3.2 Категории памяти

| Категория        | Назначение                              | Аллокатор         |
|------------------|-----------------------------------------|-------------------|
| Стек             | Локальные переменные, временные объекты | Автоматический    |
| Куча (общая)     | Долгоживущие объекты                    | `SystemAllocator` |
| Пул              | Однотипные объекты                      | `PoolAllocator`   |
| Стек (кастомный) | Временные выделения                     | `StackAllocator`  |
| GPU              | Ресурсы Vulkan                          | VMA               |

---

## 4. Интерфейс аллокатора

### 4.1 Базовый интерфейс

```cpp
export module ProjectV.Core.Memory;

import std;

export namespace projectv::core::memory {

/// Результат выделения памяти
struct AllocationResult
{
    void* ptr;
    std::size_t actual_size;
    std::size_t alignment;
};

/// Интерфейс аллокатора памяти
export class IAllocator
{
public:
    virtual ~IAllocator() = default;

    /// Выделить память
    /// @param size Размер в байтах
    /// @param alignment Выравнивание (по умолчанию alignof(std::max_align_t))
    /// @return Указатель на выделенную память или nullptr
    [[nodiscard]] virtual auto allocate(
        std::size_t size,
        std::size_t alignment = alignof(std::max_align_t)
    ) -> void* = 0;

    /// Освободить память
    /// @param ptr Указатель на память
    /// @param size Размер (для аллокаторов с отслеживанием размера)
    virtual auto deallocate(
        void* ptr,
        std::size_t size
    ) noexcept -> void = 0;

    /// Выделить память с результатом
    [[nodiscard]] auto allocate_aligned(
        std::size_t size,
        std::size_t alignment
    ) -> std::expected<AllocationResult, Error>;

    /// Получить статистику использования
    struct Stats {
        std::size_t total_allocated;
        std::size_t total_freed;
        std::size_t current_usage;
        std::size_t peak_usage;
    };

    [[nodiscard]] virtual auto get_stats() const noexcept -> Stats = 0;
};

} // namespace projectv::core::memory
```

### 4.2 Стандартные аллокаторы

```cpp
export module ProjectV.Core.Memory;

import std;

export namespace projectv::core::memory {

/// Системный аллокатор (обёртка над malloc/free)
export class SystemAllocator final : public IAllocator
{
public:
    SystemAllocator() = default;
    ~SystemAllocator() noexcept override = default;

    [[nodiscard]] auto allocate(std::size_t size, std::size_t alignment)
        -> void* override
    {
        return std::aligned_alloc(alignment, size);
    }

    auto deallocate(void* ptr, std::size_t size) noexcept -> void override
    {
        std::free(ptr);
    }

    [[nodiscard]] auto get_stats() const noexcept -> Stats override
    {
        return Stats{};
    }
};

/// Пуловый аллокатор для однотипных объектов
export template<typename T, std::size_t PoolSize = 1024>
class ObjectPool final
{
public:
    ObjectPool();
    ~ObjectPool() noexcept;

    template<typename... Args>
    [[nodiscard]] auto create(Args&&... args) -> T*;

    auto destroy(T* ptr) noexcept -> void;

    [[nodiscard]] auto capacity() const noexcept -> std::size_t;
    [[nodiscard]] auto available() const noexcept -> std::size_t;

private:
    struct Block {
        alignas(T) std::byte storage[sizeof(T)];
        Block* next;
    };

    Block* free_list_;
    std::unique_ptr<Block[]> blocks_;
    std::size_t capacity_;
};

/// Стековый аллокатор для временных выделений
export class StackAllocator final : public IAllocator
{
public:
    explicit StackAllocator(std::size_t size);
    ~StackAllocator() noexcept override;

    /// Получить маркер для отката
    [[nodiscard]] auto get_marker() const noexcept -> std::size_t;

    /// Откатить allocations до маркера
    auto rewind_to_marker(std::size_t marker) noexcept -> void;

    /// Сбросить весь аллокатор
    auto reset() noexcept -> void;

private:
    std::byte* buffer_;
    std::size_t size_;
    std::size_t offset_;
};

} // namespace projectv::core::memory
```

---

## 5. Выравнивание памяти

### 5.1 Стандартные выравнивания

| Тип данных         | Выравнивание | Обоснование                               |
|--------------------|--------------|-------------------------------------------|
| `std::max_align_t` | 16 байт      | Максимальное фундаментальное выравнивание |
| SIMD (SSE/AVX)     | 16/32 байта  | Требования векторных инструкций           |
| AVX-512            | 64 байта     | Требования 512-битных регистров           |
| Cache line         | 64 байта     | Предотвращение False Sharing              |
| Страница памяти    | 4096 байт    | Границы виртуальной памяти                |

### 5.2 Функции выравнивания

```cpp
export namespace projectv::core::memory {

/// Выровнять размер вверх
[[nodiscard]] constexpr auto align_up(
    std::size_t size,
    std::size_t alignment
) noexcept -> std::size_t
{
    return (size + alignment - 1) & ~(alignment - 1);
}

/// Проверить выравнивание указателя
[[nodiscard]] constexpr auto is_aligned(
    const void* ptr,
    std::size_t alignment
) noexcept -> bool
{
    return (reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1)) == 0;
}

/// Получить выравнивание для типа
template<typename T>
[[nodiscard]] constexpr auto alignment_for() noexcept -> std::size_t
{
    return alignof(T);
}

/// Выравнивание для SIMD-операций
[[nodiscard]] constexpr auto simd_alignment() noexcept -> std::size_t
{
    return 32;  // AVX2
}

/// Выравнивание для cache-line
[[nodiscard]] constexpr auto cache_line_alignment() noexcept -> std::size_t
{
    return 64;
}

} // namespace projectv::core::memory
```

### 5.3 Выровненные типы

```cpp
export namespace projectv::core::math {

/// Выровненный 4-компонентный вектор
export struct alignas(16) Vec4
{
    float x, y, z, w;

    [[nodiscard]] auto data() noexcept -> float* { return &x; }
    [[nodiscard]] auto data() const noexcept -> const float* { return &x; }
};

/// Выровненная 4x4 матрица
export struct alignas(64) Mat4
{
    float m[16];
};

static_assert(alignof(Vec4) == 16, "Vec4 должен быть выровнен по 16 байт");
static_assert(alignof(Mat4) == 64, "Mat4 должен быть выровнен по 64 байта");

} // namespace projectv::core::math
```

---

## 6. Умные указатели

### 6.1 Стандартные умные указатели

```cpp
// Уникальное владение
auto unique = std::make_unique<Resource>(args...);

// Разделяемое владение
auto shared = std::make_shared<Resource>(args...);

// Слабая ссылка
std::weak_ptr<Resource> weak = shared;
```

### 6.2 Пользовательские удалители

```cpp
/// Удалитель для аллокатора
export template<typename T, typename Allocator>
struct AllocatorDeleter
{
    Allocator* allocator;

    auto operator()(T* ptr) noexcept -> void
    {
        if (ptr) {
            ptr->~T();
            allocator->deallocate(ptr, sizeof(T));
        }
    }
};

/// Пользовательский unique_ptr с аллокатором
export template<typename T, typename Allocator>
using UniquePtr = std::unique_ptr<T, AllocatorDeleter<T, Allocator>>;

/// Создать объект с аллокатором
export template<typename T, typename Allocator, typename... Args>
[[nodiscard]] auto allocate_unique(
    Allocator& allocator,
    Args&&... args
) -> UniquePtr<T, Allocator>
{
    auto* ptr = allocator.allocate(sizeof(T), alignof(T));
    if (!ptr) {
        throw std::bad_alloc();
    }

    try {
        new (ptr) T(std::forward<Args>(args)...);
    } catch (...) {
        allocator.deallocate(ptr, sizeof(T));
        throw;
    }

    return UniquePtr<T, Allocator>{
        reinterpret_cast<T*>(ptr),
        AllocatorDeleter<T, Allocator>{&allocator}
    };
}
```

---

## 7. GPU-память (VMA)

### 7.1 Интерфейс VMA

```cpp
export module ProjectV.Render.Vulkan;

import std;
import vma;

export namespace projectv::render::vulkan {

/// Типы памяти Vulkan
export enum class MemoryUsage : uint32_t
{
    Unknown = VMA_MEMORY_USAGE_UNKNOWN,
    GPU_Only = VMA_MEMORY_USAGE_GPU_ONLY,
    CPU_Only = VMA_MEMORY_USAGE_CPU_ONLY,
    CPU_To_GPU = VMA_MEMORY_USAGE_CPU_TO_GPU,
    GPU_To_CPU = VMA_MEMORY_USAGE_GPU_TO_CPU,
};

/// Выделение GPU-памяти
export class VulkanAllocator
{
public:
    explicit VulkanAllocator(VkInstance instance, VkPhysicalDevice physical, VkDevice device);
    ~VulkanAllocator() noexcept;

    /// Создать буфер
    [[nodiscard]] auto create_buffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        MemoryUsage memory_usage
    ) -> std::expected<Buffer, Error>;

    /// Создать изображение
    [[nodiscard]] auto create_image(
        const VkImageCreateInfo& info,
        MemoryUsage memory_usage
    ) -> std::expected<Image, Error>;

    /// Отобразить память в CPU
    [[nodiscard]] auto map_memory(Buffer& buffer) -> std::expected<void*, Error>;

    /// Отменить отображение памяти
    auto unmap_memory(Buffer& buffer) noexcept -> void;

private:
    VmaAllocator allocator_;
};

} // namespace projectv::render::vulkan
```

---

## 8. Профилирование памяти

### 8.1 Трекер памяти

```cpp
export module ProjectV.Core.Memory;

import std;

export namespace projectv::core::memory {

/// Глобальный трекер памяти
export class MemoryTracker
{
public:
    static auto instance() -> MemoryTracker&;

    /// Зарегистрировать выделение
    auto record_allocation(
        void* ptr,
        std::size_t size,
        std::size_t alignment,
        const char* file,
        int line
    ) -> void;

    /// Зарегистрировать освобождение
    auto record_deallocation(void* ptr) -> void;

    /// Получить статистику
    [[nodiscard]] auto get_stats() const noexcept -> Stats;

    /// Проверить утечки
    [[nodiscard]] auto has_leaks() const noexcept -> bool;

    /// Вывести отчёт об утечках
    auto dump_leaks() const -> void;

private:
    MemoryTracker() = default;

    struct Allocation {
        std::size_t size;
        std::size_t alignment;
        const char* file;
        int line;
    };

    std::unordered_map<void*, Allocation> allocations_;
    std::atomic<std::size_t> total_allocated_{0};
    std::atomic<std::size_t> total_freed_{0};
};

} // namespace projectv::core::memory
```

### 8.2 Макросы отслеживания

```cpp
/// Макросы для отслеживания памяти
#ifdef PROJECTV_DEBUG_MEMORY

#define PROJECTV_NEW(type, ...) \
    projectv::core::memory::tracked_new<type>(__FILE__, __LINE__, ##__VA_ARGS__)

#define PROJECTV_DELETE(ptr) \
    projectv::core::memory::tracked_delete(ptr)

#else

#define PROJECTV_NEW(type, ...) std::make_unique<type>(__VA_ARGS__)
#define PROJECTV_DELETE(ptr) delete ptr

#endif
```

---

## 9. Требования соответствия

### 9.1 Обязательные требования

1. Все операции выделения памяти ДОЛЖНЫ использовать аллокаторы
2. Все типы данных ДОЛЖНЫ быть правильно выровнены
3. Все ресурсы ДОЛЖНЫ следовать паттерну RAII
4. GPU-память ДОЛЖНА управляться через VMA

### 9.2 Запрещённые практики

1. Прямые вызовы `new`/`delete` (использовать `std::make_unique`/`std::make_shared`)
2. Прямые вызовы `malloc`/`free` (использовать аллокаторы)
3. Сырые указатели для владения (использовать умные указатели)
4. Ручное управление временем жизни без RAII
