# Modding API Specification

---

## Обзор

Документ описывает архитектуру **Modding API** для ProjectV. Система модов позволит:

- Добавлять новые типы блоков и материалов
- Создавать пользовательские инструменты и предметы
- Расширять игровой UI
- Добавлять новые механики через scripting

---

## 1. Архитектура

### 1.1 Принципы

| Принцип          | Описание                               |
|------------------|----------------------------------------|
| **Sandboxed**    | Моды изолированы от核心 engine кода      |
| **Data-driven**  | Контент определяется данными, не кодом |
| **Hot-loadable** | Моды загружаются без перезапуска       |
| **Versioned**    | Версионирование API для совместимости  |

### 1.2 Структура мода

```
mods/
└── my_mod/
    ├── manifest.json       # Метаданные мода
    ├── blocks/             # Новые блоки
    │   ├── custom_stone.json
    │   └── custom_ore.json
    ├── items/              # Новые предметы
    │   └── magic_wand.json
    ├── materials/          # Новые материалы
    │   └── glow_metal.mat
    ├── textures/           # Текстуры
    │   └── custom_stone.png
    ├── scripts/            # Lua скрипты (опционально)
    │   └── init.lua
    └── ui/                 # UI расширения
        └── custom_panel.rml
```

---

## 2. Manifest Format

### 2.1 Структура

```json
{
    "id": "com.example.my_mod",
    "name": "My Awesome Mod",
    "version": "1.0.0",
    "minEngineVersion": "0.1.0",
    "description": "Adds custom blocks and items",
    "author": "Developer Name",
    "dependencies": [
        {
            "id": "com.example.core_lib",
            "version": ">=1.0.0"
        }
    ],
    "entryPoints": {
        "init": "scripts/init.lua",
        "blocks": "blocks/",
        "items": "items/",
        "materials": "materials/"
    },
    "permissions": [
        "blocks.create",
        "items.create",
        "materials.create",
        "ui.extend"
    ]
}
```

### 2.2 Валидация

```cpp
export namespace projectv::modding {

struct ModManifest {
    std::string id;              // com.example.my_mod
    std::string name;
    semver::version version;
    semver::version minEngineVersion;
    std::string description;
    std::string author;
    std::vector<Dependency> dependencies;
    EntryPoints entryPoints;
    std::vector<std::string> permissions;

    /// Валидация manifest
    [[nodiscard]] auto validate() const noexcept
        -> std::expected<void, ManifestError>;

    /// Проверка совместимости с версией движка
    [[nodiscard]] auto is_compatible(semver::version engineVersion) const noexcept
        -> bool;

    struct glaze {
        using T = ModManifest;
        static constexpr auto value = glz::object(
            "id", &T::id,
            "name", &T::name,
            "version", &T::version,
            "minEngineVersion", &T::minEngineVersion,
            "description", &T::description,
            "author", &T::author,
            "dependencies", &T::dependencies,
            "entryPoints", &T::entryPoints,
            "permissions", &T::permissions
        );
    };
};

} // namespace projectv::modding
```

---

## 3. Block Definition API

### 3.1 JSON Format

```json
{
    "id": "my_mod:glowstone",
    "displayName": "Glowstone Block",
    "category": "building",
    "properties": {
        "solid": true,
        "transparent": false,
        "lightEmission": 15,
        "hardness": 0.3,
        "blastResistance": 1.5,
        "flammable": false
    },
    "appearance": {
        "texture": "textures/glowstone.png",
        "material": "materials/glow.mat",
        "particleTexture": "textures/glowstone_particle.png"
    },
    "drops": [
        {
            "item": "my_mod:glowstone_dust",
            "count": { "min": 2, "max": 4 },
            "chance": 1.0
        }
    ],
    "behaviors": [
        "light_source",
        "redstone_conductor"
    ]
}
```

### 3.2 C++ API

```cpp
export namespace projectv::modding {

/// Свойства блока
struct BlockProperties {
    bool solid{true};
    bool transparent{false};
    uint8_t lightEmission{0};
    float hardness{1.0f};
    float blastResistance{1.0f};
    bool flammable{false};
    bool replaceable{false};
    bool liquid{false};
};

/// Определение блока от мода
struct BlockDefinition {
    std::string id;                    // my_mod:glowstone
    std::string displayName;
    std::string category;
    BlockProperties properties;
    Appearance appearance;
    std::vector<DropEntry> drops;
    std::vector<std::string> behaviors;

    /// Регистрация блока в движке
    [[nodiscard]] auto register_block() const noexcept
        -> std::expected<BlockID, BlockError>;
};

/// Менеджер блоков
class BlockRegistry {
public:
    /// Регистрирует блок из мода
    auto register_block(
        std::string const& modId,
        BlockDefinition const& def
    ) noexcept -> std::expected<BlockID, BlockError>;

    /// Получает блок по ID
    [[nodiscard]] auto get_block(std::string_view fullId) const noexcept
        -> BlockDefinition const*;

    /// Получает блок по числовому ID
    [[nodiscard]] auto get_block(BlockID id) const noexcept
        -> BlockDefinition const*;

    /// Перечисляет все блоки мода
    [[nodiscard]] auto list_mod_blocks(std::string_view modId) const noexcept
        -> std::vector<BlockDefinition const*>;
};

} // namespace projectv::modding
```

