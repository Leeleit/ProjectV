# Архитектура тестирования DOD и ECS

---

## Обзор

Документ описывает архитектуру тестирования для ProjectV с акцентом на **Data-Oriented Design (DOD)** и *
*Entity-Component-System (ECS)**. Традиционное юнит-тестирование с mock-объектами плохо подходит для DOD — нужна
специализация для проверки инвариантов памяти и изолированного тестирования систем.

---

## 1. Философия тестирования DOD/ECS

### 1.1 Принципы

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DOD/ECS Testing Philosophy                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. TEST DATA, NOT OBJECTS                                              │
│     - Тестируем трансформации данных                                    │
│     - Входной буфер → Система → Выходной буфер                          │
│     - Mock-объекты не нужны — данные это и есть контракт                │
│                                                                          │
│  2. STATIC ASSERTIONS FIRST                                             │
│     - sizeof, alignof проверяются на этапе компиляции                   │
│     - Инварианты памяти — часть API контракта                           │
│     - Нарушение кэш-линии = compile error                               │
│                                                                          │
│  3. ISOLATED SYSTEM TESTS                                               │
│     - Каждая ECS система тестируется изолированно                       │
│     - Fake World с минимальным набором компонентов                      │
│     - No rendering, no Vulkan в юнит-тестах                             │
│                                                                          │
│  4. DETERMINISTIC SIMULATION                                            │
│     - Fixed timestep для воспроизводимости                              │
│     - Seed для random generator                                         │
│     - Snapshot testing для CA simulation                                │
│                                                                          │
│  5. PERFORMANCE REGRESSION GUARD                                        │
│     - Benchmark tests с порогами                                        │
│     - Cache miss counters (via perf counters)                           │
│     - Memory bandwidth tests                                            │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Уровни тестирования

```
                    ┌───────────────────────────────────────┐
                    │         E2E / Integration             │
                    │   (Full Engine, Vulkan Rendering)     │
                    │           [Manual / CI Nightly]       │
                    └───────────────────┬───────────────────┘
                                        │
              ┌─────────────────────────┴─────────────────────────┐
              │              System Integration Tests               │
              │         (ECS World + Physics + Voxel)               │
              │                   [CI Per-Commit]                   │
              └─────────────────────────┬─────────────────────────┘
                                        │
         ┌──────────────────────────────┴──────────────────────────────┐
         │                    Isolated System Tests                     │
         │            (MovementSystem, FluidCASystem, etc.)             │
         │                      [CI Per-Commit]                         │
         └──────────────────────────────┬──────────────────────────────┘
                                        │
┌───────────────────────────────────────┴───────────────────────────────┐
│                      Static Assertions (Compile-Time)                   │
│              (sizeof, alignof, cache-line, invariants)                  │
│                         [Always, Every Build]                           │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Static Assertions для инвариантов памяти

### 2.1 Базовые проверки

```cpp
// tests/static_assertions/memory_layout.cpp

import std;
import ProjectV.Gameplay.Components;
import ProjectV.Voxel.Data;
import ProjectV.Simulation.CellularAutomata;

// === Cache Line Size ===
constexpr size_t CACHE_LINE_SIZE = 64;

// === Component Size Assertions ===

// TransformComponent должен быть 48 bytes (16-byte aligned for SIMD)
static_assert(sizeof(projectv::gameplay::TransformComponent) == 48);
static_assert(alignof(projectv::gameplay::TransformComponent) == 16);

// VelocityComponent должен быть 24 bytes
static_assert(sizeof(projectv::gameplay::VelocityComponent) == 24);
static_assert(alignof(projectv::gameplay::VelocityComponent) == 4);

// VoxelChunkComponent должен быть 16 bytes
static_assert(sizeof(projectv::gameplay::VoxelChunkComponent) == 16);

// === Cache-Line Assertions ===

// Критичные компоненты должны помещаться в одну кэш-линию
static_assert(sizeof(projectv::gameplay::TransformComponent) <= CACHE_LINE_SIZE,
    "TransformComponent exceeds cache line - will cause false sharing");

