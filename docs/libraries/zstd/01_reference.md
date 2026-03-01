# Zstandard: Чистый справочник

**Zstandard (Zstd)** — алгоритм сжатия общего назначения, разработанный Yann Collet в Facebook. Обеспечивает
исключительный баланс между скоростью сжатия, скоростью декомпрессии и степенью сжатия.

> **Для понимания:** Представьте Zstd как "умный архивариус". Он не просто сжимает данные — он запоминает паттерны и
> использует их для предсказания. Как опытный библиотекарь, который знает, где что лежит, и может мгновенно найти
> нужное.

## Ключевые характеристики

| Параметр              | Значение      |
|-----------------------|---------------|
| Скорость сжатия       | до 500 MB/s   |
| Скорость декомпрессии | до 2000 MB/s  |
| Степень сжатия        | 2.5:1 — 3.5:1 |
| Уровни сжатия         | 1–22          |
| Макс. размер окна     | 128 MB        |
| Потокобезопасность    | Да            |
| Dictionary            | Да (до 16 MB) |

## Типичное применение

- Базы данных и системы хранения
- Сетевые протоколы
- Файловые системы и архивы
- Game engine ресурсы
- Резервное копирование

---

## Архитектура

### Блочная структура фрейма

```
┌─────────────────────────────────────────────────────────────┐
│                         Zstd Frame                          │
├──────────┬──────────┬──────────────┬──────────┬────────────┤
│  Header  │  Block 1 │  Block 2 ... │  Block N │  Checksum  │
│  4 bytes │ variable │   variable   │ variable │  4 bytes   │
└──────────┴──────────┴──────────────┴──────────┴────────────┘
```

Каждый фрейм содержит:

- **Header (4 байта)** — магическое число, версия, параметры
- **Blocks** — сжатые данные переменного размера (max 128 KB)
- **Checksum (опционально)** — xxHash для проверки целостности

### Этапы сжатия

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Input Data │ -> │   Matching   │ -> │   Entropy   │
│              │    │   (Zstd)     │    │   Coding    │
└──────────────┘    └──────────────┘    └──────────────┘
                           ↓
                    ┌──────────────┐
                    │   LDM Mode   │
                    │ (опционально)│
                    └──────────────┘
```

#### 1. Поиск совпадений (Matching)

Zstd использует несколько стратегий:

| Метод              | Описание                           | Применение            |
|--------------------|------------------------------------|-----------------------|
| **Hash Chain**     | Хэш-таблица для быстрого поиска    | Быстрые уровни (1–3)  |
| **Binary Tree**    | Двоичное дерево для точного поиска | Средние уровни (4–15) |
| **Optimal Parser** | Динамическое программирование      | Макс. сжатие (19–22)  |

#### 2. Кодирование энтропии (Entropy Coding)

- **FSE (Finite State Entropy)** — кодирование символов
- **Huffman** — кодирование литералов
- **tANS** — асимметричная система счисления

> **Для понимания:** Энтропийное кодирование — это как сокращённая запись. Вместо "камень камень камень" пишем "
> 3×камень". Чем чаще повторяется паттерн, тем короче его представление.

#### 3. Long Distance Matching (LDM)

LDM — механизм для обнаружения повторений на больших расстояниях (до 128 MB):

- Эффективен для данных с периодическими паттернами
- Активируется на уровнях 4+
- Требует дополнительной памяти

---

## Уровни сжатия

### Таблица уровней

| Уровень | Скорость сжатия | Скорость декпрессиоми | Степень | Применение           |
|---------|-----------------|-----------------------|---------|----------------------|
| 1       | ~500 MB/s       | ~2000 MB/s            | 2.0x    | Real-time, streaming |
| 3       | ~400 MB/s       | ~1800 MB/s            | 2.2x    | Network packets      |
| 5       | ~250 MB/s       | ~1500 MB/s            | 2.5x    | Balanced             |
| 10      | ~80 MB/s        | ~1000 MB/s            | 2.8x    | Storage              |
| 15      | ~30 MB/s        | ~800 MB/s             | 3.2x    | Archives             |
| 19      | ~10 MB/s        | ~600 MB/s             | 3.5x    | Max compression      |

### Выбор уровня

```cpp
// Быстрая проверка пригодности уровня
bool can_use_level(int level) {
    // Уровни 12+ требуют значительного времени
    return level >= 12 ? std::chrono::seconds(5) : std::chrono::milliseconds(100);
}
```

---

## Dictionary Compression

### Концепция

Dictionaries позволяют Zstd "запомнить" типичные паттерны данных до начала сжатия. Это критически важно для данных с
повторяющейся структурой.

> **Для понимания:** Dictionary — это как "шпаргалка" для архиватора. Вместо того чтобы каждый раз заново разбираться в
> структуре данных, он сразу знает типичные паттерны. Как если бы архивариус заранее знал, что в архиве часто
> встречаются
> документы определённого формата.

### Обучение словаря

```cpp
#include <zstd.h>
#include <vector>
#include <span>
#include <cstddef>

