# Slang: Интеграция в ProjectV

## C++26 Module Integration

### Модуль SlangShaderManager

```cpp
// slang_shader_manager.cppm
export module projectv.shaders.slang;

// Global Module Fragment для изоляции заголовков Slang, Vulkan, VMA, Flecs
module;
#include <slang.h>
#include <slang-com-ptr.h>
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <flecs.h>
export module projectv.shaders.slang;

// Импорт ProjectV модулей
import :core.memory;      // projectv_core_memory
import :core.logging;     // projectv_core_logging
import :core.profiling;   // projectv_core_profiling

namespace projectv {

export class SlangShaderManager
{
public:
    struct ShaderModule
    {
        VkShaderModule module = VK_NULL_HANDLE;
        std::string entryPoint;
        VkShaderStageFlagBits stage;
    };

    explicit SlangShaderManager(VkDevice device);
    ~SlangShaderManager();

    SlangShaderManager(const SlangShaderManager&) = delete;
    SlangShaderManager& operator=(const SlangShaderManager&) = delete;

    std::expected<void, std::string> initialize(
        projectv::core::memory::ArenaAllocator& arena,
        const std::vector<std::string>& searchPaths
    );

    std::expected<ShaderModule, std::string> loadSPIRV(
        const std::string& path,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    std::expected<ShaderModule, std::string> loadCached(
        const std::string& name,
        const std::string& path,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    void clearCache();
    void destroyModule(const std::string& name);

private:
    VkDevice device_;
    projectv::core::memory::ArenaAllocator* arena_;

    Slang::ComPtr<slang::IGlobalSession> globalSession_;
    Slang::ComPtr<slang::ISession> session_;

    std::unordered_map<std::string, ShaderModule> moduleCache_;

    std::expected<std::vector<uint32_t>, std::string> readSPIRVFile(const std::string& path);
    std::expected<VkShaderModule, std::string> createShaderModuleHandle(const std::vector<uint32_t>& code);
};

} // namespace projectv
```

### Использование модуля в коде ProjectV

```cpp
// main.cppm
import projectv.shaders.slang;
import :core.memory;
import :core.logging;

int main() {
    // Создание ArenaAllocator для временных данных компиляции шейдеров
    projectv::core::memory::ArenaAllocator shaderArena(16 * 1024 * 1024); // 16MB
    
    // Инициализация SlangShaderManager
    SlangShaderManager shaderManager(device);
    auto initResult = shaderManager.initialize(shaderArena, {
        "shaders/core",
        "shaders/voxel",
        "shaders/materials"
    });
    
    if (!initResult) {
        projectv::core::Log::error("Slang", "Failed to initialize: {}", initResult.error());
        return 1;
    }
    
    // Загрузка шейдера
    auto shader = shaderManager.loadCached(
        "voxel_gbuffer",
        "shaders/voxel/rendering/gbuffer.slang",
        "vsMain",
        VK_SHADER_STAGE_VERTEX_BIT
    );
    
    if (shader) {
        projectv::core::Log::info("Slang", "Shader loaded successfully");
    }
    
    // Сброс арены в конце кадра
    shaderArena.reset();
}
```

## CMake-конфигурация

### Поиск slangc

```cmake
# cmake/FindSlang.cmake
find_program(SLANGC_EXECUTABLE
    NAMES slangc
    HINTS
        $ENV{VULKAN_SDK}/bin
        ${CMAKE_SOURCE_DIR}/external/slang/build/bin
        ${CMAKE_SOURCE_DIR}/external/slang/build/Release/bin
)

find_path(SLANG_INCLUDE_DIR
    NAMES slang.h
    HINTS
        $ENV{VULKAN_SDK}/include
        ${CMAKE_SOURCE_DIR}/external/slang/include
)

find_library(SLANG_LIBRARY
    NAMES slang slangd
    HINTS
        $ENV{VULKAN_SDK}/lib
        ${CMAKE_SOURCE_DIR}/external/slang/build/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Slang
    REQUIRED_VARS SLANGC_EXECUTABLE SLANG_INCLUDE_DIR SLANG_LIBRARY
)

if(Slang_FOUND)
    add_library(Slang::slang UNKNOWN IMPORTED)
    set_target_properties(Slang::slang PROPERTIES
        IMPORTED_LOCATION "${SLANG_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
    )
endif()
```

### Модуль компиляции шейдеров

