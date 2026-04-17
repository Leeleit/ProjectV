# Интеграция Tracy в ProjectV

**🟡 Уровень 2: Средний**

## На этой странице

- [Обзор интеграции](#обзор-интеграции)
- [Vulkan Profiling](#vulkan-profiling)
- [ECS (flecs) Profiling](#ecs-flecs-profiling)
- [Memory Profiling (VMA)](#memory-profiling-vma)
- [Воксельные паттерны профилирования](#воксельные-паттерны-профилирования)
- [Производительность воксельного движка](#производительность-воксельного-движка)
- [Примеры кода ProjectV](#примеры-кода-projectv)

---

## Обзор интеграции

ProjectV использует Tracy для профилирования всех компонентов воксельного движка:

```mermaid
flowchart TD
    Tracy[Tracy Profiler] --> Vulkan[Vulkan GPU Profiling]
    Tracy --> ECS[ECS System Profiling]
    Tracy --> Memory[Memory Profiling (VMA)]
    Tracy --> Voxels[Voxel Algorithms]
    
    Vulkan --> Render[Voxel Rendering]
    Vulkan --> Compute[Compute Shaders]
    
    ECS --> Physics[JoltPhysics Systems]
    ECS --> Audio[miniaudio Systems]
    
    Memory --> Chunks[Voxel Chunk Memory]
    Memory --> Textures[Texture Memory]
    
    Voxels --> Generation[Chunk Generation]
    Voxels --> Meshing[Mesh Generation]
    Voxels --> Lighting[Lighting Calculation]
```

## Vulkan Profiling

### Инициализация Vulkan контекста

```cpp
// Инициализация Tracy Vulkan контекста с volk
VkPhysicalDevice physicalDevice = ...;
VkDevice device = ...;
VkQueue queue = ...;
VkCommandBuffer cmdbuf = ...;

TracyVkContext tracyCtx = TracyVkContext(
    physicalDevice, device, queue, cmdbuf,
    vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
    vkGetCalibratedTimestampsEXT
);
```

### Профилирование воксельного рендеринга

```cpp
void renderVoxelChunk(VkCommandBuffer cmdBuffer, const VoxelChunk& chunk) {
    TracyVkZone(tracyCtx, cmdBuffer, "RenderVoxelChunk");
    
    // Настройка pipeline для вокселей
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, voxelPipeline);
    
    // Привязка буферов с воксельными данными
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &chunk.vertexBuffer, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, chunk.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Отрисовка вокселей
    vkCmdDrawIndexed(cmdBuffer, chunk.indexCount, 1, 0, 0, 0);
    
    // Сбор данных Tracy
    TracyVkCollect(tracyCtx, cmdBuffer);
}
```

### Профилирование compute шейдеров

```cpp
void dispatchVoxelCompute(VkCommandBuffer cmdBuffer, uint32_t voxelCount) {
    TracyVkZone(tracyCtx, cmdBuffer, "VoxelCompute");
    
    // Диспатч compute шейдера для обработки вокселей
    uint32_t groupCount = (voxelCount + 255) / 256;
    vkCmdDispatch(cmdBuffer, groupCount, 1, 1);
    
    TracyVkCollect(tracyCtx, cmdBuffer);
}
```

## ECS (flecs) Profiling

### Автоматическое профилирование систем

```cpp
// Макрос для автоматического профилирования ECS систем
#define TRACY_ECS_SYSTEM(world, name, phase, ...) \
    world.system<__VA_ARGS__>(#name).kind(phase).iter([](flecs::iter& it) { \
        ZoneScopedN(#name); \
        TracyPlot(#name "_Entities", (int64_t)it.count()); \
        // Реализация системы...

void setupECSProfiling(flecs::world& world) {
    // Профилирование системы физики вокселей
    TRACY_ECS_SYSTEM(world, UpdateVoxelPhysics, EcsOnUpdate, 
                     VoxelTransform, VoxelPhysics);
    
    // Профилирование системы рендеринга вокселей
    TRACY_ECS_SYSTEM(world, RenderVoxels, EcsOnStore, 
                     VoxelRenderable, VoxelTransform);
    
    // Профилирование системы загрузки чанков
    TRACY_ECS_SYSTEM(world, LoadVoxelChunks, EcsPostUpdate,
                     VoxelChunkLoader, VoxelPosition);
}
```

### Профилирование запросов и итераций

```cpp
// Профилирование сложных запросов
void processVoxelQueries(flecs::world& world) {
    ZoneScopedN("VoxelQueries");
    
    // Запрос для обработки вокселей с физикой
    flecs::query<VoxelTransform, VoxelPhysics> physicsQuery = world.query<VoxelTransform, VoxelPhysics>();
    
    TracyPlot("PhysicsEntities", (int64_t)physicsQuery.count());
    
    physicsQuery.each([](VoxelTransform& transform, VoxelPhysics& physics) {
        // Обработка физики для каждого вокселя
    });
}
```

## Memory Profiling (VMA)

### Отслеживание аллокаций VMA

```cpp
// Аллокация памяти для воксельных данных через VMA
VmaAllocator allocator = ...;

VkBufferCreateInfo bufferInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = voxelDataSize,
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
};

VmaAllocationCreateInfo allocCreateInfo = {
    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
};

VkBuffer voxelBuffer;
VmaAllocation allocation;
VmaAllocationInfo allocInfo;

// Аллокация
vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, 
                &voxelBuffer, &allocation, &allocInfo);

// Отслеживание в Tracy
TracyAlloc(allocInfo.pMappedData, allocInfo.size);
```

### Освобождение памяти с отслеживанием

```cpp
void freeVoxelBuffer(VkBuffer buffer, VmaAllocation allocation, VmaAllocator allocator) {
    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(allocator, allocation, &allocInfo);
    
    // Отслеживание освобождения
    TracyFree(allocInfo.pMappedData);
    
    // Освобождение VMA
    vmaDestroyBuffer(allocator, buffer, allocation);
}
```

### Профилирование пулов памяти для вокселей

```cpp
class VoxelMemoryPool {
    VmaPool pool;
    VmaAllocator allocator;
    
public:
    void* allocateVoxelData(size_t size, const char* name) {
        VkBufferCreateInfo bufferInfo = { /* ... */ };
        VmaAllocationCreateInfo allocCreateInfo = { /* ... */ };
        VmaAllocation allocation;
        VmaAllocationInfo allocInfo;
        
        vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, 
                       nullptr, &allocation, &allocInfo);
        
        // Отслеживание с именем типа
        TracyAllocN(allocInfo.pMappedData, allocInfo.size, name);
        
        return allocInfo.pMappedData;
    }
    
    void freeVoxelData(void* data) {
        // Поиск allocation по data...
        TracyFree(data);
        // Освобождение VMA...
    }
};
```

## Воксельные паттерны профилирования

### Профилирование генерации чанков

```cpp
void generateVoxelChunk(Chunk& chunk, int x, int y, int z) {
    ZoneScopedN("GenerateChunk");
    TracyMessageL(("Generating chunk at " + std::to_string(x) + "," + 
                   std::to_string(y) + "," + std::to_string(z)).c_str());
    
    // Измерение времени разных этапов генерации
    {
        ZoneScopedN("TerrainGeneration");
        generateTerrain(chunk);
        TracyPlot("TerrainTime", getElapsedTimeMs());
    }
    
    {
        ZoneScopedN("CaveGeneration");
        generateCaves(chunk);
        TracyPlot("CaveTime", getElapsedTimeMs());
    }
    
    {
        ZoneScopedN("StructurePlacement");
        placeStructures(chunk);
        TracyPlot("StructureTime", getElapsedTimeMs());
    }
    
    TracyPlot("TotalChunkGenTime", getTotalTimeMs());
    TracyPlot("ChunksGenerated", 1);
}
```

### Профилирование мешинга вокселей

```cpp
void generateMeshFromVoxels(const VoxelGrid& voxels, Mesh& mesh) {
    ZoneScopedN("VoxelMeshing");
    
    // Разные алгоритмы мешинга
    TracyPlot("VoxelCount", (int64_t)voxels.count());
    
    if (useGreedyMeshing) {
        ZoneScopedN("GreedyMeshing");
        greedyMesh(voxels, mesh);
        TracyPlot("GreedyTriangles", (int64_t)mesh.triangleCount);
    } else {
        ZoneScopedN("NaiveMeshing");
        naiveMesh(voxels, mesh);
        TracyPlot("NaiveTriangles", (int64_t)mesh.triangleCount);
    }
    
    TracyPlot("TotalTriangles", (int64_t)mesh.triangleCount);
}
```

### Профилирование освещения вокселей

```cpp
void calculateVoxelLighting(VoxelGrid& voxels, const LightSources& lights) {
    ZoneScopedN("VoxelLighting");
    
    // Анализ производительности разных алгоритмов освещения
    TracyPlot("LightCount", (int64_t)lights.count());
    TracyPlot("LitVoxels", (int64_t)voxels.litCount());
    
    {
        ZoneScopedN("DirectLighting");
        calculateDirectLighting(voxels, lights);
        TracyPlot("DirectLightTime", getElapsedTimeMs());
    }
    
    {
        ZoneScopedN("GlobalIllumination");
        calculateGlobalIllumination(voxels);
        TracyPlot("GITime", getElapsedTimeMs());
    }
    
    {
        ZoneScopedN("AmbientOcclusion");
        calculateAmbientOcclusion(voxels);
        TracyPlot("AOTime", getElapsedTimeMs());
    }
}
```

## Производительность воксельного движка

### Критичные метрики для мониторинга

```cpp
void updatePerformanceMetrics() {
    // Основные метрики производительности
    TracyPlot("FPS", framesPerSecond);
    TracyPlot("FrameTimeMs", frameTime);
    TracyPlot("CPUTimeMs", cpuTime);
    TracyPlot("GPUTimeMs", gpuTime);
    
    // Воксельные метрики
    TracyPlot("VisibleVoxels", visibleVoxelCount);
    TracyPlot("RenderedChunks", renderedChunkCount);
    TracyPlot("ChunksInMemory", chunksInMemory);
    
    // Использование памяти
    TracyPlot("VoxelMemoryMB", voxelMemoryUsage / (1024.0f * 1024.0f));
    TracyPlot("TextureMemoryMB", textureMemoryUsage / (1024.0f * 1024.0f));
    TracyPlot("BufferMemoryMB", bufferMemoryUsage / (1024.0f * 1024.0f));
    
    // Анализ bottleneck
    if (gpuTime > cpuTime) {
        TracyMessageL("Bottleneck: GPU bound - оптимизируйте рендеринг");
    } else {
        TracyMessageL("Bottleneck: CPU bound - оптимизируйте логику");
    }
}
```

### Анализ производительности алгоритмов

```cpp
void benchmarkVoxelAlgorithms() {
    TracyMessageL("=== Benchmarking Voxel Algorithms ===");
    
    // Сравнение алгоритмов генерации
    {
        ZoneScopedN("PerlinNoiseGeneration");
        generateWithPerlinNoise();
        TracyPlot("PerlinTime", getElapsedTimeMs());
    }
    
    {
        ZoneScopedN("SimplexNoiseGeneration");
        generateWithSimplexNoise();
        TracyPlot("SimplexTime", getElapsedTimeMs());
    }
    
    {
        ZoneScopedN("WaveletNoiseGeneration");
        generateWithWaveletNoise();
        TracyPlot("WaveletTime", getElapsedTimeMs());
    }
    
    TracyMessageL("=== Benchmark Complete ===");
}
```

### Оптимизация на основе данных Tracy

```cpp
void optimizeBasedOnProfiling() {
    // Анализ самых затратных зон
    if (mostExpensiveZoneTime > 10.0f) { // > 10ms
        TracyMessageL(("Optimization needed for: " + 
                      std::string(mostExpensiveZoneName)).c_str());
        
        if (strstr(mostExpensiveZoneName, "Voxel")) {
            TracyMessageL("Consider: LOD, occlusion culling, chunk streaming");
        } else if (strstr(mostExpensiveZoneName, "Physics")) {
            TracyMessageL("Consider: spatial partitioning, simplified collisions");
        } else if (strstr(mostExpensiveZoneName, "Lighting")) {
            TracyMessageL("Consider: light baking, simplified GI");
        }
    }
    
    // Оптимизация на основе memory usage
    if (voxelMemoryUsage > memoryBudget) {
        TracyMessageL("Memory budget exceeded - reducing chunk cache");
        reduceChunkCacheSize();
    }
}
```

## Примеры кода ProjectV

### Полный пример: профилирование воксельного рендерера

```cpp
// Пример из docs/examples/
void renderVoxelFrame(VkCommandBuffer cmdBuffer, const Scene& scene) {
    FrameMarkNamed("VoxelFrame");
    
    // Профилирование всего кадра
    {
        ZoneScopedN("FullFrame");
        
        // Обновление
        {
            ZoneScopedN("Update");
            updateScene(scene);
        }
        
        // Рендеринг
        {
            TracyVkZone(vkCtx, cmdBuffer, "VulkanRender");
            
            // Проходы рендеринга
            {
                ZoneScopedN("ShadowPass");
                renderShadowMaps(cmdBuffer, scene);
            }
            
            {
                ZoneScopedN("GeometryPass");
                renderGeometry(cmdBuffer, scene);
            }
            
            {
                ZoneScopedN("LightingPass");
                renderLighting(cmdBuffer, scene);
            }
            
            {
                ZoneScopedN("PostProcessing");
                renderPostProcessing(cmdBuffer, scene);
            }
            
            TracyVkCollect(vkCtx, cmdBuffer);
        }
        
        // Представление
        {
            ZoneScopedN("Present");
            presentFrame();
        }
    }
    
    // Обновление метрик
    TracyPlot("FrameTime", getFrameTimeMs());
    TracyPlot("FPS", 1000.0f / getFrameTimeMs());
}
```

### Пример: профилирование загрузки чанков

```cpp
void streamVoxelChunks(const PlayerPosition& playerPos) {
    ZoneScopedN("ChunkStreaming");
    
    // Определение чанков для загрузки/выгрузки
    auto chunksToLoad = determineChunksToLoad(playerPos);
    auto chunksToUnload = determineChunksToUnload(playerPos);
    
    TracyPlot("ChunksToLoad", (int64_t)chunksToLoad.size());
    TracyPlot("ChunksToUnload", (int64_t)chunksToUnload.size());
    
    // Асинхронная загрузка
    {
        ZoneScopedN("AsyncChunkLoading");
        for (const auto& chunkPos : chunksToLoad) {
            ZoneScopedN("LoadSingleChunk");
            loadChunkAsync(chunkPos);
        }
    }
    
    // Выгрузка
    {
        ZoneScopedN("ChunkUnloading");
        for (const auto& chunkPos : chunksToUnload) {
            unloadChunk(chunkPos);
        }
    }
    
    TracyPlot("LoadedChunks", (int64_t)getLoadedChunkCount());
    TracyPlot("ChunkMemoryMB", getChunkMemoryUsage() / (1024.0f * 1024.0f));
}
```

### Пример: интеграция с ImGui для отладки

```cpp
void renderDebugUI() {
    ZoneScopedN("DebugUI");
    
    ImGui::Begin("Tracy Profiling");
    
    // Отображение текущих метрик
    ImGui::Text("FPS: %.1f", framesPerSecond);
    ImGui::Text("Frame Time: %.2f ms", frameTime);
    ImGui::Text("Visible Voxels: %d", visibleVoxelCount);
    ImGui::Text("Chunks Rendered: %d", renderedChunkCount);
    
    // Управление профилированием
    if (ImGui::Button("Start Profiling Session")) {
        tracy::StartCapture();
        TracyMessageL("Profiling session started");
    }
    
    if (ImGui::Button("Stop Profiling Session")) {
        tracy::StopCapture();
        TracyMessageL("Profiling session stopped");
    }
    
    ImGui::End();
}
```

---

## Best Practices для ProjectV

### 1. Уровни детализации профилирования

```cpp
// Уровень 1: Базовый (всегда включён)
#define TRACY_BASIC
FrameMark;
TracyPlot("FPS", framesPerSecond);

// Уровень 2: Детальный (debug builds)
#ifdef DEBUG
#define TRACY_DETAILED
ZoneScopedN("VoxelProcessing");
TracyPlot("VoxelCount", voxelCount);
#endif

// Уровень 3: Экстремальный (по запросу)
if (enableExtremeProfiling) {
    TracyCallstack(16);
    TracyAllocS(ptr, size, 8);
}
```

### 2. Профилирование по категориям

```cpp
enum class ProfilingCategory {
    Rendering,
    Physics,
    Audio,
    ChunkLoading,
    AI,
    Memory
};

void profileCategory(ProfilingCategory category, const std::function<void()>& func) {
    const char* names[] = {"Rendering", "Physics", "Audio", "ChunkLoading", "AI", "Memory"};
    ZoneScopedC(categoryColors[static_cast<int>(category)]);
    ZoneName(names[static_cast<int>(category)], strlen(names[static_cast<int>(category)]));
    
    func();
}
```

### 3. Контекстные сообщения для вокселей

```cpp
void profileVoxelOperation(const std::string& operation, const ChunkPosition& pos) {
    std::string message = operation + " at " + 
                         std::to_string(pos.x) + "," + 
                         std::to_string(pos.y) + "," + 
                         std::to_string(pos.z);
    
    TracyMessage(message.c_str(), message.size());
    ZoneScopedN(operation.c_str());
    
    // Выполнение операции...
}
```

## Интеграция с другими компонентами ProjectV

### Профилирование miniaudio аудиосистемы

```cpp
// Профилирование аудио обработки в Tracy
void updateAudioSystem(AudioSystem& audio) {
    ZoneScopedN("AudioUpdate");
    
    // Обновление позиций источников звука
    {
        ZoneScopedN("AudioPositionUpdate");
        audio.updatePositions();
        TracyPlot("ActiveSounds", (int64_t)audio.getActiveSoundCount());
    }
    
    // Обработка процедурных звуков
    {
        ZoneScopedN("ProceduralAudio");
        audio.updateProceduralSounds();
        TracyPlot("ProceduralSounds", (int64_t)audio.getProceduralSoundCount());
    }
    
    // Обновление spatial audio
    {
        ZoneScopedN("SpatialAudio");
        audio.updateSpatialEffects();
    }
}
```

### Профилирование glm математических операций

```cpp
// Профилирование оптимизированных glm вычислений для вокселей
void profileVoxelMath(const std::vector<glm::vec3>& positions) {
    ZoneScopedN("VoxelMath");
    TracyPlot("VectorCount", (int64_t)positions.size());
    
    // SIMD оптимизированные операции
    {
        ZoneScopedN("SIMDTransformations");
        glm::mat4 transform = getWorldTransform();
        std::vector<glm::vec3> transformed = transformPositionsSIMD(positions, transform);
        TracyPlot("TransformedVectors", (int64_t)transformed.size());
    }
    
    // Расчёты освещения
    {
        ZoneScopedN("LightingCalculations");
        calculateVoxelLighting(positions);
    }
    
    // Расчёты LOD
    {
        ZoneScopedN("LODCalculations");
        calculateVoxelLOD(positions);
    }
}
```

### Профилирование fastgltf загрузки воксельных моделей

```cpp
// Профилирование загрузки glTF моделей для вокселей
void loadVoxelModel(const std::string& modelPath) {
    ZoneScopedN("LoadVoxelModel");
    TracyPlot("ModelSizeMB", getFileSizeMB(modelPath));
    
    // Парсинг glTF
    {
        ZoneScopedN("GLTFParsing");
        fastgltf::Parser parser;
        auto asset = parser.loadGltfBinary(modelPath);
        TracyPlot("GLTFParseTime", getElapsedTimeMs());
    }
    
    // Загрузка текстур
    {
        ZoneScopedN("TextureLoading");
        loadTextures(asset);
        TracyPlot("TextureCount", (int64_t)getTextureCount());
    }
    
    // Загрузка мешей
    {
        ZoneScopedN("MeshLoading");
        loadMeshes(asset);
        TracyPlot("MeshCount", (int64_t)getMeshCount());
        TracyPlot("VertexCount", (int64_t)getTotalVertexCount());
    }
}
```

### Профилирование JoltPhysics для воксельной физики

```cpp
// Профилирование физической симуляции вокселей
void updateVoxelPhysics(PhysicsSystem& physics, float deltaTime) {
    ZoneScopedN("VoxelPhysics");
    TracyPlot("PhysicsDeltaTime", (int64_t)(deltaTime * 1000.0f));
    
    // Обновление симуляции
    {
        ZoneScopedN("PhysicsStep");
        physics.stepSimulation(deltaTime);
        TracyPlot("PhysicsStepTime", getElapsedTimeMs());
    }
    
    // Обработка коллизий
    {
        ZoneScopedN("CollisionDetection");
        physics.detectCollisions();
        TracyPlot("CollisionCount", (int64_t)physics.getCollisionCount());
    }
    
    // Разрешение коллизий
    {
        ZoneScopedN("CollisionResolution");
        physics.resolveCollisions();
        TracyPlot("ResolvedCollisions", (int64_t)physics.getResolvedCollisionCount());
    }
    
    // Интеграция с ECS
    {
        ZoneScopedN("ECSPhysicsSync");
        syncPhysicsWithECS(physics);
    }
}
```

## Специализированные воксельные метрики для ProjectV

### Критичные метрики производительности воксельного движка

```cpp
struct VoxelPerformanceMetrics {
    // Генерация чанков
    int64_t chunksGenerated = 0;
    float chunkGenTimeMs = 0.0f;
    float avgChunkGenTimeMs = 0.0f;
    
    // Мешинг
    int64_t meshesGenerated = 0;
    float meshingTimeMs = 0.0f;
    int64_t trianglesGenerated = 0;
    
    // Освещение
    float lightingTimeMs = 0.0f;
    int64_t litVoxels = 0;
    float lightPropagationTimeMs = 0.0f;
    
    // Память
    size_t voxelMemoryBytes = 0;
    size_t textureMemoryBytes = 0;
    size_t bufferMemoryBytes = 0;
    
    // Загрузка/выгрузка
    int64_t chunksLoaded = 0;
    int64_t chunksUnloaded = 0;
    float chunkStreamingTimeMs = 0.0f;
};

void updateVoxelMetrics(VoxelPerformanceMetrics& metrics) {
    // Отправка метрик в Tracy
    TracyPlot("ChunksGenerated", metrics.chunksGenerated);
    TracyPlot("ChunkGenTimeMs", metrics.chunkGenTimeMs);
    TracyPlot("AvgChunkGenTimeMs", metrics.avgChunkGenTimeMs);
    
    TracyPlot("MeshesGenerated", metrics.meshesGenerated);
    TracyPlot("MeshingTimeMs", metrics.meshingTimeMs);
    TracyPlot("TrianglesGenerated", metrics.trianglesGenerated);
    
    TracyPlot("LightingTimeMs", metrics.lightingTimeMs);
    TracyPlot("LitVoxels", metrics.litVoxels);
    TracyPlot("LightPropagationTimeMs", metrics.lightPropagationTimeMs);
    
    TracyPlot("VoxelMemoryMB", metrics.voxelMemoryBytes / (1024.0f * 1024.0f));
    TracyPlot("TextureMemoryMB", metrics.textureMemoryBytes / (1024.0f * 1024.0f));
    TracyPlot("BufferMemoryMB", metrics.bufferMemoryBytes / (1024.0f * 1024.0f));
    
    TracyPlot("ChunksLoaded", metrics.chunksLoaded);
    TracyPlot("ChunksUnloaded", metrics.chunksUnloaded);
    TracyPlot("ChunkStreamingTimeMs", metrics.chunkStreamingTimeMs);
}
```

### Оптимизация на основе метрик Tracy

```cpp
void analyzeAndOptimizeVoxelEngine() {
    ZoneScopedN("VoxelOptimizationAnalysis");
    
    // Анализ самых дорогих операций
    TracyMessageL("=== Voxel Engine Performance Analysis ===");
    
    // Генерация чанков
    if (chunkGenTimeMs > 5.0f) { // > 5ms
        TracyMessageL("WARNING: Chunk generation time exceeds threshold");
        TracyMessageL("Suggestions: Parallel generation, LOD reduction, caching");
    }
    
    // Мешинг
    if (meshingTimeMs > 10.0f) { // > 10ms
        TracyMessageL("WARNING: Meshing time exceeds threshold");
        TracyMessageL("Suggestions: Greedy meshing, GPU meshing, reduce detail");
    }
    
    // Освещение
    if (lightingTimeMs > 8.0f) { // > 8ms
        TracyMessageL("WARNING: Lighting calculation time exceeds threshold");
        TracyMessageL("Suggestions: Light baking, simplified GI, shadow map caching");
    }
    
    // Память
    if (voxelMemoryBytes > 1024 * 1024 * 512) { // > 512MB
        TracyMessageL("WARNING: Voxel memory usage high");
        TracyMessageL("Suggestions: Chunk streaming, compression, LOD culling");
    }
    
    // Рекомендации по оптимизации
    if (isGPUBound()) {
        TracyMessageL("BOTTLENECK: GPU bound - optimize rendering");
        TracyMessageL("Actions: Reduce draw calls, use instancing, optimize shaders");
    } else if (isCPUBound()) {
        TracyMessageL("BOTTLENECK: CPU bound - optimize logic");
        TracyMessageL("Actions: Parallel systems, reduce entity count, optimize algorithms");
    } else if (isMemoryBound()) {
        TracyMessageL("BOTTLENECK: Memory bound - optimize memory usage");
        TracyMessageL("Actions: Compression, streaming, cache optimization");
    }
}
```

## Примеры профилирования для типичных сценариев ProjectV

### Сценарий 1: Генерация нового мира

```cpp
void generateNewWorld(const WorldParameters& params) {
    FrameMarkNamed("WorldGeneration");
    TracyMessageL("Starting world generation...");
    
    // Генерация террейна
    {
        ZoneScopedN("TerrainGeneration");
        generateTerrain(params);
        TracyPlot("TerrainChunks", (int64_t)getTerrainChunkCount());
    }
    
    // Генерация пещер
    {
        ZoneScopedN("CaveGeneration");
        generateCaves(params);
        TracyPlot("CaveSystems", (int64_t)getCaveSystemCount());
    }
    
    // Размещение структур
    {
        ZoneScopedN("StructurePlacement");
        placeStructures(params);
        TracyPlot("StructuresPlaced", (int64_t)getStructureCount());
    }
    
    // Настройка биомов
    {
        ZoneScopedN("BiomeSetup");
        setupBiomes(params);
        TracyPlot("BiomeCount", (int64_t)getBiomeCount());
    }
    
    TracyMessageL("World generation complete!");
    TracyPlot("TotalWorldGenTime", getTotalTimeMs());
}
```

### Сценарий 2: Деструкция вокселей (разрушение блоков)

```cpp
void destroyVoxels(const std::vector<VoxelPosition>& positions, Player& player) {
    ZoneScopedN("VoxelDestruction");
    TracyPlot("VoxelsToDestroy", (int64_t)positions.size());
    
    // Проверка возможности разрушения
    {
        ZoneScopedN("DestructionValidation");
        validateDestruction(positions, player);
    }
    
    // Обновление данных вокселей
    {
        ZoneScopedN("VoxelDataUpdate");
        updateVoxelData(positions);
        TracyPlot("VoxelsUpdated", (int64_t)positions.size());
    }
    
    // Перегенерация мешей
    {
        ZoneScopedN("MeshRegeneration");
        regenerateMeshes(positions);
        TracyPlot("ChunksRemeshed", (int64_t)getAffectedChunkCount());
    }
    
    // Обновление освещения
    {
        ZoneScopedN("LightingUpdate");
        updateLighting(positions);
    }
    
    // Физика обломков
    {
        ZoneScopedN("DebrisPhysics");
        createDebrisPhysics(positions);
        TracyPlot("DebrisPieces", (int64_t)getDebrisCount());
    }
    
    // Звуковые эффекты
    {
        ZoneScopedN("SoundEffects");
        playDestructionSounds(positions);
    }
}
```

### Сценарий 3: Загрузка/сохранение игрового мира

```cpp
void saveVoxelWorld(const std::string& savePath, const VoxelWorld& world) {
    ZoneScopedN("WorldSave");
    TracyPlot("WorldSizeChunks", (int64_t)world.getChunkCount());
    
    // Сериализация данных чанков
    {
        ZoneScopedN("ChunkSerialization");
        serializeChunks(world);
        TracyPlot("ChunksSerialized", (int64_t)world.getChunkCount());
    }
    
    // Сериализация сущностей
    {
        ZoneScopedN("EntitySerialization");
        serializeEntities(world);
        TracyPlot("EntitiesSerialized", (int64_t)world.getEntityCount());
    }
    
    // Сериализация состояния игрока
    {
        ZoneScopedN("PlayerStateSerialization");
        serializePlayerState(world);
    }
    
    // Запись в файл
    {
        ZoneScopedN("FileWrite");
        writeToFile(savePath);
        TracyPlot("SaveFileSizeMB", getFileSizeMB(savePath));
    }
    
    TracyPlot("TotalSaveTimeMs", getTotalTimeMs());
    TracyMessageL(("World saved to: " + savePath).c_str());
}
```

## Best Practices для воксельного профилирования

### 1. Стратегии профилирования для разных масштабов

```cpp
// Профилирование для маленьких миров (до 100 чанков)
void profileSmallWorld() {
    TracyMessageL("Profiling small world - focus on single-thread performance");
    ZoneScopedN("SmallWorldProfile");
    
    // Приоритет: оптимизация алгоритмов
    TracyPlot("ChunkCount", (int64_t)getChunkCount());
    TracyPlot("EntityCount", (int64_t)getEntityCount());
}

// Профилирование для средних миров (100-1000 чанков)
void profileMediumWorld() {
    TracyMessageL("Profiling medium world - focus on multithreading");
    ZoneScopedN("MediumWorldProfile");
    
    // Приоритет: параллелизм
    TracyPlot("WorkerThreads", (int64_t)getThreadCount());
    TracyPlot("TasksQueued", (int64_t)getTaskQueueSize());
}

// Профилирование для больших миров (1000+ чанков)
void profileLargeWorld() {
    TracyMessageL("Profiling large world - focus on memory and streaming");
    ZoneScopedN("LargeWorldProfile");
    
    // Приоритет: управление памятью и streaming
    TracyPlot("LoadedChunks", (int64_t)getLoadedChunkCount());
    TracyPlot("MemoryUsageMB", getMemoryUsageMB());
    TracyPlot("StreamingSpeedMBps", getStreamingSpeedMBps());
}
```

### 2. Профилирование для разных аппаратных конфигураций

```cpp
void profileForHardware(HardwareProfile hardware) {
    TracyMessageL(("Profiling for hardware: " + hardware.name).c_str());
    
    if (hardware.cpuCores < 4) {
        TracyMessageL("Low core count - optimize single-thread performance");
    }
    
    if (hardware.gpuVRAM < 4096) { // < 4GB
        TracyMessageL("Low VRAM - reduce texture resolution, use compression");
    }
    
    if (hardware.systemRAM < 8192) { // < 8GB
        TracyMessageL("Low system RAM - aggressive streaming, reduce cache size");
    }
}
```

### 3. Автоматическое создание отчётов оптимизации

```cpp
struct OptimizationReport {
    std::string bottleneck;
    std::vector<std::string> suggestions;
    float estimatedImprovementPercent;
};

OptimizationReport generateVoxelOptimizationReport() {
    ZoneScopedN("GenerateOptimizationReport");
    
    OptimizationReport report;
    
    // Анализ данных Tracy
    if (getGpuTime() > getCpuTime() * 1.5f) {
        report.bottleneck = "GPU bound";
        report.suggestions = {
            "Reduce draw calls",
            "Use GPU instancing",
            "Optimize shaders",
            "Implement occlusion culling"
        };
        report.estimatedImprovementPercent = 30.0f;
    }
    
    // Отправка отчёта через Tracy
    TracyMessageL(("Optimization Report: " + report.bottleneck).c_str());
    for (const auto& suggestion : report.suggestions) {
        TracyMessageL(("  - " + suggestion).c_str());
    }
    TracyMessageL(("Estimated improvement: " + std::to_string(report.estimatedImprovementPercent) + "%").c_str());
    
    return report;
}
```

## Дальше

**Следующие шаги:**

1. **Интеграция в ваш проект** — используйте примеры выше для настройки Tracy
2. **Анализ производительности** — найдите bottleneck в вашем воксельном движке
3. **Оптимизация** — примените результаты профилирования для улучшения производительности

**См. также:**

- [Быстрый старт](../tracy/quickstart.md) — основы Tracy
- [Справочник API](../tracy/api-reference.md) — все макросы и функции
- [Решение проблем](../tracy/troubleshooting.md) — устранение ошибок
- [Интеграция с Vulkan](../vulkan/projectv-integration.md) — профилирование графики
- [Интеграция с ECS](../flecs/projectv-integration.md) — профилирование систем
- [Интеграция с VMA](../vma/projectv-integration.md) — профилирование памяти

← [На главную документацию Tracy](../tracy/README.md)