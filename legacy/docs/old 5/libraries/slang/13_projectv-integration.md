# Slang в ProjectV: Интеграция

**🟡 Уровень 2: Средний** — Детали интеграции Slang в сборку ProjectV и реализация SlangShaderManager.

---

## CMake интеграция

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

# Дополнительные флаги
set(PROJECTV_SLANG_FLAGS
    -enable-slang-matrix-layout-row-major
    -fvk-use-dx-position-w
)

# Функция компиляции одного шейдера
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

# Функция компиляции группы шейдеров
function(projectv_add_shader_target TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    set(ALL_OUTPUTS)

    foreach(SRC IN LISTS ARG_SOURCES)
        get_filename_component(NAME_WE ${SRC} NAME_WE)

        # Вершинный шейдер
        set(VERT_OUT "${CMAKE_BINARY_DIR}/shaders/${NAME_WE}_vert.spv")
        projectv_compile_shader(${SRC} vsMain vertex ${VERT_OUT})
        list(APPEND ALL_OUTPUTS ${VERT_OUT})

        # Фрагментный шейдер
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

---

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

    // Запрет копирования
    SlangShaderManager(const SlangShaderManager&) = delete;
    SlangShaderManager& operator=(const SlangShaderManager&) = delete;

    // Инициализация Slang API
    bool initialize(const std::vector<std::string>& searchPaths);

    // Загрузка precompiled SPIR-V
    ShaderModule loadSPIRV(
        const std::string& path,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    // Загрузка с кэшированием
    ShaderModule loadCached(
        const std::string& name,
        const std::string& path,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

#ifdef PROJECTV_SHADER_HOT_RELOAD
    // Runtime компиляция
    ShaderModule compile(
        const std::string& slangSource,
        const std::string& moduleName,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage
    );

    // Горячая перезагрузка
    void hotReload(const std::string& name);

    // Проверка изменений
    void checkForChanges();
#endif

    // Очистка кэша
    void clearCache();

    // Уничтожение модулей
    void destroyModule(const std::string& name);

private:
    VkDevice device_;

    // Slang API
    Slang::ComPtr<slang::IGlobalSession> globalSession_;
    Slang::ComPtr<slang::ISession> session_;

    // Кэш модулей
    std::unordered_map<std::string, ShaderModule> moduleCache_;

    // Чтение SPIR-V файла
    std::vector<uint32_t> readSPIRVFile(const std::string& path);

    // Создание VkShaderModule
    VkShaderModule createShaderModule(const std::vector<uint32_t>& code);
};

} // namespace projectv
```

### Реализация

```cpp
// src/renderer/slang_shader_manager.cpp
#include "slang_shader_manager.hpp"
#include <fstream>
#include <iostream>

namespace projectv {

SlangShaderManager::SlangShaderManager(VkDevice device)
    : device_(device)
{
}

SlangShaderManager::~SlangShaderManager()
{
    clearCache();
}

bool SlangShaderManager::initialize(const std::vector<std::string>& searchPaths)
{
    // Создание глобальной сессии
    SlangResult result = slang::createGlobalSession(globalSession_.writeRef());
    if (SLANG_FAILED(result))
    {
        std::cerr << "Failed to create Slang global session\n";
        return false;
    }

    // Настройка цели компиляции
    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession_->findProfile("spirv_1_5");
    targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    // Настройка сессии
    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    // Пути поиска
    std::vector<const char*> pathPtrs;
    for (const auto& path : searchPaths)
    {
        pathPtrs.push_back(path.c_str());
    }
    sessionDesc.searchPaths = pathPtrs.data();
    sessionDesc.searchPathCount = static_cast<int>(pathPtrs.size());

    // Создание сессии
    result = globalSession_->createSession(sessionDesc, session_.writeRef());
    if (SLANG_FAILED(result))
    {
        std::cerr << "Failed to create Slang session\n";
        return false;
    }

    return true;
}

ShaderModule SlangShaderManager::loadSPIRV(
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ShaderModule shaderModule;
    shaderModule.entryPoint = entryPoint;
    shaderModule.stage = stage;

    auto spirvCode = readSPIRVFile(path);
    if (spirvCode.empty())
    {
        std::cerr << "Failed to read SPIR-V: " << path << "\n";
        return shaderModule;
    }

    shaderModule.module = createShaderModule(spirvCode);
    return shaderModule;
}

ShaderModule SlangShaderManager::loadCached(
    const std::string& name,
    const std::string& path,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    auto it = moduleCache_.find(name);
    if (it != moduleCache_.end())
    {
        return it->second;
    }

    auto module = loadSPIRV(path, entryPoint, stage);
    moduleCache_[name] = module;
    return module;
}

void SlangShaderManager::clearCache()
{
    for (auto& [name, module] : moduleCache_)
    {
        if (module.module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device_, module.module, nullptr);
        }
    }
    moduleCache_.clear();
}

void SlangShaderManager::destroyModule(const std::string& name)
{
    auto it = moduleCache_.find(name);
    if (it != moduleCache_.end())
    {
        if (it->second.module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device_, it->second.module, nullptr);
        }
        moduleCache_.erase(it);
    }
}

