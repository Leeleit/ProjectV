# Evil C++: Продвинутые техники для High-Performance кода

**🔴 Продвинутый уровень** — Техники, нарушающие "хороший" C++ ради производительности.

Этот документ охватывает техники, которые выходят за рамки стандартного Modern C++, но необходимы для написания
высокопроизводительного кода в воксельном движке.

> **Связь с философией:** Это практическое
> продолжение [11_evil-hacks-philosophy.md](../../philosophy/11_evil-hacks-philosophy.md). "Evil" техники допустимы
> только
> в hot paths и только после профилирования. См.
> также [02_zero-cost-abstractions.md](../../philosophy/02_zero-cost-abstractions.md) — иногда zero-cost требует "
> некрасивого" кода.

---

## Содержание

1. [Type Punning и Strict Aliasing](#type-punning-и-strict-aliasing)
2. [Fast Inverse Square Root: история и современные аналоги](#fast-inverse-square-root-история-и-современные-аналоги)
3. [Custom Allocators](#custom-allocators)
4. [Pointer Tagging](#pointer-tagging)
5. [CRTP vs Virtual Functions](#crtp-vs-virtual-functions)
6. [Deducing This (C++23)](#deducing-this-c23)
7. [STL Traps и как их избежать](#stl-traps-и-как-их-избежать)
8. [Cache Line Optimizations](#cache-line-optimizations)

---

## Type Punning и Strict Aliasing

### Что такое Strict Aliasing?

**Strict Aliasing Rule** (строгое псевдонимирование) — компилятор предполагает, что указатели разных типов не указывают
на одну и ту же память. Нарушение этого правила — UB.

```cpp
// UB: Нарушение strict aliasing
void bad_example() {
    float f = 1.0f;
    int i = *(int*)&f;  // UB: int* и float* alias одна память
    printf("%d\n", i);  // Может быть "оптимизировано" в undefined
}
```

### Почему компилятор так делает?

```cpp
// Компилятор видит:
void process(int* a, float* b) {
    *a = 1;
    *b = 2.0f;
    // Компилятор считает, что a и b не пересекаются
    // Может переставить операции или убрать load
    return *a;  // Может вернуть 1 без чтения памяти
}
```

### Правильные способы Type Punning

#### 1. std::bit_cast (C++20) — Золотой стандарт

> **Метафора:** `std::bit_cast` — это "смена одежды". Вы берёте данные (биты) и говорите: "теперь это не float, а
> uint32_t". Сами данные не меняются, меняется только то, как мы их интерпретирую. Это безопасный способ сделать
> reinterpret_cast без UB. Компилятор видит это и может оптимизировать (например, просто mov между регистрами).

```cpp
#include <bit>

float f = 1.0f;
uint32_t i = std::bit_cast<uint32_t>(f);  // OK: defined behavior
float f2 = std::bit_cast<float>(i);

// Для массивов
std::array<float, 4> floats = {1.0f, 2.0f, 3.0f, 4.0f};
auto ints = std::bit_cast<std::array<uint32_t, 4>>(floats);
```

> **Для понимания:** `std::bit_cast` требует, чтобы типы имели одинаковый размер и были trivially copyable. Это
> гарантирует, что "смена одежды" безопасна — нет виртуальных функций, нет указателей на динамические данные.

#### 2. std::memcpy — Классический подход

```cpp
#include <cstring>

float f = 1.0f;
uint32_t i;
std::memcpy(&i, &f, sizeof(float));  // OK: компилятор оптимизирует

// Компилятор видит это и генерирует эффективный код
// (обычно просто mov между регистрами)
```

#### 3. Union (только в C, UB в C++)

```cpp
// Работает в C99, но формально UB в C++
// Однако все major компиляторы поддерживают это
union FloatInt {
    float f;
    uint32_t i;
};

FloatInt fi;
fi.f = 1.0f;
uint32_t i = fi.i;  // GCC, Clang, MSVC: OK
```

### Practical Example: Быстрое сравнение float

```cpp
// Задача: сравнить float с 0.0f быстро
// Проблема: NaN, -0.0f

// Медленный путь
bool is_zero_slow(float f) {
    return f == 0.0f;  // NaN comparisons are tricky
}

// Быстрый путь через bit representation
bool is_zero_fast(float f) {
    uint32_t bits = std::bit_cast<uint32_t>(f);
    return bits == 0 || bits == 0x80000000;  // +0.0f или -0.0f
}

// Ещё быстрее для "is positive"
bool is_positive(float f) {
    return std::bit_cast<uint32_t>(f) & 0x80000000 == 0;
}
```

### Practical Example: Быстрый abs для float

```cpp
// Традиционный путь
float abs_traditional(float f) {
    return f < 0.0f ? -f : f;  // Branch!
}

// Быстрый путь через bit hack
float abs_bit_hack(float f) {
    uint32_t bits = std::bit_cast<uint32_t>(f);
    bits &= 0x7FFFFFFF;  // Clear sign bit
    return std::bit_cast<float>(bits);
}

// SIMD путь (AVX2)
__m256 abs_avx2(__m256 v) {
    __m256 mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
    return _mm256_and_ps(v, mask);  // 1 instruction, no branch
}
```

---

## Fast Inverse Square Root: история и современные аналоги

### Исторический контекст: Quake III Arena

Знаменитый `Q_rsqrt` из Quake III Arena (1999):

```cpp
// Оригинальный код из Quake III (John Carmack)
// Исторический пример — НЕ ИСПОЛЬЗОВАТЬ В ПРОДАКШЕНЕ
float Q_rsqrt(float number) {
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    i  = * ( long * ) &y;                       // Evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // What the fuck?
    y  = * ( float * ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
    // y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, can be removed

    return y;
}
```

**Почему это работало:**

- "Магическое число" `0x5f3759df` — аппроксимация log₂(√x)
- Один проход Newton-Raphson для уточнения
- Точность ~0.175% error

**Почему НЕ использовать сейчас:**

1. **UB (Undefined Behavior)** — нарушение strict aliasing
2. **Медленнее чем rsqrt на современных CPU** — аппаратная инструкция быстрее
3. **Непереносимо** — работает только для IEEE 754 float

### Современный подход: rsqrt intrinsics

```cpp
#include <immintrin.h>

// SSE: 4 × rsqrt параллельно
inline __m128 rsqrt_sse(__m128 x) {
    return _mm_rsqrt_ps(x);  // Аппаратная инструкция, ~3 cycles
}

// С уточнением Newton-Raphson (один проход)
inline __m128 rsqrt_sse_precise(__m128 x) {
    __m128 rsqrt = _mm_rsqrt_ps(x);
    // Newton-Raphson: y = y * (1.5 - 0.5 * x * y * y)
    __m128 half = _mm_set1_ps(0.5f);
    __m128 threehalfs = _mm_set1_ps(1.5f);
    __m128 x_half = _mm_mul_ps(x, half);
    __m128 rsqrt_sq = _mm_mul_ps(rsqrt, rsqrt);
    rsqrt = _mm_mul_ps(rsqrt, _mm_sub_ps(threehalfs, _mm_mul_ps(x_half, rsqrt_sq)));
    return rsqrt;
}

// AVX2: 8 × rsqrt параллельно
inline __m256 rsqrt_avx2(__m256 x) {
    return _mm256_rsqrt_ps(x);
}
```

### Сравнение производительности

| Метод                  | Точность      | Latency (cycles) | Throughput      |
|------------------------|---------------|------------------|-----------------|
| `1.0f / sqrtf(x)`      | Full          | ~30              | 1 per 30 cycles |
| `Q_rsqrt` (historical) | ~0.175% error | ~15              | 1 per 15 cycles |
| `_mm_rsqrt_ps`         | ~0.01% error  | ~3               | 4 per cycle     |
| `_mm_rsqrt_ps + NR`    | Full          | ~6               | 4 per cycle     |

**Рекомендация:** Используйте `_mm_rsqrt_ps` без Newton-Raphson для игр (ошибка незаметна), с NR для научных расчётов.

---

## Custom Allocators

### Почему new/delete — проблема?

```cpp
// Проблема 1: Individual allocations are slow
for (int i = 0; i < 10000; ++i) {
    auto* voxel = new Voxel();  // malloc overhead каждый раз
    process(voxel);
    delete voxel;               // free overhead каждый раз
}

// Проблема 2: Fragmentation
// После многих new/delete память фрагментируется
// Cache misses растут
```

### std::pmr (Polymorphic Memory Resources) — C++17

```cpp
#include <memory_resource>
#include <vector>

// 1. monotonic_buffer_resource — только allocation, no free
char buffer[1024 * 1024];  // 1 MB
std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer)};

std::pmr::vector<int> vec{&pool};
vec.push_back(1);  // Быстрая аллокация из pool
vec.push_back(2);
// При уничтожении pool — всё освобождается разом

// 2. unsynchronized_pool_resource — для single-threaded
std::pmr::unsynchronized_pool_resource unsync_pool;
std::pmr::vector<Voxel> voxels{&unsync_pool};

// 3. synchronized_pool_resource — thread-safe
std::pmr::synchronized_pool_resource sync_pool;
```

### Frame Allocator (Linear/Bump Allocator)

```cpp
// Самый быстрый аллокатор для временных данных
class FrameAllocator {
    char* buffer_;
    size_t capacity_;
    size_t offset_;

public:
    explicit FrameAllocator(size_t capacity)
        : buffer_(static_cast<char*>(std::malloc(capacity)))
        , capacity_(capacity)
        , offset_(0) {}

    ~FrameAllocator() { std::free(buffer_); }

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // Align offset
        size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);

        if (aligned + size > capacity_) {
            throw std::bad_alloc{};
        }

        void* ptr = buffer_ + aligned;
        offset_ = aligned + size;
        return ptr;
    }

    void reset() { offset_ = 0; }  // "Free" everything at once!

    // No individual deallocation
    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;
};

// Использование
class VoxelRenderer {
    FrameAllocator frame_alloc_{16 * 1024 * 1024};  // 16 MB

    void render_frame() {
        frame_alloc_.reset();  // Free previous frame

        // Temporary data — no individual deletes!
        auto* temp_vertices = frame_alloc_.allocate<Vertex>(32768);
        auto* temp_normals = frame_alloc_.allocate<glm::vec3>(32768);

        generate_mesh(temp_vertices, temp_normals);
        upload_to_gpu(temp_vertices, temp_normals);

        // Всё освобождается при следующем reset()
    }
};
```

### Pool Allocator для одинаковых объектов

```cpp
template<typename T, size_t PoolSize = 1024>
class PoolAllocator {
    union Node {
        T data;
        Node* next;
    };

    Node pool_[PoolSize];
    Node* free_list_ = nullptr;

public:
    PoolAllocator() {
        for (size_t i = 0; i < PoolSize - 1; ++i) {
            pool_[i].next = &pool_[i + 1];
        }
        pool_[PoolSize - 1].next = nullptr;
        free_list_ = &pool_[0];
    }

    T* allocate() {
        if (!free_list_) return nullptr;  // Pool exhausted
        T* ptr = &free_list_->data;
        free_list_ = free_list_->next;
        return ptr;
    }

    void deallocate(T* ptr) {
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = free_list_;
        free_list_ = node;
    }
};

// Использование для VoxelChunks
PoolAllocator<VoxelChunk> chunk_pool;

VoxelChunk* chunk = chunk_pool.allocate();
// ... use chunk ...
chunk_pool.deallocate(chunk);
```

---

## CRTP vs Virtual Functions

### Проблема с Virtual Functions

```cpp
// Virtual functions = cache misses + indirect calls
struct Shape {
    virtual ~Shape() = default;
    virtual float area() const = 0;
};

struct Circle : Shape {
    float radius;
    float area() const override { return 3.14159f * radius * radius; }
};

// Проблема: indirect call через vtable
float total_area(const std::vector<std::unique_ptr<Shape>>& shapes) {
    float total = 0.0f;
    for (const auto& shape : shapes) {
        total += shape->area();  // Virtual call! Cache miss potential
    }
    return total;
}
```

### CRTP (Curiously Recurring Template Pattern)

```cpp
// CRTP: Static polymorphism
template<typename Derived>
struct Shape {
    float area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

struct Circle : Shape<Circle> {
    float radius;
    float area_impl() const { return 3.14159f * radius * radius; }
};

struct Square : Shape<Square> {
    float side;
    float area_impl() const { return side * side; }
};

// Теперь area() — inline, no virtual call!
template<typename T>
float total_area(const std::vector<T>& shapes) {
    float total = 0.0f;
    for (const auto& shape : shapes) {
        total += shape.area();  // Inline! No vtable lookup
    }
    return total;
}
```

### CRTP Mixins для ECS

```cpp
// Mixin: добавляем функциональность через CRTP
template<typename Derived>
struct Transformable {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale{1.0f};

    void translate(const glm::vec3& delta) {
        position += delta;
    }

    void rotate(const glm::quat& q) {
        rotation = q * rotation;
    }

    glm::mat4 model_matrix() const {
        auto& self = static_cast<const Derived&>(*this);
        // Use self for additional data if needed
        return glm::translate(glm::mat4(1.0f), position) *
               glm::mat4_cast(rotation) *
               glm::scale(glm::mat4(1.0f), scale);
    }
};

// Usage
struct VoxelEntity : Transformable<VoxelEntity> {
    uint32_t chunk_id;
    uint32_t voxel_id;
};

// Теперь VoxelEntity имеет translate(), rotate(), model_matrix()
// БЕЗ virtual overhead!
```

### Когда использовать CRTP?

✅ **Да:**

- Hot paths в рендеринге
- Mixins для ECS компонентов
- Static interfaces

❌ **Нет:**

- Runtime polymorphism нужен (heterogeneous collections)
- API boundaries (binary compatibility)
- Сложная иерархия

---

## Deducing This (C++23)

### Проблема:*this в member functions

```cpp
// До C++23: явный CRTP
template<typename Derived>
struct Base {
    void interface() {
        static_cast<Derived*>(this)->implementation();
    }
};

struct Concrete : Base<Concrete> {
    void implementation() { /* ... */ }
};
```

### C++23: Explicit Object Parameter

```cpp
// C++23: Deducing This
struct Shape {
    template<typename Self>
    float area(this Self&& self) {
        return self.area_impl();  // Call on derived type
    }
};

struct Circle : Shape {
    float radius;
    float area_impl() const { return 3.14159f * radius * radius; }
};

// Автоматический dispatch БЕЗ virtual!
Circle c{1.0f};
float a = c.area();  // Calls Circle::area_impl

// Работает с references
void process(Shape& s) {
    float a = s.area();  // Correct dispatch even through base reference
}
```

### Practical Example: Fluent Interface

```cpp
// Builder pattern с Deducing This
template<typename Derived>
class BuilderBase {
public:
    template<typename Self>
    Derived&& with_name(this Self&& self, std::string name) {
        self.name_ = std::move(name);
        return std::forward<Self>(self);
    }
};

class MeshBuilder : public BuilderBase<MeshBuilder> {
    std::string name_;
    std::vector<Vertex> vertices_;

public:
    MeshBuilder&& with_vertices(this MeshBuilder&& self, std::vector<Vertex> verts) {
        self.vertices_ = std::move(verts);
        return std::move(self);
    }

    Mesh build() && {
        return Mesh{std::move(name_), std::move(vertices_)};
    }
};

// Usage
Mesh mesh = MeshBuilder{}
    .with_name("Cube")
    .with_vertices(generate_cube())
    .build();
```

### Recursive Lambdas с Deducing This

```cpp
// До C++23: Y-combinator или std::function overhead
auto factorial =  -> int {
    return n <= 1 ? 1 : n * factorial(n - 1);  // Error: factorial не captured
};

// C++23: Recursive lambda
auto factorial =  -> int {
    return n <= 1 ? 1 : n * self(n - 1);  // Works!
};

// Tree traversal
auto traverse =  -> void {
    if (!node) return;
    self(node->left);   // Recursive call
    process(node->data);
    self(node->right);  // Recursive call
};
```

---

## STL Traps и как их избежать

### 1. std::vector\<bool\> — это НЕ vector

```cpp
// ПРОБЛЕМА: std::vector<bool> — specialization, прокси-объекты
std::vector<bool> flags(100);
bool* ptr = &flags[0];  // ОШИБКА КОМПИЛЯЦИИ! Нет storage

// РЕШЕНИЕ 1: std::vector<char>
std::vector<char> flags(100);
char* ptr = flags.data();  // OK

// РЕШЕНИЕ 2: std::bitset для фиксированного размера
std::bitset<100> flags;
bool b = flags[0];

// РЕШЕНИЕ 3: boost::dynamic_bitset
boost::dynamic_bitset<> flags(100);
```

### 2. std::shared_ptr — атомики дорогие

```cpp
// ПРОБЛЕМА: shared_ptr использует atomic reference count
auto ptr = std::make_shared<VoxelChunk>();
auto ptr2 = ptr;  // atomic increment! ~10-50 cycles

// РЕШЕНИЕ: unique_ptr + explicit ownership
auto ptr = std::make_unique<VoxelChunk>();
auto ptr2 = ptr.get();  // Raw pointer, no overhead

// РЕШЕНИЕ: intrusive refcount для hot paths
class VoxelChunk {
    std::atomic<int> refcount_{0};
public:
    void add_ref() { refcount_.fetch_add(1, std::memory_order_relaxed); }
    void release() { if (refcount_.fetch_sub(1) == 1) delete this; }
};
```

### 3. Iterator Invalidation

```cpp
// ПРОБЛЕМА: push_back invalidates iterators/pointers
std::vector<int> vec = {1, 2, 3};
int* ptr = &vec[0];
vec.push_back(4);  // May reallocate!
*ptr;  // UB! Pointer invalid

// РЕШЕНИЕ: reserve
std::vector<int> vec;
vec.reserve(1000);  // Pre-allocate
int* ptr = &vec[0];
vec.push_back(4);   // OK, no reallocation

// РЕШЕНИЕ: stable pointers with deque
std::deque<int> deq;
int* ptr = &deq[0];
deq.push_back(4);   // OK, pointers stable
```

### 4. std::map vs std::unordered_map

```cpp
// std::map: O(log n), tree-based, pointer-heavy
std::map<int, VoxelChunk*> chunks;
chunks.find(x);  // Tree traversal, cache misses

// std::unordered_map: O(1) average, hash table
std::unordered_map<int, VoxelChunk*> chunks;
chunks.find(x);  // Hash lookup, better cache locality

// ДЛЯ ВЫСОКОЙ ПРОИЗВОДИТЕЛЬНОСТИ: flat_map (sorted vector)
struct FlatMap {
    std::vector<std::pair<int, VoxelChunk*>> data;

    VoxelChunk* find(int key) {
        auto it = std::lower_bound(data.begin(), data.end(), key,
             { return a.first < b; });
        return (it != data.end() && it->first == key) ? it->second : nullptr;
    }
};
// Лучший cache locality, но O(log n) lookup, O(n) insert
```

### 5. std::function — type erasure overhead

```cpp
// ПРОБЛЕМА: std::function имеет overhead
std::function<void(int)> callback =  { process(x); };
callback(42);  // Virtual call through type-erased wrapper

// РЕШЕНИЕ: Template parameter
template<typename Callback>
void process_with_callback(int value, Callback&& cb) {
    cb(value);  // Inline! No overhead
}

// РЕШЕНИЕ: Function pointer для простых cases
using Callback = void(*)(int);
void process_with_callback(int value, Callback cb) {
    cb(value);  // Direct call, can be inlined
}
```

---

## Small Buffer Optimization (SBO)

### Проблема: Heap Allocations в "простых" операциях

```cpp
// std::string — может аллоцировать на heap даже для коротких строк
std::string s = "Hello";  // Может быть heap allocation!

// std::function — всегда heap allocation для captures
int x = 42;
std::function<void()> f = [x] { print(x); };  // Heap allocation!

// std::any — всегда heap allocation
std::any a = 42;  // Heap allocation for int!
```

### SBO в std::string

```cpp
// Большинство реализаций std::string имеют SSO (Small String Optimization)
// Обычно 15-23 байта хранятся inline

static_assert(sizeof(std::string) >= 16);  // Обычно 24-32 bytes

std::string short_str = "Hello";   // SSO: inline storage, no heap
std::string long_str = "This is a very long string that exceeds SSO limit";  // Heap

// Проверка SSO
bool is_sso(const std::string& s) {
    return s.capacity() <= s.size() &&
           s.size() <= 15;  // Implementation-defined
}
```

### Custom SBO String для вокселей

```cpp
// Fixed-capacity string для имён чанков, материалов, etc.
template<size_t Capacity = 31>
class FixedString {
    static_assert(Capacity <= 255);  // uint8_t for size

    char data_[Capacity + 1];
    uint8_t size_ = 0;

public:
    FixedString() = default;

    FixedString(std::string_view sv) {
        size_ = static_cast<uint8_t>(std::min(sv.size(), Capacity));
        std::memcpy(data_, sv.data(), size_);
        data_[size_] = '\0';
    }

    std::string_view view() const { return {data_, size_}; }
    const char* c_str() const { return data_; }
    size_t size() const { return size_; }

    bool operator==(const FixedString& other) const {
        return size_ == other.size_ &&
               std::memcmp(data_, other.data_, size_) == 0;
    }
};

static_assert(sizeof(FixedString<31>) == 32);  // Compact!

// Usage
FixedString<31> material_name = "stone_bricks";
FixedString<31> chunk_id = "chunk_0_0_0";
```

### SBO для std::function

```cpp
// Проблема: std::function всегда heap allocates для lambdas с captures
std::function<void()> f = [big_data = std::vector<int>(1000)]() {
    process(big_data);
};  // Heap allocation!

// Решение 1: Шаблонный параметр (но теряем type erasure)
template<typename F>
void set_callback(F&& callback) {
    callback_ = std::forward<F>(callback);  // Store directly
}

// Решение 2: Custom SBO function
template<typename Signature, size_t BufferSize = 32>
class SBOFunction;

template<typename R, typename... Args, size_t BufferSize>
class SBOFunction<R(Args...), BufferSize> {
    alignas(std::max_align_t) char buffer_[BufferSize];

    using InvokePtr = R(*)(void*, Args...);
    using DestroyPtr = void(*)(void*);

    InvokePtr invoke_ = nullptr;
    DestroyPtr destroy_ = nullptr;

public:
    SBOFunction() = default;

    template<typename F>
    SBOFunction(F&& f) {
        static_assert(sizeof(F) <= BufferSize);
        static_assert(alignof(F) <= alignof(std::max_align_t));

        new (buffer_) std::decay_t<F>(std::forward<F>(f));

        invoke_ =  -> R {
            return (*static_cast<std::decay_t<F>*>(ptr))(std::forward<Args>(args)...);
        };

        destroy_ =  {
            static_cast<std::decay_t<F>*>(ptr)->~std::decay_t<F>();
        };
    }

    ~SBOFunction() {
        if (destroy_) destroy_(buffer_);
    }

    R operator()(Args... args) const {
        return invoke_(buffer_, std::forward<Args>(args)...);
    }
};

// Usage: lambdas с captures помещаются в buffer
SBOFunction<void(), 64> callback = [x = 42, y = 100]() {
    print(x + y);
};  // No heap allocation!

// Для больших captures всё равно heap
SBOFunction<void(), 32> big_callback = [vec = std::vector<int>(100)]() {
    // Won't compile: sizeof(lambda) > 32
};
```

### SBO в intrusive_ptr

```cpp
// intrusive_ptr с embedded reference count
template<typename T>
class IntrusivePtr {
    T* ptr_;

public:
    explicit IntrusivePtr(T* p = nullptr) : ptr_(p) {
        if (ptr_) ptr_->add_ref();
    }

    ~IntrusivePtr() {
        if (ptr_) ptr_->release();
    }

    // Copy, move, etc...
};

// Объект с embedded refcount (SBO-friendly)
class VoxelChunk {
    mutable std::atomic<uint32_t> refcount_{0};

public:
    void add_ref() const { refcount_.fetch_add(1, std::memory_order_relaxed); }
    void release() const { if (refcount_.fetch_sub(1) == 1) delete this; }

    // Данные чанка...
};

// No separate allocation for control block (как в shared_ptr)
IntrusivePtr<VoxelChunk> chunk = new VoxelChunk();
```

---

## Type Erasure без Virtual Functions

### Проблема: Virtual Functions — overhead

```cpp
// Традиционный polymorphism
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void render(VkCommandBuffer cmd) = 0;
};

class VulkanRenderer : public Renderer {
    void render(VkCommandBuffer cmd) override { /* ... */ }
};

// Overhead: vtable lookup, cache miss, no inlining
void draw(Renderer* renderer) {
    renderer->render(cmd);  // Virtual call
}
```

### Type Erasure с Templates

```cpp
// Type-erased wrapper без virtual
class RendererRef {
    void* ptr_;
    void (*render_fn)(void*, VkCommandBuffer);
    void (*destroy_fn)(void*);

public:
    template<typename T>
    RendererRef(T&& obj) {
        using ObjT = std::decay_t<T>;
        ptr_ = new ObjT(std::forward<T>(obj));

        render_fn =  {
            static_cast<ObjT*>(p)->render(cmd);
        };

        destroy_fn =  {
            delete static_cast<ObjT*>(p);
        };
    }

    ~RendererRef() { destroy_fn(ptr_); }

    void render(VkCommandBuffer cmd) { render_fn(ptr_, cmd); }
};

// Usage
struct MyRenderer {
    void render(VkCommandBuffer cmd) { /* inline-able */ }
};

RendererRef renderer = MyRenderer{};
renderer.render(cmd);  // No virtual call!
```

### SBO + Type Erasure = std::any_like

```cpp
// Type-erased container с SBO
template<size_t BufferSize = 64, size_t Align = alignof(std::max_align_t)>
class Any {
    alignas(Align) char buffer_[BufferSize];

    using MoveFn = void(*)(char* dst, char* src);
    using DestroyFn = void(*)(char*);
    using TypeInfo = const std::type_info& (*)();

    MoveFn move_ = nullptr;
    DestroyFn destroy_ = nullptr;
    TypeInfo type_info_ = nullptr;
    bool is_heap_ = false;

    void* heap_ptr_ = nullptr;  // Для больших объектов

public:
    Any() = default;

    template<typename T>
    Any(T&& value) {
        using ValueT = std::decay_t<T>;

        if constexpr (sizeof(ValueT) <= BufferSize &&
                      alignof(ValueT) <= Align) {
            // SBO path
            new (buffer_) ValueT(std::forward<T>(value));
            is_heap_ = false;

            move_ =  {
                new (dst) ValueT(std::move(*reinterpret_cast<ValueT*>(src)));
            };

            destroy_ =  {
                reinterpret_cast<ValueT*>(p)->~ValueT();
            };
        } else {
            // Heap path
            heap_ptr_ = new ValueT(std::forward<T>(value));
            is_heap_ = true;
        }

        type_info_ =  -> const std::type_info& { return typeid(ValueT); };
    }

    ~Any() {
        if (destroy_) {
            destroy_(is_heap_ ? static_cast<char*>(heap_ptr_) : buffer_);
            if (is_heap_) operator delete(heap_ptr_);
        }
    }

    template<typename T>
    T* cast() {
        if (type_info_() != typeid(T)) return nullptr;
        return reinterpret_cast<T*>(is_heap_ ? heap_ptr_ : buffer_);
    }
};

// Usage
Any<64> any = std::vector<int>{1, 2, 3};  // Fits in buffer
auto* vec = any.cast<std::vector<int>>();
```

### Practical Example: ECS Component Storage

```cpp
// Type-erased component pool с SBO
class ComponentPoolBase {
public:
    virtual ~ComponentPoolBase() = default;
    virtual void* get(EntityID entity) = 0;
    virtual void remove(EntityID entity) = 0;
};

template<typename T, size_t ChunkSize = 4096>
class ComponentPool : public ComponentPoolBase {
    // Chunked storage для better cache locality
    std::vector<std::unique_ptr<std::array<T, ChunkSize / sizeof(T)>>> chunks_;
    std::unordered_map<EntityID, size_t> entity_to_index_;

public:
    T* get(EntityID entity) override {
        auto it = entity_to_index_.find(entity);
        if (it == entity_to_index_.end()) return nullptr;

        size_t idx = it->second;
        size_t chunk_idx = idx / (ChunkSize / sizeof(T));
        size_t in_chunk = idx % (ChunkSize / sizeof(T));

        return &(*chunks_[chunk_idx])[in_chunk];
    }

    template<typename... Args>
    T* emplace(EntityID entity, Args&&... args) {
        // Find or allocate slot
        size_t idx = entity_to_index_.size();
        entity_to_index_[entity] = idx;

        size_t chunk_idx = idx / (ChunkSize / sizeof(T));
        size_t in_chunk = idx % (ChunkSize / sizeof(T));

        if (chunks_.size() <= chunk_idx) {
            chunks_.push_back(std::make_unique<std::array<T, ChunkSize / sizeof(T)>>());
        }

        return new (&(*chunks_[chunk_idx])[in_chunk]) T(std::forward<Args>(args)...);
    }
};

// Type-erased storage
std::unordered_map<std::type_index, std::unique_ptr<ComponentPoolBase>> pools_;

template<typename T>
ComponentPool<T>* get_pool() {
    auto key = std::type_index(typeid(T));
    auto it = pools_.find(key);
    if (it == pools_.end()) {
        pools_[key] = std::make_unique<ComponentPool<T>>();
    }
    return static_cast<ComponentPool<T>*>(pools_[key].get());
}
```

---

## Pointer Tagging

На x86_64 адреса выровнены на 8 байт (64-bit). Младшие 3 бита всегда нули — можно использовать для хранения данных.

### Базовая реализация

```cpp
// Использование младших битов для хранения данных
template<typename T>
class TaggedPointer {
    static_assert(alignof(T) >= 8, "T must be 8-byte aligned");

    uintptr_t ptr_;

public:
    TaggedPointer(T* p, uint8_t tag = 0) : ptr_(reinterpret_cast<uintptr_t>(p) | tag) {
        assert(tag < 8);  // Только 3 бита доступны
        assert((reinterpret_cast<uintptr_t>(p) & 0x7) == 0);  // Must be aligned
    }

    T* get() const { return reinterpret_cast<T*>(ptr_ & ~0x7ULL); }
    uint8_t tag() const { return ptr_ & 0x7; }

    T* operator->() const { return get(); }
};

// Использование для вокселей
enum class VoxelFlags : uint8_t {
    None = 0,
    Modified = 1,
    Occluded = 2,
    Border = 4
};

struct VoxelChunk;
TaggedPointer<VoxelChunk> chunk_with_flags(chunk_ptr, flags);
```

### Практическое применение: Compact Chunk Storage

```cpp
// Хранение флагов в указателе вместо отдельного поля
class VoxelWorld {
    std::vector<TaggedPointer<VoxelChunk>> chunks_;

public:
    VoxelChunk* get_chunk(size_t idx) {
        return chunks_[idx].get();
    }

    VoxelFlags get_flags(size_t idx) {
        return static_cast<VoxelFlags>(chunks_[idx].tag());
    }

    void set_modified(size_t idx) {
        auto& cp = chunks_[idx];
        cp = TaggedPointer<VoxelChunk>(cp.get(), cp.tag() | static_cast<uint8_t>(VoxelFlags::Modified));
    }
};

// Экономия: 1 байт на чанк вместо отдельного поля
// Для 10,000 чанков = 10 KB экономии
```

---

## Cache Line Optimizations

### Cache Line Size

```cpp
// Обычно 64 байта на x86_64
constexpr size_t CACHE_LINE_SIZE = 64;

// Выравнивание
struct alignas(CACHE_LINE_SIZE) HotData {
    // Данные, к которым часто обращаются
    // Заполняем до 64 байт
};
```

### False Sharing

```cpp
// ПЛОХО: Два потока пишут в одну cache line
struct Counters {
    std::atomic<uint32_t> visible_chunks;  // Thread 1
    std::atomic<uint32_t> culled_chunks;   // Thread 2
    // Обе переменные в одной cache line → false sharing!
};

// ХОРОШО: Каждая переменная в своей cache line
struct alignas(64) CacheLineAlignedCounter {
    std::atomic<uint32_t> count;
    char padding[60];  // Pad to 64 bytes
};

struct CountersFixed {
    CacheLineAlignedCounter visible;   // Thread 1
    CacheLineAlignedCounter culled;    // Thread 2
};
```

### Prefetching

```cpp
// Ручной prefetch для предсказуемых паттернов
void process_voxels_prefetch(const uint8_t* voxels, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        // Prefetch данные на N итераций вперёд
        if (i + 64 < count) {
            _mm_prefetch(reinterpret_cast<const char*>(voxels + i + 64), _MM_HINT_T0);
        }

        process(voxels[i]);
    }
}

// AVX2 prefetch
void process_voxels_prefetch_avx2(const uint8_t* voxels, size_t count) {
    for (size_t i = 0; i < count; i += 32) {
        // Prefetch 256 байт вперёд
        _mm_prefetch(reinterpret_cast<const char*>(voxels + i + 256), _MM_HINT_T0);

        __m256i v = _mm256_loadu_si256((const __m256i*)(voxels + i));
        // ... process ...
    }
}
```

---

## Summary: Чек-лист для Performance-Critical Code

### Type Punning

- [ ] Используйте `std::bit_cast` (C++20) или `std::memcpy`
- [ ] Избегайте `reinterpret_cast` для разных типов
- [ ] Union type punning — формально UB в C++

### Memory Management

- [ ] Используйте Frame Allocator для временных данных
- [ ] Pool Allocator для часто создаваемых/удаляемых объектов
- [ ] `std::pmr` для STL containers с custom allocator
- [ ] `reserve()` для vectors когда размер известен

### Polymorphism

- [ ] CRTP для static polymorphism в hot paths
- [ ] Deducing This (C++23) для cleaner CRTP
- [ ] Virtual functions только для heterogeneous collections

### STL Usage

- [ ] Избегайте `std::vector<bool>`
- [ ] `std::unique_ptr` вместо `std::shared_ptr` где возможно
- [ ] `std::unordered_map` или flat_map вместо `std::map`
- [ ] Templates вместо `std::function` в hot paths

