# Zstd — Reference

<!-- anchor: 01_reference -->

Zstandard (Zstd) — современный алгоритм сжатия с фокусом на баланс скорости и степени сжатия, идеально подходящий для воксельного движка.

---

## Почему Zstd для ProjectV

### Сравнение алгоритмов сжатия

| Алгоритм | Скорость сжатия | Скорость декомпрессии | Степень сжатия | Использование в ProjectV |
|----------|-----------------|----------------------|----------------|--------------------------|
| **Zstd** | **500 MB/s** | **2000 MB/s** | **2.8:1** | **Воксельные чанки, сейвы, сетевой протокол** |
| LZ4 | 800 MB/s | 4000 MB/s | 2.1:1 | Быстрая загрузка текстур |
| zlib | 100 MB/s | 400 MB/s | 2.5:1 | Устаревший формат |
| LZMA | 10 MB/s | 100 MB/s | 3.5:1 | Архивация релизов |

### Ключевые преимущества Zstd

1. **Предиктивный словарь (Dictionary Compression)**
   - Обучение на типичных данных ProjectV (воксельные чанки)
   - Улучшение сжатия на 10-50% для повторяющихся паттернов

2. **Многопоточное сжатие**
   - Линейное масштабирование до 256 потоков
   - Идеально для параллельной обработки чанков

3. **Настраиваемые уровни сжатия (1-22)**
   - Уровень 1: максимальная скорость (почти как LZ4)
   - Уровень 22: максимальное сжатие (почти как LZMA)

4. **Long Distance Matching**
   - Эффективное сжатие больших воксельных миров
   - Обнаружение повторений на расстоянии до 128MB

---

## Архитектура Zstd

### Блочная структура

```
┌─────────────────────────────────────────────────────────────┐
│                         Zstd Frame                          │
├──────────┬──────────┬──────────────┬──────────┬────────────┤
│  Header  │  Block 1 │  Block 2 ... │  Block N │  Checksum  │
│  4 bytes │ variable │   variable   │ variable │  4 bytes   │
└──────────┴──────────┴──────────────┴──────────┴────────────┘
```

### Этапы сжатия

1. **Предикция (Prediction)**
   - Finite State Entropy (FSE) для символов
   - Huffman coding для литералов

2. **Сопоставление (Matching)**
   - Hash chain для поиска повторений
   - Optimal parser для выбора лучших матчей

3. **Кодирование (Encoding)**
   - tANS (tabled Asymmetric Numeral Systems)
   - Битовая упаковка для эффективного хранения

### Memory Layout (Data-Oriented Design)

```cpp
struct ZstdBlock {
    uint32_t compressed_size;
    uint32_t original_size;
    uint8_t* data;  // Сжатые данные

    // Cache-friendly структура
    alignas(64) uint8_t buffer[ZSTD_BLOCKSIZE_MAX];
};

struct ZstdFrame {
    std::vector<ZstdBlock> blocks;
    ZSTD_CDict* dictionary;  // Общий словарь для всех блоков
    uint64_t total_compressed_size;
    uint64_t total_original_size;

    // Статистика для мониторинга
    struct {
        double compression_ratio;
        double compression_speed;  // MB/s
        double decompression_speed; // MB/s
    } stats;
};
```

---

## Dictionary Compression для воксельных данных

### Обучение словаря на данных ProjectV

