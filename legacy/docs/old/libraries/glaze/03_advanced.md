# Glaze — Advanced

Продвинутые фичи Glaze, оптимизация производительности и интеграция с воксельным движком.

---

## Бинарная сериализация для сетевого протокола

### Почему бинарный формат для ProjectV

| Формат     | Размер (пример) | Скорость | Использование               |
|------------|-----------------|----------|-----------------------------|
| JSON       | 100KB           | 5ms      | Конфиги, метаданные         |
| **Binary** | **20KB**        | **1ms**  | **Сетевой протокол, сейвы** |

### Сериализация воксельного чанка

```cpp
#include <glaze/glaze.hpp>
#include <array>
#include <print>

struct VoxelChunk {
    glm::ivec3 coord;
    std::array<uint8_t, 4096> voxels;  // 16³ вокселей
    std::array<uint8_t, 1024> metadata; // Дополнительные данные

    struct glaze {
        using T = VoxelChunk;
        static constexpr auto value = glz::object(
            "coord", &T::coord,
            "voxels", &T::voxels,
            "metadata", &T::metadata
        );
    };
};

// Компрессия чанка для сети/сохранения
std::vector<uint8_t> compress_chunk(const VoxelChunk& chunk) {
    std::vector<uint8_t> buffer;

    // Бинарная сериализация (в 5 раз компактнее JSON)
    glz::write_binary(chunk, buffer);

    std::print("Chunk compressed: {} -> {} bytes ({}%)\n",
               sizeof(chunk), buffer.size(),
               (buffer.size() * 100) / sizeof(chunk));

    return buffer;
}

// Декомпрессия
std::expected<VoxelChunk, std::string> decompress_chunk(std::span<const uint8_t> data) {
    VoxelChunk chunk;
    auto error = glz::read_binary(chunk, data);

    if (error) {
        return std::unexpected(glz::format_error(error, "chunk data"));
    }

    return chunk;
}
```

### Сетевой пакет с бинарными данными

```cpp
#include <glaze/glaze.hpp>
#include <variant>
#include <print>

enum class PacketType : uint8_t {
    ChunkUpdate,
    PlayerMove,
    EntitySpawn,
    ChatMessage
};

struct ChunkUpdatePacket {
    glm::ivec3 coord;
    std::vector<uint8_t> compressed_data;
    uint64_t timestamp;

    struct glaze {
        using T = ChunkUpdatePacket;
        static constexpr auto value = glz::object(
            "coord", &T::coord,
            "compressed_data", &T::compressed_data,
            "timestamp", &T::timestamp
        );
    };
};

struct NetworkPacket {
    PacketType type;
    uint32_t sequence;
    std::variant<ChunkUpdatePacket, /* другие типы... */> data;

    struct glaze {
        using T = NetworkPacket;
        static constexpr auto value = glz::object(
            "type", &T::type,
            "sequence", &T::sequence,
            "data", &T::data
        );
    };
};

// Отправка пакета
std::vector<uint8_t> serialize_packet(const NetworkPacket& packet) {
    std::vector<uint8_t> buffer;
    glz::write_binary(packet, buffer);

    // Добавляем заголовок (тип + размер)
    std::vector<uint8_t> framed;
    framed.push_back(static_cast<uint8_t>(packet.type));

    uint32_t size = static_cast<uint32_t>(buffer.size());
    auto* size_bytes = reinterpret_cast<const uint8_t*>(&size);
    framed.insert(framed.end(), size_bytes, size_bytes + 4);

    framed.insert(framed.end(), buffer.begin(), buffer.end());

    return framed;
}
```

---

## Кастомные адаптеры для типов ProjectV

### Адаптер для glm::mat4 (column-major)

