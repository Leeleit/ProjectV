# Интеграция в ProjectV

**🟡 Уровень 2: Средний**

CMake, SDL3, flecs, glm, Tracy — настройка и связка.

---

## CMake конфигурация

### Подключение miniaudio

```cmake
# CMakeLists.txt

# Вариант 1: Через подмодуль
add_subdirectory(external/miniaudio)
target_link_libraries(ProjectV PRIVATE miniaudio)

# Вариант 2: Single-file
add_library(miniaudio STATIC external/miniaudio/miniaudio.c)
target_include_directories(miniaudio PUBLIC external/miniaudio)
target_link_libraries(ProjectV PRIVATE miniaudio)

# Платформенные зависимости
if(LINUX)
    target_link_libraries(ProjectV PRIVATE pthread dl m)
endif()
```

### Минимальная конфигурация

```cmake
cmake_minimum_required(VERSION 3.25)
project(ProjectV LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)

# miniaudio
add_subdirectory(external/miniaudio)

# Основной проект
add_executable(ProjectV src/main.cpp)
target_link_libraries(ProjectV PRIVATE miniaudio)

if(LINUX)
    target_link_libraries(ProjectV PRIVATE pthread dl m)
endif()
```

---

## Интеграция с SDL3

### Инициализация

SDL3 и miniaudio не конфликтуют. SDL3 не имеет встроенного аудио API (в отличие от SDL2), что упрощает интеграцию.

```cpp
#include <SDL3/SDL.h>
#include <miniaudio.h>

class AudioSystem {
public:
    bool init() {
        // Инициализация SDL
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            return false;
        }
        
        // Инициализация miniaudio
        ma_engine_config config = ma_engine_config_init();
        config.sampleRate = 48000;
        config.channels = 2;
        
        ma_result result = ma_engine_init(&config, &m_engine);
        if (result != MA_SUCCESS) {
            SDL_Quit();
            return false;
        }
        
        return true;
    }
    
    void shutdown() {
        ma_engine_uninit(&m_engine);
        SDL_Quit();
    }
    
    ma_engine* getEngine() { return &m_engine; }

private:
    ma_engine m_engine;
};
```

### Обработка событий в главном цикле

```cpp
void mainLoop() {
    bool running = true;
    
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            // Обработка других событий...
        }
        
        // Обновление логики...
        update();
        
        // Рендеринг...
        render();
    }
}
```

---

## Интеграция с flecs ECS

### Компоненты

```cpp
#include <flecs.h>
#include <miniaudio.h>

// Компонент позиции (для spatial audio)
struct Position {
    float x, y, z;
};

// Компонент звука
struct Sound {
    ma_sound handle;
    bool isPlaying;
    bool is3D;
};

// Компонент listener
struct Listener {
    ma_uint32 index;  // Индекс listener в miniaudio
};

// Тег для звуковых эффектов
struct SFX {};
struct Music {};
struct Ambient {};
```

### Система аудио

```cpp
class AudioSystem {
public:
    static void registerSystems(flecs::world& ecs, ma_engine* pEngine) {
        ecs.set<AudioSystemContext>({ pEngine });
        
        // Система обновления позиции 3D звуков
        ecs.system<Sound, const Position>("UpdateSoundPosition")
            .kind(flecs::OnUpdate)
            .iter(updateSoundPosition);
        
        // Система обновления listener
        ecs.system<Listener, const Position>("UpdateListenerPosition")
            .kind(flecs::OnUpdate)
            .iter(updateListenerPosition);
        
        // Система очистки завершённых звуков
        ecs.system<Sound>("CleanupFinishedSounds")
            .kind(flecs::OnUpdate)
            .iter(cleanupFinishedSounds);
    }

private:
    static ma_engine* s_pEngine;
    
    static void updateSoundPosition(flecs::iter& it, Sound* sounds, const Position* positions) {
        for (auto i : it) {
            if (sounds[i].is3D) {
                ma_sound_set_position(&sounds[i].handle, 
                    positions[i].x, positions[i].y, positions[i].z);
            }
        }
    }
    
    static void updateListenerPosition(flecs::iter& it, Listener* listeners, const Position* positions) {
        for (auto i : it) {
            ma_engine_listener_set_position(s_pEngine, listeners[i].index,
                positions[i].x, positions[i].y, positions[i].z);
        }
    }
    
    static void cleanupFinishedSounds(flecs::iter& it, Sound* sounds) {
        for (auto i : it) {
            if (sounds[i].isPlaying && !ma_sound_is_playing(&sounds[i].handle)) {
                // Звук завершился
                sounds[i].isPlaying = false;
                // Можно удалить сущность или вернуть в pool
            }
        }
    }
};

// Контекст для доступа к engine
struct AudioSystemContext {
    ma_engine* pEngine;
};
```