```cmake
# cmake/ProjectVShaders.cmake
include_guard()

# Параметры компиляции ProjectV
set(PROJECTV_SLANG_TARGET spirv)
set(PROJECTV_SLANG_PROFILE spirv_1_5)
set(PROJECTV_SLANG_OPTIMIZATION -O2)

# Пути к модулям
set(PROJECTV_SLANG_INCLUDES
    -I ${PROJECT_SOURCE_DIR}/shaders/core
    -I ${PROJECT_SOURCE_DIR}/shaders/voxel
    -I ${PROJECT_SOURCE_DIR}/shaders/materials
)

# Дополнительные флаги для Vulkan 1.4
set(PROJECTV_SLANG_FLAGS
    -enable-slang-matrix-layout-row-major
    -fvk-use-dx-position-w
)

function(projectv_compile_shader SOURCE ENTRY STAGE OUTPUT)
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${SLANGC_EXECUTABLE}
            ${SOURCE}
            -o ${OUTPUT}
            -target ${PROJECTV_SLANG_TARGET}
            -profile ${PROJECTV_SLANG_PROFILE}
            -entry ${ENTRY}
            -stage ${STAGE}
            ${PROJECTV_SLANG_OPTIMIZATION}
            ${PROJECTV_SLANG_INCLUDES}
            ${PROJECTV_SLANG_FLAGS}
        DEPENDS ${SOURCE}
        COMMENT "ProjectV Shader: ${SOURCE} -> ${OUTPUT}"
        VERBATIM
    )
endfunction()

function(projectv_add_shader_target TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    set(ALL_OUTPUTS)

    foreach(SRC IN LISTS ARG_SOURCES)
        get_filename_component(NAME_WE ${SRC} NAME_WE)

        set(VERT_OUT "${CMAKE_BINARY_DIR}/shaders/${NAME_WE}_vert.spv")
        projectv_compile_shader(${SRC} vsMain vertex ${VERT_OUT})
        list(APPEND ALL_OUTPUTS ${VERT_OUT})

        set(FRAG_OUT "${CMAKE_BINARY_DIR}/shaders/${NAME_WE}_frag.spv")
        projectv_compile_shader(${SRC} fsMain fragment ${FRAG_OUT})
        list(APPEND ALL_OUTPUTS ${FRAG_OUT})
    endforeach()

    add_custom_target(${TARGET_NAME} ALL DEPENDS ${ALL_OUTPUTS})
endfunction()
```

### Использование в CMakeLists.txt

```cmake
# CMakeLists.txt (фрагмент)
# Интеграция Slang из external/
add_subdirectory(external/slang)

# Подключение ProjectV модулей
target_link_libraries(projectv_shaders PRIVATE
    projectv_core_memory      # MemoryManager для ArenaAllocator
    projectv_core_logging     # Logging для сообщений компиляции
    projectv_core_profiling   # Tracy hooks для профилирования
)

# Основные шейдеры
projectv_add_shader_target(projectv_shaders
    SOURCES
        shaders/voxel/rendering/gbuffer.slang
        shaders/voxel/rendering/lighting.slang
)

# Compute шейдеры
projectv_compile_shader(
    shaders/voxel/compute/culling.slang
    csMain
    compute
    ${CMAKE_BINARY_DIR}/shaders/culling.spv
)

add_custom_target(projectv_compute_shaders
    DEPENDS ${CMAKE_BINARY_DIR}/shaders/culling.spv
)

add_dependencies(ProjectV projectv_shaders projectv_compute_shaders)
```

### CMake конфигурация для C++26 Module

```cmake
# Включение поддержки C++26 Modules
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройка для модулей C++26
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/experimental:module /std:c++latest)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fmodules-ts -std=c++2b)
endif()

# Создание модуля projectv.shaders.slang
add_library(projectv_shaders_slang)
target_sources(projectv_shaders_slang
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES slang_shader_manager.cppm
)
target_link_libraries(projectv_shaders_slang PRIVATE
    Slang::slang
    Vulkan::Vulkan
    projectv_core_memory
    projectv_core_logging
    projectv_core_profiling
)
```

## MemoryManager Integration

### SlangArenaAllocator для временных данных компиляции

```cpp
// slang_arena_allocator.hpp
#pragma once

#include <slang.h>
#include <projectv/core/memory/arena_allocator.hpp>

namespace projectv {

class SlangArenaAllocator : public slang::IAllocator
{
public:
    explicit SlangArenaAllocator(core::memory::ArenaAllocator& arena)
        : arena_(arena) {}

    void* allocate(size_t size, size_t alignment) override {
        ZoneScopedN("SlangAllocator::allocate");
        
        // Выравнивание до степени двойки
        size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);
        
        void* ptr = arena_.allocate(alignedSize, alignment);
        if (!ptr) {
            core::Log::error("Slang", "Failed to allocate {} bytes with alignment {}", size, alignment);
            return nullptr;
        }
        
        TracyPlot("Slang/AllocationSize", static_cast<int64_t>(alignedSize));
        return ptr;
    }

    void deallocate(void* ptr) override {
        ZoneScopedN("SlangAllocator::deallocate");
        // ArenaAllocator не поддерживает индивидуальное освобождение
        // Память освобождается целиком при reset()
    }

private:
    core::memory::ArenaAllocator& arena_;
};

} // namespace projectv
```

### Интеграция ArenaAllocator в SlangShaderManager

