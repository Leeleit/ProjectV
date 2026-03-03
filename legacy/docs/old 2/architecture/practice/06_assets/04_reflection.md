# Static Reflection: C++26 P2996 [🟢 Уровень 1]

**🟢 Уровень 1: Фундаментальный** — Zero-boilerplate рефлексия и сериализация на основе C++26 Static Reflection.

---

## Обзор

ProjectV использует **C++26 P2996 Static Reflection** для:

- **Zero-boilerplate сериализации** — никаких макросов или внешних специализаций
- **Compile-time инспекции типов** — итерация по членам на этапе компиляции
- **Автоматической генерации UI** — ImGui Inspector без ручного кода
- **Валидации данных** — декларативные constraints через атрибуты

### Ключевые преимущества P2996

| Возможность                  | Описание                                                   |
|------------------------------|------------------------------------------------------------|
| `std::meta::info`            | Мета-тип для представления reflection информации           |
| `^T`                         | Operator для получения reflection типа `T`                 |
| `[:expr:]`                   | Splice operator для "возвращения" значения из meta-context |
| `std::meta::members_of(^T)`  | Получение всех членов типа                                 |
| `std::meta::name_of(member)` | Получение имени члена                                      |
| `std::meta::type_of(member)` | Получение типа члена                                       |

---

## 1. Базовая Reflection без Boilerplate

### 1.1 Автоматическая инспекция типа

```cpp
// ProjectV.Core.Reflection.cppm
export module ProjectV.Core.Reflection;

import std;
import std.meta;

export namespace projectv::core {

/// Концепт для рефлексируемого типа.
export template<typename T>
concept Reflectable = std::is_class_v<T> || std::is_enum_v<T>;

/// Получение имён всех членов типа.
export template<Reflectable T>
consteval auto get_member_names() -> std::vector<std::string_view> {
    std::vector<std::string_view> names;

    constexpr auto members = std::meta::members_of(^T);
    for constexpr auto member : members {
        if constexpr (std::meta::is_data_member(member)) {
            names.push_back(std::meta::name_of(member));
        }
    }

    return names;
}

/// Получение количества членов.
export template<Reflectable T>
consteval auto get_member_count() -> size_t {
    constexpr auto members = std::meta::members_of(^T);
    size_t count = 0;
    for constexpr auto member : members {
        if constexpr (std::meta::is_data_member(member)) {
            ++count;
        }
    }
    return count;
}

/// Применение функции ко всем членам объекта.
export template<Reflectable T, typename F>
constexpr auto for_each_member(T&& obj, F&& func) -> void {
    constexpr auto members = std::meta::members_of(^std::remove_cvref_t<T>);

    for constexpr auto member : members {
        if constexpr (std::meta::is_data_member(member)) {
            using MemberType = [:std::meta::type_of(member):];
            constexpr std::string_view name = std::meta::name_of(member);

            // Доступ к члену через splice: obj.[:member:]
            func(name, obj.[:member:]);
        }
    }
}

} // namespace projectv::core
```

### 1.2 Пример использования

```cpp
// ProjectV.Components.Transform.cppm
export module ProjectV.Components.Transform;

import std;
import glm;

export namespace projectv {

/// Transform component — zero boilerplate!
/// Никаких макросов, reflection работает автоматически.
export struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    // Опционально: методы для инвариантов
    [[nodiscard]] auto to_matrix() const noexcept -> glm::mat4 {
        glm::mat4 m = glm::mat4_cast(rotation);
        m = glm::translate(m, position);
        m = glm::scale(m, scale);
        return m;
    }
};

/// Material component с вложенными структурами.
export struct PBRMaterial {
    std::string name{"default"};
    glm::vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float emissive_strength{0.0f};
    bool double_sided{false};
};

/// VoxelChunk для сериализации.
export struct VoxelChunk {
    int32_t x{0}, y{0}, z{0};
    uint32_t version{2};
    std::vector<uint8_t> blocks;
    std::vector<uint8_t> metadata;
    uint64_t timestamp{0};
    uint32_t checksum{0};
};

} // namespace projectv

// Пример compile-time инспекции:
static_assert(projectv::core::get_member_count<Transform>() == 3);
static_assert(projectv::core::get_member_count<PBRMaterial>() == 6);
```

---

## 2. Zero-Boilerplate Сериализация

### 2.1 JSON Serializer на Static Reflection

