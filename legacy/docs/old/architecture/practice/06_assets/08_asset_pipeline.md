# Asset Pipeline: Компиляция и загрузка ресурсов

Пайплайн обработки ресурсов для ProjectV.

---

## Обзор

Asset Pipeline в ProjectV отвечает за:

1. **Компиляцию шейдеров** — Slang → SPIR-V
2. **Загрузку воксельных моделей** — .vox, .vxl
3. **Обработку текстур** — KTX2, Basis Universal
4. **Сериализацию данных** — Zstd, Bit Packing

---

## 1. Shader Compilation

### Slang → SPIR-V

ProjectV использует **Slang** как основной шейдерный язык:

| Преимущество          | Описание                                |
|-----------------------|-----------------------------------------|
| **Cross-compilation** | HLSL, GLSL, SPIR-V из одного источника  |
| **Modules**           | Модульная система для переиспользования |
| **Modern syntax**     | Удобный синтаксис с generics            |
| **Vulkan support**    | Нативная генерация SPIR-V               |

### Offline Compilation (Production)

```cmake
# CMakeLists.txt - Офлайн компиляция шейдеров
find_program(SLANGC slangc REQUIRED)

function(compile_slang_shader TARGET INPUT OUTPUT)
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${SLANGC}
            -target spirv
            -stage ${STAGE}
            -entry ${ENTRY}
            -O3
            -o ${OUTPUT}
            ${INPUT}
        DEPENDS ${INPUT}
        COMMENT "Compiling Slang shader: ${INPUT} → ${OUTPUT}"
    )
endfunction()

# Примеры
compile_slang_shader(
    "voxel_vertex"
    "${SHADERS_DIR}/voxel_mesh.slang"
    "${OUTPUT_DIR}/voxel_vertex.spv"
    STAGE "vertex"
    ENTRY "vsMain"
)

compile_slang_shader(
    "voxel_fragment"
    "${SHADERS_DIR}/voxel_mesh.slang"
    "${OUTPUT_DIR}/voxel_fragment.spv"
    STAGE "fragment"
    ENTRY "fsMain"
)

compile_slang_shader(
    "voxel_compute"
    "${SHADERS_DIR}/voxel_marching.slang"
    "${OUTPUT_DIR}/voxel_compute.spv"
    STAGE "compute"
    ENTRY "csMain"
)
```

### Runtime Compilation (Development)

```cpp
namespace projectv::assets {

class ShaderCompiler {
public:
    std::expected<ShaderModule, ShaderError> compileFromSource(
        const std::filesystem::path& sourcePath,
        VkShaderStageFlagBits stage,
        const std::string& entryPoint = "main"
    ) {
        // Для development режима - компиляция на лету
        std::string slangcOutput = runSlangc(sourcePath, stage, entryPoint);

        if (slangcOutput.empty()) {
            return std::unexpected(ShaderError::CompilationFailed);
        }

        // Парсим SPIR-V байткод
        std::vector<uint32_t> spirv = parseSpirv(slangcOutput);

        // Создаём VkShaderModule
        VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * sizeof(uint32_t),
            .pCode = spirv.data()
        };

        VkShaderModule module;
        VkResult result = vkCreateShaderModule(device_, &createInfo, nullptr, &module);

        if (result != VK_SUCCESS) {
            return std::unexpected(ShaderError::ModuleCreationFailed);
        }

        return ShaderModule{
            .module = module,
            .stage = stage,
            .entryPoint = entryPath
        };
    }

    // Hot reload для шейдеров
    void hotReload(const std::filesystem::path& changedShader) {
        auto it = loadedShaders_.find(changedShader);
        if (it == loadedShaders_.end()) return;

        auto newModule = compileFromSource(changedShader, it->second.stage, it->second.entryPoint);
        if (newModule) {
            // Удаляем старый модуль
            vkDestroyShaderModule(device_, it->second.module, nullptr);

            // Заменяем на новый
            it->second = *newModule;

            // Пересоздаём pipeline
            notifyPipelineRecreate(changedShader);
        }
    }

private:
    VkDevice device_;
    std::unordered_map<std::filesystem::path, ShaderModule> loadedShaders_;

    std::string runSlangc(
        const std::filesystem::path& source,
        VkShaderStageFlagBits stage,
        const std::string& entry
    ) {
        // Формируем команду
        std::string stageStr;
        switch (stage) {
            case VK_SHADER_STAGE_VERTEX_BIT: stageStr = "vertex"; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: stageStr = "fragment"; break;
            case VK_SHADER_STAGE_COMPUTE_BIT: stageStr = "compute"; break;
            case VK_SHADER_STAGE_MESH_BIT_EXT: stageStr = "mesh"; break;
            case VK_SHADER_STAGE_TASK_BIT_EXT: stageStr = "task"; break;
            default: return "";
        }

        std::string cmd = fmt::format(
            "slangc -target spirv -stage {} -entry {} -O3 {}",
            stageStr, entry, source.string()
        );

        // Выполняем и возвращаем результат
        return executeCommand(cmd);
    }
};

} // namespace projectv::assets
```