```cpp
// slang_shader_manager.cpp (фрагмент)
std::expected<void, std::string> SlangShaderManager::initialize(
    projectv::core::memory::ArenaAllocator& arena,
    const std::vector<std::string>& searchPaths)
{
    ZoneScopedN("SlangShaderManager::initialize");
    
    arena_ = &arena;
    
    // Создание кастомного аллокатора для Slang
    slangAllocator_ = std::make_unique<SlangArenaAllocator>(arena);
    
    SlangResult result = slang::createGlobalSession(globalSession_.writeRef());
    if (SLANG_FAILED(result)) {
        return std::unexpected("Failed to create Slang global session");
    }

    // Настройка сессии с кастомным аллокатором
    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession_->findProfile("spirv_1_5");
    targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.allocator = slangAllocator_.get();  // Использование нашего аллокатора

    std::vector<const char*> pathPtrs;
    for (const auto& path : searchPaths) {
        pathPtrs.push_back(path.c_str());
    }
    sessionDesc.searchPaths = pathPtrs.data();
    sessionDesc.searchPathCount = static_cast<int>(pathPtrs.size());

    result = globalSession_->createSession(sessionDesc, session_.writeRef());
    if (SLANG_FAILED(result)) {
        return std::unexpected("Failed to create Slang session");
    }

    core::Log::info("Slang", "Shader manager initialized with {} search paths", searchPaths.size());
    return {};
}
```

### Использование в ProjectV Engine

```cpp
// engine.cpp
void ProjectVEngine::initializeShaders() {
    ZoneScopedN("Engine::initializeShaders");
    
    // Создание арены для компиляции шейдеров (16MB)
    shaderArena_ = std::make_unique<core::memory::ArenaAllocator>(16 * 1024 * 1024);
    
    // Инициализация менеджера шейдеров
    shaderManager_ = std::make_unique<SlangShaderManager>(vulkanDevice_);
    
    auto initResult = shaderManager_->initialize(*shaderArena_, {
        "shaders/core",
        "shaders/voxel",
        "shaders/materials"
    });
    
    if (!initResult) {
        core::Log::error("Slang", "Shader manager initialization failed: {}", initResult.error());
        return;
    }
    
    core::Log::info("Slang", "Shader system initialized successfully");
}

void ProjectVEngine::endFrame() {
    ZoneScopedN("Engine::endFrame");
    
    // Сброс арены в конце кадра
    if (shaderArena_) {
        shaderArena_->reset();
        TracyPlot("Slang/ArenaReset", 1);
    }
}
```

## Vulkan + volk интеграция

### Получение расширений instance

```cpp
Uint32 extensionCount = 0;
const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

// extensions — массив строк, НЕ освобождать
// Пример: {"VK_KHR_surface", "VK_KHR_win32_surface"}
```

### Создание VkShaderModule

```cpp
#include <vector>
#include <fstream>

std::expected<std::vector<uint32_t>, std::string> readSPIRV(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::unexpected("Failed to open file: " + path);
    }

    size_t size = file.tellg();
    if (size % sizeof(uint32_t) != 0) {
        return std::unexpected("Invalid SPIR-V file size");
    }

    file.seekg(0);
    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), size);

    return code;
}

std::expected<VkShaderModule, std::string> createShaderModule(
    VkDevice device,
    const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &module);

    if (result != VK_SUCCESS) {
        return std::unexpected("vkCreateShaderModule failed: " + std::to_string(result));
    }

    return module;
}
```

## SlangShaderManager

### Заголовочный файл

```cpp
// src/renderer/slang_shader_manager.hpp
#pragma once

#include <vulkan/vulkan.h>
#include <slang.h>
#include <slang-com-ptr.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <expected>

namespace projectv {

class SlangShaderManager
{
public:
    struct ShaderModule
    {
        VkShaderModule module = VK_NULL_HANDLE;
        std::string entryPoint;
        VkShaderStageFlagBits stage;
    };

    explicit SlangShaderManager(VkDevice device);
    ~SlangShaderManager();

    SlangShaderManager(const SlangShaderManager&) = delete;
    SlangShaderManager& operator=(const SlangShaderManager&) = delete;

    std::expected<void, std::string> initialize(const std::vector<std::string>& searchPaths);

    std::expected<ShaderModule, std::string> loadSPIRV(
        const std::string& path,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    std::expected<ShaderModule, std::string> loadCached(
        const std::string& name,
        const std::string& path,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

#ifdef PROJECTV_SHADER_HOT_RELOAD
    std::expected<ShaderModule, std::string> compile(
        const std::string& slangSource,
        const std::string& moduleName,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    void hotReload(const std::string& name);
    void checkForChanges();
#endif

    void clearCache();
    void destroyModule(const std::string& name);

private:
    VkDevice device_;

    Slang::ComPtr<slang::IGlobalSession> globalSession_;
    Slang::ComPtr<slang::ISession> session_;

    std::unordered_map<std::string, ShaderModule> moduleCache_;

    std::expected<std::vector<uint32_t>, std::string> readSPIRVFile(const std::string& path);
    std::expected<VkShaderModule, std::string> createShaderModuleHandle(const std::vector<uint32_t>& code);
};

} // namespace projectv
```

