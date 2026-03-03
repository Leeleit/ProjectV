# Шаблоны и Концепты (Templates & Concepts)

**🟢🟡🔴 Уровни сложности:**

- 🟢 Уровень 1: Основы шаблонов, `constexpr`, простые концепты
- 🟡 Уровень 2: Специализация шаблонов, Deducing This, продвинутые концепты
- 🔴 Уровень 3: Метапрограммирование, SFINAE, концепты для Vulkan/ECS

Шаблоны в C++ — это способ писать код один раз для многих типов данных. В ProjectV шаблоны критичны для универсального
ECS, воксельных данных и безопасных обёрток Vulkan.

---

## Уровень 1: Основы шаблонов

### Шаблонные функции и классы

Шаблонные функции и классы позволяют работать с любым типом, который поддерживает необходимые операции.

```cpp
// Шаблонная функция для линейной интерполяции (Lerp)
template <typename T>
T lerp(T start, T end, float t) {
    return start + (end - start) * t;
}

// Использование в ProjectV
auto pos = lerp(glm::vec3(0), glm::vec3(10), 0.5f);  // Интерполяция позиции
auto health = lerp(0.0f, 100.0f, 0.5f);              // Интерполяция здоровья
```

### Константы времени компиляции (constexpr)

Заменяйте макросы (`#define`) на `constexpr`. Это типизировано и проверяется компилятором.

```cpp
// Воксельные константы для ProjectV
constexpr int CHUNK_SIZE = 32;
constexpr float GRAVITY = -9.81f;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;  // 32768 вокселей

// Предвычисленные таблицы для освещения
constexpr std::array<float, 8> LIGHT_FALLOFF = {1.0f, 0.8f, 0.6f, 0.4f, 0.3f, 0.2f, 0.1f, 0.05f};
```

> **Для понимания:** `constexpr` — это не просто константа. Это инструкция компилятору: "Вычисли это на этапе
> компиляции". В runtime это уже готовое число, а не выражение. Для `CHUNK_VOLUME` компилятор подставит `32768`, а не
> умножает три числа каждый раз.

### Простые концепты (C++20)

Концепты решают проблему непонятных ошибок шаблонов, явно указывая требования к типам.

```cpp
// Определяем требование: тип должен уметь складываться
template<typename T>
concept Summable = requires(T a, T b) {
    { a + b } -> std::same_as<T>;
};

// Функция примет ТОЛЬКО суммируемые типы
template<Summable T>
T add(T a, T b) {
    return a + b;
}

// add(Transform{}, Transform{}); // ОШИБКА: Transform не Summable. Четко и понятно!
```

> **Метафора:** Концепты — это "договор" для шаблонов. Без концепта функция говорит: "Принимаю что угодно, потом
> посмотрим". С концептом: "Принимаю только то, что умеет складываться". Ошибка обнаруживается на первой строке, а не в
> глубине STL с трёхстраничным сообщением.

---

## Уровень 2: Специализация и продвинутые концепты

### Специализация шаблонов

Можно создавать специализированные версии шаблонов для конкретных типов.

```cpp
// Общий шаблон
template<typename T>
struct ComponentSerializer {
    static void serialize(const T& component) {
        // Общая реализация
    }
};

// Специализация для glm::vec3
template<>
struct ComponentSerializer<glm::vec3> {
    static void serialize(const glm::vec3& vec) {
        // Оптимизированная реализация для векторов
        std::cout << "Vector: " << vec.x << ", " << vec.y << ", " << vec.z;
    }
};
```

### Deducing This (C++23/26) — Замена CRTP

> **Для понимания:** CRTP (Curiously Recurring Template Pattern) — это хак для статического полиморфизма. Пишешь
`class Derived : Base<Derived>` — и компилятор "видит" производный тип в базовом классе. Это работает, но выглядит как
> заклинание. C++23/26 вводит **Deducing This**: явный параметр `this` в методах. Теперь вместо шаблонной магии — чистый
> синтаксис.

