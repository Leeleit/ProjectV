# Паттерны ProjectV

**🟡 Уровень 2: Средний**

Sound pools, chunk audio, voxel spatial audio, ECS компоненты.

---

## Sound Pool

Пул звуков для частого воспроизведения одного и того же звука (выстрелы, шаги).

### Проблема

При частом вызове `ma_engine_play_sound` для одного файла происходит:

1. Загрузка файла с диска
2. Декодирование
3. Воспроизведение
4. Удаление

Это неэффективно для звуков, которые играют часто.

### Решение: Предзагруженный пул

```cpp
#include <miniaudio.h>
#include <vector>
#include <queue>

class SoundPool {
public:
    SoundPool(ma_engine* pEngine, const char* filePath, size_t poolSize = 8)
        : m_pEngine(pEngine), m_filePath(filePath) {

        m_sounds.resize(poolSize);
        m_available.reserve(poolSize);

        for (size_t i = 0; i < poolSize; i++) {
            ma_result result = ma_sound_init_from_file(pEngine, filePath,
                MA_SOUND_FLAG_DECODE, NULL, NULL, &m_sounds[i]);

            if (result == MA_SUCCESS) {
                m_available.push_back(i);
            }
        }
    }

    ~SoundPool() {
        for (auto& sound : m_sounds) {
            ma_sound_uninit(&sound);
        }
    }

    bool play(float volume = 1.0f) {
        if (m_available.empty()) {
            // Все звуки заняты, перезапускаем первый
            ma_sound_seek_to_pcm_frame(&m_sounds[0], 0);
            ma_sound_set_volume(&m_sounds[0], volume);
            ma_sound_start(&m_sounds[0]);
            return true;
        }

        size_t index = m_available.back();
        m_available.pop_back();

        ma_sound_seek_to_pcm_frame(&m_sounds[index], 0);
        ma_sound_set_volume(&m_sounds[index], volume);
        ma_sound_start(&m_sounds[index]);

        // Отслеживание завершения
        m_playing.push_back(index);

        return true;
    }

    void update() {
        // Возвращение завершённых звуков в пул
        auto it = m_playing.begin();
        while (it != m_playing.end()) {
            if (!ma_sound_is_playing(&m_sounds[*it])) {
                m_available.push_back(*it);
                it = m_playing.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    ma_engine* m_pEngine;
    std::string m_filePath;
    std::vector<ma_sound> m_sounds;
    std::vector<size_t> m_available;
    std::vector<size_t> m_playing;
};
```

### Использование

```cpp
SoundPool gunshotPool(&engine, "sounds/gunshot.wav", 16);

// В игровом цикле
void onPlayerShoot() {
    gunshotPool.play(0.8f);
}

// Каждый кадр
gunshotPool.update();
```

---

## Chunk Audio

Короткие звуки, полностью загруженные в память.

### Sound Chunk

```cpp
class SoundChunk {
public:
    SoundChunk() = default;

    bool load(ma_engine* pEngine, const char* filePath) {
        m_pEngine = pEngine;

        // Загрузка и декодирование в память
        ma_decoder decoder;
        ma_result result = ma_decoder_init_file(filePath, NULL, &decoder);
        if (result != MA_SUCCESS) return false;

        // Получение длины
        ma_uint64 length;
        ma_decoder_get_length_in_pcm_frames(&decoder, &length);

        // Выделение буфера
        size_t bytesPerFrame = ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        m_buffer.resize(static_cast<size_t>(length * bytesPerFrame));

        // Декодирование
        ma_uint64 framesRead;
        ma_decoder_read_pcm_frames(&decoder, m_buffer.data(), length, &framesRead);

        m_format = decoder.outputFormat;
        m_channels = decoder.outputChannels;
        m_sampleRate = decoder.outputSampleRate;

        ma_decoder_uninit(&decoder);
        return true;
    }

    bool play(float volume = 1.0f) {
        // Создание звука из буфера памяти
        // Примечание: требуется custom data source или ma_decoder_init_memory
        ma_decoder_config config = ma_decoder_config_init(m_format, m_channels, m_sampleRate);

        ma_decoder* pDecoder = new ma_decoder();
        ma_decoder_init_memory(m_buffer.data(), m_buffer.size(), &config, pDecoder);

        ma_sound sound;
        ma_sound_init_from_data_source(m_pEngine, pDecoder, 0, NULL, &sound);
        ma_sound_set_volume(&sound, volume);
        ma_sound_start(&sound);

        // Управление жизненным циклом требует additional handling
        return true;
    }

private:
    ma_engine* m_pEngine = nullptr;
    std::vector<uint8_t> m_buffer;
    ma_format m_format;
    ma_uint32 m_channels;
    ma_uint32 m_sampleRate;
};
```

