# Vulkan

Документ описывает правила использования Vulkan 1.4 в современном
графическом движке.

---

## Vulkan 1.4 без legacy

Vulkan 1.4 (спецификация 1.4.308, июнь 2026) включает в core:

- **Dynamic Rendering** (VK_KHR_dynamic_rendering) — без `VkRenderPass`.
- **Bindless Descriptors** (VK_EXT_descriptor_indexing) — неограниченные
  массивы ресурсов в одном descriptor set.
- **Mesh Shaders** (VK_EXT_mesh_shader) — замена классического
  vertex+geometry pipeline.
- **Push Descriptors** (VK_KHR_push_descriptor) — дескрипторы через push
  constants.
- **Buffer Device Address (BDA)** (Vulkan 1.2 core) — 64-битные GPU
  указатели на буферы.
- **Synchronization2** (VK_KHR_synchronization2) — упрощённые barriers.
- **Maintenance 5 / 6** — баг-фиксы и оптимизации.

Все ключевые extensions promoted в core. Использовать Vulkan 1.4 core,
не extensions.

---

## Dynamic Rendering

Без `VkRenderPass` и `VkFramebuffer`. Спецификация attachments
передаётся прямо в `vkCmdBeginRendering`.

```cpp
VkRenderingInfo render_info{};
render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
render_info.renderArea = {{0, 0}, {width, height}};
render_info.layerCount = 1;
render_info.colorAttachmentCount = 1;
render_info.pColorAttachments = &color_attachment;

vkCmdBeginRendering(cmd, &render_info);
vkCmdEndRendering(cmd);
```

Преимущества:

- Меньше объектов для управления.
- Проще переключение между targets.
- Нет overhead на render pass compatibility checking.

---

## Bindless Descriptors

Все ресурсы в одном descriptor set. Шейдер индексирует по handle.

```cpp
VkDescriptorSetLayoutBinding binding{};
binding.binding = 0;
binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
binding.descriptorCount = 65536;
binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layout);

VkWriteDescriptorSet write{};
write.descriptorCount = 1;
write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
write.pImageInfo = &image_info;
vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
```

В шейдере:

```glsl
layout(set = 0, binding = 0) uniform sampler2D textures[];

void main() {
    int idx = push_constants.texture_index;
    vec4 color = texture(textures[idx], uv);
}
```

Один draw call может рендерить с разными текстурами, меняя только
push constants.

### Лимиты Vulkan 1.4

- `maxDescriptorSetUniformBuffers`: 90 (было 72)
- `maxDescriptorSetStorageBuffers`: 96 (было 24)
- `maxDescriptorSetStorageImages`: 144 (было 24)
- `maxBoundDescriptorSets`: 7 (было 4)
- `maxPushDescriptors`: 32 (было 16)

---

## Mesh Shaders

Классический vertex+geometry pipeline заменён mesh shader'ами. GPU
генерирует треугольники программно, без vertex buffers.

```cpp
void task_shader() {
    if (frustum_cull(meshlet_bounds[gl_WorkGroupID.x])) {
        EmitMeshTaskEXT(1, 1, 1);
    }
}

void mesh_shader() {
    SetMeshOutputsEXT(64, 1);
}
```

### Best practices (Vulkanised 2023, NVIDIA)

- **Culling в task shader**, не в meshlet-cull внутри mesh shader.
- **Один task shader на meshlet** — мелкая гранулярность для балансировки
  нагрузки.
- **Использовать `VK_EXT_mesh_shader` + cluster culling** для
  Nanite-style virtualized geometry.

### Cluster Culling Shaders (`VK_EXT_cluster_culling_shader`)

Расширение поверх mesh shaders, оптимизированное для Nanite-style
геометрии: cluster-level culling, встроенная поддержка lod selection,
meshlet-driven BVH.

---

## Buffer Device Address (BDA)

GPU читает буферы через 64-битные указатели, а не через descriptor
bindings.

```hlsl
layout(buffer_reference) readonly buffer Storage {
    uint data[];
};

void main() {
    Storage* storage = Storage(addr);
    uint value = storage.data[gl_GlobalInvocationID.x];
}
```

Преимущества:

- Bindless без лимитов на descriptor sets.
- Указатели в compute shaders.
- Совместимо с mesh shaders и cluster culling.

---

## Shader Execution Reordering (SER)

NVIDIA RTX-специфичная (Blackwell+) фича для снижения divergence в
ray-tracing шейдерах.

### Проблема

Ray tracing создаёт divergent шейдеры: разные лучи бьются в разные
материалы, требуют разные вычисления. Warp/wavefront исполняет все
threads в lockstep — divergent paths замедляют warp в 2-32×.

### Решение

SER переупорядочивает threads на лету, группируя их по coherence. После
SER warp исполняет coherent threads — divergence уменьшается.

```hlsl
RayQuery ray = ...;
HitObject hit = RayQuery_GetHitObject(ray);
HitObject_SetHint(hit, HIT_OBJECT_HINT_COHERENT, MATERIAL_METAL);
ReorderThread(CoherenceBox);
```