// Обучение словаря на пользовательских данных
std::expected<size_t, std::string> train_dictionary(
    std::span<const uint8_t> dict_buffer,
    std::span<const std::span<const uint8_t>> samples
) {
    // Подготовка указателей и размеров
    std::vector<size_t> sample_sizes(samples.size());
    std::vector<const void*> sample_ptrs(samples.size());

    for (size_t i = 0; i < samples.size(); ++i) {
        sample_ptrs[i] = samples[i].data();
        sample_sizes[i] = samples[i].size();
    }

    // Обучение
    size_t result = ZDICT_trainFromBuffer(
        dict_buffer.data(),
        dict_buffer.size(),
        sample_ptrs.data(),
        sample_sizes.data(),
        static_cast<unsigned>(samples.size())
    );

    if (ZDICT_isError(result)) {
        return std::unexpected(ZDICT_getErrorName(result));
    }

    return result; // Размер обученного словаря
}
```

### Использование словаря

```cpp
// Создание compression dictionary
ZSTD_CDict* create_cdict(
    std::span<const uint8_t> dict_data,
    int compression_level
) {
    return ZSTD_createCDict(
        dict_data.data(),
        dict_data.size(),
        compression_level
    );
}

// Создание decompression dictionary
ZSTD_DDict* create_ddict(std::span<const uint8_t> dict_data) {
    return ZSTD_createDDict(
        dict_data.data(),
        dict_data.size()
    );
}
```

---

## API: Базовое сжатие

### Простое API (однохнопечное)

```cpp
#include <zstd.h>
#include <vector>
#include <span>
#include <expected>

// Сжатие данных
std::expected<std::vector<uint8_t>, std::string> compress(
    std::span<const uint8_t> src,
    int level = 3
) {
    // Вычисление максимального размера вывода
    const size_t bound = ZSTD_compressBound(src.size());

    std::vector<uint8_t> dst(bound);

    // Сжатие
    const size_t compressed_size = ZSTD_compress(
        dst.data(),
        bound,
        src.data(),
        src.size(),
        level
    );

    if (ZSTD_isError(compressed_size)) {
        return std::unexpected(ZSTD_getErrorName(compressed_size));
    }

    dst.resize(compressed_size);
    return dst;
}

// Декомпрессия данных
std::expected<std::vector<uint8_t>, std::string> decompress(
    std::span<const uint8_t> src
) {
    // Получение размера оригинальных данных
    const unsigned long long original_size = ZSTD_getFrameContentSize(
        src.data(),
        src.size()
    );

    if (original_size == ZSTD_CONTENTSIZE_ERROR) {
        return std::unexpected("Invalid compressed data");
    }

    if (original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return std::unexpected("Unknown original size - use streaming API");
    }

    std::vector<uint8_t> dst(static_cast<size_t>(original_size));

    const size_t decompressed_size = ZSTD_decompress(
        dst.data(),
        original_size,
        src.data(),
        src.size()
    );

    if (ZSTD_isError(decompressed_size)) {
        return std::unexpected(ZSTD_getErrorName(decompressed_size));
    }

    if (decompressed_size != original_size) {
        return std::unexpected("Size mismatch after decompression");
    }

    return dst;
}
```

### Продвинутое API (streaming)

```cpp
#include <zstd.h>
#include <vector>
#include <span>
#include <expected>

