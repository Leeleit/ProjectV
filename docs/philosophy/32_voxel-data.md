# Воксельные данные

Документ описывает хранение и обработку воксельных данных в современном
GPU-driven движке.

---

## GPU-Driven Rendering

Традиционный voxel rendering: CPU генерирует меши из чанков, отправляет
в GPU, рисует. CPU — bottleneck для больших сцен.

GPU-driven: GPU сам генерирует меши из чанков. CPU только задаёт чанки
для рендеринга.

Преимущества:

- CPU не загружен генерацией мешей.
- GPU обрабатывает миллионы вокселей параллельно.
- Чанки могут обновляться без CPU-side mesh rebuild.

---

## Sparse64Tree

Sparse64Tree — flat иерархия вокселей с фиксированным 64-bit ключом.
Альтернатива Sparse Voxel Octree (SVO).

### Мотивация

SVO (Laine & Karras 2010) — иерархическое деление пространства на
octant-ы. Гибкое, но для GPU-рендеринга избыточно:

- Octree traversal даёт pointer chasing.
- Random access к иерархии = cache miss-ы.
- Уровни вложенности усложняют LOD.

Sparse64Tree — flat иерархия с фиксированным 64-bit ключом.

### Структура

```cpp
struct Sparse64Tree {
    // Ключ: x:21, y:21, z:21, lod:1
    static uint64_t make_key(int32_t x, int32_t y, int32_t z, uint32_t lod);

    std::vector<uint64_t> keys;
    std::vector<ChunkData> chunks;

    const ChunkData* find(int32_t x, int32_t y, int32_t z) const;
}
```

Lookup — бинарный поиск по отсортированному массиву. O(log N) без
pointer chasing.

### Преимущества перед SVO

- Flat структура: friendly к кэшу.
- Binary search: предсказуемая latency.
- Простая сериализация.
- LOD встроен в ключ.

---

## Мешинг: Compute vs Mesh Shaders

### Compute-мешинг (CPU-driven fallback)

```cpp
std::vector<Vertex> vertices;
std::vector<uint32_t> indices;
mesh_chunk(chunk_data, vertices, indices);

VkBuffer vertex_buffer = create_buffer(vertices);
vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);
vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT32);
vkCmdDrawIndexed(cmd, indices.size(), 1, 0, 0, 0);
```

Используется для startup, загрузки уровней, синхронного мешинга.

### Mesh shader мешинг (GPU-driven, основной)

```glsl
void task_shader() {
    uint chunk_id = gl_WorkGroupID.x;
    ChunkData chunk = chunks[chunk_id];

    if (frustum_cull(chunk.bounds)) {
        EmitMeshTaskEXT(1, 1, 1);
    }
}

void mesh_shader() {
    ChunkData chunk = chunks[gl_WorkGroupID.x];
    SetMeshOutputsEXT(chunk.vertex_count, chunk.primitive_count);
}
```

GPU сам генерирует меши. CPU только задаёт чанки для обработки.

### Best practice

Culling в task shader, не в mesh shader. Task shader имеет доступ к
chunk bounds до запуска mesh shader — дешёвый frustum cull.

---

## Bindless Resources (Descriptor Indexing)

Все текстуры чанков, материалы, таблицы плотности — в одном descriptor
set.

Mesh shader получает handle через push constants и индексирует в
descriptor set.

---

## Что хранить, а что вычислять

### Хранить

- Тип вокселя (8-bit material id).
- Плотность материала.
- Метаданные.

Не хранить:

- Позиции вершин меша (вычисляются в mesh shader).
- Нормали (вычисляются в mesh shader через finite difference).
- UV (генерируются из позиции + atlas координат).
- Освещение (вычисляется через DDGI или ray queries).

### Размер чанка

Чанк 16×16×16 = 4096 вокселей. Каждый воксель — 1 байт. Один чанк = 4 KB.

16³ — компромисс между cache locality и overhead.

---

## Meshlet + RT pipeline

Современные движки (Nanite в UE 5) используют meshlet-based BVH для
RT. В воксельном контексте:

1. Chunk = группа meshlet'ов.
2. Mesh shader генерирует meshlet-ы из вокселей.
3. Каждый meshlet имеет tight AABB.
4. RT acceleration structure использует meshlet AABB для traversal.
5. Ray-shader интерсектит meshlet через procedural geometry.

Преимущества:

- RT работает на полной геометрии без полного vertex upload.
- Meshlet-level culling: RT не обрабатывает невидимые meshlet-ы.

---

## DDGI (Dynamic Diffuse Global Illumination)

DDGI заменяет классическую VCT (voxel cone tracing). DDGI использует
probe grid — регулярная сетка точек, хранящих irradiance и distance.

- Плотность сетки: 8 m между probe.
- Каждая probe обновляется раз в кадр.
- Update на GPU в compute shader.
- Sample в fragment shader при расчёте GI.

Плюсы DDGI над VCT:

- Не зависит от воксельной структуры.
- Лучше для динамических сцен.
- Меньше артефактов на границах геометрии.

---

## RTX ray queries для освещения

Fragment shader использует `RayQuery` (DXR 1.1+) для:

- Soft shadows.
- Ambient occlusion.
- Reflections (на металлических/зеркальных поверхностях).
- Refraction (стекло, вода).

```hlsl
RayQuery<RayQueryFlag::Opaque> query;
query.TraceRayInline(tlas, flags, 0xff, origin, tmin, direction, tmax);
query.Proceed();
if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
    float t = query.CommittedRayT();
}
```

Inline ray queries — без dispatch, прямо в fragment shader.

---

## Позиция в ландшафте voxel rendering подходов

| Подход | Плюсы | Минусы | Рекомендация |
|:-------|:------|:-------|:-------------|
| SVO ray-marching | Гибкая структура | Pointer chasing | Не использовать для GPU-driven |
| Sparse voxel DDA | Простая интеграция | CPU-heavy | Не использовать |
| Sparse64Tree + Mesh Shaders | Cache-friendly, GPU-driven | Сложнее в реализации | Рекомендуется |
| BVH + standard meshes | Стандартная инфраструктура | Не для procedural вокселей | Для импортированных мешей |
| Marching cubes on CPU | Простая генерация | CPU bottleneck | Для статических мешей |
| Marching cubes on GPU | GPU-driven | Медленно для больших | Альтернатива |

Современный выбор: Sparse64Tree + Mesh Shaders + RTX queries.

---

## Пример: применение в движке

В ProjectV используется Sparse64Tree для хранения воксельных чанков,
mesh shaders для генерации мешей на GPU, RTX ray queries для освещения.
Sparse64Tree даёт flat структуру с бинарным поиском; mesh shaders дают
GPU-driven мешинг без CPU bottleneck.

---

## Источники и дальнейшее чтение

- **Laine & Karras — Efficient Sparse Voxel Octrees** (NVIDIA Research,
  2010) — канонический paper по SVO.
  <https://research.nvidia.com/publication/efficient-sparse-voxel-octrees>
- **Meshlet ray tracing pipelines** — DiVA Portal, 2026.
- [31_vulkan.md](31_vulkan.md) — Vulkan 1.4 + mesh shaders + SER + OMM.
- [22_ecs.md](22_ecs.md) — ECS и компоненты вокселей.
- [21_dod.md](21_dod.md) — SoA для chunk data.