---

## 4. Item Definition API

### 4.1 JSON Format

```json
{
    "id": "my_mod:magic_wand",
    "displayName": "Magic Wand",
    "category": "tools",
    "type": "tool",
    "properties": {
        "maxStack": 1,
        "durability": 500,
        "attackDamage": 5,
        "attackSpeed": 1.5,
        "enchantable": true
    },
    "appearance": {
        "texture": "textures/magic_wand.png",
        "model": "models/magic_wand.gltf",
        "handAnimation": "wand_swipe"
    },
    "abilities": [
        {
            "type": "on_use",
            "action": "cast_spell",
            "cooldown": 2.0,
            "script": "scripts/wand_spell.lua"
        }
    ]
}
```

### 4.2 C++ API

```cpp
export namespace projectv::modding {

/// Тип предмета
enum class ItemType : uint8_t {
    Misc,
    Tool,
    Weapon,
    Armor,
    Consumable,
    Block,      // Блок как предмет
    Custom
};

/// Определение предмета
struct ItemDefinition {
    std::string id;
    std::string displayName;
    std::string category;
    ItemType type;
    ItemProperties properties;
    Appearance appearance;
    std::vector<Ability> abilities;

    [[nodiscard]] auto register_item() const noexcept
        -> std::expected<ItemID, ItemError>;
};

/// Менеджер предметов
class ItemRegistry {
public:
    auto register_item(
        std::string const& modId,
        ItemDefinition const& def
    ) noexcept -> std::expected<ItemID, ItemError>;

    [[nodiscard]] auto get_item(std::string_view fullId) const noexcept
        -> ItemDefinition const*;

    [[nodiscard]] auto get_item(ItemID id) const noexcept
        -> ItemDefinition const*;
};

} // namespace projectv::modding
```

---

## 5. Scripting API (Lua)

### 5.1 API Surface

```lua
-- ProjectV Modding API (Lua)

-- =============== World API ===============

-- Получить блок на позиции
function world.getBlock(x, y, z) -> BlockState

-- Установить блок на позиции
function world.setBlock(x, y, z, blockId, properties?) -> boolean

-- Получить воксельные данные
function world.getVoxelData(x, y, z) -> VoxelData

-- Создать взрыв
function world.createExplosion(x, y, z, power, fire?) -> void

-- Получить сущности в области
function world.getEntitiesInArea(minX, minY, minZ, maxX, maxY, maxZ) -> Entity[]

-- =============== Player API ===============

-- Получить инвентарь игрока
function player.getInventory() -> Inventory

-- Выдать предмет игроку
function player.giveItem(itemId, count) -> boolean

-- Получить позицию игрока
function player.getPosition() -> Vec3

-- Телепортировать игрока
function player.teleport(x, y, z) -> void

-- =============== Events ===============

-- Событие: блок разрушен
event.onBlockBreak(function(pos, blockId, playerId)
    -- Custom logic
end)

-- Событие: блок размещён
event.onBlockPlace(function(pos, blockId, playerId)
    -- Custom logic
end)

-- Событие: игрок присоединился
event.onPlayerJoin(function(playerId, playerName)
    -- Custom logic
end)

-- Событие: использование предмета
event.onItemUse(function(playerId, itemId, targetPos)
    -- Custom logic
end)

-- =============== Logging ===============

function log.info(message) -> void
function log.warning(message) -> void
function log.error(message) -> void
```

### 5.2 C++ Integration

```cpp
export namespace projectv::modding {

/// Lua VM для модов
class ModScriptingEngine {
public:
    /// Создаёт Lua VM с sandbox
    [[nodiscard]] static auto create() noexcept
        -> std::expected<ModScriptingEngine, ScriptingError>;

    /// Загружает скрипт мода
    auto load_script(
        std::string const& modId,
        std::filesystem::path const& scriptPath
    ) noexcept -> std::expected<void, ScriptingError>;

    /// Вызывает функцию в скрипте
    auto call_function(
        std::string const& modId,
        std::string const& functionName,
        sol::variadic_args args
    ) noexcept -> sol::protected_function_result;

    /// Регистрирует callback для события
    auto register_event_handler(
        std::string const& eventName,
        sol::function callback
    ) noexcept -> void;

    /// Отправляет событие во все зарегистрированные handlers
    auto dispatch_event(
        std::string const& eventName,
        EventData const& data
    ) noexcept -> void;

private:
    sol::state lua_;
    std::unordered_map<std::string, std::vector<sol::function>> eventHandlers_;
};

} // namespace projectv::modding
```

---

## 6. Mod Loading Pipeline

