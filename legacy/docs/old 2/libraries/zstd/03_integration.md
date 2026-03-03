# Интеграция Zstd

🟢 **Уровень 1: Базовый**

Настройка CMake, сборка и базовые паттерны использования.

## CMake конфигурация

### Вариант 1: Подмодуль Git (рекомендуется)

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(MyProject)

# Добавление подмодуля Zstd
add_subdirectory(external/zstd/build/cmake)

# Настройка опций Zstd
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Don't build Zstd programs")
set(ZSTD_BUILD_SHARED ON CACHE BOOL "Build shared library")
set(ZSTD_BUILD_STATIC OFF CACHE BOOL "Don't build static library")

# Подключение к проекту
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE libzstd_shared)
target_include_directories(my_app PRIVATE external/zstd/lib)
```

### Вариант 2: Поиск установленной библиотеки

```cmake
# Поиск системной установки Zstd
find_package(zstd 1.4.0 REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE zstd::libzstd_shared)
```

### Вариант 3: FetchContent (CMake 3.14+)

```cmake
include(FetchContent)

FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.5
)

FetchContent_MakeAvailable(zstd)

# Подключение
target_link_libraries(my_app PRIVATE libzstd_static)
```

## Структура проекта

```
project/
├── CMakeLists.txt
├── external/
│   └── zstd/              # Подмодуль Git
├── src/
│   ├── main.cpp
│   └── compression/
│       ├── zstd_wrapper.hpp
│       └── zstd_wrapper.cpp
└── tests/
    └── compression_test.cpp
```

## Базовая C++ обёртка

### Заголовочный файл

```cpp
// compression/zstd_wrapper.hpp
#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <memory>

namespace compression {

enum class ZstdLevel {
    Fastest = 1,
    Fast = 2,
    Default = 3,
    Balanced = 5,
    High = 10,
    Maximum = 22
};

class ZstdException : public std::runtime_error {
public:
    explicit ZstdException(const std::string& message)
        : std::runtime_error(message) {}
};

class ZstdCompressor {
public:
    explicit ZstdCompressor(ZstdLevel level = ZstdLevel::Default);
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

    // Утилиты
    static size_t getCompressedBound(size_t srcSize);
    static size_t getFrameContentSize(const void* data, size_t size);

    // Настройка уровня сжатия
    void setLevel(ZstdLevel level);
    ZstdLevel getLevel() const { return level_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ZstdLevel level_;
};

} // namespace compression
```

### Реализация

```cpp
// compression/zstd_wrapper.cpp
#include "zstd_wrapper.hpp"
#include <zstd.h>
#include <cstring>

namespace compression {

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
        // Консервативная оценка для неизвестного размера
        contentSize = size * 10;
    }

    std::vector<uint8_t> decompressed(contentSize);

    size_t decompressedSize = ZSTD_decompressDCtx(
        impl_->dctx,
        decompressed.data(), contentSize,
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

} // namespace compression
```

## Использование

```cpp
#include "compression/zstd_wrapper.hpp"
#include <iostream>
#include <fstream>

int main() {
    // Создание компрессора
    compression::ZstdCompressor compressor(compression::ZstdLevel::Balanced);
    
    // Чтение файла
    std::ifstream file("data.bin", std::ios::binary | std::ios::ate);
    size_t fileSize = file.tellg();
    file.seekg(0);
    
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    
    // Сжатие
    auto compressed = compressor.compress(data);
    
    std::cout << "Original size: " << data.size() << " bytes" << std::endl;
    std::cout << "Compressed size: " << compressed.size() << " bytes" << std::endl;
    std::cout << "Ratio: " 
              << (100.0 * compressed.size() / data.size()) << "%" << std::endl;
    
    // Распаковка
    auto decompressed = compressor.decompress(compressed);
    
    // Проверка
    if (data == decompressed) {
        std::cout << "Decompression successful!" << std::endl;
    }
    
    return 0;
}
```

## Кэширование контекстов

Для высокопроизводительных сценариев кэшируйте экземпляры `ZstdCompressor`:

```cpp
class ZstdCompressorPool {
public:
    ZstdCompressor& getCompressor(ZstdLevel level = ZstdLevel::Default) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = compressors_.find(level);
        if (it != compressors_.end()) {
            return *it->second;
        }
        
        auto [inserted, _] = compressors_.emplace(
            level, std::make_unique<ZstdCompressor>(level)
        );
        return *inserted->second;
    }

private:
    std::unordered_map<ZstdLevel, std::unique_ptr<ZstdCompressor>> compressors_;
    std::mutex mutex_;
};
```

## Многопоточное использование

Zstd контексты не потокобезопасны. Для многопоточного использования:

```cpp
// Вариант 1: Один компрессор на поток
thread_local compression::ZstdCompressor tlsCompressor;

void compressInThread(const std::vector<uint8_t>& data) {
    auto compressed = tlsCompressor.compress(data);
    // ...
}

// Вариант 2: Пул компрессоров
class ThreadSafeZstdPool {
public:
    std::unique_ptr<compression::ZstdCompressor> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!pool_.empty()) {
            auto compressor = std::move(pool_.back());
            pool_.pop_back();
            return compressor;
        }
        
        return std::make_unique<compression::ZstdCompressor>();
    }
    
    void release(std::unique_ptr<compression::ZstdCompressor> compressor) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::move(compressor));
    }

private:
    std::vector<std::unique_ptr<compression::ZstdCompressor>> pool_;
    std::mutex mutex_;
};
```

## Константы и лимиты

```cpp
// Максимальный размер блока
constexpr size_t ZSTD_BLOCKSIZE_MAX = 128 * 1024;  // 128 KB

// Максимальный уровень сжатия
constexpr int ZSTD_MAX_CLEVEL = 22;

// Magic number
constexpr uint32_t ZSTD_MAGICNUMBER = 0xFD2FB528;

// Специальные значения размера контента
constexpr unsigned long long ZSTD_CONTENTSIZE_UNKNOWN = UINT64_MAX - 1;
constexpr unsigned long long ZSTD_CONTENTSIZE_ERROR = UINT64_MAX - 2;