#### Старый способ: CRTP

```cpp
// CRTP: работает, но выглядит странно
template<typename Derived>
class ComponentBase {
public:
    void update(float dt) {
        static_cast<Derived*>(this)->onUpdate(dt);  // Приведение типа
    }
};

// Использование: класс наследуется от шаблона с самим собой
class TransformComponent : public ComponentBase<TransformComponent> {
public:
    void onUpdate(float dt) {
        // Логика обновления
    }
};
```

#### Новый способ: Deducing This (C++23)

```cpp
// Deducing This: чистый синтаксис
class ComponentBase {
public:
    template<typename Self>
    void update(this Self&& self, float dt) {
        self.onUpdate(dt);  // Прямой вызов, без каста
    }
};

// Использование: обычное наследование
class TransformComponent : public ComponentBase {
public:
    void onUpdate(float dt) {
        // Логика обновления
    }
};
```

**Преимущества Deducing This:**

1. **Читаемость:** Нет странного `class Foo : Base<Foo>`
2. **Меньше кода:** Не нужно повторять имя класса
3. **Поддержка const/volatile:** `this Self const& self` автоматически
4. **Perfect forwarding:** `this Self&& self` работает корректно

> **Связь с философией:** Это пример "zero-cost abstraction"
> из [02_zero-cost-abstractions.md](../../philosophy/02_zero-cost-abstractions.md). Deducing This даёт статический
> полиморфизм без runtime-накладных расходов виртуальных функций.

### Концепты для ECS компонентов

```cpp
// Концепт для компонентов, которые можно сериализовать
template<typename T>
concept SerializableComponent = requires(T component) {
    { T::serialize(component) } -> std::same_as<std::vector<uint8_t>>;
    { T::deserialize(std::vector<uint8_t>{}) } -> std::same_as<T>;
};

// Использование в системе сохранения
template<SerializableComponent T>
void saveComponent(flecs::entity entity) {
    auto& component = entity.get<T>();
    auto data = T::serialize(component);
    // Сохраняем данные
}
```

---

## Уровень 3: Метапрограммирование и специализированные паттерны

### SFINAE (Substitution Failure Is Not An Error)

Старый способ ограничения шаблонов, который всё ещё полезен в некоторых случаях.

```cpp
// SFINAE для проверки наличия метода
template<typename T, typename = void>
struct HasUpdateMethod : std::false_type {};

template<typename T>
struct HasUpdateMethod<T, std::void_t<decltype(std::declval<T>().update(0.0f))>>
    : std::true_type {};

// Использование
template<typename T>
void processSystem(T& system) {
    if constexpr (HasUpdateMethod<T>::value) {
        system.update(1.0f / 60.0f);
    }
}
```

> **Для понимания:** SFINAE — это "хак компилятора". Если подстановка типа в шаблон вызывает ошибку — это не ошибка
> компиляции, просто этот вариант шаблона отбрасывается. С концептами код становится чище, но SFINAE всё ещё нужен для
> сложных случаев.

### if constexpr (C++17) — Compile-time ветвление

```cpp
// Условная компиляция внутри функции
template<typename T>
void process(T& value) {
    if constexpr (std::is_integral_v<T>) {
        value *= 2;  // Только для целых
    } else if constexpr (std::is_floating_point_v<T>) {
        value *= 1.5f;  // Только для float/double
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
        value *= 2.0f;  // Только для vec3
    }
    // Для других типов — пустая функция
}
```

> **Метафора:** `if constexpr` — это "разветвитель времени компиляции". В runtime веток нет: компилятор оставляет только
> одну, подходящую под тип. Никаких накладных расходов на проверку условий.

### Концепты для Vulkan обёрток