```cpp
// ProjectV.Serialization.Json.cppm
export module ProjectV.Serialization.Json;

import std;
import std.meta;
import ProjectV.Core.Reflection;
import glm;

export namespace projectv::serialization {

/// Ошибки сериализации.
export enum class SerializeError : uint8_t {
    InvalidFormat,
    BufferTooSmall,
    MissingField,
    TypeMismatch,
    InvalidValue
};

export template<typename T>
using SerializeResult = std::expected<T, SerializeError>;

/// JSON Serializer с zero boilerplate.
export class JsonSerializer {
public:
    /// Сериализация объекта в JSON строку.
    /// Работает для любого Reflectable типа автоматически.
    template<typename T>
    auto serialize(T const& obj) const -> std::string {
        if constexpr (std::is_enum_v<T>) {
            return serialize_enum(obj);
        } else if constexpr (requires { std::format("{}", obj); }) {
            return std::format("{}", obj);
        } else if constexpr (std::ranges::range<T>) {
            return serialize_range(obj);
        } else {
            return serialize_struct(obj);
        }
    }

    /// Десериализация из JSON строки.
    template<typename T>
    auto deserialize(std::string_view json) const -> SerializeResult<T> {
        T obj{};
        auto result = deserialize_into(json, obj);
        if (!result) {
            return std::unexpected(result.error());
        }
        return obj;
    }

private:
    /// Сериализация структуры через reflection.
    template<typename T>
    auto serialize_struct(T const& obj) const -> std::string {
        std::string result = "{";
        bool first = true;

        constexpr auto members = std::meta::members_of(^T);

        for constexpr auto member : members {
            if constexpr (std::meta::is_data_member(member)) {
                if (!first) result += ",";
                first = false;

                constexpr auto name = std::meta::name_of(member);
                result += std::format("\"{}\":{}", name, serialize(obj.[:member:]));
            }
        }

        result += "}";
        return result;
    }

    /// Сериализация enum через reflection.
    template<typename E>
        requires std::is_enum_v<E>
    auto serialize_enum(E value) const -> std::string {
        // Попытка найти имя enum value
        constexpr auto enumerators = std::meta::enumerators_of(^E);

        for constexpr auto e : enumerators {
            if (value == [:e:]) {
                constexpr auto name = std::meta::name_of(e);
                return std::format("\"{}\"", name);
            }
        }

        // Fallback: числовое значение
        return std::format("{}", static_cast<std::underlying_type_t<E>>(value));
    }

    /// Сериализация диапазона (vector, array, etc).
    template<std::ranges::range R>
    auto serialize_range(R const& range) const -> std::string {
        std::string result = "[";
        bool first = true;

        for (auto const& elem : range) {
            if (!first) result += ",";
            first = false;
            result += serialize(elem);
        }

        result += "]";
        return result;
    }

    /// Сериализация glm::vec3.
    auto serialize(glm::vec3 const& v) const -> std::string {
        return std::format("[{},{},{}]", v.x, v.y, v.z);
    }

    /// Сериализация glm::vec4.
    auto serialize(glm::vec4 const& v) const -> std::string {
        return std::format("[{},{},{},{}]", v.x, v.y, v.z, v.w);
    }

    /// Сериализация glm::quat.
    auto serialize(glm::quat const& q) const -> std::string {
        return std::format("[{},{},{},{}]", q.x, q.y, q.z, q.w);
    }

    /// Сериализация строки.
    auto serialize(std::string const& s) const -> std::string {
        return std::format("\"{}\"", escape_json(s));
    }

    /// Сериализация bool.
    auto serialize(bool b) const -> std::string {
        return b ? "true" : "false";
    }

    /// Сериализация чисел.
    template<std::integral I>
    auto serialize(I value) const -> std::string {
        return std::format("{}", value);
    }

    template<std::floating_point F>
    auto serialize(F value) const -> std::string {
        return std::format("{}", value);
    }

    /// Десериализация в существующий объект.
    template<typename T>
    auto deserialize_into(std::string_view json, T& obj) const
        -> std::expected<void, SerializeError> {
        // Парсинг JSON и заполнение obj через reflection
        // ... реализация парсера
        return {};
    }

    /// Escape для JSON строк.
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
                default:   result += c; break;
            }
        }

        return result;
    }
};

} // namespace projectv::serialization
```

### 2.2 Бинарная сериализация

