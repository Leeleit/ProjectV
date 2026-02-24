# Vulkan Libraries — Advanced

<!-- anchor: 03_advanced -->

Продвинутые техники Vulkan для ProjectV: bindless rendering, timeline semaphores, async compute и GPU-driven rendering.

---

## Bindless Rendering для воксельного движка

### Почему bindless для ProjectV

| Подход | Дескрипторов | Производительность | Гибкость |
|--------|--------------|-------------------|----------|
| **Legacy** | 16-64 | Низкая | Ограниченная |
| **Bindless** | **1M+** | **Высокая** | **Максимальная** |

### Настройка bindless дескрипторов

```cpp
#include <volk.h>
#include <print>
#include <vector>

class BindlessDescriptorManager {
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

    // Bindless ресурсы
    std::vector<VkImageView> image_views_;
    std::vector<VkSampler> samplers_;
    std::vector<VkBufferView> buffer_views_;

public:
    struct CreateInfo {
        uint32_t max_images = 1024 * 1024;  // 1M текстур
        uint32_t max_samplers = 16;
        uint32_t max_buffers = 65536;
    };

    std::expected<void, std::string> initialize(VkDevice device, const CreateInfo& info) {
        device_ = device;

        // 1. Создание descriptor pool с bindless флагом
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, info.max_images },
            { VK_DESCRIPTOR_TYPE_SAMPLER, info.max_samplers },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, info.max_buffers }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        pool_info.maxSets = 1;
        pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;

        VkResult result = vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_);
        if (result != VK_SUCCESS) {
            return std::unexpected("Failed to create bindless descriptor pool");
        }

        // 2. Создание bindless descriptor set layout
        VkDescriptorSetLayoutBinding bindings[3] = {};

        // Sampled images (bindless)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount = info.max_images;
        bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

        // Samplers
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[1].descriptorCount = info.max_samplers;
        bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

        // Storage buffers (для воксельных данных)
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = info.max_buffers;
        bindings[2].stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorBindingFlags binding_flags[3] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {};
        flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flags_info.bindingCount = static_cast<uint32_t>(std::size(binding_flags));
        flags_info.pBindingFlags = binding_flags;

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.pNext = &flags_info;
        layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layout_info.bindingCount = static_cast<uint32_t>(std::size(bindings));
        layout_info.pBindings = bindings;

        result = vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &descriptor_set_layout_);
        if (result != VK_SUCCESS) {
            return std::unexpected("Failed to create bindless descriptor set layout");
        }

        // 3. Выделение descriptor set
        VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_info = {};
        variable_count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variable_count_info.descriptorSetCount = 1;
        uint32_t max_counts[] = { info.max_images, info.max_samplers, info.max_buffers };
        variable_count_info.pDescriptorCounts = max_counts;

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.pNext = &variable_count_info;
        alloc_info.descriptorPool = descriptor_pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &descriptor_set_layout_;

        result = vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set_);
        if (result != VK_SUCCESS) {
            return std::unexpected("Failed to allocate bindless descriptor set");
        }

        // 4. Инициализация дефолтных ресурсов
        initialize_default_resources();

        std::print("Bindless descriptor manager initialized: {} images, {} buffers\n",
                   info.max_images, info.max_buffers);

        return {};
    }

private:
    void initialize_default_resources() {
        // Создание дефолтного sampler (nearest)
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        VkSampler default_sampler;
        vkCreateSampler(device_, &sampler_info, nullptr, &default_sampler);
        samplers_.push_back(default_sampler);

        // Обновление дескрипторов
        VkWriteDescriptorSet writes[2] = {};

        // Samplers
        VkDescriptorImageInfo sampler_info_desc = {};
        sampler_info_desc.sampler = default_sampler;

        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptor_set_;
        writes[0].dstBinding = 1;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &sampler_info_desc;

        // Пустые изображения (placeholder)
        VkDescriptorImageInfo empty_image_info = {};
        empty_image_info.imageView = VK_NULL_HANDLE;
        empty_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = descriptor_set_;
        writes[1].dstBinding = 0;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &empty_image_info;

        vkUpdateDescriptorSets(device_, 2, writes, 0, nullptr);
    }

public:
    // Добавление текстуры в bindless массив
    uint32_t add_texture(VkImageView image_view) {
        uint32_t index = static_cast<uint32_t>(image_views_.size());
        image_views_.push_back(image_view);

        VkDescriptorImageInfo image_info = {};
        image_info.imageView = image_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptor_set_;
        write.dstBinding = 0;
        write.dstArrayElement = index;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.descriptorCount = 1;
        write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

        return index;
    }

    // Добавление буфера с воксельными данными
    uint32_t add_voxel_buffer(VkBuffer buffer, VkDeviceSize size) {
        uint32_t index = static_cast<uint32_t>(buffer_views_.size());

        VkBufferViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        view_info.buffer = buffer;
        view_info.format = VK_FORMAT_R8_UINT;  // Для воксельных данных
        view_info.offset = 0;
        view_info.range = size;

        VkBufferView buffer_view;
        vkCreateBufferView(device_, &view_info, nullptr, &buffer_view);
        buffer_views_.push_back(buffer_view);

        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = buffer;
        buffer_info.offset = 0;
        buffer_info.range = size;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptor_set_;
        write.dstBinding = 2;
        write.dstArrayElement = index;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

        return index;
    }

    VkDescriptorSet descriptor_set() const { return descriptor_set_; }
    VkDescriptorSetLayout descriptor_set_layout() const { return descriptor_set_layout_; }
};
```

