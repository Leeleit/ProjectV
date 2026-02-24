# Продвинутые оптимизации для высокопроизводительных систем

> **Версия библиотеки:** Draco 1.5.7 (коммит из `external/draco`)

## Философия производительности в контексте воксельных движков

Draco — это библиотека сжатия геометрии, использующая энтропийное кодирование (rANS) и предсказательные схемы. В
контексте высокопроизводительного воксельного движка мы должны понимать фундаментальные ограничения и возможности этой
технологии.

> **Для понимания:** Алгоритмы вроде rANS в Draco — это как чтение зашифрованного свитка с начала до конца. Ты не можешь
> начать читать с середины, не прочитав начало. GPU ненавидит такие задачи: его 10,000 ядер хотят читать книгу с любой
> страницы одновременно. Поэтому Draco — это работа для «мастеров» (ядер CPU), а не для «фабрики» (GPU). Мы
> распаковываем
> данные на CPU и отдаём GPU уже готовую таблицу.

## Многопоточное декодирование через Job System

Поскольку Draco не поддерживает параллельное декодирование внутри одного битстрима, мы используем пакетную обработку
чанков на нескольких потоках.

### Архитектура Job System для Draco

```cpp
#include <draco/compression/decode.h>
#include <print>
#include <expected>
#include <span>
#include <vector>
#include <atomic>
#include <latch>
#include <thread>

struct VoxelChunkData {
    std::vector<float> positions;      // SoA: отдельный массив позиций
    std::vector<uint8_t> material_ids; // SoA: отдельный массив идентификаторов материалов
    std::vector<uint16_t> light_levels; // SoA: отдельный массив уровней освещения
    std::vector<uint32_t> indices;     // Индексы треугольников (если есть)
};

struct DecodeResult {
    std::expected<VoxelChunkData, draco::Status> data;
    size_t chunk_id;
};

class DracoJobSystem {
public:
    explicit DracoJobSystem(size_t num_workers = std::thread::hardware_concurrency())
        : workers_(num_workers) {
        // Инициализация пула потоков
        for (size_t i = 0; i < num_workers; ++i) {
            workers_[i] = std::jthread([this](std::stop_token stoken) {
                worker_thread(stoken);
            });
        }
    }

    ~DracoJobSystem() {
        // Остановка всех воркеров
        for (auto& worker : workers_) {
            worker.request_stop();
        }
        cv_.notify_all();
    }

    auto decode_chunk_async(std::span<const std::byte> compressed_data, size_t chunk_id)
        -> std::future<DecodeResult> {
        auto promise = std::make_shared<std::promise<DecodeResult>>();
        auto future = promise->get_future();

        {
            std::lock_guard lock(queue_mutex_);
            tasks_.push_back({
                .compressed_data = std::vector<std::byte>(compressed_data.begin(), compressed_data.end()),
                .chunk_id = chunk_id,
                .promise = std::move(promise)
            });
        }

        cv_.notify_one();
        return future;
    }

private:
    struct DecodeTask {
        std::vector<std::byte> compressed_data;
        size_t chunk_id;
        std::shared_ptr<std::promise<DecodeResult>> promise;
    };

    void worker_thread(std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            std::optional<DecodeTask> task;

            {
                std::unique_lock lock(queue_mutex_);
                cv_.wait(lock, stoken, [this] { return !tasks_.empty(); });

                if (stoken.stop_requested() || tasks_.empty()) {
                    continue;
                }

                task = std::move(tasks_.back());
                tasks_.pop_back();
            }

            if (task) {
                auto result = decode_chunk_sync(task->compressed_data);
                task->promise->set_value(DecodeResult{
                    .data = std::move(result),
                    .chunk_id = task->chunk_id
                });
            }
        }
    }

    auto decode_chunk_sync(std::span<const std::byte> compressed_data)
        -> std::expected<VoxelChunkData, draco::Status> {
        draco::DecoderBuffer buffer;
        buffer.Init(reinterpret_cast<const char*>(compressed_data.data()),
                    compressed_data.size());

        draco::Decoder decoder;
        auto geometry_type = draco::Decoder::GetEncodedGeometryType(&buffer);

        if (!geometry_type.ok()) {
            return std::unexpected(geometry_type.status());
        }

        if (geometry_type.value() == draco::POINT_CLOUD) {
            return decode_point_cloud(decoder, buffer);
        } else if (geometry_type.value() == draco::TRIANGULAR_MESH) {
            return decode_mesh(decoder, buffer);
        }

        return std::unexpected(draco::Status(draco::Status::DRACO_ERROR,
                                             "Unsupported geometry type"));
    }

    auto decode_point_cloud(draco::Decoder& decoder, draco::DecoderBuffer& buffer)
        -> std::expected<VoxelChunkData, draco::Status> {
        std::unique_ptr<draco::PointCloud> pc = decoder.DecodePointCloudFromBuffer(&buffer);
        if (!pc) {
            return std::unexpected(draco::Status(draco::Status::DRACO_ERROR,
                                                 "Failed to decode point cloud"));
        }

        VoxelChunkData result;

        // Извлечение позиций (обязательный атрибут)
        const auto* pos_attr = pc->GetNamedAttribute(draco::GeometryAttribute::POSITION);
        if (!pos_attr) {
            return std::unexpected(draco::Status(draco::Status::DRACO_ERROR,
                                                 "No position attribute found"));
        }

        result.positions.resize(pc->num_points() * 3);
        for (draco::PointIndex i(0); i < pc->num_points(); ++i) {
            pos_attr->GetMappedValue(i, &result.positions[i.value() * 3]);
        }

        // Извлечение пользовательских атрибутов (material_id, light_level и т.д.)
        for (int attr_id = 0; attr_id < pc->num_attributes(); ++attr_id) {
            const auto* attr = pc->attribute(attr_id);
            if (attr->attribute_type() == draco::GeometryAttribute::GENERIC) {
                // Проверяем метаданные для определения семантики
                const auto* metadata = pc->GetAttributeMetadataByAttributeId(attr_id);
                if (metadata) {
                    std::string semantic;
                    if (metadata->GetEntryString("semantic", &semantic)) {
                        if (semantic == "material_id") {
                            extract_attribute<uint8_t>(*attr, result.material_ids);
                        } else if (semantic == "light_level") {
                            extract_attribute<uint16_t>(*attr, result.light_levels);
                        }
                    }
                }
            }
        }

        return result;
    }

    template<typename T>
    void extract_attribute(const draco::PointAttribute& attr, std::vector<T>& output) {
        output.resize(attr.size());
        for (draco::AttributeValueIndex i(0); i < attr.size(); ++i) {
            T value;
            attr.GetValue(i, &value);
            output[i.value()] = value;
        }
    }

    std::vector<std::jthread> workers_;
    std::vector<DecodeTask> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable_any cv_;
};
```