static_assert(sizeof(projectv::gameplay::VelocityComponent) <= CACHE_LINE_SIZE,
    "VelocityComponent exceeds cache line");

// === Hot Path Structures ===

// CellState для CA симуляции - критичен для bandwidth
static_assert(sizeof(projectv::simulation::CellState) == 32,
    "CellState must be 32 bytes for optimal memory bandwidth");

static_assert(alignof(projectv::simulation::CellState) >= 16,
    "CellState must be 16-byte aligned for SIMD");

// VoxelData для GPU - должен совпадать с GPU layout
static_assert(sizeof(projectv::voxel::VoxelData) == 4,
    "VoxelData must be 4 bytes for GPU compatibility");

static_assert(alignof(projectv::voxel::VoxelData) == 4,
    "VoxelData must be 4-byte aligned");
```

### 2.2 Автоматизированные проверки через CMake

```cmake
# cmake/StaticAssertions.cmake

# Создаёт executable для static assertions
function(add_static_assertions_target)
    add_executable(projectv_static_assertions
        tests/static_assertions/memory_layout.cpp
    )

    target_link_libraries(projectv_static_assertions PRIVATE
        ProjectV.Gameplay.Components
        ProjectV.Voxel.Data
        ProjectV.Simulation.CellularAutomata
    )

    # Запускается при каждой сборке
    add_custom_target(verify_static_assertions
        COMMAND projectv_static_assertions
        COMMENT "Verifying static assertions..."
    )

    add_dependencies(ProjectV verify_static_assertions)
endfunction()
```

### 2.3 Macro для инвариантов

```cpp
// ProjectV.Test.InvariantMacros.hpp (header for tests)

#pragma once

#include <cstddef>