### Шейдер для bindless воксельного рендеринга

```glsl
#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require

// Bindless дескрипторы
layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler texture_samplers[];
layout(set = 0, binding = 2) buffer VoxelBuffers {
    uint voxels[];
} voxel_buffers[];

// Push constants для воксельного чанка
layout(push_constant) uniform PushConstants {
    uint voxel_buffer_index;
    uint texture_index;
    ivec3 chunk_coord;
    uint lod;
} push;

// Структура вокселя
struct Voxel {
    uint material_id;
    uint light_level;
    uint flags;
};

Voxel get_voxel(ivec3 pos) {
    // Линеаризация координат (16x16x16 чанк)
    uint index = pos.x + pos.y * 16 + pos.z * 256;

    // Non-uniform access к bindless буферу
    uint data = voxel_buffers[nonuniformEXT(push.voxel_buffer_index)].voxels[index];

    return Voxel(
        data & 0xFF,           // material_id
        (data >> 8) & 0xFF,    // light_level
        (data >> 16) & 0xFFFF  // flags
    );
}

vec4 sample_voxel_texture(ivec3 pos) {
    Voxel voxel = get_voxel(pos);

    // Non-uniform access к bindless текстуре
    vec4 color = texture(
        sampler2D(textures[nonuniformEXT(voxel.material_id)], texture_samplers[0]),
        vec2(pos.xz) / 16.0
    );

    // Освещение
    color.rgb *= float(voxel.light_level) / 255.0;

    return color;
}
```

---

## Timeline Semaphores для асинхронных вычислений

### Timeline vs Binary Semaphores

| Тип | Значение | Использование |
|-----|----------|---------------|
| **Binary** | 0/1 | Простая синхронизация |
| **Timeline** | **uint64_t** | **Сложные зависимости, async compute** |

### Асинхронная генерация вокселей