### Простой вариант: ma_sound с флагом DECODE

```cpp
// Флаг MA_SOUND_FLAG_DECODE загружает весь файл в память
ma_sound sound;
ma_sound_init_from_file(&engine, "explosion.wav", MA_SOUND_FLAG_DECODE, NULL, NULL, &sound);
ma_sound_start(&sound);
```

---

## Voxel Spatial Audio

Адаптация spatial audio для воксельного мира.

### Особенности воксельного мира

1. **Огромное количество источников** — блоки, мобы, частицы
2. **Динамический мир** — звуки появляются и исчезают
3. **Затухание через блоки** — звук глушится стенами

### Distance-based culling

```cpp
class VoxelAudioManager {
public:
    static constexpr float MAX_AUDIBLE_DISTANCE = 64.0f;

    void playBlockSound(const char* soundPath, const glm::vec3& blockPos, float volume = 1.0f) {
        glm::vec3 listenerPos = getListenerPosition();
        float distance = glm::length(blockPos - listenerPos);

        if (distance > MAX_AUDIBLE_DISTANCE) {
            return;  // Слишком далеко
        }

        // Расчёт громкости с учётом расстояния
        float distanceAttenuation = 1.0f - (distance / MAX_AUDIBLE_DISTANCE);
        float finalVolume = volume * distanceAttenuation;

        // Воспроизведение
        ma_engine_play_sound(&m_engine, soundPath, NULL);

        // Для 3D звука:
        // ma_sound sound;
        // ma_sound_init_from_file(&m_engine, soundPath, 0, NULL, NULL, &sound);
        // ma_sound_set_position(&sound, blockPos.x, blockPos.y, blockPos.z);
        // ma_sound_set_volume(&sound, finalVolume);
        // ma_sound_start(&sound);
    }

private:
    ma_engine m_engine;
    glm::vec3 getListenerPosition();
};
```

### Пул для блочных звуков

```cpp
class BlockSoundPool {
public:
    void init(ma_engine* pEngine) {
        m_pEngine = pEngine;

        // Предзагрузка частых звуков
        m_digPool.init(pEngine, "sounds/dig.wav", 8);
        m_placePool.init(pEngine, "sounds/place.wav", 8);
        m_stepPool.init(pEngine, "sounds/step.wav", 16);
        m_breakPool.init(pEngine, "sounds/break.wav", 8);
    }

    void playDig(const glm::vec3& pos) {
        if (isAudible(pos)) {
            m_digPool.play();
        }
    }

    void playPlace(const glm::vec3& pos) {
        if (isAudible(pos)) {
            m_placePool.play();
        }
    }

    void playStep(const glm::vec3& pos) {
        if (isAudible(pos)) {
            m_stepPool.play();
        }
    }

    void playBreak(const glm::vec3& pos) {
        if (isAudible(pos)) {
            m_breakPool.play();
        }
    }

    void update() {
        m_digPool.update();
        m_placePool.update();
        m_stepPool.update();
        m_breakPool.update();
    }

private:
    ma_engine* m_pEngine;
    SoundPool m_digPool;
    SoundPool m_placePool;
    SoundPool m_stepPool;
    SoundPool m_breakPool;

    bool isAudible(const glm::vec3& pos) {
        return glm::length(pos - m_listenerPos) < 64.0f;
    }

    glm::vec3 m_listenerPos;
};
```

### Occlusion (заглушение через стены)

