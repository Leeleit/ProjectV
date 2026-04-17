# Serialization Strategy: C++26 Static Reflection + Zstd [🟢 Уровень 1]

**🟢 Уровень 1: Фундаментальный** — Zero-boilerplate сериализация воксельных миров ProjectV.

---

## Обзор

ProjectV использует **C++26 P2996 Static Reflection** + **Zstd** для:

- **Zero-copy** десериализацию где возможно
- **Zero-boilerplate reflection**, **Compile-time reflection** без макросов
- **Высокую скорость** сжатия/распаковки
- **Отсутствие исключений** — используем `std::expected`

---

## 1. Glaze: Reflection и JSON/Бинарная сериализация

### 1.1 Базовая структура с glaze

```cpp
#include <glaze/glaze.hpp>
#include <expected>
#include <string>

namespace projectv {

// Ошибки сериализации
enum class SerializeError {
    InvalidFormat,
    BufferTooSmall,
    InvalidChecksum,
    CompressionFailed,
    DecompressionFailed,
    VersionMismatch,
    MissingField
};

template<typename T>
using SerializeResult = std::expected<T, SerializeError>;

// Воксельный чанк с автоматической сериализацией
struct VoxelChunk {
    int32_t x, y, z;                    // Координаты чанка
    uint32_t version = 1;               // Версия формата
    std::vector<uint8_t> blocks;        // ID блоков (16³ = 4096)
    std::vector<uint8_t> metadata;      // Дополнительные данные
    uint64_t timestamp;                 // Время последнего изменения
    uint32_t checksum;                  // Контрольная сумма

    // Glaze metadata для автоматической reflection
    struct glaze {
        using T = VoxelChunk;
        static constexpr auto value = glz::object(
            "x", &T::x,
            "y", &T::y,
            "z", &T::z,
            "version", &T::version,
            "blocks", &T::blocks,
            "metadata", &T::metadata,
            "timestamp", &T::timestamp,
            "checksum", &T::checksum
        );
    };
};

// ECS компонент для сохранения
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    struct glaze {
        using T = TransformComponent;
        static constexpr auto value = glz::object(
            "position", &T::position,
            "rotation", &T::rotation,
            "scale", &T::scale
        );
    };
};

// Glaze умеет работать с glm типами напрямую (через специализации)
} // namespace projectv

// Специализация glaze для glm::vec3
template<>
struct glz::meta<glm::vec3> {
    using T = glm::vec3;
    static constexpr auto value = glz::object(
        "x", &T::x,
        "y", &T::y,
        "z", &T::z
    );
};

template<>
struct glz::meta<glm::quat> {
    using T = glm::quat;
    static constexpr auto value = glz::object(
        "x", &T::x,
        "y", &T::y,
        "z", &T::z,
        "w", &T::w
    );
};
```

### 1.2 JSON сериализация

```cpp
namespace projectv {

class JsonSerializer {
public:
    // Сериализация в JSON
    template<typename T>
    SerializeResult<std::string> toJson(const T& obj) const {
        std::string buffer;
        buffer.resize(1024); // Начальный размер

        auto result = glz::write_json(obj, buffer);

        if (result) {
            return buffer;
        }

        // Если буфер слишком мал, пробуем снова
        if (result.error() == glz::error_code::buffer_too_small) {
            buffer.resize(1024 * 1024); // 1 MB
            result = glz::write_json(obj, buffer);
            if (result) {
                return buffer;
            }
        }

        return std::unexpected(SerializeError::InvalidFormat);
    }

    // Десериализация из JSON
    template<typename T>
    SerializeResult<T> fromJson(std::string_view json) const {
        T obj{};
        auto result = glz::read_json(obj, json);

        if (result) {
            return obj;
        }

        return std::unexpected(SerializeError::InvalidFormat);
    }

    // Запись JSON в файл
    template<typename T>
    SerializeResult<void> toJsonFile(const T& obj,
                                      const std::filesystem::path& path) const {
        auto json = toJson(obj);
        if (!json) {
            return std::unexpected(json.error());
        }

        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        file.write(json->data(), json->size());
        return {};
    }

    // Чтение JSON из файла
    template<typename T>
    SerializeResult<T> fromJsonFile(const std::filesystem::path& path) const {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        auto size = file.tellg();
        file.seekg(0);

        std::string buffer(size, '\0');
        file.read(buffer.data(), size);

        return fromJson<T>(buffer);
    }
};

} // namespace projectv
```

### 1.3 Бинарная сериализация (более компактная)

```cpp
namespace projectv {

class BinarySerializer {
public:
    // Сериализация в бинарный формат
    template<typename T>
    SerializeResult<std::vector<uint8_t>> toBinary(const T& obj) const {
        std::vector<uint8_t> buffer;

        // Вычисляем необходимый размер
        auto size = glz::write_binary(obj, buffer, glz::write_to_vector{});

        if (size) {
            return buffer;
        }

        return std::unexpected(SerializeError::InvalidFormat);
    }

    // Десериализация из бинарного формата
    template<typename T>
    SerializeResult<T> fromBinary(const std::vector<uint8_t>& data) const {
        T obj{};
        auto result = glz::read_binary(obj, data);

        if (result) {
            return obj;
        }

        return std::unexpected(SerializeError::InvalidFormat);
    }

    template<typename T>
    SerializeResult<T> fromBinary(std::span<const uint8_t> data) const {
        T obj{};
        auto result = glz::read_binary(obj, data);

        if (result) {
            return obj;
        }

        return std::unexpected(SerializeError::InvalidFormat);
    }
};

} // namespace projectv
```

---

## 2. Zstd: Сжатие данных

### 2.1 Обёртка над zstd

```cpp
#include <zstd.h>

namespace projectv {

enum class ZstdLevel : int {
    Fast = 1,           // Максимальная скорость
    Balanced = 3,       // Баланс (по умолчанию)
    High = 10,          // Хорошее сжатие
    Ultra = 22          // Максимальное сжатие
};

class ZstdCompressor {
public:
    ZstdCompressor(ZstdLevel level = ZstdLevel::Balanced)
        : level_(static_cast<int>(level))
        , cctx_(ZSTD_createCCtx())
        , dctx_(ZSTD_createDCtx())
    {}

    ~ZstdCompressor() {
        if (cctx_) ZSTD_freeCCtx(cctx_);
        if (dctx_) ZSTD_freeDCtx(dctx_);
    }

    // Сжатие данных
    SerializeResult<std::vector<uint8_t>> compress(
        std::span<const uint8_t> data) const {

        auto bound = ZSTD_compressBound(data.size());
        std::vector<uint8_t> compressed(bound);

        auto result = ZSTD_compressCCtx(
            cctx_,
            compressed.data(), compressed.size(),
            data.data(), data.size(),
            level_
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(SerializeError::CompressionFailed);
        }

        compressed.resize(result);
        return compressed;
    }

    // Распаковка данных
    SerializeResult<std::vector<uint8_t>> decompress(
        std::span<const uint8_t> compressed,
        size_t originalSize) const {

        std::vector<uint8_t> decompressed(originalSize);

        auto result = ZSTD_decompressDCtx(
            dctx_,
            decompressed.data(), decompressed.size(),
            compressed.data(), compressed.size()
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(SerializeError::DecompressionFailed);
        }

        return decompressed;
    }

    // Получение размера распакованных данных
    SerializeResult<size_t> getDecompressedSize(
        std::span<const uint8_t> compressed) const {

        auto size = ZSTD_getFrameContentSize(
            compressed.data(), compressed.size());

        if (size == ZSTD_CONTENTSIZE_ERROR ||
            size == ZSTD_CONTENTSIZE_UNKNOWN) {
            return std::unexpected(SerializeError::DecompressionFailed);
        }

        return size;
    }

private:
    int level_;
    ZSTD_CCtx* cctx_;
    ZSTD_DCtx* dctx_;
};

} // namespace projectv
```