```cpp
#include <zstd.h>
#include <vector>
#include <span>
#include <print>
#include <filesystem>

class VoxelDictionary {
    ZSTD_CDict* compression_dict_ = nullptr;
    ZSTD_DDict* decompression_dict_ = nullptr;
    std::vector<uint8_t> dictionary_data_;

public:
    struct TrainingSamples {
        std::vector<std::span<const uint8_t>> samples;
        size_t total_size = 0;

        void add_voxel_chunk(std::span<const uint8_t> chunk) {
            samples.push_back(chunk);
            total_size += chunk.size();
        }
    };

    // Обучение словаря на типичных воксельных данных
    std::expected<void, std::string> train(const TrainingSamples& samples,
                                          size_t dict_size = 100 * 1024) {
        if (samples.samples.empty()) {
            return std::unexpected("No training samples provided");
        }

        // Подготовка данных для обучения
        std::vector<size_t> sample_sizes;
        std::vector<const void*> sample_ptrs;

        for (const auto& sample : samples.samples) {
            sample_sizes.push_back(sample.size());
            sample_ptrs.push_back(sample.data());
        }

        // Выделение памяти для словаря
        dictionary_data_.resize(dict_size);

        // Обучение словаря
        size_t actual_size = ZDICT_trainFromBuffer(
            dictionary_data_.data(), dict_size,
            sample_ptrs.data(), sample_sizes.data(),
            static_cast<unsigned>(samples.samples.size())
        );

        if (ZDICT_isError(actual_size)) {
            return std::unexpected(ZDICT_getErrorName(actual_size));
        }

        dictionary_data_.resize(actual_size);

        // Создание compression dictionary
        compression_dict_ = ZSTD_createCDict(
            dictionary_data_.data(), dictionary_data_.size(),
            3  // Уровень сжатия для словаря
        );

        if (!compression_dict_) {
            return std::unexpected("Failed to create compression dictionary");
        }

        // Создание decompression dictionary
        decompression_dict_ = ZSTD_createDDict(
            dictionary_data_.data(), dictionary_data_.size()
        );

        if (!decompression_dict_) {
            return std::unexpected("Failed to create decompression dictionary");
        }

        std::print("Dictionary trained: {} samples, {} -> {} bytes ({}%)\n",
                   samples.samples.size(), samples.total_size, actual_size,
                   (actual_size * 100) / samples.total_size);

        return {};
    }

    // Сохранение словаря в файл
    bool save(const std::filesystem::path& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            std::print(stderr, "Failed to open dictionary file: {}\n", path.string());
            return false;
        }

        // Заголовок словаря
        struct DictionaryHeader {
            char magic[4] = {'Z', 'D', 'I', 'C'};
            uint32_t version = 1;
            uint64_t data_size;
            uint64_t sample_count;
            uint64_t total_training_size;
        } header;

        header.data_size = dictionary_data_.size();
        header.sample_count = 0;  // Будет заполнено при загрузке
        header.total_training_size = 0;

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(dictionary_data_.data()),
                  dictionary_data_.size());

        std::print("Dictionary saved: {} bytes\n", dictionary_data_.size());
        return true;
    }

    // Загрузка словаря из файла
    std::expected<void, std::string> load(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected("Failed to open dictionary file");
        }

        size_t file_size = file.tellg();
        file.seekg(0);

        if (file_size < sizeof(DictionaryHeader)) {
            return std::unexpected("Dictionary file too small");
        }

        DictionaryHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        // Проверка magic
        if (std::string(header.magic, 4) != "ZDIC") {
            return std::unexpected("Invalid dictionary format");
        }

        dictionary_data_.resize(header.data_size);
        file.read(reinterpret_cast<char*>(dictionary_data_.data()),
                 header.data_size);

        // Создание dictionaries
        compression_dict_ = ZSTD_createCDict(
            dictionary_data_.data(), dictionary_data_.size(), 3);

        decompression_dict_ = ZSTD_createDDict(
            dictionary_data_.data(), dictionary_data_.size());

        if (!compression_dict_ || !decompression_dict_) {
            return std::unexpected("Failed to create dictionaries from file");
        }

        std::print("Dictionary loaded: {} bytes\n", dictionary_data_.size());
        return {};
    }

    ZSTD_CDict* compression_dict() const { return compression_dict_; }
    ZSTD_DDict* decompression_dict() const { return decompression_dict_; }
    std::span<const uint8_t> data() const { return dictionary_data_; }

    ~VoxelDictionary() {
        if (compression_dict_) ZSTD_freeCDict(compression_dict_);
        if (decompression_dict_) ZSTD_freeDDict(decompression_dict_);
    }
};
```

### Типичные паттерны воксельных данных для словаря

```cpp
// Сбор образцов для обучения словаря
VoxelDictionary::TrainingSamples collect_training_samples() {
    VoxelDictionary::TrainingSamples samples;

    // 1. Пустые чанки (воздух)
    std::array<uint8_t, 4096> empty_chunk{};
    samples.add_voxel_chunk(empty_chunk);

    // 2. Полные чанки (камень)
    std::array<uint8_t, 4096> solid_chunk;
    std::fill(solid_chunk.begin(), solid_chunk.end(), 1);  // Камень
    samples.add_voxel_chunk(solid_chunk);

    // 3. Чанки с поверхностью (земля сверху, воздух снизу)
    std::array<uint8_t, 4096> surface_chunk;
    for (int z = 0; z < 16; ++z) {
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                int index = x + y * 16 + z * 256;
                surface_chunk[index] = (y < 8) ? 1 : 0;  // Земля ниже 8 уровня
            }
        }
    }
    samples.add_voxel_chunk(surface_chunk);

    // 4. Чанки с пещерами
    std::array<uint8_t, 4096> cave_chunk;
    // ... генерация процедурных пещер
    samples.add_voxel_chunk(cave_chunk);

    // 5. Чанки с структурами (деревья, здания)
    std::array<uint8_t, 4096> structure_chunk;
    // ... генерация структур
    samples.add_voxel_chunk(structure_chunk);

    return samples;
}
```

---

## Производительность: бенчмарки и метрики

### Сравнение уровней сжатия для воксельных данных