```cpp
#include <glm/glm.hpp>
#include <glaze/glaze.hpp>
#include <array>

template<>
struct glz::meta<glm::mat4> {
    using T = glm::mat4;

    static constexpr auto value = [] {
        std::array<float, 16> arr{};
        return glz::object(
            "m00", glz::detail::make_ptr<0>(arr),
            "m01", glz::detail::make_ptr<1>(arr),
            "m02", glz::detail::make_ptr<2>(arr),
            "m03", glz::detail::make_ptr<3>(arr),
            "m10", glz::detail::make_ptr<4>(arr),
            "m11", glz::detail::make_ptr<5>(arr),
            "m12", glz::detail::make_ptr<6>(arr),
            "m13", glz::detail::make_ptr<7>(arr),
            "m20", glz::detail::make_ptr<8>(arr),
            "m21", glz::detail::make_ptr<9>(arr),
            "m22", glz::detail::make_ptr<10>(arr),
            "m23", glz::detail::make_ptr<11>(arr),
            "m30", glz::detail::make_ptr<12>(arr),
            "m31", glz::detail::make_ptr<13>(arr),
            "m32", glz::detail::make_ptr<14>(arr),
            "m33", glz::detail::make_ptr<15>(arr)
        );
    }();

    static void to_json(const glm::mat4& mat, auto& ctx) {
        std::array<float, 16> arr;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                arr[col * 4 + row] = mat[col][row];
            }
        }
        glz::write<value>(arr, ctx);
    }

    static void from_json(glm::mat4& mat, auto& ctx) {
        std::array<float, 16> arr;
        glz::read<value>(arr, ctx);

        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                mat[col][row] = arr[col * 4 + row];
            }
        }
    }
};
```

### Адаптер для std::chrono::time_point

```cpp
#include <chrono>
#include <glaze/glaze.hpp>

template<>
struct glz::meta<std::chrono::system_clock::time_point> {
    using T = std::chrono::system_clock::time_point;

    static constexpr auto value = glz::number;

    static void to_json(const T& tp, auto& ctx) {
        auto duration = tp.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        glz::write<value>(millis, ctx);
    }

    static void from_json(T& tp, auto& ctx) {
        int64_t millis;
        glz::read<value>(millis, ctx);
        tp = T(std::chrono::milliseconds(millis));
    }
};
```

### Адаптер для enum flags (битовые поля)

```cpp
#include <glaze/glaze.hpp>
#include <print>

enum class VoxelFlags : uint32_t {
    None = 0,
    Transparent = 1 << 0,
    Emissive = 1 << 1,
    Flammable = 1 << 2,
    Conducting = 1 << 3,
    Magnetic = 1 << 4
};

template<>
struct glz::meta<VoxelFlags> {
    using T = VoxelFlags;

    static constexpr auto value = glz::number;

    static void to_json(const T& flags, auto& ctx) {
        uint32_t value = static_cast<uint32_t>(flags);
        glz::write<value>(value, ctx);
    }

    static void from_json(T& flags, auto& ctx) {
        uint32_t value;
        glz::read<value>(value, ctx);
        flags = static_cast<T>(value);
    }
};

// Использование
struct VoxelType {
    std::string name;
    VoxelFlags flags = VoxelFlags::None;

    struct glaze {
        using T = VoxelType;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "flags", &T::flags
        );
    };
};
```

---

## Интеграция с ECS (flecs)

### Сериализация компонентов

```cpp
#include <flecs.h>
#include <glaze/glaze.hpp>
#include <print>

// Компонент для сериализации
struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    struct glaze {
        using T = Transform;
        static constexpr auto value = glz::object(
            "position", &T::position,
            "rotation", &T::rotation,
            "scale", &T::scale
        );
    };
};

// Компонент с динамическими данными
struct Inventory {
    std::vector<std::string> items;
    std::unordered_map<std::string, uint32_t> quantities;

    struct glaze {
        using T = Inventory;
        static constexpr auto value = glz::object(
            "items", &T::items,
            "quantities", &T::quantities
        );
    };
};

// Сериализация сущности
std::vector<uint8_t> serialize_entity(flecs::entity entity) {
    struct EntityData {
        std::string name;
        Transform transform;
        std::optional<Inventory> inventory;
        // Другие компоненты...

        struct glaze {
            using T = EntityData;
            static constexpr auto value = glz::object(
                "name", &T::name,
                "transform", &T::transform,
                "inventory", &T::inventory
            );
        };
    };

    EntityData data;

    if (entity.has<flecs::Name>()) {
        data.name = entity.get<flecs::Name>()->value;
    }

    if (auto* transform = entity.get<Transform>()) {
        data.transform = *transform;
    }

    if (auto* inventory = entity.get<Inventory>()) {
        data.inventory = *inventory;
    }

    std::vector<uint8_t> buffer;
    glz::write_binary(data, buffer);

    return buffer;
}

// Десериализация сущности
flecs::entity deserialize_entity(flecs::world& world, std::span<const uint8_t> data) {
    EntityData entity_data;
    auto error = glz::read_binary(entity_data, data);

    if (error) {
        std::print(stderr, "Failed to deserialize entity: {}\n",
                   glz::format_error(error, "entity data"));
        return flecs::entity::null();
    }

    auto entity = world.entity(entity_data.name.c_str());
    entity.set(entity_data.transform);

    if (entity_data.inventory) {
        entity.set(*entity_data.inventory);
    }

    return entity;
}
```