---

## 3. Комбинированный формат: Glaze Binary + Zstd

### 3.1 Заголовок файла чанка

```cpp
namespace projectv {

// Заголовок файла чанка (32 байта)
#pragma pack(push, 1)
struct ChunkFileHeader {
    uint32_t magic = 0x564F5821;      // "VOX!"
    uint32_t version = 2;             // Версия формата
    uint32_t headerSize = sizeof(ChunkFileHeader);
    uint32_t metadataSize;            // Размер метаданных
    uint64_t uncompressedSize;        // Размер до сжатия
    uint64_t compressedSize;          // Размер после сжатия
    uint32_t checksum;                // CRC32 данных
    uint8_t compressionType = 1;      // 1 = Zstd
    uint8_t compressionLevel;         // Уровень сжатия
    uint16_t flags;                   // Флаги
    uint32_t reserved[2];             // Зарезервировано
};

static_assert(sizeof(ChunkFileHeader) == 32, "Header must be 32 bytes");

enum class ChunkFlags : uint16_t {
    Empty = 1 << 0,
    Modified = 1 << 1,
    Generated = 1 << 2,
    Delta = 1 << 3,
    RLEEncoded = 1 << 4
};
#pragma pack(pop)

} // namespace projectv
```

### 3.2 ChunkSerializer

```cpp
namespace projectv {

class ChunkSerializer {
public:
    enum class Strategy {
        Full,      // Полное сохранение
        Delta,     // Только изменения
        Smart      // Автоматический выбор
    };

    struct Config {
        Strategy strategy = Strategy::Smart;
        ZstdLevel compressionLevel = ZstdLevel::Balanced;
        bool enableChecksum = true;
    };

    // Сохранение чанка
    SerializeResult<std::vector<uint8_t>> save(
        const VoxelChunk& chunk,
        const Config& config = {}) {

        // 1. Сериализация через glaze
        auto binary = binarySerializer_.toBinary(chunk);
        if (!binary) {
            return std::unexpected(binary.error());
        }

        // 2. Вычисление контрольной суммы
        uint32_t checksum = 0;
        if (config.enableChecksum) {
            checksum = calculateCRC32(*binary);
        }

        // 3. Сжатие через zstd
        auto compressed = compressor_.compress(*binary);
        if (!compressed) {
            return std::unexpected(compressed.error());
        }

        // 4. Создание заголовка
        ChunkFileHeader header;
        header.uncompressedSize = binary->size();
        header.compressedSize = compressed->size();
        header.compressionLevel = static_cast<uint8_t>(config.compressionLevel);
        header.checksum = checksum;

        // 5. Объединение в один буфер
        std::vector<uint8_t> result;
        result.resize(sizeof(header) + compressed->size());

        std::memcpy(result.data(), &header, sizeof(header));
        std::memcpy(result.data() + sizeof(header),
                    compressed->data(), compressed->size());

        return result;
    }

    // Загрузка чанка
    SerializeResult<VoxelChunk> load(
        std::span<const uint8_t> data) {

        // 1. Парсинг заголовка
        if (data.size() < sizeof(ChunkFileHeader)) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        ChunkFileHeader header;
        std::memcpy(&header, data.data(), sizeof(header));

        // 2. Проверка magic number
        if (header.magic != 0x564F5821) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 3. Проверка версии
        if (header.version > 2) {
            return std::unexpected(SerializeError::VersionMismatch);
        }

        // 4. Распаковка
        auto compressedData = data.subspan(sizeof(header));
        auto decompressed = compressor_.decompress(
            compressedData, header.uncompressedSize);

        if (!decompressed) {
            return std::unexpected(decompressed.error());
        }

        // 5. Проверка контрольной суммы
        if (header.checksum != 0) {
            auto checksum = calculateCRC32(*decompressed);
            if (checksum != header.checksum) {
                return std::unexpected(SerializeError::InvalidChecksum);
            }
        }

        // 6. Десериализация через glaze
        return binarySerializer_.fromBinary<VoxelChunk>(*decompressed);
    }

private:
    BinarySerializer binarySerializer_;
    ZstdCompressor compressor_{ZstdLevel::Balanced};

    static uint32_t calculateCRC32(std::span<const uint8_t> data);
};

} // namespace projectv
```

---

## 4. Интеграция с ECS (Flecs)

### 4.1 Сериализация компонентов

```cpp
namespace projectv {

// Вспомогательная структура для сохранения состояния ECS
struct ECSState {
    uint32_t version = 1;
    std::vector<std::string> entityNames;
    std::vector<TransformComponent> transforms;
    // Добавьте другие компоненты по необходимости

    struct glaze {
        using T = ECSState;
        static constexpr auto value = glz::object(
            "version", &T::version,
            "entityNames", &T::entityNames,
            "transforms", &T::transforms
        );
    };
};

class ECSSerializer {
public:
    // Экспорт состояния мира
    SerializeResult<ECSState> exportWorld(flecs::world& world) {
        ECSState state;

        // Собираем все сущности с TransformComponent
        world.each([&](flecs::entity e, TransformComponent& transform) {
            state.entityNames.push_back(e.name().c_str());
            state.transforms.push_back(transform);
        });

        return state;
    }

    // Импорт состояния мира
    SerializeResult<void> importWorld(
        flecs::world& world,
        const ECSState& state) {

        for (size_t i = 0; i < state.entityNames.size(); ++i) {
            auto entity = world.entity(state.entityNames[i].c_str());
            entity.set<TransformComponent>(state.transforms[i]);
        }

        return {};
    }

    // Сохранение в файл
    SerializeResult<void> saveWorld(
        flecs::world& world,
        const std::filesystem::path& path,
        ZstdLevel compressionLevel = ZstdLevel::Balanced) {

        // Экспорт состояния
        auto state = exportWorld(world);
        if (!state) {
            return std::unexpected(state.error());
        }

        // Сериализация в бинарный формат
        auto binary = binarySerializer_.toBinary(*state);
        if (!binary) {
            return std::unexpected(binary.error());
        }

        // Сжатие
        ZstdCompressor compressor{compressionLevel};
        auto compressed = compressor.compress(*binary);
        if (!compressed) {
            return std::unexpected(compressed.error());
        }

        // Запись в файл
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // Заголовок
        uint64_t uncompressedSize = binary->size();
        uint64_t compressedSize = compressed->size();
        file.write(reinterpret_cast<const char*>(&uncompressedSize), 8);
        file.write(reinterpret_cast<const char*>(&compressedSize), 8);
        file.write(reinterpret_cast<const char*>(compressed->data()),
                   compressed->size());

        return {};
    }

    // Загрузка из файла
    SerializeResult<void> loadWorld(
        flecs::world& world,
        const std::filesystem::path& path) {

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        auto fileSize = file.tellg();
        file.seekg(0);

        // Чтение заголовка
        uint64_t uncompressedSize, compressedSize;
        file.read(reinterpret_cast<char*>(&uncompressedSize), 8);
        file.read(reinterpret_cast<char*>(&compressedSize), 8);

        // Чтение сжатых данных
        std::vector<uint8_t> compressed(compressedSize);
        file.read(reinterpret_cast<char*>(compressed.data()), compressedSize);

        // Распаковка
        ZstdCompressor compressor;
        auto decompressed = compressor.decompress(compressed, uncompressedSize);
        if (!decompressed) {
            return std::unexpected(decompressed.error());
        }

        // Десериализация
        auto state = binarySerializer_.fromBinary<ECSState>(*decompressed);
        if (!state) {
            return std::unexpected(state.error());
        }

        // Импорт в мир
        return importWorld(world, *state);
    }

private:
    BinarySerializer binarySerializer_;
};

} // namespace projectv
```

