# Slang: Продвинутые оптимизации и DOD

## Data-Oriented Design для шейдеров

### SoA vs AoS в шейдерах

Традиционный подход (AoS — Array of Structures) хранит данные вершины вместе:

```slang
// AoS — плохо для cache locality при обработке одного поля
struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
    float4 color;
};

StructuredBuffer<Vertex> vertices;
```

SoA (Structure of Arrays) разделяет данные для последовательного доступа:

```slang
// SoA — хорошо для cache locality
struct VertexBufferSoA
{
    float3 positions[];
    float3 normals[];
    float2 uvs[];
    float4 colors[];
};

StructuredBuffer<VertexBufferSoA> vertexBuffers;

// Доступ: обрабатываем только нужное поле
float3 p = vertexBuffers[0].positions[index];
```

> **Для понимания:** AoS — как шкаф с ящиками, где в каждом ящике лежит полный набор вещей (одежда + обувь +
> аксессуары). Чтобы перебрать только обувь, нужно открыть все ящики. SoA — как несколько шкафов: один только с одеждой,
> другой только с обувью. Перебрать всю обувь = открыть один шкаф.

### Hot/Cold Data Separation

Горячие данные (читаются каждый кадр):

- Позиции вершин, нормали
- Матрицы трансформации
- Индексы видимых объектов

Холодные данные (читаются редко):

- Метаданные материалов
- Параметры компиляции
- Таблицы поиска (LUT)

```slang
// Hot data — часто обновляется
struct FrameConstants
{
    float4x4 viewProj;
    float4x4 invViewProj;
    float3 cameraPos;
    float time;
    uint frameIndex;
};

// Cold data — статичные параметры
struct MaterialConstants
{
    float4 baseColor;
    float metallic;
    float roughness;
    float padding;
};
```

### Cache Line Alignment в C++

```cpp
alignas(64) struct alignas(64) ShaderParameterBlock {
    float4x4 viewProj;
    float3 cameraPos;
    float padding1;    // 4 байта → выравнивание до 16 байт
    uint frameIndex;
    float padding2[3]; // Выравнивание до 64 байт
};

static_assert(sizeof(ShaderParameterBlock) == 64, "Cache line aligned");
static_assert(alignof(ShaderParameterBlock) == 64, "Cache line aligned");
```

## Zero-Copy загрузка шейдеров

### Memory Mapping больших шейдеров

```cpp
#include <sys/mman.h>
#include <fcntl.h>

class MappedShaderSource {
public:
    bool load(const std::string& path) {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;

        struct stat sb;
        fstat(fd, &sb);
        m_size = sb.st_size;

        m_data = mmap(nullptr, m_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m_data == MAP_FAILED) {
            close(fd);
            return false;
        }

        // Подсказка ядру о паттерне доступа
        madvise(m_data, m_size, MADV_SEQUENTIAL);
        return true;
    }

    std::span<const char> getSource() const {
        return {static_cast<const char*>(m_data), m_size};
    }

    ~MappedShaderSource() {
        if (m_data) munmap(m_data, m_size);
    }

private:
    size_t m_size = 0;
    void* m_data = nullptr;
};
```

### Потоковая компиляция

```cpp
class StreamingShaderCompiler {
public:
    std::expected<VkShaderModule, std::string> compileAndUpload(
        const MappedShaderSource& source,
        slang::ISession* session,
        VkDevice device,
        const std::string& entryPoint,
        VkShaderStageFlagBits stage)
    {
        Slang::ComPtr<slang::IBlob> diagnostics;

        // Компиляция
        auto module = session->loadModuleFromSourceString(
            "stream_module",
            "stream_module",
            source.getSource().data(),
            diagnostics.writeRef()
        );

        if (!module) {
            return std::unexpected("Compilation failed");
        }

        // Получение SPIR-V
        Slang::ComPtr<slang::IBlob> spirvCode;
        module->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

        // Создание VkShaderModule
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = spirvCode->getBufferSize();
        createInfo.pCode = spirvCode->getBufferPointer();

        VkShaderModule moduleHandle;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &moduleHandle) != VK_SUCCESS) {
            return std::unexpected("Failed to create shader module");
        }

        return moduleHandle;
    }
};
```

## Многопоточная компиляция

### Параллельная компиляция шейдеров через stdexec

