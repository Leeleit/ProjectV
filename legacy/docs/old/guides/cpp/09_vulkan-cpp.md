# Vulkan C++ идиомы

**🔴 Уровень 3: Продвинутый** — Современные C++ паттерны для безопасной и эффективной работы с Vulkan.

Vulkan — низкоуровневый графический API, требующий ручного управления ресурсами. Этот раздел показывает, как применять
Modern C++ идиомы (RAII, умные указатели, move semantics) для создания безопасного, выразительного и производительного
Vulkan кода в ProjectV.

> **Связь с философией:** Vulkan — это квинтэссенция zero-cost abstractions.
> См. [06_vulkan-philosophy.md](../../philosophy/06_vulkan-philosophy.md). RAII для GPU ресурсов — это не просто
> удобство,
> это способ гарантировать корректность в мире, где утечка GPU памяти может крашнуть драйвер.

## 1. RAII для Vulkan объектов

Каждый Vulkan объект (`VkBuffer`, `VkImage`, `VkPipeline`) должен быть обернут в RAII класс.

### Базовый шаблон RAII обертки

```cpp
template<typename HandleType, auto Deleter>
class VulkanObject {
public:
    VulkanObject() = default;

    explicit VulkanObject(VkDevice device) : m_device(device) {}

    ~VulkanObject() {
        if (m_handle != VK_NULL_HANDLE) {
            Deleter(m_device, m_handle, nullptr);
        }
    }

    // Запрещаем копирование
    VulkanObject(const VulkanObject&) = delete;
    VulkanObject& operator=(const VulkanObject&) = delete;

    // Разрешаем перемещение
    VulkanObject(VulkanObject&& other) noexcept
        : m_device(other.m_device), m_handle(other.m_handle) {
        other.m_handle = VK_NULL_HANDLE;
    }

    VulkanObject& operator=(VulkanObject&& other) noexcept {
        if (this != &other) {
            reset();
            m_device = other.m_device;
            m_handle = other.m_handle;
            other.m_handle = VK_NULL_HANDLE;
        }
        return *this;
    }

    void reset() {
        if (m_handle != VK_NULL_HANDLE) {
            Deleter(m_device, m_handle, nullptr);
            m_handle = VK_NULL_HANDLE;
        }
    }

    HandleType get() const { return m_handle; }
    operator HandleType() const { return m_handle; }
    explicit operator bool() const { return m_handle != VK_NULL_HANDLE; }

private:
    VkDevice m_device{VK_NULL_HANDLE};
    HandleType m_handle{VK_NULL_HANDLE};
};
```

### Специализированные обертки

```cpp
// Делетер для VkBuffer
void vkBufferDeleter(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks*) {
    vkDestroyBuffer(device, buffer, nullptr);
}

using VulkanBuffer = VulkanObject<VkBuffer, vkBufferDeleter>;

// Делетер для VkImage
void vkImageDeleter(VkDevice device, VkImage image, const VkAllocationCallbacks*) {
    vkDestroyImage(device, image, nullptr);
}

using VulkanImage = VulkanObject<VkImage, vkImageDeleter>;

// Использование
VulkanBuffer createVertexBuffer(VkDevice device, const std::vector<Vertex>& vertices) {
    VulkanBuffer buffer(device);

    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = sizeof(Vertex) * vertices.size();
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VkBuffer rawBuffer;
    vkCreateBuffer(device, &info, nullptr, &rawBuffer);

    // Перемещаем владение в RAII обертку
    buffer = VulkanBuffer(device, rawBuffer);
    return buffer;
}
```

## 2. Умные указатели для Vulkan ресурсов

### `std::unique_ptr` с кастомным делетером

```cpp
struct BufferDeleter {
    VkDevice device;

    void operator()(VkBuffer buffer) const {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
        }
    }
};

using UniqueBuffer = std::unique_ptr<VkBuffer_T, BufferDeleter>;

UniqueBuffer createUniqueBuffer(VkDevice device, VkBufferCreateInfo info) {
    VkBuffer buffer;
    vkCreateBuffer(device, &info, nullptr, &buffer);
    return UniqueBuffer(buffer, BufferDeleter{device});
}

// Использование
auto buffer = createUniqueBuffer(device, bufferInfo);
// При выходе из области видимости буфер автоматически удалится
```

### `std::shared_ptr` для разделяемых ресурсов

```cpp
class Texture {
public:
    static std::shared_ptr<Texture> create(VkDevice device, const std::string& path) {
        return std::make_shared<Texture>(device, path);
    }

    Texture(VkDevice device, const std::string& path) : m_device(device) {
        // Загрузка текстуры, создание VkImage, VkImageView, VkSampler
        loadFromFile(path);
    }

    ~Texture() {
        // Автоматическое освобождение всех Vulkan ресурсов
        cleanup();
    }

    VkImageView getView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }

private:
    VkDevice m_device;
    VulkanImage m_image;
    VkImageView m_imageView{VK_NULL_HANDLE};
    VkSampler m_sampler{VK_NULL_HANDLE};

    void loadFromFile(const std::string& path) { /* ... */ }
    void cleanup() { /* ... */ }
};

// Использование
auto grassTexture = Texture::create(device, "textures/grass.png");
auto dirtTexture = Texture::create(device, "textures/dirt.png");

// Несколько мешей могут использовать одну текстуру
mesh1.setTexture(grassTexture);
mesh2.setTexture(grassTexture); // Разделяемый ресурс
```