class ZstdCompressor {
public:
    explicit ZstdCompressor(int level = 3) : level_(level) {
        cctx_ = ZSTD_createCCtx();
        if (!cctx_) {
            // В ProjectV используем std::expected вместо throw
            // Пример правильного подхода:
            // return std::unexpected("Failed to create compression context");
        }
        ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, level);
    }

    ~ZstdCompressor() {
        if (cctx_) ZSTD_freeCCtx(cctx_);
    }

    // Сжатие с использованием словаря
    std::expected<std::vector<uint8_t>, std::string> compress_with_dict(
        std::span<const uint8_t> src,
        std::span<const uint8_t> dict
    ) {
        // Создание словаря для этого контекста (или переиспользовать CDict)
        ZSTD_CDict* cdict = ZSTD_createCDict(
            dict.data(), dict.size(), level_
        );

        if (!cdict) {
            return std::unexpected("Failed to create dictionary");
        }

        ZSTD_CCtx_refCDict(cctx_, cdict);

        const size_t bound = ZSTD_compressBound(src.size());
        std::vector<uint8_t> dst(bound);

        const size_t result = ZSTD_compress2(
            cctx_,
            dst.data(), bound,
            src.data(), src.size()
        );

        ZSTD_freeCDict(cdict);

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        dst.resize(result);
        return dst;
    }

private:
    ZSTD_CCtx* cctx_ = nullptr;
    int level_;
};

class ZstdDecompressor {
public:
    ZstdDecompressor() {
        dctx_ = ZSTD_createDCtx();
        if (!dctx_) {
            // В ProjectV используем std::expected вместо throw
            // Пример правильного подхода:
            // return std::unexpected("Failed to create decompression context");
        }
    }

    ~ZstdDecompressor() {
        if (dctx_) ZSTD_freeDCtx(dctx_);
    }

    std::expected<std::vector<uint8_t>, std::string> decompress_with_dict(
        std::span<const uint8_t> src,
        std::span<const uint8_t> dict
    ) {
        // Создание словаря
        ZSTD_DDict* ddict = ZSTD_createDDict(
            dict.data(), dict.size()
        );

        if (!ddict) {
            return std::unexpected("Failed to create decompression dictionary");
        }

        ZSTD_DCtx_refDDict(dctx_, ddict);

        // Определение размера
        const unsigned long long original_size = ZSTD_getFrameContentSize(
            src.data(), src.size()
        );

        if (original_size == ZSTD_CONTENTSIZE_ERROR) {
            ZSTD_freeDDict(ddict);
            return std::unexpected("Invalid frame");
        }

        std::vector<uint8_t> dst(static_cast<size_t>(original_size));

        const size_t result = ZSTD_decompressDCtx(
            dctx_,
            dst.data(), dst.size(),
            src.data(), src.size()
        );

        ZSTD_freeDDict(ddict);

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        dst.resize(result);
        return dst;
    }

private:
    ZSTD_DCtx* dctx_ = nullptr;
};
```

---

## API: Streaming (потоковое сжатие)

### Потоковое сжатие

```cpp
#include <zstd.h>
#include <span>
#include <vector>
#include <expected>

class ZstdStreamingCompressor {
public:
    explicit ZstdStreamingCompressor(int level = 3)
        : level_(level)
    {
        cctx_ = ZSTD_createCStream();
        if (!cctx_) {
            // В ProjectV используем std::expected вместо throw
            // Пример правильного подхода:
            // return std::unexpected("Failed to create stream");
        }

        ZSTD_initCStream(cctx_, level);
    }

    ~ZstdStreamingCompressor() {
        if (cctx_) ZSTD_freeCStream(cctx_);
    }

    // Инициализация с параметрами
    void set_checksum(bool enabled = true) {
        ZSTD_CCtx_setParameter(cctx_, ZSTD_c_checksumFlag, enabled ? 1 : 0);
    }

    void set_dict(std::span<const uint8_t> dict) {
        // Создаём и кэшируем словарь
        cdict_ = ZSTD_createCDict(dict.data(), dict.size(), level_);
        if (cdict_) {
            ZSTD_CCtx_refCDict(cctx_, cdict_);
        }
    }