> **Для понимания:** Job System для Draco — это как конвейер на складе. Каждый рабочий (поток) берёт один контейнер (
> чанк), распаковывает его, кладёт результат на ленту (буфер GPU) и берёт следующий. Контейнеры независимы, поэтому
> рабочие не мешают друг другу.

## Zero-Copy Upload в GPU через VMA

Ключевая оптимизация: избегаем лишних копий данных из CPU в GPU. Распаковываем напрямую в замапленную память VMA.

### Интеграция с Vulkan Memory Allocator

```cpp
#include <vk_mem_alloc.h>
#include <span>
#include <mdspan>

struct GpuVoxelBuffers {
    VkBuffer position_buffer;
    VkBuffer material_buffer;
    VkBuffer light_buffer;
    VmaAllocation position_allocation;
    VmaAllocation material_allocation;
    VmaAllocation light_allocation;
    void* position_mapped;
    void* material_mapped;
    void* light_mapped;
};

class DracoGpuUploader {
public:
    DracoGpuUploader(VmaAllocator allocator, VkDeviceSize chunk_size)
        : allocator_(allocator), chunk_size_(chunk_size) {}

    auto create_gpu_buffers() -> std::expected<GpuVoxelBuffers, VkResult> {
        GpuVoxelBuffers buffers{};

        // Создание буфера для позиций (3 float на воксель)
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = chunk_size_ * sizeof(float) * 3,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VmaAllocationCreateInfo alloc_info = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        if (vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                            &buffers.position_buffer, &buffers.position_allocation,
                            nullptr) != VK_SUCCESS) {
            return std::unexpected(VK_ERROR_OUT_OF_HOST_MEMORY);
        }

        // Маппинг памяти для прямого доступа
        vmaMapMemory(allocator_, buffers.position_allocation, &buffers.position_mapped);

        // Аналогично создаём буферы для material_id и light_level
        // ...

        return buffers;
    }

    auto upload_chunk_direct(const VoxelChunkData& chunk_data, GpuVoxelBuffers& buffers)
        -> std::expected<void, VkResult> {
        // Прямая запись в замапленную память VMA
        auto positions_span = std::span<float, std::dynamic_extent>(
            static_cast<float*>(buffers.position_mapped),
            chunk_data.positions.size()
        );
        std::ranges::copy(chunk_data.positions, positions_span.begin());

        // Для material_ids и light_levels аналогично
        auto materials_span = std::span<uint8_t, std::dynamic_extent>(
            static_cast<uint8_t*>(buffers.material_mapped),
            chunk_data.material_ids.size()
        );
        std::ranges::copy(chunk_data.material_ids, materials_span.begin());

        // Флаш не требуется, так как память HOST_COHERENT
        return {};
    }

    void destroy_buffers(GpuVoxelBuffers& buffers) {
        if (buffers.position_mapped) {
            vmaUnmapMemory(allocator_, buffers.position_allocation);
        }
        vmaDestroyBuffer(allocator_, buffers.position_buffer, buffers.position_allocation);
        // ... уничтожение остальных буферов
    }

private:
    VmaAllocator allocator_;
    VkDeviceSize chunk_size_;
};
```