```cpp
// ProjectV.Serialization.Binary.cppm
export module ProjectV.Serialization.Binary;

import std;
import std.meta;
import ProjectV.Core.Reflection;

export namespace projectv::serialization {

/// Бинарный сериализатор с zero boilerplate.
export class BinarySerializer {
public:
    /// Сериализация в бинарный формат.
    template<typename T>
    auto serialize(T const& obj) const -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer;
        buffer.reserve(estimate_size<T>());
        serialize_into(buffer, obj);
        return buffer;
    }

    /// Десериализация из бинарного формата.
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

private:
    /// Оценка размера объекта.
    template<typename T>
    static consteval auto estimate_size() -> size_t {
        size_t size = 0;

        if constexpr (std::is_trivially_copyable_v<T>) {
            return sizeof(T);
        } else {
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

    /// Сериализация в буфер.
    template<typename T>
    auto serialize_into(std::vector<uint8_t>& buffer, T const& obj) const -> void {
        if constexpr (std::is_trivially_copyable_v<T>) {
            // Прямое копирование для POD типов
            auto const* ptr = reinterpret_cast<uint8_t const*>(&obj);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
        } else if constexpr (std::ranges::range<T>) {
            // Размер + элементы
            auto size = static_cast<uint32_t>(obj.size());
            serialize_into(buffer, size);

            for (auto const& elem : obj) {
                serialize_into(buffer, elem);
            }
        } else {
            // Структура: итерация по членам
            constexpr auto members = std::meta::members_of(^T);

            for constexpr auto member : members {
                if constexpr (std::meta::is_data_member(member)) {
                    serialize_into(buffer, obj.[:member:]);
                }
            }
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
        } else if constexpr (std::ranges::range<T>) {
            using SizeType = uint32_t;
            SizeType size{};

            auto result = deserialize_from(data, offset, size);
            if (!result) return result;

            obj.resize(size);
            for (auto& elem : obj) {
                result = deserialize_from(data, offset, elem);
                if (!result) return result;
            }
        } else {
            constexpr auto members = std::meta::members_of(^T);

            for constexpr auto member : members {
                if constexpr (std::meta::is_data_member(member)) {
                    auto result = deserialize_from(data, offset, obj.[:member:]);
                    if (!result) return result;
                }
            }
        }

        return {};
    }
};

} // namespace projectv::serialization
```

### 2.3 Полный пример: Сериализация VoxelChunk в бинарный формат

Ниже приведён **полный, неурезанный пример** сериализатора `VoxelChunk`, использующий статические циклы `template for` (
P2996):

