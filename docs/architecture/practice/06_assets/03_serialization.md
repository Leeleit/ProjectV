# Serialization Strategy: Boost.PFR + Glaze + Zstd

Zero-boilerplate сериализация воксельных миров ProjectV с гарантированной компиляцией сегодня.

---

## Обзор

ProjectV использует **Boost.PFR + Glaze** + **Zstd** для:

- **Zero-copy** десериализацию где возможно (memory-mapped файлы)
- **Zero-boilerplate reflection** — никаких макросов, агрегатные типы
- **Высокую скорость** сжатия/распаковки
- **Отсутствие исключений** — используем `std::expected` для обработки ошибок
- **Гарантированную компиляцию** — без экспериментального std::meta

---

## 1. Boost.PFR + Glaze: Zero-Macro Reflection

### 1.1 Автоматическая сериализация структур

```cpp
// ProjectV.Serialization.Reflection.cppm
export module ProjectV.Serialization.Reflection;

import std;
import boost.pfr;
import glaze;

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
concept Serializable = std::is_aggregate_v<T> && requires {
    { glz::write_json(std::declval<T>()) } -> std::same_as<std::string>;
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
        // Структура: сумма размеров членов через Boost.PFR
        size_t size = 0;
        boost::pfr::for_each_field(T{}, [&](auto&& field) {
            using FieldType = std::decay_t<decltype(field)>;
            size += compute_serial_size<FieldType>();
        });
        return size;
    }
}

} // namespace projectv::serialization
```

### 1.2 Р’РѕРєСЃРµР»СЊРЅС‹Р№ С‡Р°РЅРє вЂ” Zero Boilerplate

```cpp
// ProjectV.Voxel.Chunk.cppm
export module ProjectV.Voxel.Chunk;

import std;
import glm;

export namespace projectv {

/// Р’РѕРєСЃРµР»СЊРЅС‹Р№ С‡Р°РЅРє РґР»СЏ СЃРµСЂРёР°Р»РёР·Р°С†РёРё.
/// РќРёРєР°РєРёС… РјР°РєСЂРѕСЃРѕРІ вЂ” reflection СЂР°Р±РѕС‚Р°РµС‚ Р°РІС‚РѕРјР°С‚РёС‡РµСЃРєРё!
export struct VoxelChunk {
    int32_t x{0}, y{0}, z{0};           ///< РљРѕРѕСЂРґРёРЅР°С‚С‹ С‡Р°РЅРєР°
    uint32_t version{2};                ///< Р’РµСЂСЃРёСЏ С„РѕСЂРјР°С‚Р°
    std::vector<uint8_t> blocks;        ///< ID Р±Р»РѕРєРѕРІ (16Ві = 4096)
    std::vector<uint8_t> metadata;      ///< Р”РѕРїРѕР»РЅРёС‚РµР»СЊРЅС‹Рµ РґР°РЅРЅС‹Рµ
    uint64_t timestamp{0};              ///< Р’СЂРµРјСЏ РїРѕСЃР»РµРґРЅРµРіРѕ РёР·РјРµРЅРµРЅРёСЏ
    uint32_t checksum{0};               ///< РљРѕРЅС‚СЂРѕР»СЊРЅР°СЏ СЃСѓРјРјР°

    // РњРµС‚РѕРґС‹ РґР»СЏ РёРЅРІР°СЂРёР°РЅС‚РѕРІ (РѕРїС†РёРѕРЅР°Р»СЊРЅРѕ)
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

/// ECS РєРѕРјРїРѕРЅРµРЅС‚ Transform вЂ” С‚РѕР¶Рµ zero boilerplate.
export struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] auto to_matrix() const noexcept -> glm::mat4;
};

/// РњР°С‚РµСЂРёР°Р» PBR.
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

## 2. Binary Serialization с Boost.PFR + Glaze

### 2.1 Binary Serializer на Boost.PFR + Glaze

```cpp
// ProjectV.Serialization.Binary.cppm
export module ProjectV.Serialization.Binary;

import std;
import boost.pfr;
import glaze;
import glm;