---

## 5. Автоматическая генерация UI с glaze

### 5.1 ImGui Inspector из glaze metadata

```cpp
#include <imgui.h>

namespace projectv {

// Генератор ImGui UI на основе glaze reflection
class GlazeInspector {
public:
    template<typename T>
    void inspect(const char* name, T& obj) {
        if constexpr (requires { typename T::glaze; }) {
            if (ImGui::CollapsingHeader(name)) {
                ImGui::Indent();
                inspectFields(obj);
                ImGui::Unindent();
            }
        } else {
            // Базовый тип
            inspectValue(name, obj);
        }
    }

private:
    // Специализации для базовых типов
    void inspectValue(const char* name, float& value) {
        ImGui::DragFloat(name, &value, 0.1f);
    }

    void inspectValue(const char* name, int& value) {
        ImGui::DragInt(name, &value, 1);
    }

    void inspectValue(const char* name, bool& value) {
        ImGui::Checkbox(name, &value);
    }

    void inspectValue(const char* name, glm::vec3& value) {
        ImGui::DragFloat3(name, &value.x, 0.1f);
    }

    void inspectValue(const char* name, glm::quat& value) {
        ImGui::DragFloat4(name, &value.x, 0.01f);
    }

    // Обработка полей через glaze metadata
    template<typename T>
    void inspectFields(T& obj) {
        // glaze предоставляет compile-time итерацию по полям
        // Это упрощённый пример, реальная реализация
        // использует glz::for_each_field
        auto& value = obj;

        // Для каждого поля в glaze metadata:
        // - Получить имя поля
        // - Вызвать inspectValue для соответствующего типа
    }
};

// Использование
void inspectEntity(flecs::entity e) {
    GlazeInspector inspector;

    if (e.has<TransformComponent>()) {
        auto* transform = e.get_mut<TransformComponent>();
        inspector.inspect("Transform", *transform);
    }
}

} // namespace projectv
```

---

## 6. Полный пример: Save/Load системы

### 6.1 WorldSaveSystem

```cpp
namespace projectv {

struct WorldMetadata {
    std::string worldName;
    uint64_t createdTimestamp;
    uint64_t modifiedTimestamp;
    uint32_t totalChunks;
    uint32_t version = 1;

    struct glaze {
        using T = WorldMetadata;
        static constexpr auto value = glz::object(
            "worldName", &T::worldName,
            "createdTimestamp", &T::createdTimestamp,
            "modifiedTimestamp", &T::modifiedTimestamp,
            "totalChunks", &T::totalChunks,
            "version", &T::version
        );
    };
};

class WorldSaveSystem {
public:
    WorldSaveSystem(flecs::world& ecs)
        : ecs_(ecs)
    {}

    // Сохранение мира
    SerializeResult<void> saveWorld(const std::filesystem::path& saveDir) {
        // 1. Создание директории
        std::error_code ec;
        std::filesystem::create_directories(saveDir, ec);
        if (ec) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 2. Сохранение метаданных
        WorldMetadata metadata;
        metadata.worldName = saveDir.filename().string();
        metadata.createdTimestamp = getCurrentTimestamp();
        metadata.modifiedTimestamp = metadata.createdTimestamp;

        auto metadataResult = jsonSerializer_.toJsonFile(metadata,
            saveDir / "metadata.json");
        if (!metadataResult) {
            return std::unexpected(metadataResult.error());
        }

        // 3. Сохранение ECS состояния
        ECSSerializer ecsSerializer;
        auto ecsResult = ecsSerializer.saveWorld(ecs_,
            saveDir / "ecs_state.bin");
        if (!ecsResult) {
            return std::unexpected(ecsResult.error());
        }

        // 4. Сохранение чанков (постранично)
        // ...

        return {};
    }

    // Загрузка мира
    SerializeResult<void> loadWorld(const std::filesystem::path& saveDir) {
        // 1. Проверка существования
        if (!std::filesystem::exists(saveDir / "metadata.json")) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 2. Загрузка метаданных
        auto metadata = jsonSerializer_.fromJsonFile<WorldMetadata>(
            saveDir / "metadata.json");
        if (!metadata) {
            return std::unexpected(metadata.error());
        }

        // 3. Загрузка ECS состояния
        ECSSerializer ecsSerializer;
        auto ecsResult = ecsSerializer.loadWorld(ecs_,
            saveDir / "ecs_state.bin");
        if (!ecsResult) {
            return std::unexpected(ecsResult.error());
        }

        // 4. Загрузка чанков
        // ...

        return {};
    }

private:
    flecs::world& ecs_;
    JsonSerializer jsonSerializer_;

    static uint64_t getCurrentTimestamp() {
        return static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
    }
};

} // namespace projectv
```

---

## 7. Рекомендации для ProjectV

### Когда использовать JSON vs Binary

| Формат            | Использование                              |
|-------------------|--------------------------------------------|
| **JSON**          | Конфигурация, отладка, обмен данными       |
| **Binary**        | Сохранения, сетевой трафик, большие объёмы |
| **Binary + Zstd** | Воксельные чанки, ресурсы, backup          |

### Уровни сжатия Zstd

| Уровень         | Скорость | Сжатие  | Использование         |
|-----------------|----------|---------|-----------------------|
| 1-3 (Fast)      | Высокая  | Средняя | Runtime сохранение    |
| 3-10 (Balanced) | Средняя  | Хорошая | Регулярные сохранения |
| 10-22 (Ultra)   | Низкая   | Лучшее  | Экспорт, архивация    |

### Избегайте

- **Слишком агрессивного сжатия** (уровень >15) для runtime — медленно
- **Сохранения без контрольных сумм** — риск повреждения данных
- **Глубокой вложенности** в JSON — медленно парсится

---

## Ссылки

- [glaze документация](../libraries/glaze/00_overview.md)
- [Zstd документация](../libraries/zstd/00_overview.md)
- [Banned Features: Exceptions](../guides/cpp/11_banned-features.md)

---

## 1. Static Reflection: Сериализация без Boilerplate

### 1.1 Автоматическая сериализация структур