### Shader Hot Reload System

```cpp
namespace projectv::assets {

class ShaderHotReloader {
public:
    void startWatching(const std::filesystem::path& shadersDir) {
        watcherThread_ = std::thread([this, shadersDir]() {
            watchDirectory(shadersDir);
        });
    }

    void stopWatching() {
        running_ = false;
        if (watcherThread_.joinable()) {
            watcherThread_.join();
        }
    }

    void processChanges() {
        std::lock_guard<std::mutex> lock(changedFilesMutex_);

        for (const auto& file : changedFiles_) {
            compiler_.hotReload(file);
        }

        changedFiles_.clear();
    }

private:
    std::thread watcherThread_;
    std::atomic<bool> running_{true};
    std::mutex changedFilesMutex_;
    std::vector<std::filesystem::path> changedFiles_;
    ShaderCompiler compiler_;

    void watchDirectory(const std::filesystem::path& dir) {
        // File system watcher (platform-specific)
#ifdef _WIN32
        HANDLE hDir = CreateFileW(
            dir.wstring().c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        );

        while (running_) {
            BYTE buffer[4096];
            DWORD bytesReturned;

            if (ReadDirectoryChangesW(
                hDir, buffer, sizeof(buffer), TRUE,
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                &bytesReturned, nullptr, nullptr
            )) {
                BYTE* ptr = buffer;
                while (true) {
                    FILE_NOTIFY_INFORMATION* info =
                        reinterpret_cast<FILE_NOTIFY_INFORMATION*>(ptr);

                    if (info->Action == FILE_ACTION_MODIFIED) {
                        std::wstring filename(info->FileName,
                                            info->FileNameLength / sizeof(wchar_t));

                        std::lock_guard<std::mutex> lock(changedFilesMutex_);
                        changedFiles_.push_back(dir / filename);
                    }

                    if (info->NextEntryOffset == 0) break;
                    ptr += info->NextEntryOffset;
                }
            }
        }

        CloseHandle(hDir);
#endif
    }
};

} // namespace projectv::assets
```

---

## 2. Voxel Models

### Форматы

| Формат   | Описание        | Использование                   |
|----------|-----------------|---------------------------------|
| **.vox** | MagicaVoxel     | Импорт из редактора             |
| **.vxl** | ProjectV Custom | Оптимизированный runtime формат |
| **.vxb** | Binary Voxel    | Сжатый бинарный формат          |

### .VOX Importer (MagicaVoxel)