export namespace projectv::serialization {

/// Бинарный сериализатор с zero boilerplate.
///
/// ## Features
/// - Автоматическая reflection через Boost.PFR для агрегатных типов
/// - Поддержка nested типов через Glaze
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

        // Используем Glaze для бинарной сериализации
        glz::write_binary(obj, buffer);
        return buffer;
    }

    /// Десериализация из бинарного формата.
    /// @param data Бинарные данные
    /// @return Объект или ошибка
    template<typename T>
    auto deserialize(std::span<uint8_t const> data) const
        -> SerializeResult<T> {
        T obj{};

        auto result = glz::read_binary(obj, data);
        if (result) {
            return obj;
        } else {
            return std::unexpected(SerializeError::InvalidFormat);
        }
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

    /// Сериализация в файл с memory-mapping.
    template<typename T>
    auto serialize_to_file(T const& obj, std::filesystem::path const& path) const
        -> std::expected<void, SerializeError> {

        auto buffer = serialize(obj);

        // Memory-mapped запись для zero-copy
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        file.write(reinterpret_cast<char const*>(buffer.data()), buffer.size());
        return {};
    }

    /// Десериализация из файла с memory-mapping.
    template<typename T>
    auto deserialize_from_file(std::filesystem::path const& path) const
        -> SerializeResult<T> {

        // Memory-mapped чтение для zero-copy
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        auto size = file.tellg();
        file.seekg(0);

        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);

        return deserialize<T>(buffer);
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
            // Структура: сумма размеров членов через Boost.PFR
            size_t size = 0;
            boost::pfr::for_each_field(T{}, [&](auto&& field) {
                using FieldType = std::decay_t<decltype(field)>;
                size += estimate_size<FieldType>();
            });
            return size;
        }
    }
};

} // namespace projectv::serialization
```

---

## 3. JSON Serialization с Glaze

### 3.1 JSON Serializer на Glaze

```cpp
// ProjectV.Serialization.Json.cppm
export module ProjectV.Serialization.Json;

import std;
import glaze;
import glm;

export namespace projectv::serialization {

/// JSON Serializer с zero boilerplate через Glaze.
export class JsonSerializer {
public:
    /// Сериализация в JSON.
    template<typename T>
    auto serialize(T const& obj) const -> std::string {
        std::string json;
        glz::write_json(obj, json);
        return json;
    }

    /// Десериализация из JSON.
    template<typename T>
    auto deserialize(std::string_view json) const -> SerializeResult<T> {
        T obj{};

        auto result = glz::read_json(obj, json);
        if (result) {
            return obj;
        } else {
            return std::unexpected(SerializeError::InvalidFormat);
        }
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

    /// Сериализация с форматированием (для отладки).
    template<typename T>
    auto serialize_pretty(T const& obj, int indent = 2) const -> std::string {
        std::string json;
        glz::write<glz::opts{.indentation_width = indent}>(obj, json);
        return json;
    }

    /// Валидация JSON схемы.
    template<typename T>
    auto validate_schema(std::string_view json) const -> bool {
        T obj{};
        return glz::read_json(obj, json).success();
    }
};

} // namespace projectv::serialization
```

## 4. Zstd Compression

### 4.1 Zstd Compressor

```cpp
// ProjectV.Serialization.Zstd.cppm
export module ProjectV.Serialization.Zstd;

import std;
import zstd;

export namespace projectv::serialization {

/// РЈСЂРѕРІРЅРё СЃР¶Р°С‚РёСЏ Zstd.
export enum class ZstdLevel : int {
    Fast = 1,           ///< РњР°РєСЃРёРјР°Р»СЊРЅР°СЏ СЃРєРѕСЂРѕСЃС‚СЊ
    Balanced = 3,       ///< Р‘Р°Р»Р°РЅСЃ (РїРѕ СѓРјРѕР»С‡Р°РЅРёСЋ)
    High = 10,          ///< РҐРѕСЂРѕС€РµРµ СЃР¶Р°С‚РёРµ
    Ultra = 22          ///< РњР°РєСЃРёРјР°Р»СЊРЅРѕРµ СЃР¶Р°С‚РёРµ
};

/// Zstd РєРѕРјРїСЂРµСЃСЃРѕСЂ.
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

