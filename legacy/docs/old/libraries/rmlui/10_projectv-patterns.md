# Паттерны RmlUi для ProjectV

🔴 **Уровень 3: Продвинутый**

Паттерны HUD для воксельного движка: health bar, inventory, меню, data bindings с ECS.

## HUD для воксельного движка

### Структура документов

```
ui/
├── hud/
│   ├── health_bar.rml
│   ├── crosshair.rml
│   ├── hotbar.rml
│   └── minimap.rml
├── menus/
│   ├── main_menu.rml
│   ├── pause_menu.rml
│   ├── inventory.rml
│   └── settings.rml
├── styles/
│   ├── hud.rcss
│   ├── menus.rcss
│   └── common.rcss
└── sprites/
    ├── icons.png
    └── buttons.png
```

### Health Bar

**hud/health_bar.rml:**

```html
<rml>
<head>
    <link type="text/rcss" href="../styles/hud.rcss"/>
</head>
<body data-model="player">
    <div id="health-container">
        <div id="health-icon"></div>
        <div id="health-bar-wrapper">
            <div id="health-bar-bg">
                <div id="health-bar-fill" style="width: {{health_percent}}%;"></div>
            </div>
            <span id="health-text">{{health}} / {{max_health}}</span>
        </div>
    </div>

    <div id="stamina-container">
        <div id="stamina-bar" style="width: {{stamina_percent}}%;"></div>
    </div>
</body>
</rml>
```

**styles/hud.rcss:**

```css
@import url("common.rcss");

@sprite_sheet hud_icons {
    src: "../sprites/icons.png";
    resolution: 2dp;
    health: 0px 0px 32px 32px;
    stamina: 32px 0px 32px 32px;
}

#health-container {
    position: absolute;
    left: 20dp;
    bottom: 20dp;
    display: flex;
    align-items: center;
    gap: 8dp;
}

#health-icon {
    width: 32dp;
    height: 32dp;
    decorator: image(hud_icons:health);
}

#health-bar-wrapper {
    display: flex;
    flex-direction: column;
    gap: 4dp;
}

#health-bar-bg {
    width: 200dp;
    height: 20dp;
    background: rgba(0, 0, 0, 0.6);
    border: 2dp #444;
    border-radius: 4dp;
    overflow: hidden;
}

#health-bar-fill {
    height: 100%;
    background: linear-gradient(to right, #ff4444, #44ff44);
    transition: width 0.2s ease-out;
}

#health-text {
    font-size: 12dp;
    color: white;
    text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.8);
}

#stamina-container {
    position: absolute;
    left: 60dp;
    bottom: 50dp;
    width: 180dp;
    height: 6dp;
    background: rgba(0, 0, 0, 0.4);
    border-radius: 3dp;
    overflow: hidden;
}

#stamina-bar {
    height: 100%;
    background: #4a9eff;
    transition: width 0.1s ease-out;
}
```

### Crosshair

**hud/crosshair.rml:**

```html
<rml>
<head>
    <link type="text/rcss" href="../styles/hud.rcss"/>
</head>
<body data-model="game">
    <div id="crosshair" data-if="show_crosshair">
        <div class="crosshair-line horizontal"></div>
        <div class="crosshair-line vertical"></div>
    </div>

    <div id="interaction-hint" data-if="can_interact">
        {{interaction_text}}
    </div>
</body>
</rml>
```

```css
#crosshair {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: 20dp;
    height: 20dp;
    pointer-events: none;
}

.crosshair-line {
    position: absolute;
    background: rgba(255, 255, 255, 0.8);
}

.crosshair-line.horizontal {
    width: 20dp;
    height: 2dp;
    top: 50%;
    left: 0;
    transform: translateY(-50%);
}

.crosshair-line.vertical {
    width: 2dp;
    height: 20dp;
    left: 50%;
    top: 0;
    transform: translateX(-50%);
}

#interaction-hint {
    position: absolute;
    top: calc(50% + 40dp);
    left: 50%;
    transform: translateX(-50%);
    padding: 8dp 16dp;
    background: rgba(0, 0, 0, 0.6);
    border-radius: 4dp;
    font-size: 14dp;
    color: white;
    white-space: nowrap;
}
```