```cpp
// ProjectV.Serialization.Reflection.cppm
export module ProjectV.Serialization.Reflection;

import std;
import std.meta;

export namespace projectv::serialization {

/// Ошибки сериализации
export enum class SerializeError : uint8_t {
    InvalidFormat,
    BufferTooSmall,
    InvalidChecksum,
    CompressionFailed,
    DecompressionFailed,
    VersionMismatch,
    MissingField,
    TypeMismatch
};

export template<typename T>
using SerializeResult = std::expected<T, SerializeError>;

/// Концепт для сериализуемого типа.
export template<typename T>
concept Serializable = requires(T const& obj) {
    { std::meta::members_of(^T) } -> std::same_as<std::meta::info>;
};

/// Вычисление размера типа для сериализации.
export template<typename T>
consteval auto compute_serial_size() -> size_t {
    if constexpr (std::is_trivially_copyable_v<T>) {
        return sizeof(T);
    } else if constexpr (std::ranges::range<T>) {
        // Размер + элементы (верхняя оценка)
        using ValueType = std::ranges::range_value_t<T>;
        return sizeof(uint32_t) + sizeof(ValueType) * 1024; // Estimate
    } else {
        // Структура: сумма размеров членов
        size_t size = 0;
        constexpr auto members = std::meta::members_of(^T);

        for constexpr auto member : members {
            if constexpr (std::meta::is_data_member(member)) {
                using MemberType = [:std::meta::type_of(member):];
                size += compute_serial_size<MemberType>();
            }
        }

        return size;
    }
}

} // namespace projectv::serialization
```

### 1.2 Воксельный чанк — Zero Boilerplate

```cpp
// ProjectV.Voxel.Chunk.cppm
export module ProjectV.Voxel.Chunk;

import std;
import glm;

export namespace projectv {

/// Воксельный чанк для сериализации.
/// Никаких макросов — reflection работает автоматически!
export struct VoxelChunk {
    int32_t x{0}, y{0}, z{0};           ///< Координаты чанка
    uint32_t version{2};                ///< Версия формата
    std::vector<uint8_t> blocks;        ///< ID блоков (16³ = 4096)
    std::vector<uint8_t> metadata;      ///< Дополнительные данные
    uint64_t timestamp{0};              ///< Время последнего изменения
    uint32_t checksum{0};               ///< Контрольная сумма

    // Методы для инвариантов (опционально)
    [[nodiscard]] auto block_index(int32_t lx, int32_t ly, int32_t lz) const noexcept
        -> size_t {
        return static_cast<size_t>(lx + ly * 16 + lz * 256);
    }

    [[nodiscard]] auto get_block(int32_t lx, int32_t ly, int32_t lz) const noexcept
        -> uint8_t {
        auto idx = block_index(lx, ly, lz);
        return idx < blocks.size() ? blocks[idx] : 0;
    }

    auto set_block(int32_t lx, int32_t ly, int32_t lz, uint8_t type) -> void {
        auto idx = block_index(lx, ly, lz);
        if (idx < blocks.size()) {
            blocks[idx] = type;
        }
    }
};

/// ECS компонент Transform — тоже zero boilerplate.
export struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] auto to_matrix() const noexcept -> glm::mat4;
};

/// Материал PBR.
export struct PBRMaterial {
    std::string name{"default"};
    glm::vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float emissive_strength{0.0f};
    bool double_sided{false};
};

} // namespace projectv
```

---

## 2. Binary Serialization

### 2.1 Binary Serializer на Static Reflection

```cpp
// ProjectV.Serialization.Binary.cppm
export module ProjectV.Serialization.Binary;

import std;
import std.meta;
import glm;

export namespace projectv::serialization {

/// Бинарный сериализатор с zero boilerplate.
///
/// ## Features
/// - Автоматическая reflection для всех структур
/// - Поддержка nested типов
/// - Поддержка STL контейнеров
/// - Zero-copy для trivially_copyable типов
export class BinarySerializer {
public:
    /// Сериализация объекта в бинарный формат.
    /// @param obj Объект для сериализации
    /// @return Вектор байтов
    template<typename T>
    auto serialize(T const& obj) const -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer;
        buffer.reserve(estimate_size<T>());
        serialize_into(buffer, obj);
        return buffer;
    }

    /// Десериализация из бинарного формата.
    /// @param data Бинарные данные
    /// @return Объект или ошибка
    template<typename T>
    auto deserialize(std::span<uint8_t const> data) const
        -> SerializeResult<T> {
        T obj{};
        size_t offset = 0;

        auto result = deserialize_from(data, offset, obj);
        if (!result) {
            return std::unexpected(result.error());
        }

        return obj;
    }

    /// Десериализация с проверкой размера.
    template<typename T>
    auto deserialize_checked(std::span<uint8_t const> data) const
        -> SerializeResult<T> {
        if (data.size() < estimate_size<T>()) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        return deserialize<T>(data);
    }

private:
    /// Оценка размера объекта.
    template<typename T>
    static consteval auto estimate_size() -> size_t {
        if constexpr (std::is_trivially_copyable_v<T>) {
            return sizeof(T);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return sizeof(uint32_t) + 256; // Estimate
        } else if constexpr (std::ranges::range<T>) {
            using ValueType = std::ranges::range_value_t<T>;
            return sizeof(uint32_t) + sizeof(ValueType) * 64; // Estimate
        } else {
            // Структура
            size_t size = 0;
            if constexpr (requires { std::meta::members_of(^T); }) {
                constexpr auto members = std::meta::members_of(^T);
                for constexpr auto member : members {
                    if constexpr (std::meta::is_data_member(member)) {
                        using MemberType = [:std::meta::type_of(member):];
                        size += estimate_size<MemberType>();
                    }
                }
            }
            return size;
        }
    }

    /// Сериализация в буфер.
    template<typename T>
    auto serialize_into(std::vector<uint8_t>& buffer, T const& obj) const -> void {
        if constexpr (std::is_trivially_copyable_v<T>) {
            // Прямое копирование для POD
            auto const* ptr = reinterpret_cast<uint8_t const*>(&obj);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            // Строка: длина + данные
            auto size = static_cast<uint32_t>(obj.size());
            serialize_into(buffer, size);
            buffer.insert(buffer.end(), obj.begin(), obj.end());
        }
        else if constexpr (std::is_same_v<T, glm::vec3>) {
            serialize_into(buffer, obj.x);
            serialize_into(buffer, obj.y);
            serialize_into(buffer, obj.z);
        }
        else if constexpr (std::is_same_v<T, glm::vec4>) {
            serialize_into(buffer, obj.x);
            serialize_into(buffer, obj.y);
            serialize_into(buffer, obj.z);
            serialize_into(buffer, obj.w);
        }
        else if constexpr (std::is_same_v<T, glm::quat>) {
            serialize_into(buffer, obj.x);
            serialize_into(buffer, obj.y);
            serialize_into(buffer, obj.z);
            serialize_into(buffer, obj.w);
        }
        else if constexpr (std::is_same_v<T, glm::mat4>) {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    serialize_into(buffer, obj[i][j]);
                }
            }
        }
        else if constexpr (std::ranges::range<T>) {
            // Контейнер: размер + элементы
            auto size = static_cast<uint32_t>(obj.size());
            serialize_into(buffer, size);

            for (auto const& elem : obj) {
                serialize_into(buffer, elem);
            }
        }
        else if constexpr (requires { std::meta::members_of(^T); }) {
            // Структура: итерация по членам через reflection
            constexpr auto members = std::meta::members_of(^T);

            for constexpr auto member : members {
                if constexpr (std::meta::is_data_member(member)) {
                    serialize_into(buffer, obj.[:member:]);
                }
            }
        }
        else {
            static_assert(std::is_trivially_copyable_v<T>,
                "Type must be trivially copyable or have reflection support");
        }
    }

    /// Десериализация из буфера.
    template<typename T>
    auto deserialize_from(
        std::span<uint8_t const> data,
        size_t& offset,
        T& obj
    ) const -> std::expected<void, SerializeError> {
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (offset + sizeof(T) > data.size()) {
                return std::unexpected(SerializeError::BufferTooSmall);
            }
            std::memcpy(&obj, data.data() + offset, sizeof(T));
            offset += sizeof(T);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            uint32_t size{};
            auto result = deserialize_from(data, offset, size);
            if (!result) return result;

            if (offset + size > data.size()) {
                return std::unexpected(SerializeError::BufferTooSmall);
            }

            obj.assign(reinterpret_cast<char const*>(data.data() + offset), size);
            offset += size;
        }
        else if constexpr (std::is_same_v<T, glm::vec3>) {
            auto rx = deserialize_from(data, offset, obj.x);
            if (!rx) return rx;
            auto ry = deserialize_from(data, offset, obj.y);
            if (!ry) return ry;
            auto rz = deserialize_from(data, offset, obj.z);
            if (!rz) return rz;
        }
        else if constexpr (std::is_same_v<T, glm::vec4> || std::is_same_v<T, glm::quat>) {
            auto rx = deserialize_from(data, offset, obj.x);
            if (!rx) return rx;
            auto ry = deserialize_from(data, offset, obj.y);
            if (!ry) return ry;
            auto rz = deserialize_from(data, offset, obj.z);
            if (!rz) return rz;
            auto rw = deserialize_from(data, offset, obj.w);
            if (!rw) return rw;
        }
        else if constexpr (std::is_same_v<T, glm::mat4>) {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    auto r = deserialize_from(data, offset, obj[i][j]);
                    if (!r) return r;
                }
            }
        }
        else if constexpr (std::ranges::range<T>) {
            using SizeType = uint32_t;
            SizeType size{};

            auto result = deserialize_from(data, offset, size);
            if (!result) return result;

            obj.resize(size);
            for (auto& elem : obj) {
                result = deserialize_from(data, offset, elem);
                if (!result) return result;
            }
        }
        else if constexpr (requires { std::meta::members_of(^T); }) {
            constexpr auto members = std::meta::members_of(^T);

            for constexpr auto member : members {
                if constexpr (std::meta::is_data_member(member)) {
                    auto result = deserialize_from(data, offset, obj.[:member:]);
                    if (!result) return result;
                }
            }
        }
        else {
            static_assert(std::is_trivially_copyable_v<T>,
                "Type must be trivially copyable or have reflection support");
        }

        return {};
    }
};

} // namespace projectv::serialization
```

