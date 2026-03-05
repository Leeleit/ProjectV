# Zstd Quickstart

🟢 **Уровень 1: Базовый**

Минимальный рабочий пример использования Zstd для сжатия данных.

## Базовое сжатие и распаковка

```cpp
#include <zstd.h>
#include <vector>
#include <iostream>
#include <cstring>

int main() {
    // 1. Создание тестовых данных
    const char* message = "Hello, Zstandard! This is a test message for compression.";
    const size_t srcSize = strlen(message);

    // 2. Определение максимального размера сжатых данных
    size_t dstCapacity = ZSTD_compressBound(srcSize);
    std::vector<char> compressed(dstCapacity);

    // 3. Сжатие (уровень 3 — баланс скорости и сжатия)
    size_t compressedSize = ZSTD_compress(
        compressed.data(), dstCapacity,
        message, srcSize,
        3
    );

    if (ZSTD_isError(compressedSize)) {
        std::cerr << "Ошибка сжатия: " << ZSTD_getErrorName(compressedSize) << std::endl;
        return 1;
    }

    compressed.resize(compressedSize);

    // 4. Распаковка
    std::vector<char> decompressed(srcSize + 1);  // +1 для null-терминатора
    size_t decompressedSize = ZSTD_decompress(
        decompressed.data(), decompressed.size(),
        compressed.data(), compressedSize
    );

    if (ZSTD_isError(decompressedSize)) {
        std::cerr << "Ошибка распаковки: " << ZSTD_getErrorName(decompressedSize) << std::endl;
        return 1;
    }

    decompressed[decompressedSize] = '\0';

    // 5. Проверка результата
    std::cout << "Исходный размер: " << srcSize << " байт" << std::endl;
    std::cout << "Сжатый размер: " << compressedSize << " байт" << std::endl;
    std::cout << "Коэффициент сжатия: "
              << static_cast<double>(srcSize) / compressedSize << "x" << std::endl;
    std::cout << "Распакованное сообщение: " << decompressed.data() << std::endl;

    return 0;
}
```

## Сжатие бинарных данных

```cpp
#include <zstd.h>
#include <vector>
#include <cstdint>

struct DataBlock {
    uint32_t id;
    float x, y, z;
    uint8_t flags;
};

std::vector<uint8_t> compressDataBlock(const std::vector<DataBlock>& blocks) {
    const size_t srcSize = blocks.size() * sizeof(DataBlock);
    const size_t dstCapacity = ZSTD_compressBound(srcSize);

    std::vector<uint8_t> compressed(dstCapacity);

    size_t compressedSize = ZSTD_compress(
        compressed.data(), dstCapacity,
        blocks.data(), srcSize,
        3
    );

    if (ZSTD_isError(compressedSize)) {
        throw std::runtime_error(ZSTD_getErrorName(compressedSize));
    }

    compressed.resize(compressedSize);
    return compressed;
}

std::vector<DataBlock> decompressDataBlock(const std::vector<uint8_t>& compressed) {
    // Определение размера распакованных данных
    unsigned long long contentSize = ZSTD_getFrameContentSize(
        compressed.data(), compressed.size()
    );

    if (contentSize == ZSTD_CONTENTSIZE_ERROR) {
        throw std::runtime_error("Invalid Zstd frame");
    }

    if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw std::runtime_error("Unknown content size");
    }

    std::vector<DataBlock> blocks(contentSize / sizeof(DataBlock));

    size_t decompressedSize = ZSTD_decompress(
        blocks.data(), contentSize,
        compressed.data(), compressed.size()
    );

    if (ZSTD_isError(decompressedSize)) {
        throw std::runtime_error(ZSTD_getErrorName(decompressedSize));
    }

    return blocks;
}
```

## Использование контекстов

Для многократных операций сжатия эффективнее использовать контексты:

```cpp
#include <zstd.h>
#include <vector>

class ZstdCompressor {
public:
    ZstdCompressor(int level = 3) : level_(level) {
        cctx_ = ZSTD_createCCtx();
        dctx_ = ZSTD_createDCtx();

        if (!cctx_ || !dctx_) {
            throw std::runtime_error("Failed to create Zstd context");
        }
    }

    ~ZstdCompressor() {
        ZSTD_freeCCtx(cctx_);
        ZSTD_freeDCtx(dctx_);
    }

    std::vector<uint8_t> compress(const void* data, size_t size) {
        size_t dstCapacity = ZSTD_compressBound(size);
        std::vector<uint8_t> compressed(dstCapacity);

        size_t compressedSize = ZSTD_compressCCtx(
            cctx_,
            compressed.data(), dstCapacity,
            data, size,
            level_
        );

        if (ZSTD_isError(compressedSize)) {
            throw std::runtime_error(ZSTD_getErrorName(compressedSize));
        }

        compressed.resize(compressedSize);
        return compressed;
    }

    std::vector<uint8_t> decompress(const void* data, size_t size) {
        unsigned long long contentSize = ZSTD_getFrameContentSize(data, size);

        if (contentSize == ZSTD_CONTENTSIZE_ERROR) {
            throw std::runtime_error("Invalid Zstd frame");
        }

        if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
            contentSize = size * 10;  // Консервативная оценка
        }

        std::vector<uint8_t> decompressed(contentSize);

        size_t decompressedSize = ZSTD_decompressDCtx(
            dctx_,
            decompressed.data(), contentSize,
            data, size
        );

        if (ZSTD_isError(decompressedSize)) {
            throw std::runtime_error(ZSTD_getErrorName(decompressedSize));
        }

        decompressed.resize(decompressedSize);
        return decompressed;
    }

private:
    ZSTD_CCtx* cctx_;
    ZSTD_DCtx* dctx_;
    int level_;
};
```

## Выбор уровня сжатия

| Уровень | Скорость сжатия | Степень сжатия | Сценарий               |
|---------|-----------------|----------------|------------------------|
| -5      | ~1500 MB/s      | Низкая         | Реальное время, буферы |
| 1       | ~600 MB/s       | Хорошая        | Частые операции        |
| 3       | ~350 MB/s       | Хорошая        | По умолчанию           |
| 10      | ~60 MB/s        | Высокая        | Файловый архив         |
| 19      | ~5 MB/s         | Максимальная   | Дистрибутив            |

## Обработка ошибок

Все функции Zstd возвращают `size_t`. Проверка на ошибку:

```cpp
size_t result = ZSTD_compress(...);

if (ZSTD_isError(result)) {
    // Получить имя ошибки
    const char* errorName = ZSTD_getErrorName(result);

    // Получить код ошибки
    ZSTD_ErrorCode errorCode = ZSTD_getErrorCode(result);

    // Обработка по коду
    switch (errorCode) {
        case ZSTD_error_dstSize_tooSmall:
            // Буфер назначения слишком мал
            break;
        case ZSTD_error_memory_allocation:
            // Ошибка выделения памяти
            break;
        case ZSTD_error_corruption_detected:
            // Данные повреждены
            break;
        default:
            break;
    }
}
```

## Ключевые функции

| Функция                      | Описание                            |
|------------------------------|-------------------------------------|
| `ZSTD_compress()`            | Однократное сжатие                  |
| `ZSTD_decompress()`          | Однократная распаковка              |
| `ZSTD_compressBound()`       | Максимальный размер сжатых данных   |
| `ZSTD_getFrameContentSize()` | Размер исходных данных из заголовка |
| `ZSTD_isError()`             | Проверка результата на ошибку       |
| `ZSTD_getErrorName()`        | Строковое описание ошибки           |
