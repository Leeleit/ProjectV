# Лямбды и Функциональное программирование (Functional C++)

**🟢🟡🔴 Уровни сложности:**

- 🟢 Уровень 1: Основы лямбд, STL алгоритмы, простые пайплайны
- 🟡 Уровень 2: Продвинутые лямбды, ranges, функциональные паттерны для ECS
- 🔴 Уровень 3: Монадические операции, ленивые вычисления, оптимизированные пайплайны для вокселей

Современный C++ предлагает функциональный стиль написания кода, который делает его более кратким, выразительным и менее
склонным к ошибкам. В ProjectV это используется в коллбеках ECS, обработке воксельных данных и создании декларативных
систем.

---

## Уровень 1: Основы лямбд и алгоритмов

### Лямбды (Lambdas)

Лямбда-выражение — это анонимная функция прямо в коде. Она состоит из захвата `[]`, параметров `()` и тела `{}`.

```cpp
// Анатомия лямбды
auto updateHealth = [dt](flecs::entity entity, Health& health) {
    // [dt] - переменная захвачена из внешнего окружения
    health.value -= 10.0f * dt;
};

// В ECS (Flecs) для ProjectV:
world.each([](Transform& transform, const Velocity& velocity) {
    transform.position += velocity.linear * (1.0f / 60.0f);
    transform.rotation = glm::rotate(transform.rotation,
                                     velocity.angular * (1.0f / 60.0f),
                                     glm::vec3(0, 1, 0));
});
```

### Правила захвата:

- `[=]` — Захват всех переменных по значению (копия)
- `[&]` — Захват всех переменных по ссылке (быстро, но следите за временем жизни!)
- `[this]` — Захват текущего объекта класса (для вызова его методов)
- `[x, &y]` — Смешанный захват: `x` по значению, `y` по ссылке

> **Для понимания:** Захват по значению — это копия. Захват по ссылке — это указатель под капотом. Если вы захватили по
> ссылке локальную переменную, а лямбда пережила эту переменную (например, сохранилась в коллбек) — вы получите висячую
> ссылку и UB.

### Алгоритмы STL для воксельных данных

```cpp
// Обработка воксельного чанка
std::vector<Voxel> chunk(CHUNK_VOLUME);

// Подсчёт непрозрачных вокселей
int solidCount = std::count_if(chunk.begin(), chunk.end(),
    [](const Voxel& v) { return v.type != VoxelType::AIR; });

// Найти все воксели с определённым материалом
std::vector<Voxel*> stoneVoxels;
std::copy_if(chunk.begin(), chunk.end(), std::back_inserter(stoneVoxels),
    [](const Voxel& v) { return v.type == VoxelType::STONE; });

// Сортировка вокселей по расстоянию от центра
std::sort(chunk.begin(), chunk.end(),
    [center = glm::vec3(CHUNK_SIZE/2)](const Voxel& a, const Voxel& b) {
        return glm::distance(a.position, center) < glm::distance(b.position, center);
    });
```

---

## Уровень 2: Ranges и функциональные паттерны

### Ranges (C++20)

**Ranges** позволяют писать алгоритмы через пайплайны (`|`), как в Python или C#. Это ленивые вычисления: данные
обрабатываются только когда они нужны.

```cpp
#include <ranges>

// Обработка сущностей в ECS через пайплайн
auto activeEntities = world.entities()
    | std::views::filter([](flecs::entity e) { return e.is_alive(); })
    | std::views::transform([](flecs::entity e) {
        return std::make_pair(e, e.get<Transform>());
    })
    | std::views::filter([](auto pair) {
        return pair.second != nullptr;
    })
    | std::views::take(100);  // Ограничиваем для производительности

for (auto [entity, transform] : activeEntities) {
    // Обработка только активных сущностей с трансформацией
    updateEntity(entity, *transform);
}
```

### std::function и SBO — Осторожно в Hot Paths

> **Для понимания:** `std::function` — это не просто указатель на функцию. Внутри него есть небольшой буфер (обычно
> 16-32 байта), называемый Small Buffer Optimization (SBO). Если вызываемый объект влезает в этот буфер — аллокации кучи
> не происходит. Но если вы захватываете в лямбде много данных — `std::function` выделяет память на куче. В hot path это
> убийца производительности.

```cpp
#include <functional>

// Лямбда без захвата — влезает в SBO
auto lambda1 = []() { return 42; };
std::function<int()> func1 = lambda1;  // SBO, нет аллокации

// Лямбда с большим захватом — НЕ влезает в SBO
std::array<int, 100> bigData;
auto lambda2 = [bigData]() { return bigData[0]; };
std::function<int()> func2 = lambda2;  // АЛЛОКАЦИЯ на куче!

// В hot path это убийца производительности
void processVoxels(std::function<void(Voxel&)> processor);  // ПЛОХО!

// Лучше использовать шаблон:
template<typename Func>
void processVoxels(Func&& processor);  // ХОРОШО: инлайн, нет аллокации
```