---

## 3. JSON Serialization

### 3.1 JSON Serializer на Static Reflection

```cpp
// ProjectV.Serialization.Json.cppm
export module ProjectV.Serialization.Json;

import std;
import std.meta;
import glm;

export namespace projectv::serialization {

/// JSON Serializer с zero boilerplate.
export class JsonSerializer {
public:
    /// Сериализация в JSON.
    template<typename T>
    auto serialize(T const& obj) const -> std::string {
        return serialize_value(obj);
    }

    /// Десериализация из JSON.
    template<typename T>
    auto deserialize(std::string_view json) const -> SerializeResult<T> {
        T obj{};
        size_t pos = 0;

        auto result = deserialize_value(json, pos, obj);
        if (!result) {
            return std::unexpected(result.error());
        }

        return obj;
    }

    /// Запись в файл.
    template<typename T>
    auto save(T const& obj, std::filesystem::path const& path) const
        -> std::expected<void, SerializeError> {
        auto json = serialize(obj);

        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        file.write(json.data(), json.size());
        return {};
    }

    /// Чтение из файла.
    template<typename T>
    auto load(std::filesystem::path const& path) const
        -> SerializeResult<T> {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        auto size = file.tellg();
        file.seekg(0);

        std::string json(size, '\0');
        file.read(json.data(), size);

        return deserialize<T>(json);
    }

private:
    /// Сериализация значения.
    template<typename T>
    auto serialize_value(T const& obj) const -> std::string {
        if constexpr (std::is_enum_v<T>) {
            return serialize_enum(obj);
        } else if constexpr (std::is_same_v<T, bool>) {
            return obj ? "true" : "false";
        } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
            return std::format("{}", obj);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return std::format("\"{}\"", escape_json(obj));
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            return std::format("[{},{},{}]", obj.x, obj.y, obj.z);
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            return std::format("[{},{},{},{}]", obj.x, obj.y, obj.z, obj.w);
        } else if constexpr (std::is_same_v<T, glm::quat>) {
            return std::format("[{},{},{},{}]", obj.x, obj.y, obj.z, obj.w);
        } else if constexpr (std::ranges::range<T>) {
            return serialize_array(obj);
        } else if constexpr (requires { std::meta::members_of(^T); }) {
            return serialize_object(obj);
        } else {
            return "null";
        }
    }

    /// Сериализация объекта через reflection.
    template<typename T>
    auto serialize_object(T const& obj) const -> std::string {
        std::string result = "{";
        bool first = true;

        constexpr auto members = std::meta::members_of(^T);

        for constexpr auto member : members {
            if constexpr (std::meta::is_data_member(member)) {
                if (!first) result += ",";
                first = false;

                constexpr auto name = std::meta::name_of(member);
                result += std::format("\"{}\":{}", name, serialize_value(obj.[:member:]));
            }
        }

        result += "}";
        return result;
    }

    /// Сериализация enum.
    template<typename E>
        requires std::is_enum_v<E>
    auto serialize_enum(E value) const -> std::string {
        constexpr auto enumerators = std::meta::enumerators_of(^E);

        for constexpr auto e : enumerators {
            if (value == [:e:]) {
                constexpr auto name = std::meta::name_of(e);
                return std::format("\"{}\"", name);
            }
        }

        return std::format("{}", static_cast<std::underlying_type_t<E>>(value));
    }

    /// Сериализация массива.
    template<std::ranges::range R>
    auto serialize_array(R const& range) const -> std::string {
        std::string result = "[";
        bool first = true;

        for (auto const& elem : range) {
            if (!first) result += ",";
            first = false;
            result += serialize_value(elem);
        }

        result += "]";
        return result;
    }

    /// Escape для JSON.
    static auto escape_json(std::string_view s) -> std::string {
        std::string result;
        result.reserve(s.size());

        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        result += std::format("\\u{:04x}", static_cast<unsigned char>(c));
                    } else {
                        result += c;
                    }
                    break;
            }
        }

        return result;
    }

    /// Десериализация значения.
    template<typename T>
    auto deserialize_value(std::string_view json, size_t& pos, T& obj) const
        -> std::expected<void, SerializeError> {
        skip_whitespace(json, pos);

        if constexpr (std::is_same_v<T, bool>) {
            if (json.substr(pos, 4) == "true") {
                obj = true;
                pos += 4;
            } else if (json.substr(pos, 5) == "false") {
                obj = false;
                pos += 5;
            } else {
                return std::unexpected(SerializeError::InvalidFormat);
            }
        } else if constexpr (std::is_integral_v<T>) {
            auto result = parse_number<T>(json, pos);
            if (!result) return std::unexpected(result.error());
            obj = *result;
        } else if constexpr (std::is_floating_point_v<T>) {
            auto result = parse_number<T>(json, pos);
            if (!result) return std::unexpected(result.error());
            obj = *result;
        } else if constexpr (std::is_same_v<T, std::string>) {
            auto result = parse_string(json, pos);
            if (!result) return std::unexpected(result.error());
            obj = std::move(*result);
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            // [x,y,z]
            if (json[pos] != '[') return std::unexpected(SerializeError::InvalidFormat);
            ++pos;

            auto x = parse_number<float>(json, pos);
            if (!x) return std::unexpected(x.error());
            skip_whitespace(json, pos);
            if (json[pos] != ',') return std::unexpected(SerializeError::InvalidFormat);
            ++pos;

            auto y = parse_number<float>(json, pos);
            if (!y) return std::unexpected(y.error());
            skip_whitespace(json, pos);
            if (json[pos] != ',') return std::unexpected(SerializeError::InvalidFormat);
            ++pos;

            auto z = parse_number<float>(json, pos);
            if (!z) return std::unexpected(z.error());
            skip_whitespace(json, pos);
            if (json[pos] != ']') return std::unexpected(SerializeError::InvalidFormat);
            ++pos;

            obj = glm::vec3{*x, *y, *z};
        } else if constexpr (std::ranges::range<T>) {
            // Array
            if (json[pos] != '[') return std::unexpected(SerializeError::InvalidFormat);
            ++pos;

            skip_whitespace(json, pos);
            while (json[pos] != ']') {
                typename T::value_type elem;
                auto result = deserialize_value(json, pos, elem);
                if (!result) return result;

                obj.push_back(std::move(elem));

                skip_whitespace(json, pos);
                if (json[pos] == ',') ++pos;
                skip_whitespace(json, pos);
            }
            ++pos; // ']'
        } else if constexpr (requires { std::meta::members_of(^T); }) {
            // Object
            if (json[pos] != '{') return std::unexpected(SerializeError::InvalidFormat);
            ++pos;

            skip_whitespace(json, pos);
            while (json[pos] != '}') {
                // Parse key
                auto key_result = parse_string(json, pos);
                if (!key_result) return std::unexpected(key_result.error());

                skip_whitespace(json, pos);
                if (json[pos] != ':') return std::unexpected(SerializeError::InvalidFormat);
                ++pos;
                skip_whitespace(json, pos);

                // Find member by name
                constexpr auto members = std::meta::members_of(^T);
                bool found = false;

                for constexpr auto member : members) {
                    if constexpr (std::meta::is_data_member(member)) {
                        constexpr auto name = std::meta::name_of(member);
                        if (*key_result == name) {
                            auto result = deserialize_value(json, pos, obj.[:member:]);
                            if (!result) return result;
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    // Skip unknown field
                    skip_value(json, pos);
                }

                skip_whitespace(json, pos);
                if (json[pos] == ',') ++pos;
                skip_whitespace(json, pos);
            }
            ++pos; // '}'
        }

        return {};
    }

    /// Skip whitespace.
    static auto skip_whitespace(std::string_view json, size_t& pos) -> void {
        while (pos < json.size() && std::isspace(json[pos])) {
            ++pos;
        }
    }

    /// Skip JSON value.
    static auto skip_value(std::string_view json, size_t& pos) -> void {
        skip_whitespace(json, pos);

        if (json[pos] == '{') {
            int depth = 1;
            ++pos;
            while (depth > 0 && pos < json.size()) {
                if (json[pos] == '{') ++depth;
                else if (json[pos] == '}') --depth;
                ++pos;
            }
        } else if (json[pos] == '[') {
            int depth = 1;
            ++pos;
            while (depth > 0 && pos < json.size()) {
                if (json[pos] == '[') ++depth;
                else if (json[pos] == ']') --depth;
                ++pos;
            }
        } else if (json[pos] == '"') {
            ++pos;
            while (pos < json.size() && json[pos] != '"') {
                if (json[pos] == '\\') ++pos;
                ++pos;
            }
            ++pos;
        } else {
            while (pos < json.size() &&
                   json[pos] != ',' && json[pos] != '}' &&
                   json[pos] != ']' && !std::isspace(json[pos])) {
                ++pos;
            }
        }
    }

    /// Parse number.
    template<typename N>
    auto parse_number(std::string_view json, size_t& pos) const
        -> std::expected<N, SerializeError> {
        skip_whitespace(json, pos);

        size_t start = pos;
        bool is_float = false;

        if (json[pos] == '-') ++pos;
        while (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '.')) {
            if (json[pos] == '.') is_float = true;
            ++pos;
        }

        std::string num_str(json.substr(start, pos - start));

        try {
            if constexpr (std::is_floating_point_v<N>) {
                return static_cast<N>(std::stod(num_str));
            } else {
                return static_cast<N>(std::stoll(num_str));
            }
        } catch (...) {
            return std::unexpected(SerializeError::InvalidFormat);
        }
    }

    /// Parse string.
    auto parse_string(std::string_view json, size_t& pos) const
        -> std::expected<std::string, SerializeError> {
        skip_whitespace(json, pos);

        if (json[pos] != '"') {
            return std::unexpected(SerializeError::InvalidFormat);
        }
        ++pos;

        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') {
                ++pos;
                if (pos >= json.size()) {
                    return std::unexpected(SerializeError::InvalidFormat);
                }
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }

        if (pos >= json.size()) {
            return std::unexpected(SerializeError::InvalidFormat);
        }
        ++pos; // '"'

        return result;
    }
};

} // namespace projectv::serialization
```

