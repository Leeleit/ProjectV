# Решение проблем Zstd

Типичные ошибки и способы их решения.

## Обработка ошибок

### Проверка результата

Все функции Zstd возвращают `size_t`. Проверка на ошибку:

```cpp
size_t result = ZSTD_compress(dst, dstCapacity, src, srcSize, 3);

if (ZSTD_isError(result)) {
    const char* errorName = ZSTD_getErrorName(result);
    ZSTD_ErrorCode errorCode = ZSTD_getErrorCode(result);

    fprintf(stderr, "Error: %s (code: %d)\n", errorName, errorCode);
    return;
}
```

### Коды ошибок

| Код | Имя                               | Описание                     |
|-----|-----------------------------------|------------------------------|
| 0   | `ZSTD_error_no_error`             | Успех                        |
| 1   | `ZSTD_error_GENERIC`              | Общая ошибка                 |
| 2   | `ZSTD_error_dstSize_tooSmall`     | Буфер назначения слишком мал |
| 3   | `ZSTD_error_srcSize_wrong`        | Неверный размер источника    |
| 4   | `ZSTD_error_corruption_detected`  | Повреждённые данные          |
| 5   | `ZSTD_error_checksum_wrong`       | Неверная контрольная сумма   |
| 6   | `ZSTD_error_dictionary_corrupted` | Повреждённый словарь         |
| 7   | `ZSTD_error_dictionary_wrong`     | Неверный словарь             |
| 8   | `ZSTD_error_memory_allocation`    | Ошибка выделения памяти      |

## Распространённые проблемы

### Проблема: dstSize_tooSmall

**Симптомы:**

- `ZSTD_compress()` возвращает ошибку
- `ZSTD_getErrorName()` возвращает "Destination buffer is too small"

**Причины:**

1. Буфер назначения меньше `ZSTD_compressBound(srcSize)`
2. Неверный расчёт размера буфера

**Решение:**

```cpp
// Неправильно
std::vector<uint8_t> compressed(data.size());  // Недостаточно!

// Правильно
size_t dstCapacity = ZSTD_compressBound(data.size());
std::vector<uint8_t> compressed(dstCapacity);

size_t result = ZSTD_compress(
    compressed.data(), compressed.size(),
    data.data(), data.size(),
    3
);

if (ZSTD_isError(result)) {
    // Обработка ошибки
}

compressed.resize(result);  // Уменьшение до фактического размера
```

### Проблема: frameParameter_unsupported

**Симптомы:**

- `ZSTD_decompress()` возвращает ошибку
- Не удаётся распаковать данные

**Причины:**

1. Данные не являются Zstd фреймом
2. Несовместимая версия Zstd
3. Повреждённые данные

**Решение:**

```cpp
// Проверка, что это Zstd фрейм
unsigned long long contentSize = ZSTD_getFrameContentSize(data, size);

if (contentSize == ZSTD_CONTENTSIZE_ERROR) {
    // Не является Zstd фреймом
    return false;
}

// Проверка magic number
if (size >= 4) {
    uint32_t magic = *reinterpret_cast<const uint32_t*>(data);
    if (magic != 0xFD2FB528) {
        // Неверный magic number
        return false;
    }
}
```

### Проблема: corruption_detected

**Симптомы:**

- Ошибка при распаковке
- Данные не проходят валидацию

**Причины:**

1. Данные повреждены при передаче или хранении
2. Неверный словарь
3. Неполные данные

**Решение:**

```cpp
// Включить checksum при сжатии для обнаружения повреждений
ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

// При распаковке проверить целостность
size_t result = ZSTD_decompress(dst, dstCapacity, src, srcSize);

if (ZSTD_isError(result)) {
    if (ZSTD_getErrorCode(result) == ZSTD_error_corruption_detected) {
        // Данные повреждены
        fprintf(stderr, "Data corruption detected\n");
    }
}
```

### Проблема: Низкое сжатие

**Симптомы:**

- Сжатые данные почти равны исходным
- Коэффициент сжатия < 50%

**Причины:**

1. Случайные или шумовые данные
2. Уже сжатые данные (архивы, изображения)
3. Неоптимальная структура данных

**Решение:**

```cpp
// Проверка сжимаемости данных
double estimateCompressibility(const void* data, size_t size) {
    // Быстрый тест сжатия
    size_t bound = ZSTD_compressBound(size);
    std::vector<uint8_t> compressed(bound);

    size_t compressedSize = ZSTD_compress(
        compressed.data(), bound,
        data, size,
        1  // Быстрый уровень для оценки
    );

    if (ZSTD_isError(compressedSize)) {
        return 0.0;
    }

    return static_cast<double>(compressedSize) / size;
}

// Если сжимаемость низкая (< 0.9), возможно данные уже сжаты
double ratio = estimateCompressibility(data, size);
if (ratio > 0.9) {
    // Данные плохо сжимаются
    // Рассмотреть альтернативы
}
```