### Создание звуков

```cpp
flecs::entity createSound3D(flecs::world& ecs, ma_engine* pEngine, 
                            const char* filePath, float x, float y, float z) {
    auto entity = ecs.entity();
    
    // Инициализация звука
    Sound* sound = entity.get_mut<Sound>();
    ma_result result = ma_sound_init_from_file(pEngine, filePath, 
        MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT, NULL, NULL, &sound->handle);
    
    if (result != MA_SUCCESS) {
        entity.destruct();
        return flecs::entity::null();
    }
    
    sound->isPlaying = false;
    sound->is3D = true;
    
    // Установка позиции
    entity.set<Position>({ x, y, z });
    ma_sound_set_position(&sound->handle, x, y, z);
    
    // Включение spatialization
    ma_sound_set_spatialization_enabled(&sound->handle, true);
    ma_sound_set_attenuation_model(&sound->handle, ma_attenuation_model_inverse);
    ma_sound_set_min_distance(&sound->handle, 1.0f);
    ma_sound_set_max_distance(&sound->handle, 50.0f);
    
    // Подключение к endpoint
    ma_sound_start(&sound->handle);
    sound->isPlaying = true;
    
    return entity;
}
```

---

## Интеграция с glm

### Конвертация типов

glm и miniaudio используют совместимые типы данных.

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Установка позиции звука из glm::vec3
void setSoundPosition(ma_sound* pSound, const glm::vec3& pos) {
    ma_sound_set_position(pSound, pos.x, pos.y, pos.z);
}

// Установка позиции listener из glm::vec3
void setListenerPosition(ma_engine* pEngine, ma_uint32 listenerIndex, const glm::vec3& pos) {
    ma_engine_listener_set_position(pEngine, listenerIndex, pos.x, pos.y, pos.z);
}

// Установка направления listener из glm::vec3
void setListenerDirection(ma_engine* pEngine, ma_uint32 listenerIndex, const glm::vec3& forward, const glm::vec3& up) {
    ma_engine_listener_set_direction(pEngine, listenerIndex, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(pEngine, listenerIndex, up.x, up.y, up.z);
}

// Установка направления звука из glm::vec3
void setSoundDirection(ma_sound* pSound, const glm::vec3& dir) {
    ma_sound_set_direction(pSound, dir.x, dir.y, dir.z);
}

// Установка скорости звука для Doppler эффекта
void setSoundVelocity(ma_sound* pSound, const glm::vec3& vel) {
    ma_sound_set_velocity(pSound, vel.x, vel.y, vel.z);
}
```

### Направление из кватерниона

```cpp
// Получение forward вектора из кватерниона
glm::vec3 getForwardVector(const glm::quat& rotation) {
    return rotation * glm::vec3(0.0f, 0.0f, -1.0f);  // -Z forward
}

// Получение up вектора из кватерниона
glm::vec3 getUpVector(const glm::quat& rotation) {
    return rotation * glm::vec3(0.0f, 1.0f, 0.0f);  // +Y up
}

// Установка ориентации listener из Transform
void setListenerOrientation(ma_engine* pEngine, ma_uint32 listenerIndex, 
                            const glm::vec3& position, const glm::quat& rotation) {
    setListenerPosition(pEngine, listenerIndex, position);
    setListenerDirection(pEngine, listenerIndex, 
        getForwardVector(rotation), getUpVector(rotation));
}
```

---

## Интеграция с Tracy

### Профилирование audio callback

```cpp
#include <tracy/Tracy.hpp>

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ZoneScopedN("AudioCallback");
    
    // Ваш код обработки аудио...
    processAudio(pOutput, frameCount);
    
    FrameMark;  // Отметка конца кадра (опционально)
}
```

### Профилирование High-level API

```cpp
void playSound(ma_engine* pEngine, const char* filePath) {
    ZoneScoped;
    TracyMessageLiteral(filePath, strlen(filePath));
    
    ma_engine_play_sound(pEngine, filePath, NULL);
}
```

### Кастомные метрики

```cpp
// Счётчик активных звуков
static int s_activeSoundCount = 0;

