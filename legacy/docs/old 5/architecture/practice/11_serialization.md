# Serialization Strategy: Glaze + Zstd для воксельного мира [🟡 Уровень 2]

**🟡 Уровень 2: Средний** — Сериализация и сжатие для сохранения воксельных миров ProjectV.

## Обзор

ProjectV использует комбинацию **glaze** для reflection/сериализации и **zstd** для сжатия. Это обеспечивает:

- **Zero-copy** десериализацию где возможно
- **Compile-time reflection** без макросов
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