```cpp
#include <stdexec/execution.hpp>
#include <print>
#include <expected>

namespace stdex = stdexec;

class StdexecShaderCompiler {
public:
    struct CompileTask {
        std::string sourcePath;
        std::string entryPoint;
        VkShaderStageFlagBits stage;
    };

    struct CompileResult {
        std::expected<VkShaderModule, std::string> module;
        std::string sourcePath;
        VkShaderStageFlagBits stage;
    };

    explicit StdexecShaderCompiler(stdex::scheduler auto scheduler)
        : scheduler_(scheduler) {}

    // Возвращает sender для асинхронной компиляции
    auto compileAsync(const CompileTask& task)
        -> stdex::sender auto {
        
        return stdex::schedule(scheduler_)
             | stdex::then([task]() -> CompileResult {
                    ZoneScopedN("ShaderCompileAsync");
                    
                    // Компиляция в пуле потоков
                    auto module = compileShader(task);
                    
                    return CompileResult{
                        .module = std::move(module),
                        .sourcePath = task.sourcePath,
                        .stage = task.stage
                    };
                });
    }

    // Пакетная компиляция нескольких шейдеров
    auto compileBatch(std::span<const CompileTask> tasks)
        -> stdex::sender auto {
        
        return stdex::schedule(scheduler_)
             | stdex::bulk(tasks.size(), [tasks](size_t idx) {
                    ZoneScopedN("ShaderBatchCompile");
                    
                    const auto& task = tasks[idx];
                    auto module = compileShader(task);
                    
                    return CompileResult{
                        .module = std::move(module),
                        .sourcePath = task.sourcePath,
                        .stage = task.stage
                    };
                });
    }

    // Компиляция с приоритетом (например, для критичных шейдеров)
    auto compileWithPriority(const CompileTask& task, uint32_t priority)
        -> stdex::sender auto {
        
        // В реальной реализации здесь была бы логика приоритетного планирования
        // Для примера просто передаём в обычный compileAsync
        return compileAsync(task);
    }

private:
    static std::expected<VkShaderModule, std::string> compileShader(const CompileTask& task) {
        // Thread-local slang session для избежания contention
        thread_local auto session = createThreadLocalSession();
        
        try {
            // Загрузка исходного кода
            MappedShaderSource source;
            if (!source.load(task.sourcePath)) {
                return std::unexpected("Failed to load shader source");
            }

            // Компиляция через slang
            Slang::ComPtr<slang::IBlob> diagnostics;
            auto module = session->loadModuleFromSourceString(
                "shader_module",
                task.entryPoint.c_str(),
                source.getSource().data(),
                diagnostics.writeRef()
            );

            if (!module) {
                return std::unexpected("Compilation failed");
            }

            // Генерация SPIR-V
            Slang::ComPtr<slang::IBlob> spirvCode;
            module->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnostics.writeRef());

            // Создание VkShaderModule
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = spirvCode->getBufferSize();
            createInfo.pCode = spirvCode->getBufferPointer();

            VkShaderModule moduleHandle;
            if (vkCreateShaderModule(device, &createInfo, nullptr, &moduleHandle) != VK_SUCCESS) {
                return std::unexpected("Failed to create shader module");
            }

            return moduleHandle;
        } catch (const std::exception& e) {
            return std::unexpected(std::format("Compilation error: {}", e.what()));
        }
    }

    static Slang::ComPtr<slang::ISession> createThreadLocalSession() {
        // Создание thread-local сессии slang
        Slang::ComPtr<slang::IGlobalSession> globalSession;
        slang::createGlobalSession(globalSession.writeRef());

        slang::TargetDesc targetDesc{};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = globalSession->findProfile("spirv_1_5");

        slang::SessionDesc sessionDesc{};
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;

        Slang::ComPtr<slang::ISession> session;
        globalSession->createSession(sessionDesc, session.writeRef());
        
        return session;
    }

    stdex::scheduler auto scheduler_;
};

// Интеграция с глобальным ThreadPool ProjectV
class ProjectVShaderCompiler {
public:
    ProjectVShaderCompiler(stdex::scheduler auto global_scheduler)
        : compiler_(global_scheduler) {}

    // Интерфейс для ECS системы
    auto compileForEntity(const StdexecShaderCompiler::CompileTask& task,
                         uint32_t priority = 0)
        -> stdex::sender auto {
        
        return compiler_.compileWithPriority(task, priority)
             | stdex::then([](StdexecShaderCompiler::CompileResult result) {
                    if (result.module) {
                        return std::move(*result.module);
                    }
                    return std::unexpected<VkShaderModule, std::string>(
                        std::move(result.module.error())
                    );
                });
    }

    // Пакетная компиляция для нескольких сущностей
    auto compileForEntities(std::span<const StdexecShaderCompiler::CompileTask> tasks,
                           std::span<const uint32_t> priorities)
        -> stdex::sender auto {
        
        return compiler_.compileBatch(tasks)
             | stdex::then([](std::vector<StdexecShaderCompiler::CompileResult> results) {
                    std::vector<std::expected<VkShaderModule, std::string>> compiled;
                    compiled.reserve(results.size());
                    
                    for (auto& result : results) {
                        if (result.module) {
                            compiled.push_back(std::move(*result.module));
                        } else {
                            compiled.push_back(std::unexpected(result.module.error()));
                        }
                    }
                    
                    return compiled;
                });
    }

private:
    StdexecShaderCompiler compiler_;
};
```