---

## 4. Zstd Compression

### 4.1 Zstd Compressor

```cpp
// ProjectV.Serialization.Zstd.cppm
export module ProjectV.Serialization.Zstd;

import std;
import zstd;

export namespace projectv::serialization {

/// Уровни сжатия Zstd.
export enum class ZstdLevel : int {
    Fast = 1,           ///< Максимальная скорость
    Balanced = 3,       ///< Баланс (по умолчанию)
    High = 10,          ///< Хорошее сжатие
    Ultra = 22          ///< Максимальное сжатие
};

/// Zstd компрессор.
export class ZstdCompressor {
public:
    explicit ZstdCompressor(ZstdLevel level = ZstdLevel::Balanced)
        : level_(static_cast<int>(level))
        , cctx_(ZSTD_createCCtx())
        , dctx_(ZSTD_createDCtx())
    {}

    ~ZstdCompressor() noexcept {
        if (cctx_) ZSTD_freeCCtx(cctx_);
        if (dctx_) ZSTD_freeDCtx(dctx_);
    }

    ZstdCompressor(ZstdCompressor&&) noexcept = default;
    ZstdCompressor& operator=(ZstdCompressor&&) noexcept = default;
    ZstdCompressor(const ZstdCompressor&) = delete;
    ZstdCompressor& operator=(const ZstdCompressor&) = delete;

    /// Сжатие данных.
    auto compress(std::span<uint8_t const> data) const
        -> SerializeResult<std::vector<uint8_t>> {

        auto bound = ZSTD_compressBound(data.size());
        std::vector<uint8_t> compressed(bound);

        auto result = ZSTD_compressCCtx(
            cctx_,
            compressed.data(), compressed.size(),
            data.data(), data.size(),
            level_
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(SerializeError::CompressionFailed);
        }

        compressed.resize(result);
        return compressed;
    }

    /// Распаковка данных.
    auto decompress(std::span<uint8_t const> compressed, size_t original_size) const
        -> SerializeResult<std::vector<uint8_t>> {

        std::vector<uint8_t> decompressed(original_size);

        auto result = ZSTD_decompressDCtx(
            dctx_,
            decompressed.data(), decompressed.size(),
            compressed.data(), compressed.size()
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(SerializeError::DecompressionFailed);
        }

        return decompressed;
    }

    /// Получение размера распакованных данных.
    auto get_decompressed_size(std::span<uint8_t const> compressed) const
        -> SerializeResult<size_t> {

        auto size = ZSTD_getFrameContentSize(
            compressed.data(), compressed.size());

        if (size == ZSTD_CONTENTSIZE_ERROR ||
            size == ZSTD_CONTENTSIZE_UNKNOWN) {
            return std::unexpected(SerializeError::DecompressionFailed);
        }

        return size;
    }

    /// Установка уровня сжатия.
    auto set_level(ZstdLevel level) -> void {
        level_ = static_cast<int>(level);
    }

private:
    int level_;
    ZSTD_CCtx* cctx_;
    ZSTD_DCtx* dctx_;
};

} // namespace projectv::serialization
```