void trackActiveSounds(int delta) {
    s_activeSoundCount += delta;
    TracyPlot("Active Sounds", s_activeSoundCount);
}

// В системе очистки:
if (!ma_sound_is_playing(&sound->handle)) {
    trackActiveSounds(-1);
    // ...
}
```

### Профилирование ECS систем

```cpp
static void updateSoundPosition(flecs::iter& it, Sound* sounds, const Position* positions) {
    ZoneScopedN("UpdateSoundPosition");
    
    for (auto i : it) {
        if (sounds[i].is3D) {
            ma_sound_set_position(&sounds[i].handle, 
                positions[i].x, positions[i].y, positions[i].z);
        }
    }
}
```

---

## Примеры кода

### Полный пример инициализации

```cpp
// audio_system.hpp
#pragma once

#include <SDL3/SDL.h>
#include <miniaudio.h>
#include <flecs.h>
#include <glm/glm.hpp>
#include <tracy/Tracy.hpp>

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem() { shutdown(); }
    
    bool init();
    void shutdown();
    
    void registerECS(flecs::world& ecs);
    
    // Управление звуками
    flecs::entity playSound2D(const char* filePath, float volume = 1.0f);
    flecs::entity playSound3D(const char* filePath, const glm::vec3& position, float volume = 1.0f);
    
    // Управление listener
    void setListenerPosition(const glm::vec3& pos);
    void setListenerOrientation(const glm::vec3& forward, const glm::vec3& up);
    
    // Группы
    void setSFXVolume(float volume);
    void setMusicVolume(float volume);
    void setAmbientVolume(float volume);
    
    ma_engine* getEngine() { return &m_engine; }

private:
    ma_engine m_engine;
    ma_sound_group m_sfxGroup;
    ma_sound_group m_musicGroup;
    ma_sound_group m_ambientGroup;
    flecs::world* m_ecs = nullptr;
    
    static void updateSoundPositionSystem(flecs::iter& it, Sound* sounds, const Position* positions);
    static void cleanupFinishedSoundsSystem(flecs::iter& it, Sound* sounds);
};
```

```cpp
// audio_system.cpp
#include "audio_system.hpp"

bool AudioSystem::init() {
    ZoneScoped;
    
    // Конфигурация engine
    ma_engine_config config = ma_engine_config_init();
    config.sampleRate = 48000;
    config.channels = 2;
    config.listenerCount = 1;
    
    ma_result result = ma_engine_init(&config, &m_engine);
    if (result != MA_SUCCESS) {
        return false;
    }
    
    // Создание групп
    ma_sound_group_init(&m_engine, 0, NULL, &m_sfxGroup);
    ma_sound_group_init(&m_engine, 0, NULL, &m_musicGroup);
    ma_sound_group_init(&m_engine, 0, NULL, &m_ambientGroup);
    
    return true;
}

void AudioSystem::shutdown() {
    ZoneScoped;
    
    ma_sound_group_uninit(&m_sfxGroup);
    ma_sound_group_uninit(&m_musicGroup);
    ma_sound_group_uninit(&m_ambientGroup);
    ma_engine_uninit(&m_engine);
}