### Реализация

```cpp
// src/renderer/slang_shader_manager.cpp
#include "slang_shader_manager.hpp"
#include <fstream>

namespace projectv {

SlangShaderManager::SlangShaderManager(VkDevice device)
    : device_(device)
{
}

SlangShaderManager::~SlangShaderManager()
{
    clearCache();
}

std::expected<void, std::string> SlangShaderManager::initialize(
    const std::vector<std::string>& searchPaths)
{
    SlangResult result = slang::createGlobalSession(globalSession_.writeRef());
    if (SLANG_FAILED(result)) {
        return std::unexpected("Failed to create Slang global session");
    }

    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession_->findProfile("spirv_1_5");
    targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    std::vector<const char*> pathPtrs;
    for (const auto& path : searchPaths) {
        pathPtrs.push_back(path.c_str());
    }
    sessionDesc.searchPaths = pathPtrs.data();
    sessionDesc.searchPathCount = static_cast<int>(pathPtrs.size());

    result = globalSession_->createSession(sessionDesc, session_.writeRef());
    if (SLANG_FAILED(result)) {
        return std::unexpected("Failed to create Slang session");
    }

    return {};
}

std::expected<SlangShaderManager::ShaderModule, std::string>
SlangShaderManager::loadSPIRV(
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ShaderModule shaderModule;
    shaderModule.entryPoint = entryPoint;
    shaderModule.stage = stage;

    auto spirvCode = readSPIRVFile(path);
    if (!spirvCode) {
        return std::unexpected(spirvCode.error());
    }

    auto module = createShaderModuleHandle(*spirvCode);
    if (!module) {
        return std::unexpected(module.error());
    }

    shaderModule.module = *module;
    return shaderModule;
}

std::expected<SlangShaderManager::ShaderModule, std::string>
SlangShaderManager::loadCached(
    const std::string& name,
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    auto it = moduleCache_.find(name);
    if (it != moduleCache_.end()) {
        return it->second;
    }

    auto module = loadSPIRV(path, entryPoint, stage);
    if (!module) {
        return std::unexpected(module.error());
    }

    moduleCache_[name] = *module;
    return *module;
}

void SlangShaderManager::clearCache()
{
    for (auto& [name, module] : moduleCache_) {
        if (module.module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, module.module, nullptr);
        }
    }
    moduleCache_.clear();
}

void SlangShaderManager::destroyModule(const std::string& name)
{
    auto it = moduleCache_.find(name);
    if (it != moduleCache_.end()) {
        if (it->second.module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, it->second.module, nullptr);
        }
        moduleCache_.erase(it);
    }
}

std::expected<std::vector<uint32_t>, std::string>
SlangShaderManager::readSPIRVFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::unexpected("Failed to open: " + path);
    }

    size_t size = file.tellg();
    if (size % sizeof(uint32_t) != 0) {
        return std::unexpected("Invalid SPIR-V file size");
    }

    file.seekg(0);
    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), size);

    return code;
}

std::expected<VkShaderModule, std::string>
SlangShaderManager::createShaderModuleHandle(const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(device_, &createInfo, nullptr, &module);

    if (result != VK_SUCCESS) {
        return std::unexpected("vkCreateShaderModule failed: " + std::to_string(result));
    }

    return module;
}

#ifdef PROJECTV_SHADER_HOT_RELOAD
std::expected<SlangShaderManager::ShaderModule, std::string>
SlangShaderManager::compile(
    const std::string& slangSource,
    const std::string& moduleName,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ShaderModule shaderModule;
    shaderModule.entryPoint = entryPoint;
    shaderModule.stage = stage;

    Slang::ComPtr<slang::IBlob> diagnostics;
    auto module = session_->loadModuleFromSourceString(
        moduleName.c_str(),
        moduleName.c_str(),
        slangSource.c_str(),
        diagnostics.writeRef()
    );

    if (!module) {
        const char* diag = diagnostics
            ? static_cast<const char*>(diagnostics->getBufferPointer())
            : "Unknown error";
        return std::unexpected("Slang compile error: " + std::string(diag));
    }

    Slang::ComPtr<slang::IEntryPoint> entry;
    module->findEntryPointByName(entryPoint.c_str(), entry.writeRef());

    if (!entry) {
        return std::unexpected("Entry point not found: " + entryPoint);
    }

    slang::IComponentType* components[] = {module.Get(), entry.Get()};
    Slang::ComPtr<slang::IComponentType> program;
    session_->createCompositeComponentType(
        components, 2,
        program.writeRef(),
        diagnostics.writeRef()
    );

    Slang::ComPtr<slang::IComponentType> linked;
    program->link(linked.writeRef(), diagnostics.writeRef());

    Slang::ComPtr<slang::IBlob> spirvCode;
    linked->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

    if (!spirvCode) {
        return std::unexpected("Failed to generate SPIR-V");
    }

    std::vector<uint32_t> code(
        reinterpret_cast<const uint32_t*>(spirvCode->getBufferPointer()),
        reinterpret_cast<const uint32_t*>(
            static_cast<const char*>(spirvCode->getBufferPointer()) + spirvCode->getBufferSize()
        )
    );

    auto moduleHandle = createShaderModuleHandle(code);
    if (!moduleHandle) {
        return std::unexpected(moduleHandle.error());
    }

    shaderModule.module = *moduleHandle;
    return shaderModule;
}
#endif

} // namespace projectv
```