```cpp
#include <zstd.h>
#include <chrono>
#include <print>
#include <vector>

struct CompressionBenchmark {
    struct Result {
        int level;
        size_t compressed_size;
        std::chrono::microseconds compression_time;
        std::chrono::microseconds decompression_time;
        double compression_ratio;
        double compression_speed;    // MB/s
        double decompression_speed;  // MB/s

        struct glaze {
            using T = Result;
            static constexpr auto value = glz::object(
                "level", &T::level,
                "compressed_size", &T::compressed_size,
                "compression_time", &T::compression_time,
                "decompression_time", &T::decompression_time,
                "compression_ratio", &T::compression_ratio,
                "compression_speed", &T::compression_speed,
                "decompression_speed", &T::decompression_speed
            );
        };
    };

    static std::vector<Result> run_benchmark(std::span<const uint8_t> data,
                                            const VoxelDictionary& dict) {
        std::vector<Result> results;

        // Тестируем уровни сжатия от 1 до 22
        for (int level = 1; level <= 22; ++level) {
            // Пропускаем медленные уровни для быстрых тестов
            if (level > 10 && data.size() < 10 * 1024 * 1024) {
                continue;
            }

            Result result;
            result.level = level;

            // Сжатие
            auto compress_start = std::chrono::high_resolution_clock::now();

            size_t bound = ZSTD_compressBound(data.size());
            std::vector<uint8_t> compressed(bound);

            size_t compressed_size = ZSTD_compress_usingCDict(
                compressed.data(), bound,
                data.data(), data.size(),
                dict.compression_dict()
            );

            auto compress_end = std::chrono::high_resolution_clock::now();

            if (ZSTD_isError(compressed_size)) {
                std::print(stderr, "Compression failed at level {}: {}\n",
                           level, ZSTD_getErrorName(compressed_size));
                continue;
            }

            compressed.resize(compressed_size);

            // Декомпрессия
            auto decompress_start = std::chrono::high_resolution_clock::now();

            std::vector<uint8_t> decompressed(data.size());
            size_t decompressed_size = ZSTD_decompress_usingDDict(
                decompressed.data(), data.size(),
                compressed.data(), compressed_size,
                dict.decompression_dict()
            );

            auto decompress_end = std::chrono::high_resolution_clock::now();

            if (ZSTD_isError(decompressed_size) || decompressed_size != data.size()) {
                std::print(stderr, "Decompression failed at level {}\n", level);
                continue;
            }

            // Проверка целостности
            if (memcmp(data.data(), decompressed.data(), data.size()) != 0) {
                std::print(stderr, "Data corruption at level {}\n", level);
                continue;
            }

            // Заполнение результатов
            result.compressed_size = compressed_size;
            result.compression_time = std::chrono::duration_cast<std::chrono::microseconds>(
                compress_end - compress_start);
            result.decompression_time = std::chrono::duration_cast<std::chrono::microseconds>(
                decompress_end - decompress_start);
            result.compression_ratio = static_cast<double>(data.size()) / compressed_size;

            // Расчет скоростей (MB/s)
            double data_mb = data.size() / (1024.0 * 1024.0);
            double compress_seconds = result.compression_time.count() / 1'000'000.0;
            double decompress_seconds = result.decompression_time.count() / 1'000'000.0;

            result.compression_speed = data_mb / compress_seconds;
            result.decompression_speed = data_mb / decompress_seconds;

            results.push_back(result);

            std::print("Level {}: {} -> {} bytes ({:.2f}x) "
                       "compress: {:.1f} MB/s, decompress: {:.1f} MB/s\n",
                       level, data.size(), compressed_size, result.compression_ratio,
                       result.compression_speed, result.decompression_speed);
        }

        return results;
    }
};
```

### Рекомендации по уровням сжатия для ProjectV

| Использование | Уровень | Скорость | Сжатие | Причина |
|---------------|---------|----------|--------|---------|
| **Сетевой протокол** | **1-3** | **500+ MB/s** | **2.0x** | Минимальная задержка |
| **Воксельные чанки** | **5-7** | **200-300 MB/s** | **2.5x** | Баланс скорости/сжатия |
| **Сохранения мира** | **10-12** | **50-100 MB/s** | **3.0x** | Максимальное сжатие |
| **Архивация релизов** | **19-22** | **5-10 MB/s** | **3.5x** | Минимальный размер |

---

## Заключение

Zstd предоставляет для ProjectV:

1. **Идеальный баланс** скорости и степени сжатия
2. **Dictionary Compression** для воксельных данных (10-50% улучшение)
3. **Многопоточную обработку** для параллельного сжатия чанков
4. **Настраиваемые уровни** от максимальной скорости до максимального сжатия

### Ключевые применения в ProjectV:

1. **Сжатие воксельных чанков** для передачи по сети и сохранения на диск
2. **Архивация миров** с максимальным сжатием для долгосрочного хранения
3. **Сетевой протокол** с минимальной задержкой для multiplayer
4. **Кэширование текстур** с быстрой декомпрессией для загрузки

Zstd — это не просто библиотека сжатия, это инфраструктура для эффективной работы с данными в ProjectV, оптимизированная под специфику воксельного движка.