void AudioSystem::registerECS(flecs::world& ecs) {
    m_ecs = &ecs;
    
    ecs.system<Sound, const Position>("UpdateSoundPosition")
        .kind(flecs::OnUpdate)
        .iter(updateSoundPositionSystem);
    
    ecs.system<Sound>("CleanupFinishedSounds")
        .kind(flecs::PreStore)
        .iter(cleanupFinishedSoundsSystem);
}

flecs::entity AudioSystem::playSound2D(const char* filePath, float volume) {
    ZoneScoped;
    
    auto entity = m_ecs->entity();
    Sound* sound = entity.get_mut<Sound>();
    
    ma_result result = ma_sound_init_from_file(&m_engine, filePath, 
        0, &m_sfxGroup, NULL, &sound->handle);
    
    if (result != MA_SUCCESS) {
        entity.destruct();
        return flecs::entity::null();
    }
    
    ma_sound_set_volume(&sound->handle, volume);
    ma_sound_start(&sound->handle);
    
    sound->isPlaying = true;
    sound->is3D = false;
    
    return entity;
}

flecs::entity AudioSystem::playSound3D(const char* filePath, const glm::vec3& position, float volume) {
    ZoneScoped;
    
    auto entity = m_ecs->entity();
    Sound* sound = entity.get_mut<Sound>();
    
    ma_result result = ma_sound_init_from_file(&m_engine, filePath, 
        0, &m_sfxGroup, NULL, &sound->handle);
    
    if (result != MA_SUCCESS) {
        entity.destruct();
        return flecs::entity::null();
    }
    
    // Настройка 3D
    ma_sound_set_spatialization_enabled(&sound->handle, true);
    ma_sound_set_position(&sound->handle, position.x, position.y, position.z);
    ma_sound_set_attenuation_model(&sound->handle, ma_attenuation_model_inverse);
    ma_sound_set_min_distance(&sound->handle, 1.0f);
    ma_sound_set_max_distance(&sound->handle, 100.0f);
    ma_sound_set_volume(&sound->handle, volume);
    ma_sound_start(&sound->handle);
    
    sound->isPlaying = true;
    sound->is3D = true;
    
    entity.set<Position>({ position.x, position.y, position.z });
    
    return entity;
}

void AudioSystem::setListenerPosition(const glm::vec3& pos) {
    ma_engine_listener_set_position(&m_engine, 0, pos.x, pos.y, pos.z);
}

void AudioSystem::setListenerOrientation(const glm::vec3& forward, const glm::vec3& up) {
    ma_engine_listener_set_direction(&m_engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_engine, 0, up.x, up.y, up.z);
}

void AudioSystem::setSFXVolume(float volume) {
    ma_sound_group_set_volume(&m_sfxGroup, volume);
}

void AudioSystem::setMusicVolume(float volume) {
    ma_sound_group_set_volume(&m_musicGroup, volume);
}

void AudioSystem::setAmbientVolume(float volume) {
    ma_sound_group_set_volume(&m_ambientGroup, volume);
}

void AudioSystem::updateSoundPositionSystem(flecs::iter& it, Sound* sounds, const Position* positions) {
    ZoneScopedN("AudioSystem::UpdateSoundPosition");
    
    for (auto i : it) {
        if (sounds[i].is3D && sounds[i].isPlaying) {
            ma_sound_set_position(&sounds[i].handle, 
                positions[i].x, positions[i].y, positions[i].z);
        }
    }
}

void AudioSystem::cleanupFinishedSoundsSystem(flecs::iter& it, Sound* sounds) {
    ZoneScopedN("AudioSystem::CleanupFinishedSounds");
    
    for (auto i : it) {
        if (sounds[i].isPlaying && !ma_sound_is_playing(&sounds[i].handle)) {
            ma_sound_uninit(&sounds[i].handle);
            it.entity(i).destruct();
        }
    }
}
```