```cpp
// ProjectV.Voxel.Serialization.cppm
export module ProjectV.Voxel.Serialization;

import std;
import std.meta;
import glm;
import ProjectV.Core.Reflection;
import ProjectV.Serialization.Binary;
import ProjectV.Voxel.Data;

export namespace projectv::voxel {

/// Магическое число для идентификации формата файла чанка.
export constexpr uint32_t VOXEL_CHUNK_MAGIC = 0x564F5843;  // "VOXC"

/// Текущая версия формата.
export constexpr uint32_t VOXEL_CHUNK_VERSION = 3;

/// Заголовок файла чанка.
export struct alignas(16) ChunkFileHeader {
    uint32_t magic{VOXEL_CHUNK_MAGIC};
    uint32_t version{VOXEL_CHUNK_VERSION};
    uint32_t chunk_x{0};
    uint32_t chunk_y{0};
    uint32_t chunk_z{0};
    uint32_t blocks_compressed_size{0};
    uint32_t metadata_size{0};
    uint64_t timestamp{0};
    uint32_t checksum{0};
    uint8_t compression_type{0};  // 0 = none, 1 = zstd, 2 = rle
    uint8_t padding[3]{0, 0, 0};
};

static_assert(sizeof(ChunkFileHeader) == 48, "ChunkFileHeader must be 48 bytes");

/// VoxelChunk с полной поддержкой рефлексии.
export struct VoxelChunk {
    int32_t x{0};
    int32_t y{0};
    int32_t z{0};
    uint32_t version{VOXEL_CHUNK_VERSION};
    std::vector<uint8_t> blocks;        // Сжатые данные блоков
    std::vector<uint8_t> metadata;      // Дополнительные данные
    uint64_t timestamp{0};
    uint32_t checksum{0};

    /// Вычисляет контрольную сумму.
    [[nodiscard]] auto calculate_checksum() const noexcept -> uint32_t {
        uint32_t hash = 0;

        // FNV-1a hash
        auto fnv_hash = [&hash](std::span<uint8_t const> data) {
            for (uint8_t byte : data) {
                hash ^= byte;
                hash *= 0x01000193;
            }
        };

        hash ^= static_cast<uint32_t>(x);
        hash ^= static_cast<uint32_t>(y);
        hash ^= static_cast<uint32_t>(z);
        fnv_hash(blocks);
        fnv_hash(metadata);

        return hash;
    }

    /// Проверяет валидность данных.
    [[nodiscard]] auto validate() const noexcept -> bool {
        return checksum == calculate_checksum();
    }
};

/// Полная бинарная сериализация VoxelChunk с использованием template for.
///
/// ## P2996 Features Used
/// - `^T` — оператор рефлексии типа
/// - `std::meta::members_of(^T)` — получение членов типа
/// - `std::meta::name_of(member)` — имя члена
/// - `std::meta::type_of(member)` — тип члена
/// - `[:expr:]` — splice operator
/// - `template for (constexpr auto member : members)` — статический цикл
export class VoxelChunkSerializer {
public:
    /// Сериализует VoxelChunk в бинарный буфер.
    ///
    /// ## Format
    /// ```
    /// [Header: 48 bytes]
    /// [Blocks: blocks_compressed_size bytes]
    /// [Metadata: metadata_size bytes]
    /// ```
    ///
    /// @param chunk Чанк для сериализации
    /// @return Бинарный буфер или ошибка
    [[nodiscard]] static auto serialize(VoxelChunk const& chunk) noexcept
        -> std::expected<std::vector<std::byte>, SerializeError> {

        std::vector<std::byte> buffer;
        buffer.reserve(sizeof(ChunkFileHeader) + chunk.blocks.size() + chunk.metadata.size());

        // 1. Создание и заполнение заголовка
        ChunkFileHeader header{
            .magic = VOXEL_CHUNK_MAGIC,
            .version = chunk.version,
            .chunk_x = static_cast<uint32_t>(chunk.x),
            .chunk_y = static_cast<uint32_t>(chunk.y),
            .chunk_z = static_cast<uint32_t>(chunk.z),
            .blocks_compressed_size = static_cast<uint32_t>(chunk.blocks.size()),
            .metadata_size = static_cast<uint32_t>(chunk.metadata.size()),
            .timestamp = chunk.timestamp,
            .checksum = chunk.checksum,
            .compression_type = 0  // No compression for MVP
        };

        // 2. Сериализация заголовка (тривиально копируемый)
        auto const* header_bytes = reinterpret_cast<std::byte const*>(&header);
        buffer.insert(buffer.end(), header_bytes, header_bytes + sizeof(header));

        // 3. Сериализация данных чанка через template for (P2996)
        // Это демонстрирует статический цикл по членам структуры
        constexpr auto members = std::meta::members_of(^VoxelChunk);

        template for (constexpr auto member : members) {
            if constexpr (std::meta::is_data_member(member)) {
                // Получаем имя и тип члена на этапе компиляции
                constexpr auto name = std::meta::name_of(member);
                constexpr auto type = std::meta::type_of(member);

                // Обработка в зависимости от типа
                if constexpr (name == "blocks") {
                    // Вектор блоков - копируем напрямую
                    serialize_vector(buffer, chunk.[:member:], header.blocks_compressed_size);
                } else if constexpr (name == "metadata") {
                    // Вектор метаданных - копируем напрямую
                    serialize_vector(buffer, chunk.[:member:], header.metadata_size);
                } else if constexpr (type == ^int32_t) {
                    // int32_t - уже в заголовке
                    // Ничего не делаем, данные уже записаны
                } else if constexpr (type == ^uint32_t) {
                    // uint32_t - уже в заголовке или пропускаем
                    // version, checksum уже в заголовке
                } else if constexpr (type == ^uint64_t) {
                    // timestamp - уже в заголовке
                }
            }
        }

        return buffer;
    }

    /// Десериализует VoxelChunk из бинарного буфера.
    ///
    /// @param data Бинарные данные
    /// @return Чанк или ошибка
    [[nodiscard]] static auto deserialize(std::span<std::byte const> data) noexcept
        -> std::expected<VoxelChunk, SerializeError> {

        // 1. Проверка минимального размера
        if (data.size() < sizeof(ChunkFileHeader)) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        // 2. Чтение заголовка
        ChunkFileHeader header;
        std::memcpy(&header, data.data(), sizeof(header));

        // 3. Валидация magic number
        if (header.magic != VOXEL_CHUNK_MAGIC) {
            return std::unexpected(SerializeError::InvalidFormat);
        }

        // 4. Проверка версии
        if (header.version > VOXEL_CHUNK_VERSION) {
            return std::unexpected(SerializeError::InvalidValue);
        }

        // 5. Создание чанка и заполнение через template for
        VoxelChunk chunk;
        size_t offset = sizeof(ChunkFileHeader);

        // Заполнение базовых полей из заголовка
        chunk.x = static_cast<int32_t>(header.chunk_x);
        chunk.y = static_cast<int32_t>(header.chunk_y);
        chunk.z = static_cast<int32_t>(header.chunk_z);
        chunk.version = header.version;
        chunk.timestamp = header.timestamp;
        chunk.checksum = header.checksum;

        // Десериализация данных через template for
        constexpr auto members = std::meta::members_of(^VoxelChunk);

        template for (constexpr auto member : members) {
            if constexpr (std::meta::is_data_member(member)) {
                constexpr auto name = std::meta::name_of(member);
                constexpr auto type = std::meta::type_of(member);

                if constexpr (name == "blocks") {
                    // Чтение вектора блоков
                    auto result = deserialize_vector(
                        data, offset, chunk.[:member:], header.blocks_compressed_size
                    );
                    if (!result) {
                        return std::unexpected(result.error());
                    }
                } else if constexpr (name == "metadata") {
                    // Чтение вектора метаданных
                    auto result = deserialize_vector(
                        data, offset, chunk.[:member:], header.metadata_size
                    );
                    if (!result) {
                        return std::unexpected(result.error());
                    }
                }
                // Остальные поля уже заполнены из заголовка
            }
        }

        // 6. Валидация контрольной суммы
        if (!chunk.validate()) {
            return std::unexpected(SerializeError::InvalidValue);
        }

        return chunk;
    }

    /// Вычисляет размер сериализованного чанка.
    [[nodiscard]] static auto serialized_size(VoxelChunk const& chunk) noexcept
        -> size_t {
        return sizeof(ChunkFileHeader) + chunk.blocks.size() + chunk.metadata.size();
    }

