# Тестирование

Документ описывает правила тестирования в высокопроизводительных движках.

---

## Что тестировать

### Системы ECS

Классические unit-тесты методов в игровом движке малоценны.
`pos += vel * dt` тривиальна. Сложность — в комбинациях компонентов и
взаимодействии систем.

Правильный подход:

1. Создать мир (World).
2. Добавить сущности с нужным набором компонентов.
3. Вызвать одну итерацию системы (`world.progress(dt)`).
4. Проверить изменения компонентов.

Тест «чёрного ящика» — реальный сценарий использования.

```cpp
TEST(MovementSystem, UpdatesPositionFromVelocity) {
    flecs::world world;
    auto entity = world.entity().set<Position>({0, 0, 0})
                                   .set<Velocity>({1, 0, 0});

    MovementSystem::update(world, 1.0f);

    const auto* pos = entity.get<Position>();
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
}
```

### Инварианты данных (static_assert)

Проверка layout-а структур на этапе компиляции.

```cpp
struct Voxel {
    uint32_t material_id;
    uint32_t metadata;
    static_assert(sizeof(Voxel) == 8);
    static_assert(alignof(Voxel) == 4);
    static_assert(std::is_trivial_v<Voxel>);
}
```

Если кто-то добавит поле и нарушит layout, компиляция упадёт.

### Алгоритмы с нетривиальной логикой

Greedy meshing, SVO compression, chunk algorithms — сложная логика,
граничные случаи. Тесты пишутся до реализации (TDD).

Property-Based Testing: генерация случайных данных, проверка
инвариантов.

```cpp
TEST(GreedyMeshing, VertexCountAlwaysMultipleOfThree) {
    for (auto chunk : generate_random_chunks(1000)) {
        auto mesh = greedy_mesh(chunk);
        EXPECT_EQ(mesh.vertices.size() % 3, 0);
    }
}
```

### Регрессии производительности

Алгоритмические изменения могут незаметно деградировать
производительность. Для критических путей — бенчмарки в Google
Benchmark.

```cpp
static void BM_GreedyMeshing(benchmark::State& state) {
    auto chunk = generate_standard_chunk();
    for (auto _ : state) {
        auto mesh = greedy_mesh(chunk);
        benchmark::DoNotOptimize(mesh);
    }
}
BENCHMARK(BM_GreedyMeshing);
```

Бенчмарк в CI: если новая версия медленнее baseline на >5% — алерт.

---

## Что не тестировать

### Тривиальный код

Сеттеры, геттеры, простые арифметические операции. Не ломается.

### Интеграция с GPU

Vulkan команды нельзя запустить без GPU. Это область ручного тестирования
и RenderDoc.

### UI и визуальное качество

Пиксельный перфекционизм в тестах — rabbit hole. UI — субъективное
качество. Визуальные регрессии ловятся RenderDoc screenshot diff.

### Производительность как функциональность

«Должно работать быстро» — не функциональное требование.

---

## Культура тестирования

Тест — документация того, как система должна работать.

### Когда писать тесты

- Перед реализацией сложных алгоритмов (TDD).
- Сразу при обнаружении бага (regression test).
- При фиксации инвариантов данных.

### Когда не писать тесты

- Для тривиального кода.
- Для UI без автоматизации.
- Для GPU-кода без CI-инфраструктуры.

### Где писать тесты

`tests/` — корневая директория для всех тестов. Каждая подсистема
имеет свой файл.

Google Test framework. Запуск: `ctest`.

---

## Property-Based Testing

Классическое тестирование: вход → выход. Property-based: инвариант
выполняется для всех возможных входов.

### RapidCheck (C++)

```cpp
#include <rapidcheck.h>

rc::check([]() {
    auto chunk = generate_random_chunk();
    auto mesh = greedy_mesh(chunk);

    RC_ASSERT(mesh.vertices.size() % 3 == 0);
    RC_ASSERT(mesh.indices.size() == mesh.vertices.size() / 3);
});
```

RapidCheck генерирует случайные данные и проверяет инварианты.

### Когда использовать

- Алгоритмы с большим пространством входов.
- Инварианты данных (size, alignment, формат).

---

## Метрики покрытия

Coverage — процент строк, покрытых тестами. Целевая метрика для
проекта: 80% для core, 60% для peripheral.

Измеряется через gcov / clang-coverage. В CI: алерт если coverage
падает ниже baseline.

Coverage ≠ качество тестов. Цель — тесты, которые находят баги.

---

## Тестирование в CI

### Шаг 1: Build Debug

```
cmake --build build/linux-clang-debug
```

Любая ошибка компиляции = провал CI.

### Шаг 2: ctest

```
cd build/linux-clang-debug && ctest --output-on-failure
```

Все тесты должны проходить.

### Шаг 3: Validation

Vulkan validation layers включены в Debug. Любое предупреждение = провал
CI.

### Шаг 4: Lint

clang-tidy, clang-format. Любое предупреждение = провал.

### Шаг 5: Benchmark (еженедельно)

Google Benchmark для критических путей. Алерт если регрессия > 5%.

---

## Правила

1. Тесты для сложных алгоритмов — до реализации (TDD).
2. Тесты для инвариантов — static_assert + property-based.
3. Тесты для багов — в момент исправления.
4. Не тестировать тривиальный код.
5. Не тестировать GPU-код без CI-инфраструктуры.
6. Coverage — метрика, не цель.

---

## Пример: применение в движке

В ProjectV тесты — `tests/GraphicsPushConstantsTests.cpp`,
`tests/VoxelWorldTests.cpp`, `tests/FlecsStagingTests.cpp` и другие. Google
Test + Google Benchmark. CI прогоняет ctest, validation layers, clang-tidy.
Benchmark в CI — еженедельно.

---

## Источники и дальнейшее чтение

- **Google Test documentation** — фреймворк тестирования.
  <https://github.com/google/googletest>
- **Google Benchmark** — микро-бенчмарки.
  <https://github.com/google/benchmark>
- **RapidCheck** — property-based testing для C++.
  <https://github.com/emil-e/rapidcheck>
- [22_ecs.md](22_ecs.md) — тестирование ECS систем.
- [94_build-and-ci.md](94_build-and-ci.md) — CI gates.
- [93_performance-methodology.md](93_performance-methodology.md) — порог
  регрессии 5%.