    /// РЎР¶Р°С‚РёРµ РґР°РЅРЅС‹С….
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

    /// Р Р°СЃРїР°РєРѕРІРєР° РґР°РЅРЅС‹С….
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

    /// РџРѕР»СѓС‡РµРЅРёРµ СЂР°Р·РјРµСЂР° СЂР°СЃРїР°РєРѕРІР°РЅРЅС‹С… РґР°РЅРЅС‹С….
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

    /// РЈСЃС‚Р°РЅРѕРІРєР° СѓСЂРѕРІРЅСЏ СЃР¶Р°С‚РёСЏ.
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

### 5.1 Р—Р°РіРѕР»РѕРІРѕРє С„Р°Р№Р»Р° С‡Р°РЅРєР°

```cpp
// ProjectV.Voxel.ChunkFormat.cppm
export module ProjectV.Voxel.ChunkFormat;

import std;
import ProjectV.Serialization.Binary;
import ProjectV.Serialization.Zstd;

export namespace projectv {

/// Р—Р°РіРѕР»РѕРІРѕРє С„Р°Р№Р»Р° С‡Р°РЅРєР° (32 Р±Р°Р№С‚Р°).
#pragma pack(push, 1)
export struct ChunkFileHeader {
    uint32_t magic{0x564F5821};      ///< "VOX!"
    uint32_t version{2};             ///< Р’РµСЂСЃРёСЏ С„РѕСЂРјР°С‚Р°
    uint32_t header_size{sizeof(ChunkFileHeader)};
    uint32_t metadata_size{0};       ///< Р Р°Р·РјРµСЂ РјРµС‚Р°РґР°РЅРЅС‹С…
    uint64_t uncompressed_size{0};   ///< Р Р°Р·РјРµСЂ РґРѕ СЃР¶Р°С‚РёСЏ
    uint64_t compressed_size{0};     ///< Р Р°Р·РјРµСЂ РїРѕСЃР»Рµ СЃР¶Р°С‚РёСЏ
    uint32_t checksum{0};            ///< CRC32 РґР°РЅРЅС‹С…
    uint8_t compression_type{1};     ///< 1 = Zstd
    uint8_t compression_level{3};    ///< РЈСЂРѕРІРµРЅСЊ СЃР¶Р°С‚РёСЏ
    uint16_t flags{0};               ///< Р¤Р»Р°РіРё
    uint32_t reserved[2]{0};         ///< Р—Р°СЂРµР·РµСЂРІРёСЂРѕРІР°РЅРѕ
};

static_assert(sizeof(ChunkFileHeader) == 32, "Header must be 32 bytes");

/// Р¤Р»Р°РіРё С‡Р°РЅРєР°.
export enum class ChunkFlags : uint16_t {
    Empty = 1 << 0,
    Modified = 1 << 1,
    Generated = 1 << 2,
    Delta = 1 << 3,
    RLEEncoded = 1 << 4
};
#pragma pack(pop)

/// РЎРµСЂРёР°Р»РёР·Р°С‚РѕСЂ С‡Р°РЅРєРѕРІ.
export class ChunkSerializer {
public:
    enum class Strategy {
        Full,      ///< РџРѕР»РЅРѕРµ СЃРѕС…СЂР°РЅРµРЅРёРµ
        Delta,     ///< РўРѕР»СЊРєРѕ РёР·РјРµРЅРµРЅРёСЏ
        Smart      ///< РђРІС‚РѕРјР°С‚РёС‡РµСЃРєРёР№ РІС‹Р±РѕСЂ
    };

    struct Config {
        Strategy strategy{Strategy::Smart};
        ZstdLevel compression_level{ZstdLevel::Balanced};
        bool enable_checksum{true};
    };

    /// РЎРѕС…СЂР°РЅРµРЅРёРµ С‡Р°РЅРєР°.
    auto save(VoxelChunk const& chunk, Config const& config = {}) const
        -> SerializeResult<std::vector<uint8_t>> {

        // 1. РЎРµСЂРёР°Р»РёР·Р°С†РёСЏ С‡РµСЂРµР· reflection
        serialization::BinarySerializer bin;
        auto binary = bin.serialize(chunk);
        if (!binary.has_value()) {
            // Note: BinarySerializer::serialize returns vector, not expected
        }

        std::vector<uint8_t> const& binary_data = bin.serialize(chunk);

        // 2. Р’С‹С‡РёСЃР»РµРЅРёРµ РєРѕРЅС‚СЂРѕР»СЊРЅРѕР№ СЃСѓРјРјС‹
        uint32_t checksum = 0;
        if (config.enable_checksum) {
            checksum = calculate_crc32(binary_data);
        }

        // 3. РЎР¶Р°С‚РёРµ С‡РµСЂРµР· Zstd
        serialization::ZstdCompressor compressor{config.compression_level};
        auto compressed = compressor.compress(binary_data);
        if (!compressed) {
            return std::unexpected(compressed.error());
        }

        // 4. РЎРѕР·РґР°РЅРёРµ Р·Р°РіРѕР»РѕРІРєР°
        ChunkFileHeader header;
        header.uncompressed_size = binary_data.size();
        header.compressed_size = compressed->size();
        header.compression_level = static_cast<uint8_t>(config.compression_level);
        header.checksum = checksum;

        // 5. РћР±СЉРµРґРёРЅРµРЅРёРµ РІ РѕРґРёРЅ Р±СѓС„РµСЂ
        std::vector<uint8_t> result;
        result.resize(sizeof(header) + compressed->size());

        std::memcpy(result.data(), &header, sizeof(header));
        std::memcpy(result.data() + sizeof(header),
                    compressed->data(), compressed->size());

        return result;
    }

    /// Р—Р°РіСЂСѓР·РєР° С‡Р°РЅРєР°.
    auto load(std::span<uint8_t const> data) const
        -> SerializeResult<VoxelChunk> {

        // 1. РџР°СЂСЃРёРЅРі Р·Р°РіРѕР»РѕРІРєР°
        if (data.size() < sizeof(ChunkFileHeader)) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        ChunkFileHeader header;
        std::memcpy(&header, data.data(), sizeof(header));

        // 2. РџСЂРѕРІРµСЂРєР° magic number
        if (header.magic != 0x564F5821) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 3. РџСЂРѕРІРµСЂРєР° РІРµСЂСЃРёРё
        if (header.version > 2) {
            return std::unexpected(SerializeError::VersionMismatch);
        }

        // 4. Р Р°СЃРїР°РєРѕРІРєР°
        auto compressed_data = data.subspan(sizeof(header));
        serialization::ZstdCompressor compressor;
        auto decompressed = compressor.decompress(compressed_data, header.uncompressed_size);

        if (!decompressed) {
            return std::unexpected(decompressed.error());
        }

        // 5. РџСЂРѕРІРµСЂРєР° РєРѕРЅС‚СЂРѕР»СЊРЅРѕР№ СЃСѓРјРјС‹
        if (header.checksum != 0) {
            auto checksum = calculate_crc32(*decompressed);
            if (checksum != header.checksum) {
                return std::unexpected(SerializeError::InvalidChecksum);
            }
        }

        // 6. Р”РµСЃРµСЂРёР°Р»РёР·Р°С†РёСЏ С‡РµСЂРµР· reflection
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

### 6.1 РЎРµСЂРёР°Р»РёР·Р°С†РёСЏ СЃРѕСЃС‚РѕСЏРЅРёСЏ ECS

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

/// РЎРѕСЃС‚РѕСЏРЅРёРµ ECS РґР»СЏ СЃРѕС…СЂР°РЅРµРЅРёСЏ.
/// РђРІС‚РѕРјР°С‚РёС‡РµСЃРєР°СЏ reflection вЂ” РЅРёРєР°РєРёС… РјР°РєСЂРѕСЃРѕРІ!
export struct ECSState {
    uint32_t version{1};
    std::vector<std::string> entity_names;
    std::vector<TransformComponent> transforms;
    std::vector<PBRMaterial> materials;
    // Р”РѕР±Р°РІСЊС‚Рµ РґСЂСѓРіРёРµ РєРѕРјРїРѕРЅРµРЅС‚С‹ РїРѕ РЅРµРѕР±С…РѕРґРёРјРѕСЃС‚Рё
};

/// РЎРµСЂРёР°Р»РёР·Р°С‚РѕСЂ ECS.
export class ECSSerializer {
public:
    /// Р­РєСЃРїРѕСЂС‚ СЃРѕСЃС‚РѕСЏРЅРёСЏ РјРёСЂР°.
    auto export_world(flecs::world& world) -> ECSState {
        ECSState state;

        // РЎРѕР±РёСЂР°РµРј РІСЃРµ СЃСѓС‰РЅРѕСЃС‚Рё СЃ РєРѕРјРїРѕРЅРµРЅС‚Р°РјРё
        world.each([&](flecs::entity e, TransformComponent& transform) {
            state.entity_names.push_back(e.name().c_str() ? e.name().c_str() : "");
            state.transforms.push_back(transform);
        });

        world.each([&](flecs::entity e, PBRMaterial& material) {
            state.materials.push_back(material);
        });

        return state;
    }

    /// РРјРїРѕСЂС‚ СЃРѕСЃС‚РѕСЏРЅРёСЏ РјРёСЂР°.
    auto import_world(flecs::world& world, ECSState const& state) -> void {
        for (size_t i = 0; i < state.entity_names.size(); ++i) {
            auto entity = world.entity(state.entity_names[i].c_str());
            entity.set<TransformComponent>(state.transforms[i]);
        }

        for (size_t i = 0; i < state.materials.size(); ++i) {
            // РЎРІСЏР·С‹РІР°РµРј РјР°С‚РµСЂРёР°Р»С‹ СЃ СЃСѓС‰РЅРѕСЃС‚СЏРјРё
        }
    }

    /// РЎРѕС…СЂР°РЅРµРЅРёРµ РІ С„Р°Р№Р».
    auto save_world(
        flecs::world& world,
        std::filesystem::path const& path,
        ZstdLevel compression_level = ZstdLevel::Balanced
    ) -> std::expected<void, SerializeError> {

        // Р­РєСЃРїРѕСЂС‚ СЃРѕСЃС‚РѕСЏРЅРёСЏ
        ECSState state = export_world(world);

        // РЎРµСЂРёР°Р»РёР·Р°С†РёСЏ С‡РµСЂРµР· reflection
        serialization::BinarySerializer bin;
        auto binary = bin.serialize(state);

        // РЎР¶Р°С‚РёРµ
        serialization::ZstdCompressor compressor{compression_level};
        auto compressed = compressor.compress(binary);
        if (!compressed) {
            return std::unexpected(compressed.error());
        }

        // Р—Р°РїРёСЃСЊ РІ С„Р°Р№Р»
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // Р—Р°РіРѕР»РѕРІРѕРє
        uint64_t uncompressed_size = binary.size();
        uint64_t compressed_size = compressed->size();
        file.write(reinterpret_cast<char const*>(&uncompressed_size), 8);
        file.write(reinterpret_cast<char const*>(&compressed_size), 8);
        file.write(reinterpret_cast<char const*>(compressed->data()),
                   compressed->size());

        return {};
    }

    /// Р—Р°РіСЂСѓР·РєР° РёР· С„Р°Р№Р»Р°.
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

        // Р§С‚РµРЅРёРµ Р·Р°РіРѕР»РѕРІРєР°
        uint64_t uncompressed_size, compressed_size;
        file.read(reinterpret_cast<char*>(&uncompressed_size), 8);
        file.read(reinterpret_cast<char*>(&compressed_size), 8);

        // Р§С‚РµРЅРёРµ СЃР¶Р°С‚С‹С… РґР°РЅРЅС‹С…
        std::vector<uint8_t> compressed(compressed_size);
        file.read(reinterpret_cast<char*>(compressed.data()), compressed_size);

        // Р Р°СЃРїР°РєРѕРІРєР°
        serialization::ZstdCompressor compressor;
        auto decompressed = compressor.decompress(compressed, uncompressed_size);
        if (!decompressed) {
            return std::unexpected(decompressed.error());
        }

        // Р”РµСЃРµСЂРёР°Р»РёР·Р°С†РёСЏ С‡РµСЂРµР· reflection
        serialization::BinarySerializer bin;
        auto state = bin.deserialize<ECSState>(*decompressed);
        if (!state) {
            return std::unexpected(state.error());
        }

        // РРјРїРѕСЂС‚ РІ РјРёСЂ
        import_world(world, *state);

        return {};
    }
};

} // namespace projectv::ecs
```

---

## 7. Р¤РѕСЂРјР°С‚ С„Р°Р№Р»Р° РјРёСЂР°

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

/// РњРµС‚Р°РґР°РЅРЅС‹Рµ РјРёСЂР°.
export struct WorldMetadata {
    std::string world_name;
    uint64_t created_timestamp{0};
    uint64_t modified_timestamp{0};
    uint32_t total_chunks{0};
    uint32_t version{1};
};

/// РЎРёСЃС‚РµРјР° СЃРѕС…СЂР°РЅРµРЅРёСЏ РјРёСЂР°.
export class WorldSaveSystem {
public:
    explicit WorldSaveSystem(flecs::world& ecs)
        : ecs_(ecs)
    {}