### Сохранение/загрузка мира

```cpp
#include <glaze/glaze.hpp>
#include <fstream>
#include <print>

struct WorldSave {
    struct EntityRecord {
        uint64_t id;
        std::vector<uint8_t> data;

        struct glaze {
            using T = EntityRecord;
            static constexpr auto value = glz::object(
                "id", &T::id,
                "data", &T::data
            );
        };
    };

    std::string version = "1.0.0";
    std::chrono::system_clock::time_point save_time;
    std::vector<EntityRecord> entities;
    std::unordered_map<std::string, std::vector<uint8_t>> chunks;

    struct glaze {
        using T = WorldSave;
        static constexpr auto value = glz::object(
            "version", &T::version,
            "save_time", &T::save_time,
            "entities", &T::entities,
            "chunks", &T::chunks
        );
    };
};

bool save_world(flecs::world& world, const std::filesystem::path& path) {
    WorldSave save;
    save.save_time = std::chrono::system_clock::now();

    // Сериализуем все сущности
    world.each([&](flecs::entity entity) {
        if (entity.has<Transform>()) {  // Только "сохраняемые" сущности
            auto data = serialize_entity(entity);
            save.entities.push_back({
                .id = entity.id(),
                .data = std::move(data)
            });
        }
    });

    // Сериализуем в бинарный формат
    std::vector<uint8_t> buffer;
    glz::write_binary(save, buffer);

    // Сохраняем в файл
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::print(stderr, "Failed to open save file: {}\n", path.string());
        return false;
    }

    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    std::print("World saved: {} entities, {} bytes\n",
               save.entities.size(), buffer.size());

    return true;
}
```

---

## Оптимизация производительности

### Precompiled Headers для Glaze

```cmake
# CMakeLists.txt
target_precompile_headers(ProjectV PRIVATE
  <glaze/glaze.hpp>
  <glaze/core.hpp>
)
```

### Zero-copy для больших данных

```cpp
#include <glaze/glaze.hpp>
#include <memory>

// Memory-mapped файл для конфигов
class MappedConfigFile {
    std::unique_ptr<std::byte[]> data_;
    size_t size_;

public:
    std::expected<std::string_view, std::string> load(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected("Failed to open file");
        }

        size_ = file.tellg();
        file.seekg(0);

        data_ = std::make_unique<std::byte[]>(size_);
        file.read(reinterpret_cast<char*>(data_.get()), size_);

        return std::string_view(reinterpret_cast<const char*>(data_.get()), size_);
    }

    template<typename T>
    std::expected<T, std::string> parse() {
        T result;
        auto error = glz::read_json(result, std::string_view(
            reinterpret_cast<const char*>(data_.get()), size_));

        if (error) {
            return std::unexpected(glz::format_error(error, "config file"));
        }

        return result;
    }
};

// Использование
auto mapped = std::make_unique<MappedConfigFile>();
if (auto view = mapped->load("config/engine.json")) {
    if (auto config = mapped->parse<EngineConfig>()) {
        // Zero-copy парсинг завершён
    }
}
```

### Thread-local парсеры