### 6.1 Загрузка модов

```cpp
export namespace projectv::modding {

/// Состояние загрузки мода
enum class ModLoadState {
    Unloaded,
    Loading,
    Loaded,
    Failed,
    Disabled
};

/// Загруженный мод
struct LoadedMod {
    ModManifest manifest;
    ModLoadState state{ModLoadState::Unloaded};
    std::filesystem::path path;
    std::string errorMessage;
    std::unordered_map<std::string, BlockID> registeredBlocks;
    std::unordered_map<std::string, ItemID> registeredItems;
};

/// Менеджер модов
class ModManager {
public:
    /// Инициализация менеджера модов
    [[nodiscard]] static auto create(
        std::filesystem::path const& modsDir,
        BlockRegistry& blockRegistry,
        ItemRegistry& itemRegistry
    ) noexcept -> std::expected<ModManager, ModError>;

    /// Обнаруживает все моды в директории
    auto discover_mods() noexcept -> void;

    /// Разрешает зависимости между модами
    [[nodiscard]] auto resolve_dependencies() noexcept
        -> std::expected<std::vector<std::string>, DependencyError>;

    /// Загружает мод по ID
    auto load_mod(std::string const& modId) noexcept
        -> std::expected<void, ModError>;

    /// Выгружает мод
    auto unload_mod(std::string const& modId) noexcept
        -> std::expected<void, ModError>;

    /// Перезагружает мод (hot-reload)
    auto reload_mod(std::string const& modId) noexcept
        -> std::expected<void, ModError>;

    /// Получает информацию о моде
    [[nodiscard]] auto get_mod(std::string const& modId) const noexcept
        -> LoadedMod const*;

    /// Перечисляет все моды
    [[nodiscard]] auto list_mods() const noexcept
        -> std::vector<LoadedMod const*>;

    /// Включает/выключает мод
    auto set_mod_enabled(std::string const& modId, bool enabled) noexcept
        -> std::expected<void, ModError>;

private:
    std::filesystem::path modsDir_;
    BlockRegistry* blockRegistry_;
    ItemRegistry* itemRegistry_;
    ModScriptingEngine scriptingEngine_;
    std::unordered_map<std::string, LoadedMod> mods_;
    std::vector<std::string> loadOrder_;
};

} // namespace projectv::modding
```

---

## 7. Security & Sandboxing

### 7.1 Разрешения

```cpp
export namespace projectv::modding {

/// Разрешения для модов
export enum class ModPermission : uint8_t {
    BlocksCreate    = 1 << 0,   ///< Создание блоков
    ItemsCreate     = 1 << 1,   ///< Создание предметов
    MaterialsCreate = 1 << 2,   ///< Создание материалов
    UIExtend        = 1 << 3,   ///< Расширение UI
    WorldModify     = 1 << 4,   ///< Изменение мира
    PlayerInteract  = 1 << 5,   ///< Взаимодействие с игроками
    NetworkAccess   = 1 << 6,   ///< Сетевой доступ
    FileIO          = 1 << 7,   ///< Доступ к файловой системе
};

/// Проверка разрешений
class PermissionManager {
public:
    /// Проверяет, имеет ли мод разрешение
    [[nodiscard]] auto has_permission(
        std::string const& modId,
        ModPermission permission
    ) const noexcept -> bool;

    /// Запрашивает разрешение у пользователя
    auto request_permission(
        std::string const& modId,
        ModPermission permission
    ) noexcept -> std::expected<void, PermissionError>;

    /// Предоставляет разрешение
    auto grant_permission(
        std::string const& modId,
        ModPermission permission
    ) noexcept -> void;

    /// Отзывает разрешение
    auto revoke_permission(
        std::string const& modId,
        ModPermission permission
    ) noexcept -> void;
};

} // namespace projectv::modding
```

### 7.2 Lua Sandbox

```cpp
// Опасные функции Lua изолируются
// sandboxed_lua.cpp

auto create_sandboxed_lua() -> sol::state {
    sol::state lua;

    // Отключаем опасные функции
    lua["dofile"] = sol::nil;
    lua["loadfile"] = sol::nil;
    lua["load"] = sol::nil;
    lua["os"]["execute"] = sol::nil;
    lua["os"]["exit"] = sol::nil;
    lua["os"]["remove"] = sol::nil;
    lua["os"]["rename"] = sol::nil;
    lua["io"]["popen"] = sol::nil;

    // Предоставляем безопасные API
    register_safe_api(lua);

    return lua;
}
```

---

## Статус

| Компонент            | Статус         | Приоритет |
|----------------------|----------------|-----------|
| Manifest Format      | Специфицирован | P0        |
| Block Definition API | Специфицирован | P0        |
| Item Definition API  | Специфицирован | P0        |
| Scripting API        | Специфицирован | P1        |
| Mod Loading Pipeline | Специфицирован | P0        |
| Permission System    | Специфицирован | P1        |
| Lua Sandbox          | Специфицирован | P1        |