> **Метафора:** `std::function` — это "универсальный контейнер для функций". Он как рюкзак: можно положить что угодно.
> Но если рюкзак маленький (SBO), а груз большой — придётся нанять грузовик (аллокация кучи). Для горячих циклов лучше
> использовать шаблоны: они как руки — ничего не прячут, всё видно компилятору, и он может инлайнить.

### Альтернативы std::function

```cpp
// Вариант 1: Шаблон (бинарный раздувание, но быстро)
template<typename Func>
void forEachVoxel(Func&& func) {
    for (auto& voxel : voxels) {
        func(voxel);
    }
}

// Вариант 2: Функциональный указатель (нет SBO, но нет аллокации)
using VoxelProcessor = void(*)(Voxel&);
void forEachVoxel(VoxelProcessor func);

// Вариант 3: C++26 std::function_ref (нет аллокации, ссылка на callable)
#if __cpp_lib_function_ref >= 202306L
#include <functional>
void forEachVoxel(std::function_ref<void(Voxel&)> func);
#endif
```

### Функциональные паттерны для ECS

```cpp
// Функциональная система обработки компонентов
class FunctionalSystem {
public:
    void process(flecs::world& world) {
        // Паттерн: map-reduce для обработки компонентов
        auto totalDamage = world.entities()
            | std::views::filter([](flecs::entity e) { return e.has<Health>(); })
            | std::views::transform([](flecs::entity e) { return e.get<Health>()->damage; })
            | std::ranges::fold_left(0.0f, std::plus<>());

        // Паттерн: filter-transform для создания событий
        auto damageEvents = world.entities()
            | std::views::filter([](flecs::entity e) {
                auto health = e.get<Health>();
                return health && health->damage > 0;
            })
            | std::views::transform([](flecs::entity e) {
                return DamageEvent{e, e.get<Health>()->damage};
            });

        // Обработка событий
        for (auto event : damageEvents) {
            processDamageEvent(event);
        }
    }
};
```

### Лямбды с шаблонами

```cpp
// Шаблонная лямбда для обработки любых компонентов
auto componentProcessor = []<typename T>(flecs::entity entity, T& component) {
    if constexpr (std::is_same_v<T, Transform>) {
        component.position += glm::vec3(0, -9.81f, 0) * (1.0f / 60.0f);
    } else if constexpr (std::is_same_v<T, Health>) {
        component.value = std::max(0.0f, component.value);
    }
    // Обработка других типов компонентов...
};

// Использование в системе
world.each([&](flecs::entity entity) {
    entity.each(componentProcessor);
});
```

---

## Уровень 3: Монадические операции и оптимизации

### Ленивые вычисления для вокселей

```cpp
// Ленивый пайплайн для обработки вокселей
class VoxelPipeline {
    std::vector<Voxel> chunk_;

public:
    auto getVisibleVoxels() const {
        return chunk_
            | std::views::filter([](const Voxel& v) { return v.type != VoxelType::AIR; })
            | std::views::filter([](const Voxel& v) { return v.light > 0; })
            | std::views::transform([](const Voxel& v) -> VisibleVoxel {
                return {v.position, v.type, v.light};
            });
    }

    auto getCollidableVoxels() const {
        return chunk_
            | std::views::filter([](const Voxel& v) {
                return v.type != VoxelType::AIR && v.type != VoxelType::WATER;
            })
            | std::views::transform([](const Voxel& v) -> CollisionVoxel {
                return {v.position, getCollisionShape(v.type)};
            });
    }
};

// Использование: вычисления происходят только при итерации
for (auto voxel : pipeline.getVisibleVoxels()) {
    renderVoxel(voxel);
}
```

> **Метафора:** Ленивые вычисления — это "рецепт", а не "блюдо". Когда вы пишете `chunk_ | filter | transform`, вы не
> готовите еду — вы пишете рецепт. Блюдо готовится только когда вы начинаете есть (итерировать). Это экономит ресурсы:
> если вы не доели (не прошли весь range), остаток не готовится.

### std::expected — Монада для обработки ошибок (C++23)

> **Для понимания:** В функциональном программировании `Either<E, T>` — это тип, который содержит либо ошибку `E`, либо
> значение `T`. В C++23 это `std::expected<T, E>`. Это монада: у неё есть операции `and_then` (цепочка) и `or_else` (
> обработка ошибки).