```cpp
#include <glaze/glaze.hpp>
#include <thread>
#include <vector>

class ThreadSafeParser {
    // Каждый поток имеет свой парсер (glz::context не thread-safe)
    static thread_local glz::context ctx_;

public:
    template<typename T>
    std::expected<T, std::string> parse(std::string_view json) {
        T result;
        auto error = glz::read_json(result, json, ctx_);

        if (error) {
            return std::unexpected(glz::format_error(error, json));
        }

        return result;
    }
};

// Параллельная загрузка конфигов на основе stdexec
#include <stdexec/execution.hpp>

stdexec::sender auto load_configs_parallel_stdexec(const std::vector<std::string>& config_files) {
    return stdexec::schedule(stdexec::get_default_scheduler())
         | stdexec::bulk(static_cast<int>(config_files.size()),
               [&config_files](int idx) -> std::expected<EngineConfig, std::string> {
                   ThreadSafeParser parser;
                   return parser.parse<EngineConfig>(config_files[idx]);
               });
}

// Пример использования с stdexec
void example_usage(const std::vector<std::string>& config_files) {
    auto load_task = load_configs_parallel_stdexec(config_files)
                   | stdexec::then([](std::vector<std::expected<EngineConfig, std::string>> results) {
                         std::vector<EngineConfig> valid_configs;
                         for (auto& result : results) {
                             if (result) {
                                 valid_configs.push_back(std::move(*result));
                             }
                         }
                         return valid_configs;
                     });

    // Запускаем асинхронно
    stdexec::sync_wait(std::move(load_task));
}
```

### Оптимизация под воксельные данные

```cpp
#include <glaze/glaze.hpp>
#include <span>
#include <print>

// Специализация для массивов вокселей
template<>
struct glz::meta<std::span<const uint8_t>> {
    using T = std::span<const uint8_t>;

    static constexpr auto value = glz::array<uint8_t>;

    static void to_json(const T& span, auto& ctx) {
        ctx.writer.StartArray();
        for (uint8_t byte : span) {
            glz::write<value>(byte, ctx);
        }
        ctx.writer.EndArray();
    }

    static void from_json(T& span, auto& ctx) {
        // Для std::span мы не можем изменить размер, поэтому
        // эта специализация только для чтения
        // В реальном коде нужно использовать std::vector<uint8_t>
        // или другой контейнер с изменяемым размером
        static_assert(false, "std::span<const uint8_t> is read-only for Glaze");
    }
};

// Альтернатива: специализация для std::vector<uint8_t>
template<>
struct glz::meta<std::vector<uint8_t>> {
    using T = std::vector<uint8_t>;

    static constexpr auto value = glz::array<uint8_t>;

    static void to_json(const T& vec, auto& ctx) {
        ctx.writer.StartArray();
        for (uint8_t byte : vec) {
            glz::write<value>(byte, ctx);
        }
        ctx.writer.EndArray();
    }

    static void from_json(T& vec, auto& ctx) {
        vec.clear();

        auto* arr = ctx.value.GetArray();
        vec.reserve(arr.Size());

        for (const auto& element : *arr) {
            uint8_t byte = static_cast<uint8_t>(element.GetUint());
            vec.push_back(byte);
        }
    }
};
```

### Оптимизация для воксельных данных: битовые массивы

```cpp
#include <glaze/glaze.hpp>
#include <bitset>
#include <print>

// Битовая маска для 4096 вокселей (512 байт вместо 4096)
class VoxelBitmask {
    std::bitset<4096> bits;

public:
    bool get(size_t index) const { return bits[index]; }
    void set(size_t index, bool value) { bits[index] = value; }

    struct glaze {
        using T = VoxelBitmask;
        static constexpr auto value = glz::object(
            "bits", &T::bits
        );
    };
};

// Специализация для std::bitset
template<size_t N>
struct glz::meta<std::bitset<N>> {
    using T = std::bitset<N>;

    static constexpr auto value = glz::string;

    static void to_json(const T& bitset, auto& ctx) {
        std::string str = bitset.to_string();
        glz::write<value>(str, ctx);
    }

    static void from_json(T& bitset, auto& ctx) {
        std::string str;
        glz::read<value>(str, ctx);
        bitset = T(str);
    }
};

// Использование для sparse воксельных данных
struct SparseVoxelChunk {
    glm::ivec3 coord;
    VoxelBitmask occupancy;  // Какие воксели заполнены
    std::vector<uint8_t> voxels;  // Только заполненные воксели

    struct glaze {
        using T = SparseVoxelChunk;
        static constexpr auto value = glz::object(
            "coord", &T::coord,
            "occupancy", &T::occupancy,
            "voxels", &T::voxels
        );
    };
};
```