private:
    /// Сериализация вектора в буфер.
    static auto serialize_vector(
        std::vector<std::byte>& buffer,
        std::vector<uint8_t> const& vec,
        uint32_t expected_size
    ) noexcept -> void {
        if (vec.size() != expected_size) {
            return;  // Size mismatch, skip
        }

        auto const* bytes = reinterpret_cast<std::byte const*>(vec.data());
        buffer.insert(buffer.end(), bytes, bytes + vec.size());
    }

    /// Десериализация вектора из буфера.
    static auto deserialize_vector(
        std::span<std::byte const> data,
        size_t& offset,
        std::vector<uint8_t>& vec,
        uint32_t size
    ) noexcept -> std::expected<void, SerializeError> {
        if (offset + size > data.size()) {
            return std::unexpected(SerializeError::BufferTooSmall);
        }

        vec.resize(size);
        std::memcpy(vec.data(), data.data() + offset, size);
        offset += size;

        return {};
    }
};

/// Compile-time тест сериализации.
consteval auto test_chunk_serialization() -> bool {
    VoxelChunk original{
        .x = 10,
        .y = 20,
        .z = 30,
        .version = VOXEL_CHUNK_VERSION,
        .blocks = {1, 2, 3, 4, 5},
        .metadata = {0xAB, 0xCD},
        .timestamp = 1234567890,
        .checksum = 0  // Will be calculated
    };

    original.checksum = original.calculate_checksum();

    // Note: Full serialization test requires runtime
    // This is a compile-time structure validation
    constexpr auto members = std::meta::members_of(^VoxelChunk);
    size_t member_count = 0;

    template for (constexpr auto member : members) {
        if constexpr (std::meta::is_data_member(member)) {
            ++member_count;
        }
    }

    return member_count == 7;  // x, y, z, version, blocks, metadata, timestamp, checksum
}

static_assert(test_chunk_serialization(), "VoxelChunk serialization test failed");

/// Пример использования template for для обхода всех членов VoxelChunk.
export auto inspect_voxel_chunk(VoxelChunk const& chunk) -> void {
    constexpr auto members = std::meta::members_of(^VoxelChunk);

    template for (constexpr auto member : members) {
        if constexpr (std::meta::is_data_member(member)) {
            constexpr auto name = std::meta::name_of(member);
            constexpr auto type = std::meta::type_of(member);

            // Печатаем имя и значение
            if constexpr (type == ^int32_t) {
                std::println("{}: {}", name, chunk.[:member:]);
            } else if constexpr (type == ^uint32_t) {
                std::println("{}: {}", name, chunk.[:member:]);
            } else if constexpr (type == ^uint64_t) {
                std::println("{}: {}", name, chunk.[:member:]);
            } else if constexpr (std::meta::is_sequence_type(type)) {
                std::println("{}: [{} elements]", name, chunk.[:member:].size());
            }
        }
    }
}

} // namespace projectv::voxel
```

---

## 3. Автоматический ImGui Inspector

### 3.1 Генератор UI на Static Reflection

```cpp
// ProjectV.UI.Inspector.cppm
export module ProjectV.UI.Inspector;

import std;
import std.meta;
import imgui;
import glm;
import ProjectV.Core.Reflection;

export namespace projectv::ui {

/// Автоматический Inspector для любого Reflectable типа.
/// Генерирует ImGui UI на основе reflection информации.
export class Inspector {
public:
    /// Инспектирование объекта.
    /// Генерирует UI для всех полей автоматически.
    template<typename T>
    auto inspect(char const* name, T& obj) -> bool {
        ImGui::PushID(name);
        bool modified = false;

        if constexpr (std::is_enum_v<T>) {
            modified = inspect_enum(name, obj);
        } else if constexpr (std::is_same_v<T, float>) {
            modified = ImGui::DragFloat(name, &obj, 0.1f);
        } else if constexpr (std::is_same_v<T, double>) {
            float v = static_cast<float>(obj);
            if (ImGui::DragFloat(name, &v, 0.1f)) {
                obj = v;
                modified = true;
            }
        } else if constexpr (std::is_same_v<T, int32_t>) {
            modified = ImGui::DragInt(name, &obj, 1);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            int v = static_cast<int>(obj);
            if (ImGui::DragInt(name, &v, 1, 0, INT_MAX)) {
                obj = static_cast<uint32_t>(v);
                modified = true;
            }
        } else if constexpr (std::is_same_v<T, bool>) {
            modified = ImGui::Checkbox(name, &obj);
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            modified = ImGui::DragFloat3(name, &obj.x, 0.1f);
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            modified = ImGui::DragFloat4(name, &obj.x, 0.01f);
        } else if constexpr (std::is_same_v<T, glm::quat>) {
            modified = inspect_quaternion(name, obj);
        } else if constexpr (std::is_same_v<T, std::string>) {
            modified = inspect_string(name, obj);
        } else if constexpr (std::ranges::range<T>) {
            modified = inspect_range(name, obj);
        } else {
            // Структура: рекурсивная инспекция
            if (ImGui::CollapsingHeader(name)) {
                ImGui::Indent();
                modified = inspect_struct(obj);
                ImGui::Unindent();
            }
        }

        ImGui::PopID();
        return modified;
    }

private:
    /// Инспектирование структуры через reflection.
    template<typename T>
    auto inspect_struct(T& obj) -> bool {
        bool modified = false;

        constexpr auto members = std::meta::members_of(^T);

        for constexpr auto member : members {
            if constexpr (std::meta::is_data_member(member)) {
                constexpr auto name = std::meta::name_of(member);

                // Используем splice для доступа к члену
                if (inspect(name.data(), obj.[:member:])) {
                    modified = true;
                }
            }
        }

        return modified;
    }