### Использование std::mdspan для многомерного доступа

После загрузки данных в GPU мы можем использовать многомерные представления для удобного доступа к воксельным данным:

```cpp
#include <mdspan>

struct VoxelChunkView {
    using position_view = std::mdspan<float, std::dextents<size_t, 3>, std::layout_right>;
    using material_view = std::mdspan<uint8_t, std::dextents<size_t, 3>, std::layout_right>;
    using light_view = std::mdspan<uint16_t, std::dextents<size_t, 3>, std::layout_right>;

    position_view positions;
    material_view materials;
    light_view lights;
};

auto create_chunk_view(const VoxelChunkData& data, size_t width, size_t height, size_t depth)
    -> VoxelChunkView {
    // Предполагаем, что данные упакованы в плоские массивы
    auto pos_span = std::span<float, std::dynamic_extent>(data.positions);
    auto mat_span = std::span<uint8_t, std::dynamic_extent>(data.material_ids);
    auto light_span = std::span<uint16_t, std::dynamic_extent>(data.light_levels);

    return VoxelChunkView{
        .positions = position_view(pos_span.data(), width, height, depth),
        .materials = material_view(mat_span.data(), width, height, depth),
        .lights = light_view(light_span.data(), width, height, depth)
    };
}

// Пример доступа к конкретному вокселю
void process_voxel(const VoxelChunkView& view, size_t x, size_t y, size_t z) {
    float pos_x = view.positions(x, y, z * 3 + 0);
    float pos_y = view.positions(x, y, z * 3 + 1);
    float pos_z = view.positions(x, y, z * 3 + 2);

    uint8_t material = view.materials(x, y, z);
    uint16_t light = view.lights(x, y, z);

    // Обработка вокселя...
}
```

## Структура данных SoA (Structure of Arrays)

Для максимальной cache-locality и эффективной работы с GPU мы храним атрибуты вокселей отдельно:

