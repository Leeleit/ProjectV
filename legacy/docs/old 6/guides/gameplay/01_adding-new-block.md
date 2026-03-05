# Добавление нового типа блока [🟢 Уровень 1]

**🟢 Уровень 1: Базовый** — Пошаговое руководство по созданию нового блока.

## Обзор

В этом руководстве мы создадим новый блок **"Обсидиан"** — крепкий, тёмный блок с особыми свойствами.

---

## Шаг 1: Определение BlockID

Каждый блок имеет уникальный `blockId`. Резервация ID происходит в `BlockRegistry`:

```cpp
// src/gameplay/blocks/block_registry.hpp
namespace projectv::gameplay {

class BlockRegistry {
public:
    // Зарезервированные ID
    static constexpr uint16_t AIR = 0;
    static constexpr uint16_t STONE = 1;
    static constexpr uint16_t DIRT = 2;
    static constexpr uint16_t GRASS = 3;
    static constexpr uint16_t WOOD = 4;
    static constexpr uint16_t LEAVES = 5;
    static constexpr uint16_t SAND = 6;
    static constexpr uint16_t WATER = 7;
    
    // Модовые блоки начинаются с 1000
    static constexpr uint16_t MOD_BLOCK_START = 1000;
    
    // Добавляем обсидиан
    static constexpr uint16_t OBSIDIAN = 100;
    
    // Регистрация
    static void registerAll(flecs::world& ecs);
    
    // Получение свойств блока
    static const BlockTypeComponent& getProperties(uint16_t blockId);
    
private:
    static std::unordered_map<uint16_t, BlockTypeComponent> blockProperties_;
};

} // namespace projectv::gameplay
```

---

## Шаг 2: Создание компонента блока

```cpp
// src/gameplay/blocks/obsidian.hpp
#pragma once

#include "gameplay/components.hpp"
#include "gameplay/blocks/block_registry.hpp"

namespace projectv::gameplay {

// Определение свойств обсидиана
inline BlockTypeComponent createObsidianBlock() {
    BlockTypeComponent block;
    block.blockId = BlockRegistry::OBSIDIAN;
    block.isSolid = true;
    block.isTransparent = false;
    block.isBreakable = true;
    block.hardness = 50.0f;  // Очень крепкий
    block.lightLevel = 0.0f;
    return block;
}

// Материал для рендеринга
struct ObsidianMaterial {
    glm::vec4 baseColor{0.1f, 0.05f, 0.15f, 1.0f};  // Тёмно-фиолетовый
    float roughness{0.3f};
    float metallic{0.0f};
    
    struct glaze {
        using T = ObsidianMaterial;
        static constexpr auto value = glz::object(
            "baseColor", &T::baseColor,
            "roughness", &T::roughness,
            "metallic", &T::metallic
        );
    };
};

} // namespace projectv::gameplay
```

---

## Шаг 3: Регистрация блока

```cpp
// src/gameplay/blocks/block_registry.cpp
#include "block_registry.hpp"
#include "obsidian.hpp"

namespace projectv::gameplay {

std::unordered_map<uint16_t, BlockTypeComponent> BlockRegistry::blockProperties_;

void BlockRegistry::registerAll(flecs::world& ecs) {
    // Базовые блоки
    blockProperties_[AIR] = BlockTypeComponent{
        .blockId = AIR,
        .isSolid = false,
        .isTransparent = true,
        .isBreakable = false
    };
    
    blockProperties_[STONE] = BlockTypeComponent{
        .blockId = STONE,
        .isSolid = true,
        .hardness = 1.5f
    };
    
    blockProperties_[DIRT] = BlockTypeComponent{
        .blockId = DIRT,
        .isSolid = true,
        .hardness = 0.5f
    };
    
    // ... другие базовые блоки
    
    // Обсидиан
    blockProperties_[OBSIDIAN] = createObsidianBlock();
    
    // Регистрация в ECS как singleton
    ecs.set<BlockRegistry*>(this);
}

const BlockTypeComponent& BlockRegistry::getProperties(uint16_t blockId) {
    static BlockTypeComponent unknown{
        .blockId = 0,
        .isSolid = false
    };
    
    auto it = blockProperties_.find(blockId);
    return it != blockProperties_.end() ? it->second : unknown;
}

} // namespace projectv::gameplay
```

---

## Шаг 4: Текстура блока

### Вариант A: Простая текстура

Поместите текстуру в `assets/textures/blocks/obsidian.png` (16x16 или 32x32 пикселей).

```cpp
// При загрузке текстур
auto obsidianTexture = assetManager->loadTexture("blocks/obsidian");
```

### Вариант B: Procedural texture

```cpp
// src/gameplay/blocks/procedural_textures.hpp
namespace projectv::gameplay {

// Генерация текстуры обсидиана процедурно
std::vector<uint8_t> generateObsidianTexture(uint32_t size = 16) {
    std::vector<uint8_t> data(size * size * 4);
    
    std::mt19937 rng(42);  // Fixed seed для консистентности
    std::uniform_real_distribution<float> noise(0.0f, 1.0f);
    
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            uint32_t idx = (y * size + x) * 4;
            
            // Базовый цвет
            float base = 0.05f + noise(rng) * 0.1f;
            
            // Добавляем трещины
            float crack = noise(rng) > 0.95f ? 0.3f : 0.0f;
            
            // Фиолетовый оттенок
            data[idx + 0] = static_cast<uint8_t>((base + crack) * 255);  // R
            data[idx + 1] = static_cast<uint8_t>(base * 0.5f * 255);      // G
            data[idx + 2] = static_cast<uint8_t>((base * 1.5f + crack) * 255);  // B
            data[idx + 3] = 255;  // A
        }
    }
    
    return data;
}

} // namespace projectv::gameplay
```