## Структура шейдерной системы ProjectV

### Структура директорий

```
ProjectV/
├── shaders/
│   ├── core/
│   │   ├── types.slang         # Базовые типы данных
│   │   ├── math.slang          # Математические утилиты
│   │   └── constants.slang     # Константы движка
│   │
│   ├── voxel/
│   │   ├── data/
│   │   │   ├── chunk.slang     # VoxelChunk generic
│   │   │   ├── octree.slang    # SVO структуры
│   │   │   └── sparse.slang    # Sparse представления
│   │   │
│   │   ├── rendering/
│   │   │   ├── gbuffer.slang   # G-buffer pass
│   │   │   ├── lighting.slang  # Deferred lighting
│   │   │   └── raymarch.slang  # Ray marching
│   │   │
│   │   └── compute/
│   │       ├── generation.slang
│   │       ├── culling.slang
│   │       └── lod.slang
│   │
│   └── materials/
│       ├── interface.slang     # IMaterial интерфейс
│       ├── pbr.slang           # PBR реализация
│       └── voxel_material.slang
│
└── build/shaders/
    ├── core/
    ├── voxel/
    └── materials/
```

### Поток данных

```
Исходники .slang
       ↓
slangc (CMake add_custom_command)
       ↓
SPIR-V .spv файлы
       ↓
VkShaderModule (vkCreateShaderModule)
       ↓
VkPipeline (Graphics/Compute)
       ↓
Рендеринг кадра
```

## VMA интеграция

```cpp
#include <vma/vk_mem_alloc.h>

std::expected<void, std::string> initVma(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkInstance instance,
    VmaAllocator& allocator)
{
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VkResult result = vmaCreateAllocator(&allocatorInfo, &allocator);
    if (result != VK_SUCCESS) {
        return std::unexpected("vmaCreateAllocator failed: " + std::to_string(result));
    }

    return {};
}
```

## Flecs ECS интеграция

### Синхронизация ECS-данных с шейдерами

```cpp
#include <flecs.h>

struct ShaderUniforms {
    glm::mat4 viewProj;
    glm::vec3 cameraPos;
    uint32_t frameIndex;
};

void syncToShader(
    flecs::world& world,
    VkBuffer uniformBuffer,
    VmaAllocator allocator)
{
    // Получаем данные из ECS
    auto cameraQuery = world.query<const CameraTransform, const CameraProjection>();

    ShaderUniforms uniforms{};
    cameraQuery.each([&](const CameraTransform& transform, const CameraProjection& proj) {
        uniforms.viewProj = proj.viewProj;
        uniforms.cameraPos = transform.position;
    });
    uniforms.frameIndex = world.global_id() & 0xFFFFFFFF;

    // Копируем в GPU буфер
    void* data;
    vmaMapMemory(allocator, uniformBufferMemory, &data);
    std::memcpy(data, &uniforms, sizeof(ShaderUniforms));
    vmaUnmapMemory(allocator, uniformBufferMemory);
}
```

## Logging Integration

### SlangLogger для перенаправления логов Slang

```cpp
// slang_logger.hpp
#pragma once

#include <slang.h>
#include <projectv/core/logging/log.hpp>

namespace projectv {

class SlangLogger : public slang::IDiagnosticSink
{
public:
    void write(const char* message, slang::Severity severity) override {
        ZoneScopedN("SlangLogger::write");
        
        switch (severity) {
            case slang::Severity::Error:
                core::Log::error("Slang", "{}", message);
                TracyPlot("Slang/Error", 1);
                break;
                
            case slang::Severity::Warning:
                core::Log::warning("Slang", "{}", message);
                TracyPlot("Slang/Warning", 1);
                break;
                
            case slang::Severity::Info:
                core::Log::info("Slang", "{}", message);
                break;
                
            case slang::Severity::Note:
            case slang::Severity::Internal:
            default:
                core::Log::debug("Slang", "{}", message);
                break;
        }
    }
};

} // namespace projectv
```

### Интеграция логгера в SlangShaderManager