```cpp
struct VoxelChunkSoA {
    // Позиции: отдельный массив для каждой компоненты (AoS для vec3, но SoA относительно других атрибутов)
    std::vector<float> position_x;
    std::vector<float> position_y;
    std::vector<float> position_z;

    // Материалы и освещение
    std::vector<uint8_t> material_ids;
    std::vector<uint16_t> light_levels;

    // Дополнительные атрибуты
    std::vector<uint8_t> occlusion;
    std::vector<uint8_t> moisture;
    std::vector<int8_t> temperature;

    // Метод для преобразования из Draco PointCloud
    static auto from_draco_point_cloud(const draco::PointCloud& pc)
        -> std::expected<VoxelChunkSoA, draco::Status> {
        VoxelChunkSoA result;

        const auto* pos_attr = pc.GetNamedAttribute(draco::GeometryAttribute::POSITION);
        if (!pos_attr || pos_attr->num_components() != 3) {
            return std::unexpected(draco::Status(draco::Status::DRACO_ERROR,
                                                 "Invalid position attribute"));
        }

        result.position_x.resize(pc.num_points());
        result.position_y.resize(pc.num_points());
        result.position_z.resize(pc.num_points());

        // Извлечение с сохранением SoA
        for (draco::PointIndex i(0); i < pc.num_points(); ++i) {
            float pos[3];
            pos_attr->GetMappedValue(i, pos);
            result.position_x[i.value()] = pos[0];
            result.position_y[i.value()] = pos[1];
            result.position_z[i.value()] = pos[2];
        }

        // Извлечение пользовательских атрибутов
        extract_custom_attributes(pc, result);

        return result;
    }

private:
    static void extract_custom_attributes(const draco::PointCloud& pc, VoxelChunkSoA& soa) {
        for (int attr_id = 0; attr_id < pc.num_attributes(); ++attr_id) {
            const auto* attr = pc.attribute(attr_id);
            if (attr->attribute_type() == draco::GeometryAttribute::GENERIC) {
                const auto* metadata = pc.GetAttributeMetadataByAttributeId(attr_id);
                if (!metadata) continue;

                std::string semantic;
                if (!metadata->GetEntryString("semantic", &semantic)) continue;

                if (semantic == "material_id" && attr->num_components() == 1) {
                    extract_attribute<uint8_t>(*attr, soa.material_ids);
                } else if (semantic == "light_level" && attr->num_components() == 1) {
                    extract_attribute<uint16_t>(*attr, soa.light_levels);
                }
                }
            }
        }
    }

    template<typename T>
    static void extract_attribute(const draco::PointAttribute& attr, std::vector<T>& output) {
        output.resize(attr.size());
        for (draco::AttributeValueIndex i(0); i < attr.size(); ++i) {
            T value;
            attr.GetValue(i, &value);
            output[i.value()] = value;
        }
    }
};
```

> **Для понимания:** SoA — это как сортировка носков по цвету в разные ящики. Когда GPU нужно обработать все красные
> носки (material_id), он открывает один ящик и берёт их подряд, не перебирая все остальные атрибуты. Это даёт
> максимальную cache-locality и предсказуемость доступа.

## Метаданные и Property Tables для воксельных свойств

Draco поддерживает расширение `EXT_structural_metadata` через `DRACO_TRANSCODER_SUPPORTED`. Это позволяет хранить
структурированные свойства вокселей (тип материала, твёрдость, горючесть и т.д.) непосредственно в сжатом файле.

> **Для понимания:** Метаданные чанка — это таможенная декларация на контейнере. Мы не читаем её, когда грузчики (GPU)
> разгружают коробки (воксели). Мы читаем её один раз при въезде в порт (инициализация), чтобы понять, нужно ли вообще
> пускать этот контейнер на конвейер.

### Использование Property Tables

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
#include <draco/metadata/structural_metadata.h>

struct VoxelPropertyTable {
    std::vector<uint8_t> material_types;
    std::vector<float> hardness;
    std::vector<float> flammability;
    std::vector<uint8_t> transparency;
};

auto extract_voxel_properties(const draco::PointCloud& pc)
    -> std::expected<VoxelPropertyTable, draco::Status> {
    const auto* structural_meta = pc.GetStructuralMetadata();
    if (!structural_meta) {
        return std::unexpected(draco::Status(draco::Status::DRACO_ERROR,
                                             "No structural metadata found"));
    }

    VoxelPropertyTable result;

    // Поиск property table по имени
    for (int i = 0; i < structural_meta->NumPropertyTables(); ++i) {
        const auto* prop_table = structural_meta->GetPropertyTable(i);
        if (!prop_table) continue;

        const auto* schema = prop_table->schema();
        if (!schema || schema->name != "VoxelProperties") continue;

        // Извлечение свойств
        for (int prop_idx = 0; prop_idx < prop_table->NumProperties(); ++prop_idx) {
            const auto* prop = prop_table->GetProperty(prop_idx);
            if (!prop) continue;

            if (prop->name == "material_type") {
                extract_property<uint8_t>(*prop, result.material_types);
            } else if (prop->name == "hardness") {
                extract_property<float>(*prop, result.hardness);
            } else if (prop->name == "flammability") {
                extract_property<float>(*prop, result.flammability);
            } else if (prop->name == "transparency") {
                extract_property<uint8_t>(*prop, result.transparency);
            }
        }
    }

    return result;
}

