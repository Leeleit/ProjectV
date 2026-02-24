# Паттерны Zstd в ProjectV

Оптимизации сжатия для воксельных данных в ProjectV.

## Сжатие воксельных чанков

### Bit-Packing вокселей

Наивное представление вокселя неэффективно:

```cpp
// Наивное представление — 18 байт на воксель
struct VoxelNaive {
    uint32_t type;     // 4 байта (нужно 8-10 бит)
    glm::vec3 color;   // 12 байт (нужно 12-16 бит)
    uint8_t light;     // 1 байт (нужно 4 бита)
    uint8_t flags;     // 1 байт (нужно 4 бита)
};
// 32³ × 18 = 589 KB на чанк
```

Оптимизированное представление:

```cpp
// Оптимизированное — 2 байта на воксель
struct VoxelPacked {
    uint16_t data;

    // Layout:
    // [15:12] - Material (16 типов)
    // [11:8]  - Light level (16 уровней)
    // [7:0]   - Voxel type (256 типов)

    uint8_t getType() const { return data & 0xFF; }
    uint8_t getLight() const { return (data >> 8) & 0xF; }
    uint8_t getMaterial() const { return (data >> 12) & 0xF; }

    void setType(uint8_t type) { data = (data & 0xFF00) | (type & 0xFF); }
    void setLight(uint8_t light) { data = (data & 0xF0FF) | ((light & 0xF) << 8); }
    void setMaterial(uint8_t mat) { data = (data & 0x0FFF) | ((mat & 0xF) << 12); }
};
// 32³ × 2 = 65 KB на чанк (в 9 раз меньше)
```

### RLE для однородных областей

```cpp
struct RLEEntry {
    uint16_t count;      // Количество повторений
    uint16_t voxelData;  // Упакованные данные вокселя
};

struct ChunkCompressed {
    uint32_t magic;           // 'VOX1'
    uint16_t format;          // 0 = raw, 1 = RLE
    uint16_t bitsPerVoxel;    // 16 or 32
    std::vector<RLEEntry> entries;

    std::vector<uint16_t> unpack() const {
        std::vector<uint16_t> voxels;
        voxels.reserve(32 * 32 * 32);

        for (const auto& entry : entries) {
            for (uint16_t i = 0; i < entry.count; ++i) {
                voxels.push_back(entry.voxelData);
            }
        }
        return voxels;
    }
};
```

### Класс ChunkCompressor

```cpp
// src/compression/chunk_compressor.hpp
#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace ProjectV::Compression {

struct ChunkHeader {
    uint32_t magic = 0x564F5821;  // "VOX!"
    uint32_t version = 1;
    uint32_t uncompressedSize;
    uint32_t compressedSize;
    uint64_t checksum;
    uint8_t compressionLevel;
    uint8_t flags;
    uint16_t reserved;
};

static_assert(sizeof(ChunkHeader) == 32, "ChunkHeader must be 32 bytes");

class ChunkCompressor {
public:
    ChunkCompressor();
    ~ChunkCompressor();

    // Сжатие чанка с заголовком
    std::vector<uint8_t> compressChunk(
        const uint16_t* voxels,
        size_t count,
        int level = 3
    );

    // Распаковка чанка
    std::vector<uint16_t> decompressChunk(
        const uint8_t* data,
        size_t size
    );

    // Валидация
    static bool validateChunk(const uint8_t* data, size_t size);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    static uint64_t calculateChecksum(const void* data, size_t size);
};

} // namespace ProjectV::Compression
```

## Словарное сжатие для чанков

### Генерация словаря

```cpp
class VoxelDictionaryGenerator {
public:
    void collectSample(const std::vector<uint16_t>& chunkData) {
        samples_.insert(
            samples_.end(),
            reinterpret_cast<const uint8_t*>(chunkData.data()),
            reinterpret_cast<const uint8_t*>(chunkData.data() + chunkData.size())
        );
        sampleSizes_.push_back(chunkData.size() * sizeof(uint16_t));
    }

    std::vector<uint8_t> train(size_t dictSize = 100 * 1024) {
        if (sampleSizes_.size() < 100) {
            throw std::runtime_error("Need at least 100 samples");
        }

        std::vector<uint8_t> dictionary(dictSize);

        size_t result = ZDICT_trainFromBuffer(
            dictionary.data(), dictionary.size(),
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

### Использование словаря

```cpp
class DictionaryChunkCompressor {
public:
    void loadDictionary(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        size_t size = file.tellg();
        file.seekg(0);

        dictBuffer_.resize(size);
        file.read(reinterpret_cast<char*>(dictBuffer_.data()), size);

        cdict_ = ZSTD_createCDict(dictBuffer_.data(), dictBuffer_.size(), 3);
        ddict_ = ZSTD_createDDict(dictBuffer_.data(), dictBuffer_.size());
        cctx_ = ZSTD_createCCtx();
        dctx_ = ZSTD_createDCtx();
    }