```cpp
#if __cpp_lib_expected >= 202202L
#include <expected>

// Использование std::expected вместо исключений
std::expected<VkBuffer, VulkanError> createBuffer(VkDevice device, const BufferCreateInfo& info) {
    VkBuffer buffer;
    if (vkCreateBuffer(device, &info, nullptr, &buffer) != VK_SUCCESS) {
        return std::unexpected(VulkanError::BufferCreationFailed);
    }
    return buffer;
}

// Цепочка операций через and_then
auto result = createBuffer(device, bufferInfo)
    .and_then([&](VkBuffer buffer) -> std::expected<VkDeviceMemory, VulkanError> {
        return allocateMemory(device, buffer);
    })
    .and_then([&](VkDeviceMemory memory) -> std::expected<void, VulkanError> {
        return bindBufferMemory(device, buffer, memory);
    });

// Проверка результата
if (result) {
    // Успех
} else {
    // Ошибка: result.error()
}
#endif
```

### Параллельные алгоритмы для воксельной обработки

```cpp
// Параллельная обработка чанков
void processChunksParallel(std::vector<VoxelChunk>& chunks) {
    std::for_each(std::execution::par, chunks.begin(), chunks.end(),
        [](VoxelChunk& chunk) {
            // Каждый чанк обрабатывается в отдельном потоке
            chunk.generateMesh();
            chunk.calculateLighting();
            chunk.updateCollision();
        });
}

// Параллельный reduce для статистики
auto chunkStats = std::transform_reduce(
    std::execution::par,
    chunks.begin(), chunks.end(),
    ChunkStats{},
    [](ChunkStats a, ChunkStats b) {
        return ChunkStats{
            a.voxelCount + b.voxelCount,
            a.solidCount + b.solidCount,
            std::max(a.maxLight, b.maxLight)
        };
    },
    [](const VoxelChunk& chunk) {
        return ChunkStats{
            chunk.voxelCount(),
            chunk.solidVoxelCount(),
            chunk.maxLightLevel()
        };
    });
```

---

## Для ProjectV

### Функциональные системы ECS

```cpp
// Декларативная система физики
class PhysicsSystem {
public:
    void update(flecs::world& world, float dt) {
        // Паттерн: filter-map-reduce для физики
        auto totalForce = world.entities()
            | std::views::filter([](flecs::entity e) {
                return e.has<Transform>() && e.has<RigidBody>();
            })
            | std::views::transform([dt](flecs::entity e) {
                auto [transform, body] = e.get<Transform, RigidBody>();
                return calculateForce(*transform, *body, dt);
            })
            | std::ranges::fold_left(glm::vec3(0), std::plus<>());

        // Применение сил
        world.each([dt, &totalForce](flecs::entity e, Transform& t, RigidBody& b) {
            applyForce(t, b, totalForce, dt);
        });
    }
};
```

**Связь с философией:** Лямбды в ECS — это пример композиции вместо наследования.
См. [04_ecs-philosophy.md](../../philosophy/04_ecs-philosophy.md).

### Воксельные пайплайны

```cpp
// Пайплайн для рендеринга вокселей
class VoxelRenderPipeline {
public:
    auto getRenderData(const VoxelChunk& chunk) const {
        return chunk.voxels()
            | std::views::filter(&Voxel::isVisible)      // Только видимые
            | std::views::transform(&Voxel::toRenderData) // Конвертация
            | std::views::filter([](const RenderData& rd) {
                return rd.distanceToCamera < 100.0f;     // Обрезка по расстоянию
            })
            | std::views::chunk(1024);                   // Группировка для инстанцирования
    }
};
```

---

## Распространённые ошибки и решения

### Ошибка: Захват по ссылке уничтоженных объектов

**Проблема:** `[&]` захватывает локальные переменные, которые уничтожаются.
**Решение:** Используйте захват по значению или убедитесь в времени жизни объектов.

### Ошибка: std::function в hot path

**Проблема:** Аллокации кучи из-за SBO overflow.
**Решение:** Используйте шаблоны или указатели вместо `std::function`.

### Ошибка: Избыточные копирования в пайплайнах

**Проблема:** `transform` создаёт временные объекты на каждом шаге.
**Решение:** Используйте `std::views::transform` с ссылками или перемещением.

### Ошибка: Неучтённая ленивость ranges

**Проблема:** Предполагается, что пайплайн выполняется сразу.
**Решение:** Помните, что вычисления происходят только при итерации.

### Ошибка: Параллелизм с разделяемым состоянием

**Проблема:** Параллельные алгоритмы с общими данными без синхронизации.
**Решение:** Используйте thread-local данные или атомарные операции.
