# Деревья решений Vulkan [🟡 Уровень 2]

**🟡 Уровень 2: Средний** — Выбор правильных подходов и архитектурных решений для различных задач Vulkan.

## Оглавление

- [Выбор графического API](#выбор-графического-api)
- [Выбор архитектуры рендеринга](#выбор-архитектуры-рендеринга)
- [Выбор синхронизации](#выбор-синхронизации)
- [Выбор управления памятью](#выбор-управления-памятью)
- [Выбор мультитрединга](#выбор-мультитрединга)
- [Выбор расширений](#выбор-расширений)
- [Производительность и отладка](#производительность-и-отладка)

---

## Выбор графического API

```mermaid
flowchart TD
    Start["Начало: выбор графического API"] --> Q1{"Платформа?"}
    Q1 -->|Windows| Q2{"Требуемая производительность?"}
    Q1 -->|Linux| VulkanLinux["Vulkan (рекомендовано)"]
    Q1 -->|macOS| Metal["Metal (Vulkan через MoltenVK)"]
    
    Q2 -->|Максимальная| VulkanWin["Vulkan (лучший контроль)"]
    Q2 -->|Средняя| DirectX12["DirectX 12 (проще Vulkan)"]
    Q2 -->|Минимальная| DirectX11["DirectX 11 (быстрая разработка)"]
    
    VulkanWin --> VulkanCheck{"Есть опыт Vulkan?"}
    DirectX12 --> DXCheck{"Есть опыт DirectX 12?"}
    
    VulkanCheck -->|Да| UseVulkan["Использовать Vulkan"]
    VulkanCheck -->|Нет| ConsiderDX12["Рассмотреть DirectX 12"]
    
    DXCheck -->|Да| UseDX12["Использовать DirectX 12"]
    DXCheck -->|Нет| ConsiderVulkan["Изучить Vulkan или DirectX 11"]
    
    UseVulkan --> FinalVulkan["Vulkan: низкоуровневый контроль,\nлучшая производительность,\nкроссплатформенность"]
    UseDX12 --> FinalDX12["DirectX 12: хороший контроль,\nтолько Windows,\nлучшая интеграция с ОС"]
    
    FinalVulkan --> End["Выбор сделан"]
    FinalDX12 --> End
```

**Для ProjectV:** Vulkan — единственный вариант из-за требований кроссплатформенности, низкоуровневого контроля и
производительности для воксельного рендеринга.

---

## Выбор архитектуры рендеринга

```mermaid
flowchart TD
    Start["Архитектура рендеринга"] --> Q1{"Тип контента?"}
    Q1 -->|Статичный мир| Classic["Классический рендеринг\n(forward/deferred)"]
    Q1 -->|Динамичный воксельный мир| ComputeFirst["Compute-first архитектура"]
    Q1 -->|Процедурная генерация| GPUDriven["GPU-driven рендеринг"]
    
    Classic --> Q2{"Качество освещения?"}
    ComputeFirst --> Q3{"Объём данных?"}
    GPUDriven --> Q4{"Уровень детализации?"}
    
    Q2 -->|Высокое| Deferred["Deferred rendering\n(MRТ, GBuffer)"]
    Q2 -->|Среднее| ForwardPlus["Forward+ rendering\n(tiled/clustered)"]
    Q2 -->|Низкое| SimpleForward["Simple forward rendering"]
    
    Q3 -->|Огромный >1GB| Sparse["Sparse memory + streaming"]
    Q3 -->|Большой 100MB-1GB| Chunked["Chunk-based LOD + culling"]
    Q3 -->|Средний <100MB| SimpleCompute["Basic compute generation"]
    
    Q4 -->|Динамический| MeshShaders["Mesh shaders\n(VK_EXT_mesh_shader)"]
    Q4 -->|Статический| Indirect["Indirect drawing\n(vkCmdDrawIndirect)"]
    
    Deferred --> FinalDeferred["Deferred: сложные материалы,\nмного источников света,\nвысокий overhead"]
    ForwardPlus --> FinalForwardPlus["Forward+: баланс качества\nи производительности"]
    SimpleForward --> FinalSimple["Simple forward: быстрая разработка,\nограниченное освещение"]
    
    Sparse --> FinalSparse["Sparse: эффективная память,\nсложная реализация"]
    Chunked --> FinalChunked["Chunked: хорошая масштабируемость,\nсредняя сложность"]
    SimpleCompute --> FinalCompute["Basic compute: простота,\nограниченный масштаб"]
    
    MeshShaders --> FinalMesh["Mesh shaders: современный подход,\nтребует Vulkan 1.3+"]
    Indirect --> FinalIndirect["Indirect drawing: проверенный подход,\nширокая поддержка"]
```

**Рекомендации для ProjectV:**

1. **Compute-first архитектура** для генерации геометрии
2. **GPU Driven Rendering** для минимизации draw calls
3. **Async Compute** для параллельной обработки
4. **Timeline Semaphores** для синхронизации

---

## Выбор синхронизации

```mermaid
flowchart TD
    Start["Выбор синхронизации"] --> Q1{"Тип приложения?"}
    Q1 -->|Single-threaded| SimpleSync["Простая синхронизация\n(fences + binary semaphores)"]
    Q1 -->|Multi-threaded| AdvancedSync["Продвинутая синхронизация\n(timeline semaphores)"]
    Q1 -->|Real-time compute| ComputeSync["Compute synchronization\n(multiple queues)"]
    
    SimpleSync --> Q2{"Нужен vsync?"}
    AdvancedSync --> Q3{"Количество потоков?"}
    ComputeSync --> Q4{"Отношение compute/graphics?"}
    
    Q2 -->|Да| VSyncOn["FIFO present mode\n(естественный vsync)"]
    Q2 -->|Нет| VSyncOff["MAILBOX/IMMEDIATE\n(минимальная задержка)"]
    
    Q3 -->|2-4 потока| Timeline["Timeline semaphores\n(точный контроль)"]
    Q3 -->|4+ потоков| Sync2["Synchronization2\n(упрощённый API)"]
    
    Q4 -->|Compute > Graphics| ComputeHeavy["Separate compute queue\n+ timeline semaphores"]
    Q4 -->|Graphics > Compute| GraphicsHeavy["Unified queue\n+ pipeline barriers"]
    Q4 -->|Balanced| Balanced["Multiple queues\n+ resource ownership transfer"]
    
    VSyncOn --> FinalVSync["FIFO: плавный рендеринг,\nвозможные микрозадержки"]
    VSyncOff --> FinalNoVSync["MAILBOX: triple buffering,\nминимальная задержка"]
    
    Timeline --> FinalTimeline["Timeline semaphores:\nгибкость, точность,\nтребует Vulkan 1.2+"]
    Sync2 --> FinalSync2["Synchronization2:\nпростота, производительность,\nтребует Vulkan 1.3+"]
    
    ComputeHeavy --> FinalComputeHeavy["Separate compute: максимальная\nпараллельность, сложная синхронизация"]
    GraphicsHeavy --> FinalGraphicsHeavy["Unified queue: простота,\nограниченный параллелизм"]
    Balanced --> FinalBalanced["Multiple queues: баланс,\nсредняя сложность"]
```

**Оптимальный выбор для ProjectV:**

- **Timeline semaphores** для точного контроля над асинхронными операциями
- **Synchronization2** для упрощённого API (если доступно)
- **Separate compute queue** для параллельной обработки вокселей

---

## Выбор управления памятью

```mermaid
flowchart TD
    Start["Управление памятью GPU"] --> Q1{"Объём данных?"}
    Q1 -->|Малый <256MB| SimpleMem["Простое управление\n(vkAllocateMemory)"]
    Q1 -->|Средний 256MB-2GB| VMA["VMA (Vulkan Memory Allocator)"]
    Q1 -->|Большой >2GB| AdvancedMem["Продвинутое управление\n(sparse + streaming)"]
    
    SimpleMem --> Q2{"Частота аллокаций?"}
    VMA --> Q3{"Типы ресурсов?"}
    AdvancedMem --> Q4{"Паттерн доступа?"}
    
    Q2 -->|Часто| Pooling["Memory pools\n(переиспользование)"]
    Q2 -->|Редко| Direct["Direct allocations\n(простота)"]
    
    Q3 -->|Buffers only| BufferOnly["Buffer-focused strategy"]
    Q3 -->|Images only| ImageOnly["Image-focused strategy"]
    Q3 -->|Mixed| Mixed["Balanced strategy"]
    
    Q4 -->|Random access| Sparse["Sparse residency\n(частичное выделение)"]
    Q4 -->|Sequential access| Streaming["Streaming\n(прогрессивная загрузка)"]
    Q4 -->|Mostly read| Cached["Cached\n(кэширование в памяти)"]
    
    Pooling --> FinalPooling["Memory pools: снижение фрагментации,\nсложность управления"]
    Direct --> FinalDirect["Direct: простота,\nвозможная фрагментация"]
    
    BufferOnly --> FinalBuffer["Buffer-focused: оптимизация\nдля вершин/индексов"]
    ImageOnly --> FinalImage["Image-focused: оптимизация\nдля текстур/рентаргетов"]
    Mixed --> FinalMixed["Balanced: универсальность,\nне оптимально для всех случаев"]
    
    Sparse --> FinalSparse["Sparse: огромные миры,\nсложная реализация"]
    Streaming --> FinalStreaming["Streaming: открытые миры,\nпостоянная загрузка"]
    Cached --> FinalCached["Cached: повторное использование,\nвысокие требования к памяти"]
```

**Рекомендации для ProjectV:**

1. **VMA** для основного управления памятью
2. **Sparse residency** для воксельных миров >4GB
3. **Memory pooling** для часто создаваемых ресурсов (чтобы снизить фрагментацию)

---

## Выбор мультитрединга

```mermaid
flowchart TD
    Start["Мультитрединг в Vulkan"] --> Q1{"Цель параллелизма?"}
    Q1 -->|Record commands| CmdParallelism["Параллельная запись команд"]
    Q1 -->|Resource creation| ResParallelism["Параллельное создание ресурсов"]
    Q1 -->|Frame processing| FrameParallelism["Параллельная обработка кадров"]
    
    CmdParallelism --> Q2{"Количество объектов?"}
    ResParallelism --> Q3{"Тип ресурсов?"}
    FrameParallelism --> Q4{"Задержка vs пропускная способность?"}
    
    Q2 -->|Много (>1000)| SecondaryBuffers["Secondary command buffers\n(параллельная запись)"]
    Q2 -->|Немного (<1000)| PrimaryBuffers["Primary command buffers\n(однопоточная запись)"]
    
    Q3 -->|Buffers| AsyncBuffer["Async buffer creation\n(отдельные потоки)"]
    Q3 -->|Images| AsyncImage["Async image creation\n(осторожно с layout transitions)"]
    Q3 -->|Pipelines| AsyncPipeline["Pipeline compilation\n(фоновые потоки)"]
    
    Q4 -->|Low latency| FrameParallel["Frame parallelism\n(каждый кадр в своём потоке)"]
    Q4 -->|High throughput| TaskParallel["Task parallelism\n(разделение работы)"]
    
    SecondaryBuffers --> FinalSecondary["Secondary CBs: высокая параллельность,\noverhead на submission"]
    PrimaryBuffers --> FinalPrimary["Primary CBs: простота,\nограниченный параллелизм"]
    
    AsyncBuffer --> FinalAsyncBuffer["Async buffers: безопасно,\nхорошая масштабируемость"]
    AsyncImage --> FinalAsyncImage["Async images: требует синхронизации,\nумеренная сложность"]
    AsyncPipeline --> FinalAsyncPipeline["Async pipelines: долгая компиляция,\nхороший кандидат для асинхронности"]
    
    FrameParallel --> FinalFrame["Frame parallel: минимальная задержка,\nвысокие требования к памяти"]
    TaskParallel --> FinalTask["Task parallel: максимальная пропускная способность,\nсложная синхронизация"]
```

**Для ProjectV:**

- **Secondary command buffers** для параллельной генерации команд для разных чанков
- **Async pipeline compilation** для компиляции шейдеров в фоне
- **Frame parallelism** для минимизации задержки ввода

---

## Выбор расширений

```mermaid
flowchart TD
    Start["Выбор расширений Vulkan"] --> Q1{"Основные требования?"}
    Q1 -->|Modern features| Vulkan13["Vulkan 1.3 core features"]
    Q1 -->|Platform support| Platform["Platform-specific extensions"]
    Q1 -->|Performance| Perf["Performance extensions"]
    
    Vulkan13 --> Q2{"Нужны современные фичи?"}
    Platform --> Q3{"Целевая платформа?"}
    Perf --> Q4{"Боттленек?"}
    
    Q2 -->|Dynamic rendering| DynRender["VK_KHR_dynamic_rendering"]
    Q2 -->|Mesh shaders| MeshShader["VK_EXT_mesh_shader"]
    Q2 -->|Shader objects| ShaderObj["VK_EXT_shader_object"]
    
    Q3 -->|Windows| WinExt["VK_KHR_win32_surface\n+ VK_KHR_surface"]
    Q3 -->|Linux| LinuxExt["VK_KHR_xlib/xcb_surface\n+ VK_KHR_surface"]
    Q3 -->|Android| AndroidExt["VK_KHR_android_surface\n+ VK_KHR_surface"]
    
    Q4 -->|CPU overhead| CPUPerf["VK_EXT_descriptor_buffer\n+ VK_KHR_maintenance4"]
    Q4 -->|GPU performance| GPUPerf["VK_EXT_graphics_pipeline_library\n+ VK_EXT_pipeline_creation_cache_control"]
    Q4 -->|Memory usage| MemPerf["VK_KHR_buffer_device_address\n+ VK_EXT_memory_budget"]
    
    DynRender --> FinalDynRender["Dynamic rendering: упрощённый API,\nтребует Vulkan 1.3+"]
    MeshShader --> FinalMeshShader["Mesh shaders: современная геометрия,\nтребует поддержки GPU"]
    ShaderObj --> FinalShaderObj["Shader objects: гибкость,\nэкспериментальная фича"]
    
    WinExt --> FinalWin["Windows extensions: обязательны\nдля работы на Windows"]
    LinuxExt --> FinalLinux["Linux extensions: обязательны\nдля работы на Linux"]
    AndroidExt --> FinalAndroid["Android extensions: обязательны\nдля работы на Android"]
    
    CPUPerf --> FinalCPU["Descriptor buffer: снижение CPU overhead,\nсложный API"]
    GPUPerf --> FinalGPU["Pipeline libraries: ускорение компиляции,\nумеренная сложность"]
    MemPerf --> FinalMem["Memory budget: оптимизация памяти,\nтребует мониторинга"]
```

**Обязательные для ProjectV:**

1. **VK_KHR_swapchain** — вывод на экран
2. **VK_KHR_surface** + платформенно-специфичные — создание поверхности
3. **VK_EXT_descriptor_indexing** — bindless rendering для текстур вокселей
4. **VK_KHR_buffer_device_address** — прямой доступ к данным из шейдеров

**Рекомендуемые:**

1. **VK_KHR_timeline_semaphore** — продвинутая синхронизация
2. **VK_EXT_memory_budget** — мониторинг использования памяти
3. **VK_KHR_dynamic_rendering** — упрощённый рендеринг (Vulkan 1.3+)

---

## Производительность и отладка

```mermaid
flowchart TD
    Start["Производительность и отладка"] --> Q1{"Проблема?"}
    Q1 -->|Low FPS| PerfIssue["Проблемы производительности"]
    Q1 -->|Crashes| CrashIssue["Крэши и нестабильность"]
    Q1 -->|Visual artifacts| VisualIssue["Визуальные артефакты"]
    
    PerfIssue --> Q2{"Боттленек?"}
    CrashIssue --> Q3{"Тип крэша?"}
    VisualIssue --> Q4{"Тип артефакта?"}
    
    Q2 -->|CPU| CPUProfiling["Профилирование CPU\n(Tracy, RenderDoc CPU)"]
    Q2 -->|GPU| GPUProfiling["Профилирование GPU\n(RenderDoc GPU, Nsight)"]
    Q2 -->|Memory| MemProfiling["Профилирование памяти\n(VMA, Vulkan memory tracker)"]
    
    Q3 -->|Device lost| DeviceLost["Device lost\n(проверка validation layers)"]
    Q3 -->|Access violation| AccessViolation["Access violation\n(проверка bounds и synchronization)"]
    Q3 -->|Driver crash| DriverCrash["Driver crash\n(упрощение сцены, обновление драйвера)"]
    
    Q4 -->|Corrupted geometry| GeometryIssue["Проблемы с геометрией\n(проверка vertex/index buffers)"]
    Q4 -->|Texture artifacts| TextureIssue["Проблемы с текстурами\n(проверка samplers и mipmaps)"]
    Q4 -->|Lighting issues| LightingIssue["Проблемы с освещением\n(проверка shaders и uniforms)"]
    
    CPUProfiling --> FinalCPUProf["CPU profiling: поиск hot paths,\nоптимизация алгоритмов"]
    GPUProfiling --> FinalGPUProf["GPU profiling: анализ GPU timeline,\nоптимизация конвейера"]
    MemProfiling --> FinalMemProf["Memory profiling: поиск утечек,\nоптимизация аллокаций"]
    
    DeviceLost --> FinalDeviceLost["Validation layers: поиск VUID нарушений,\nисправление ошибок API"]
    AccessViolation --> FinalAccess["Bounds checking: проверка индексов,\nсинхронизация доступа"]
    DriverCrash --> FinalDriver["Driver issues: упрощение шейдеров,\nтестирование на разных драйверах"]
    
    GeometryIssue --> FinalGeometry["Geometry debug: wireframe mode,\nпроверка vertex data"]
    TextureIssue --> FinalTexture["Texture debug: validation слои,\nпроверка форматов и layouts"]
    LightingIssue --> FinalLighting["Lighting debug: debug shaders,\nпроверка uniform buffers"]
```

**Инструменты для ProjectV:**

1. **Tracy** — профилирование CPU/GPU в реальном времени
2. **RenderDoc** — захват и анализ кадров
3. **Vulkan Validation Layers** — проверка корректности API вызовов
4. **VMA debugging** — отслеживание выделений памяти

---

## 🧭 Навигация

### Следующие шаги

🟢 **[Основные понятия Vulkan](concepts.md)** — Фундаментальные концепции Vulkan API  
🟡 **[Быстрый старт Vulkan](quickstart.md)** — Практическое создание треугольника  
🔴 **[Производительность Vulkan](performance.md)** — Оптимизация и продвинутые техники

### Связанные разделы

🔗 **[ProjectV Integration](projectv-integration.md)** — Специфичные для ProjectV подходы  
🔗 **[Решение проблем Vulkan](troubleshooting.md)** — Отладка и типичные ошибки  
🔗 **[Интеграция Vulkan](integration.md)** — Настройка с SDL3, volk, VMA

← **[Назад к основной документации Vulkan](README.md)**