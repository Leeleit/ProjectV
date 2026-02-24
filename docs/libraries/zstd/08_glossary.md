# Глоссарий Zstd

Ключевые термины и сокращения библиотеки Zstandard.

## Основные термины

### Frame (Кадр)

Базовая единица данных Zstd. Каждый вызов `ZSTD_compress()` создаёт один Frame. Frame содержит:

- Magic number (`0xFD2FB528`)
- Frame Header с метаданными
- Один или несколько сжатых блоков
- Опциональный checksum (xxHash64)

Несколько Frame можно конкатенировать в один поток — декомпрессор автоматически распознаёт и обрабатывает их.

### Block (Блок)

Единица внутри Frame, обрабатываемая независимо. Максимальный размер — `ZSTD_BLOCKSIZE_MAX` (128 KB).

Типы блоков:

| Тип                  | Описание                                   |
|----------------------|--------------------------------------------|
| **Raw block**        | Несжатые данные (если сжатие невыгодно)    |
| **RLE block**        | Все байты одинаковые (Run-Length Encoding) |
| **Compressed block** | LZ + FSE/Huffman кодирование               |

### CCtx (Compression Context)

Контекст компрессора. Содержит внутреннее состояние, таблицы и рабочую память.

```cpp
ZSTD_CCtx* cctx = ZSTD_createCCtx();
// ... использование ...
ZSTD_CCtx_reset(cctx, ZSTD_reset_session_only);  // Сброс между вызовами
ZSTD_freeCCtx(cctx);
```

Создание контекста — дорогая операция. Рекомендуется переиспользовать.

### DCtx (Decompression Context)

Контекст декомпрессора. Аналогичен CCtx, но для распаковки.

```cpp
ZSTD_DCtx* dctx = ZSTD_createDCtx();
// ... использование ...
ZSTD_freeDCtx(dctx);
```

## Словарное сжатие

### CDict (Compression Dictionary)

Предварительно обработанный словарь для сжатия. Создаётся один раз, используется многократно.

```cpp
ZSTD_CDict* cdict = ZSTD_createCDict(dictBuffer, dictSize, compressionLevel);
ZSTD_compress_usingCDict(cctx, dst, dstCap, src, srcSize, cdict);
ZSTD_freeCDict(cdict);
```

### DDict (Decompression Dictionary)

Словарь для распаковки.

```cpp
ZSTD_DDict* ddict = ZSTD_createDDict(dictBuffer, dictSize);
ZSTD_decompress_usingDDict(dctx, dst, dstCap, src, srcSize, ddict);
ZSTD_freeDDict(ddict);
```

### Dictionary ID

Уникальный идентификатор словаря (4 байта) в Frame Header. Позволяет автоматически определить нужный словарь.

```cpp
unsigned dictId = ZSTD_getDictID_fromDDict(ddict);
unsigned frameId = ZSTD_getDictID_fromFrame(compressedData, compressedSize);
```

## Потоковый API

### ZSTD_inBuffer

Структура входного буфера для потокового API.

```cpp
typedef struct {
    const void* src;   // Указатель на данные
    size_t size;       // Полный размер буфера
    size_t pos;        // Текущая позиция чтения (обновляется Zstd)
} ZSTD_inBuffer;
```

### ZSTD_outBuffer

Структура выходного буфера для потокового API.

```cpp
typedef struct {
    void* dst;   // Указатель на буфер назначения
    size_t size; // Полный размер буфера
    size_t pos;  // Текущая позиция записи (обновляется Zstd)
} ZSTD_outBuffer;
```

### EndDirective (ZSTD_EndDirective)

Управление завершением потока при `ZSTD_compressStream2`:

| Значение          | Описание                                       |
|-------------------|------------------------------------------------|
| `ZSTD_e_continue` | Продолжать накапливать данные                  |
| `ZSTD_e_flush`    | Вытолкнуть накопленные данные в выходной буфер |
| `ZSTD_e_end`      | Завершить Frame                                |

## Параметры компрессора

### Compression Level (Уровень сжатия)

Целое число, управляющее компромиссом скорость/соотношение:

- Диапазон: `ZSTD_minCLevel()` до `ZSTD_maxCLevel()` (обычно от -131072 до 22)
- `ZSTD_CLEVEL_DEFAULT` = 3

### Window Size (windowLog)

Log2 размера поискового окна. Больший размер → лучшее сжатие → больше памяти.

```cpp
ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog, 17);  // 2^17 = 128 KB
```

### Checksum (checksumFlag)

Включение xxHash64 контрольной суммы для проверки целостности.

```cpp
ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
```

### ZSTD_reset_directive

Управление сбросом контекста:

| Значение                            | Описание                                  |
|-------------------------------------|-------------------------------------------|
| `ZSTD_reset_session_only`           | Сброс только текущего сеанса              |
| `ZSTD_reset_parameters`             | Сброс параметров к значениям по умолчанию |
| `ZSTD_reset_session_and_parameters` | Полный сброс                              |

## Коды ошибок

### ZSTD_isError

Макрос для проверки результата на ошибку.

```cpp
size_t result = ZSTD_compress(...);
if (ZSTD_isError(result)) {
    fprintf(stderr, "Error: %s\n", ZSTD_getErrorName(result));
}
```

### Специальные возвращаемые значения

| Константа                  | Значение       | Описание                             |
|----------------------------|----------------|--------------------------------------|
| `ZSTD_CONTENTSIZE_UNKNOWN` | UINT64_MAX - 1 | Размер неизвестен (потоковое сжатие) |
| `ZSTD_CONTENTSIZE_ERROR`   | UINT64_MAX - 2 | Не является Zstd frame               |

## Алгоритмы

### LZ (Lempel-Ziv)

Алгоритм поиска повторяющихся последовательностей. Заменяет повторяющиеся фрагменты ссылками на предыдущие вхождения.

### FSE (Finite State Entropy)

Алгоритм энтропийного кодирования на основе ANS (Asymmetric Numeral Systems). Разработан Яном Колле. Преимущества перед
Huffman:

- Лучшее сжатие для неравномерного распределения
- Высокая скорость декодирования
- Эффективность на небольших блоках

### RLE (Run-Length Encoding)

Кодирование серий одинаковых значений. Используется для блоков, где все байты одинаковы.

## Сокращения

| Сокращение | Расшифровка                | Описание                          |
|------------|----------------------------|-----------------------------------|
| **FSE**    | Finite State Entropy       | Алгоритм энтропийного кодирования |
| **LZ**     | Lempel-Ziv                 | Алгоритм поиска повторов          |
| **RLE**    | Run-Length Encoding        | Кодирование серий                 |
| **CCtx**   | Compression Context        | Контекст компрессора              |
| **DCtx**   | Decompression Context      | Контекст декомпрессора            |
| **CDict**  | Compression Dictionary     | Словарь для сжатия                |
| **DDict**  | Decompression Dictionary   | Словарь для распаковки            |
| **ANS**    | Asymmetric Numeral Systems | Математическая основа FSE         |

## Константы

| Константа             | Значение   | Описание                    |
|-----------------------|------------|-----------------------------|
| `ZSTD_MAGICNUMBER`    | 0xFD2FB528 | Magic number Zstd frame     |
| `ZSTD_BLOCKSIZE_MAX`  | 128 KB     | Максимальный размер блока   |
| `ZSTD_CLEVEL_DEFAULT` | 3          | Уровень сжатия по умолчанию |
| `ZSTD_MAX_CLEVEL`     | 22         | Максимальный уровень сжатия |