---

## 5. Chunk File Format

### 5.1 Заголовок файла чанка

```cpp
// ProjectV.Voxel.ChunkFormat.cppm
export module ProjectV.Voxel.ChunkFormat;

import std;
import ProjectV.Serialization.Binary;
import ProjectV.Serialization.Zstd;

export namespace projectv {

/// Заголовок файла чанка (32 байта).
#pragma pack(push, 1)
export struct ChunkFileHeader {
    uint32_t magic{0x564F5821};      ///< "VOX!"
    uint32_t version{2};             ///< Версия формата
    uint32_t header_size{sizeof(ChunkFileHeader)};
    uint32_t metadata_size{0};       ///< Размер метаданных
    uint64_t uncompressed_size{0};   ///< Размер до сжатия
    uint64_t compressed_size{0};     ///< Размер после сжатия
    uint32_t checksum{0};            ///< CRC32 данных
    uint8_t compression_type{1};     ///< 1 = Zstd
    uint8_t compression_level{3};    ///< Уровень сжатия
    uint16_t flags{0};               ///< Флаги
    uint32_t reserved[2]{0};         ///< Зарезервировано
};

static_assert(sizeof(ChunkFileHeader) == 32, "Header must be 32 bytes");

/// Флаги чанка.
export enum class ChunkFlags : uint16_t {
    Empty = 1 << 0,
    Modified = 1 << 1,
    Generated = 1 << 2,
    Delta = 1 << 3,
    RLEEncoded = 1 << 4
};
#pragma pack(pop)

/// Сериализатор чанков.
export class ChunkSerializer {
public:
    enum class Strategy {
        Full,      ///< Полное сохранение
        Delta,     ///< Только изменения
        Smart      ///< Автоматический выбор
    };

    struct Config {
        Strategy strategy{Strategy::Smart};
        ZstdLevel compression_level{ZstdLevel::Balanced};
        bool enable_checksum{true};
    };

    /// Сохранение чанка.
    auto save(VoxelChunk const& chunk, Config const& config = {}) const
        -> SerializeResult<std::vector<uint8_t>> {

        // 1. Сериализация через reflection
        serialization::BinarySerializer bin;
        auto binary = bin.serialize(chunk);
        if (!binary.has_value()) {
            // Note: BinarySerializer::serialize returns vector, not expected
        }

        std::vector<uint8_t> const& binary_data = bin.serialize(chunk);

        // 2. Вычисление контрольной суммы
        uint32_t checksum = 0;
        if (config.enable_checksum) {
            checksum = calculate_crc32(binary_data);
        }

        // 3. Сжатие через Zstd
        serialization::ZstdCompressor compressor{config.compression_level};
        auto compressed = compressor.compress(binary_data);
        if (!compressed) {
            return std::unexpected(compressed.error());
        }

        // 4. Создание заголовка
        ChunkFileHeader header;
        header.uncompressed_size = binary_data.size();
        header.compressed_size = compressed->size();
        header.compression_level = static_cast<uint8_t>(config.compression_level);
        header.checksum = checksum;

        // 5. Объединение в один буфер
        std::vector<uint8_t> result;
        result.resize(sizeof(header) + compressed->size());

        std::memcpy(result.data(), &header, sizeof(header));
        std::memcpy(result.data() + sizeof(header),
                    compressed->data(), compressed->size());

        return result;
    }

    /// Загрузка чанка.
    auto load(std::span<uint8_t const> data) const
        -> SerializeResult<VoxelChunk> {

        // 1. Парсинг заголовка
        if (data.size() < sizeof(ChunkFileHeader)) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        ChunkFileHeader header;
        std::memcpy(&header, data.data(), sizeof(header));

        // 2. Проверка magic number
        if (header.magic != 0x564F5821) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 3. Проверка версии
        if (header.version > 2) {
            return std::unexpected(SerializeError::VersionMismatch);
        }

        // 4. Распаковка
        auto compressed_data = data.subspan(sizeof(header));
        serialization::ZstdCompressor compressor;
        auto decompressed = compressor.decompress(compressed_data, header.uncompressed_size);

        if (!decompressed) {
            return std::unexpected(decompressed.error());
        }

        // 5. Проверка контрольной суммы
        if (header.checksum != 0) {
            auto checksum = calculate_crc32(*decompressed);
            if (checksum != header.checksum) {
                return std::unexpected(SerializeError::InvalidChecksum);
            }
        }

        // 6. Десериализация через reflection
        serialization::BinarySerializer bin;
        return bin.deserialize<VoxelChunk>(*decompressed);
    }

private:
    static auto calculate_crc32(std::span<uint8_t const> data) -> uint32_t;
};

} // namespace projectv
```

---

## 6. ECS Serialization

### 6.1 Сериализация состояния ECS

