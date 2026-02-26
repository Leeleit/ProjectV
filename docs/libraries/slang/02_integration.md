# Slang: Интеграция в ProjectV

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
find_package(Slang REQUIRED)
include(cmake/ProjectVShaders.cmake)

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

## Tracy профилирование

```cpp
#include <tracy/Tracy.hpp>

void SlangShaderManager::compileWithTracing(
    const std::string& slangSource,
    const std::string& moduleName,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ZoneScopedN("SlangCompile");

    auto start = std::chrono::high_resolution_clock::now();

    auto result = compile(slangSource, moduleName, entryPoint, stage);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start
    );

    TracyPlot("Slang/CompileTimeMs", duration.count());
    TracyPlot("Slang/Success", result.has_value() ? 1 : 0);

    if (result) {
        TracyPlot("Slang/ShaderSize", static_cast<int64_t>(
            result->module != VK_NULL_HANDLE ? 1 : 0
        ));
    }
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