### Hotbar (Quick Inventory)

**hud/hotbar.rml:**

```html
<rml>
<head>
    <link type="text/rcss" href="../styles/hud.rcss"/>
</head>
<body data-model="inventory">
    <div id="hotbar">
        <div data-for="slot in hotbar" class="hotbar-slot" data-attr-selected="slot.index == selected_index">
            <div class="slot-number">{{slot.index + 1}}</div>
            <div class="slot-icon" data-if="slot.item">
                <img src="{{slot.item.icon}}"/>
                <span class="slot-count" data-if="slot.item.count > 1">{{slot.item.count}}</span>
            </div>
        </div>
    </div>
</body>
</rml>
```

```css
#hotbar {
    position: absolute;
    bottom: 20dp;
    left: 50%;
    transform: translateX(-50%);
    display: flex;
    gap: 4dp;
    padding: 4dp;
    background: rgba(0, 0, 0, 0.4);
    border-radius: 4dp;
}

.hotbar-slot {
    width: 48dp;
    height: 48dp;
    background: rgba(0, 0, 0, 0.5);
    border: 2dp solid rgba(255, 255, 255, 0.2);
    border-radius: 4dp;
    display: flex;
    align-items: center;
    justify-content: center;
    position: relative;
}

.hotbar-slot[selected="true"] {
    border-color: #4a9eff;
    background: rgba(74, 158, 255, 0.2);
}

.slot-number {
    position: absolute;
    top: 2dp;
    left: 4dp;
    font-size: 10dp;
    color: rgba(255, 255, 255, 0.6);
}

.slot-icon {
    width: 32dp;
    height: 32dp;
}

.slot-count {
    position: absolute;
    bottom: 2dp;
    right: 4dp;
    font-size: 10dp;
    color: white;
    text-shadow: 1px 1px 1px rgba(0, 0, 0, 0.8);
}
```

## Inventory Screen

**menus/inventory.rml:**

```html
<rml>
<head>
    <title>Inventory</title>
    <link type="text/rcss" href="../styles/menus.rcss"/>
</head>
<body data-model="inventory">
    <div id="inventory-panel">
        <div id="inventory-header">
            <h1>Inventory</h1>
            <button id="close-btn" data-event-click="close_inventory">×</button>
        </div>

        <div id="inventory-content">
            <!-- Player slots -->
            <div id="player-slots">
                <div class="equipment-slot" data-for="slot in equipment">
                    <img data-if="slot.item" src="{{slot.item.icon}}"/>
                    <span class="slot-label">{{slot.name}}</span>
                </div>
            </div>

            <!-- Inventory grid -->
            <div id="inventory-grid">
                <div data-for="item in items"
                     class="inventory-item"
                     data-event-click="select_item(item.index)">
                    <img src="{{item.icon}}"/>
                    <span class="item-count" data-if="item.count > 1">{{item.count}}</span>
                </div>
            </div>

            <!-- Item details -->
            <div id="item-details" data-if="selected_item">
                <h2>{{selected_item.name}}</h2>
                <p>{{selected_item.description}}</p>
                <div id="item-actions">
                    <button data-event-click="use_item">Use</button>
                    <button data-event-click="drop_item">Drop</button>
                </div>
            </div>
        </div>
    </div>
</body>
</rml>
```