    std::vector<uint8_t> compress(const uint16_t* voxels, size_t count) {
        size_t srcSize = count * sizeof(uint16_t);
        size_t dstCapacity = ZSTD_compressBound(srcSize);
        std::vector<uint8_t> compressed(dstCapacity);

        size_t result = ZSTD_compress_usingCDict(
            cctx_,
            compressed.data(), dstCapacity,
            voxels, srcSize,
            cdict_
        );

        if (ZSTD_isError(result)) {
            throw std::runtime_error(ZSTD_getErrorName(result));
        }

        compressed.resize(result);
        return compressed;
    }

    std::vector<uint16_t> decompress(const uint8_t* data, size_t size) {
        size_t contentSize = ZSTD_getFrameContentSize(data, size);
        std::vector<uint16_t> voxels(contentSize / sizeof(uint16_t));

        size_t result = ZSTD_decompress_usingDDict(
            dctx_,
            voxels.data(), contentSize,
            data, size,
            ddict_
        );

        if (ZSTD_isError(result)) {
            throw std::runtime_error(ZSTD_getErrorName(result));
        }

        return voxels;
    }

    ~DictionaryChunkCompressor() {
        if (cdict_) ZSTD_freeCDict(cdict_);
        if (ddict_) ZSTD_freeDDict(ddict_);
        if (cctx_) ZSTD_freeCCtx(cctx_);
        if (dctx_) ZSTD_freeDCtx(dctx_);
    }

private:
    std::vector<uint8_t> dictBuffer_;
    ZSTD_CDict* cdict_ = nullptr;
    ZSTD_DDict* ddict_ = nullptr;
    ZSTD_CCtx* cctx_ = nullptr;
    ZSTD_DCtx* dctx_ = nullptr;
};
```

## Memory-Mapped I/O

### Кроссплатформенная абстракция

```cpp
// src/io/memory_mapped_file.hpp
#pragma once

#include <string>
#include <cstdint>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace ProjectV::IO {

class MemoryMappedFile {
public:
    enum class AccessMode { ReadOnly, ReadWrite, WriteCopy };

    MemoryMappedFile() = default;
    ~MemoryMappedFile() { unmap(); }

    bool open(const std::string& path, AccessMode mode = AccessMode::ReadOnly) {
#ifdef _WIN32
        DWORD access = (mode == AccessMode::ReadWrite) ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
        DWORD shareMode = (mode == AccessMode::ReadWrite) ? 0 : FILE_SHARE_READ;

        handle_ = CreateFileA(path.c_str(), access, shareMode, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER fileSize;
        GetFileSizeEx(handle_, &fileSize);
        size_ = fileSize.QuadPart;

        DWORD protect = (mode == AccessMode::ReadWrite) ? PAGE_READWRITE : PAGE_READONLY;
        mapping_ = CreateFileMappingA(handle_, nullptr, protect, 0, 0, nullptr);
        if (!mapping_) {
            CloseHandle(handle_);
            return false;
        }

        DWORD mapAccess = (mode == AccessMode::ReadWrite) ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
        if (mode == AccessMode::WriteCopy) mapAccess = FILE_MAP_COPY;

        data_ = MapViewOfFile(mapping_, mapAccess, 0, 0, size_);
        if (!data_) {
            CloseHandle(mapping_);
            CloseHandle(handle_);
            return false;
        }
#else
        int flags = (mode == AccessMode::ReadWrite) ? O_RDWR : O_RDONLY;
        fd_ = ::open(path.c_str(), flags);
        if (fd_ == -1) return false;

        struct stat sb;
        if (fstat(fd_, &sb) == -1) {
            ::close(fd_);
            return false;
        }
        size_ = sb.st_size;

        int prot = PROT_READ;
        if (mode == AccessMode::ReadWrite) prot |= PROT_WRITE;

        int mapFlags = MAP_PRIVATE;
        if (mode == AccessMode::ReadWrite) mapFlags = MAP_SHARED;

        data_ = mmap(nullptr, size_, prot, mapFlags, fd_, 0);
        if (data_ == MAP_FAILED) {
            ::close(fd_);
            data_ = nullptr;
            return false;
        }
#endif
        return true;
    }

    void unmap() {
        if (!data_) return;
#ifdef _WIN32
        UnmapViewOfFile(data_);
        CloseHandle(mapping_);
        CloseHandle(handle_);
        handle_ = nullptr;
        mapping_ = nullptr;
#else
        munmap(data_, size_);
        ::close(fd_);
        fd_ = -1;
#endif
        data_ = nullptr;
        size_ = 0;
    }

    const uint8_t* data() const { return static_cast<const uint8_t*>(data_); }
    size_t size() const { return size_; }

private:
    void* data_ = nullptr;
    size_t size_ = 0;
#ifdef _WIN32
    HANDLE handle_ = nullptr;
    HANDLE mapping_ = nullptr;
#else
    int fd_ = -1;
#endif
};

} // namespace ProjectV::IO
```

### Загрузка чанков из MMF

```cpp
class VoxelWorldLoader {
public:
    bool openWorld(const std::string& path) {
        if (!worldFile_.open(path)) {
            return false;
        }
        parseWorldHeader();
        return true;
    }