```cpp
float calculateOcclusion(const glm::vec3& soundPos, const glm::vec3& listenerPos, VoxelWorld& world) {
    // Raycast от listener к звуку
    glm::vec3 dir = glm::normalize(soundPos - listenerPos);
    float maxDistance = glm::length(soundPos - listenerPos);

    int blocksBetween = 0;

    // Простая проверка: считаем блоки на линии
    glm::vec3 current = listenerPos;
    float step = 1.0f;

    while (glm::length(current - listenerPos) < maxDistance) {
        if (world.isSolid(current)) {
            blocksBetween++;
        }
        current += dir * step;
    }

    // Каждый блок глушит на 10%
    return glm::max(0.0f, 1.0f - blocksBetween * 0.1f);
}

void playOccludedSound(ma_engine* pEngine, const char* path,
                       const glm::vec3& pos, VoxelWorld& world) {
    glm::vec3 listenerPos = getListenerPosition();
    float occlusion = calculateOcclusion(pos, listenerPos, world);

    ma_sound sound;
    ma_sound_init_from_file(pEngine, path, 0, NULL, NULL, &sound);
    ma_sound_set_position(&sound, pos.x, pos.y, pos.z);
    ma_sound_set_volume(&sound, occlusion);
    ma_sound_start(&sound);
}
```

---

## ECS Audio Components

### Полный набор компонентов

```cpp
// Звуковой emitter (источник звука)
struct AudioEmitter {
    std::string soundPath;
    float volume = 1.0f;
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    bool loop = false;
    bool is3D = true;
    bool autoPlay = true;
};

// Воспроизводимый звук
struct AudioInstance {
    ma_sound handle;
    bool isValid = false;
    bool is3D = false;
};

// Listener (слушатель)
struct AudioListener {
    ma_uint32 index = 0;
};

// Группа звуков
struct AudioGroup {
    ma_sound_group handle;
};

// Теги
struct AutoDestroy {};  // Автоудаление после завершения
struct Music {};        // Музыка
struct SFX {};          // Звуковые эффекты
struct Ambient {};      // Фоновые звуки
```

### Система инициализации звуков

```cpp
void initAudioSystem(flecs::world& ecs, ma_engine* pEngine) {
    // Система инициализации emitters
    ecs.system<AudioEmitter, AudioInstance>("InitAudioEmitters")
        .kind(flecs::OnAdd)
        .each([pEngine](flecs::entity e, AudioEmitter& emitter, AudioInstance& instance) {
            ma_result result = ma_sound_init_from_file(pEngine, emitter.soundPath.c_str(),
                emitter.loop ? MA_SOUND_FLAG_DECODE : 0, NULL, NULL, &instance.handle);

            if (result == MA_SUCCESS) {
                instance.isValid = true;
                instance.is3D = emitter.is3D;

                if (emitter.is3D) {
                    ma_sound_set_spatialization_enabled(&instance.handle, true);
                    ma_sound_set_min_distance(&instance.handle, emitter.minDistance);
                    ma_sound_set_max_distance(&instance.handle, emitter.maxDistance);
                }

                ma_sound_set_volume(&instance.handle, emitter.volume);
                ma_sound_set_looping(&instance.handle, emitter.loop);

                if (emitter.autoPlay) {
                    ma_sound_start(&instance.handle);
                }
            }
        });

    // Система обновления позиций
    ecs.system<AudioInstance, const Position>("UpdateAudioPositions")
        .kind(flecs::OnUpdate)
        .each([](AudioInstance& instance, const Position& pos) {
            if (instance.isValid && instance.is3D) {
                ma_sound_set_position(&instance.handle, pos.x, pos.y, pos.z);
            }
        });

    // Система очистки завершённых звуков
    ecs.system<AudioInstance, AutoDestroy>("CleanupFinishedAudio")
        .kind(flecs::PreStore)
        .each([](flecs::entity e, AudioInstance& instance, AutoDestroy) {
            if (instance.isValid && !ma_sound_is_playing(&instance.handle)) {
                ma_sound_uninit(&instance.handle);
                instance.isValid = false;
                e.destruct();
            }
        });
}
```

### Фабрики звуков