```cpp
// slang_shader_manager.cpp (фрагмент)
std::expected<SlangShaderManager::ShaderModule, std::string>
SlangShaderManager::loadCached(
    const std::string& name,
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ZoneScopedN("SlangShaderManager::loadCached");
    
    auto it = moduleCache_.find(name);
    if (it != moduleCache_.end()) {
        core::Log::debug("Slang", "Shader '{}' found in cache", name);
        return it->second;
    }

    core::Log::info("Slang", "Loading shader '{}' from {}", name, path);
    
    auto module = loadSPIRV(path, entryPoint, stage);
    if (!module) {
        core::Log::error("Slang", "Failed to load shader '{}': {}", name, module.error());
        return std::unexpected(module.error());
    }

    moduleCache_[name] = *module;
    
    // Логирование успешной загрузки
    size_t codeSize = 0;
    if (module->module != VK_NULL_HANDLE) {
        // Получение размера шейдера через Vulkan (пример)
        // VkShaderModule не предоставляет размер напрямую, нужно хранить отдельно
        codeSize = 0; // Заглушка, реальный размер нужно хранить в ShaderModule
    }
    
    core::Log::info("Slang", "Shader '{}' loaded successfully ({} bytes)", name, codeSize);
    TracyPlot("Slang/ShaderLoaded", 1);
    
    return *module;
}

std::expected<SlangShaderManager::ShaderModule, std::string>
SlangShaderManager::loadSPIRV(
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ZoneScopedN("SlangShaderManager::loadSPIRV");
    
    ShaderModule shaderModule;
    shaderModule.entryPoint = entryPoint;
    shaderModule.stage = stage;

    core::Log::debug("Slang", "Reading SPIR-V file: {}", path);
    
    auto spirvCode = readSPIRVFile(path);
    if (!spirvCode) {
        core::Log::error("Slang", "Failed to read SPIR-V: {}", spirvCode.error());
        return std::unexpected(spirvCode.error());
    }

    core::Log::debug("Slang", "SPIR-V file read: {} bytes", spirvCode->size() * sizeof(uint32_t));
    
    auto module = createShaderModuleHandle(*spirvCode);
    if (!module) {
        core::Log::error("Slang", "Failed to create shader module: {}", module.error());
        return std::unexpected(module.error());
    }

    shaderModule.module = *module;
    core::Log::debug("Slang", "Shader module created successfully");
    
    return shaderModule;
}
```

### Уровни логирования для операций компиляции

```cpp
// Пример использования в ProjectV Engine
void ProjectVEngine::compileShader(const std::string& name, const std::string& source) {
    ZoneScopedN("Engine::compileShader");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    core::Log::info("Slang", "Starting compilation of shader '{}'", name);
    
    auto result = shaderManager_->compile(source, name, "main", VK_SHADER_STAGE_COMPUTE_BIT);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime
    );
    
    if (result) {
        core::Log::info("Slang", "Shader '{}' compiled successfully in {} ms", name, duration.count());
        TracyPlot("Slang/CompileSuccess", 1);
    } else {
        core::Log::error("Slang", "Shader '{}' compilation failed in {} ms: {}", 
            name, duration.count(), result.error());
        TracyPlot("Slang/CompileFailure", 1);
    }
    
    TracyPlot("Slang/CompileDurationMs", duration.count());
}
```

## Profiling Integration

### Tracy Hooks для всех операций Slang

```cpp
// slang_profiler.hpp
#pragma once

#include <tracy/Tracy.hpp>
#include <chrono>

namespace projectv {

class SlangProfiler
{
public:
    struct CompileMetrics
    {
        std::chrono::microseconds parseTime;
        std::chrono::microseconds semanticTime;
        std::chrono::microseconds codegenTime;
        std::chrono::microseconds totalTime;
        size_t spirvSize;
        bool success;
    };

    static CompileMetrics profileCompile(
        const std::string& moduleName,
        std::function<std::expected<ShaderModule, std::string>()> compileFunc)
    {
        ZoneScopedN("SlangProfiler::profileCompile");
        
        CompileMetrics metrics{};
        auto totalStart = std::chrono::high_resolution_clock::now();
        
        // Этап 1: Парсинг и анализ
        {
            ZoneScopedN("SlangParse");
            auto start = std::chrono::high_resolution_clock::now();
            // Здесь был бы вызов парсера Slang
            auto end = std::chrono::high_resolution_clock::now();
            metrics.parseTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            TracyPlot("Slang/ParseTimeUs", metrics.parseTime.count());
        }
        
        // Этап 2: Семантический анализ
        {
            ZoneScopedN("SlangSemantic");
            auto start = std::chrono::high_resolution_clock::now();
            // Здесь был бы семантический анализ
            auto end = std::chrono::high_resolution_clock::now();
            metrics.semanticTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            TracyPlot("Slang/SemanticTimeUs", metrics.semanticTime.count());
        }
        
        // Этап 3: Генерация кода
        {
            ZoneScopedN("SlangCodegen");
            auto start = std::chrono::high_resolution_clock::now();
            auto result = compileFunc();
            auto end = std::chrono::high_resolution_clock::now();
            metrics.codegenTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            metrics.success = result.has_value();
            if (result) {
                // Получение размера SPIR-V (нужно хранить в ShaderModule)
                metrics.spirvSize = 0; // Заглушка
                TracyPlot("Slang/SPIRVSize", static_cast<int64_t>(metrics.spirvSize));
            }
            
            TracyPlot("Slang/CodegenTimeUs", metrics.codegenTime.count());
            TracyPlot("Slang/CompileSuccess", metrics.success ? 1 : 0);
        }
        
        auto totalEnd = std::chrono::high_resolution_clock::now();
        metrics.totalTime = std::chrono::duration_cast<std::chrono::microseconds>(totalEnd - totalStart);
        
        TracyPlot("Slang/TotalCompileTimeUs", metrics.totalTime.count());
        TracyPlot("Slang/ShaderCount", 1);
        
        return metrics;
    }
    
    static void profileShaderCacheHit(const std::string& name) {
        ZoneScopedN("SlangCacheHit");
        TracyPlot("Slang/CacheHit", 1);
        TracyMessageL(("Shader cache hit: " + name).c_str());
    }
    
    static void profileShaderCacheMiss(const std::string& name) {
        ZoneScopedN("SlangCacheMiss");
        TracyPlot("Slang/CacheMiss", 1);
        TracyMessageL(("Shader cache miss: " + name).c_str());
    }
    
    static void profileArenaAllocation(size_t size) {
        TracyPlot("Slang/ArenaAllocationSize", static_cast<int64_t>(size));
        TracyPlot("Slang/ArenaAllocationCount", 1);
    }
    
    static void profileArenaReset() {
        TracyPlot("Slang/ArenaReset", 1);
    }
};

} // namespace projectv
```

