# Gameplay Guides: Обзор

Практические руководства по разработке игровой логики в ProjectV.

## Структура

| Руководство                                        | Уровень        | Описание                     |
|----------------------------------------------------|----------------|------------------------------|
| [01_adding-new-block.md](./01_adding-new-block.md) | 🟢 Базовый     | Добавление нового типа блока |
| 02_player-controls.md                              | 🟡 Средний     | Создание управления игроком  |
| 03_inventory-system.md                             | 🟡 Средний     | Система инвентаря            |
| 04_world-generation.md                             | 🔴 Продвинутый | Генерация мира               |

---

## Архитектура Gameplay Layer

```
┌─────────────────────────────────────────────────────────────────┐
│                    Gameplay Layer                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Components (Data)          Systems (Logic)                    │
│  ├── TransformComponent     ├── UpdateTransforms               │
│  ├── VelocityComponent      ├── PhysicsSync                    │
│  ├── PlayerComponent        ├── PlayerMovement                 │
│  ├── BlockTypeComponent     ├── BlockInteraction               │
│  └── InventoryComponent     └── InventoryManagement            │
│                                                                 │
│  Blocks (Content)           Events (Communication)             │
│  ├── StoneBlock             ├── OnBlockPlaced                  │
│  ├── DirtBlock              ├── OnBlockDestroyed               │
│  ├── WoodBlock              └── OnBlockInteract                │
│  └── CustomBlock                                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Принципы

### 1. Data-Oriented Design

Компоненты содержат **только данные**, без логики:

```cpp
// ✅ Правильно
struct PlayerComponent {
    float moveSpeed;
    float jumpForce;
    bool isGrounded;
};

// ❌ Неправильно
struct PlayerComponent {
    float moveSpeed;
    void move(float dt);  // Логика не в компоненте!
};
```

### 2. ECS Systems

Логика находится в системах:

```cpp
// Система обрабатывает компоненты
ecs.system<PlayerComponent, VelocityComponent, InputComponent>("PlayerMovement")
    .each([](PlayerComponent& p, VelocityComponent& v, const InputComponent& i) {
        if (i.moveForward) {
            v.linear.z = p.moveSpeed;
        }
    });
```

### 3. Events для связи

Системы общаются через события:

```cpp
// Событие разрушения блока
struct BlockDestroyedEvent {
    glm::ivec3 position;
    uint16_t blockId;
    flecs::entity destroyer;
};

// Observer
ecs.observer<BlockDestroyedEvent>()
    .event(flecs::OnAdd)
    .each([](const BlockDestroyedEvent& e) {
        // Обработка события
    });
```

---

## Базовые компоненты

```cpp
// Transform — позиция, поворот, масштаб
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

// Velocity — скорость
struct VelocityComponent {
    glm::vec3 linear{0.0f};
    glm::vec3 angular{0.0f};
};

// Player — данные игрока
struct PlayerComponent {
    float moveSpeed{5.0f};
    float jumpForce{8.0f};
    float mouseSensitivity{0.1f};
    bool isGrounded{false};
    float health{100.0f};
    float maxHealth{100.0f};
};

// Input — ввод
struct InputComponent {
    bool moveForward{false};
    bool moveBackward{false};
    bool moveLeft{false};
    bool moveRight{false};
    bool jump{false};
    bool interact{false};
    glm::vec2 mouseDelta{0.0f};
};

// Inventory — инвентарь
struct InventoryComponent {
    static constexpr size_t SLOT_COUNT = 36;

    struct Slot {
        uint16_t blockId{0};
        uint16_t count{0};
    };

    std::array<Slot, SLOT_COUNT> slots;
    uint8_t selectedSlot{0};
};

// Block — тип блока
struct BlockTypeComponent {
    uint16_t blockId{0};
    bool isSolid{true};
    bool isTransparent{false};
    bool isBreakable{true};
    float hardness{1.0f};
    float lightLevel{0.0f};
};
```

---

## Базовые системы

```cpp
void registerGameplaySystems(flecs::world& ecs) {
    // 1. Ввод
    ecs.system<InputComponent>("ProcessInput")
        .kind(flecs::PreUpdate)
        .iter([](flecs::iter& it, InputComponent* inputs) {
            auto* sdl = it.world().ctx<SDLInputHandler>();
            for (auto i : it) {
                sdl->updateInput(inputs[i]);
            }
        });

    // 2. Движение игрока
    ecs.system<PlayerComponent, VelocityComponent, InputComponent>("PlayerMovement")
        .kind(flecs::OnUpdate)
        .multi_threaded()
        .each([](PlayerComponent& p, VelocityComponent& v, const InputComponent& i) {
            glm::vec3 moveDir{0.0f};
            if (i.moveForward) moveDir.z += 1.0f;
            if (i.moveBackward) moveDir.z -= 1.0f;
            if (i.moveRight) moveDir.x += 1.0f;
            if (i.moveLeft) moveDir.x -= 1.0f;

            v.linear.x = moveDir.x * p.moveSpeed;
            v.linear.z = moveDir.z * p.moveSpeed;

            if (i.jump && p.isGrounded) {
                v.linear.y = p.jumpForce;
            }
        });

    // 3. Применение скорости
    ecs.system<TransformComponent, VelocityComponent>("ApplyVelocity")
        .kind(flecs::OnUpdate)
        .multi_threaded()
        .each([](TransformComponent& t, const VelocityComponent& v, float dt) {
            t.position += v.linear * dt;
        });

    // 4. Взаимодействие с блоками
    ecs.system<PlayerComponent, TransformComponent, InputComponent>("BlockInteraction")
        .kind(flecs::OnUpdate)
        .iter([](flecs::iter& it, PlayerComponent* players, TransformComponent* transforms, InputComponent* inputs) {
            auto* voxelWorld = it.world().ctx<VoxelWorld>();

            for (auto i : it) {
                if (inputs[i].interact) {
                    auto ray = voxelWorld->raycast(
                        transforms[i].position,
                        getForwardVector(transforms[i]),
                        5.0f
                    );

                    if (ray.hit) {
                        // Разрушение блока
                        voxelWorld->destroyBlock(ray.blockPos);
                    }
                }
            }
        });
}
```

---

## Ссылки

- [Engine Structure](../../architecture/practice/00_engine-structure.md)
- [ECS Philosophy](../../philosophy/04_ecs-philosophy.md)
- [Flecs Patterns](../../libraries/flecs/10_projectv-patterns.md)