## 3. Move semantics для передачи владения

Vulkan объекты часто передаются между системами. Move semantics позволяют делать это без копирования.

```cpp
class VulkanPipeline {
public:
    VulkanPipeline() = default;

    VulkanPipeline(VkDevice device, const PipelineConfig& config)
        : m_device(device) {
        createPipeline(config);
    }

    // Move constructor
    VulkanPipeline(VulkanPipeline&& other) noexcept
        : m_device(other.m_device),
          m_pipeline(other.m_pipeline),
          m_layout(other.m_layout) {
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_layout = VK_NULL_HANDLE;
    }

    // Move assignment
    VulkanPipeline& operator=(VulkanPipeline&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_device = other.m_device;
            m_pipeline = other.m_pipeline;
            m_layout = other.m_layout;
            other.m_pipeline = VK_NULL_HANDLE;
            other.m_layout = VK_NULL_HANDLE;
        }
        return *this;
    }

    // Фабричный метод, возвращающий по значению (move)
    static VulkanPipeline createGraphicsPipeline(VkDevice device,
                                                const ShaderModules& shaders) {
        VulkanPipeline pipeline;
        pipeline.m_device = device;
        pipeline.createGraphicsPipelineInternal(shaders);
        return pipeline; // NRVO или move
    }

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_layout{VK_NULL_HANDLE};

    void createPipeline(const PipelineConfig& config) { /* ... */ }
    void createGraphicsPipelineInternal(const ShaderModules& shaders) { /* ... */ }
    void cleanup() { /* ... */ }
};

// Использование
auto pipeline = VulkanPipeline::createGraphicsPipeline(device, shaders);
// Передача владения в рендерер
renderer.setPipeline(std::move(pipeline));
```

## 4. Шаблоны для type-safe Vulkan кода

### Type-safe дескрипторные наборы

```cpp
template<VkDescriptorType Type>
class DescriptorSet {
public:
    DescriptorSet(VkDevice device, VkDescriptorSetLayout layout)
        : m_device(device) {
        allocateDescriptorSet(layout);
    }

    template<typename T>
    void writeBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset = 0) {
        static_assert(Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                     Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     "This descriptor type doesn't support buffers");

        VkDescriptorBufferInfo info{};
        info.buffer = buffer;
        info.offset = offset;
        info.range = sizeof(T);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = Type;
        write.pBufferInfo = &info;

        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }

    template<typename T>
    void writeImage(uint32_t binding, VkImageView view, VkSampler sampler) {
        static_assert(Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     "This descriptor type doesn't support images");

        VkDescriptorImageInfo info{};
        info.imageView = view;
        info.sampler = sampler;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = Type;
        write.pImageInfo = &info;

        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }

private:
    VkDevice m_device;
    VkDescriptorSet m_set{VK_NULL_HANDLE};

    void allocateDescriptorSet(VkDescriptorSetLayout layout) { /* ... */ }
};

// Использование с проверкой типов на этапе компиляции
using UniformSet = DescriptorSet<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>;
using TextureSet = DescriptorSet<VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER>;

UniformSet uniforms(device, uniformLayout);
uniforms.writeBuffer<CameraData>(0, cameraBuffer); // OK
// uniforms.writeImage(0, textureView, sampler); // Ошибка компиляции!

TextureSet textures(device, textureLayout);
textures.writeImage(0, textureView, sampler); // OK
// textures.writeBuffer<CameraData>(0, cameraBuffer); // Ошибка компиляции!
```

## 5. Функциональный стиль для построения Vulkan структур

### Builder pattern с fluent interface