```css
#inventory-panel {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: 600dp;
    height: 400dp;
    background: rgba(20, 20, 20, 0.95);
    border: 2dp solid #444;
    border-radius: 8dp;
    display: flex;
    flex-direction: column;
}

#inventory-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 16dp;
    border-bottom: 1dp solid #333;
}

#inventory-header h1 {
    margin: 0;
    font-size: 24dp;
    color: white;
}

#close-btn {
    width: 32dp;
    height: 32dp;
    background: none;
    border: none;
    font-size: 24dp;
    color: #888;
    cursor: pointer;
}

#close-btn:hover {
    color: white;
}

#inventory-content {
    display: flex;
    flex: 1;
    padding: 16dp;
    gap: 16dp;
}

#player-slots {
    width: 100dp;
    display: flex;
    flex-direction: column;
    gap: 8dp;
}

.equipment-slot {
    width: 64dp;
    height: 64dp;
    background: rgba(255, 255, 255, 0.05);
    border: 2dp solid #333;
    border-radius: 4dp;
    position: relative;
}

.slot-label {
    position: absolute;
    bottom: -16dp;
    left: 50%;
    transform: translateX(-50%);
    font-size: 10dp;
    color: #666;
}

#inventory-grid {
    flex: 1;
    display: flex;
    flex-wrap: wrap;
    gap: 4dp;
    align-content: flex-start;
}

.inventory-item {
    width: 48dp;
    height: 48dp;
    background: rgba(255, 255, 255, 0.05);
    border: 2dp solid #333;
    border-radius: 4dp;
    cursor: pointer;
    position: relative;
}

.inventory-item:hover {
    border-color: #4a9eff;
}

#item-details {
    width: 180dp;
    padding: 12dp;
    background: rgba(255, 255, 255, 0.03);
    border-radius: 4dp;
}

#item-details h2 {
    margin: 0 0 8dp 0;
    font-size: 16dp;
    color: #4a9eff;
}

#item-details p {
    font-size: 12dp;
    color: #888;
    margin: 0 0 12dp 0;
}
```

## Pause Menu

**menus/pause_menu.rml:**

```html
<rml>
<head>
    <title>Pause Menu</title>
    <link type="text/rcss" href="../styles/menus.rcss"/>
</head>
<body data-model="game">
    <div id="pause-overlay">
        <div id="pause-menu">
            <h1>Paused</h1>

            <div id="menu-buttons">
                <button class="menu-button" data-event-click="resume_game">
                    <span class="button-text">Resume</span>
                </button>

                <button class="menu-button" data-event-click="open_settings">
                    <span class="button-text">Settings</span>
                </button>

                <button class="menu-button" data-event-click="save_game">
                    <span class="button-text">Save Game</span>
                </button>

                <button class="menu-button danger" data-event-click="quit_to_menu">
                    <span class="button-text">Quit to Menu</span>
                </button>
            </div>
        </div>
    </div>
</body>
</rml>
```

```css
#pause-overlay {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.7);
    display: flex;
    align-items: center;
    justify-content: center;
}

#pause-menu {
    width: 300dp;
    padding: 32dp;
    background: rgba(20, 20, 20, 0.95);
    border: 2dp solid #444;
    border-radius: 8dp;
    text-align: center;
}

#pause-menu h1 {
    margin: 0 0 24dp 0;
    font-size: 32dp;
    color: white;
}

#menu-buttons {
    display: flex;
    flex-direction: column;
    gap: 8dp;
}

.menu-button {
    height: 48dp;
    background: linear-gradient(180deg, #3a3a3a, #2a2a2a);
    border: 2dp solid #444;
    border-radius: 4dp;
    color: white;
    font-size: 16dp;
    cursor: pointer;
    transition: background 0.2s, border-color 0.2s;
}

.menu-button:hover {
    background: linear-gradient(180deg, #4a4a4a, #3a3a3a);
    border-color: #4a9eff;
}

.menu-button.danger {
    border-color: #663333;
}

.menu-button.danger:hover {
    background: linear-gradient(180deg, #553333, #442222);
    border-color: #ff4444;
}
```

## Data Bindings с ECS

### Регистрация моделей данных