```cpp
// Создание одноразового 3D звука
flecs::entity createOneShot3D(flecs::world& ecs, const char* path,
                              const glm::vec3& pos, float volume = 1.0f) {
    auto e = ecs.entity()
        .set<AudioEmitter>({
            .soundPath = path,
            .volume = volume,
            .is3D = true,
            .autoPlay = true
        })
        .set<AudioInstance>({})
        .set<Position>({ pos.x, pos.y, pos.z })
        .add<AutoDestroy>()
        .add<SFX>();

    return e;
}

// Создание музыки
flecs::entity createMusic(flecs::world& ecs, const char* path, float volume = 0.5f) {
    auto e = ecs.entity()
        .set<AudioEmitter>({
            .soundPath = path,
            .volume = volume,
            .is3D = false,
            .loop = true,
            .autoPlay = true
        })
        .set<AudioInstance>({})
        .add<Music>();

    return e;
}

// Создание loop-ambient
flecs::entity createAmbient(flecs::world& ecs, const char* path,
                            const glm::vec3& pos, float radius = 20.0f) {
    auto e = ecs.entity()
        .set<AudioEmitter>({
            .soundPath = path,
            .volume = 0.3f,
            .minDistance = 1.0f,
            .maxDistance = radius,
            .loop = true,
            .is3D = true,
            .autoPlay = true
        })
        .set<AudioInstance>({})
        .set<Position>({ pos.x, pos.y, pos.z })
        .add<Ambient>();

    return e;
}
```

---

## Ambient Soundscape

Система фоновых звуков для атмосферы.

```cpp
class AmbientSoundscape {
public:
    void init(ma_engine* pEngine) {
        m_pEngine = pEngine;
    }

    void addAmbientZone(const std::string& name, const glm::vec3& center,
                        float radius, const std::string& soundPath) {
        Zone zone;
        zone.name = name;
        zone.center = center;
        zone.radius = radius;
        zone.soundPath = soundPath;
        zone.isActive = false;

        m_zones.push_back(zone);
    }

    void update(const glm::vec3& listenerPos) {
        for (auto& zone : m_zones) {
            float distance = glm::length(listenerPos - zone.center);
            bool shouldBeActive = distance < zone.radius;

            if (shouldBeActive && !zone.isActive) {
                // Вход в зону
                ma_sound_init_from_file(m_pEngine, zone.soundPath.c_str(),
                    MA_SOUND_FLAG_DECODE, NULL, NULL, &zone.sound);
                ma_sound_set_looping(&zone.sound, true);
                ma_sound_set_volume(&zone.sound, 0.0f);
                ma_sound_start(&zone.sound);
                zone.isActive = true;
            } else if (!shouldBeActive && zone.isActive) {
                // Выход из зоны
                ma_sound_stop(&zone.sound);
                ma_sound_uninit(&zone.sound);
                zone.isActive = false;
            }

            if (zone.isActive) {
                // Плавное изменение громкости
                float t = 1.0f - (distance / zone.radius);
                float targetVolume = t * 0.5f;  // Макс 0.5
                ma_sound_set_volume(&zone.sound, targetVolume);
            }
        }
    }

private:
    struct Zone {
        std::string name;
        glm::vec3 center;
        float radius;
        std::string soundPath;
        ma_sound sound;
        bool isActive;
    };

    ma_engine* m_pEngine;
    std::vector<Zone> m_zones;
};
```

---

## Audio Events

Система событий для развязки игрового кода и аудио.

```cpp
// Типы событий
enum class AudioEventType {
    PlaySound,
    StopSound,
    SetVolume,
    SetPosition
};

struct AudioEvent {
    AudioEventType type;
    std::string soundId;
    glm::vec3 position;
    float value;
};

// Очередь событий
class AudioEventQueue {
public:
    void push(const AudioEvent& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push(event);
    }

    bool pop(AudioEvent& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_events.empty()) return false;
        event = m_events.front();
        m_events.pop();
        return true;
    }

private:
    std::queue<AudioEvent> m_events;
    std::mutex m_mutex;
};

// Обработчик событий
void processAudioEvents(AudioEventQueue& queue, AudioSystem& audio) {
    AudioEvent event;
    while (queue.pop(event)) {
        switch (event.type) {
            case AudioEventType::PlaySound:
                audio.playSound3D(event.soundId.c_str(), event.position, event.value);
                break;
            case AudioEventType::SetVolume:
                // ...
                break;
            // ...
        }
    }
}

// Использование из игрового кода
void onExplosion(const glm::vec3& pos) {
    g_audioQueue.push({
        .type = AudioEventType::PlaySound,
        .soundId = "explosion",
        .position = pos,
        .value = 1.0f
    });
}