### Thread-Local сессия

```cpp
class ThreadLocalSlangSession {
public:
    slang::ISession* get() {
        if (!m_session) {
            m_session = createSession();
        }
        return m_session.get();
    }

private:
    thread_local Slang::ComPtr<slang::ISession> m_session;

    Slang::ComPtr<slang::ISession> createSession() {
        Slang::ComPtr<slang::ISession> session;

        slang::TargetDesc targetDesc{};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = m_globalSession->findProfile("spirv_1_5");

        slang::SessionDesc sessionDesc{};
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;

        m_globalSession->createSession(sessionDesc, session.writeRef());
        return session;
    }

    Slang::ComPtr<slang::IGlobalSession> m_globalSession;
};
```

## GPU-Driven паттерны

### Indirect Drawing с compute shader

```slang
// Compute shader: определяет что рисовать
[numthreads(64, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= params.objectCount) return;

    Object object = objects[id.x];

    // Frustum culling
    if (!isInFrustum(object.aabb, params.frustum)) return;

    // LOD selection
    float dist = distance(object.position, params.cameraPos);
    uint lod = selectLOD(dist);

    // Запись в indirect buffer
    uint slot;
    InterlockedAdd(drawCount[0], 1, slot);

    DrawCommand cmd;
    cmd.vertexCount = lodVertexCounts[lod];
    cmd.instanceCount = 1;
    cmd.firstVertex = lodVertexOffsets[lod];
    cmd.firstInstance = slot;

    drawCommands[slot] = cmd;
}
```

### Buffer Device Address для zero-copy

```slang
// Shader получает указатели на буфера через push constants
[[vk::push_constant]]
struct DrawParams
{
    DeviceBuffer objects;      // Указатель на массив объектов
    DeviceBuffer drawCommands; // Указатель на indirect commands
    DeviceBuffer materials;   // Указатель на данные материалов
    uint objectCount;
} params;

// GPU-driven rendering: CPU не трогает данные
[numthreads(64, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= params.objectCount) return;

    Object obj = params.objects.data[id.x];
    Material mat = params.materials.data[obj.materialId];

    // Полная обработка на GPU
    float4 worldPos = mul(params.modelMatrix, float4(obj.position, 1.0));
    float4 clipPos = mul(params.viewProj, worldPos);
    float3 normal = normalize(mul((float3x3)obj.normalMatrix, mat.normal));

    // Фрагментный shader должен вывести clipPos.w для depth
    output.depth = clipPos.z / clipPos.w;
}
```

```cpp
// C++: только передача адресов
void updateDrawParams(VkCommandBuffer cmd, VkBuffer objectBuffer, VkBuffer commandBuffer) {
    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = objectBuffer;
    VkDeviceAddress objectAddr = vkGetBufferDeviceAddress(device, &addrInfo);

    addrInfo.buffer = commandBuffer;
    VkDeviceAddress commandAddr = vkGetBufferDeviceAddress(device, &addrInfo);

    DrawParams params{};
    params.objects = objectAddr;
    params.drawCommands = commandAddr;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(params), &params);
}
```