```cpp
#include <volk.h>
#include <print>
#include <atomic>

class AsyncVoxelGenerator {
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;

    // Timeline semaphores
    VkSemaphore timeline_semaphore_ = VK_NULL_HANDLE;
    std::atomic<uint64_t> timeline_value_{0};

    // Compute pipeline для генерации вокселей
    VkPipeline compute_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

public:
    struct GenerationTask {
        uint64_t task_id;
        VkBuffer voxel_buffer;
        glm::ivec3 chunk_coord;
        uint32_t lod;
    };

    std::expected<void, std::string> initialize(VkDevice device, VkQueue compute_queue) {
        device_ = device;
        compute_queue_ = compute_queue;

        // Создание timeline semaphore
        VkSemaphoreTypeCreateInfo timeline_info = {};
        timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_info.initialValue = 0;

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = &timeline_info;

        VkResult result = vkCreateSemaphore(device_, &semaphore_info, nullptr, &timeline_semaphore_);
        if (result != VK_SUCCESS) {
            return std::unexpected("Failed to create timeline semaphore");
        }

        // Создание compute pipeline для генерации вокселей
        if (!create_compute_pipeline()) {
            return std::unexpected("Failed to create compute pipeline");
        }

        std::print("Async voxel generator initialized\n");
        return {};
    }

    // Асинхронная генерация чанка
    uint64_t generate_chunk_async(const GenerationTask& task) {
        uint64_t wait_value = timeline_value_.load();
        uint64_t signal_value = wait_value + 1;

        VkCommandBuffer cmd = begin_compute_command_buffer();

        // Dispatch compute shader
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_);

        // Push constants
        struct PushConstants {
            glm::ivec3 chunk_coord;
            uint32_t lod;
            uint64_t buffer_device_address;
        } push;

        push.chunk_coord = task.chunk_coord;
        push.lod = task.lod;

        // Получение device address буфера
        VkBufferDeviceAddressInfo address_info = {};
        address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        address_info.buffer = task.voxel_buffer;
        push.buffer_device_address = vkGetBufferDeviceAddress(device_, &address_info);

        vkCmdPushConstants(cmd, pipeline_layout_,
                          VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(push), &push);

        // Dispatch (16x16x16 вокселей / 8x8x8 work groups)
        vkCmdDispatch(cmd, 2, 2, 2);

        end_compute_command_buffer(cmd);

        // Submit с timeline semaphore
        VkTimelineSemaphoreSubmitInfo timeline_submit_info = {};
        timeline_submit_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timeline_submit_info.waitSemaphoreValueCount = 1;
        timeline_submit_info.pWaitSemaphoreValues = &wait_value;
        timeline_submit_info.signalSemaphoreValueCount = 1;
        timeline_submit_info.pSignalSemaphoreValues = &signal_value;

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = &timeline_submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &timeline_semaphore_;

        vkQueueSubmit(compute_queue_, 1, &submit_info, VK_NULL_HANDLE);

        timeline_value_.store(signal_value);
        return signal_value;
    }

    // Ожидание завершения задачи
    bool wait_for_task(uint64_t task_value, uint64_t timeout_ns = UINT64_MAX) {
        VkSemaphoreWaitInfo wait_info = {};
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &timeline_semaphore_;
        wait_info.pValues = &task_value;

        VkResult result = vkWaitSemaphores(device_, &wait_info, timeout_ns);
        return result == VK_SUCCESS;
    }

    // Получение текущего значения timeline
    uint64_t current_timeline_value() const {
        uint64_t value;
        vkGetSemaphoreCounterValue(device_, timeline_semaphore_, &value);
        return value;
    }

private:
    VkCommandBuffer begin_compute_command_buffer() {
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        // Используем transient command pool для одноразовых команд
        static VkCommandPool transient_pool = create_transient_command_pool();
        alloc_info.commandPool = transient_pool;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device_, &alloc_info, &cmd);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &begin_info);
        return cmd;
    }

    void end_compute_command_buffer(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
    }

    bool create_compute_pipeline() {
        // Создание compute shader module
        std::vector<uint32_t> compute_shader = compile_compute_shader();

        VkShaderModuleCreateInfo shader_info = {};
        shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_info.codeSize = compute_shader.size() * sizeof(uint32_t);
        shader_info.pCode = compute_shader.data();

        VkShaderModule compute_module;
        vkCreateShaderModule(device_, &shader_info, nullptr, &compute_module);

        // Pipeline layout
        VkPushConstantRange push_constant = {};
        push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant.offset = 0;
        push_constant.size = sizeof(struct PushConstants);

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constant;

        vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipeline_layout_);

        // Compute pipeline
        VkComputePipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = compute_module;
        pipeline_info.stage.pName = "main";
        pipeline_info.layout = pipeline_layout_;

        VkResult result = vkCreateComputePipelines(device_, VK_NULL_HANDLE,
                                                  1, &pipeline_info, nullptr, &compute_pipeline_);

        vkDestroyShaderModule(device_, compute_module, nullptr);

        return result == VK_SUCCESS;
    }

    VkCommandPool create_transient_command_pool() {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool_info.queueFamilyIndex = get_compute_queue_family();

        VkCommandPool pool;
        vkCreateCommandPool(device_, &pool_info, nullptr, &pool);
        return pool;
    }

    uint32_t get_compute_queue_family() {
        // Реализация получения queue family
        return 0;
    }

    std::vector<uint32_t> compile_compute_shader() {
        // Компиляция compute shader для генерации вокселей
        return {}; // Заглушка
    }
};
```

### Compute Shader для генерации вокселей