template<typename T>
void extract_property(const draco::PropertyTableProperty& prop, std::vector<T>& output) {
    output.resize(prop.count());
    for (size_t i = 0; i < prop.count(); ++i) {
        T value;
        if (prop.GetValue(i, &value)) {
            output[i] = value;
        }
    }
}
#endif
```

### Создание Property Tables при кодировании

```cpp
#ifdef DRACO_TRANSCODER_SUPPORTED
auto add_voxel_properties(draco::PointCloud& pc, const VoxelPropertyTable& properties)
    -> draco::Status {
    auto structural_meta = std::make_unique<draco::StructuralMetadata>();

    draco::PropertyTableSchema schema;
    schema.name = "VoxelProperties";

    // Добавление свойств в схему
    draco::PropertyTableProperty material_prop;
    material_prop.name = "material_type";
    material_prop.type = draco::DT_UINT8;
    material_prop.component_type = draco::DT_UINT8;

    draco::PropertyTableProperty hardness_prop;
    hardness_prop.name = "hardness";
    hardness_prop.type = draco::DT_FLOAT32;
    hardness_prop.component_type = draco::DT_FLOAT32;

    // Создание property table
    auto prop_table = std::make_unique<draco::PropertyTable>();
    prop_table->SetSchema(schema);

    // Заполнение данными
    prop_table->AddProperty(material_prop, properties.material_types.data(),
                            properties.material_types.size());
    prop_table->AddProperty(hardness_prop, properties.hardness.data(),
                            properties.hardness.size());

    structural_meta->AddPropertyTable(std::move(prop_table));
    pc.SetStructuralMetadata(std::move(structural_meta));

    return draco::Status();
}
#endif
```

## Оптимизация кодирования для воксельных данных

Воксельные данные имеют специфические характеристики: регулярная структура, множество повторяющихся значений, низкая
энтропия. Мы можем использовать это для улучшения сжатия.

### Специализированные настройки кодировщика

```cpp
class VoxelDracoEncoder {
public:
    VoxelDracoEncoder() {
        encoder_.SetSpeedOptions(5, 7);  // Баланс скорости кодирования/декодирования
        encoder_.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);  // Быстрее для регулярных данных
    }

    auto encode_voxel_chunk(const VoxelChunkSoA& chunk)
        -> std::expected<std::vector<std::byte>, draco::Status> {
        draco::PointCloud pc;

        // Добавление позиций
        draco::GeometryAttribute pos_attr;
        pos_attr.Init(draco::GeometryAttribute::POSITION,
                      nullptr,  // data
                      3,        // components
                      draco::DT_FLOAT32,
                      false,    // normalized
                      sizeof(float) * 3,
                      0);

        int pos_attr_id = pc.AddAttribute(pos_attr, true, chunk.position_x.size());
        auto* pos_attr_ptr = pc.attribute(pos_attr_id);

        for (size_t i = 0; i < chunk.position_x.size(); ++i) {
            float pos[3] = {chunk.position_x[i], chunk.position_y[i], chunk.position_z[i]};
            pos_attr_ptr->SetAttributeValue(draco::AttributeValueIndex(i), pos);
        }

        // Добавление пользовательских атрибутов с метаданными
        add_custom_attribute(pc, "material_id", chunk.material_ids);
        add_custom_attribute(pc, "light_level", chunk.light_levels);

        // Настройка квантования
        encoder_.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);  // ~0.05% точности
        encoder_.SetAttributeQuantizationForAttribute(pos_attr_id, 12);

        // Кодирование
        draco::EncoderBuffer buffer;
        auto status = encoder_.EncodePointCloudToBuffer(pc, &buffer);

        if (!status.ok()) {
            return std::unexpected(status);
        }

        std::vector<std::byte> result(
            reinterpret_cast<const std::byte*>(buffer.data()),
            reinterpret_cast<const std::byte*>(buffer.data()) + buffer.size()
        );

        return result;
    }

