# Modern C++ для ProjectV

**🟡 Уровень 2: Средний** — C++20/23/26 features для bleeding edge разработки.

> **Связь с философией:** Этот документ дополняет [10_cpp23-26-features.md](10_cpp23-26-features.md). Здесь фокус на
> практическом применении Modern C++, там — реалистичный взгляд на поддержку компиляторов. См.
> также [02_zero-cost-abstractions.md](../../philosophy/02_zero-cost-abstractions.md) — новые стандарты должны давать
> измеримую пользу.

## Оглавление

- [1. C++20 Modules](#1-c20-modules)
- [2. C++20 Concepts](#2-c20-concepts)
- [3. C++20 Coroutines](#3-c20-coroutines)

---

## 1. C++20 Modules

> **⚠️ Warning: Модули — только для экспериментов**
>
> В ProjectV мы используем классические `#include` как основной путь. C++20 Modules имеют плохую поддержку
> в системах сборки (CMake + Ninja/MSBuild нестабильны), IDE (IntelliSense, clangd ограничены) и требуют
> специфичных флагов для каждого компилятора.
>
> **Рекомендация:** Используйте `#include` в продакшен-коде. Модули можно исследовать в изолированных
> экспериментах, но не внедряйте их в основной кодовой базе до стабилизации инструментальных средств.

### Проблема с #include

```cpp
// Традиционный подход
#include <vector>        // ~50,000 строк кода
#include <string>        // ~20,000 строк кода
#include <algorithm>     // ~30,000 строк кода
// Компиляция каждого TU: 100,000+ строк header-ов!
// Для 100 файлов: 10,000,000 строк компилируется многократно
```

### Решение: C++20 Modules

```cpp
// hello.cppm (module interface)
export module hello;

import <iostream>;

export namespace Hello {
    void sayHello() {
        std::cout << "Hello from module!\n";
    }
}

// main.cpp
import hello;
import std;  // C++23: Standard Library Module

int main() {
    Hello::sayHello();
    return 0;
}
```

### Module для ProjectV

```cpp
// src/voxel/voxel.cppm
export module ProjectV.Voxel;

import std;
import glm;

export namespace ProjectV::Voxel {

struct Voxel {
    uint16_t type;
    uint8_t light;
    uint8_t flags;
};

class VoxelChunk {
public:
    static constexpr size_t SIZE = 16;
    static constexpr size_t VOLUME = SIZE * SIZE * SIZE;

    Voxel& at(uint32_t x, uint32_t y, uint32_t z) {
        return voxels_[x + y * SIZE + z * SIZE * SIZE];
    }

private:
    std::array<Voxel, VOLUME> voxels_;
};

} // namespace ProjectV::Voxel
```

### CMake конфигурация

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)  # Modules support
project(ProjectV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Module library
add_library(ProjectV.Voxel)
target_sources(ProjectV.Voxel PUBLIC FILE_SET CXX_MODULES FILES
  src/voxel/voxel.cppm
)
target_link_libraries(ProjectV.Voxel PRIVATE glm::glm)

# Executable
add_executable(ProjectV src/main.cpp)
target_link_libraries(ProjectV PRIVATE ProjectV.Voxel)
```

### Сравнение времени компиляции

| Конфигурация | Cold build | Incremental | Clean rebuild |
|--------------|------------|-------------|---------------|
| **#include** | 45 сек     | 15 сек      | 45 сек        |
| **Modules**  | 30 сек     | 2 сек       | 8 сек         |

---

## 2. C++20 Concepts

### Проблема с шаблонами

```cpp
// Ошибки шаблонов — кошмар для отладки
template<typename T>
void process(T value) {
    value.update();  // Ошибка: 200 строк в std::vector если T не имеет update()
}
```

### Решение: Concepts

```cpp
// Определение concept
template<typename T>
concept VoxelData = requires(T v) {
    { v.type() } -> std::convertible_to<uint16_t>;
    { v.light() } -> std::convertible_to<uint8_t>;
    { v.flags() } -> std::convertible_to<uint8_t>;
    { v.isEmpty() } -> std::convertible_to<bool>;
};

// Использование
template<VoxelData V>
void processVoxel(V& voxel) {
    if (!voxel.isEmpty()) {
        uint16_t type = voxel.type();
        // ...
    }
}

// Или с requires
template<typename T>
requires VoxelData<T>
void processVoxel(T& voxel);

// Или сокращённая форма
void processVoxel(VoxelData auto& voxel);
```

### Concepts для ECS компонентов

```cpp
// Concepts для типизации воксельных компонентов
template<typename T>
concept VoxelComponent = requires(T c) {
    typename T::VoxelType;
    { c.getChunkPosition() } -> std::same_as<glm::ivec3>;
    { c.getLOD() } -> std::same_as<uint32_t>;
    { c.isDirty() } -> std::same_as<bool>;
    { c.markClean() } noexcept;
};

// Concept для сериализуемых компонентов
template<typename T>
concept SerializableComponent = requires(T c, BinaryWriter& w, BinaryReader& r) {
    { c.serialize(w) } -> std::same_as<void>;
    { c.deserialize(r) } -> std::same_as<void>;
};

// Composite concept
template<typename T>
concept VoxelEntity = VoxelComponent<T> && SerializableComponent<T>;

// Использование в системах
template<VoxelEntity Entity>
class VoxelSystem {
public:
    void update(Entity& entity, float deltaTime);
};
```

### Standard Concepts

```cpp
#include <concepts>

// Стандартные concepts
std::integral auto x = 42;           // int, long, etc.
std::floating_point auto y = 3.14f;  // float, double
std::same_as<int> auto z = 10;       // Точно int
std::convertible_to<float> auto f = 10;  // Можно привести к float

// Для контейнеров
std::ranges::range auto container = std::vector<int>{1, 2, 3};
std::sortable auto& data = container;
```

---

## 3. C++20 Coroutines

### Проблема с асинхронностью

```cpp
// Callback hell
void loadChunk(glm::ivec3 pos,
               std::function<void(VoxelChunk*)> onSuccess,
               std::function<void(Error)> onError) {
    fileSystem->readAsync(chunkPath(pos),
        [onSuccess, onError](Buffer data) {
            if (data.valid()) {
                auto chunk = deserialize(data);
                onSuccess(chunk);
            } else {
                onError(Error::FileNotFound);
            }
        });
}

// Использование: 3+ уровня вложенности
loadChunk(pos,  {
    generateMesh(chunk,  {
        uploadToGPU(mesh,  {
            // finally...
        });
    });
});
```

### Решение: Coroutines

```cpp
// Базовый Task type
template<typename T>
class Task {
public:
    struct promise_type {
        T value;
        std::exception_ptr exception;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T v) { value = v; }
        void unhandled_exception() { exception = std::current_exception(); }
    };

    // ... implementation
};

// Асинхронная загрузка чанка
Task<VoxelChunk*> loadChunk(glm::ivec3 pos) {
    Buffer data = co_await fileSystem->readAsync(chunkPath(pos));

    if (!data.valid()) {
        throw Error::FileNotFound;
    }

    VoxelChunk* chunk = deserialize(data);
    co_return chunk;
}

// Композиция
Task<GPUBuffer> loadAndUploadChunk(glm::ivec3 pos) {
    VoxelChunk* chunk = co_await loadChunk(pos);
    Mesh mesh = co_await generateMeshAsync(chunk);
    GPUBuffer buffer = co_await uploadToGPUAsync(mesh);
    co_return buffer;
}

// Использование: плоский код!
Task<void> loadWorldAround(glm::vec3 playerPos) {
    for (auto& pos : getVisibleChunks(playerPos)) {
        GPUBuffer buffer = co_await loadAndUploadChunk(pos);
        chunkCache_[pos] = buffer;
    }
}
```

### Thread Pool для Coroutines

```cpp
// Awaiter для thread pool
class ThreadPoolAwaiter {
public:
    ThreadPoolAwaiter(ThreadPool& pool) : pool_(pool) {}

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        pool_.submit([handle]() {
            handle.resume();
        });
    }

    void await_resume() {}

private:
    ThreadPool& pool_;
};

// Использование
Task<void> heavyComputation() {
    // Переключение на background thread
    co_await threadPool_;

    // Выполнение на thread pool
    auto result = computeVoxelMesh();

    co_return result;
}
```

### Generator для воксельных данных

```cpp
// C++23 std::generator или custom
template<typename T>
class Generator {
public:
    struct promise_type {
        T value;

        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T v) {
            value = v;
            return {};
        }

        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    // Iterator interface
    bool next() {
        handle_.resume();
        return !handle_.done();
    }

    T value() const { return handle_.promise().value; }

private:
    std::coroutine_handle<promise_type> handle_;
};

// Использование: генерация вокселей
Generator<Voxel> generateChunkVoxels(const NoiseSettings& noise) {
    for (uint32_t z = 0; z < 16; z++) {
        for (uint32_t y = 0; y < 16; y++) {
            for (uint32_t x = 0; x < 16; x++) {
                Voxel v = calculateVoxel(x, y, z, noise);
                co_yield v;
            }
        }
    }
}

// Потребление
void processChunk() {
    auto gen = generateChunkVoxels(noiseSettings);
    while (gen.next()) {
        processVoxel(gen.value());
    }
}
```

---

## Чеклист внедрения

- [ ] Обновить CMake до 3.28+ для Modules
- [ ] Создать модуль `ProjectV.Core`
- [ ] Добавить Concepts для компонентов ECS
- [ ] Реализовать Task для асинхронной загрузки
- [ ] Профилировать время компиляции