```cpp
class PipelineBuilder {
public:
    PipelineBuilder& addShaderStage(VkShaderStageFlagBits stage,
                                   VkShaderModule module) {
        VkPipelineShaderStageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage = stage;
        info.module = module;
        info.pName = "main";
        m_shaderStages.push_back(info);
        return *this;
    }

    PipelineBuilder& setVertexInput(const VertexInputDescription& desc) {
        m_vertexInput = desc.getCreateInfo();
        return *this;
    }

    PipelineBuilder& setInputAssembly(VkPrimitiveTopology topology) {
        m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_inputAssembly.topology = topology;
        m_inputAssembly.primitiveRestartEnable = VK_FALSE;
        return *this;
    }

    PipelineBuilder& setRasterization(VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
                                     VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT) {
        m_rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_rasterization.polygonMode = polygonMode;
        m_rasterization.cullMode = cullMode;
        m_rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
        m_rasterization.lineWidth = 1.0f;
        return *this;
    }

    VulkanPipeline build(VkDevice device, VkRenderPass renderPass) {
        VkGraphicsPipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.stageCount = static_cast<uint32_t>(m_shaderStages.size());
        info.pStages = m_shaderStages.data();
        info.pVertexInputState = &m_vertexInput;
        info.pInputAssemblyState = &m_inputAssembly;
        info.pRasterizationState = &m_rasterization;
        // ... остальные состояния

        VkPipeline pipeline;
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

        return VulkanPipeline(device, pipeline);
    }

private:
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;
    VkPipelineVertexInputStateCreateInfo m_vertexInput{};
    VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};
    VkPipelineRasterizationStateCreateInfo m_rasterization{};
    // ... другие состояния
};

// Использование (fluent interface)
auto pipeline = PipelineBuilder()
    .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
    .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
    .setVertexInput(vertexDescription)
    .setInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    .setRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT)
    .build(device, renderPass);
```

## 6. Обработка ошибок с `std::expected`

Vulkan функции возвращают `VkResult`. `std::expected` идеально подходит для обработки таких результатов.

```cpp
std::expected<VkBuffer, VkResult> createBuffer(VkDevice device,
                                              VkBufferCreateInfo info,
                                              VkMemoryPropertyFlags properties) {
    VkBuffer buffer;
    VkResult result = vkCreateBuffer(device, &info, nullptr, &buffer);

    if (result != VK_SUCCESS) {
        return std::unexpected(result);
    }

    // Выделение памяти
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    VkDeviceMemory memory;
    result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);

    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        return std::unexpected(result);
    }

    result = vkBindBufferMemory(device, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        return std::unexpected(result);
    }

    return buffer;
}

// Использование с pattern matching (C++23)
auto buffer = createBuffer(device, bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

if (buffer) {
    // Успех
    useBuffer(*buffer);
} else {
    // Ошибка
    logError("Failed to create buffer: {}", vkResultToString(buffer.error()));

    // Обработка конкретных ошибок
    switch (buffer.error()) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            // Попробовать освободить память
            cleanupUnusedResources();
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            // Использовать менее требовательные настройки
            useReducedQuality();
            break;
        default:
            // Критическая ошибка
            throw VulkanError(buffer.error());
    }
}
```

## 7. Интеграция с ECS (flecs)

### Компоненты для Vulkan ресурсов

```cpp
// Компонент для Vulkan буфера
struct GPUBuffer {
    VulkanBuffer buffer;
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize size{0};
    VkBufferUsageFlags usage{0};
};

// Компонент для текстуры
struct GPUTexture {
    VulkanImage image;
    VkImageView view{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
    VkExtent3D extent{};
};

// Компонент для пайплайна
struct GraphicsPipeline {
    VulkanPipeline pipeline;
    VkPipelineLayout layout{VK_NULL_HANDLE};
};

// Система рендеринга
class RenderSystem {
public:
    void render(flecs::world& world, VkCommandBuffer cmd) {
        world.each([cmd](flecs::entity e, const GPUBuffer& buffer, const GPUTexture& texture) {
            // Привязка буфера и текстуры
            vkCmdBindVertexBuffers(cmd, 0, 1, &buffer.buffer.get(), &offsets);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        });
    }
};
```

---

## Для ProjectV

### RAII для GPU ресурсов — Зачем это нужно

> **Метафора:** GPU ресурсы — это как арендованная квартира. Вы въехали (создали VkBuffer), но если вы не выселитесь
> вовремя (vkDestroyBuffer), арендодатель (драйвер) не сможет сдать её другому. В отличие от CPU памяти, GPU память
> ограничена. Утечка 100 буферов по 10MB — это 1GB видеопамяти, которая больше недоступна ничему, даже ОС. RAII
> гарантирует "выселение" при выходе из области видимости.

### VMA (Vulkan Memory Allocator) с RAII

```cpp
class VMABuffer {
    VmaAllocator allocator_;
    VkBuffer buffer_{VK_NULL_HANDLE};
    VmaAllocation allocation_{nullptr};

public:
    VMABuffer(VmaAllocator allocator, const VkBufferCreateInfo& bufferInfo,
              const VmaAllocationCreateInfo& allocInfo) : allocator_(allocator) {
        vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                       &buffer_, &allocation_, nullptr);
    }

    ~VMABuffer() {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
        }
    }

    // Move-only
    VMABuffer(VMABuffer&& other) noexcept;
    VMABuffer& operator=(VMABuffer&& other) noexcept;
    VMABuffer(const VMABuffer&) = delete;
    VMABuffer& operator=(const VMABuffer&) = delete;

    void* map() {
        void* data;
        vmaMapMemory(allocator_, allocation_, &data);
        return data;
    }

    void unmap() { vmaUnmapMemory(allocator_, allocation_); }
};
```

**Связь с библиотеками:** Подробнее о VMA см. [docs/libraries/vma/](../../libraries/vma/).