```cpp
namespace projectv::assets {

// Формат MagicaVoxel .vox
// https://github.com/ephtracy/voxel-model

struct VoxChunk {
    char id[4];        // "VOX "
    int32_t version;   // 通常 150
};

struct VoxSizeChunk {
    char id[4];        // "SIZE"
    int32_t contentSize;
    int32_t sizeX;
    int32_t sizeY;
    int32_t sizeZ;
};

struct VoxVoxel {
    uint8_t x, y, z;
    uint8_t colorIndex;
};

class VoxImporter {
public:
    std::expected<VoxelModel, ImportError> load(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(ImportError::FileNotFound);
        }

        // 1. Парсим заголовок
        VoxChunk header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (std::strncmp(header.id, "VOX ", 4) != 0) {
            return std::unexpected(ImportError::InvalidFormat);
        }

        VoxelModel model;

        // 2. Читаем чанки
        while (file) {
            char chunkId[4];
            file.read(chunkId, 4);

            if (!file) break;

            int32_t contentSize, childrenSize;
            file.read(reinterpret_cast<char*>(&contentSize), sizeof(contentSize));
            file.read(reinterpret_cast<char*>(&childrenSize), sizeof(childrenSize));

            if (std::strncmp(chunkId, "SIZE", 4) == 0) {
                // Размер модели
                file.read(reinterpret_cast<char*>(&model.sizeX), sizeof(int32_t));
                file.read(reinterpret_cast<char*>(&model.sizeY), sizeof(int32_t));
                file.read(reinterpret_cast<char*>(&model.sizeZ), sizeof(int32_t));
            }
            else if (std::strncmp(chunkId, "XYZI", 4) == 0) {
                // Воксели
                int32_t voxelCount;
                file.read(reinterpret_cast<char*>(&voxelCount), sizeof(voxelCount));

                model.voxels.resize(voxelCount);
                file.read(reinterpret_cast<char*>(model.voxels.data()),
                         voxelCount * sizeof(VoxVoxel));
            }
            else if (std::strncmp(chunkId, "RGBA", 4) == 0) {
                // Палитра цветов
                model.palette.resize(256);
                file.read(reinterpret_cast<char*>(model.palette.data()),
                         256 * sizeof(uint32_t));
            }
            else {
                // Пропускаем неизвестный чанк
                file.seekg(contentSize + childrenSize, std::ios::cur);
            }
        }

        return model;
    }

    // Конвертация во внутренний формат
    VoxelChunk toVoxelChunk(const VoxelModel& model) {
        VoxelChunk chunk;
        chunk.sizeX = model.sizeX;
        chunk.sizeY = model.sizeY;
        chunk.sizeZ = model.sizeZ;

        // Инициализируем воздухом
        chunk.voxels.resize(chunk.sizeX * chunk.sizeY * chunk.sizeZ, VoxelData{});

        // Заполняем вокселями
        for (const auto& voxel : model.voxels) {
            size_t index = voxel.x + voxel.z * chunk.sizeX +
                          voxel.y * chunk.sizeX * chunk.sizeZ;

            chunk.voxels[index] = VoxelData{
                .materialId = static_cast<uint16_t>(voxel.colorIndex),
                .density = 255,
                .flags = 0
            };
        }

        return chunk;
    }
};

} // namespace projectv::assets
```

### .VXL Custom Format

```cpp
namespace projectv::assets {

// ProjectV Custom Voxel Format (.vxl)
// Оптимизирован для быстрой загрузки

struct VxlHeader {
    char magic[4];          // "PVVX"
    uint32_t version;       // Format version
    uint32_t flags;         // Compression flags
    uint32_t sizeX, sizeY, sizeZ;
    uint32_t voxelCount;    // Non-air voxels only
    uint32_t materialCount;
    uint32_t compressedSize;
};

struct VxlVoxel {
    uint16_t x, y, z;       // Position (relative to chunk)
    uint16_t materialId;
    uint8_t density;
    uint8_t flags;
};

class VxlExporter {
public:
    bool save(const VoxelChunk& chunk, const std::filesystem::path& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;

        // 1. Подсчитываем non-air воксели
        std::vector<VxlVoxel> voxels;
        for (uint16_t z = 0; z < chunk.sizeZ; ++z) {
            for (uint16_t y = 0; y < chunk.sizeY; ++y) {
                for (uint16_t x = 0; x < chunk.sizeX; ++x) {
                    const auto& v = chunk.getVoxel(x, y, z);
                    if (v.materialId != 0) {
                        voxels.push_back({
                            .x = x, .y = y, .z = z,
                            .materialId = v.materialId,
                            .density = v.density,
                            .flags = v.flags
                        });
                    }
                }
            }
        }

        // 2. Сжимаем данные
        std::vector<uint8_t> uncompressed(
            reinterpret_cast<uint8_t*>(voxels.data()),
            reinterpret_cast<uint8_t*>(voxels.data()) +
            voxels.size() * sizeof(VxlVoxel)
        );

        auto compressed = zstdCompress(uncompressed, 3);

        // 3. Записываем заголовок
        VxlHeader header = {
            .magic = {'P', 'V', 'V', 'X'},
            .version = 1,
            .flags = 0,
            .sizeX = chunk.sizeX,
            .sizeY = chunk.sizeY,
            .sizeZ = chunk.sizeZ,
            .voxelCount = static_cast<uint32_t>(voxels.size()),
            .materialCount = 0,
            .compressedSize = static_cast<uint32_t>(compressed.size())
        };

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // 4. Записываем сжатые данные
        file.write(reinterpret_cast<const char*>(compressed.data()),
                  compressed.size());

        return true;
    }
};

class VxlImporter {
public:
    std::expected<VoxelChunk, ImportError> load(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(ImportError::FileNotFound);
        }

        // 1. Читаем заголовок
        VxlHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (std::strncmp(header.magic, "PVVX", 4) != 0) {
            return std::unexpected(ImportError::InvalidFormat);
        }

        // 2. Читаем и разжимаем данные
        std::vector<uint8_t> compressed(header.compressedSize);
        file.read(reinterpret_cast<char*>(compressed.data()), header.compressedSize);

        auto uncompressed = zstdDecompress(compressed);

        // 3. Парсим воксели
        VoxelChunk chunk;
        chunk.sizeX = header.sizeX;
        chunk.sizeY = header.sizeY;
        chunk.sizeZ = header.sizeZ;
        chunk.voxels.resize(header.sizeX * header.sizeY * header.sizeZ);

        const VxlVoxel* voxels = reinterpret_cast<const VxlVoxel*>(uncompressed.data());
        for (uint32_t i = 0; i < header.voxelCount; ++i) {
            size_t index = voxels[i].x + voxels[i].z * chunk.sizeX +
                          voxels[i].y * chunk.sizeX * chunk.sizeZ;
            chunk.voxels[index] = VoxelData{
                .materialId = voxels[i].materialId,
                .density = voxels[i].density,
                .flags = voxels[i].flags
            };
        }

        return chunk;
    }
};

} // namespace projectv::assets
```