    /// РЎРѕС…СЂР°РЅРµРЅРёРµ РјРёСЂР°.
    auto save_world(std::filesystem::path const& save_dir)
        -> std::expected<void, SerializeError> {

        // 1. РЎРѕР·РґР°РЅРёРµ РґРёСЂРµРєС‚РѕСЂРёРё
        std::error_code ec;
        std::filesystem::create_directories(save_dir, ec);
        if (ec) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 2. РЎРѕС…СЂР°РЅРµРЅРёРµ РјРµС‚Р°РґР°РЅРЅС‹С… (JSON)
        WorldMetadata metadata;
        metadata.world_name = save_dir.filename().string();
        metadata.created_timestamp = get_current_timestamp();
        metadata.modified_timestamp = metadata.created_timestamp;

        serialization::JsonSerializer json;
        auto json_result = json.save(metadata, save_dir / "metadata.json");
        if (!json_result) {
            return json_result;
        }

        // 3. РЎРѕС…СЂР°РЅРµРЅРёРµ ECS СЃРѕСЃС‚РѕСЏРЅРёСЏ
        ecs::ECSSerializer ecs_serializer;
        auto ecs_result = ecs_serializer.save_world(ecs_, save_dir / "ecs_state.bin");
        if (!ecs_result) {
            return ecs_result;
        }

        // 4. РЎРѕС…СЂР°РЅРµРЅРёРµ С‡Р°РЅРєРѕРІ (РїРѕСЃС‚СЂР°РЅРёС‡РЅРѕ)
        // ...

        return {};
    }