private:
    template<typename T>
    void add_custom_attribute(draco::PointCloud& pc, std::string_view semantic,
                              const std::vector<T>& data) {
        draco::GeometryAttribute attr;
        attr.Init(draco::GeometryAttribute::GENERIC,
                  nullptr,
                  1,
                  draco::DataTypeForType<T>(),
                  false,
                  sizeof(T),
                  0);

        int attr_id = pc.AddAttribute(attr, true, data.size());
        auto* attr_ptr = pc.attribute(attr_id);

        for (size_t i = 0; i < data.size(); ++i) {
            attr_ptr->SetAttributeValue(draco::AttributeValueIndex(i), &data[i]);
        }

        // Добавление метаданных
        auto metadata = std::make_unique<draco::AttributeMetadata>(attr_id);
        metadata->AddEntryString("semantic", std::string(semantic));
        pc.AddAttributeMetadata(attr_id, std::move(metadata));
    }

    draco::Encoder encoder_;
};
```

> **Для понимания:** Квантование позиций вокселей — это как уменьшение разрешения карты. Если ваш мир состоит из блоков
> 1×1×1 метр, нет смысла хранить позиции с точностью до миллиметра. 12 бит (0.05% точности) достаточно, чтобы отличить
> один блок от другого, но в 4 раза меньше данных.

## Производительность и метрики для воксельных чанков

### Ожидаемые показатели сжатия

| Тип чанка                  | Размер исходный | Размер сжатый | Коэффициент | Время декодирования |
|----------------------------|-----------------|---------------|-------------|---------------------|
| 16×16×16 (4096 вокселей)   | 196 КБ          | 8-12 КБ       | 16-24×      | 0.5-1 мс            |
| 32×32×32 (32768 вокселей)  | 1.5 МБ          | 50-80 КБ      | 18-30×      | 3-6 мс              |
| 64×64×64 (262144 вокселей) | 12 МБ           | 400-700 КБ    | 17-30×      | 20-40 мс            |

### Влияние параметров на производительность

```cpp
struct EncodingProfile {
    std::string_view name;
    int position_bits;      // Квантование позиций
    int encoding_speed;     // 0-10 (медленнее-быстрее)
    int decoding_speed;     // 0-10 (медленнее-быстрее)
    bool use_edgebreaker;   // Edgebreaker vs Sequential
};

constexpr EncodingProfile profiles[] = {
    {"Archive", 10, 0, 0, true},      // Максимальное сжатие, медленное декодирование
    {"Streaming", 12, 5, 7, false},   // Баланс для стриминга
    {"RealTime", 14, 10, 10, false},  // Максимальная скорость, меньшее сжатие
};

auto select_profile(std::string_view use_case) -> const EncodingProfile& {
    if (use_case == "disk_storage") return profiles[0];
    if (use_case == "network_streaming") return profiles[1];
    return profiles[2];  // real_time_rendering
}
```

## Решение специфических проблем

### Проблема: Высокая загрузка CPU при потоковой загрузке

**Симптом:** Основной поток блокируется на декодировании Draco, частота кадров падает.

**Решение:** Использование Job System с приоритетами:

```cpp
class PrioritizedDracoJobSystem : public DracoJobSystem {
public:
    enum class Priority {
        Low,      // Фоновая загрузка (дальние чанки)
        Normal,   // Обычная загрузка
        High,     // Чанки в поле зрения
        Critical  // Чанки непосредственно перед камерой
    };

    auto decode_chunk_async(std::span<const std::byte> data, size_t chunk_id, Priority priority)
        -> std::future<DecodeResult> {
        auto promise = std::make_shared<std::promise<DecodeResult>>();

        {
            std::lock_guard lock(queue_mutex_);
            auto it = std::ranges::find_if(tasks_, [priority](const auto& task) {
                return task.priority < priority;  // Вставляем перед задачами с меньшим приоритетом
            });

            tasks_.insert(it, {
                .compressed_data = std::vector<std::byte>(data.begin(), data.end()),
                .chunk_id = chunk_id,
                .priority = priority,
                .promise = std::move(promise)
            });
        }

        cv_.notify_one();
        return promise->get_future();
    }

private:
    struct PrioritizedDecodeTask : DecodeTask {
        Priority priority;