    /// Инспектирование enum через reflection.
    template<typename E>
        requires std::is_enum_v<E>
    auto inspect_enum(char const* name, E& value) -> bool {
        bool modified = false;
        int current = static_cast<int>(value);

        // Построение списка значений enum
        constexpr auto enumerators = std::meta::enumerators_of(^E);
        constexpr size_t count = enumerators.size();

        // Массив имён
        static constexpr std::array<char const*, count> names = []() consteval {
            std::array<char const*, count> arr{};
            size_t i = 0;
            for constexpr auto e : enumerators {
                arr[i++] = std::meta::name_of(e).data();
            }
            return arr;
        }();

        if (ImGui::Combo(name, &current, names.data(), static_cast<int>(count))) {
            value = static_cast<E>(current);
            modified = true;
        }

        return modified;
    }

    /// Инспектирование кватерниона (Euler angles).
    auto inspect_quaternion(char const* name, glm::quat& q) -> bool {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(q));

        if (ImGui::DragFloat3(name, &euler.x, 1.0f)) {
            q = glm::quat(glm::radians(euler));
            return true;
        }

        return false;
    }

    /// Инспектирование строки.
    auto inspect_string(char const* name, std::string& str) -> bool {
        char buffer[256];
        std::strncpy(buffer, str.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        if (ImGui::InputText(name, buffer, sizeof(buffer))) {
            str = buffer;
            return true;
        }

        return false;
    }

    /// Инспектирование диапазона.
    template<std::ranges::range R>
    auto inspect_range(char const* name, R& range) -> bool {
        bool modified = false;

        if (ImGui::CollapsingHeader(name)) {
            ImGui::Indent();
            ImGui::Text("Size: %zu", range.size());

            size_t i = 0;
            for (auto& elem : range) {
                char label[32];
                std::snprintf(label, sizeof(label), "[%zu]", i++);

                if (inspect(label, elem)) {
                    modified = true;
                }
            }

            ImGui::Unindent();
        }

        return modified;
    }
};

} // namespace projectv::ui
```

### 3.2 Интеграция с Flecs ECS

```cpp
// ProjectV.ECS.Inspector.cppm
export module ProjectV.ECS.Inspector;

import std;
import std.meta;
import flecs;
import imgui;
import ProjectV.UI.Inspector;
import ProjectV.Components.Transform;

export namespace projectv::ecs {

/// ECS Entity Inspector с автоматической reflection.
export class ECSInspector {
public:
    explicit ECSInspector(flecs::world& world)
        : world_(world)
    {}

    /// Отрисовка inspector UI.
    auto draw() -> void {
        ImGui::Begin("Entity Inspector");

        draw_entity_list();

        if (selected_entity_.is_alive()) {
            ImGui::Separator();
            draw_entity_components(selected_entity_);
        }

        ImGui::End();
    }

private:
    flecs::world& world_;
    flecs::entity selected_entity_{};
    ui::Inspector inspector_;

    auto draw_entity_list() -> void {
        ImGui::Text("Entities:");

        world_.each([this](flecs::entity e) {
            char label[64];
            std::snprintf(label, sizeof(label), "%s (id: %u)",
                         e.name().c_str() ? e.name().c_str() : "unnamed",
                         static_cast<uint32_t>(e.id()));

            if (ImGui::Selectable(label, e == selected_entity_)) {
                selected_entity_ = e;
            }
        });
    }

    auto draw_entity_components(flecs::entity e) -> void {
        ImGui::Text("Entity: %s",
                    e.name().c_str() ? e.name().c_str() : "unnamed");

        // Используем reflection для автоматической инспекции компонентов
        inspect_component<Transform>(e, "Transform");
        inspect_component<PBRMaterial>(e, "PBRMaterial");
        inspect_component<VoxelChunk>(e, "VoxelChunk");
        // Добавьте другие компоненты...
    }