---

## Производительность: бенчмарки и метрики

### Сравнение форматов для ProjectV

```cpp
#include <glaze/glaze.hpp>
#include <nlohmann/json.hpp>
#include <print>
#include <chrono>

struct BenchmarkResult {
    std::string library;
    size_t size_bytes;
    std::chrono::microseconds serialize_time;
    std::chrono::microseconds deserialize_time;
    double compression_ratio;

    struct glaze {
        using T = BenchmarkResult;
        static constexpr auto value = glz::object(
            "library", &T::library,
            "size_bytes", &T::size_bytes,
            "serialize_time", &T::serialize_time,
            "deserialize_time", &T::deserialize_time,
            "compression_ratio", &T::compression_ratio
        );
    };
};

void run_benchmarks() {
    EngineConfig config = create_test_config();
    std::vector<BenchmarkResult> results;

    // Glaze JSON
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::string json = glz::write_json(config);
        auto serialize_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start);

        start = std::chrono::high_resolution_clock::now();
        EngineConfig parsed;
        glz::read_json(parsed, json);
        auto deserialize_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start);

        results.push_back({
            .library = "Glaze JSON",
            .size_bytes = json.size(),
            .serialize_time = serialize_time,
            .deserialize_time = deserialize_time,
            .compression_ratio = 1.0
        });
    }

    // Glaze Binary
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> binary;
        glz::write_binary(config, binary);
        auto serialize_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start);

        start = std::chrono::high_resolution_clock::now();
        EngineConfig parsed;
        glz::read_binary(parsed, binary);
        auto deserialize_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start);

        results.push_back({
            .library = "Glaze Binary",
            .size_bytes = binary.size(),
            .serialize_time = serialize_time,
            .deserialize_time = deserialize_time,
            .compression_ratio = static_cast<double>(binary.size()) /
                                glz::write_json(config).size()
        });
    }

    // nlohmann/json (для сравнения)
    {
        auto start = std::chrono::high_resolution_clock::now();
        nlohmann::json json = nlohmann::json::parse(glz::write_json(config));
        auto serialize_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start);

        start = std::chrono::high_resolution_clock::now();
        // Парсинг обратно...
        auto deserialize_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start);

        results.push_back({
            .library = "nlohmann/json",
            .size_bytes = json.dump().size(),
            .serialize_time = serialize_time,
            .deserialize_time = deserialize_time,
            .compression_ratio = 1.0
        });
    }

    // Сохраняем результаты
    std::string results_json = glz::write_json(results);
    std::print("Benchmark results:\n{}\n", results_json);
}
```

### Рекомендации по производительности

1. **Для конфигов**: Glaze JSON + precompiled headers
2. **Для сетевого протокола**: Glaze Binary + Zstd сжатие
3. **Для сохранения мира**: Glaze Binary + sparse воксельные данные
4. **Для hot reload**: memory-mapped файлы + zero-copy парсинг
5. **Для многопоточности**: thread-local парсеры

---

## Заключение

Glaze предоставляет для ProjectV:

1. **Zero-copy парсинг** для быстрой загрузки конфигов
2. **Бинарную сериализацию** для сетевого протокола и сейвов
3. **Кастомные адаптеры** для типов ProjectV (glm, chrono, enum flags)
4. **Интеграцию с ECS** для сериализации сущностей
5. **Оптимизации производительности**: precompiled headers, thread-local парсеры

Ключевые преимущества:

- **Compile-time reflection**: проверка типов во время компиляции
- **Data-oriented design**: cache-friendly структуры
- **Modern C++23/26**: std::expected, std::print, std::span
- **Интеграция с ProjectV**: воксельные данные, ECS, hot reload

Glaze — это не просто библиотека JSON, это инфраструктура для сериализации в ProjectV, оптимизированная под требования
воксельного движка.