```glsl
#version 460
#extension GL_EXT_buffer_reference : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

// Push constants
layout(push_constant) uniform PushConstants {
    ivec3 chunk_coord;
    uint lod;
    uint64_t buffer_device_address;
} push;

// Структура вокселя
struct Voxel {
    uint material_id;
    uint light_level;
    uint flags;
};

// Buffer device address для записи вокселей
layout(buffer_reference, std430) buffer VoxelBuffer {
    Voxel voxels[];
};

void main() {
    ivec3 voxel_pos = ivec3(gl_GlobalInvocationID.xyz);

    // Генерация вокселя на основе позиции и координат чанка
    ivec3 world_pos = push.chunk_coord * 16 + voxel_pos;

    // Простая процедурная генерация (замените на свою логику)
    float height = sin(world_pos.x * 0.1) * cos(world_pos.z * 0.1) * 10.0;

    Voxel voxel;

    if (world_pos.y < height) {
        voxel.material_id = 1;  // Земля
        voxel.light_level = 15;
        voxel.flags = 0;
    } else if (world_pos.y < height + 1) {
        voxel.material_id = 2;  // Трава
        voxel.light_level = 15;
        voxel.flags = 1;  // Прозрачный
    } else {
        voxel.material_id = 0;  // Воздух
        voxel.light_level = 15;
        voxel.flags = 2;  // Пустой
    }

    // Запись вокселя через buffer device address
    VoxelBuffer voxel_buffer = VoxelBuffer(push.buffer_device_address);

    // Линеаризация координат
    uint index = voxel_pos.x + voxel_pos.y * 16 + voxel_pos.z * 256;
    voxel_buffer.voxels[index] = voxel;
}
```

---

## GPU-Driven Rendering для воксельного движка

### Почему GPU-Driven для ProjectV

| Подход | CPU Overhead | GPU Utilization | Scalability |
|--------|--------------|-----------------|-------------|
| **CPU-Driven** | Высокий | Низкая | Ограниченная |
| **GPU-Driven** | **Низкий** | **Высокая** | **Масштабируемая** |

### Indirect Drawing с воксельными чанками