    // Сжатие порции данных
    std::expected<std::vector<uint8_t>, std::string> compress(
        std::span<const uint8_t> input,
        bool flush = false
    ) {
        ZSTD_inBuffer in{
            .src = input.data(),
            .size = input.size(),
            .pos = 0
        };

        std::vector<uint8_t> output(ZSTD_CStreamOutSize());
        ZSTD_outBuffer out{
            .dst = output.data(),
            .size = output.size(),
            .pos = 0
        };

        size_t result = ZSTD_compressStream2(
            cctx_,
            &out,
            &in,
            flush ? ZSTD_e_end : ZSTD_e_continue
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        output.resize(out.pos);
        return output;
    }

    // Завершение потока
    std::expected<std::vector<uint8_t>, std::string> flush() {
        ZSTD_inBuffer in{.src = nullptr, .size = 0, .pos = 0};

        std::vector<uint8_t> output(ZSTD_CStreamOutSize());
        ZSTD_outBuffer out{
            .dst = output.data(),
            .size = output.size(),
            .pos = 0
        };

        size_t result = ZSTD_compressStream2(cctx_, &out, &in, ZSTD_e_end);

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        output.resize(out.pos);
        return output;
    }

private:
    ZSTD_CStream* cctx_ = nullptr;
    ZSTD_CDict* cdict_ = nullptr;
    int level_;
};
```

### Потоковая декомпрессия

```cpp
class ZstdStreamingDecompressor {
public:
    ZstdStreamingDecompressor() {
        dctx_ = ZSTD_createDStream();
        if (!dctx_) {
            // В ProjectV используем std::expected вместо throw
            // Пример правильного подхода:
            // return std::unexpected("Failed to create stream");
        }

        ZSTD_initDStream(dctx_);
    }

    ~ZstdStreamingDecompressor() {
        if (dctx_) ZSTD_freeDStream(dctx_);
    }

    void set_dict(std::span<const uint8_t> dict) {
        ddict_ = ZSTD_createDDict(dict.data(), dict.size());
        if (ddict_) {
            ZSTD_DCtx_refDDict(dctx_, ddict_);
        }
    }

    std::expected<std::vector<uint8_t>, std::string> decompress(
        std::span<const uint8_t> input
    ) {
        ZSTD_inBuffer in{
            .src = input.data(),
            .size = input.size(),
            .pos = 0
        };

        std::vector<uint8_t> output(ZSTD_DStreamOutSize());
        ZSTD_outBuffer out{
            .dst = output.data(),
            .size = output.size(),
            .pos = 0
        };

        size_t result = ZSTD_decompressStream(dctx_, &out, &in);

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        output.resize(out.pos);
        return output;
    }

private:
    ZSTD_DStream* dctx_ = nullptr;
    ZSTD_DDict* ddict_ = nullptr;
};
```

---

## Многопоточное сжатие

### Концепция

Zstd поддерживает многопоточное сжатие через параметр `nbWorkers`:

```cpp
#include <zstd.h>
#include <thread>
#include <vector>

void compress_with_workers(
    std::span<const uint8_t> src,
    std::span<uint8_t> dst,
    int workers
) {
    ZSTD_CCtx* cctx = ZSTD_createCCtx();

    // Установка количества рабочих потоков
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, workers);

    // Установка уровня сжатия
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 3);

    ZSTD_compress2(cctx, dst.data(), dst.size(), src.data(), src.size());

    ZSTD_freeCCtx(cctx);
}
```

### Рекомендации по потокам

| Размер данных | Рекомендуемые потоки |
|---------------|----------------------|
| < 1 MB        | 0–2                  |
| 1–100 MB      | 2–4                  |
| 100 MB – 1 GB | 4–8                  |
| > 1 GB        | 8–16                 |

---

## Параметры контекста

### Доступные параметры

```cpp
// Уровень сжатия
ZSTD_c_compressionLevel = 1..22

// Многопоточность
ZSTD_c_nbWorkers = 0..256

// Контрольные флаги
ZSTD_c_checksumFlag = 0/1
ZSTD_c_contentSizeFlag = 0/1
ZSTD_c_dictIDFlag = 0/1

// Dictionary ID
ZSTD_c_dictID = 0..