---

## 3. Texture Pipeline

### Форматы текстур

| Формат              | Преимущества        | Использование |
|---------------------|---------------------|---------------|
| **KTX2**            | GPU-native, mipmaps | Все текстуры  |
| **Basis Universal** | Supercompression    | Streaming     |
| **PNG**             | Lossless, source    | Editor only   |

### KTX2 Loader

```cpp
namespace projectv::assets {

class Ktx2Loader {
public:
    struct Ktx2Texture {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
        uint32_t mipLevels;
        VkFormat format;
        VkExtent3D extent;
    };

    std::expected<Ktx2Texture, TextureError> load(
        const std::filesystem::path& path,
        VkDevice device,
        VmaAllocator allocator
    ) {
        // KTX2 header
        struct Ktx2Header {
            uint8_t identifier[12];
            uint32_t vkFormat;
            uint32_t typeSize;
            uint32_t pixelWidth;
            uint32_t pixelHeight;
            uint32_t pixelDepth;
            uint32_t layerCount;
            uint32_t faceCount;
            uint32_t levelCount;
            uint32_t supercompressionScheme;
            // ... additional fields
        };

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(TextureError::FileNotFound);
        }

        Ktx2Header header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        // Validate
        const uint8_t ktx2Identifier[] = {
            0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB,
            0x0D, 0x0A, 0x1A, 0x0A
        };

        if (std::memcmp(header.identifier, ktx2Identifier, 12) != 0) {
            return std::unexpected(TextureError::InvalidFormat);
        }

        // Create image
        VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = static_cast<VkFormat>(header.vkFormat),
            .extent = {
                .width = header.pixelWidth,
                .height = header.pixelHeight,
                .depth = header.pixelDepth > 0 ? header.pixelDepth : 1
            },
            .mipLevels = header.levelCount,
            .arrayLayers = header.layerCount * header.faceCount,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VmaAllocationCreateInfo allocInfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY
        };

        Ktx2Texture texture;
        texture.format = imageInfo.format;
        texture.extent = imageInfo.extent;
        texture.mipLevels = imageInfo.mipLevels;

        vmaCreateImage(allocator, &imageInfo, &allocInfo,
                       &texture.image, &texture.allocation, nullptr);

        // Upload data (staging buffer + command buffer)
        uploadTextureData(file, texture, header, device, allocator);

        // Create image view
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texture.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = texture.mipLevels,
                .layerCount = imageInfo.arrayLayers
            }
        };

        vkCreateImageView(device, &viewInfo, nullptr, &texture.imageView);

        return texture;
    }

private:
    void uploadTextureData(
        std::ifstream& file,
        Ktx2Texture& texture,
        const Ktx2Header& header,
        VkDevice device,
        VmaAllocator allocator
    ) {
        // Читаем данные уровней мипмапов
        for (uint32_t level = 0; level < header.levelCount; ++level) {
            // ... чтение и загрузка каждого уровня
        }
    }
};

} // namespace projectv::assets
```

### Texture Atlas Generation