    std::vector<uint16_t> loadChunk(const glm::ivec3& position) {
        auto it = chunkIndex_.find(position);
        if (it == chunkIndex_.end()) {
            return {};  // Пустой чанк
        }

        const auto& index = it->second;
        const uint8_t* compressedData = worldFile_.data() + index.offset;

        return decompressor_.decompress(compressedData, index.size);
    }

private:
    struct ChunkIndex {
        uint64_t offset;
        uint32_t size;
    };

    IO::MemoryMappedFile worldFile_;
    DictionaryChunkCompressor decompressor_;
    std::unordered_map<glm::ivec3, ChunkIndex> chunkIndex_;

    void parseWorldHeader();
};
```

## Интеграция с flecs ECS

### Компоненты

```cpp
// Компонент для хранения сжатых данных
struct CompressedChunk {
    std::vector<uint8_t> data;
    uint32_t uncompressedSize = 0;
    uint64_t checksum = 0;
};

// Компонент для распакованных данных
struct VoxelData {
    std::vector<uint16_t> voxels;
};
```

### Система сжатия

```cpp
class ZstdCompressionSystem {
public:
    ZstdCompressionSystem(flecs::world& world) {
        // Система для сжатия данных при изменении
        world.system<VoxelData, CompressedChunk>()
            .kind(flecs::OnSet)
            .each([this](flecs::entity e, VoxelData& voxels, CompressedChunk& compressed) {
                auto compressedData = compressor_.compress(
                    voxels.voxels.data(),
                    voxels.voxels.size()
                );

                compressed.data = std::move(compressedData);
                compressed.uncompressedSize = voxels.voxels.size() * sizeof(uint16_t);
                compressed.checksum = calculateChecksum(voxels.voxels);
            });

        // Система для распаковки по требованию
        world.system<const CompressedChunk>()
            .kind(flecs::OnStore)
            .each([this](flecs::entity e, const CompressedChunk& compressed) {
                auto decompressed = compressor_.decompress(
                    compressed.data.data(),
                    compressed.data.size(),
                    compressed.uncompressedSize
                );

                e.set<VoxelData>({
                    std::move(decompressed)
                });
            });
    }

private:
    ChunkCompressor compressor_;
};
```

## Сравнение методов

| Метод                      | Размер чанка 32³ | Степень сжатия |
|----------------------------|------------------|----------------|
| Naive struct               | 589 KB           | 100%           |
| Packed 16-bit              | 65 KB            | 11%            |
| Packed + Zstd              | 20 KB            | 3.4%           |
| Packed + Zstd + Dict       | 13 KB            | 2.2%           |
| Packed + RLE + Zstd + Dict | 8 KB             | 1.4%           |

## Рекомендации

1. **Bit-Packing**: Всегда используйте для воксельных данных
2. **RLE**: Для чанков с большими однородными областями
3. **Словарь**: Для множества однотипных чанков
4. **Memory-Mapped**: Для файлов мира > 100 MB
5. **Пул контекстов**: Для многопоточного сжатия