namespace projectv::test {

/// Проверяет что структура помещается в N кэш-линий.
#define PV_ASSERT_CACHE_LINES(Type, N)                                         \
    static_assert(sizeof(Type) <= (N * 64),                                    \
        #Type " exceeds " #N " cache lines (64 bytes each)")

/// Проверяет выравнивание.
#define PV_ASSERT_ALIGNMENT(Type, Align)                                       \
    static_assert(alignof(Type) == Align,                                      \
        #Type " must be aligned to " #Align " bytes")

/// Проверяет точный размер.
#define PV_ASSERT_SIZE(Type, Size)                                             \
    static_assert(sizeof(Type) == Size,                                        \
        #Type " must be exactly " #Size " bytes")

/// Проверяет что тип trivially copyable (для memcpy, GPU upload).
#define PV_ASSERT_TRIVIALLY_COPYABLE(Type)                                     \
    static_assert(std::is_trivially_copyable_v<Type>,                         \
        #Type " must be trivially copyable for GPU upload")

/// Проверяет что тип имеет стандартный layout.
#define PV_ASSERT_STANDARD_LAYOUT(Type)                                        \
    static_assert(std::is_standard_layout_v<Type>,                            \
        #Type " must have standard layout for deterministic memory")

// === Примеры использования ===

namespace examples {
    struct alignas(16) GoodComponent {
        float x, y, z, w;  // 16 bytes
        float a, b, c, d;  // 16 bytes
        float e, f, g, h;  // 16 bytes
    };

    PV_ASSERT_SIZE(GoodComponent, 48);
    PV_ASSERT_ALIGNMENT(GoodComponent, 16);
    PV_ASSERT_TRIVIALLY_COPYABLE(GoodComponent);
    PV_ASSERT_STANDARD_LAYOUT(GoodComponent);
}

} // namespace projectv::test
```

---

## 3. Data-In / Data-Out паттерн для ECS систем

### 3.1 Концепция

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Data-In / Data-Out Pattern                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                        Test Function                              │   │
│  │                                                                   │   │
│  │   ┌─────────────┐      ┌─────────────┐      ┌─────────────┐     │   │
│  │   │  Input Data │ ───▶ │    System   │ ───▶ │ Output Data │     │   │
│  │   │  (Vector)   │      │  (Pure Fn)  │      │  (Vector)   │     │   │
│  │   └─────────────┘      └─────────────┘      └─────────────┘     │   │
│  │         │                                          │             │   │
│  │         │                                          │             │   │
│  │         ▼                                          ▼             │   │
│  │   ┌─────────────┐                          ┌─────────────┐       │   │
│  │   │   EXPECT    │                          │   EXPECT    │       │   │
│  │   │   (Known)   │                          │   (Known)   │       │   │
│  │   └─────────────┘                          └─────────────┘       │   │
│  │                                                                   │   │
│  │   Преимущества:                                                   │   │
│  │   - Нет mock-объектов                                            │   │
│  │   - Детерминированный                                             │   │
│  │   - Легко параллелизуется                                        │   │
│  │   - Snapshot testing                                              │   │
│  │                                                                   │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Тестовый фреймворк

```cpp
// ProjectV.Test.Framework.cppm
export module ProjectV.Test.Framework;

import std;

export namespace projectv::test {

/// Результат теста.
export struct TestResult {
    bool passed{false};
    std::string message;
    std::source_location location;
};

/// Базовый класс для Data-In/Data-Out тестов.
export template<typename Input, typename Output>
class DataIOTest {
public:
    using InputType = Input;
    using OutputType = Output;

    /// Подготавливает входные данные.
    virtual auto prepare_input() -> Input = 0;

    /// Выполняет тестируемую систему.
    virtual auto execute(Input&& input) -> Output = 0;

    /// Проверяет выходные данные.
    virtual auto verify(Output const& output) -> TestResult = 0;

    /// Запускает тест.
    auto run() -> TestResult {
        auto input = prepare_input();
        auto output = execute(std::move(input));
        return verify(output);
    }
};

/// Сравнивает два значения с допуском для float.
export auto approx_equal(float a, float b, float epsilon = 1e-5f) -> bool {
    return std::abs(a - b) < epsilon;
}

/// Сравнивает два массива.
export template<typename T>
auto array_equal(std::span<T const> a, std::span<T const> b) -> bool
    requires std::equality_comparable<T> {

    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin());
}

/// Логирует ошибку с контекстом.
export auto log_failure(
    std::string_view test_name,
    std::string_view expected,
    std::string_view actual,
    std::source_location loc = std::source_location::current()
) -> void;

/// Макрос для проверки.
#define PV_EXPECT_EQ(actual, expected)                                         \
    do {                                                                       \
        if ((actual) != (expected)) {                                          \
            ::projectv::test::log_failure(                                     \
                "EXPECT_EQ",                                                   \
                std::to_string(expected),                                      \
                std::to_string(actual),                                        \
                std::source_location::current()                                \
            );                                                                 \
            return ::projectv::test::TestResult{                              \
                .passed = false,                                               \
                .message = std::format("Expected {} but got {}",               \
                    expected, actual)                                          \
            };                                                                 \
        }                                                                      \
    } while (0)

#define PV_EXPECT_NEAR(actual, expected, epsilon)                              \
    do {                                                                       \
        if (!::projectv::test::approx_equal(actual, expected, epsilon)) {     \
            return ::projectv::test::TestResult{                              \
                .passed = false,                                               \
                .message = std::format("Expected {} ± {} but got {}",          \
                    expected, epsilon, actual)                                 \
            };                                                                 \
        }                                                                      \
    } while (0)

} // namespace projectv::test
```

### 3.3 Пример теста MovementSystem

```cpp
// tests/systems/movement_system_test.cpp

import std;
import glm;
import ProjectV.Test.Framework;
import ProjectV.Gameplay.Components;
import ProjectV.ECS.Flecs;
import flecs;

namespace projectv::test {

/// Тест для MovementSystem.
class MovementSystemTest : public DataIOTest<
    std::vector<std::tuple<TransformComponent, VelocityComponent>>,
    std::vector<TransformComponent>
> {
public:
    auto prepare_input() -> InputType override {
        return {
            // Entity 1: движется по X
            {TransformComponent{.position{0, 0, 0}},
             VelocityComponent{.linear{5.0f, 0, 0}}},

            // Entity 2: движется по Y
            {TransformComponent{.position{0, 0, 0}},
             VelocityComponent{.linear{0, 3.0f, 0}}},

            // Entity 3: движется по диагонали
            {TransformComponent{.position{10, 10, 10}},
             VelocityComponent{.linear{1.0f, 1.0f, 1.0f}}},
        };
    }

    auto execute(InputType&& input) -> OutputType override {
        // Создаём минимальный Flecs world
        flecs::world world;

        // Регистрируем компоненты
        world.component<TransformComponent>();
        world.component<VelocityComponent>();

        // Создаём entities из input
        std::vector<flecs::entity> entities;
        for (auto const& [transform, velocity] : input) {
            auto e = world.entity();
            e.set<TransformComponent>(transform);
            e.set<VelocityComponent>(velocity);
            entities.push_back(e);
        }

        // Регистрируем MovementSystem
        auto system = world.system<TransformComponent, VelocityComponent>(
            "MovementSystem"
        ).each([](flecs::entity e, TransformComponent& t, VelocityComponent const& v) {
            t.position += v.linear * 1.0f;  // delta_time = 1.0
        });

        // Выполняем 1 тик
        world.progress(1.0f);

        // Собираем output
        OutputType output;
        for (auto const& e : entities) {
            auto const* t = e.get<TransformComponent>();
            output.push_back(*t);
        }

        return output;
    }

    auto verify(OutputType const& output) -> TestResult override {
        // Entity 1: position = (5, 0, 0)
        PV_EXPECT_NEAR(output[0].position.x, 5.0f, 0.001f);
        PV_EXPECT_NEAR(output[0].position.y, 0.0f, 0.001f);
        PV_EXPECT_NEAR(output[0].position.z, 0.0f, 0.001f);

        // Entity 2: position = (0, 3, 0)
        PV_EXPECT_NEAR(output[1].position.x, 0.0f, 0.001f);
        PV_EXPECT_NEAR(output[1].position.y, 3.0f, 0.001f);
        PV_EXPECT_NEAR(output[1].position.z, 0.0f, 0.001f);

        // Entity 3: position = (11, 11, 11)
        PV_EXPECT_NEAR(output[2].position.x, 11.0f, 0.001f);
        PV_EXPECT_NEAR(output[2].position.y, 11.0f, 0.001f);
        PV_EXPECT_NEAR(output[2].position.z, 11.0f, 0.001f);

        return TestResult{.passed = true, .message = "All positions correct"};
    }
};

// Запуск теста
int main() {
    MovementSystemTest test;
    auto result = test.run();

    if (!result.passed) {
        std::fprintf(stderr, "FAILED: %s\n", result.message.c_str());
        return 1;
    }

    std::printf("PASSED: %s\n", result.message.c_str());
    return 0;
}

} // namespace projectv::test
```

### 3.4 Пример теста FluidCASystem

```cpp
// tests/systems/fluid_ca_system_test.cpp

import std;
import glm;
import ProjectV.Test.Framework;
import ProjectV.Simulation.CellularAutomata;
import ProjectV.Voxel.Data;

namespace projectv::test {

/// Тест для FluidCA System (Cellular Automata).
class FluidCASystemTest : public DataIOTest<
    std::vector<simulation::CellState>,  // Input: grid of cells
    std::vector<simulation::CellState>   // Output: grid after 1 step
> {
public:
    static constexpr uint32_t GRID_SIZE = 8;
    static constexpr size_t CELL_COUNT = GRID_SIZE * GRID_SIZE * GRID_SIZE;

    auto prepare_input() -> InputType override {
        InputType cells(CELL_COUNT);

        // Создаём простую сцену: вода падает вниз
        // Воздух над водой должен заполниться

        // Верхний слой: вода (density = 1.0)
        for (uint32_t x = 0; x < GRID_SIZE; ++x) {
            for (uint32_t z = 0; z < GRID_SIZE; ++z) {
                uint32_t idx = index(x, GRID_SIZE - 1, z);
                cells[idx] = {
                    .density = 1.0f,
                    .velocity_y = 0.0f,
                    .material_type = 1  // Water
                };
            }
        }

        // Остальное: воздух
        for (size_t i = 0; i < CELL_COUNT; ++i) {
            if (cells[i].material_type == 0) {
                cells[i] = {
                    .density = 0.0f,
                    .velocity_y = 0.0f,
                    .material_type = 0  // Air
                };
            }
        }

        return cells;
    }

    auto execute(InputType&& input) -> OutputType override {
        // Создаём симулятор
        simulation::CASimulator simulator(GRID_SIZE, GRID_SIZE, GRID_SIZE);

        // Загружаем входные данные
        for (uint32_t x = 0; x < GRID_SIZE; ++x) {
            for (uint32_t y = 0; y < GRID_SIZE; ++y) {
                for (uint32_t z = 0; z < GRID_SIZE; ++z) {
                    simulator.set_cell(x, y, z, input[index(x, y, z)]);
                }
            }
        }

        // Выполняем 1 шаг симуляции
        simulator.step(0.016f);  // 16ms timestep

        // Собираем выходные данные
        OutputType output(CELL_COUNT);
        for (uint32_t x = 0; x < GRID_SIZE; ++x) {
            for (uint32_t y = 0; y < GRID_SIZE; ++y) {
                for (uint32_t z = 0; z < GRID_SIZE; ++z) {
                    output[index(x, y, z)] = simulator.get_cell(x, y, z);
                }
            }
        }

        return output;
    }

    auto verify(OutputType const& output) -> TestResult override {
        // После 1 шага вода должна начать падать
        // Проверяем что вода в верхнем слое уменьшилась
        // а в слое ниже увеличилась

        float top_water = 0.0f;
        float below_water = 0.0f;

        for (uint32_t x = 0; x < GRID_SIZE; ++x) {
            for (uint32_t z = 0; z < GRID_SIZE; ++z) {
                // Верхний слой (y = GRID_SIZE - 1)
                top_water += output[index(x, GRID_SIZE - 1, z)].density;

                // Слой ниже (y = GRID_SIZE - 2)
                below_water += output[index(x, GRID_SIZE - 2, z)].density;
            }
        }

        // Вода должна двигаться вниз
        if (below_water <= top_water) {
            return TestResult{
                .passed = false,
                .message = std::format(
                    "Water should flow down: top={}, below={}",
                    top_water, below_water
                )
            };
        }

        return TestResult{
            .passed = true,
            .message = "Fluid simulation behaves correctly"
        };
    }

private:
    static auto index(uint32_t x, uint32_t y, uint32_t z) -> size_t {
        return x + y * GRID_SIZE + z * GRID_SIZE * GRID_SIZE;
    }
};

} // namespace projectv::test
```

---

## 4. Fake World для изолированного тестирования

### 4.1 Fake ECS World

```cpp
// ProjectV.Test.FakeWorld.cppm
export module ProjectV.Test.FakeWorld;

import std;
import flecs;

export namespace projectv::test {

/// Fake World для изолированного тестирования ECS систем.
///
/// ## Особенности
/// - Минимальная конфигурация Flecs
/// - Без рендеринга
/// - Без многопоточности
/// - Детерминированный
export class FakeWorld {
public:
    FakeWorld() noexcept {
        // Отключаем многопоточность для детерминизма
        world_.set_threads(0);
    }

    /// Создаёт entity с компонентами.
    template<typename... Components>
    auto create_entity(Components&&... components) -> flecs::entity {
        auto e = world_.entity();
        (e.set<Components>(std::forward<Components>(components)), ...);
        return e;
    }

    /// Создаёт несколько entities одного типа.
    template<typename Component>
    auto create_entities(std::span<Component const> components)
        -> std::vector<flecs::entity> {

        std::vector<flecs::entity> entities;
        entities.reserve(components.size());

        for (auto const& c : components) {
            entities.push_back(create_entity(c));
        }

        return entities;
    }

    /// Регистрирует систему.
    template<typename... Components>
    auto register_system(
        std::string_view name,
        auto&& update_func
    ) -> flecs::system {
        return world_.system<Components...>(name.data())
            .each(std::forward<decltype(update_func)>(update_func));
    }

    /// Выполняет N тиков симуляции.
    auto tick(uint32_t count = 1, float delta_time = 0.016f) -> void {
        for (uint32_t i = 0; i < count; ++i) {
            world_.progress(delta_time);
        }
    }

    /// Получает компонент entity.
    template<typename Component>
    [[nodiscard]] auto get(flecs::entity e) const -> Component const* {
        return e.get<Component>();
    }

    /// Получает mutable компонент entity.
    template<typename Component>
    [[nodiscard]] auto get_mut(flecs::entity e) -> Component* {
        return e.get_mut<Component>();
    }

    /// Собирает все компоненты типа T в vector.
    template<typename Component>
    [[nodiscard]] auto collect_all() const -> std::vector<Component> {
        std::vector<Component> result;

        world_.each([&](flecs::entity e, Component const& c) {
            result.push_back(c);
        });

        return result;
    }

    /// Очищает все entities.
    auto clear() -> void {
        world_.delete_with(flecs::Wildcard);
    }

    /// Получает Flecs world.
    [[nodiscard]] auto raw() -> flecs::world& {
        return world_;
    }

private:
    flecs::world world_;
};

} // namespace projectv::test
```

### 4.2 Пример использования FakeWorld

```cpp
// tests/systems/chunk_system_test.cpp

import std;
import ProjectV.Test.FakeWorld;
import ProjectV.Test.Framework;
import ProjectV.Gameplay.Components;
import glm;

namespace projectv::test {

/// Тест для ChunkLoadingSystem.
class ChunkLoadingSystemTest {
public:
    auto run() -> TestResult {
        FakeWorld world;

        // Создаём player entity
        auto player = world.create_entity(
            TransformComponent{.position{100.0f, 50.0f, 200.0f}},
            PlayerComponent{.move_speed = 5.0f}
        );

        // Регистрируем ChunkLoadingSystem
        world.register_system<TransformComponent, VoxelChunkComponent>(
            "ChunkLoadingSystem",
            [](flecs::entity e, TransformComponent const& t, VoxelChunkComponent& chunk) {
                // Обновляем chunk координаты на основе позиции
                chunk.chunk_x = static_cast<int32_t>(t.position.x / 32);
                chunk.chunk_y = static_cast<int32_t>(t.position.y / 32);
                chunk.chunk_z = static_cast<int32_t>(t.position.z / 32);
            }
        );

        // Выполняем 1 тик
        world.tick(1);

        // Проверяем результат
        auto const* chunk = world.get<VoxelChunkComponent>(player);

        // Player at (100, 50, 200) → chunk (3, 1, 6) with 32³ chunks
        PV_EXPECT_EQ(chunk->chunk_x, 3);
        PV_EXPECT_EQ(chunk->chunk_y, 1);
        PV_EXPECT_EQ(chunk->chunk_z, 6);

        return TestResult{.passed = true, .message = "Chunk coordinates correct"};
    }
};

} // namespace projectv::test
```

---

## 5. Snapshot Testing для CA симуляции

### 5.1 Концепция Snapshot Testing

```cpp
// ProjectV.Test.Snapshot.cppm
export module ProjectV.Test.Snapshot;

import std;
import ProjectV.Simulation.CellularAutomata;

export namespace projectv::test {

/// Snapshot системы для детерминистических тестов.
export class SnapshotTester {
public:
    /// Сохраняет snapshot симуляции.
    static auto save(
        simulation::CASimulator const& sim,
        std::filesystem::path const& path
    ) -> void {
        std::ofstream file(path, std::ios::binary);

        // Header
        uint32_t magic = 0x534E4150;  // "SNAP"
        uint32_t version = 1;
        file.write(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<char*>(&version), sizeof(version));

        // Dimensions
        uint32_t size_x = sim.size_x();
        uint32_t size_y = sim.size_y();
        uint32_t size_z = sim.size_z();
        file.write(reinterpret_cast<char*>(&size_x), sizeof(size_x));
        file.write(reinterpret_cast<char*>(&size_y), sizeof(size_y));
        file.write(reinterpret_cast<char*>(&size_z), sizeof(size_z));

        // Cell data
        for (uint32_t z = 0; z < size_z; ++z) {
            for (uint32_t y = 0; y < size_y; ++y) {
                for (uint32_t x = 0; x < size_x; ++x) {
                    auto cell = sim.get_cell(x, y, z);
                    file.write(reinterpret_cast<char*>(&cell), sizeof(cell));
                }
            }
        }
    }

    /// Загружает snapshot.
    static auto load(std::filesystem::path const& path)
        -> std::expected<simulation::CASimulator, std::string> {

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected("Cannot open snapshot file");
        }

        // Header
        uint32_t magic, version;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));

        if (magic != 0x534E4150) {
            return std::unexpected("Invalid snapshot magic number");
        }

        // Dimensions
        uint32_t size_x, size_y, size_z;
        file.read(reinterpret_cast<char*>(&size_x), sizeof(size_x));
        file.read(reinterpret_cast<char*>(&size_y), sizeof(size_y));
        file.read(reinterpret_cast<char*>(&size_z), sizeof(size_z));

        simulation::CASimulator sim(size_x, size_y, size_z);

        // Cell data
        for (uint32_t z = 0; z < size_z; ++z) {
            for (uint32_t y = 0; y < size_y; ++y) {
                for (uint32_t x = 0; x < size_x; ++x) {
                    simulation::CellState cell;
                    file.read(reinterpret_cast<char*>(&cell), sizeof(cell));
                    sim.set_cell(x, y, z, cell);
                }
            }
        }

        return sim;
    }

    /// Сравнивает два snapshot'а.
    static auto compare(
        simulation::CASimulator const& a,
        simulation::CASimulator const& b,
        float epsilon = 1e-5f
    ) -> bool {
        if (a.size_x() != b.size_x() ||
            a.size_y() != b.size_y() ||
            a.size_z() != b.size_z()) {
            return false;
        }

        for (uint32_t z = 0; z < a.size_z(); ++z) {
            for (uint32_t y = 0; y < a.size_y(); ++y) {
                for (uint32_t x = 0; x < a.size_x(); ++x) {
                    auto cell_a = a.get_cell(x, y, z);
                    auto cell_b = b.get_cell(x, y, z);

                    if (std::abs(cell_a.density - cell_b.density) > epsilon ||
                        std::abs(cell_a.velocity_x - cell_b.velocity_x) > epsilon ||
                        std::abs(cell_a.velocity_y - cell_b.velocity_y) > epsilon ||
                        std::abs(cell_a.velocity_z - cell_b.velocity_z) > epsilon) {
                        return false;
                    }
                }
            }
        }

        return true;
    }
};

} // namespace projectv::test
```

---

## 6. Performance Regression Tests

### 6.1 Benchmark Framework

```cpp
// ProjectV.Test.Benchmark.cppm
export module ProjectV.Test.Benchmark;

import std;
import std.chrono;

export namespace projectv::test {

/// Результат бенчмарка.
export struct BenchmarkResult {
    std::string name;
    uint64_t iterations{0};
    double total_time_ms{0.0};
    double avg_time_us{0.0};
    double min_time_us{0.0};
    double max_time_us{0.0};
    double threshold_us{0.0};  // Порог для regression
    bool passed{false};
};

/// Простой бенчмарк.
export template<typename Func>
auto benchmark(
    std::string_view name,
    Func&& func,
    uint64_t iterations = 1000,
    double threshold_us = 100.0
) -> BenchmarkResult {

    using clock = std::chrono::high_resolution_clock;

    std::vector<double> times;
    times.reserve(iterations);

    for (uint64_t i = 0; i < iterations; ++i) {
        auto start = clock::now();
        func();
        auto end = clock::now();

        auto us = std::chrono::duration<double, std::micro>(end - start).count();
        times.push_back(us);
    }

    double total = 0.0;
    double min_t = times[0];
    double max_t = times[0];

    for (auto t : times) {
        total += t;
        min_t = std::min(min_t, t);
        max_t = std::max(max_t, t);
    }

    double avg = total / iterations;

    return BenchmarkResult{
        .name = std::string(name),
        .iterations = iterations,
        .total_time_ms = total / 1000.0,
        .avg_time_us = avg,
        .min_time_us = min_t,
        .max_time_us = max_t,
        .threshold_us = threshold_us,
        .passed = avg <= threshold_us
    };
}

/// Выводит результат бенчмарка.
export auto print_benchmark(BenchmarkResult const& result) -> void {
    std::println("[{}] {} iterations in {:.2f} ms",
        result.passed ? "PASS" : "FAIL",
        result.iterations,
        result.total_time_ms);
    std::println("  avg: {:.2f} us, min: {:.2f} us, max: {:.2f} us",
        result.avg_time_us,
        result.min_time_us,
        result.max_time_us);
    std::println("  threshold: {:.2f} us", result.threshold_us);

    if (!result.passed) {
        std::println("  PERFORMANCE REGRESSION DETECTED!");
    }
}

} // namespace projectv::test
```

### 6.2 Пример бенчмарка

```cpp
// tests/benchmarks/transform_benchmark.cpp

import std;
import glm;
import ProjectV.Test.Benchmark;
import ProjectV.Gameplay.Components;

namespace projectv::test {

int main() {
    // Benchmark: Transform matrix calculation
    TransformComponent transform{
        .position{1.0f, 2.0f, 3.0f},
        .rotation{0.707f, 0.0f, 0.707f, 0.0f},
        .scale{2.0f, 2.0f, 2.0f}
    };

    auto result = benchmark(
        "TransformComponent::to_matrix",
        [&] { transform.to_matrix(); },
        100000,  // iterations
        1.0      // threshold: 1 us (should be very fast)
    );

    print_benchmark(result);

    return result.passed ? 0 : 1;
}

} // namespace projectv::test
```

---

## 7. CMake интеграция

```cmake
# tests/CMakeLists.txt

# === Static Assertions ===
add_executable(projectv_static_assertions
    static_assertions/memory_layout.cpp
)
target_link_libraries(projectv_static_assertions PRIVATE
    ProjectV.Gameplay.Components
    ProjectV.Voxel.Data
)

# === System Tests ===
add_executable(projectv_test_movement
    systems/movement_system_test.cpp
)
target_link_libraries(projectv_test_movement PRIVATE
    ProjectV.Test.Framework
    ProjectV.ECS.Flecs
    ProjectV.Gameplay.Components
    flecs::flecs_static
)

add_executable(projectv_test_fluid_ca
    systems/fluid_ca_system_test.cpp
)
target_link_libraries(projectv_test_fluid_ca PRIVATE
    ProjectV.Test.Framework
    ProjectV.Simulation.CellularAutomata
)

# === Benchmarks ===
add_executable(projectv_benchmark_transform
    benchmarks/transform_benchmark.cpp
)
target_link_libraries(projectv_benchmark_transform PRIVATE
    ProjectV.Test.Benchmark
    ProjectV.Gameplay.Components
)

# === CTest Integration ===
enable_testing()

add_test(NAME StaticAssertions COMMAND projectv_static_assertions)
add_test(NAME MovementSystem COMMAND projectv_test_movement)
add_test(NAME FluidCASystem COMMAND projectv_test_fluid_ca)

# Benchmark tests (fail on regression)
add_test(NAME TransformBenchmark COMMAND projectv_benchmark_transform)
```

---

## Статус

| Компонент                | Статус         | Приоритет |
|--------------------------|----------------|-----------|
| Static Assertions        | Специфицирован | P0        |
| Data-In/Data-Out Pattern | Специфицирован | P0        |
| FakeWorld                | Специфицирован | P0        |
| Snapshot Testing         | Специфицирован | P1        |
| Benchmark Framework      | Специфицирован | P1        |

---

## Ссылки

- [Google Test](https://github.com/google/googletest)
- [Catch2](https://github.com/catchorg/Catch2)
