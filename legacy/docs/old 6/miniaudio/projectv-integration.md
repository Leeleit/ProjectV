# Интеграция miniaudio в ProjectV

**🟡 Уровень 2: Средний**

Этот документ описывает специфические аспекты интеграции miniaudio в ProjectV — воксельный игровой движок. Он содержит
паттерны, оптимизации и примеры кода, ориентированные на особенности воксельного мира и архитектуры ProjectV.

## Содержание

1. [Воксельное Spatial Audio](#1-воксельное-spatial-audio)
2. [Процедурные звуки](#2-процедурные-звуки)
3. [Интеграция с ECS (flecs)](#3-интеграция-с-ecs-flecs)
4. [Оптимизации производительности](#4-оптимизации-производительности)
5. [Пул звуков для разрушения блоков](#5-пул-звуков-для-разрушения-блоков)
6. [Дистанционный culling звуков](#6-дистанционный-culling-звуков)
7. [Материалы блоков и акустика](#7-материалы-блоков-и-акустика)

---

## 1. Воксельное Spatial Audio

Воксельные миры требуют особого подхода к 3D аудио из-за большого количества препятствий (блоков) и специфической
геометрии.

### Окклюзия через блоки

Звук должен заглушаться при прохождении через блоки. Используйте Raycast по вокселям для расчёта окклюзии.

```cpp
float calculate_occlusion(VoxelWorld* world, glm::vec3 source, glm::vec3 listener) {
    // Bresenham 3D по вокселям
    int blocks = raycast_voxels(world, source, listener);

    // Каждый блок гасит звук (коэффициент 0.7)
    float occlusion = powf(0.7f, static_cast<float>(blocks));

    // Минимальная громкость даже при полной окклюзии
    occlusion = std::max(occlusion, 0.1f);

    return occlusion;
}

// Использование в audio system
void apply_occlusion(ma_sound* sound, VoxelWorld* world, glm::vec3 source_pos) {
    glm::vec3 listener_pos = get_camera_position();
    float occlusion = calculate_occlusion(world, source_pos, listener_pos);

    float base_volume;
    ma_sound_get_volume(sound, &base_volume);
    ma_sound_set_volume(sound, base_volume * occlusion);
}
```

### Дистанционное затухание

В больших воксельных мирах нужно реалистичное затухание звука с расстоянием. Используйте обратное квадратичное затухание
для реализма.

```cpp
float calculate_distance_attenuation(glm::vec3 source, glm::vec3 listener,
                                   float max_distance, float rolloff_factor = 1.0f) {
    float distance = glm::distance(source, listener);

    if (distance >= max_distance) {
        return 0.0f;
    }

    // Inverse square law with clamp
    float attenuation = 1.0f / (1.0f + rolloff_factor * distance * distance);
    return attenuation;
}
```

## 2. Процедурные звуки

Для ветра, воды, лавы и других процедурных звуков используйте генерацию на лету через `ma_noise` и `ma_waveform`.

### Генератор ветра

```cpp
void wind_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    static ma_noise noise;
    static bool initialized = false;

    if (!initialized) {
        ma_noise_config config = ma_noise_config_init(ma_format_f32, 1, ma_noise_type_white);
        ma_noise_init(&config, NULL, &noise);
        initialized = true;
    }

    float* output = static_cast<float*>(pOutput);
    float wind_strength = get_wind_strength(); // Изменяется со временем

    ma_noise_read_pcm_frames(&noise, pOutput, frameCount, NULL);

    // Применяем низкочастотный фильтр для звука ветра
    for (ma_uint32 i = 0; i < frameCount; ++i) {
        output[i] *= wind_strength * 0.1f; // Ослабляем громкость
    }
}

// Инициализация устройства для процедурного звука
ma_result init_procedural_audio(ma_device* device, ma_device_data_proc dataCallback) {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1; // Моно для процедурных звуков
    config.sampleRate = 44100;
    config.dataCallback = dataCallback;
    config.periodSizeInMilliseconds = 10; // Низкая latency

    return ma_device_init(NULL, &config, device);
}
```

### Комбинирование процедурных звуков

Для сложных звуков (вода + ветер) используйте микширование нескольких генераторов:

```cpp
struct ProceduralAudioMixer {
    ma_noise water_noise;
    ma_waveform wind_wave;
    ma_biquad_filter lowpass_filter;

    float water_strength = 0.5f;
    float wind_strength = 0.3f;
};

void mixed_procedural_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ProceduralAudioMixer* mixer = static_cast<ProceduralAudioMixer*>(pDevice->pUserData);

    float* output = static_cast<float*>(pOutput);

    // Генерируем отдельные звуки
    ma_noise_read_pcm_frames(&mixer->water_noise, output, frameCount, NULL);
    ma_waveform_read_pcm_frames(&mixer->wind_wave, output, frameCount, NULL);

    // Применяем фильтр и микширование
    for (ma_uint32 i = 0; i < frameCount; ++i) {
        // Смешиваем с разной силой
        output[i] = output[i] * mixer->water_strength + output[i] * mixer->wind_strength;

        // Применяем низкочастотный фильтр
        ma_biquad_process_pcm_frames(&mixer->lowpass_filter, &output[i], 1, ma_format_f32, 1);
    }
}
```

## 3. Интеграция с ECS (flecs)

Управление аудио через компоненты flecs для согласованности с архитектурой ProjectV.

### Компоненты аудио

```cpp
// Компонент для источников звука
struct AudioSource {
    ma_sound* sound = nullptr;
    float base_volume = 1.0f;
    float max_distance = 100.0f;
    bool spatial = true;
    bool looping = false;
    bool playing = false;
};

// Компонент для слушателя (камеры)
struct AudioListener {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Глобальный engine для всех слушателей
    static ma_engine* global_engine;
};

// Инициализация компонентов
ECS_COMPONENT(flecs_world, AudioSource);
ECS_COMPONENT(flecs_world, AudioListener);
ECS_COMPONENT(flecs_world, Position3D);
ECS_COMPONENT(flecs_world, Velocity3D); // Для доплер-эффекта
```

### Система spatial audio

```cpp
// Система обновления позиций и громкости звуков
ECS_SYSTEM(flecs_world, SpatialAudioSystem, EcsOnUpdate,
           AudioSource, Position3D);

void SpatialAudioSystem(flecs::iter& it) {
    auto sources = it.field<AudioSource>(1);
    auto positions = it.field<Position3D>(2);

    // Находим активного слушателя (обычно камера)
    flecs::entity listener = it.world().lookup("camera_listener");
    if (!listener.is_alive()) return;

    const AudioListener* listener_comp = listener.get<AudioListener>();
    const Position3D* listener_pos = listener.get<Position3D>();

    if (!listener_comp || !listener_pos) return;

    // Устанавливаем позицию слушателя в engine
    ma_engine_listener_set_position(
        AudioListener::global_engine,
        0, // listener index
        listener_pos->x, listener_pos->y, listener_pos->z
    );

    // Обновляем ориентацию слушателя
    ma_engine_listener_set_direction(
        AudioListener::global_engine,
        0,
        listener_comp->forward.x, listener_comp->forward.y, listener_comp->forward.z
    );

    for (auto i : it) {
        if (!sources[i].sound || !sources[i].spatial) continue;

        // Устанавливаем позицию источника звука
        ma_sound_set_position(
            sources[i].sound,
            positions[i].x, positions[i].y, positions[i].z
        );

        // Расчёт дистанции и управление воспроизведением
        float distance = glm::distance(
            glm::vec3(positions[i].x, positions[i].y, positions[i].z),
            glm::vec3(listener_pos->x, listener_pos->y, listener_pos->z)
        );

        if (distance > sources[i].max_distance) {
            // Останавливаем звук, если он слишком далеко
            if (sources[i].playing) {
                ma_sound_stop(sources[i].sound);
                sources[i].playing = false;
            }
        } else {
            // Запускаем или продолжаем воспроизведение
            if (!sources[i].playing) {
                ma_sound_start(sources[i].sound);
                sources[i].playing = true;
            }

            // Настраиваем громкость в зависимости от расстояния
            float distance_factor = 1.0f - (distance / sources[i].max_distance);
            float volume = sources[i].base_volume * distance_factor;
            ma_sound_set_volume(sources[i].sound, volume);
        }
    }
}
```

### Система управления звуками

```cpp
// Система для управления жизненным циклом звуков
ECS_SYSTEM(flecs_world, AudioLifecycleSystem, EcsOnUpdate,
           AudioSource);

void AudioLifecycleSystem(flecs::iter& it) {
    auto sources = it.field<AudioSource>(1);

    for (auto i : it) {
        if (!sources[i].sound) continue;

        // Проверяем, закончилось ли воспроизведение
        if (ma_sound_at_end(sources[i].sound)) {
            if (sources[i].looping) {
                // Перезапускаем для зацикленных звуков
                ma_sound_seek_to_pcm_frame(sources[i].sound, 0);
                ma_sound_start(sources[i].sound);
            } else {
                // Останавливаем и помечаем как неиграющий
                ma_sound_stop(sources[i].sound);
                sources[i].playing = false;
            }
        }
    }
}
```

## 4. Оптимизации производительности

### Resource Manager для воксельных звуков

Используйте `ma_resource_manager` для кэширования часто используемых звуков (разрушение блоков, шаги).

```cpp
ma_resource_manager_config resource_mgr_config = ma_resource_manager_config_init();
resource_mgr_config.decodedFormat = ma_format_f32;
resource_mgr_config.decodedChannels = 2;
resource_mgr_config.decodedSampleRate = 48000;

ma_resource_manager resource_mgr;
ma_resource_manager_init(&resource_mgr_config, &resource_mgr);

// Предзагрузка часто используемых звуков
const char* common_sounds[] = {
    "sounds/stone_break.wav",
    "sounds/dirt_break.wav",
    "sounds/wood_break.wav",
    "sounds/step_stone.wav",
    "sounds/step_dirt.wav"
};

for (const char* path : common_sounds) {
    ma_resource_manager_data_source_callbacks callbacks = {};
    ma_resource_manager_register_file(&resource_mgr, path, 0, &callbacks, NULL);
}

// Использование в engine
ma_engine_config engine_config = ma_engine_config_init();
engine_config.pResourceManager = &resource_mgr;

ma_engine engine;
ma_engine_init(&engine_config, &engine);
```

## 5. Пул звуков для разрушения блоков

При разрушении десятков блоков одновременно не создавайте новые `ma_sound` каждый раз.

```cpp
class SoundPool {
public:
    SoundPool(ma_engine* engine, const char* file_path, size_t pool_size = 32)
        : m_engine(engine), m_pool_size(pool_size) {

        m_sounds.resize(pool_size);
        m_available.resize(pool_size, true);

        for (size_t i = 0; i < pool_size; ++i) {
            ma_sound_init_from_file(engine, file_path, 0, NULL, NULL, &m_sounds[i]);
            ma_sound_set_volume(&m_sounds[i], 1.0f);
        }
    }

    ~SoundPool() {
        for (auto& sound : m_sounds) {
            ma_sound_uninit(&sound);
        }
    }

    ma_sound* acquire(glm::vec3 position) {
        for (size_t i = 0; i < m_pool_size; ++i) {
            if (m_available[i]) {
                m_available[i] = false;

                ma_sound_set_position(&m_sounds[i], position.x, position.y, position.z);
                ma_sound_seek_to_pcm_frame(&m_sounds[i], 0);
                ma_sound_start(&m_sounds[i]);

                return &m_sounds[i];
            }
        }
        return nullptr; // Все звуки заняты
    }

    void release(ma_sound* sound) {
        for (size_t i = 0; i < m_pool_size; ++i) {
            if (&m_sounds[i] == sound) {
                ma_sound_stop(sound);
                m_available[i] = true;
                break;
            }
        }
    }

private:
    ma_engine* m_engine;
    size_t m_pool_size;
    std::vector<ma_sound> m_sounds;
    std::vector<bool> m_available;
};

// Использование
SoundPool stone_break_pool(&engine, "sounds/stone_break.wav", 64);

// При разрушении блока
void on_block_break(glm::vec3 block_position, BlockType type) {
    ma_sound* sound = stone_break_pool.acquire(block_position);
    if (sound) {
        // Автоматически освободится после воспроизведения
        // (нужно отслеживать окончание воспроизведения)
    }
}
```

## 6. Дистанционный culling звуков

Не обрабатывайте звуки, которые находятся дальше определённого расстояния от слушателя.

```cpp
struct AudioCullingSystem {
    float culling_distance = 200.0f; // Максимальная дистанция обработки

    void update(flecs::iter& it, AudioSource* sources, Position3D* positions) {
        glm::vec3 listener_pos = get_listener_position();

        for (auto i : it) {
            if (!sources[i].sound) continue;

            float distance = glm::distance(
                glm::vec3(positions[i].x, positions[i].y, positions[i].z),
                listener_pos
            );

            if (distance > culling_distance) {
                // Полностью отключаем звук за пределами culling distance
                if (ma_sound_is_playing(sources[i].sound)) {
                    ma_sound_stop(sources[i].sound);
                }
                // Можно также выгрузить ресурс, если он не нужен
            }
        }
    }
};
```

## 7. Материалы блоков и акустика

Разные материалы блоков по-разному влияют на звук:

```cpp
enum class BlockMaterial {
    Stone,    // Камень: сильное поглощение, короткая реверберация
    Dirt,     // Земля: хорошее поглощение низких частот
    Wood,     // Дерево: среднее поглощение, тёплый звук
    Metal,    // Металл: слабое поглощение, длинная реверберация
    Air,      // Воздух: минимальное поглощение
    Water     // Вода: сильное поглощение, изменённая скорость звука
};

struct MaterialAcoustics {
    float absorption_factor;  // 0.0 - полное поглощение, 1.0 - нет поглощения
    float low_freq_absorption; // Поглощение низких частот
    float reverb_time;        // Время реверберации в секундах
    float sound_speed_factor; // Множитель скорости звука
};

std::unordered_map<BlockMaterial, MaterialAcoustics> material_acoustics = {
    {BlockMaterial::Stone,    {0.3f, 0.4f, 0.5f, 1.0f}},
    {BlockMaterial::Dirt,     {0.5f, 0.8f, 0.3f, 0.9f}},
    {BlockMaterial::Wood,     {0.6f, 0.6f, 0.8f, 0.8f}},
    {BlockMaterial::Metal,    {0.1f, 0.2f, 2.0f, 1.2f}},
    {BlockMaterial::Air,      {0.95f, 0.95f, 0.1f, 1.0f}},
    {BlockMaterial::Water,    {0.2f, 0.9f, 0.2f, 4.3f}} // Вода: скорость звука в 4.3 раза выше
};

float calculate_material_absorption(VoxelWorld* world, glm::vec3 start, glm::vec3 end) {
    std::vector<BlockMaterial> materials = raycast_materials(world, start, end);

    float total_absorption = 1.0f;
    for (const auto& material : materials) {
        const auto& acoustics = material_acoustics[material];
        total_absorption *= acoustics.absorption_factor;
    }

    return total_absorption;
}
```

## 8. Интеграция с системой чанков ProjectV

Аудио в ProjectV должно работать с системой чанков (16×16×16 блоков) для эффективного управления звуками в больших
воксельных мирах.

### Аудио для чанков

Каждый чанк может иметь свои звуковые характеристики и источники:

```cpp
struct ChunkAudio {
    // Процедурные звуки чанка (ветер, вода)
    ma_sound* ambient_sound = nullptr;

    // Источники звука внутри чанка
    std::vector<ma_sound*> sound_sources;

    // Акустические характеристики чанка
    float reverb_factor = 1.0f;
    float absorption_factor = 1.0f;

    // Флаги активности
    bool is_active = false;
    bool needs_update = false;
};

class ChunkAudioManager {
public:
    ChunkAudioManager(ma_engine* engine, size_t max_chunks = 1024)
        : m_engine(engine), m_max_chunks(max_chunks) {
        m_chunk_audio.resize(max_chunks);
    }

    // Активация чанка (когда он становится видимым)
    void activate_chunk(ChunkID chunk_id, glm::vec3 world_position) {
        ChunkAudio& audio = m_chunk_audio[chunk_id];

        if (!audio.ambient_sound) {
            // Создаём процедурный звук для чанка
            audio.ambient_sound = create_chunk_ambient_sound(chunk_id);
        }

        // Устанавливаем позицию
        if (audio.ambient_sound) {
            ma_sound_set_position(audio.ambient_sound,
                world_position.x, world_position.y, world_position.z);
        }

        audio.is_active = true;
    }

    // Деактивация чанка (когда он выходит из видимости)
    void deactivate_chunk(ChunkID chunk_id) {
        ChunkAudio& audio = m_chunk_audio[chunk_id];

        if (audio.ambient_sound) {
            ma_sound_stop(audio.ambient_sound);
        }

        // Останавливаем все источники звука в чанке
        for (ma_sound* sound : audio.sound_sources) {
            ma_sound_stop(sound);
        }

        audio.is_active = false;
    }

    // Добавление звука в чанк (разрушение блока, шаги)
    ma_sound* add_chunk_sound(ChunkID chunk_id, const char* sound_file,
                              glm::vec3 position, float volume = 1.0f) {
        ChunkAudio& audio = m_chunk_audio[chunk_id];

        ma_sound* sound = nullptr;
        ma_sound_init_from_file(m_engine, sound_file, 0, NULL, NULL, &sound);

        if (sound) {
            ma_sound_set_position(sound, position.x, position.y, position.z);
            ma_sound_set_volume(sound, volume);
            ma_sound_start(sound);

            audio.sound_sources.push_back(sound);
        }

        return sound;
    }

private:
    ma_engine* m_engine;
    size_t m_max_chunks;
    std::vector<ChunkAudio> m_chunk_audio;

    ma_sound* create_chunk_ambient_sound(ChunkID chunk_id) {
        // Создание процедурного ambient звука для чанка
        // Например, ветер, вода, биом-специфичные звуки
        // Реализация зависит от типа чанка (биом, высота, содержимое)
        return nullptr;
    }
};
```

### Streaming аудио для больших миров

Для поддержки бесконечных миров используйте streaming подход:

1. **Приоритетные чанки**: Звуки в ближайших чанках имеют высший приоритет
2. **Фоновые чанки**: Отдалённые чанки используют упрощённые звуки
3. **Асинхронная загрузка**: Загрузка звуков в фоновом потоке

```cpp
struct AudioStreamingSystem {
    // Приоритетные чанки (расстояние < 100 блоков)
    std::vector<ChunkID> high_priority_chunks;

    // Средние приоритеты (расстояние 100-500 блоков)
    std::vector<ChunkID> medium_priority_chunks;

    // Низкие приоритеты (расстояние > 500 блоков)
    std::vector<ChunkID> low_priority_chunks;

    // Обновление приоритетов на основе позиции слушателя
    void update_priorities(glm::vec3 listener_pos, const std::vector<ChunkID>& visible_chunks) {
        high_priority_chunks.clear();
        medium_priority_chunks.clear();
        low_priority_chunks.clear();

        for (ChunkID chunk_id : visible_chunks) {
            glm::vec3 chunk_center = get_chunk_center(chunk_id);
            float distance = glm::distance(listener_pos, chunk_center);

            if (distance < 100.0f) {
                high_priority_chunks.push_back(chunk_id);
            } else if (distance < 500.0f) {
                medium_priority_chunks.push_back(chunk_id);
            } else {
                low_priority_chunks.push_back(chunk_id);
            }
        }
    }
};
```

## Практические рекомендации для ProjectV

### Настройки для разных сценариев

1. **Фоновые звуки (музыка, амбиент)**:
  - Используйте `ma_sound` с включённым стримингом (`MA_SOUND_FLAG_STREAM`)
  - Задавайте низкий приоритет в resource manager
  - Отключайте spatial audio (`MA_SOUND_FLAG_NO_SPATIALIZATION`)

2. **Звуки разрушения/строительства**:
  - Используйте пулы звуков
  - Включайте spatial audio
  - Настраивайте короткую длительность и быстрое затухание

3. **Звуки UI**:
  - Используйте `ma_engine_play_sound` для простоты
  - Отключайте spatial audio
  - Минимизируйте latency (маленький period size)

4. **3D позиционированные звуки (монстры, NPC)**:
  - Используйте `ma_sound` с spatial audio
  - Настраивайте доплер-эффект через `ma_sound_set_doppler_factor`
  - Реализуйте occlusion через raycasting

### Интеграция с другими системами ProjectV

1. **Профилирование с Tracy**:
   ```cpp
   // В data callback
   ZoneScopedN("AudioCallback");
   // ...
   ```

2. **Синхронизация с кадрами рендеринга**:
  - Обновляйте позиции звуков в начале кадра
  - Применяйте аудиоэффекты до рендеринга
  - Синхронизируйте с VSync для минимизации latency

3. **Конфигурация через параметры проекта**:
  - Выносите настройки audio в конфигурационные файлы
  - Позволяйте пользователям настраивать громкость отдельных категорий
  - Реализуйте систему сохранения/загрузки настроек аудио

4. **Интеграция с Vulkan GPU-Driven рендерингом**:
  - Синхронизируйте аудио обновления с графическими кадрами
  - Используйте общие данные позиционирования (glm векторы)
  - Реализуйте shared memory для передачи позиций камеры и объектов
  - Настраивайте audio LOD на основе графического LOD

5. **Интеграция с glm математикой**:
   ```cpp
   // Использование glm для аудио расчётов
   glm::vec3 audio_source_pos = glm::vec3(10.0f, 5.0f, 2.0f);
   glm::vec3 listener_pos = get_camera_position();
   float distance = glm::distance(audio_source_pos, listener_pos);
   glm::vec3 direction = glm::normalize(audio_source_pos - listener_pos);

   // Доплер-эффект с glm
   glm::vec3 listener_velocity = get_camera_velocity();
   glm::vec3 source_velocity = get_source_velocity();
   float relative_velocity = glm::dot(listener_velocity - source_velocity, direction);
   float doppler_factor = calculate_doppler_factor(relative_velocity);
   ```

6. **Интеграция с системой чанков**:
  - Используйте `ChunkAudioManager` для управления звуками чанков
  - Реализуйте audio streaming для бесконечных миров
  - Связывайте акустические характеристики с материалами чанков

7. **Специализированные воксельные звуки**:
   ```cpp
   // Звуки разрушения блоков по материалам
   SoundPool stone_break_pool(&engine, "sounds/stone_break.wav", 32);
   SoundPool dirt_break_pool(&engine, "sounds/dirt_break.wav", 32);
   SoundPool wood_break_pool(&engine, "sounds/wood_break.wav", 32);

   // Шаги по разным материалам
   SoundPool stone_step_pool(&engine, "sounds/step_stone.wav", 16);
   SoundPool grass_step_pool(&engine, "sounds/step_grass.wav", 16);

   // Процедурные звуки для воксельных элементов
   // Ветер через листву деревьев
   // Водопады и течение рек
   // Лава: пузырьки, шипение
   ```

### Отладка и диагностика

1. **Визуализация аудиосистемы**:
  - Отображайте позиции источников звука в debug overlay
  - Показывайте активные звуки и их параметры
  - Визуализируйте occlusion rays

2. **Статистика производительности**:
   ```cpp
   struct AudioStats {
       size_t active_sounds = 0;
       size_t sound_pool_usage = 0;
       float cpu_usage_percent = 0.0f;
       float latency_ms = 0.0f;
       size_t active_chunks = 0;
       size_t streaming_queue_size = 0;
   };
   ```

3. **Тестирование в разных условиях**:
  - Тестируйте с разным количеством одновременных звуков
  - Проверяйте производительность при разрушении множества блоков
  - Тестируйте на разных аудиоустройствах (USB, встроенная карта, Bluetooth)

---

## Примеры кода в ProjectV

См. также:

- [miniaudio_lowlevel.cpp](../examples/miniaudio_lowlevel.cpp) — низкоуровневый API
- [miniaudio_playback.cpp](../examples/miniaudio_playback.cpp) — базовое воспроизведение
- [miniaudio_sdl.cpp](../examples/miniaudio_sdl.cpp) — интеграция с SDL
- [miniaudio_streaming.cpp](../examples/miniaudio_streaming.cpp) — стриминг больших файлов

---

**← [Назад к основной документации miniaudio](README.md)**
