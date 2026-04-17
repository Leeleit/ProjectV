# Интеграция с ECS (системы компонентов)

**🟡 Уровень 2: Средний**

Этот документ описывает универсальные принципы интеграции miniaudio с системами компонентов (ECS) и паттернами
управления аудио в современных игровых движках. Информация представлена в архитектурно-независимом виде и может быть
адаптирована под различные ECS реализации (flecs, EnTT, Unity ECS и другие).

## Содержание

1. [Основные концепции](#1-основные-концепции)
  - [Компоненты для аудио](#11-компоненты-для-аудио)
  - [Слушатель (Listener)](#12-слушатель-listener)
  - [Специфика ECS архитектур](#13-специфика-ecs-архитектур)

2. [Архитектурные паттерны](#2-архитектурные-паттерны)
  - [Event-based интеграция](#21-event-based-интеграция)
  - [Component-based управление](#22-component-based-управление)
  - [Hybrid подход](#23-hybrid-подход)

3. [Управление жизненным циклом звуков](#3-управление-жизненным-циклом-звуков)
  - [Создание и уничтожение](#31-создание-и-уничтожение)
  - [Пул звуков (Sound Pool)](#32-пул-звуков-sound-pool)
  - [Resource management в ECS](#33-resource-management-в-ecs)

4. [Spatial Audio в ECS](#4-spatial-audio-в-ecs)
  - [Компоненты для 3D позиционирования](#41-компоненты-для-3d-позиционирования)
  - [Системы обновления позиций](#42-системы-обновления-позиций)
  - [Дистанционный culling](#43-дистанционный-culling)

5. [Производительность и оптимизация](#5-производительность-и-оптимизация)
  - [Batch processing звуков](#51-batch-processing-звуков)
  - [Data-oriented дизайн для аудио](#52-data-oriented-дизайн-для-аудио)
  - [Минимизация аллокаций](#53-минимизация-аллокаций)

6. [Примеры для различных ECS](#6-примеры-для-различных-ecs)
  - [flecs](#61-flecs)
  - [EnTT](#62-entt)
  - [Unity ECS/Burst](#63-unity-ecsburst)
  - [Самописные системы](#64-самописные-системы)

---

## 1. Основные концепции

### 1.1 Компоненты для аудио

В ECS-архитектуре аудио источники представляются как компоненты. Вот пример обобщённой структуры компонента аудио:

```cpp
// Обобщённый компонент AudioSource, независимый от конкретной ECS
struct GenericAudioSource {
    void* sound_handle;         // Указатель на ma_sound или аналогичную структуру
    float base_volume = 1.0f;
    float max_distance = 100.0f;
    bool spatial = true;
    bool looping = false;
    bool playing = false;
    bool auto_release = true;   // Автоматически освобождать звук после завершения

    // Для 3D позиционирования
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;

    // Для доплер-эффекта
    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
};
```

### 1.2 Слушатель (Listener)

Слушатель обычно представляется как отдельная сущность или singleton-компонент:

```cpp
// Компонент слушателя
struct AudioListener {
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    float forward_x = 0.0f, forward_y = 0.0f, forward_z = -1.0f;
    float up_x = 0.0f, up_y = 1.0f, up_z = 0.0f;

    // Глобальный engine для всех слушателей
    void* audio_engine = nullptr;

    // Параметры слушателя
    float master_volume = 1.0f;
    int listener_index = 0;  // Индекс слушателя в движке (поддержка многопользовательских игр)
};
```

### 1.3 Специфика ECS архитектур

Разные ECS имеют различные характеристики:

| Архитектура    | Характеристики аудио-интеграции                                          |
|----------------|--------------------------------------------------------------------------|
| **flecs**      | Многослойные системы, query-based обновления, хорошая поддержка иерархий |
| **EnTT**       | Registry-based, signal/slot системы, высокопроизводительные queries      |
| **Unity ECS**  | Burst компиляция, Jobs system, оптимизировано для больших объёмов данных |
| **Самописные** | Полный контроль, но требуется реализация базовой инфраструктуры          |

## 2. Архитектурные паттерны

### 2.1 Event-based интеграция

Использование событий для триггеров аудио:

```cpp
// Пример события воспроизведения звука
struct PlaySoundEvent {
    const char* sound_path = nullptr;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    float volume = 1.0f;
    bool spatial = true;
    bool looping = false;

    // Приоритет для управления пулами
    int priority = 0;
};

// Система обработки аудио событий
class AudioEventSystem {
public:
    void process_events(ma_engine* engine, const std::vector<PlaySoundEvent>& events) {
        for (const auto& event : events) {
            // Создание или использование звука из пула
            ma_sound* sound = acquire_sound_from_pool(engine, event.sound_path);
            if (sound) {
                if (event.spatial) {
                    ma_sound_set_position(sound, event.pos_x, event.pos_y, event.pos_z);
                }
                ma_sound_set_volume(sound, event.volume);
                ma_sound_start(sound);
            }
        }
    }

private:
    ma_sound* acquire_sound_from_pool(ma_engine* engine, const char* path) {
        // Реализация пула звуков
        // ...
    }
};
```

### 2.2 Component-based управление

Прямое управление через компоненты:

```cpp
// Обобщённая система обновления компонентов AudioSource
template<typename EntityType, typename RegistryType>
void update_audio_sources(RegistryType& registry, ma_engine* engine, const AudioListener& listener) {
    // Query для всех сущностей с компонентами AudioSource и Position
    registry.template view<GenericAudioSource, PositionComponent>().each(
        [engine, &listener](EntityType entity, GenericAudioSource& audio, PositionComponent& pos) {

            if (!audio.sound_handle) return;
            ma_sound* sound = static_cast<ma_sound*>(audio.sound_handle);

            // Обновление позиции
            if (audio.spatial) {
                ma_sound_set_position(sound, pos.x, pos.y, pos.z);
            }

            // Проверка дистанции и управление воспроизведением
            float distance = calculate_distance(pos.x, pos.y, pos.z,
                                              listener.pos_x, listener.pos_y, listener.pos_z);

            if (distance > audio.max_distance) {
                if (audio.playing) {
                    ma_sound_stop(sound);
                    audio.playing = false;
                }
            } else {
                if (!audio.playing) {
                    ma_sound_start(sound);
                    audio.playing = true;
                }

                // Настройка громкости по расстоянию
                float volume_factor = 1.0f - (distance / audio.max_distance);
                float volume = audio.base_volume * volume_factor;
                ma_sound_set_volume(sound, volume);
            }
        }
    );
}
```

### 2.3 Hybrid подход

Комбинирование событий и компонентов:

```cpp
// Hybrid система
class HybridAudioSystem {
public:
    struct QueuedSound {
        Entity entity;
        const char* path;
        bool play_once;
    };

    void update(RegistryType& registry, ma_engine* engine) {
        // 1. Обработка событий (новые звуки)
        process_sound_events(registry, engine);

        // 2. Обновление существующих компонентов
        update_existing_sounds(registry, engine);

        // 3. Очистка завершённых звуков
        cleanup_finished_sounds(registry);
    }

private:
    std::vector<QueuedSound> m_sound_queue;

    void process_sound_events(RegistryType& registry, ma_engine* engine) {
        for (const auto& queued : m_sound_queue) {
            // Создание компонента AudioSource для сущности
            registry.template emplace<GenericAudioSource>(queued.entity,
                create_sound_for_entity(engine, queued.path, queued.play_once));
        }
        m_sound_queue.clear();
    }
};
```

## 3. Управление жизненным циклом звуков

### 3.1 Создание и уничтожение

Правильное управление памятью в ECS:

```cpp
// Фабрика звуков с привязкой к жизненному циклу сущности
class SoundFactory {
public:
    ma_sound* create_sound_for_entity(ma_engine* engine, const char* path, Entity entity) {
        ma_sound* sound = new ma_sound();
        ma_result result = ma_sound_init_from_file(engine, path, 0, nullptr, nullptr, sound);

        if (result == MA_SUCCESS) {
            // Связываем звук с сущностью для автоматического освобождения
            m_entity_sound_map[entity] = sound;
            return sound;
        } else {
            delete sound;
            return nullptr;
        }
    }

    void destroy_sound_for_entity(Entity entity) {
        auto it = m_entity_sound_map.find(entity);
        if (it != m_entity_sound_map.end()) {
            ma_sound_uninit(it->second);
            delete it->second;
            m_entity_sound_map.erase(it);
        }
    }

private:
    std::unordered_map<Entity, ma_sound*> m_entity_sound_map;
};
```

### 3.2 Пул звуков (Sound Pool)

Эффективное повторное использование звуков:

```cpp
// Обобщённый пул звуков для ECS
template<size_t PoolSize = 64>
class ECSSoundPool {
public:
    ECSSoundPool(ma_engine* engine, const char* file_path)
        : m_engine(engine) {

        // Предварительное создание звуков
        for (size_t i = 0; i < PoolSize; ++i) {
            ma_sound* sound = new ma_sound();
            ma_sound_init_from_file(engine, file_path, 0, nullptr, nullptr, sound);
            m_sounds[i] = sound;
            m_available[i] = true;
        }
    }

    ~ECSSoundPool() {
        for (auto sound : m_sounds) {
            if (sound) {
                ma_sound_uninit(sound);
                delete sound;
            }
        }
    }

    // Привязка звука к сущности
    ma_sound* acquire_for_entity(Entity entity, float x, float y, float z) {
        for (size_t i = 0; i < PoolSize; ++i) {
            if (m_available[i]) {
                m_available[i] = false;
                m_entity_to_sound[entity] = m_sounds[i];
                m_sound_to_index[m_sounds[i]] = i;

                ma_sound_set_position(m_sounds[i], x, y, z);
                ma_sound_seek_to_pcm_frame(m_sounds[i], 0);
                ma_sound_start(m_sounds[i]);

                return m_sounds[i];
            }
        }
        return nullptr;
    }

    // Освобождение звука при удалении сущности
    void release_for_entity(Entity entity) {
        auto it = m_entity_to_sound.find(entity);
        if (it != m_entity_to_sound.end()) {
            ma_sound_stop(it->second);

            auto index_it = m_sound_to_index.find(it->second);
            if (index_it != m_sound_to_index.end()) {
                m_available[index_it->second] = true;
            }

            m_entity_to_sound.erase(it);
        }
    }

private:
    ma_engine* m_engine;
    std::array<ma_sound*, PoolSize> m_sounds;
    std::array<bool, PoolSize> m_available;
    std::unordered_map<Entity, ma_sound*> m_entity_to_sound;
    std::unordered_map<ma_sound*, size_t> m_sound_to_index;
};
```

### 3.3 Resource management в ECS

Интеграция с системами управления ресурсами ECS:

```cpp
// Ресурсный менеджер для аудио в ECS
class AudioResourceManager {
public:
    // Регистрация звукового файла как ресурса
    ResourceID register_sound(const char* path) {
        ResourceID id = generate_resource_id();
        m_sound_paths[id] = path;

        // Предзагрузка в фоновом режиме
        background_load_sound(path);

        return id;
    }

    // Получение звука по ResourceID
    ma_sound* get_sound(ResourceID id) {
        auto it = m_loaded_sounds.find(id);
        if (it != m_loaded_sounds.end()) {
            return it->second;
        }

        // Ленивая загрузка
        auto path_it = m_sound_paths.find(id);
        if (path_it != m_sound_paths.end()) {
            ma_sound* sound = load_sound_immediately(path_it->second.c_str());
            if (sound) {
                m_loaded_sounds[id] = sound;
                return sound;
            }
        }

        return nullptr;
    }

    // Освобождение неиспользуемых звуков
    void release_unused_sounds() {
        std::vector<ResourceID> to_release;

        for (const auto& pair : m_loaded_sounds) {
            if (!is_sound_in_use(pair.first)) {
                to_release.push_back(pair.first);
            }
        }

        for (ResourceID id : to_release) {
            ma_sound_uninit(m_loaded_sounds[id]);
            delete m_loaded_sounds[id];
            m_loaded_sounds.erase(id);
        }
    }

private:
    std::unordered_map<ResourceID, std::string> m_sound_paths;
    std::unordered_map<ResourceID, ma_sound*> m_loaded_sounds;
    std::unordered_set<ResourceID> m_sounds_in_use;

    ResourceID generate_resource_id() {
        static ResourceID counter = 0;
        return counter++;
    }
};
```

## 4. Spatial Audio в ECS

### 4.1 Компоненты для 3D позиционирования

```cpp
// Компонент для 3D позиционирования (независимый от конкретной математической библиотеки)
struct Transform3D {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float rotation_x = 0.0f, rotation_y = 0.0f, rotation_z = 0.0f;
    float scale_x = 1.0f, scale_y = 1.0f, scale_z = 1.0f;
};

// Компонент для физического движения (для доплер-эффекта)
struct Velocity3D {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};
```

### 4.2 Системы обновления позиций

```cpp
// Обобщённая система spatial audio
class SpatialAudioSystem {
public:
    void update(RegistryType& registry, ma_engine* engine, const AudioListener& listener) {
        // Query для всех звуков с трансформами
        auto view = registry.template view<GenericAudioSource, Transform3D>();

        for (auto [entity, audio, transform] : view.each()) {
            if (!audio.sound_handle || !audio.spatial) continue;

            ma_sound* sound = static_cast<ma_sound*>(audio.sound_handle);

            // Обновление позиции
            ma_sound_set_position(sound, transform.x, transform.y, transform.z);

            // Обновление ориентации (если поддерживается)
            if (registry.template all_of<Velocity3D>(entity)) {
                auto& velocity = registry.template get<Velocity3D>(entity);

                // Настройка доплер-эффекта
                ma_sound_set_velocity(sound, velocity.x, velocity.y, velocity.z);
                ma_sound_set_doppler_factor(sound, calculate_doppler_factor(velocity, listener));
            }

            // Расстояние до слушателя
            float distance = std::sqrt(
                std::pow(transform.x - listener.pos_x, 2) +
                std::pow(transform.y - listener.pos_y, 2) +
                std::pow(transform.z - listener.pos_z, 2)
            );

            // Управление громкостью по расстоянию
            update_volume_by_distance(sound, audio, distance, listener);

            // Управление воспроизведением по расстоянию
            update_playback_by_distance(sound, audio, distance);
        }
    }

private:
    float calculate_doppler_factor(const Velocity3D& velocity, const AudioListener& listener) {
        // Упрощённая модель доплер-эффекта
        float relative_speed = std::sqrt(
            std::pow(velocity.x - listener.vel_x, 2) +
            std::pow(velocity.y - listener.vel_y, 2) +
            std::pow(velocity.z - listener.vel_z, 2)
        );

        const float speed_of_sound = 343.0f; // м/с
        return speed_of_sound / (speed_of_sound + relative_speed);
    }

    void update_volume_by_distance(ma_sound* sound, GenericAudioSource& audio,
                                  float distance, const AudioListener& listener) {
        if (distance >= audio.max_distance) {
            ma_sound_set_volume(sound, 0.0f);
        } else {
            // Inverse square law
            float attenuation = 1.0f / (1.0f + distance * distance);
            float volume = audio.base_volume * attenuation * listener.master_volume;
            ma_sound_set_volume(sound, volume);
        }
    }

    void update_playback_by_distance(ma_sound* sound, GenericAudioSource& audio, float distance) {
        if (distance > audio.max_distance) {
            if (audio.playing) {
                ma_sound_stop(sound);
                audio.playing = false;
            }
        } else {
            if (!audio.playing) {
                ma_sound_start(sound);
                audio.playing = true;
            }
        }
    }
};
```

### 4.3 Дистанционный culling

```cpp
// Система дистанционного culling'а звуков
class AudioCullingSystem {
public:
    void update(RegistryType& registry, const AudioListener& listener) {
        const float culling_distance = 200.0f; // Настраиваемое значение

        auto view = registry.template view<GenericAudioSource, Transform3D>();

        for (auto [entity, audio, transform] : view.each()) {
            if (!audio.sound_handle) continue;

            float distance = std::sqrt(
                std::pow(transform.x - listener.pos_x, 2) +
                std::pow(transform.y - listener.pos_y, 2) +
                std::pow(transform.z - listener.pos_z, 2)
            );

            if (distance > culling_distance) {
                // Полное отключение звука за пределами culling distance
                ma_sound* sound = static_cast<ma_sound*>(audio.sound_handle);
                if (ma_sound_is_playing(sound)) {
                    ma_sound_stop(sound);
                    audio.playing = false;
                }

                // Можно также выгрузить ресурс, если он не нужен
                if (audio.auto_release && !is_sound_needed_soon(entity)) {
                    release_sound_resource(sound);
                    audio.sound_handle = nullptr;
                }
            }
        }
    }

private:
    bool is_sound_needed_soon(Entity entity) {
        // Логика определения, понадобится ли звук в ближайшее время
        // Например, проверка активности сущности, её скорости и т.д.
        return true;
    }

    void release_sound_resource(ma_sound* sound) {
        // Освобождение ресурсов звука (возврат в пул или полное удаление)
        // ...
    }
};
```

## 5. Производительность и оптимизация

### 5.1 Batch processing звуков

Обработка звуков пакетами для улучшения cache locality:

```cpp
// Пакетная обработка звуков
class BatchAudioProcessor {
public:
    struct AudioBatch {
        std::vector<ma_sound*> sounds;
        std::vector<float> positions_x;
        std::vector<float> positions_y;
        std::vector<float> positions_z;
        std::vector<float> volumes;
        std::vector<bool> should_play;
    };

    void process_batch(AudioBatch& batch, const AudioListener& listener) {
        // Обновление позиций пакетом (лучше для cache locality)
        for (size_t i = 0; i < batch.sounds.size(); ++i) {
            ma_sound_set_position(batch.sounds[i],
                batch.positions_x[i],
                batch.positions_y[i],
                batch.positions_z[i]);

            ma_sound_set_volume(batch.sounds[i], batch.volumes[i]);

            if (batch.should_play[i]) {
                if (!ma_sound_is_playing(batch.sounds[i])) {
                    ma_sound_start(batch.sounds[i]);
                }
            } else {
                if (ma_sound_is_playing(batch.sounds[i])) {
                    ma_sound_stop(batch.sounds[i]);
                }
            }
        }
    }

    // Сборка пакета из ECS компонентов
    AudioBatch build_batch_from_ecs(RegistryType& registry, const AudioListener& listener) {
        AudioBatch batch;

        auto view = registry.template view<GenericAudioSource, Transform3D>();
        view.each([&](auto entity, GenericAudioSource& audio, Transform3D& transform) {
            if (!audio.sound_handle) return;

            ma_sound* sound = static_cast<ma_sound*>(audio.sound_handle);
            batch.sounds.push_back(sound);
            batch.positions_x.push_back(transform.x);
            batch.positions_y.push_back(transform.y);
            batch.positions_z.push_back(transform.z);

            // Расчёт громкости
            float distance = calculate_distance(transform, listener);
            float volume = (distance < audio.max_distance) ?
                audio.base_volume * (1.0f - distance / audio.max_distance) : 0.0f;
            batch.volumes.push_back(volume);

            // Определение, должен ли звук играть
            batch.should_play.push_back(distance <= audio.max_distance);
        });

        return batch;
    }
};
```

### 5.2 Data-oriented дизайн для аудио

Структурирование данных для оптимальной обработки:

```cpp
// Data-oriented структура для аудио компонентов
struct AudioComponentData {
    // SoA (Structure of Arrays) layout для лучшей cache locality
    std::vector<ma_sound*> sounds;
    std::vector<float> base_volumes;
    std::vector<float> max_distances;
    std::vector<bool> spatial_flags;
    std::vector<bool> playing_flags;
    std::vector<bool> looping_flags;

    // Позиции (если spatial)
    std::vector<float> positions_x;
    std::vector<float> positions_y;
    std::vector<float> positions_z;

    // Методы для работы с данными
    void add_component(ma_sound* sound, float volume, float max_dist,
                      bool spatial, float x, float y, float z) {
        sounds.push_back(sound);
        base_volumes.push_back(volume);
        max_distances.push_back(max_dist);
        spatial_flags.push_back(spatial);
        playing_flags.push_back(false);
        looping_flags.push_back(false);

        if (spatial) {
            positions_x.push_back(x);
            positions_y.push_back(y);
            positions_z.push_back(z);
        } else {
            positions_x.push_back(0.0f);
            positions_y.push_back(0.0f);
            positions_z.push_back(0.0f);
        }
    }

    void update_positions(const AudioListener& listener) {
        for (size_t i = 0; i < sounds.size(); ++i) {
            if (!spatial_flags[i]) continue;

            // Обновление позиции
            ma_sound_set_position(sounds[i], positions_x[i], positions_y[i], positions_z[i]);

            // Расчёт расстояния и громкости
            float distance = std::sqrt(
                std::pow(positions_x[i] - listener.pos_x, 2) +
                std::pow(positions_y[i] - listener.pos_y, 2) +
                std::pow(positions_z[i] - listener.pos_z, 2)
            );

            if (distance <= max_distances[i]) {
                float volume = base_volumes[i] * (1.0f - distance / max_distances[i]);
                ma_sound_set_volume(sounds[i], volume);

                if (!playing_flags[i]) {
                    ma_sound_start(sounds[i]);
                    playing_flags[i] = true;
                }
            } else {
                if (playing_flags[i]) {
                    ma_sound_stop(sounds[i]);
                    playing_flags[i] = false;
                }
            }
        }
    }
};
```

### 5.3 Минимизация аллокаций

```cpp
// Пул компонентов для избежания частых аллокаций
template<typename ComponentType, size_t PreallocatedCount = 1024>
class ComponentPool {
public:
    ComponentPool() {
        // Предварительное выделение памяти
        m_components.reserve(PreallocatedCount);
        m_free_indices.reserve(PreallocatedCount);

        for (size_t i = 0; i < PreallocatedCount; ++i) {
            m_free_indices.push_back(i);
        }
    }

    ComponentType* allocate() {
        if (m_free_indices.empty()) {
            // Расширение пула при необходимости
            size_t new_index = m_components.size();
            m_components.resize(m_components.size() * 2);
            m_free_indices.push_back(new_index);

            for (size_t i = new_index + 1; i < m_components.size(); ++i) {
                m_free_indices.push_back(i);
            }
        }

        size_t index = m_free_indices.back();
        m_free_indices.pop_back();

        return &m_components[index];
    }

    void deallocate(ComponentType* component) {
        // Находим индекс компонента
        size_t index = component - &m_components[0];
        if (index < m_components.size()) {
            // Очистка компонента
            *component = ComponentType{};
            m_free_indices.push_back(index);
        }
    }

private:
    std::vector<ComponentType> m_components;
    std::vector<size_t> m_free_indices;
};
```

## 6. Примеры для различных ECS

### 6.1 flecs

```cpp
// Пример для flecs ECS
namespace flecs_integration {

    // Компоненты
    struct AudioSource {
        ma_sound* sound = nullptr;
        float base_volume = 1.0f;
        float max_distance = 100.0f;
        bool spatial = true;
        bool looping = false;
        bool playing = false;
    };

    struct AudioListener {
        float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
        ma_engine* engine = nullptr;
    };

    // Системы
    void SpatialAudioSystem(flecs::iter& it, AudioSource* sources, Position* positions) {
        // Находим слушателя
        flecs::entity listener = it.world().lookup("audio_listener");
        if (!listener.is_alive()) return;

        const AudioListener* listener_comp = listener.get<AudioListener>();
        const Position* listener_pos = listener.get<Position>();

        if (!listener_comp || !listener_pos) return;

        // Устанавливаем позицию слушателя
        ma_engine_listener_set_position(listener_comp->engine, 0,
            listener_pos->x, listener_pos->y, listener_pos->z);

        // Обновляем источники звука
        for (auto i : it) {
            if (!sources[i].sound || !sources[i].spatial) continue;

            ma_sound_set_position(sources[i].sound,
                positions[i].x, positions[i].y, positions[i].z);

            float distance = std::sqrt(
                std::pow(positions[i].x - listener_pos->x, 2) +
                std::pow(positions[i].y - listener_pos->y, 2) +
                std::pow(positions[i].z - listener_pos->z, 2)
            );

            if (distance > sources[i].max_distance) {
                if (sources[i].playing) {
                    ma_sound_stop(sources[i].sound);
                    sources[i].playing = false;
                }
            } else {
                if (!sources[i].playing) {
                    ma_sound_start(sources[i].sound);
                    sources[i].playing = true;
                }

                float volume = sources[i].base_volume * (1.0f - distance / sources[i].max_distance);
                ma_sound_set_volume(sources[i].sound, volume);
            }
        }
    }

    // Регистрация в мире flecs
    void register_audio_components(flecs::world& world) {
        world.component<AudioSource>();
        world.component<AudioListener>();

        // Система spatial audio
        world.system<AudioSource, Position>("SpatialAudioSystem")
            .iter(SpatialAudioSystem);
    }
}
```

### 6.2 EnTT

```cpp
// Пример для EnTT ECS
namespace entt_integration {

    struct AudioSource {
        ma_sound* sound = nullptr;
        float base_volume = 1.0f;
        float max_distance = 100.0f;
        bool spatial = true;
    };

    struct Position {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };

    class AudioSystem {
    public:
        AudioSystem(entt::registry& registry, ma_engine* engine)
            : m_registry(registry), m_engine(engine) {}

        void update(const Position& listener_pos) {
            auto view = m_registry.view<AudioSource, Position>();

            view.each([&](auto entity, AudioSource& audio, Position& pos) {
                if (!audio.sound || !audio.spatial) return;

                ma_sound_set_position(audio.sound, pos.x, pos.y, pos.z);

                float distance = std::sqrt(
                    std::pow(pos.x - listener_pos.x, 2) +
                    std::pow(pos.y - listener_pos.y, 2) +
                    std::pow(pos.z - listener_pos.z, 2)
                );

                if (distance > audio.max_distance) {
                    if (ma_sound_is_playing(audio.sound)) {
                        ma_sound_stop(audio.sound);
                    }
                } else {
                    if (!ma_sound_is_playing(audio.sound)) {
                        ma_sound_start(audio.sound);
                    }

                    float volume = audio.base_volume * (1.0f - distance / audio.max_distance);
                    ma_sound_set_volume(audio.sound, volume);
                }
            });
        }

    private:
        entt::registry& m_registry;
        ma_engine* m_engine;
    };
}
```

### 6.3 Unity ECS/Burst

```cpp
// Пример для Unity ECS (псевдокод, так как Unity использует C#)
namespace UnityECSIntegration {

    // В Unity ECS компоненты - это struct с [Serializable] атрибутом
    [Serializable]
    public struct AudioSourceComponent : IComponentData {
        public Entity SoundEntity;
        public float BaseVolume;
        public float MaxDistance;
        public bool Spatial;
        public bool Playing;
    }

    [Serializable]
    public struct PositionComponent : IComponentData {
        public float3 Value;
    }

    // Система в Unity ECS
    [UpdateInGroup(typeof(AudioSystemGroup))]
    public partial class SpatialAudioSystem : SystemBase {
        protected override void OnUpdate() {
            // Получаем позицию слушателя
            float3 listenerPosition = GetSingleton<AudioListenerComponent>().Position;

            // Job для параллельной обработки
            Entities
                .WithName("UpdateSpatialAudio")
                .ForEach((ref AudioSourceComponent audio, in PositionComponent position) => {
                    if (!audio.Spatial) return;

                    // Расчёт расстояния
                    float distance = math.distance(position.Value, listenerPosition);

                    if (distance <= audio.MaxDistance) {
                        if (!audio.Playing) {
                            // Запуск звука (через команды)
                            EntityManager.AddComponent<StartAudioTag>(audio.SoundEntity);
                            audio.Playing = true;
                        }

                        // Установка громкости
                        float volume = audio.BaseVolume * (1.0f - distance / audio.MaxDistance);
                        EntityManager.SetComponentData(audio.SoundEntity,
                            new VolumeComponent { Value = volume });
                    } else {
                        if (audio.Playing) {
                            // Остановка звука
                            EntityManager.AddComponent<StopAudioTag>(audio.SoundEntity);
                            audio.Playing = false;
                        }
                    }
                })
                .ScheduleParallel();
        }
    }
}
```

### 6.4 Самописные системы

```cpp
// Пример простой самописной ECS системы
namespace CustomECS {

    class Entity {
    public:
        uint32_t id;
        std::bitset<64> component_mask;
    };

    class AudioManager {
    public:
        struct AudioComponent {
            ma_sound* sound;
            float base_volume;
            float max_distance;
            bool spatial;
            bool playing;
        };

        void update(float delta_time) {
            // Обновление всех аудио компонентов
            for (auto& [entity_id, component] : m_audio_components) {
                if (!component.sound) continue;

                // Проверка, есть ли у сущности компонент Position
                if (component.spatial && m_position_components.count(entity_id)) {
                    const auto& pos = m_position_components[entity_id];
                    const auto& listener = m_listener_position;

                    ma_sound_set_position(component.sound, pos.x, pos.y, pos.z);

                    float distance = std::sqrt(
                        std::pow(pos.x - listener.x, 2) +
                        std::pow(pos.y - listener.y, 2) +
                        std::pow(pos.z - listener.z, 2)
                    );

                    if (distance > component.max_distance) {
                        if (component.playing) {
                            ma_sound_stop(component.sound);
                            component.playing = false;
                        }
                    } else {
                        if (!component.playing) {
                            ma_sound_start(component.sound);
                            component.playing = true;
                        }

                        float volume = component.base_volume *
                            (1.0f - distance / component.max_distance);
                        ma_sound_set_volume(component.sound, volume);
                    }
                }
            }
        }

        void add_audio_component(uint32_t entity_id, AudioComponent component) {
            m_audio_components[entity_id] = component;
        }

    private:
        std::unordered_map<uint32_t, AudioComponent> m_audio_components;
        std::unordered_map<uint32_t, struct Position> m_position_components;
        struct Position m_listener_position = {0.0f, 0.0f, 0.0f};
    };
}
```

---

## Практические рекомендации

### Выбор архитектуры

1. **Для простых проектов**: Используйте event-based систему с пулом звуков.
2. **Для средних проектов**: Комбинируйте event-based и component-based подходы.
3. **Для сложных проектов**: Полностью component-based архитектура с оптимизированными системами.

### Оптимизации производительности

1. **Пакетная обработка**: Группируйте обновления звуков для улучшения cache locality.
2. **Data-oriented дизайн**: Используйте SoA (Structure of Arrays) для аудио компонентов.
3. **Пул объектов**: Избегайте аллокаций во время выполнения с помощью пулов.

### Интеграция с другими системами

1. **Профилирование**: Интегрируйте с Tracy или другими профилировщиками.
2. **Конфигурация**: Выносите параметры аудио в конфигурационные файлы.
3. **Модульность**: Разделяйте аудио логику на независимые модули.

---

## См. также

- [Основные понятия](concepts.md) — общие принципы miniaudio
- [Быстрый старт](quickstart.md) — базовые примеры использования
- [Производительность](performance.md) — оптимизации и рекомендации
- [Примеры кода](../examples/) — дополнительные примеры интеграции

---

**← [Назад к основной документации miniaudio](README.md)**