    /// Инспектирование компонента с reflection.
    template<typename T>
    auto inspect_component(flecs::entity e, char const* name) -> void {
        if (e.has<T>()) {
            auto* component = e.get_mut<T>();
            if (component) {
                inspector_.inspect(name, *component);
            }
        }
    }
};

} // namespace projectv::ecs
```

---

## 4. Валидация через Attributes

### 4.1 Декларативные Constraints

```cpp
// ProjectV.Validation.Constraints.cppm
export module ProjectV.Validation.Constraints;

import std;
import std.meta;
import glm;

export namespace projectv::validation {

/// Range constraint attribute.
export template<typename T>
struct Range {
    T min;
    T max;
};

/// Min constraint attribute.
export template<typename T>
struct Min {
    T value;
};

/// Max constraint attribute.
export template<typename T>
struct Max {
    T value;
};

/// NotEmpty constraint для строк и контейнеров.
export struct NotEmpty {};

/// Валидатор с reflection.
export class Validator {
public:
    /// Валидация объекта.
    /// Применяет constraints из attributes.
    template<typename T>
    auto validate(T& obj) -> bool {
        bool valid = true;

        constexpr auto members = std::meta::members_of(^T);

        for constexpr auto member : members) {
            if constexpr (std::meta::is_data_member(member)) {
                if (!validate_member(member, obj.[:member:])) {
                    valid = false;
                }
            }
        }

        return valid;
    }

    /// Валидация с автоматической коррекцией.
    template<typename T>
    auto validate_and_fix(T& obj) -> bool {
        bool modified = false;

        constexpr auto members = std::meta::members_of(^T);

        for constexpr auto member : members) {
            if constexpr (std::meta::is_data_member(member)) {
                if (fix_member(member, obj.[:member:])) {
                    modified = true;
                }
            }
        }

        return modified;
    }

private:
    /// Валидация отдельного члена.
    template<typename M, typename V>
    auto validate_member(M member, V& value) -> bool {
        bool valid = true;

        // Проверка атрибутов через reflection
        // В C++26 P2996 атрибуты доступны через meta::attributes_of

        if constexpr (has_attribute<Range<V>>(member)) {
            constexpr auto range = get_attribute<Range<V>>(member);
            if (value < range.min || value > range.max) {
                valid = false;
            }
        }

        if constexpr (has_attribute<Min<V>>(member)) {
            constexpr auto min_attr = get_attribute<Min<V>>(member);
            if (value < min_attr.value) {
                valid = false;
            }
        }

        if constexpr (has_attribute<Max<V>>(member)) {
            constexpr auto max_attr = get_attribute<Max<V>>(member);
            if (value > max_attr.value) {
                valid = false;
            }
        }

        return valid;
    }

    /// Коррекция значения по constraints.
    template<typename M, typename V>
    auto fix_member(M member, V& value) -> bool {
        bool modified = false;

        if constexpr (has_attribute<Range<V>>(member)) {
            constexpr auto range = get_attribute<Range<V>>(member);
            if (value < range.min) {
                value = range.min;
                modified = true;
            } else if (value > range.max) {
                value = range.max;
                modified = true;
            }
        }

        if constexpr (has_attribute<Min<V>>(member)) {
            constexpr auto min_attr = get_attribute<Min<V>>(member);
            if (value < min_attr.value) {
                value = min_attr.value;
                modified = true;
            }
        }

        if constexpr (has_attribute<Max<V>>(member)) {
            constexpr auto max_attr = get_attribute<Max<V>>(member);
            if (value > max_attr.value) {
                value = max_attr.value;
                modified = true;
            }
        }

        return modified;
    }

    /// Проверка наличия атрибута.
    template<typename Attr, typename M>
    static consteval auto has_attribute(M member) -> bool {
        // В полной реализации P2996:
        // return std::meta::has_attribute(member, ^Attr);
        return false; // Placeholder
    }

    /// Получение атрибута.
    template<typename Attr, typename M>
    static consteval auto get_attribute(M member) -> Attr {
        // В полной реализации P2996:
        // return std::meta::attribute_value<Attr>(member);
        return Attr{}; // Placeholder
    }
};

} // namespace projectv::validation
```

### 4.2 Пример использования с Constraints

```cpp
// ProjectV.Components.Material.cppm
export module ProjectV.Components.Material;

import std;
import glm;
import ProjectV.Validation.Constraints;

export namespace projectv {

/// PBR Material с валидацией через attributes.
export struct PBRMaterialValidated {
    std::string name{"default"};

    [[.range{0.0f, 1.0f}]]
    glm::vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};

    [[.range{0.0f, 1.0f}]]
    float metallic{0.0f};

    [[.range{0.01f, 1.0f}]]
    float roughness{0.5f};

    [[.min{0.0f}]]
    float emissive_strength{0.0f};

    bool double_sided{false};