## Оптимизация descriptor sets

### Bindless Rendering

```slang
// Массив текстур без фиксированного binding
[[vk::binding(0, 0)]]
Texture2D materialTextures[];  // 1000+ текстур

[[vk::binding(1, 0)]]
SamplerState linearSampler;

float4 sampleMaterial(uint textureIndex, float2 uv)
{
    // Динамический индекс без изменения pipeline
    return materialTextures[NonUniformResourceIndex(textureIndex)]
        .Sample(linearSampler, uv);
}
```

```cpp
// C++: создание descriptor set с variable descriptor count
VkDescriptorSetLayoutBinding binding{};
binding.binding = 0;
binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
binding.descriptorCount = 1024;  // Максимум текстур
binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
flagsInfo.pBindingFlags = &bindingFlags;

VkDescriptorBindingFlags bindingFlags =
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
```

### Descriptor Pool Management

```cpp
class DescriptorPoolManager {
public:
    void initialize(VkDevice device, uint32_t maxSets) {
        m_device = device;

        // Preallocated pools для разных типов descriptors
        VkDescriptorPoolSize sizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 64 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256 },
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = maxSets;
        poolInfo.poolSizeCount = std::size(sizes);
        poolInfo.pPoolSizes = sizes;

        vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool);
    }

    std::expected<VkDescriptorSet, std::string> allocate(VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet set;
        if (vkAllocateDescriptorSets(m_device, &allocInfo, &set) != VK_SUCCESS) {
            return std::unexpected("Failed to allocate descriptor set");
        }

        return set;
    }

private:
    VkDevice m_device;
    VkDescriptorPool m_pool;
};
```

## Профилирование и отладка

### Tracy интеграция

```cpp
#include <tracy/Tracy.hpp>

class ProfiledShaderCompiler {
public:
    void compileWithProfiling(
        const std::string& source,
        const std::string& name)
    {
        ZoneScopedN("ShaderCompile");

        auto start = std::chrono::high_resolution_clock::now();

        // Компиляция
        auto result = compile(source);

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start
        );

        TracyPlot("Shader/CompileTimeUs", duration.count());
        TracyPlot("Shader/Success", result.has_value() ? 1 : 0);

        if (result) {
            TracyPlot("Shader/SizeBytes",
                static_cast<int64_t>(result->size()));
        }
    }

    void loadWithProfiling(const std::string& path) {
        ZoneScopedN("ShaderLoad");

        auto start = std::chrono::high_resolution_clock::now();
        auto data = readFile(path);
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start
        );

        TracyPlot("Shader/LoadTimeUs", duration.count());
    }
};
```

### Анализ SPIR-V

```cpp
class SPIRVAnalyzer {
public:
    struct Stats {
        uint32_t instructions;
        uint32_t functions;
        uint32_t uniforms;
        uint32_t branchComplexity;
    };

    Stats analyze(const std::span<const uint32_t> spirv) {
        Stats stats{};

        // Parse SPIR-V header
        if (spirv.size() < 5) return stats;

        uint32_t idBound = spirv[4];

        // Count OpInstructions (упрощённо)
        for (size_t i = 5; i < spirv.size(); ) {
            uint32_t op = spirv[i] & 0xFFFF;
            uint32_t wordCount = spirv[i] >> 16;

            stats.instructions++;

            switch (op) {
                case 54:  // OpFunction
                    stats.functions++;
                    break;
                case 59:  // OpVariable (uniform)
                    stats.uniforms++;
                    break;
                case 330: // OpSelectionMerge
                case 331: // OpBranchConditional
                    stats.branchComplexity++;
                    break;
            }

            i += wordCount;
        }

        return stats;
    }
};
```

## Оптимизация времени компиляции

### Модульное кэширование

```cmake
# CMake: включение кэша эффектов
set(SLANG_EFFECT_CACHE_DIR "${CMAKE_BINARY_DIR}/.slang_cache")

# Флаги для кэширования
set(COMPILE_FLAGS
    ${COMPILE_FLAGS}
    "-enable-effect-cache"
)
```

### Инкрементальная компиляция