std::vector<uint32_t> SlangShaderManager::readSPIRVFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return {};
    }

    size_t size = file.tellg();
    if (size % sizeof(uint32_t) != 0)
    {
        return {};
    }

    file.seekg(0);

    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), size);

    return code;
}

VkShaderModule SlangShaderManager::createShaderModule(const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(device_, &createInfo, nullptr, &module);

    if (result != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    return module;
}

#ifdef PROJECTV_SHADER_HOT_RELOAD
ShaderModule SlangShaderManager::compile(
    const std::string& slangSource,
    const std::string& moduleName,
    const std::string& entryPoint,
    VkShaderStageFlagBits stage)
{
    ShaderModule shaderModule;
    shaderModule.entryPoint = entryPoint;
    shaderModule.stage = stage;

    // Загрузка модуля
    Slang::ComPtr<slang::IBlob> diagnostics;
    auto module = session_->loadModuleFromSourceString(
        moduleName.c_str(),
        moduleName.c_str(),
        slangSource.c_str(),
        diagnostics.writeRef()
    );

    if (!module)
    {
        const char* diag = diagnostics
            ? static_cast<const char*>(diagnostics->getBufferPointer())
            : "Unknown error";
        std::cerr << "Slang compile error: " << diag << "\n";
        return shaderModule;
    }

    // Получение entry point
    Slang::ComPtr<slang::IEntryPoint> entry;
    module->findEntryPointByName(entryPoint.c_str(), entry.writeRef());

    if (!entry)
    {
        std::cerr << "Entry point not found: " << entryPoint << "\n";
        return shaderModule;
    }

    // Линковка
    slang::IComponentType* components[] = { module, entry };
    Slang::ComPtr<slang::IComponentType> program;
    session_->createCompositeComponentType(
        components, 2,
        program.writeRef(),
        diagnostics.writeRef()
    );

    Slang::ComPtr<slang::IComponentType> linked;
    program->link(linked.writeRef(), diagnostics.writeRef());

    // Получение SPIR-V
    Slang::ComPtr<slang::IBlob> spirvCode;
    linked->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

    if (!spirvCode)
    {
        std::cerr << "Failed to generate SPIR-V\n";
        return shaderModule;
    }

    // Создание VkShaderModule
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode->getBufferSize();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(spirvCode->getBufferPointer());

    vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule.module);

    return shaderModule;
}
#endif

} // namespace projectv
```

---

## Использование в рендерере

```cpp
// src/renderer/vulkan_renderer.cpp
#include "slang_shader_manager.hpp"

class VulkanRenderer
{
public:
    void initialize(VkDevice device)
    {
        shaderManager_ = std::make_unique<SlangShaderManager>(device);

        shaderManager_->initialize({
            "shaders/core",
            "shaders/voxel",
            "shaders/materials"
        });

        loadShaders();
    }

    void loadShaders()
    {
        // G-buffer шейдеры
        auto vertShader = shaderManager_->loadCached(
            "gbuffer_vert",
            "build/shaders/gbuffer_vert.spv",
            "vsMain",
            VK_SHADER_STAGE_VERTEX_BIT
        );

        auto fragShader = shaderManager_->loadCached(
            "gbuffer_frag",
            "build/shaders/gbuffer_frag.spv",
            "fsMain",
            VK_SHADER_STAGE_FRAGMENT_BIT
        );

        // Создание pipeline
        createGBufferPipeline(vertShader, fragShader);
    }

private:
    std::unique_ptr<SlangShaderManager> shaderManager_;
};
```

---

## Следующий раздел

- **14. Паттерны ProjectV** — специфичные решения для воксельного рендеринга