### Интеграция Profiling в SlangShaderManager

```cpp
// slang_shader_manager.cpp (фрагмент)
std::expected<SlangShaderManager::ShaderModule, std::string>
SlangShaderManager::loadCached(
    const std::string& name,
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ZoneScopedN("SlangShaderManager::loadCached");
    
    auto it = moduleCache_.find(name);
    if (it != moduleCache_.end()) {
        SlangProfiler::profileShaderCacheHit(name);
        core::Log::debug("Slang", "Shader '{}' found in cache", name);
        return it->second;
    }

    SlangProfiler::profileShaderCacheMiss(name);
    core::Log::info("Slang", "Loading shader '{}' from {}", name, path);
    
    auto metrics = SlangProfiler::profileCompile(name, [&]() {
        return loadSPIRV(path, entryPoint, stage);
    });
    
    auto module = loadSPIRV(path, entryPoint, stage);
    if (!module) {
        core::Log::error("Slang", "Failed to load shader '{}': {}", name, module.error());
        return std::unexpected(module.error());
    }

    moduleCache_[name] = *module;
    
    core::Log::info("Slang", "Shader '{}' loaded in {} µs (parse: {}, semantic: {}, codegen: {})",
        name, metrics.totalTime.count(),
        metrics.parseTime.count(), metrics.semanticTime.count(), metrics.codegenTime.count());
    
    return *module;
}

std::expected<SlangShaderManager::ShaderModule, std::string>
SlangShaderManager::compile(
    const std::string& slangSource,
    const std::string& moduleName,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ZoneScopedN("SlangShaderManager::compile");
    
    auto metrics = SlangProfiler::profileCompile(moduleName, [&]() -> std::expected<ShaderModule, std::string> {
        ShaderModule shaderModule;
        shaderModule.entryPoint = entryPoint;
        shaderModule.stage = stage;

        Slang::ComPtr<slang::IBlob> diagnostics;
        auto module = session_->loadModuleFromSourceString(
            moduleName.c_str(),
            moduleName.c_str(),
            slangSource.c_str(),
            diagnostics.writeRef()
        );

        if (!module) {
            const char* diag = diagnostics
                ? static_cast<const char*>(diagnostics->getBufferPointer())
                : "Unknown error";
            return std::unexpected("Slang compile error: " + std::string(diag));
        }

        Slang::ComPtr<slang::IEntryPoint> slangEntry;
        module->findEntryPointByName(entryPoint.c_str(), slangEntry.writeRef());

        if (!slangEntry) {
            return std::unexpected("Entry point not found: " + entryPoint);
        }

        slang::IComponentType* components[] = {module.Get(), slangEntry.Get()};
        Slang::ComPtr<slang::IComponentType> program;
        session_->createCompositeComponentType(
            components, 2,
            program.writeRef(),
            diagnostics.writeRef()
        );

        Slang::ComPtr<slang::IComponentType> linked;
        program->link(linked.writeRef(), diagnostics.writeRef());

        Slang::ComPtr<slang::IBlob> spirvCode;
        linked->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

        if (!spirvCode) {
            return std::unexpected("Failed to generate SPIR-V");
        }

        std::vector<uint32_t> code(
            reinterpret_cast<const uint32_t*>(spirvCode->getBufferPointer()),
            reinterpret_cast<const uint32_t*>(
                static_cast<const char*>(spirvCode->getBufferPointer()) + spirvCode->getBufferSize()
            )
        );

        auto moduleHandle = createShaderModuleHandle(code);
        if (!moduleHandle) {
            return std::unexpected(moduleHandle.error());
        }

        shaderModule.module = *moduleHandle;
        shaderModule.spirvSize = spirvCode->getBufferSize(); // Сохраняем размер
        
        return shaderModule;
    });
    
    // Возвращаем результат из замыкания
    return [&]() -> std::expected<ShaderModule, std::string> {
        ShaderModule shaderModule;
        shaderModule.entryPoint = entryPoint;
        shaderModule.stage = stage;

        Slang::ComPtr<slang::IBlob> diagnostics;
        auto module = session_->loadModuleFromSourceString(
            moduleName.c_str(),
            moduleName.c_str(),
            slangSource.c_str(),
            diagnostics.writeRef()
        );

        if (!module) {
            const char* diag = diagnostics
                ? static_cast<const char*>(diagnostics->getBufferPointer())
                : "Unknown error";
            return std::unexpected("Slang compile error: " + std::string(diag));
        }

        Slang::ComPtr<slang::IEntryPoint> slangEntry;
        module->findEntryPointByName(entryPoint.c_str(), slangEntry.writeRef());

        if (!slangEntry) {
            return std::unexpected("Entry point not found: " + entryPoint);
        }

        slang::IComponentType* components[] = {module.Get(), slangEntry.Get()};
        Slang::ComPtr<slang::IComponentType> program;
        session_->createCompositeComponentType(
            components, 2,
            program.writeRef(),
            diagnostics.writeRef()
        );

        Slang::ComPtr<slang::IComponentType> linked;
        program->link(linked.writeRef(), diagnostics.writeRef());

        Slang::ComPtr<slang::IBlob> spirvCode;
        linked->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

        if (!spirvCode) {
            return std::unexpected("Failed to generate SPIR-V");
        }

        std::vector<uint32_t> code(
            reinterpret_cast<const uint32_t*>(spirvCode->getBufferPointer()),
            reinterpret_cast<const uint32_t*>(
                static_cast<const char*>(spirvCode->getBufferPointer()) + spirvCode->getBufferSize()
            )
        );

        auto moduleHandle = createShaderModuleHandle(code);
        if (!moduleHandle) {
            return std::unexpected(moduleHandle.error());
        }

        shaderModule.module = *moduleHandle;
        shaderModule.spirvSize = spirvCode->getBufferSize();
        
        return shaderModule;
    }();
}
```