```cpp
class IncrementalShaderCompiler {
public:
    struct CompilationResult {
        std::vector<uint32_t> spirv;
        std::string moduleHash;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::expected<CompilationResult, std::string> compileIncremental(
        const std::string& sourcePath,
        slang::ISession* session)
    {
        // Вычисляем хэш исходника
        auto sourceHash = computeFileHash(sourcePath);

        // Проверяем кэш
        if (auto cached = getFromCache(sourceHash)) {
            TracyPlot("Shader/CacheHit", 1);
            return cached;
        }

        TracyPlot("Shader/CacheHit", 0);

        // Компилируем
        auto result = doCompile(sourcePath, session);
        if (!result) return result;

        // Сохраняем в кэш
        saveToCache(sourceHash, *result);

        return result;
    }

private:
    std::string computeFileHash(const std::string& path) {
        // MD5 или xxHash для быстрого хэширования
        // Пример с xxHash64 (требует подключения xxhash.h)
        XXH64_hash_t hash = XXH64(path.data(), path.size(), 0);
        return std::to_string(hash);
    }

    std::expected<CompilationResult, std::string> getFromCache(const std::string& hash) {
        auto it = m_cache.find(hash);
        if (it == m_cache.end()) {
            return std::unexpected(std::errc::no_such_file_or_directory);
        }

        // Проверяем timestamp
        auto now = std::chrono::steady_clock::now();
        if (now - it->second.timestamp > m_maxAge) {
            m_cache.erase(it);
            return std::unexpected(std::errc::timed_out);
        }

        return it->second;
    }

    void saveToCache(const std::string& hash, const CompilationResult& result) {
        m_cache[hash] = result;
    }

    std::chrono::seconds m_maxAge{3600};  // 1 час
    std::unordered_map<std::string, CompilationResult> m_cache;
};
```

### Batch компиляция

```cpp
class BatchShaderCompiler {
public:
    std::vector<std::expected<VkShaderModule, std::string>> compileAll(
        const std::vector<ShaderSource>& sources,
        slang::ISession* session,
        VkDevice device)
    {
        std::vector<std::future<CompileResult>> futures;

        // Параллельная отправка задач
        for (const auto& src : sources) {
            futures.push_back(m_threadPool.submit([=, &session] {
                return compileOne(src, session, device);
            }));
        }

        // Сбор результатов
        std::vector<std::expected<VkShaderModule, std::string>> results;
        results.reserve(futures.size());

        for (auto& f : futures) {
            results.push_back(f.get());
        }

        return results;
    }

private:
    ThreadPool m_threadPool;  // M:N job system
};
```

## Алерты и best practices

### Правило 1: Минимум перекомпиляций

```cmake
# Плохо: перекомпилировать все шейдеры при любом изменении
add_custom_command(ALL DEPENDS ${ALL_SHADERS})

# Хорошо: инкрементальная компиляция через CMake
# Только изменённые файлы перекомпилируются
```

### Правило 2: Separate Hot и Cold модули

```slang
// hot/module.slang — часто меняется
module Hot;

// Cold модуль — кэшируется
module Cold;
export float coldFunction();
```

### Правило 3: Избегайте runtime компиляции в hot path

```cpp
// Плохо: компиляция каждый кадр
void renderFrame() {
    auto shader = compileShader(source);  // НИКОГДА
}

// Хорошо: compile-time или загрузка precompiled
VkShaderModule shader = loadPrecompiled("gbuffer.spv");
```

### Правило 4: Предварительное выделение памяти

```cpp
// Предварительно распределяем буфера для SPIR-V
std::vector<uint32_t> spirvBuffer;
spirvBuffer.reserve(1024 * 1024);  // 1MB preallocated
```

### Правило 5: используйте specialization константы вместо branch

```slang
// Плохо: динамическая ветка
if (params.enableShading) { /* ... */ }

// Хорошо: специализационная константа
[[vk::constant_id(0)]]
const bool ENABLE_SHADING = true;

[shader("fragment")]
void main() {
    if (ENABLE_SHADING) { /* compile-time branch */ }
}
```

### Правило 6: Minimize descriptor set changes

```cpp
// Группируем descriptor updates
void bindFrameDescriptors(VkCommandBuffer cmd, FrameData& frame) {
    // Один батч вместо множества отдельных bind
    vkUpdateDescriptorSets(cmd, frame.descriptorWrites.size(),
        frame.descriptorWrites.data(), 0, nullptr);
}
```