```cpp
namespace projectv::assets {

class TextureAtlasGenerator {
public:
    struct AtlasEntry {
        std::string name;
        uint32_t x, y, width, height;
    };

    struct Atlas {
        VkImage image;
        VkImageView imageView;
        uint32_t width, height;
        std::unordered_map<std::string, AtlasEntry> entries;
    };

    Atlas generate(
        const std::vector<std::filesystem::path>& texturePaths,
        uint32_t atlasSize,
        VkDevice device,
        VmaAllocator allocator
    ) {
        Atlas atlas;
        atlas.width = atlasSize;
        atlas.height = atlasSize;

        // 1. Загружаем все текстуры
        std::vector<LoadedTexture> textures;
        for (const auto& path : texturePaths) {
            textures.push_back(loadTexture(path));
        }

        // 2. Сортируем по размеру (большие первыми)
        std::sort(textures.begin(), textures.end(),
                  [](const auto& a, const auto& b) {
                      return a.width * a.height > b.width * b.height;
                  });

        // 3. Rect packing алгоритм
        RectPacker packer(atlasSize, atlasSize);

        for (const auto& tex : textures) {
            auto rect = packer.insert(tex.width, tex.height);
            if (!rect) {
                // Не влезает - нужно увеличить атлас или разбить
                continue;
            }

            atlas.entries[tex.name] = {
                .name = tex.name,
                .x = rect->x,
                .y = rect->y,
                .width = rect->width,
                .height = rect->height
            };
        }

        // 4. Создаём итоговое изображение
        // ... GPU upload

        return atlas;
    }

private:
    struct LoadedTexture {
        std::string name;
        std::vector<uint8_t> data;
        uint32_t width, height;
    };

    class RectPacker {
        // Shelf алгоритм для rect packing
        struct Shelf {
            uint32_t y;
            uint32_t height;
            uint32_t currentX;
        };

        std::vector<Shelf> shelves_;
        uint32_t width_, height_;

    public:
        RectPacker(uint32_t width, uint32_t height)
            : width_(width), height_(height) {}

        std::optional<glm::ivec4> insert(uint32_t w, uint32_t h) {
            // Ищем подходящую полку
            for (auto& shelf : shelves_) {
                if (h <= shelf.height &&
                    shelf.currentX + w <= width_) {

                    glm::ivec4 rect(shelf.currentX, shelf.y, w, h);
                    shelf.currentX += w;
                    return rect;
                }
            }

            // Создаём новую полку
            uint32_t shelfY = shelves_.empty() ? 0 :
                             shelves_.back().y + shelves_.back().height;

            if (shelfY + h <= height_) {
                shelves_.push_back({
                    .y = shelfY,
                    .height = h,
                    .currentX = w
                });

                return glm::ivec4(0, shelfY, w, h);
            }

            return std::nullopt;  // Не влезает
        }
    };
};

} // namespace projectv::assets
```

---

## 4. Asset Manifest

### Описание ресурсов

```json
// assets/manifest.json
{
    "version": 1,
    "shaders": {
        "voxel_vertex": {
            "source": "shaders/voxel_mesh.slang",
            "stage": "vertex",
            "entry": "vsMain",
            "output": "compiled/voxel_vertex.spv"
        },
        "voxel_fragment": {
            "source": "shaders/voxel_mesh.slang",
            "stage": "fragment",
            "entry": "fsMain",
            "output": "compiled/voxel_fragment.spv"
        },
        "voxel_compute": {
            "source": "shaders/voxel_marching.slang",
            "stage": "compute",
            "entry": "csMain",
            "output": "compiled/voxel_compute.spv"
        }
    },
    "textures": {
        "voxel_atlas": {
            "source": "textures/atlas/",
            "format": "KTX2",
            "output": "compiled/voxel_atlas.ktx2",
            "size": 4096
        }
    },
    "models": {
        "tree": {
            "source": "models/tree.vox",
            "output": "compiled/tree.vxl"
        },
        "rock": {
            "source": "models/rock.vox",
            "output": "compiled/rock.vxl"
        }
    }
}
```

### Asset Manager