```cpp
#include <volk.h>
#include <print>
#include <vector>

class GPUDrivenRenderer {
    VkDevice device_ = VK_NULL_HANDLE;

    // Indirect drawing buffers
    VkBuffer indirect_draw_buffer_ = VK_NULL_HANDLE;
    VkBuffer draw_count_buffer_ = VK_NULL_HANDLE;
    VkBuffer cull_data_buffer_ = VK_NULL_HANDLE;

    // Compute pipeline для culling
    VkPipeline cull_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout cull_pipeline_layout_ = VK_NULL_HANDLE;

public:
    struct ChunkInstance {
        glm::mat4 transform;
        glm::ivec3 coord;
        uint32_t lod;
        uint32_t visible;  // 1 = visible, 0 = culled
    };

    std::expected<void, std::string> initialize(VkDevice device) {
        device_ = device;

        // Создание buffers для indirect drawing
        create_indirect_buffers();

        // Создание compute pipeline для frustum culling
        if (!create_cull_pipeline()) {
            return std::unexpected("Failed to create cull pipeline");
        }

        std::print("GPU-driven renderer initialized\n");
        return {};
    }

    // Обновление indirect draw commands на GPU
    void update_indirect_commands(const std::vector<ChunkInstance>& chunks,
                                 const glm::mat4& view_proj) {
        VkCommandBuffer cmd = begin_compute_command_buffer();

        // 1. Копирование данных инстансов в GPU buffer
        copy_chunk_data_to_gpu(chunks);

        // 2. Выполнение frustum culling на GPU
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cull_pipeline_);

        struct CullConstants {
            glm::mat4 view_proj;
            uint32_t chunk_count;
            float lod_distances[4];
        } constants;

        constants.view_proj = view_proj;
        constants.chunk_count = static_cast<uint32_t>(chunks.size());
        constants.lod_distances[0] = 100.0f;  // LOD 0
        constants.lod_distances[1] = 200.0f;  // LOD 1
        constants.lod_distances[2] = 400.0f;  // LOD 2
        constants.lod_distances[3] = 800.0f;  // LOD 3

        vkCmdPushConstants(cmd, cull_pipeline_layout_,
                          VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(constants), &constants);

        // Dispatch compute shader
        uint32_t group_count = (constants.chunk_count + 63) / 64;
        vkCmdDispatch(cmd, group_count, 1, 1);

        // 3. Генерация indirect draw commands
        generate_indirect_commands(cmd);

        end_compute_command_buffer(cmd);

        // Submit
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        vkQueueSubmit(get_compute_queue(), 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(get_compute_queue());
    }

    // Получение indirect draw buffer для рендеринга
    VkBuffer indirect_draw_buffer() const { return indirect_draw_buffer_; }
    VkBuffer draw_count_buffer() const { return draw_count_buffer_; }

private:
    void create_indirect_buffers() {
        // Buffer для indirect draw commands
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = sizeof(VkDrawIndexedIndirectCommand) * 65536;  // Макс 64K draw calls
        buffer_info.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device_, &buffer_info, nullptr, &indirect_draw_buffer_);

        // Buffer для draw count (используется с VK_KHR_draw_indirect_count)
        VkBufferCreateInfo count_info = {};
        count_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        count_info.size = sizeof(uint32_t);
        count_info.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        count_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device_, &count_info, nullptr, &draw_count_buffer_);

        // Buffer для данных culling
        VkBufferCreateInfo cull_info = {};
        cull_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        cull_info.size = sizeof(ChunkInstance) * 65536;
        cull_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        cull_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device_, &cull_info, nullptr, &cull_data_buffer_);

        // Выделение памяти через VMA
        allocate_buffer_memory(indirect_draw_buffer_);
        allocate_buffer_memory(draw_count_buffer_);
        allocate_buffer_memory(cull_data_buffer_);
    }

    bool create_cull_pipeline() {
        // Создание compute shader для frustum culling
        std::vector<uint32_t> cull_shader = compile_cull_shader();

        VkShaderModuleCreateInfo shader_info = {};
        shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_info.codeSize = cull_shader.size() * sizeof(uint32_t);
        shader_info.pCode = cull_shader.data();

        VkShaderModule cull_module;
        vkCreateShaderModule(device_, &shader_info, nullptr, &cull_module);

        // Descriptor set layout для buffers
        VkDescriptorSetLayoutBinding bindings[3] = {};

        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(std::size(bindings));
        layout_info.pBindings = bindings;

        VkDescriptorSetLayout descriptor_set_layout;
        vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &descriptor_set_layout);

        // Pipeline layout
        VkPushConstantRange push_constant = {};
        push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant.offset = 0;
        push_constant.size = sizeof(struct CullConstants);

        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant;

        vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &cull_pipeline_layout_);

        // Compute pipeline
        VkComputePipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = cull_module;
        pipeline_info.stage.pName = "main";
        pipeline_info.layout = cull_pipeline_layout_;

        VkResult result = vkCreateComputePipelines(device_, VK_NULL_HANDLE,
                                                  1, &pipeline_info, nullptr, &cull_pipeline_);

        vkDestroyShaderModule(device_, cull_module, nullptr);

        return result == VK_SUCCESS;
    }

    void copy_chunk_data_to_gpu(const std::vector<ChunkInstance>& chunks) {
        // Реализация копирования данных в GPU buffer
        // Используйте vkCmdCopyBuffer или staging buffer
    }

    void generate_indirect_commands(VkCommandBuffer cmd) {
        // Compute shader для генерации indirect draw commands
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, get_indirect_gen_pipeline());

        // Dispatch
        vkCmdDispatch(cmd, 1, 1, 1);
    }

    VkQueue get_compute_queue() {
        // Получение compute queue
        return VK_NULL_HANDLE; // Заглушка
    }

    void allocate_buffer_memory(VkBuffer buffer) {
        // Выделение памяти через VMA
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VmaAllocation allocation;
        vmaAllocateMemoryForBuffer(get_vma_allocator(), buffer, &alloc_info, &allocation, nullptr);
        vmaBindBufferMemory(get_vma_allocator(), allocation, buffer);
    }

    VmaAllocator get_vma_allocator() {
        // Получение VMA allocator
        return VK_NULL_HANDLE; // Заглушка
    }

    VkPipeline get_indirect_gen_pipeline() {
        // Получение pipeline для генерации indirect commands
        return VK_NULL_HANDLE; // Заглушка
    }

    std::vector<uint32_t> compile_cull_shader() {
        // Компиляция compute shader для frustum culling
        return {}; // Заглушка
    }

    VkCommandBuffer begin_compute_command_buffer() {
        // Начало записи command buffer
        return VK_NULL_HANDLE; // Заглушка
    }

    void end_compute_command_buffer(VkCommandBuffer cmd) {
        // Завершение записи command buffer
    }
};
```

### Compute Shader для Frustum Culling

