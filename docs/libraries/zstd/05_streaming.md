# Потоковое сжатие Zstd

Потоковый API для работы с данными большого размера или неизвестного размера.

## Когда использовать потоковое сжатие

- Данные > 100 MB
- Размер данных неизвестен заранее
- Данные генерируются постепенно
- Ограниченная оперативная память

## Структуры данных

### ZSTD_inBuffer

```cpp
typedef struct {
    const void* src;   // Указатель на данные
    size_t size;       // Полный размер буфера
    size_t pos;        // Текущая позиция чтения (обновляется Zstd)
} ZSTD_inBuffer;
```

### ZSTD_outBuffer

```cpp
typedef struct {
    void* dst;   // Указатель на буфер назначения
    size_t size; // Полный размер буфера
    size_t pos;  // Текущая позиция записи (обновляется Zstd)
} ZSTD_outBuffer;
```

## Потоковое сжатие

### Создание и освобождение CStream

```cpp
ZSTD_CStream* ZSTD_createCStream(void);
size_t ZSTD_freeCStream(ZSTD_CStream* zcs);
```

### Инициализация

```cpp
size_t ZSTD_initCStream(ZSTD_CStream* zcs, int compressionLevel);
```

Или с расширенными параметрами:

```cpp
ZSTD_CStream* zcs = ZSTD_createCStream();
ZSTD_CCtx_reset(zcs, ZSTD_reset_session_and_parameters);
ZSTD_CCtx_setParameter(zcs, ZSTD_c_compressionLevel, 3);
ZSTD_CCtx_setParameter(zcs, ZSTD_c_checksumFlag, 1);
```

### Сжатие данных

```cpp
size_t ZSTD_compressStream(
    ZSTD_CStream* zcs,
    ZSTD_outBuffer* output,
    ZSTD_inBuffer* input
);
```

Или современный вариант:

```cpp
size_t ZSTD_compressStream2(
    ZSTD_CCtx* cctx,
    ZSTD_outBuffer* output,
    ZSTD_inBuffer* input,
    ZSTD_EndDirective endOp
);
```

### Завершение сжатия

```cpp
size_t ZSTD_endStream(ZSTD_CStream* zcs, ZSTD_outBuffer* output);
```

## Полный пример сжатия

```cpp
#include <zstd.h>
#include <vector>
#include <fstream>

class StreamCompressor {
public:
    StreamCompressor(int level = 3) {
        cctx_ = ZSTD_createCCtx();
        ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, level);
        ZSTD_CCtx_setParameter(cctx_, ZSTD_c_contentSizeFlag, 0);
    }

    ~StreamCompressor() {
        ZSTD_freeCCtx(cctx_);
    }

    std::vector<uint8_t> compressStream(
        const void* data, size_t size,
        size_t chunkSize = 64 * 1024
    ) {
        std::vector<uint8_t> result;
        result.reserve(ZSTD_compressBound(size));

        ZSTD_inBuffer input = { data, size, 0 };

        std::vector<uint8_t> outputBuffer(ZSTD_BLOCKSIZE_MAX);

        // Сжатие по частям
        while (input.pos < input.size) {
            ZSTD_outBuffer output = { outputBuffer.data(), outputBuffer.size(), 0 };

            size_t ret = ZSTD_compressStream2(
                cctx_, &output, &input, ZSTD_e_continue
            );

            if (ZSTD_isError(ret)) {
                throw std::runtime_error(ZSTD_getErrorName(ret));
            }

            result.insert(
                result.end(),
                outputBuffer.data(),
                outputBuffer.data() + output.pos
            );
        }

        // Завершение фрейма
        ZSTD_outBuffer output = { outputBuffer.data(), outputBuffer.size(), 0 };
        size_t ret = ZSTD_compressStream2(
            cctx_, &output, &input, ZSTD_e_end
        );

        if (ZSTD_isError(ret)) {
            throw std::runtime_error(ZSTD_getErrorName(ret));
        }

        result.insert(
            result.end(),
            outputBuffer.data(),
            outputBuffer.data() + output.pos
        );

        return result;
    }

private:
    ZSTD_CCtx* cctx_;
};
```

## Потоковая распаковка

### Создание и освобождение DStream

```cpp
ZSTD_DStream* ZSTD_createDStream(void);
size_t ZSTD_freeDStream(ZSTD_DStream* zds);
```

### Инициализация

```cpp
size_t ZSTD_initDStream(ZSTD_DStream* zds);
```

### Распаковка данных

```cpp
size_t ZSTD_decompressStream(
    ZSTD_DStream* zds,
    ZSTD_outBuffer* output,
    ZSTD_inBuffer* input
);
```

## Полный пример распаковки

```cpp
#include <zstd.h>
#include <vector>

class StreamDecompressor {
public:
    StreamDecompressor() {
        dctx_ = ZSTD_createDCtx();
    }

    ~StreamDecompressor() {
        ZSTD_freeDCtx(dctx_);
    }

    std::vector<uint8_t> decompressStream(
        const void* data, size_t size,
        size_t estimatedSize = 0
    ) {
        std::vector<uint8_t> result;

        if (estimatedSize > 0) {
            result.reserve(estimatedSize);
        }

        ZSTD_inBuffer input = { data, size, 0 };

        std::vector<uint8_t> outputBuffer(ZSTD_BLOCKSIZE_MAX);

        while (input.pos < input.size) {
            ZSTD_outBuffer output = { outputBuffer.data(), outputBuffer.size(), 0 };

            size_t ret = ZSTD_decompressStream(dctx_, &output, &input);

            if (ZSTD_isError(ret)) {
                throw std::runtime_error(ZSTD_getErrorName(ret));
            }

            result.insert(
                result.end(),
                outputBuffer.data(),
                outputBuffer.data() + output.pos
            );

            // ret == 0 означает конец фрейма
            if (ret == 0 && input.pos == input.size) {
                break;
            }
        }

        return result;
    }

private:
    ZSTD_DCtx* dctx_;
};
```