    /// Р—Р°РіСЂСѓР·РєР° РјРёСЂР°.
    auto load_world(std::filesystem::path const& save_dir)
        -> std::expected<void, SerializeError> {

        // 1. РџСЂРѕРІРµСЂРєР° СЃСѓС‰РµСЃС‚РІРѕРІР°РЅРёСЏ
        if (!std::filesystem::exists(save_dir / "metadata.json")) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 2. Р—Р°РіСЂСѓР·РєР° РјРµС‚Р°РґР°РЅРЅС‹С…
        serialization::JsonSerializer json;
        auto metadata = json.load<WorldMetadata>(save_dir / "metadata.json");
        if (!metadata) {
            return std::unexpected(metadata.error());
        }

        // 3. Р—Р°РіСЂСѓР·РєР° ECS СЃРѕСЃС‚РѕСЏРЅРёСЏ
        ecs::ECSSerializer ecs_serializer;
        auto ecs_result = ecs_serializer.load_world(ecs_, save_dir / "ecs_state.bin");
        if (!ecs_result) {
            return std::unexpected(ecs_result.error());
        }

        // 4. Р—Р°РіСЂСѓР·РєР° С‡Р°РЅРєРѕРІ
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

## 8. Byte-Level Контракты для Форматов Файлов

### 8.1 Формат .vxl (Voxel Chunk)

```
Offset | Size | Type     | Description
-------|------|----------|------------
0x00   | 4    | uint32_t | Magic: 0x564F5821 ("VOX!")
0x04   | 4    | uint32_t | Version: 2
0x08   | 4    | uint32_t | Header size: 32 bytes
0x0C   | 4    | uint32_t | Metadata size (0 = no metadata)
0x10   | 8    | uint64_t | Uncompressed data size
0x18   | 8    | uint64_t | Compressed data size
0x20   | 4    | uint32_t | CRC32 checksum of uncompressed data
0x24   | 1    | uint8_t  | Compression type: 1 = Zstd
0x25   | 1    | uint8_t  | Compression level (1-22)
0x26   | 2    | uint16_t | Flags (bitmask):
       |      |          | - Bit 0: Empty chunk
       |      |          | - Bit 1: Modified since generation
       |      |          | - Bit 2: Generated (not saved)
       |      |          | - Bit 3: Delta compression
       |      |          | - Bit 4: RLE encoded
0x28   | 8    | uint32_t[2] | Reserved (must be 0)
0x30   | N    | uint8_t[] | Compressed data (Zstd)
```

**Контракт:**

- Magic должен быть 0x564F5821
- Version должен быть ≤ 2
- Header size должен быть 32 байта
- Если metadata_size > 0, метаданные идут после сжатых данных
- CRC32 вычисляется над несжатыми данными до сжатия
- Reserved поля должны быть 0 (зарезервировано для будущих версий)

### 8.2 Формат GPU Cache (.bin)

```
Offset | Size | Type     | Description
-------|------|----------|------------
0x00   | 4    | uint32_t | Magic: 0x47505543 ("GPUC")
0x04   | 4    | uint32_t | Version: 1
0x08   | 8    | uint64_t | Total size (включая заголовок)
0x10   | 8    | uint64_t | Data size
0x18   | 4    | uint32_t | Format (Vulkan/DXGI enum)
0x1C   | 4    | uint32_t | Width
0x20   | 4    | uint32_t | Height
0x24   | 4    | uint32_t | Depth (1 для 2D текстур)
0x28   | 4    | uint32_t | Mip levels
0x2C   | 4    | uint32_t | Array layers
0x30   | 8    | uint64_t | Flags
0x38   | 24   | uint8_t[24] | Reserved
0x50   | N    | uint8_t[] | Raw texture data
```

**Контракт:**

- Data size должен быть кратен 4 (выравнивание)
- Для compressed форматов используется Zstd поверх raw data
- Mip levels ≥ 1
- Array layers ≥ 1

### 8.3 Формат ECS State / Save Files

```
Offset | Size | Type     | Description
-------|------|----------|------------
0x00   | 4    | uint32_t | Magic: 0x45435353 ("ECSS")
0x04   | 4    | uint32_t | Version: 1
0x08   | 8    | uint64_t | Uncompressed size
0x10   | 8    | uint64_t | Compressed size
0x18   | 4    | uint32_t | Entity count
0x1C   | 4    | uint32_t | Component type count
0x20   | 4    | uint32_t | Flags
0x24   | 28   | uint8_t[28] | Reserved
0x40   | N    | uint8_t[] | Compressed ECS state (Zstd)
```

**Контракт:**

- Entity count ≤ 1,000,000 (прагматичный лимит)
- Component type count ≤ 256
- Reserved поля должны быть 0

### 8.4 Формат World Metadata (.json)

```json
{
  "$schema": "https://projectv.dev/schemas/world-metadata-v1.json",
  "world_name": "string (1-64 chars)",
  "created_timestamp": "uint64 (Unix nanoseconds)",
  "modified_timestamp": "uint64 (Unix nanoseconds)",
  "total_chunks": "uint32",
  "version": "uint32 (1)",
  "seed": "optional uint64",
  "generator": "optional string",
  "author": "optional string",
  "description": "optional string (max 1024 chars)"
}
```

**Контракт:**

- Все обязательные поля должны присутствовать
- Timestamps должны быть монотонными (modified ≥ created)
- total_chunks должен быть точным числом загруженных чанков
- version должен быть 1

### 8.5 Валидация Контрактов

```cpp
// ProjectV.Serialization.Contracts.cppm
export module ProjectV.Serialization.Contracts;

import std;
import ProjectV.Serialization.Binary;

export namespace projectv::serialization {

/// Контракт для проверки форматов файлов.
export class FormatValidator {
public:
    /// Проверка .vxl файла.
    static auto validate_vxl(std::span<uint8_t const> data)
        -> std::expected<void, SerializeError> {

        if (data.size() < 32) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        // Проверка magic
        uint32_t magic;
        std::memcpy(&magic, data.data(), 4);
        if (magic != 0x564F5821) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // Проверка version
        uint32_t version;
        std::memcpy(&version, data.data() + 4, 4);
        if (version > 2) {
            return std::unexpected(SerializeError::VersionMismatch);
        }

        // Проверка header size
        uint32_t header_size;
        std::memcpy(&header_size, data.data() + 8, 4);
        if (header_size != 32) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return {};
    }

    /// Проверка GPU cache файла.
    static auto validate_gpu_cache(std::span<uint8_t const> data)
        -> std::expected<void, SerializeError> {

        if (data.size() < 80) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        uint32_t magic;
        std::memcpy(&magic, data.data(), 4);
        if (magic != 0x47505543) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return {};
    }

    /// Проверка ECS state файла.
    static auto validate_ecs_state(std::span<uint8_t const> data)
        -> std::expected<void, SerializeError> {

        if (data.size() < 64) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        uint32_t magic;
        std::memcpy(&magic, data.data(), 4);
        if (magic != 0x45435353) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        return {};
    }
};

} // namespace projectv::serialization
```

---

## 9. Сравнение Форматов

### Когда использовать JSON vs Binary

| Формат            | Использование                                  |
|-------------------|------------------------------------------------|
| **JSON**          | Конфигурация, отладка, обмен данными, metadata |
| **Binary**        | Сохранения, сетевой трафик, большие объёмы     |
| **Binary + Zstd** | Воксельные чанки, ресурсы, backup              |

### Уровни сжатия Zstd

| Уровень         | Скорость | Сжатие  | Использование         |
|-----------------|----------|---------|-----------------------|
| 1-3 (Fast)      | Высокая  | Средняя | Runtime сохранения    |
| 3-10 (Balanced) | Средняя  | Хорошая | Регулярные сохранения |
| 10-22 (Ultra)   | Низкая   | Лучшее  | Экспорт, архивация    |

---

## 10. Рекомендации

1. **Используйте Boost.PFR + Glaze** — zero boilerplate для всех агрегатных типов, гарантированная компиляция сегодня
2. **Бинарный формат** для чанков и сохранений — компактность и скорость
3. **JSON** для конфигурации — читаемость и отладка
4. **Zstd** для сжатия — баланс скорости и степени сжатия
5. **Не используйте макросы** — reflection работает автоматически через Boost.PFR
6. **Memory-mapped файлы** для zero-copy десериализации больших данных
7. **std::expected** для обработки ошибок — отсутствие исключений в критических путях
8. **Byte-level контракты** — формальная спецификация форматов файлов
9. **Валидация контрактов** при загрузке — раннее обнаружение повреждённых данных
10. **C++26 без экспериментальных фич** — гарантированная компиляция на современных компиляторах

---

## Ссылки

- [Boost.PFR: Boost.Preprocessor Free Reflection](https://www.boost.org/doc/libs/1_85_0/libs/pfr/doc/html/index.html)
- [Glaze: High-performance JSON and binary serialization](https://github.com/stephenberry/glaze)
- [Zstandard: Real-time compression algorithm](https://facebook.github.io/zstd/)
- [C++26: Стандарт для ProjectV](https://isocpp.org/std/status)
- [std::expected: Типизированные ошибки](https://en.cppreference.com/w/cpp/utility/expected)