        auto operator<=>(const PrioritizedDecodeTask& other) const {
            return priority <=> other.priority;
        }
    };
};
```

### Проблема: Память GPU фрагментирована из-за множества маленьких буферов

**Решение:** Использование пулов памяти VMA:

```cpp
class VoxelBufferPool {
public:
    VoxelBufferPool(VmaAllocator allocator, size_t chunk_size, size_t pool_size)
        : allocator_(allocator), chunk_size_(chunk_size) {

        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = chunk_size * pool_size,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VmaAllocationCreateInfo alloc_info = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };

        vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                        &pool_buffer_, &pool_allocation_, nullptr);

        vmaMapMemory(allocator_, pool_allocation_, &mapped_data_);

        // Разметка пула на чанки
        free_chunks_.reserve(pool_size);
        for (size_t i = 0; i < pool_size; ++i) {
            free_chunks_.push_back(i);
        }
    }

    auto allocate_chunk() -> std::expected<ChunkHandle, VkResult> {
        std::lock_guard lock(mutex_);

        if (free_chunks_.empty()) {
            return std::unexpected(VK_ERROR_OUT_OF_POOL_MEMORY);
        }

        size_t chunk_id = free_chunks_.back();
        free_chunks_.pop_back();

        void* chunk_ptr = static_cast<std::byte*>(mapped_data_) + (chunk_id * chunk_size_);

        return ChunkHandle{
            .buffer = pool_buffer_,
            .offset = chunk_id * chunk_size_,
            .size = chunk_size_,
            .mapped_ptr = chunk_ptr,
            .chunk_id = chunk_id
        };
    }

    void free_chunk(ChunkHandle handle) {
        std::lock_guard lock(mutex_);
        free_chunks_.push_back(handle.chunk_id);
    }

private:
    VmaAllocator allocator_;
    VkBuffer pool_buffer_;
    VmaAllocation pool_allocation_;
    void* mapped_data_;
    size_t chunk_size_;
    std::vector<size_t> free_chunks_;
    std::mutex mutex_;
};
```

### Проблема: Задержки при первом обращении к декодированным данным

**Решение:** Prefetching и warming кэша:

```cpp
class DracoCacheWarmer {
public:
    void warm_cache(std::span<const std::byte> compressed_data) {
        // Декодируем в фоне до того, как данные понадобятся
        auto future = job_system_.decode_chunk_async(compressed_data, 0);

        // Сохраняем future для последующего использования
        std::lock_guard lock(cache_mutex_);
        warming_futures_.push_back(std::move(future));
    }

    auto get_warmed_data(size_t chunk_id)
        -> std::optional<VoxelChunkData> {
        std::lock_guard lock(cache_mutex_);

        // Проверяем, есть ли уже готовые данные
        auto it = std::ranges::find_if(warming_futures_, [chunk_id](auto& future) {
            return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        });

        if (it != warming_futures_.end()) {
            auto result = it->get();
            if (result.data) {
                cache_.insert({chunk_id, std::move(*result.data)});
                warming_futures_.erase(it);
                return cache_[chunk_id];
            }
        }

        return std::nullopt;
    }

private:
    DracoJobSystem job_system_;
    std::unordered_map<size_t, VoxelChunkData> cache_;
    std::vector<std::future<DecodeResult>> warming_futures_;
    std::mutex cache_mutex_;
};
```

## Интеграция с Flecs (Entity Component System)

Поскольку ProjectV использует Flecs для управления сущностями, мы должны интегрировать декодирование Draco в
ECS-паттерны.

### Компоненты для воксельных чанков

```cpp
#include <flecs.h>
#include <print>

struct VoxelChunk {
    std::vector<float> positions;
    std::vector<uint8_t> materials;
    std::vector<uint16_t> lights;
    VkBuffer gpu_buffer;
    VmaAllocation allocation;
};