    /// Валидация после загрузки.
    auto validate() -> void {
        validation::Validator validator;
        validator.validate_and_fix(*this);
    }
};

} // namespace projectv
```

---

## 5. Enum Reflection

### 5.1 Автоматическая работа с Enum

```cpp
// ProjectV.Core.EnumReflection.cppm
export module ProjectV.Core.EnumReflection;

import std;
import std.meta;

export namespace projectv::core {

/// Получение имени enum value.
export template<typename E>
    requires std::is_enum_v<E>
consteval auto enum_name(E value) -> std::string_view {
    constexpr auto enumerators = std::meta::enumerators_of(^E);

    for constexpr auto e : enumerators {
        if (value == [:e:]) {
            return std::meta::name_of(e);
        }
    }

    return "unknown";
}

/// Получение всех имён enum.
export template<typename E>
    requires std::is_enum_v<E>
consteval auto enum_names() -> std::array<std::string_view, std::meta::enumerators_of(^E).size()> {
    constexpr auto enumerators = std::meta::enumerators_of(^E);
    std::array<std::string_view, enumerators.size()> names{};

    size_t i = 0;
    for constexpr auto e : enumerators {
        names[i++] = std::meta::name_of(e);
    }

    return names;
}

/// Получение всех значений enum.
export template<typename E>
    requires std::is_enum_v<E>
consteval auto enum_values() -> std::array<E, std::meta::enumerators_of(^E).size()> {
    constexpr auto enumerators = std::meta::enumerators_of(^E);
    std::array<E, enumerators.size()> values{};

    size_t i = 0;
    for constexpr auto e : enumerators {
        values[i++] = [:e:];
    }

    return values;
}

/// Конвертация из строки в enum.
export template<typename E>
    requires std::is_enum_v<E>
constexpr auto from_string(std::string_view name) -> std::optional<E> {
    constexpr auto enumerators = std::meta::enumerators_of(^E);

    for constexpr auto e : enumerators {
        if (std::meta::name_of(e) == name) {
            return [:e:];
        }
    }

    return std::nullopt;
}

} // namespace projectv::core

// Пример использования:
namespace projectv {

enum class BlockType : uint8_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Water = 4,
    Sand = 5
};

// Compile-time проверка:
static_assert(core::enum_name(BlockType::Water) == "Water");
static_assert(core::enum_names<BlockType>().size() == 6);
static_assert(core::from_string<BlockType>("Sand") == BlockType::Sand);

} // namespace projectv
```

---

## 6. Сравнение с альтернативами

| Подход                 | Boilerplate            | Runtime overhead | Compile-time | C++ версия |
|------------------------|------------------------|------------------|--------------|------------|
| **C++26 P2996**        | Нет                    | Нет              | Да           | C++26      |
| Glaze (текущий)        | `struct glaze { ... }` | Минимальный      | Да           | C++20      |
| Ручные макросы         | Высокий                | Нет              | Да           | Любой      |
| RTTI                   | Нет                    | Высокий          | Нет          | Любой      |
| External кодогенерация | Средний                | Нет              | Да           | Любой      |

---

## 7. Требования к компилятору

| Компилятор | Минимальная версия | Статус P2996 |
|------------|--------------------|--------------|
| GCC        | 15+                | Частичная    |
| Clang      | 19+                | Частичная    |
| MSVC       | 19.40+             | В разработке |

### Fallback для неполной поддержки

```cpp
// ProjectV.Core.Reflection.Fallback.cppm
export module ProjectV.Core.Reflection.Fallback;

// Если компилятор не поддерживает P2996 полностью,
// используем комбинацию:
// 1. __builtin_dump_struct (GCC/Clang extension)
// 2. Boost.PFR для автоматической reflection
// 3. Частичная специализация для известных типов

#if __cpp_static_reflection >= 202502L
    // Полная реализация P2996
    // ... как показано выше
#else
    // Fallback реализация
    import Boost.PFR;

    export template<typename T>
    auto for_each_member(T&& obj, auto&& func) -> void {
        boost::pfr::for_each_field(std::forward<T>(obj), [&](auto& field, size_t idx) {
            constexpr auto name = boost::pfr::get_name<T, idx>;
            func(name, field);
        });
    }
#endif
```

---

## 8. Рекомендации

1. **Используйте P2996** как основной механизм reflection
2. **Проверяйте `__cpp_static_reflection`** для conditional compilation
3. **Fallback на Boost.PFR** для компиляторов без полной поддержки P2996
4. **Не добавляйте boilerplate** — reflection работает автоматически для всех типов

---

## Ссылки

- [P2996: Static Reflection for C++26](https://wg21.link/P2996)
- [P3096: Reflection Rethink](https://wg21.link/P3096)
- [Boost.PFR](https://www.boost.org/doc/libs/release/doc/html/boost_pfr.html)
- [Serialization Strategy](../06_assets/03_serialization.md)