```cpp
// ProjectV.ECS.Serialization.cppm
export module ProjectV.ECS.Serialization;

import std;
import std.meta;
import flecs;
import ProjectV.Serialization.Binary;
import ProjectV.Serialization.Zstd;
import ProjectV.Voxel.Chunk;

export namespace projectv::ecs {

/// Состояние ECS для сохранения.
/// Автоматическая reflection — никаких макросов!
export struct ECSState {
    uint32_t version{1};
    std::vector<std::string> entity_names;
    std::vector<TransformComponent> transforms;
    std::vector<PBRMaterial> materials;
    // Добавьте другие компоненты по необходимости
};

/// Сериализатор ECS.
export class ECSSerializer {
public:
    /// Экспорт состояния мира.
    auto export_world(flecs::world& world) -> ECSState {
        ECSState state;

        // Собираем все сущности с компонентами
        world.each([&](flecs::entity e, TransformComponent& transform) {
            state.entity_names.push_back(e.name().c_str() ? e.name().c_str() : "");
            state.transforms.push_back(transform);
        });

        world.each([&](flecs::entity e, PBRMaterial& material) {
            state.materials.push_back(material);
        });

        return state;
    }

    /// Импорт состояния мира.
    auto import_world(flecs::world& world, ECSState const& state) -> void {
        for (size_t i = 0; i < state.entity_names.size(); ++i) {
            auto entity = world.entity(state.entity_names[i].c_str());
            entity.set<TransformComponent>(state.transforms[i]);
        }

        for (size_t i = 0; i < state.materials.size(); ++i) {
            // Связываем материалы с сущностями
        }
    }

    /// Сохранение в файл.
    auto save_world(
        flecs::world& world,
        std::filesystem::path const& path,
        ZstdLevel compression_level = ZstdLevel::Balanced
    ) -> std::expected<void, SerializeError> {

        // Экспорт состояния
        ECSState state = export_world(world);

        // Сериализация через reflection
        serialization::BinarySerializer bin;
        auto binary = bin.serialize(state);

        // Сжатие
        serialization::ZstdCompressor compressor{compression_level};
        auto compressed = compressor.compress(binary);
        if (!compressed) {
            return std::unexpected(compressed.error());
        }

        // Запись в файл
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // Заголовок
        uint64_t uncompressed_size = binary.size();
        uint64_t compressed_size = compressed->size();
        file.write(reinterpret_cast<char const*>(&uncompressed_size), 8);
        file.write(reinterpret_cast<char const*>(&compressed_size), 8);
        file.write(reinterpret_cast<char const*>(compressed->data()),
                   compressed->size());

        return {};
    }

    /// Загрузка из файла.
    auto load_world(
        flecs::world& world,
        std::filesystem::path const& path
    ) -> std::expected<void, SerializeError> {

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        auto file_size = file.tellg();
        file.seekg(0);

        // Чтение заголовка
        uint64_t uncompressed_size, compressed_size;
        file.read(reinterpret_cast<char*>(&uncompressed_size), 8);
        file.read(reinterpret_cast<char*>(&compressed_size), 8);

        // Чтение сжатых данных
        std::vector<uint8_t> compressed(compressed_size);
        file.read(reinterpret_cast<char*>(compressed.data()), compressed_size);

        // Распаковка
        serialization::ZstdCompressor compressor;
        auto decompressed = compressor.decompress(compressed, uncompressed_size);
        if (!decompressed) {
            return std::unexpected(decompressed.error());
        }

        // Десериализация через reflection
        serialization::BinarySerializer bin;
        auto state = bin.deserialize<ECSState>(*decompressed);
        if (!state) {
            return std::unexpected(state.error());
        }

        // Импорт в мир
        import_world(world, *state);

        return {};
    }
};

} // namespace projectv::ecs
```

---

## 7. Формат файла мира

### 7.1 World Metadata

```cpp
// ProjectV.World.SaveSystem.cppm
export module ProjectV.World.SaveSystem;

import std;
import std.meta;
import ProjectV.Serialization.Json;
import ProjectV.Serialization.Binary;
import ProjectV.Serialization.Zstd;
import ProjectV.ECS.Serialization;

export namespace projectv {

/// Метаданные мира.
export struct WorldMetadata {
    std::string world_name;
    uint64_t created_timestamp{0};
    uint64_t modified_timestamp{0};
    uint32_t total_chunks{0};
    uint32_t version{1};
};

/// Система сохранения мира.
export class WorldSaveSystem {
public:
    explicit WorldSaveSystem(flecs::world& ecs)
        : ecs_(ecs)
    {}

    /// Сохранение мира.
    auto save_world(std::filesystem::path const& save_dir)
        -> std::expected<void, SerializeError> {

        // 1. Создание директории
        std::error_code ec;
        std::filesystem::create_directories(save_dir, ec);
        if (ec) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 2. Сохранение метаданных (JSON)
        WorldMetadata metadata;
        metadata.world_name = save_dir.filename().string();
        metadata.created_timestamp = get_current_timestamp();
        metadata.modified_timestamp = metadata.created_timestamp;

        serialization::JsonSerializer json;
        auto json_result = json.save(metadata, save_dir / "metadata.json");
        if (!json_result) {
            return json_result;
        }

        // 3. Сохранение ECS состояния
        ecs::ECSSerializer ecs_serializer;
        auto ecs_result = ecs_serializer.save_world(ecs_, save_dir / "ecs_state.bin");
        if (!ecs_result) {
            return ecs_result;
        }

        // 4. Сохранение чанков (постранично)
        // ...

        return {};
    }

    /// Загрузка мира.
    auto load_world(std::filesystem::path const& save_dir)
        -> std::expected<void, SerializeError> {

        // 1. Проверка существования
        if (!std::filesystem::exists(save_dir / "metadata.json")) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 2. Загрузка метаданных
        serialization::JsonSerializer json;
        auto metadata = json.load<WorldMetadata>(save_dir / "metadata.json");
        if (!metadata) {
            return std::unexpected(metadata.error());
        }

        // 3. Загрузка ECS состояния
        ecs::ECSSerializer ecs_serializer;
        auto ecs_result = ecs_serializer.load_world(ecs_, save_dir / "ecs_state.bin");
        if (!ecs_result) {
            return std::unexpected(ecs_result.error());
        }

        // 4. Загрузка чанков
        // ...

        return {};
    }

private:
    flecs::world& ecs_;

    static auto get_current_timestamp() -> uint64_t {
        return static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
    }
};

} // namespace projectv
```

---

## 8. Сравнение форматов

### Когда использовать JSON vs Binary

| Формат            | Использование                                  |
|-------------------|------------------------------------------------|
| **JSON**          | Конфигурация, отладка, обмен данными, metadata |
| **Binary**        | Сохранения, сетевой трафик, большие объёмы     |
| **Binary + Zstd** | Воксельные чанки, ресурсы, backup              |

### Уровни сжатия Zstd

| Уровень         | Скорость | Сжатие  | Использование         |
|-----------------|----------|---------|-----------------------|
| 1-3 (Fast)      | Высокая  | Средняя | Runtime сохранение    |
| 3-10 (Balanced) | Средняя  | Хорошая | Регулярные сохранения |
| 10-22 (Ultra)   | Низкая   | Лучшее  | Экспорт, архивация    |

---

## 9. Рекомендации

1. **Используйте C++26 P2996** — zero boilerplate для всех типов
2. **Бинарный формат** для чанков и сохранений — компактность
3. **JSON** для конфигурации — читаемость
4. **Zstd** для сжатия — баланс скорости и степени сжатия
5. **Не используйте макросы** — reflection работает автоматически

---

## Ссылки

- [P2996: Static Reflection for C++26](https://wg21.link/P2996)
- [Static Reflection](./13_reflection.md)
- [Zstd Documentation](../../libraries/zstd/00_overview.md)
