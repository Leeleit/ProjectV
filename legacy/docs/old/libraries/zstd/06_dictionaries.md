# Словарное сжатие Zstd

🟡 **Уровень 2: Средний**

Использование предварительно обученных словарей для улучшения сжатия небольших данных.

## Проблема

Стандартное сжатие Zstd работает хорошо для больших файлов, но для небольших данных (< 64 KB) имеет ограничения:

- Zstd не знает структуру ваших данных
- Каждый файл сжимается независимо
- Overhead заголовков значителен для маленьких файлов

## Решение: Dictionary

Словарь — это предварительно обученный набор паттернов, который Zstd использует для улучшения сжатия.

```
Без словаря:
  Чанк 1: [ voxel_data... ] → Zstd → 4.2 KB
  Чанк 2: [ voxel_data... ] → Zstd → 4.1 KB
  Чанк 3: [ voxel_data... ] → Zstd → 4.3 KB

Со словарём:
  Dictionary: [ частые_паттерны ]
  Чанк 1 + Dict → Zstd → 2.8 KB (-33%)
  Чанк 2 + Dict → Zstd → 2.7 KB (-34%)
  Чанк 3 + Dict → Zstd → 2.9 KB (-33%)
```

## Обучение словаря

### ZDICT_trainFromBuffer

```cpp
#include <zdict.h>

size_t ZDICT_trainFromBuffer(
    void* dictBuffer,           // Буфер для словаря
    size_t dictBufferCapacity,  // Размер буфера (обычно 100 KB)
    const void* samplesBuffer,  // Все образцы конкатенированы
    const size_t* samplesSizes, // Размер каждого образца
    unsigned nbSamples          // Количество образцов
);
```

### Требования к образцам

- Минимум 100 образцов для хорошего качества
- Образцы должны быть репрезентативными
- Каждый образец ≥ 8 байт
- Общий объём ≥ 10 KB

### Пример обучения

```cpp
#include <zstd.h>
#include <zdict.h>
#include <vector>

class DictionaryTrainer {
public:
    // Добавление образца для обучения
    void addSample(const void* data, size_t size) {
        samples_.insert(
            samples_.end(),
            static_cast<const uint8_t*>(data),
            static_cast<const uint8_t*>(data) + size
        );
        sampleSizes_.push_back(size);
    }

    // Обучение словаря
    std::vector<uint8_t> train(size_t dictSize = 100 * 1024) {
        if (sampleSizes_.size() < 100) {
            throw std::runtime_error("Need at least 100 samples");
        }

        std::vector<uint8_t> dictionary(dictSize);

        size_t result = ZDICT_trainFromBuffer(
            dictionary.data(),
            dictionary.size(),
            samples_.data(),
            sampleSizes_.data(),
            static_cast<unsigned>(sampleSizes_.size())
        );

        if (ZDICT_isError(result)) {
            throw std::runtime_error(ZDICT_getErrorName(result));
        }

        dictionary.resize(result);
        return dictionary;
    }

private:
    std::vector<uint8_t> samples_;
    std::vector<size_t> sampleSizes_;
};
```

## Использование словаря

### Создание CDict и DDict

```cpp
// Словарь для сжатия
ZSTD_CDict* ZSTD_createCDict(
    const void* dictBuffer,
    size_t dictSize,
    int compressionLevel
);

// Словарь для распаковки
ZSTD_DDict* ZSTD_createDDict(
    const void* dictBuffer,
    size_t dictSize
);

// Освобождение
void ZSTD_freeCDict(ZSTD_CDict* cdict);
void ZSTD_freeDDict(ZSTD_DDict* ddict);
```

### Сжатие со словарём

```cpp
size_t ZSTD_compress_usingCDict(
    ZSTD_CCtx* cctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcSize,
    const ZSTD_CDict* cdict
);
```

### Распаковка со словарём

```cpp
size_t ZSTD_decompress_usingDDict(
    ZSTD_DCtx* dctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcSize,
    const ZSTD_DDict* ddict
);
```

## Полный пример

