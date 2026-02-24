# Интеграция Zstd в ProjectV

Специфика интеграции Zstd в воксельный движок ProjectV.

## CMake конфигурация ProjectV

### Подключение подмодуля

```cmake
# CMakeLists.txt ProjectV
option(PROJECTV_USE_ZSTD "Enable Zstd compression for voxel data" ON)

if(PROJECTV_USE_ZSTD)
    # Добавление подмодуля Zstd
    add_subdirectory(external/zstd/build/cmake)

    # Настройка опций
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Don't build Zstd programs")
    set(ZSTD_BUILD_SHARED ON CACHE BOOL "Build shared library")
    set(ZSTD_BUILD_STATIC OFF CACHE BOOL "Don't build static library")

    # Создание псевдоцели
    add_library(zstd::zstd ALIAS libzstd_shared)

    # Подключение к основному проекту
    target_link_libraries(ProjectV PRIVATE zstd::zstd)
    target_include_directories(ProjectV PRIVATE external/zstd/lib)

    # Определение для условной компиляции
    target_compile_definitions(ProjectV PRIVATE PROJECTV_ZSTD_ENABLED)
endif()
```

### Альтернатива: системная установка

```cmake
find_package(zstd 1.4.0 QUIET)

if(zstd_FOUND)
    message(STATUS "Found system Zstd: ${zstd_VERSION}")
    target_link_libraries(ProjectV PRIVATE zstd::zstd)
elseif(PROJECTV_USE_ZSTD)
    message(STATUS "Zstd not found, using submodule")
    add_subdirectory(external/zstd/build/cmake)
    target_link_libraries(ProjectV PRIVATE libzstd_shared)
endif()
```

## Структура проекта

```
ProjectV/
├── external/
│   └── zstd/                    # Подмодуль Git
├── src/
│   ├── compression/
│   │   ├── zstd_compressor.hpp  # Основной класс
│   │   ├── zstd_compressor.cpp
│   │   ├── chunk_compressor.hpp # Специализация для чанков
│   │   ├── chunk_compressor.cpp
│   │   ├── zstd_context_pool.hpp
│   │   └── zstd_context_pool.cpp
│   └── voxel/
│       ├── chunk.hpp
│       └── chunk.cpp
└── docs/
    └── libraries/
        └── zstd/
```

## Класс ZstdCompressor для ProjectV

### Заголовочный файл

```cpp
// src/compression/zstd_compressor.hpp
#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <stdexcept>

namespace ProjectV::Compression {

enum class ZstdLevel {
    Fast = 1,
    Balanced = 3,
    High = 10,
    Max = 22
};

class ZstdException : public std::runtime_error {
public:
    explicit ZstdException(const std::string& message)
        : std::runtime_error(message) {}
};

class ZstdCompressor {
public:
    explicit ZstdCompressor(ZstdLevel level = ZstdLevel::Balanced);
    ~ZstdCompressor();

    // Запрет копирования
    ZstdCompressor(const ZstdCompressor&) = delete;
    ZstdCompressor& operator=(const ZstdCompressor&) = delete;

    // Разрешение перемещения
    ZstdCompressor(ZstdCompressor&&) noexcept;
    ZstdCompressor& operator=(ZstdCompressor&&) noexcept;

    // Сжатие
    std::vector<uint8_t> compress(const void* data, size_t size);
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);

    // Распаковка
    std::vector<uint8_t> decompress(const void* data, size_t size);
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);

    // Распаковка с известным размером
    std::vector<uint8_t> decompress(
        const void* data, size_t size, size_t expectedSize);

    // Утилиты
    static size_t getCompressedBound(size_t srcSize);
    static size_t getFrameContentSize(const void* data, size_t size);

    // Настройка
    void setLevel(ZstdLevel level);
    ZstdLevel getLevel() const { return level_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ZstdLevel level_;
};

} // namespace ProjectV::Compression
```

### Реализация