```cpp
namespace projectv::assets {

class AssetManager {
public:
    void initialize(
        const std::filesystem::path& manifestPath,
        VkDevice device,
        VmaAllocator allocator
    ) {
        device_ = device;
        allocator_ = allocator;

        // Загружаем манифест
        loadManifest(manifestPath);
    }

    // Shader loading
    std::expected<ShaderHandle, AssetError> loadShader(const std::string& name) {
        auto it = manifest_.shaders.find(name);
        if (it == manifest_.shaders.end()) {
            return std::unexpected(AssetError::NotFound);
        }

        // Проверяем кэш
        if (auto cached = shaderCache_.find(name); cached != shaderCache_.end()) {
            return cached->second;
        }

        // Загружаем SPIR-V
        auto spirv = loadSpirv(it->second.output);
        if (!spirv) {
            return std::unexpected(AssetError::LoadFailed);
        }

        // Создаём модуль
        auto module = createShaderModule(*spirv, it->second.stage);
        if (!module) {
            return std::unexpected(AssetError::CreateFailed);
        }

        ShaderHandle handle{nextHandle_++};
        shaderCache_[name] = handle;
        shaderModules_[handle] = *module;

        return handle;
    }

    // Texture loading
    std::expected<TextureHandle, AssetError> loadTexture(const std::string& name) {
        auto it = manifest_.textures.find(name);
        if (it == manifest_.textures.end()) {
            return std::unexpected(AssetError::NotFound);
        }

        if (auto cached = textureCache_.find(name); cached != textureCache_.end()) {
            return cached->second;
        }

        Ktx2Loader loader;
        auto texture = loader.load(it->second.output, device_, allocator_);
        if (!texture) {
            return std::unexpected(AssetError::LoadFailed);
        }

        TextureHandle handle{nextHandle_++};
        textureCache_[name] = handle;
        textures_[handle] = *texture;

        return handle;
    }

    // Model loading
    std::expected<ModelHandle, AssetError> loadModel(const std::string& name) {
        auto it = manifest_.models.find(name);
        if (it == manifest_.models.end()) {
            return std::unexpected(AssetError::NotFound);
        }

        if (auto cached = modelCache_.find(name); cached != modelCache_.end()) {
            return cached->second;
        }

        VxlImporter importer;
        auto model = importer.load(it->second.output);
        if (!model) {
            return std::unexpected(AssetError::LoadFailed);
        }

        ModelHandle handle{nextHandle_++};
        modelCache_[name] = handle;
        models_[handle] = *model;

        return handle;
    }

private:
    VkDevice device_;
    VmaAllocator allocator_;
    uint64_t nextHandle_ = 1;

    AssetManifest manifest_;

    std::unordered_map<std::string, ShaderHandle> shaderCache_;
    std::unordered_map<std::string, TextureHandle> textureCache_;
    std::unordered_map<std::string, ModelHandle> modelCache_;

    std::unordered_map<ShaderHandle, ShaderModule> shaderModules_;
    std::unordered_map<TextureHandle, Ktx2Loader::Ktx2Texture> textures_;
    std::unordered_map<ModelHandle, VoxelChunk> models_;
};

} // namespace projectv::assets
```

---

## 5. Build Pipeline

### CMake Integration

```cmake
# cmake/AssetPipeline.cmake

# Цель для компиляции всех ассетов
add_custom_target(compile_assets
    COMMENT "Compiling all assets"
)

# Компиляция шейдеров
function(add_shader_compilation TARGET)
    add_custom_target(${TARGET}_shaders
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        COMMAND slangc -target spirv ...
        DEPENDS ${SHADER_SOURCES}
        COMMENT "Compiling shaders for ${TARGET}"
    )
    add_dependencies(compile_assets ${TARGET}_shaders)
endfunction()

# Компиляция текстур
function(add_texture_compilation TARGET)
    add_custom_target(${TARGET}_textures
        COMMAND texture_atlas_generator ...
        DEPENDS ${TEXTURE_SOURCES}
        COMMENT "Generating texture atlases for ${TARGET}"
    )
    add_dependencies(compile_assets ${TARGET}_textures)
endfunction()

# Конвертация моделей
function(add_model_conversion TARGET)
    add_custom_target(${TARGET}_models
        COMMAND vox_to_vxl_converter ...
        DEPENDS ${MODEL_SOURCES}
        COMMENT "Converting models for ${TARGET}"
    )
    add_dependencies(compile_assets ${TARGET}_models)
endfunction()
```

---

## Резюме

### Форматы ProjectV

| Тип          | Формат        | Компиляция       |
|--------------|---------------|------------------|
| **Shaders**  | .slang → .spv | Slang compiler   |
| **Textures** | .png → .ktx2  | KTX2 tools       |
| **Models**   | .vox → .vxl   | Custom converter |
| **Data**     | .json + Zstd  | Compressed       |

### Pipeline Flow

```
Source Assets → Compilation → Optimized Assets → Runtime
    │               │                │               │
    │               │                │               │
  .slang      slangc → .spv     .spv          VkShaderModule
  .png        ktx2 → .ktx2      .ktx2         VkImage
  .vox        converter → .vxl  .vxl          VoxelChunk
```