```cpp
// Концепт для типов Vulkan, которые можно безопасно уничтожать
template<typename T>
concept VulkanDestroyable = requires(T obj, VkDevice device) {
    { obj.destroy(device) } -> std::same_as<void>;
    { obj.operator bool() } -> std::same_as<bool>;
};

// Безопасная обёртка для Vulkan объектов
template<VulkanDestroyable T>
class VulkanHandle {
    T handle_;
    VkDevice device_;

public:
    VulkanHandle(VkDevice device, T handle) : device_(device), handle_(handle) {}

    ~VulkanHandle() {
        if (handle_) {
            handle_.destroy(device_);
        }
    }

    // Запрещаем копирование
    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

    // Разрешаем перемещение
    VulkanHandle(VulkanHandle&& other) noexcept
        : device_(other.device_), handle_(std::exchange(other.handle_, T{})) {}

    T get() const { return handle_; }
    operator bool() const { return static_cast<bool>(handle_); }
};
```

### Шаблонные воксельные алгоритмы

```cpp
// Шаблонный алгоритм для обработки воксельных чанков
template<typename VoxelType, typename Visitor>
void processVoxelChunk(VoxelType* chunk, int size, Visitor&& visitor) {
    for (int z = 0; z < size; ++z) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                int index = z * size * size + y * size + x;
                visitor(chunk[index], x, y, z);
            }
        }
    }
}

// Использование с лямбдой
processVoxelChunk(voxelData, CHUNK_SIZE, [](auto& voxel, int x, int y, int z) {
    if (voxel.type == VoxelType::AIR) return;
    // Обработка твёрдого вокселя
});
```

---

## Для ProjectV

### Шаблоны в ECS архитектуре

```cpp
// Шаблонная система для обработки компонентов
template<typename... Components>
class ProcessingSystem {
public:
    void process(flecs::world& world, float dt) {
        world.each([dt](flecs::entity entity, Components&... components) {
            // Обработка всех компонентов сразу
            processComponents(entity, dt, components...);
        });
    }

private:
    void processComponents(flecs::entity entity, float dt, Components&... components) {
        // Реализация обработки
    }
};

// Использование
class MovementSystem : public ProcessingSystem<TransformComponent, VelocityComponent> {
    // Специализированная логика движения
};
```

**Связь с философией:** Шаблонные системы в ECS — это пример композиции вместо наследования.
См. [04_ecs-philosophy.md](../../philosophy/04_ecs-philosophy.md).

### Концепты для воксельных данных

```cpp
// Концепт для типов вокселей
template<typename T>
concept VoxelType = requires(T voxel) {
    { voxel.type } -> std::same_as<uint8_t>;
    { voxel.light } -> std::same_as<uint8_t>;
    { T::AIR } -> std::same_as<uint8_t>;
    { T::STONE } -> std::same_as<uint8_t>;
};

// Обобщённая функция для мешинга
template<VoxelType Voxel>
void generateMesh(const Voxel* chunk, MeshData& mesh) {
    // Алгоритм жадного мешинга, работающий с любым типом вокселей
}
```

---

## Распространённые ошибки и решения

### Ошибка: Слишком сложные шаблоны

**Проблема:** `template<typename T, typename U, typename V, typename = void>` — нечитаемо.
**Решение:** Используйте концепты для явных требований.

### Ошибка: Отсутствие специализации для указателей

**Проблема:** Шаблон работает с `T`, но ломается с `T*`.
**Решение:** Добавьте специализацию или используйте `std::remove_pointer_t`.

### Ошибка: Непонятные ошибки компиляции

**Проблема:** Тысячи строк ошибок при несоответствии типов.
**Решение:** Используйте концепты для ясных сообщений об ошибках.

### Ошибка: Избыточные инстанциации

**Проблема:** Один шаблон инстанцируется для многих почти одинаковых типов.
**Решение:** Используйте `if constexpr` для условной компиляции.

### Ошибка: Code bloat от шаблонов

**Проблема:** Бинарник раздувается от множества инстанциаций.
**Решение:** Выносите общую логику в нешаблонные функции, используйте `inline` осторожно.