```glsl
#version 460

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Push constants
layout(push_constant) uniform CullConstants {
    mat4 view_proj;
    uint chunk_count;
    float lod_distances[4];
} constants;

// Входные данные (инстансы чанков)
layout(set = 0, binding = 0) buffer ChunkData {
    mat4 transforms[];
    ivec3 coords[];
    uint lods[];
    uint visibilities[];
} chunk_data;

// Выходные данные (indirect draw commands)
layout(set = 0, binding = 1) buffer IndirectCommands {
    uvec4 commands[];
} indirect_commands;

// Счётчик видимых чанков
layout(set = 0, binding = 2) buffer DrawCount {
    uint count;
} draw_count;

// Frustum planes (из view_proj матрицы)
struct Frustum {
    vec4 planes[6];
};

Frustum extract_frustum(mat4 view_proj) {
    Frustum frustum;

    // Left plane
    frustum.planes[0] = view_proj[3] + view_proj[0];

    // Right plane
    frustum.planes[1] = view_proj[3] - view_proj[0];

    // Bottom plane
    frustum.planes[2] = view_proj[3] + view_proj[1];

    // Top plane
    frustum.planes[3] = view_proj[3] - view_proj[1];

    // Near plane
    frustum.planes[4] = view_proj[3] + view_proj[2];

    // Far plane
    frustum.planes[5] = view_proj[3] - view_proj[2];

    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float length = length(frustum.planes[i].xyz);
        frustum.planes[i] /= length;
    }

    return frustum;
}

bool sphere_in_frustum(vec3 center, float radius, Frustum frustum) {
    for (int i = 0; i < 6; ++i) {
        float distance = dot(frustum.planes[i].xyz, center) + frustum.planes[i].w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

void main() {
    uint chunk_index = gl_GlobalInvocationID.x;
    if (chunk_index >= constants.chunk_count) return;

    // Центр чанка (в мировых координатах)
    vec3 chunk_center = vec3(chunk_data.coords[chunk_index]) * 16.0 + vec3(8.0);

    // Радиус чанка (bounding sphere)
    float radius = 13.856; // sqrt(3) * 8 (диагональ куба 16x16x16)

    // Извлечение frustum
    Frustum frustum = extract_frustum(constants.view_proj);

    // Проверка видимости
    bool visible = sphere_in_frustum(chunk_center, radius, frustum);

    // LOD selection на основе расстояния
    vec3 camera_pos = vec3(0.0); // Замените на реальную позицию камеры
    float distance = length(chunk_center - camera_pos);

    uint lod = 0;
    for (uint i = 0; i < 4; ++i) {
        if (distance < constants.lod_distances[i]) {
            lod = i;
            break;
        }
    }

    // Обновление видимости
    chunk_data.visibilities[chunk_index] = visible ? 1 : 0;
    chunk_data.lods[chunk_index] = lod;

    // Генерация indirect draw command для видимых чанков
    if (visible) {
        uint draw_index = atomicAdd(draw_count.count, 1);

        // VkDrawIndexedIndirectCommand
        indirect_commands.commands[draw_index] = uvec4(
            36,            // indexCount (куб: 12 треугольников * 3 индекса)
            1,             // instanceCount
            0,             // firstIndex
            0,             // vertexOffset
            chunk_index    // firstInstance
        );
    }
}
```

---

## Заключение

Продвинутые техники Vulkan для ProjectV:

### 1. **Bindless Rendering**
- **1M+ текстур** в одном дескрипторном наборе
- **Non-uniform indexing** в шейдерах
- **Минимальный CPU overhead** для смены текстур

### 2. **Timeline Semaphores**
- **Асинхронная генерация** вокселей на GPU
- **Сложные зависимости** между задачами
- **Эффективная синхронизация** compute и graphics очередей

### 3. **GPU-Driven Rendering**
- **Frustum culling** на GPU
- **Indirect drawing** с динамическим количеством draw calls
- **LOD selection** на основе расстояния

### 4. **Buffer Device Address**
- **Прямой доступ** к буферам из шейдеров
- **Гибкая работа** с воксельными данными
- **Минимальные overhead** для random access

### Преимущества для воксельного движка:

1. **Масштабируемость**: От 1K до 1M чанков без увеличения CPU overhead
2. **Производительность**: GPU делает всю тяжёлую работу (culling, LOD, generation)
3. **Гибкость**: Легко добавлять новые типы вокселей и материалы
4. **Современность**: Использование Vulkan 1.3+ features для максимальной производительности

Эти техники превращают ProjectV из простого воксельного рендерера в современный GPU-driven движок, способный рендерить миллионы вокселей с минимальным участием CPU.