```cpp
// src/compression/zstd_compressor.cpp
#include "zstd_compressor.hpp"
#include <zstd.h>

namespace ProjectV::Compression {

struct ZstdCompressor::Impl {
    ZSTD_CCtx* cctx = nullptr;
    ZSTD_DCtx* dctx = nullptr;

    Impl() {
        cctx = ZSTD_createCCtx();
        dctx = ZSTD_createDCtx();

        if (!cctx || !dctx) {
            if (cctx) ZSTD_freeCCtx(cctx);
            if (dctx) ZSTD_freeDCtx(dctx);
            throw ZstdException("Failed to create Zstd contexts");
        }
    }

    ~Impl() {
        if (cctx) ZSTD_freeCCtx(cctx);
        if (dctx) ZSTD_freeDCtx(dctx);
    }
};

ZstdCompressor::ZstdCompressor(ZstdLevel level)
    : impl_(std::make_unique<Impl>())
    , level_(level)
{
    ZSTD_CCtx_setParameter(impl_->cctx, ZSTD_c_compressionLevel,
                           static_cast<int>(level));
}

ZstdCompressor::~ZstdCompressor() = default;

ZstdCompressor::ZstdCompressor(ZstdCompressor&&) noexcept = default;
ZstdCompressor& ZstdCompressor::operator=(ZstdCompressor&&) noexcept = default;

std::vector<uint8_t> ZstdCompressor::compress(const void* data, size_t size) {
    size_t dstCapacity = ZSTD_compressBound(size);
    std::vector<uint8_t> compressed(dstCapacity);

    size_t compressedSize = ZSTD_compressCCtx(
        impl_->cctx,
        compressed.data(), dstCapacity,
        data, size,
        static_cast<int>(level_)
    );

    if (ZSTD_isError(compressedSize)) {
        throw ZstdException("Compression failed: " +
                           std::string(ZSTD_getErrorName(compressedSize)));
    }

    compressed.resize(compressedSize);
    return compressed;
}

std::vector<uint8_t> ZstdCompressor::compress(const std::vector<uint8_t>& data) {
    return compress(data.data(), data.size());
}

std::vector<uint8_t> ZstdCompressor::decompress(const void* data, size_t size) {
    unsigned long long contentSize = ZSTD_getFrameContentSize(data, size);

    if (contentSize == ZSTD_CONTENTSIZE_ERROR) {
        throw ZstdException("Not a valid Zstd frame");
    }

    if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        contentSize = size * 10;  // Консервативная оценка
    }

    return decompress(data, size, static_cast<size_t>(contentSize));
}

std::vector<uint8_t> ZstdCompressor::decompress(
    const void* data, size_t size, size_t expectedSize
) {
    std::vector<uint8_t> decompressed(expectedSize);

    size_t decompressedSize = ZSTD_decompressDCtx(
        impl_->dctx,
        decompressed.data(), expectedSize,
        data, size
    );

    if (ZSTD_isError(decompressedSize)) {
        throw ZstdException("Decompression failed: " +
                           std::string(ZSTD_getErrorName(decompressedSize)));
    }

    decompressed.resize(decompressedSize);
    return decompressed;
}

std::vector<uint8_t> ZstdCompressor::decompress(const std::vector<uint8_t>& data) {
    return decompress(data.data(), data.size());
}

size_t ZstdCompressor::getCompressedBound(size_t srcSize) {
    return ZSTD_compressBound(srcSize);
}

size_t ZstdCompressor::getFrameContentSize(const void* data, size_t size) {
    unsigned long long contentSize = ZSTD_getFrameContentSize(data, size);
    return static_cast<size_t>(contentSize);
}

void ZstdCompressor::setLevel(ZstdLevel level) {
    level_ = level;
    ZSTD_CCtx_setParameter(impl_->cctx, ZSTD_c_compressionLevel,
                           static_cast<int>(level));
}

} // namespace ProjectV::Compression
```

## Кэширование контекстов

```cpp
// src/compression/zstd_context_pool.hpp
#pragma once

#include <zstd.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

namespace ProjectV::Compression {

class ZstdContextPool {
public:
    ZstdContextPool() = default;

    ~ZstdContextPool() {
        std::lock_guard lock(mutex_);
        for (auto& [level, ctx] : compressionContexts_) {
            ZSTD_freeCCtx(ctx);
        }
        for (auto* ctx : decompressionContexts_) {
            ZSTD_freeDCtx(ctx);
        }
    }

    ZSTD_CCtx* getCompressionContext(int level) {
        std::lock_guard lock(mutex_);

        auto it = compressionContexts_.find(level);
        if (it != compressionContexts_.end()) {
            return it->second;
        }

        ZSTD_CCtx* ctx = ZSTD_createCCtx();
        if (!ctx) {
            throw ZstdException("Failed to create compression context");
        }

        ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, level);
        compressionContexts_[level] = ctx;
        return ctx;
    }

    ZSTD_DCtx* getDecompressionContext() {
        std::lock_guard lock(mutex_);

        if (!decompressionContexts_.empty()) {
            auto* ctx = decompressionContexts_.back();
            decompressionContexts_.pop_back();
            return ctx;
        }

        ZSTD_DCtx* ctx = ZSTD_createDCtx();
        if (!ctx) {
            throw ZstdException("Failed to create decompression context");
        }
        return ctx;
    }

    void returnDecompressionContext(ZSTD_DCtx* ctx) {
        std::lock_guard lock(mutex_);
        decompressionContexts_.push_back(ctx);
    }

private:
    std::unordered_map<int, ZSTD_CCtx*> compressionContexts_;
    std::vector<ZSTD_DCtx*> decompressionContexts_;
    std::mutex mutex_;
};

} // namespace ProjectV::Compression
```

## Выбор уровня сжатия

| Сценарий              | Уровень | Обоснование               |
|-----------------------|---------|---------------------------|
| Авто-сохранение       | 1       | Минимальная задержка      |
| Загрузка мира         | 3       | Баланс скорости и размера |
| Сетевая синхронизация | 1       | Критична задержка         |
| Экспорт уровня        | 15      | Размер важнее скорости    |
| Кэш LOD               | 7       | Компромисс                |

## Константы ProjectV

```cpp
// src/compression/constants.hpp
#pragma once

#include <cstddef>

namespace ProjectV::Compression {

// Размеры чанков
constexpr size_t CHUNK_SIZE = 32;
constexpr size_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr size_t CHUNK_DATA_SIZE = CHUNK_VOLUME * sizeof(uint16_t);

// Параметры сжатия
constexpr int DEFAULT_COMPRESSION_LEVEL = 3;
constexpr int FAST_COMPRESSION_LEVEL = 1;
constexpr int HIGH_COMPRESSION_LEVEL = 10;

// Лимиты
constexpr size_t MAX_COMPRESSED_CHUNK_SIZE = CHUNK_DATA_SIZE * 2;
constexpr size_t MIN_SAMPLES_FOR_DICTIONARY = 100;

} // namespace ProjectV::Compression
```