```cpp
// src/ui/ui_data_models.hpp
#pragma once

#include <flecs.h>
#include <RmlUi/Core.h>

class UIDataModels {
public:
    UIDataModels(Rml::Context* context, flecs::world& world);

    void syncAll();  // Синхронизация всех данных из ECS

private:
    Rml::Context* context_;
    flecs::world& world_;

    // Models
    Rml::DataModelHandle playerModel_;
    Rml::DataModelHandle inventoryModel_;
    Rml::DataModelHandle gameModel_;

    // Cached data
    struct PlayerData {
        int health = 100;
        int maxHealth = 100;
        float stamina = 1.0f;
        Rml::String name = "Player";
    } playerData_;

    struct InventoryItemData {
        Rml::String name;
        Rml::String icon;
        Rml::String description;
        int count = 0;
    };

    struct InventoryData {
        Rml::Vector<InventoryItemData> items;
        Rml::Vector<InventoryItemData> hotbar;
        Rml::Vector<InventoryItemData> equipment;
        int selectedIndex = -1;
    } inventoryData_;

    struct GameData {
        bool showCrosshair = true;
        bool canInteract = false;
        Rml::String interactionText;
        bool isPaused = false;
    } gameData_;

    void setupPlayerModel();
    void setupInventoryModel();
    void setupGameModel();
};
```

### Реализация

```cpp
// src/ui/ui_data_models.cpp

UIDataModels::UIDataModels(Rml::Context* context, flecs::world& world)
    : context_(context), world_(world)
{
    setupPlayerModel();
    setupInventoryModel();
    setupGameModel();
}

void UIDataModels::setupPlayerModel() {
    if (auto model = context_->CreateDataModel("player")) {
        // Регистрация computed variable для процента здоровья
        model.Bind("health", &playerData_.health);
        model.Bind("max_health", &playerData_.maxHealth);
        model.Bind("stamina", &playerData_.stamina);
        model.Bind("name", &playerData_.name);

        // Вычисляемые переменные
        model.BindGetFunc("health_percent",
            [this](Rml::Variant& variant) {
                variant = static_cast<float>(playerData_.health) /
                          static_cast<float>(playerData_.maxHealth) * 100.0f;
            });

        model.BindGetFunc("stamina_percent",
            [this](Rml::Variant& variant) {
                variant = playerData_.stamina * 100.0f;
            });

        playerModel_ = model;
    }
}

void UIDataModels::setupInventoryModel() {
    if (auto model = context_->CreateDataModel("inventory")) {
        // Регистрация типа InventoryItemData
        if (auto itemModel = model.RegisterStruct<InventoryItemData>()) {
            itemModel.RegisterMember("name", &InventoryItemData::name);
            itemModel.RegisterMember("icon", &InventoryItemData::icon);
            itemModel.RegisterMember("description", &InventoryItemData::description);
            itemModel.RegisterMember("count", &InventoryItemData::count);
        }

        // Регистрация массивов
        model.Bind("items", &inventoryData_.items);
        model.Bind("hotbar", &inventoryData_.hotbar);
        model.Bind("equipment", &inventoryData_.equipment);
        model.Bind("selected_index", &inventoryData_.selectedIndex);

        // События
        model.BindEventFunc("select_item",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
                if (!args.empty()) {
                    inventoryData_.selectedIndex = args[0].Get<int>();
                    model.DirtyVariable("selected_index");
                }
            });

        model.BindEventFunc("use_item",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                if (inventoryData_.selectedIndex >= 0) {
                    // Отправить событие в ECS
                    world_.entity().set<UseItemEvent>({.slot = inventoryData_.selectedIndex});
                }
            });

        model.BindEventFunc("drop_item",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                if (inventoryData_.selectedIndex >= 0) {
                    world_.entity().set<DropItemEvent>({.slot = inventoryData_.selectedIndex});
                }
            });

        // Selected item getter
        model.BindGetFunc("selected_item",
            [this](Rml::Variant& variant) {
                if (inventoryData_.selectedIndex >= 0 &&
                    inventoryData_.selectedIndex < static_cast<int>(inventoryData_.items.size())) {
                    variant = inventoryData_.items[inventoryData_.selectedIndex];
                }
            });

        inventoryModel_ = model;
    }
}

void UIDataModels::setupGameModel() {
    if (auto model = context_->CreateDataModel("game")) {
        model.Bind("show_crosshair", &gameData_.showCrosshair);
        model.Bind("can_interact", &gameData_.canInteract);
        model.Bind("interaction_text", &gameData_.interactionText);
        model.Bind("is_paused", &gameData_.isPaused);

        // События меню
        model.BindEventFunc("resume_game",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                world_.entity().set<PauseGameEvent>({.paused = false});
            });

        model.BindEventFunc("open_settings",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                world_.entity().set<OpenMenuEvent>({.menu = "settings"});
            });

        model.BindEventFunc("close_inventory",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                world_.entity().set<OpenMenuEvent>({.menu = "none"});
            });

        gameModel_ = model;
    }
}

void UIDataModels::syncAll() {
    // Sync player data from ECS
    world_.filter<PlayerComponent>().each([this](flecs::entity e, const PlayerComponent& player) {
        bool dirty = false;

        if (playerData_.health != static_cast<int>(player.health)) {
            playerData_.health = static_cast<int>(player.health);
            dirty = true;
        }
        if (playerData_.maxHealth != static_cast<int>(player.maxHealth)) {
            playerData_.maxHealth = static_cast<int>(player.maxHealth);
            dirty = true;
        }
        if (playerData_.stamina != player.stamina) {
            playerData_.stamina = player.stamina;
            dirty = true;
        }

        if (dirty) {
            playerModel_.DirtyVariable("health");
            playerModel_.DirtyVariable("stamina");
        }
    });

    // Sync inventory from ECS
    world_.filter<InventoryComponent>().each([this](flecs::entity e, const InventoryComponent& inv) {
        // Обновление items, hotbar, equipment
        // ...
        inventoryModel_.DirtyVariable("items");
    });

    // Sync game state
    world_.filter<GameStateComponent>().each([this](flecs::entity e, const GameStateComponent& state) {
        if (gameData_.isPaused != state.isPaused) {
            gameData_.isPaused = state.isPaused;
            gameModel_.DirtyVariable("is_paused");
        }
        if (gameData_.canInteract != state.canInteract) {
            gameData_.canInteract = state.canInteract;
            gameModel_.DirtyVariable("can_interact");
        }
        if (gameData_.interactionText.c_str() != state.interactionText) {
            gameData_.interactionText = Rml::String(state.interactionText.c_str());
            gameModel_.DirtyVariable("interaction_text");
        }
    });
}
```

