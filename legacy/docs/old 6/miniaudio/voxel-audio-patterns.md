# Spatial Audio (3D позиционирование звуков)

**🟡 Уровень 2: Средний**

Этот документ описывает универсальные принципы реализации spatial audio (3D позиционирования звуков) с использованием
miniaudio. Информация представлена в архитектурно-независимом виде и может быть применена в различных типах игр и
приложений, требующих пространственного звука.

## Содержание

1. [Основные концепции](#1-основные-концепции)
  - [Позиционирование источников звука](#11-позиционирование-источников-звука)
  - [Слушатель (Listener)](#12-слушатель-listener)
  - [Расстояние и затухание](#13-расстояние-и-затухание)

2. [Продвинутые техники](#2-продвинутые-техники)
  - [Окклюзия и обструкция](#21-окклюзия-и-обструкция)
  - [Реверберация окружения](#22-реверберация-окружения)
  - [Доплер-эффект](#23-доплер-эффект)
  - [HRTF (Head-Related Transfer Function)](#24-hrtf-head-related-transfer-function)

3. [Архитектурные паттерны](#3-архитектурные-паттерны)
  - [Система управления звуками](#31-система-управления-звуками)
  - [Дистанционный culling](#32-дистанционный-culling)
  - [Пул звуков для оптимизации](#33-пул-звуков-для-оптимизации)

4. [Интеграция с игровыми движками](#4-интеграция-с-игровыми-движками)
  - [Общие принципы интеграции](#41-общие-принципы-интеграции)
  - [Примеры для различных архитектур](#42-примеры-для-различных-архитектур)

5. [Оптимизация производительности](#5-оптимизация-производительности)
  - [Управление количеством звуков](#51-управление-количеством-звуков)
  - [LOD (Level of Detail) для звуков](#52-lod-level-of-detail-для-звуков)
  - [Асинхронная обработка](#53-асинхронная-обработка)

---

## 1. Основные концепции

### 1.1 Позиционирование источников звука

В miniaudio позиционирование звуков реализуется через функции `ma_sound_set_position`, `ma_sound_set_direction` и
`ma_sound_set_velocity`.

```cpp
// Базовый пример позиционирования звука
void position_sound_3d(ma_sound* sound, float x, float y, float z) {
    ma_sound_set_position(sound, x, y, z);
}

// Установка направления звука (для направленных источников)
void orient_sound(ma_sound* sound, float dir_x, float dir_y, float dir_z) {
    ma_sound_set_direction(sound, dir_x, dir_y, dir_z);

    // Настройка конуса направленности
    ma_sound_set_cone(sound,
        45.0f * (MA_PI / 180.0f),  // Внутренний угол (радианы)
        90.0f * (MA_PI / 180.0f),  // Внешний угол (радианы)
        0.5f                       // Громкость за пределами внешнего конуса
    );
}

// Установка скорости для доплер-эффекта
void set_sound_velocity(ma_sound* sound, float vel_x, float vel_y, float vel_z) {
    ma_sound_set_velocity(sound, vel_x, vel_y, vel_z);
    ma_sound_set_doppler_factor(sound, 1.0f); // Множитель доплер-эффекта
}
```

### 1.2 Слушатель (Listener)

Слушатель представляет позицию и ориентацию "ушей" игрока или камеры.

```cpp
// Установка позиции и ориентации слушателя
void setup_listener(ma_engine* engine, int listener_index,
                   float pos_x, float pos_y, float pos_z,
                   float forward_x, float forward_y, float forward_z,
                   float up_x, float up_y, float up_z) {

    // Позиция слушателя
    ma_engine_listener_set_position(engine, listener_index, pos_x, pos_y, pos_z);

    // Направление взгляда слушателя
    ma_engine_listener_set_direction(engine, listener_index, forward_x, forward_y, forward_z);

    // Вектор "вверх" для слушателя
    ma_engine_listener_set_up(engine, listener_index, up_x, up_y, up_z);

    // Настройка скорости слушателя для доплер-эффекта
    ma_engine_listener_set_velocity(engine, listener_index, 0.0f, 0.0f, 0.0f);

    // Настройка коэффициента затухания по расстоянию
    ma_engine_listener_set_world_up(engine, listener_index, 0.0f, 1.0f, 0.0f);
}

// Обновление позиции слушателя в реальном времени
void update_listener_from_camera(ma_engine* engine, Camera* camera) {
    setup_listener(engine, 0,
                   camera->position.x, camera->position.y, camera->position.z,
                   camera->forward.x, camera->forward.y, camera->forward.z,
                   camera->up.x, camera->up.y, camera->up.z);
}
```

### 1.3 Расстояние и затухание

Реалистичное затухание звука с расстоянием — ключевой аспект spatial audio.

```cpp
// Различные модели затухания
enum class AttenuationModel {
    Linear,        // Линейное затухание
    Inverse,       // Обратное затухание (1/distance)
    InverseSquare, // Обратное квадратичное затухание (1/distance²)
    Exponential    // Экспоненциальное затухание
};

float calculate_attenuation(float distance, float max_distance,
                           AttenuationModel model, float rolloff = 1.0f) {

    if (distance >= max_distance) {
        return 0.0f;
    }

    switch (model) {
        case AttenuationModel::Linear:
            return 1.0f - (distance / max_distance);

        case AttenuationModel::Inverse:
            return 1.0f / (1.0f + rolloff * distance);

        case AttenuationModel::InverseSquare:
            return 1.0f / (1.0f + rolloff * distance * distance);

        case AttenuationModel::Exponential:
            return expf(-rolloff * distance);

        default:
            return 1.0f;
    }
}

// Автоматическое обновление громкости по расстоянию
void update_sound_attenuation(ma_sound* sound, float source_x, float source_y, float source_z,
                             float listener_x, float listener_y, float listener_z,
                             float max_distance, float base_volume = 1.0f) {

    float distance = sqrtf(
        powf(source_x - listener_x, 2) +
        powf(source_y - listener_y, 2) +
        powf(source_z - listener_z, 2)
    );

    float attenuation = calculate_attenuation(distance, max_distance,
                                             AttenuationModel::InverseSquare, 1.0f);

    float volume = base_volume * attenuation;
    ma_sound_set_volume(sound, volume);
}
```

## 2. Продвинутые техники

### 2.1 Окклюзия и обструкция

Окклюзия (полное блокирование звука) и обструкция (частичное блокирование) — важные аспекты реалистичного spatial audio.

```cpp
// Обобщённая функция расчёта окклюзии через raycasting
float calculate_occlusion_raycast(RaycastCallback raycast, void* world_data,
                                 float source_x, float source_y, float source_z,
                                 float listener_x, float listener_y, float listener_z) {

    // Выполняем raycast от источника к слушателю
    int num_hits = raycast(world_data, source_x, source_y, source_z,
                           listener_x, listener_y, listener_z);

    // Каждое препятствие уменьшает громкость
    float occlusion = powf(0.7f, static_cast<float>(num_hits));

    // Минимальная громкость даже при полной окклюзии
    return fmaxf(occlusion, 0.1f);
}

// Применение окклюзии к звуку
void apply_occlusion_to_sound(ma_sound* sound, float occlusion_factor) {
    float current_volume;
    ma_sound_get_volume(sound, &current_volume);
    ma_sound_set_volume(sound, current_volume * occlusion_factor);
}
```

### 2.2 Реверберация окружения

Динамическая реверберация в зависимости от окружения.

```cpp
// Параметры реверберации для разных материалов
struct ReverbPreset {
    float decay_time;      // Время затухания (секунды)
    float early_reflections; // Ранние отражения
    float late_reflections;  // Поздние отражения
    float wet_level;       // Уровень эффекта
    float dry_level;       // Уровень исходного звука
};

// Пресеты реверберации для различных окружений
std::unordered_map<std::string, ReverbPreset> reverb_presets = {
    {"Cave",      {3.0f, 0.8f, 0.6f, 0.7f, 0.3f}},
    {"Room",      {1.5f, 0.5f, 0.3f, 0.4f, 0.6f}},
    {"Hall",      {2.5f, 0.7f, 0.5f, 0.6f, 0.4f}},
    {"Outside",   {0.5f, 0.1f, 0.05f, 0.1f, 0.9f}},
    {"Underwater",{4.0f, 0.9f, 0.8f, 0.8f, 0.2f}}
};

// Динамическое определение реверберации на основе окружения
ReverbPreset determine_reverb_preset(RaycastCallback raycast, void* world_data,
                                    float x, float y, float z) {
    // Анализируем окружение через raycasting в нескольких направлениях
    // ...

    // Возвращаем подходящий пресет
    return reverb_presets["Room"];
}
```

### 2.3 Доплер-эффект

Реализация доплер-эффекта для движущихся источников звука.

```cpp
// Расчёт доплер-эффекта
float calculate_doppler_factor(float source_vel_x, float source_vel_y, float source_vel_z,
                              float listener_vel_x, float listener_vel_y, float listener_vel_z,
                              float speed_of_sound = 343.0f) {

    // Относительная скорость
    float relative_speed = sqrtf(
        powf(source_vel_x - listener_vel_x, 2) +
        powf(source_vel_y - listener_vel_y, 2) +
        powf(source_vel_z - listener_vel_z, 2)
    );

    // Формула доплер-эффекта
    return speed_of_sound / (speed_of_sound + relative_speed);
}

// Обновление доплер-эффекта для звука
void update_doppler_effect(ma_sound* sound,
                          float source_vel_x, float source_vel_y, float source_vel_z,
                          float listener_vel_x, float listener_vel_y, float listener_vel_z) {

    float doppler_factor = calculate_doppler_factor(
        source_vel_x, source_vel_y, source_vel_z,
        listener_vel_x, listener_vel_y, listener_vel_z
    );

    ma_sound_set_doppler_factor(sound, doppler_factor);
}
```

### 2.4 HRTF (Head-Related Transfer Function)

Поддержка HRTF для более точного позиционирования звуков в пространстве.

```cpp
// Включение HRTF в miniaudio
ma_result enable_hrtf(ma_engine* engine, const char* hrtf_path = nullptr) {
    ma_engine_config config = ma_engine_config_init();

    // Настройка spatializer'а с HRTF
    config.listenerCount = 1;
    config.channels = 2; // Стерео для HRTF

    if (hrtf_path) {
        // Загрузка кастомного HRTF набора
        // (требуется соответствующая поддержка в miniaudio)
    }

    // Переинициализация engine с новыми настройками
    ma_engine_uninit(engine);
    return ma_engine_init(&config, engine);
}
```

## 3. Архитектурные паттерны

### 3.1 Система управления звуками

Универсальная система управления spatial audio источниками.

```cpp
class SpatialAudioManager {
public:
    struct SoundInstance {
        ma_sound* sound;
        float pos_x, pos_y, pos_z;
        float max_distance;
        float base_volume;
        bool spatial;
        bool active;
    };

    SpatialAudioManager(ma_engine* engine) : m_engine(engine) {}

    // Добавление звука с позиционированием
    SoundInstance* add_sound(const char* file_path,
                            float x, float y, float z,
                            float max_dist = 100.0f,
                            bool spatial = true) {

        ma_sound* sound = new ma_sound();
        ma_result result = ma_sound_init_from_file(m_engine, file_path, 0, nullptr, nullptr, sound);

        if (result != MA_SUCCESS) {
            delete sound;
            return nullptr;
        }

        if (spatial) {
            ma_sound_set_position(sound, x, y, z);
            ma_sound_set_spatialization_enabled(sound, true);
        }

        SoundInstance instance;
        instance.sound = sound;
        instance.pos_x = x; instance.pos_y = y; instance.pos_z = z;
        instance.max_distance = max_dist;
        instance.base_volume = 1.0f;
        instance.spatial = spatial;
        instance.active = true;

        m_sounds.push_back(instance);
        return &m_sounds.back();
    }

    // Обновление всех звуков относительно слушателя
    void update(float listener_x, float listener_y, float listener_z) {
        for (auto& instance : m_sounds) {
            if (!instance.active || !instance.spatial) continue;

            // Обновление позиции
            ma_sound_set_position(instance.sound, instance.pos_x, instance.pos_y, instance.pos_z);

            // Расчёт расстояния и громкости
            float distance = sqrtf(
                powf(instance.pos_x - listener_x, 2) +
                powf(instance.pos_y - listener_y, 2) +
                powf(instance.pos_z - listener_z, 2)
            );

            if (distance <= instance.max_distance) {
                float volume = instance.base_volume * (1.0f - distance / instance.max_distance);
                ma_sound_set_volume(instance.sound, volume);

                if (!ma_sound_is_playing(instance.sound)) {
                    ma_sound_start(instance.sound);
                }
            } else {
                if (ma_sound_is_playing(instance.sound)) {
                    ma_sound_stop(instance.sound);
                }
            }
        }
    }

private:
    ma_engine* m_engine;
    std::vector<SoundInstance> m_sounds;
};
```

### 3.2 Дистанционный culling

Оптимизация производительности через отключение далёких звуков.

```cpp
// Система дистанционного culling'а
class AudioCullingSystem {
public:
    AudioCullingSystem(float culling_distance = 200.0f)
        : m_culling_distance(culling_distance) {}

    void process_sounds(std::vector<SpatialAudioManager::SoundInstance>& sounds,
                       float listener_x, float listener_y, float listener_z) {

        for (auto& instance : sounds) {
            if (!instance.spatial) continue;

            float distance = sqrtf(
                powf(instance.pos_x - listener_x, 2) +
                powf(instance.pos_y - listener_y, 2) +
                powf(instance.pos_z - listener_z, 2)
            );

            if (distance > m_culling_distance) {
                // Полное отключение звука
                if (ma_sound_is_playing(instance.sound)) {
                    ma_sound_stop(instance.sound);
                }
                instance.active = false;
            } else {
                instance.active = true;
            }
        }
    }

private:
    float m_culling_distance;
};
```

### 3.3 Пул звуков для оптимизации

Повторное использование звуков для часто воспроизводимых эффектов.

```cpp
// Универсальный пул звуков
template<size_t PoolSize = 64>
class SoundPool {
public:
    SoundPool(ma_engine* engine, const char* file_path)
        : m_engine(engine), m_file_path(file_path) {

        initialize_pool();
    }

    ma_sound* acquire(float x, float y, float z) {
        for (size_t i = 0; i < PoolSize; ++i) {
            if (m_available[i]) {
                m_available[i] = false;

                ma_sound_set_position(&m_sounds[i], x, y, z);
                ma_sound_seek_to_pcm_frame(&m_sounds[i], 0);
                ma_sound_start(&m_sounds[i]);

                return &m_sounds[i];
            }
        }
        return nullptr;
    }

    void release(ma_sound* sound) {
        for (size_t i = 0; i < PoolSize; ++i) {
            if (&m_sounds[i] == sound) {
                ma_sound_stop(sound);
                m_available[i] = true;
                break;
            }
        }
    }

private:
    void initialize_pool() {
        for (size_t i = 0; i < PoolSize; ++i) {
            ma_sound_init_from_file(m_engine, m_file_path, 0, nullptr, nullptr, &m_sounds[i]);
            m_available[i] = true;
        }
    }

    ma_engine* m_engine;
    std::string m_file_path;
    std::array<ma_sound, PoolSize> m_sounds;
    std::array<bool, PoolSize> m_available;
};
```

## 4. Интеграция с игровыми движками

### 4.1 Общие принципы интеграции

1. **Отдельный аудио модуль**: Создайте независимый модуль для управления аудио.
2. **Интерфейс для игровой логики**: Предоставьте простой API для запуска звуков.
3. **Синхронизация с игровым циклом**: Обновляйте позиции звуков в основном цикле.
4. **Конфигурация**: Выносите параметры аудио в конфигурационные файлы.

### 4.2 Примеры для различных архитектур

```cpp
// Пример интеграции с игровым объектом
class GameObject {
public:
    virtual void update_audio(float delta_time) {
        if (m_audio_source && m_spatial_audio) {
            // Обновление позиции звука
            ma_sound_set_position(m_audio_source,
                m_position.x, m_position.y, m_position.z);

            // Обновление скорости для доплер-эффекта
            ma_sound_set_velocity(m_audio_source,
                m_velocity.x, m_velocity.y, m_velocity.z);
        }
    }

protected:
    ma_sound* m_audio_source = nullptr;
    bool m_spatial_audio = true;
    glm::vec3 m_position;
    glm::vec3 m_velocity;
};

// Пример интеграции с системой частиц
class ParticleSystem {
public:
    void play_impact_sound(float x, float y, float z) {
        // Использование пула звуков для оптимизации
        ma_sound* sound = m_impact_sound_pool.acquire(x, y, z);
        if (sound) {
            // Звук автоматически освободится после воспроизведения
        }
    }

private:
    SoundPool<32> m_impact_sound_pool;
};
```

## 5. Оптимизация производительности

### 5.1 Управление количеством звуков

```cpp
// Ограничение одновременных звуков
class AudioLimiter {
public:
    AudioLimiter(size_t max_simultaneous_sounds = 32)
        : m_max_sounds(max_simultaneous_sounds) {}

    bool can_play_sound(float priority = 1.0f) {
        if (m_active_sounds < m_max_sounds) {
            m_active_sounds++;
            return true;
        }

        // Если достигнут лимит, проверяем приоритет
        return priority > 0.8f; // Высокоприоритетные звуки проходят
    }

    void sound_finished() {
        if (m_active_sounds > 0) {
            m_active_sounds--;
        }
    }

private:
    size_t m_max_sounds;
    std::atomic<size_t> m_active_sounds{0};
};
```

### 5.2 LOD (Level of Detail) для звуков

```cpp
// Система LOD для звуков
struct AudioLOD {
    float near_distance;   // Ближняя дистанция (полное качество)
    float mid_distance;    // Средняя дистанция (упрощённая обработка)
    float far_distance;    // Дальняя дистанция (минимальная обработка)

    // Параметры обработки для каждого уровня
    struct {
        bool occlusion;    // Расчёт окклюзии
        bool reverb;       // Расчёт реверберации
        bool doppler;      // Расчёт доплер-эффекта
        int update_rate;   // Частота обновления (Гц)
    } levels[3];
};

// Применение LOD к звуку
void apply_audio_lod(ma_sound* sound, float distance, const AudioLOD& lod) {
    if (distance <= lod.near_distance) {
        // Ближний уровень: полная обработка
        ma_sound_set_volume(sound, 1.0f);
    } else if (distance <= lod.mid_distance) {
        // Средний уровень: упрощённая обработка
        ma_sound_set_volume(sound, 0.7f);
    } else if (distance <= lod.far_distance) {
        // Дальний уровень: минимальная обработка
        ma_sound_set_volume(sound, 0.3f);
    } else {
        // За пределами LOD: отключение звука
        ma_sound_stop(sound);
    }
}
```

### 5.3 Асинхронная обработка

```cpp
// Асинхронная система обновления позиций звуков
class AsyncAudioUpdater {
public:
    void start_update_thread() {
        m_running = true;
        m_update_thread = std::thread([this]() {
            while (m_running) {
                update_audio_positions();
                std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
            }
        });
    }

    void stop_update_thread() {
        m_running = false;
        if (m_update_thread.joinable()) {
            m_update_thread.join();
        }
    }

private:
    void update_audio_positions() {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Обновление позиций звуков в отдельном потоке
        for (auto& sound_data : m_sounds_to_update) {
            ma_sound_set_position(sound_data.sound,
                sound_data.pos_x, sound_data.pos_y, sound_data.pos_z);
        }
    }

    std::thread m_update_thread;
    std::atomic<bool> m_running{false};
    std::mutex m_mutex;
    std::vector<SoundUpdateData> m_sounds_to_update;
};
```

---

## Практические рекомендации

### Тестирование spatial audio

1. **Тестирование с наушниками**: Spatial audio лучше всего воспринимается в наушниках.
2. **Движение слушателя**: Тестируйте при движении камеры/игрока.
3. **Множественные источники**: Проверяйте производительность при множестве одновременных звуков.
4. **Граничные случаи**: Тестируйте экстремальные положения и скорости.

### Отладка и визуализация

```cpp
// Визуализация позиций звуков для отладки
void debug_draw_audio_sources() {
    for (const auto& instance : m_audio_manager.get_sounds()) {
        if (!instance.spatial) continue;

        // Рисование маркера позиции звука
        draw_debug_sphere(instance.pos_x, instance.pos_y, instance.pos_z,
                         instance.max_distance * 0.1f, Color::Yellow);

        // Линия к слушателю
        draw_debug_line(instance.pos_x, instance.pos_y, instance.pos_z,
                       m_listener_x, m_listener_y, m_listener_z, Color::Cyan);
    }
}
```

### Оптимизация для разных платформ

1. **Мобильные устройства**: Уменьшите количество одновременных звуков, используйте более простые модели затухания.
2. **ПК**: Можно использовать продвинутые эффекты и большее количество звуков.
3. **Консоли**: Оптимизируйте под конкретную аппаратную платформу.

---

## См. также

- [Основные понятия miniaudio](concepts.md) — общие принципы работы с аудио
- [Advanced Topics](advanced-topics.md) — продвинутые возможности miniaudio
- [Use Cases](use-cases.md) — примеры использования spatial audio в различных сценариях
- [Производительность](performance.md) — оптимизация аудиосистемы
- [Интеграция с ECS](integration-flecs.md) — управление звуками через системы компонентов

---

## Примеры кода

Дополнительные примеры интеграции spatial audio смотрите в:

- [Примеры miniaudio](../examples/) — базовые примеры работы с аудио
- [Интеграция в ProjectV](projectv-integration.md) — специализированные паттерны для воксельного движка (опционально)

---

**← [Назад к основной документации miniaudio](README.md)**