### Проблема: memory_allocation

**Симптомы:**

- `ZSTD_createCCtx()` или `ZSTD_createDCtx()` возвращает NULL
- Ошибка выделения памяти

**Причины:**

1. Недостаточно памяти
2. Слишком большой window size

**Решение:**

```cpp
// Ограничение window size для уменьшения памяти
ZSTD_CCtx* cctx = ZSTD_createCCtx();
ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog, 20);  // 1 MB window

// Проверка оценки памяти
size_t estimatedSize = ZSTD_estimateCCtxSize(3);
printf("Estimated CCtx size: %zu bytes\n", estimatedSize);
```

### Проблема: Неверный словарь

**Симптомы:**

- Ошибка при распаковке со словарём
- `ZSTD_error_dictionary_wrong`

**Причины:**

1. Неверный словарь
2. Словарь не использовался при сжатии
3. Несоответствие ID словаря

**Решение:**

```cpp
// Проверка соответствия словаря
bool verifyDictionary(
    const void* compressed, size_t compressedSize,
    const ZSTD_DDict* ddict
) {
    unsigned frameDictId = ZSTD_getDictID_fromFrame(compressed, compressedSize);

    if (frameDictId == 0) {
        // Фрейм сжат без словаря
        return true;
    }

    unsigned ddictId = ZSTD_getDictID_fromDDict(ddict);

    if (frameDictId != ddictId) {
        fprintf(stderr, "Dictionary mismatch: expected %u, got %u\n",
                frameDictId, ddictId);
        return false;
    }

    return true;
}
```

## Проблемы производительности

### Медленное сжатие

**Решения:**

1. Понизить уровень сжатия
2. Использовать многопоточное сжатие

```cpp
ZSTD_CCtx* cctx = ZSTD_createCCtx();
ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, 4);  // 4 потока
ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 3);
```

3. Кэшировать контексты

```cpp
// Пул контекстов
class ContextPool {
    std::vector<ZSTD_CCtx*> pool_;
public:
    ZSTD_CCtx* acquire() {
        if (!pool_.empty()) {
            auto ctx = pool_.back();
            pool_.pop_back();
            return ctx;
        }
        return ZSTD_createCCtx();
    }

    void release(ZSTD_CCtx* ctx) {
        ZSTD_CCtx_reset(ctx, ZSTD_reset_session_only);
        pool_.push_back(ctx);
    }
};
```

### Медленная распаковка

**Решения:**

1. Использовать контексты (не создавать каждый раз)
2. Предварительно определять размер

```cpp
// Записывать размер в заголовок
ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 1);

// При распаковке использовать ZSTD_getFrameContentSize
unsigned long long size = ZSTD_getFrameContentSize(data, dataSize);
// Выделить буфер точно под размер
```

## Отладка

### Вывод информации о фрейме

```cpp
void printFrameInfo(const void* data, size_t size) {
    ZSTD_frameHeader header;
    size_t result = ZSTD_getFrameHeader(&header, data, size);

    if (ZSTD_isError(result)) {
        printf("Invalid frame: %s\n", ZSTD_getErrorName(result));
        return;
    }

    printf("Frame Header:\n");
    printf("  Frame Content Size: %llu\n", header.frameContentSize);
    printf("  Window Size: %u\n", header.windowSize);
    printf("  Block Size Max: %u\n", header.blockSizeMax);
    printf("  Checksum: %s\n", header.checksumFlag ? "yes" : "no");
    printf("  Dict ID: %u\n", header.dictID);
}
```

### Проверка версии

```cpp
printf("Zstd version: %s (%u)\n",
       ZSTD_versionString(),
       ZSTD_versionNumber());
```

### Оценка использования памяти

```cpp
ZSTD_CCtx* cctx = ZSTD_createCCtx();
printf("CCtx size: %zu bytes\n", ZSTD_sizeof_CCtx(cctx));

ZSTD_DCtx* dctx = ZSTD_createDCtx();
printf("DCtx size: %zu bytes\n", ZSTD_sizeof_DCtx(dctx));
```

## Лучшие практики

1. **Всегда проверяйте ошибки** с помощью `ZSTD_isError()`
2. **Используйте `ZSTD_compressBound()`** для определения размера буфера
3. **Кэшируйте контексты** для многократных операций
4. **Включайте checksum** для важных данных
5. **Записывайте размер контента** в заголовок для быстрой распаковки
6. **Используйте словари** для однотипных данных малого размера
7. **Тестируйте с разными уровнями сжатия** для оптимального баланса