## EndDirective

Функция `ZSTD_compressStream2` принимает директиву завершения:

| Директива         | Описание                                 |
|-------------------|------------------------------------------|
| `ZSTD_e_continue` | Продолжать сжатие, не завершать фрейм    |
| `ZSTD_e_flush`    | Завершить текущий блок, продолжить фрейм |
| `ZSTD_e_end`      | Завершить фрейм                          |

## Мультифреймовое сжатие

```cpp
// Создание нескольких фреймов в одном потоке
void compressMultipleFrames(
    ZSTD_CCtx* cctx,
    const std::vector<std::vector<uint8_t>>& dataFrames,
    std::vector<uint8_t>& output
) {
    std::vector<uint8_t> buffer(ZSTD_BLOCKSIZE_MAX);

    for (size_t i = 0; i < dataFrames.size(); ++i) {
        ZSTD_inBuffer input = {
            dataFrames[i].data(),
            dataFrames[i].size(),
            0
        };

        // Сжатие данных
        while (input.pos < input.size) {
            ZSTD_outBuffer out = { buffer.data(), buffer.size(), 0 };
            ZSTD_compressStream2(cctx, &out, &input, ZSTD_e_continue);
            output.insert(output.end(), buffer.data(), buffer.data() + out.pos);
        }

        // Завершение текущего фрейма
        ZSTD_inBuffer empty = { nullptr, 0, 0 };
        ZSTD_outBuffer out = { buffer.data(), buffer.size(), 0 };

        size_t ret;
        do {
            out.pos = 0;
            ret = ZSTD_compressStream2(cctx, &out, &empty, ZSTD_e_end);
            output.insert(output.end(), buffer.data(), buffer.data() + out.pos);
        } while (ret > 0);

        // Сброс для нового фрейма
        ZSTD_CCtx_reset(cctx, ZSTD_reset_session_only);
    }
}
```

## Сжатие файла по частям

```cpp
void compressFile(
    const std::string& inputPath,
    const std::string& outputPath,
    int level = 3
) {
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);

    std::ifstream inFile(inputPath, std::ios::binary);
    std::ofstream outFile(outputPath, std::ios::binary);

    constexpr size_t BUFFER_SIZE = 64 * 1024;
    std::vector<uint8_t> inBuffer(BUFFER_SIZE);
    std::vector<uint8_t> outBuffer(ZSTD_BLOCKSIZE_MAX);

    bool finished = false;

    while (!finished) {
        inFile.read(reinterpret_cast<char*>(inBuffer.data()), BUFFER_SIZE);
        size_t bytesRead = inFile.gcount();
        finished = inFile.eof();

        ZSTD_inBuffer input = { inBuffer.data(), bytesRead, 0 };

        while (input.pos < input.size) {
            ZSTD_outBuffer output = { outBuffer.data(), outBuffer.size(), 0 };

            ZSTD_EndDirective mode = finished ? ZSTD_e_end : ZSTD_e_continue;
            size_t ret = ZSTD_compressStream2(cctx, &output, &input, mode);

            if (ZSTD_isError(ret)) {
                throw std::runtime_error(ZSTD_getErrorName(ret));
            }

            outFile.write(
                reinterpret_cast<char*>(outBuffer.data()),
                output.pos
            );
        }
    }

    ZSTD_freeCCtx(cctx);
}
```

## Распаковка файла по частям

```cpp
void decompressFile(
    const std::string& inputPath,
    const std::string& outputPath
) {
    ZSTD_DCtx* dctx = ZSTD_createDCtx();

    std::ifstream inFile(inputPath, std::ios::binary);
    std::ofstream outFile(outputPath, std::ios::binary);

    constexpr size_t BUFFER_SIZE = 64 * 1024;
    std::vector<uint8_t> inBuffer(BUFFER_SIZE);
    std::vector<uint8_t> outBuffer(ZSTD_BLOCKSIZE_MAX);

    while (inFile) {
        inFile.read(reinterpret_cast<char*>(inBuffer.data()), BUFFER_SIZE);
        size_t bytesRead = inFile.gcount();

        if (bytesRead == 0) break;

        ZSTD_inBuffer input = { inBuffer.data(), bytesRead, 0 };

        while (input.pos < input.size) {
            ZSTD_outBuffer output = { outBuffer.data(), outBuffer.size(), 0 };

            size_t ret = ZSTD_decompressStream(dctx, &output, &input);

            if (ZSTD_isError(ret)) {
                throw std::runtime_error(ZSTD_getErrorName(ret));
            }

            outFile.write(
                reinterpret_cast<char*>(outBuffer.data()),
                output.pos
            );

            // ret == 0 означает конец фрейма
            if (ret == 0) break;
        }
    }

    ZSTD_freeDCtx(dctx);
}
```

## Рекомендации

1. **Размер буфера**: Используйте `ZSTD_BLOCKSIZE_MAX` (128 KB) для выходного буфера
2. **Контексты**: Переиспользуйте контексты между операциями
3. **Память**: Потоковое сжатие использует фиксированный объём памяти независимо от размера данных
4. **Производительность**: Для максимальной скорости используйте многопоточное сжатие (`ZSTD_c_nbWorkers`)