```cpp
#include <zstd.h>
#include <zdict.h>
#include <vector>
#include <fstream>

class DictionaryCompressor {
public:
    // Загрузка словаря из файла
    void loadDictionary(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        size_t size = file.tellg();
        file.seekg(0);

        dictBuffer_.resize(size);
        file.read(reinterpret_cast<char*>(dictBuffer_.data()), size);

        initDictionaries();
    }

    // Установка словаря из буфера
    void setDictionary(const void* data, size_t size) {
        dictBuffer_.assign(
            static_cast<const uint8_t*>(data),
            static_cast<const uint8_t*>(data) + size
        );
        initDictionaries();
    }

    ~DictionaryCompressor() {
        if (cdict_) ZSTD_freeCDict(cdict_);
        if (ddict_) ZSTD_freeDDict(ddict_);
        if (cctx_) ZSTD_freeCCtx(cctx_);
        if (dctx_) ZSTD_freeDCtx(dctx_);
    }

    // Сжатие со словарём
    std::vector<uint8_t> compress(
        const void* data, size_t size,
        int level = 3
    ) {
        if (!cdict_) {
            throw std::runtime_error("Dictionary not loaded");
        }

        // Пересоздание CDict при смене уровня
        if (level_ != level) {
            level_ = level;
            ZSTD_freeCDict(cdict_);
            cdict_ = ZSTD_createCDict(
                dictBuffer_.data(), dictBuffer_.size(), level
            );
        }

        size_t dstCapacity = ZSTD_compressBound(size);
        std::vector<uint8_t> compressed(dstCapacity);

        size_t result = ZSTD_compress_usingCDict(
            cctx_,
            compressed.data(), dstCapacity,
            data, size,
            cdict_
        );

        if (ZSTD_isError(result)) {
            throw std::runtime_error(ZSTD_getErrorName(result));
        }

        compressed.resize(result);
        return compressed;
    }

    // Распаковка со словарём
    std::vector<uint8_t> decompress(
        const void* data, size_t size
    ) {
        if (!ddict_) {
            throw std::runtime_error("Dictionary not loaded");
        }

        unsigned long long contentSize = ZSTD_getFrameContentSize(data, size);

        if (contentSize == ZSTD_CONTENTSIZE_ERROR) {
            throw std::runtime_error("Invalid Zstd frame");
        }

        if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
            contentSize = size * 10;
        }

        std::vector<uint8_t> decompressed(contentSize);

        size_t result = ZSTD_decompress_usingDDict(
            dctx_,
            decompressed.data(), contentSize,
            data, size,
            ddict_
        );

        if (ZSTD_isError(result)) {
            throw std::runtime_error(ZSTD_getErrorName(result));
        }

        decompressed.resize(result);
        return decompressed;
    }

    // Получение ID словаря
    unsigned getDictId() const {
        if (!ddict_) return 0;
        return ZSTD_getDictID_fromDDict(ddict_);
    }

private:
    std::vector<uint8_t> dictBuffer_;
    ZSTD_CDict* cdict_ = nullptr;
    ZSTD_DDict* ddict_ = nullptr;
    ZSTD_CCtx* cctx_ = nullptr;
    ZSTD_DCtx* dctx_ = nullptr;
    int level_ = 3;

    void initDictionaries() {
        if (!cctx_) cctx_ = ZSTD_createCCtx();
        if (!dctx_) dctx_ = ZSTD_createDCtx();

        if (cdict_) ZSTD_freeCDict(cdict_);
        if (ddict_) ZSTD_freeDDict(ddict_);

        cdict_ = ZSTD_createCDict(
            dictBuffer_.data(), dictBuffer_.size(), level_
        );
        ddict_ = ZSTD_createDDict(dictBuffer_.data(), dictBuffer_.size());

        if (!cdict_ || !ddict_) {
            throw std::runtime_error("Failed to create dictionaries");
        }
    }
};
```

## Идентификация словаря

### Получение ID из словаря

```cpp
unsigned ZSTD_getDictID_fromDict(
    const void* dict, size_t dictSize
);
```

### Получение ID из сжатого фрейма

```cpp
unsigned ZSTD_getDictID_fromFrame(
    const void* src, size_t srcSize
);
```

### Получение ID из DDict

```cpp
unsigned ZSTD_getDictID_fromDDict(const ZSTD_DDict* ddict);
```

### Проверка соответствия словаря

```cpp
bool checkDictionaryMatch(
    const ZSTD_DDict* ddict,
    const void* compressed, size_t compressedSize
) {
    unsigned frameDictId = ZSTD_getDictID_fromFrame(compressed, compressedSize);
    if (frameDictId == 0) {
        // Фрейм сжат без словаря
        return true;
    }

    unsigned ddictId = ZSTD_getDictID_fromDDict(ddict);
    return frameDictId == ddictId;
}
```

## Размер словаря

| Размер    | Применение                        |
|-----------|-----------------------------------|
| 1–10 KB   | Простые структуры данных          |
| 10–50 KB  | Типичные данные                   |
| 50–100 KB | Сложные структуры (рекомендуется) |
| 100 KB+   | Максимальное качество             |

Больший словарь не всегда лучше. Оптимальный размер зависит от данных.

## Когда использовать словари

**Рекомендуется:**

- Множество однотипных данных (чанки, записи, логи)
- Размер данных < 64 KB
- Данные с повторяющейся структурой
- Офлайн-генерация словаря

**Не рекомендуется:**

- Единичные файлы
- Разнородные данные
- Случайные данные
- Данные уже хорошего размера (> 1 MB)

## Сравнение производительности

| Метод                | Размер 16 KB данных | Степень сжатия |
|----------------------|---------------------|----------------|
| Без сжатия           | 16 KB               | 100%           |
| Zstd level 3         | 5.2 KB              | 32.5%          |
| Zstd level 3 + Dict  | 3.1 KB              | 19.4%          |
| Zstd level 10        | 4.5 KB              | 28.1%          |
| Zstd level 10 + Dict | 2.8 KB              | 17.5%          |

## Ошибки обучения

### ZDICT_error_dstSize_tooSmall

Буфер для словаря слишком мал. Увеличьте `dictBufferCapacity`.

### ZDICT_error_srcSize_wrong

Образцы слишком малы. Каждый образец должен быть ≥ 8 байт.

### ZDICT_error_dictionaryCreation_failed

Недостаточно образцов или они нерепрезентативны. Добавьте больше образцов.

## Рекомендации

1. **Обучение**: Проводите офлайн на репрезентативных данных
2. **Размер**: Начните с 100 KB, экспериментируйте
3. **Образцы**: Минимум 100–1000 образцов
4. **Хранение**: Словарь должен быть доступен при распаковке
5. **Версионирование**: Включайте ID словаря в метаданные файла