### Phase 0 Compliance Check

**Проверка на нарушения Phase 0:**

1. ✅ **Нет `try-catch` блоков** - используется `std::expected` для обработки ошибок
2. ✅ **Нет `throw`** - все ошибки возвращаются через `std::unexpected`
3. ✅ **Нет `std::mutex`, `std::lock_guard`, `std::scoped_lock`** - используется lock-free подход
4. ✅ **Нет `std::thread`, `std::jthread`** - асинхронность через stdexec (в 03_advanced.md)
5. ✅ **Нет `std::future`, `std::promise`, `std::async`** - заменены на stdexec senders
6. ✅ **Нет `std::this_thread::yield()`** - заменены на `stdexec::schedule(scheduler_)`
7. ✅ **Нет исключений C++** - только `std::expected` для обработки ошибок

**Пример правильного подхода Phase 0:**

```cpp
// Правильно: использование std::expected
std::expected<ShaderModule, std::string> loadShader() {
    auto result = readFile("shader.slang");
    if (!result) {
        return std::unexpected(result.error());
    }
    return compileShader(*result);
}

// Неправильно: использование исключений (запрещено в Phase 0)
ShaderModule loadShaderWithExceptions() {
    try {
        auto data = readFileOrThrow("shader.slang");
        return compileShaderOrThrow(data);
    } catch (const std::exception& e) {
        // Запрещено!
    }
}
```

### Интеграция с глобальным ThreadPool через stdexec

```cpp
// Пример асинхронной компиляции через stdexec (детали в 03_advanced.md)
stdexec::sender auto compileShaderAsync(
    stdexec::scheduler auto scheduler,
    const std::string& source,
    const std::string& name)
{
    return stdexec::schedule(scheduler)
         | stdexec::then([=]() {
               ZoneScopedN("AsyncShaderCompile");
               return shaderManager_->compile(source, name, "main", VK_SHADER_STAGE_COMPUTE_BIT);
           })
         | stdexec::upon_error([](std::string error) {
               core::Log::error("Slang", "Async compilation failed: {}", error);
               return std::unexpected(std::move(error));
           });
}
```

## Чеклист интеграции

- [ ] slangc найден через CMake или Vulkan SDK
- [ ] Модули шейдеров организованы по частоте изменений (core → materials → render)
- [ ] SPIR-V читается в `uint32_t`-выровненный буфер
- [ ] VkShaderModule создаётся с правильным размером
- [ ] Reflection API используется для автоматического создания pipeline layout
- [ ] VMA инициализирован с `BUFFER_DEVICE_ADDRESS_BIT` для BDA
- [ ] Flecs ECS синхронизирует данные с uniform buffers каждый кадр
- [ ] Tracy профилирует время компиляции шейдеров
- [ ] Обработка ошибок через `std::expected`