---

## Шаг 5: Обработка взаимодействия

```cpp
// src/gameplay/systems/block_interaction.cpp
namespace projectv::gameplay {

void registerBlockInteractionSystem(flecs::world& ecs) {
    ecs.system<PlayerComponent, TransformComponent, InputComponent, InventoryComponent>(
        "BlockInteractionSystem")
        .kind(flecs::OnUpdate)
        .iter([](flecs::iter& it, 
                 PlayerComponent* players,
                 TransformComponent* transforms,
                 InputComponent* inputs,
                 InventoryComponent* inventories) {
            
            auto* voxelWorld = it.world().ctx<voxel::VoxelWorld>();
            auto* registry = it.world().ctx<BlockRegistry*>();
            
            for (auto i : it) {
                // Разрушение блока (левая кнопка мыши)
                if (inputs[i].attack) {
                    auto ray = voxelWorld->raycast(
                        transforms[i].position,
                        getForwardVector(transforms[i]),
                        5.0f
                    );
                    
                    if (ray.hit) {
                        uint16_t blockId = voxelWorld->getBlock(ray.blockPos);
                        const auto& props = registry->getProperties(blockId);
                        
                        // Обсидиан требует алмазную кирку
                        if (blockId == BlockRegistry::OBSIDIAN) {
                            // Проверка инструмента
                            auto& selected = inventories[i].slots[inventories[i].selectedSlot];
                            if (selected.toolType != ToolType::DIAMOND_PICKAXE) {
                                // Нельзя разрушить
                                continue;
                            }
                        }
                        
                        // Время разрушения зависит от hardness
                        float breakTime = props.hardness * 0.5f;
                        
                        // Создаём событие
                        it.world().entity()
                            .set<BlockDestroyedEvent>({
                                .position = ray.blockPos,
                                .blockId = blockId,
                                .destroyer = it.entity(i)
                            });
                        
                        // Удаляем блок
                        voxelWorld->setBlock(ray.blockPos, BlockRegistry::AIR);
                        
                        // Добавляем в инвентарь
                        addToInventory(inventories[i], blockId, 1);
                    }
                }
                
                // Установка блока (правая кнопка мыши)
                if (inputs[i].use) {
                    auto ray = voxelWorld->raycast(
                        transforms[i].position,
                        getForwardVector(transforms[i]),
                        5.0f
                    );
                    
                    if (ray.hit) {
                        // Позиция соседнего блока
                        glm::ivec3 placePos = ray.blockPos + ray.normal;
                        
                        // Проверка на коллизию с игроком
                        if (!playerIntersects(transforms[i], placePos)) {
                            auto& selected = inventories[i].slots[inventories[i].selectedSlot];
                            
                            if (selected.count > 0) {
                                voxelWorld->setBlock(placePos, selected.blockId);
                                selected.count--;
                                
                                // Событие установки
                                it.world().entity()
                                    .set<BlockPlacedEvent>({
                                        .position = placePos,
                                        .blockId = selected.blockId,
                                        .placer = it.entity(i)
                                    });
                            }
                        }
                    }
                }
            }
        });
}

// События
struct BlockDestroyedEvent {
    glm::ivec3 position;
    uint16_t blockId;
    flecs::entity destroyer;
};

struct BlockPlacedEvent {
    glm::ivec3 position;
    uint16_t blockId;
    flecs::entity placer;
};

} // namespace projectv::gameplay
```

---

## Шаг 6: Специальное поведение (опционально)

Для блоков с особым поведением используйте observers:

```cpp
// Обсидиан создаёт портал при определённой конфигурации
ecs.observer<BlockPlacedEvent>()
    .event(flecs::OnAdd)
    .iter([](flecs::iter& it, const BlockPlacedEvent* events) {
        auto* voxelWorld = it.world().ctx<voxel::VoxelWorld>();
        
        for (auto i : it) {
            if (events[i].blockId == BlockRegistry::OBSIDIAN) {
                // Проверка конфигурации портала
                if (isPortalFrame(voxelWorld, events[i].position)) {
                    // Создание портала
                    createNetherPortal(voxelWorld, events[i].position);
                }
            }
        }
    });
```

---

## Шаг 7: Тестирование

```cpp
// tests/block_test.cpp
#include <doctest.h>
#include "gameplay/blocks/obsidian.hpp"
#include "gameplay/blocks/block_registry.hpp"

TEST_CASE("Obsidian block properties") {
    using namespace projectv::gameplay;
    
    auto obsidian = createObsidianBlock();
    
    CHECK(obsidian.blockId == BlockRegistry::OBSIDIAN);
    CHECK(obsidian.isSolid == true);
    CHECK(obsidian.isTransparent == false);
    CHECK(obsidian.hardness == 50.0f);
}

TEST_CASE("Obsidian requires diamond pickaxe") {
    flecs::world ecs;
    BlockRegistry::registerAll(ecs);
    
    // ... test code
}
```

---

## Чек-лист

- [ ] Определён `blockId` в `BlockRegistry`
- [ ] Создан компонент с свойствами блока
- [ ] Блок зарегистрирован в `registerAll()`
- [ ] Текстура добавлена или сгенерирована
- [ ] Обработка взаимодействия реализована
- [ ] Тесты написаны

---

## Ссылки

- [Gameplay Overview](./00_overview.md)
- [Engine Structure](../../architecture/practice/00_engine-structure.md)
- [Voxel Data Philosophy](../../philosophy/07_voxel-data-philosophy.md)