// Предиктор
ZSTD_c_enableLongDistanceMatching = 0/1
ZSTD_c_ldmHashRateLog = 6..12
ZSTD_c_ldmMinMatch = 4..4096
```

### Пример конфигурации

```cpp
void configure_for_speed(ZSTD_CCtx* cctx) {
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 1);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, 4);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
}

void configure_for_size(ZSTD_CCtx* cctx) {
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 19);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, 0); // Однопоточно
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_enableLongDistanceMatching, 1);
}
```

---

## Обработка ошибок

### Проверка ошибок

```cpp
#include <zstd.h>
#include <expected>
#include <print>

// Шаблон проверки результата
template<typename T>
std::expected<T, std::string> check_zstd(T result) {
    if (ZSTD_isError(result)) {
        return std::unexpected(ZSTD_getErrorName(result));
    }
    return result;
}

// Пример использования
void example() {
    auto result = check_zstd(ZSTD_compress(dst, dst_size, src, src_size, 3));

    if (!result) {
        std::print(stderr, "Compression failed: {}\n", result.error());
        return;
    }

    std::println("Compressed {} -> {} bytes", src_size, *result);
}
```

### Коды ошибок

```cpp
// Проверка на ошибку
bool is_error(size_t code) {
    return ZSTD_isError(code);
}

// Получение сообщения об ошибке
const char* error_name(size_t code) {
    return ZSTD_getErrorName(code);
}

// Код ошибки (для программной обработки)
ZSTD_ErrorCode error_code(size_t code) {
    return ZSTD_getErrorCode(code);
}
```

---

## Ограничения памяти

### Размеры буферов

```cpp
// Максимальный размер сжатых данных для блока
constexpr size_t MAX_BLOCK_SIZE = ZSTD_BLOCKSIZE_MAX; // 128 KB

// Минимальный размер сжатого блока
constexpr size_t MIN_BLOCK_SIZE = 1;

// Размер входного буфера для потокового сжатия
constexpr size_t INPUT_BUFFER_SIZE = 256 * 1024; // 256 KB

// Размер выходного буфера для потокового сжатия
constexpr size_t OUTPUT_BUFFER_SIZE = ZSTD_CStreamOutSize(); // ~ 512 KB
```

### Оценка памяти

```cpp
#include <zstd.h>

// Оценка памяти для сжатия
size_t estimate_cctx_memory(int level, int workers) {
    return ZSTD_estimateCCtxSize(level) + (workers * ZSTD_estimateCScriptSize());
}

// Оценка памяти для декомпрессии
size_t estimate_dctx_memory() {
    return ZSTD_estimateDCtxSize();
}

// Оценка размера сжатых данных
size_t estimate_compressed_size(size_t src_size) {
    return ZSTD_compressBound(src_size);
}
```

---

## Формат файла

### Структура фрейма

```
┌─────────────────────────────────────────────────────────────┐
│  Magic Number (4 bytes)                                     │
│  0x28B52FD2 (little-endian)                                │
├─────────────────────────────────────────────────────────────┤
│  Frame Header Descriptor (1-2 bytes)                        │
│  - Frame size present?                                      │
│  - Single segment?                                          │
│  - Reserved bits                                            │
│  - Dictionary ID flag                                        │
│  - Checksum flag                                            │
│  - Content size flag                                        │
├─────────────────────────────────────────────────────────────┤
│  [Dictionary ID] (0-4 bytes, if present)                    │
├─────────────────────────────────────────────────────────────┤
│  [Content Size] (0-8 bytes, if present)                     │
├─────────────────────────────────────────────────────────────┤
│  Header Checksum (1 byte, if single segment + checksum)     │
├─────────────────────────────────────────────────────────────┤
│  Blocks                                                     │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ Block Header (3 bytes)                                   ││
│  │ - Block size (max 128 KB)                               ││
│  │ - Block type (raw/compressed/rle/reserved)             ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │ Block Data (variable)                                   ││
│  └─────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│  [Content Checksum] (4 bytes, if present)                  │
└─────────────────────────────────────────────────────────────┘
```

### Магическое число

```cpp
constexpr uint32_t ZSTD_MAGIC_NUMBER = 0x28B52FD2;
constexpr uint32_t ZSTD_MAGIC_DICTIONARY = 0xEC30A437; // Обучение словаря
```