### Результаты (NVIDIA, 2026)

- Path tracing: 20-30% быстрее, до 3× в path tracing-heavy workload.
- Hardware ray-traced reflections и translucency: до 40% быстрее на RTX
  40.

SER требует ray-tracing hardware (Turing+).

---

## Opacity Micromaps (OMM)

NVIDIA RTX-специфичная (Blackwell+) фича для ускорения alpha-tested
геометрии.

### Проблема

Alpha-tested геометрия (листва, заборы, цепи) даёт inconsistent
intersection results. RT cores вынуждены делать fallback на any-hit
shader для каждого потенциального пересечения.

### Решение

OMM — precomputed bitmap, который говорит hardware: «эта область
полностью непрозрачна», «эта область полностью прозрачна», «эта область
нуждается в any-hit shader». Hardware обрабатывает opaque и transparent
области без any-hit shader.

```cpp
VkMicromapEXT micromap;
vkBuildMicromapsEXT(device, 1, &build_info);

VkAccelerationStructureTrianglesOpacityMicromapEXT omm_info{};
omm_info.micromap = micromap;
vkBuildAccelerationStructuresKHR(...);
```

### Результаты (NVIDIA, 2026)

- 4× ускорение в сценах с плотной листвой.
- Существенное ускорение в сценах с цепями и заборами.

OMM требует Blackwell+ hardware.

---

## Synchronization 2.0

Vulkan 1.3+ unified синхронизация через `VkDependencyInfo`.

```cpp
VkDependencyInfo dep_info{};
dep_info.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

VkBufferMemoryBarrier2 barrier{};
barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
barrier.dstAccessMask = VK_ACCESS_2_VERTEX_INPUT_READ_BIT;
barrier.buffer = compute_buffer;

dep_info.pBufferMemoryBarriers = &barrier;
vkCmdPipelineBarrier2(cmd, &dep_info);
```

---

## Push Descriptors

Дескрипторы через push constants. Без отдельного descriptor set.

До 32 push descriptors на set (Vulkan 1.4).

---

## Память: VMA и ReBAR

### VulkanMemoryAllocator (VMA 2.1.0)

Стандартный sub-allocator для GPU памяти.

```cpp
VmaAllocator allocator;
vmaCreateAllocator(&create_info, &allocator);

VmaAllocation allocation;
VkBuffer buffer;
vmaCreateBuffer(allocator, &buffer_info, &allocation_info, &buffer, &allocation, nullptr);
```

VMA решает sub-allocation, defragmentation, budget tracking.

### ReBAR (Resizable BAR)

PCIe alternative: CPU получает прямой доступ ко всей VRAM. До 64 GB/s
вместо 32 GB/s (PCIe Gen4 x16).

Включено в BIOS на поддерживаемых материнских платах.

---

## Validation Layers

VK_LAYER_KHRONOS_validation включает:

- Проверки API usage.
- GPU-assisted validation.
- Best practices warnings.

В Debug: ON. В Release: OFF.

Validation в hot path замедляет в 5-10×.

---

## Почему Vulkan, а не OpenGL/DirectX11

- **OpenGL:** legacy, скрытые costs, неявные dependencies.
- **DirectX 11:** Windows-only.
- **DirectX 12:** близок к Vulkan, но Windows-only.
- **Metal:** macOS-only.

Vulkan — единственный modern explicit API, работающий на Linux (primary
dev-host).

---

## Пример: применение в движке

В ProjectV рендеринг полностью на Vulkan 1.4: dynamic rendering, bindless
descriptors с VMA-аллоцированными SSBO, mesh shaders для voxel chunks,
ray queries для освещения. SER и OMM — в плане после стабилизации
hardware adoption.

---

## Источники и дальнейшее чтение

- **Vulkan 1.4 Specification** — каноническая спека (1.4.308).
  <https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html>
- **Sascha Willems — How to Vulkan in 2026**.
- **Vulkanised 2026 papers** — Kerem Tuncer (FrameGraph), Preetish Kakkar.
- **NVIDIA — Improve Shader Performance with SER**.
  <https://developer.nvidia.com/blog/improve-shader-performance-and-in-game-frame-rates-with-shader-execution-reordering/>
- **NVIDIA — Best Practices for Using NVIDIA RTX Ray Tracing**.
  <https://developer.nvidia.com/blog/best-practices-for-using-nvidia-rtx-ray-tracing-updated/>
- **AMD — uProf documentation**.
  <https://www.amd.com/en/developer/uprof.html>
- **Intel — VTune Profiler documentation**.
  <https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html>
- [91_tooling-landscape.md](91_tooling-landscape.md) — VMA, volk, Vulkan
  headers.
- [30_optimization.md](30_optimization.md) — иерархия оптимизации.
- [32_voxel-data.md](32_voxel-data.md) — GPU-driven воксели.