## Анимированные переходы

### Fade-in/Fade-out для меню

```css
#pause-menu {
    opacity: 0;
    transform: scale(0.95);
    transition: opacity 0.2s ease-out, transform 0.2s ease-out;
}

#pause-menu.visible {
    opacity: 1;
    transform: scale(1.0);
}
```

```cpp
// Показ меню с анимацией
document->Show();
auto menu = document->GetElementById("pause-menu");
menu->SetClass("visible", true);
```

### Анимация повреждения

```css
@keyframes damage-flash {
    0% { background-color: rgba(255, 0, 0, 0.5); }
    100% { background-color: transparent; }
}

#damage-overlay {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    pointer-events: none;
}

#damage-overlay.hit {
    animation: damage-flash 0.3s ease-out;
}
```

```cpp
// При получении урона
void onPlayerDamage(int damage) {
    auto overlay = hudDocument->GetElementById("damage-overlay");
    overlay->SetClass("hit", true);

    // Убрать класс после анимации
    setTimeout(300ms, [overlay]() {
        overlay->SetClass("hit", false);
    });
}
```

## Резюме паттернов

| Паттерн                 | Применение                    |
|-------------------------|-------------------------------|
| **Data Model**          | Синхронизация ECS ↔ UI        |
| **Computed Variables**  | health_percent, selected_item |
| **Event Functions**     | use_item, close_menu          |
| **CSS Transitions**     | Плавные анимации UI           |
| **Keyframe Animations** | damage-flash, loading-spinner |
| **Sprite Sheets**       | Иконки, кнопки                |
| **Templates**           | Переиспользуемые окна         |
| **Virtual Lists**       | Длинные списки инвентаря      |