struct ChunkNeedsDecoding {
    std::vector<std::byte> compressed_data;
    size_t chunk_x, chunk_y, chunk_z;
};

struct ChunkDecoded {
    // Маркерный компонент
};

// Система для декодирования чанков
void decode_chunks_system(flecs::iter& it) {
    auto world = it.world();

    // Получаем все чанки, которые нужно декодировать
    auto query = world.query_builder<ChunkNeedsDecoding>()
        .term<ChunkDecoded>().oper(flecs::Not)
        .build();

    query.each([&](flecs::entity e, ChunkNeedsDecoding& needs) {
        // Декодируем в фоновом потоке через Job System
        auto future = draco_job_system.decode_chunk_async(
            needs.compressed_data,
            hash_chunk_coords(needs.chunk_x, needs.chunk_y, needs.chunk_z)
        );

        // Сохраняем future в компоненте для последующей проверки
        e.set<DecodingFuture>({std::move(future)});

        // Удаляем компонент needs, чтобы не обрабатывать повторно
        e.remove<ChunkNeedsDecoding>();
    });
}

// Система для проверки завершения декодирования
void check_decoding_system(flecs::iter& it) {
    auto query = it.world().query_builder<DecodingFuture>()
        .term<ChunkDecoded>().oper(flecs::Not)
        .build();

    query.each([&](flecs::entity e, DecodingFuture& future) {
        if (future.future.valid() &&
            future.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {

            auto result = future.future.get();
            if (result.data) {
                // Создаём компонент VoxelChunk с декодированными данными
                e.set<VoxelChunk>(std::move(*result.data));
                e.add<ChunkDecoded>();

                // Загружаем данные в GPU
                upload_to_gpu(e);
            } else {
                std::println(stderr, "Failed to decode chunk {}: {}",
                           result.chunk_id, result.data.error().error_msg());
            }

            e.remove<DecodingFuture>();
        }
    });
}
```

## Best Practices для высокопроизводительного движка

### 1. Пакетная обработка

Всегда декодируйте чанки пачками, а не по одному. Job System должен обрабатывать группы задач для минимизации накладных
расходов.

### 2. Приоритизация

Используйте систему приоритетов, чтобы чанки в поле зрения камеры декодировались первыми.

### 3. Предзагрузка

Декодируйте чанки, которые скоро понадобятся, заранее. Используйте предиктивную логику на основе движения камеры.

### 4. Кэширование

Кэшируйте декодированные чанки в памяти, чтобы избежать повторного декодирования при повторном посещении области.

### 5. Мониторинг производительности

Используйте Tracy или аналогичные инструменты для профилирования времени декодирования, использования памяти и загрузки
CPU.

```cpp
#include <tracy/Tracy.hpp>

void decode_with_profiling(std::span<const std::byte> data) {
    ZoneScopedN("DracoDecode");

    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data.data()), data.size());

    draco::Decoder decoder;

    {
        ZoneScopedN("DecodePointCloud");
        auto pc = decoder.DecodePointCloudFromBuffer(&buffer);
        TracyPlot("DracoDecodeTime", TracyPlotUnits::TimeMs);
    }
}
```

## Заключение

Draco — мощный инструмент для сжатия воксельных данных, но требует правильной интеграции в высокопроизводительный
движок. Ключевые принципы:

1. **Распараллеливание на уровне чанков** — используйте Job System для независимого декодирования каждого чанка.
2. **Zero-copy upload** — распаковывайте напрямую в замапленную память VMA.
3. **SoA хранение** — разделяйте атрибуты для cache-locality.
4. **Приоритизация** — декодируйте сначала то, что видит камеры.
5. **Интеграция с ECS** — используйте компоненты Flecs для управления состоянием чанков.

Следуя этим принципам, вы сможете сжимать воксельные данные в 20-30 раз с декодированием за миллисекунды, что критически
важно для масштабируемых воксельных миров.

---

> **Для понимания:** Интеграция Draco в высокопроизводительный движок — это как настройка гоночного автомобиля. Каждая
> деталь имеет значение: вес (размер данных), аэродинамика (cache-locality), двигатель (многопоточность) и топливная
> система (память GPU). Только сбалансированная настройка всех компонентов даст максимальную производительность.

