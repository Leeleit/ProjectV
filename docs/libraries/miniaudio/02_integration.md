# miniaudio в ProjectV: Интеграция с Vulkan 1.4, Flecs и SDL3

> **Для понимания:** Интеграция аудио в воксельный движок — это как добавить звуковое сопровождение к немому кино.
> Vulkan рисует картинку, SDL3 обрабатывает ввод, Flecs управляет сущностями, а miniaudio оживляет всё это звуком.
> Задача — сделать так, чтобы все системы работали слаженно, как оркестр.

## Архитектура звуковой подсистемы

В ProjectV аудио — это не изолированный остров, а часть ECS-мира. Звуковые компоненты — такие же сущности, как меши и
трансформы.

```
┌─────────────────────────────────────────────────────────────────┐
│                         SDL3 Window                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │   Vulkan     │    │    Flecs     │    │   miniaudio  │     │
│  │   Renderer   │◄───│    World     │───►│    Engine    │     │
│  │              │    │              │    │              │     │
│  │  - Meshes    │    │  - Position │    │  - Sounds    │     │
│  │  - Materials │    │  - Velocity │    │  - Spatial   │     │
│  │  - Lights    │    │  - AudioSrc │    │  - Groups    │     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│         │                   │                   │               │
│         └───────────────────┴───────────────────┘               │
│                          Main Loop                               │
└─────────────────────────────────────────────────────────────────┘
```

## CMake: Подключение miniaudio

### Вариант 1: FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    miniaudio
    # miniaudio repository configuration
)

# miniaudio is single-header - just fetch and make available
FetchContent_MakeAvailable(miniaudio)
```

### Вариант 2: Submodule

```cmake
# Add as git submodule
add_subdirectory(external/miniaudio)
```

### Вариант 3: Внешний build (vendor)

```cmake
# Если собираете miniaudio как статическую библиотеку
add_subdirectory(external/miniaudio EXCLUDE_FROM_ALL)

target_link_libraries(projectv PRIVATE miniaudio)
```

## Инициализация Audio Engine

> **Для понимания:** Инициализация аудио — это как включение звуковой карты перед игрой. Делаем это один раз при старте
> приложения, после создания окна SDL3 и инициализации Vulkan.

### Порядок инициализации

```cpp
// 1. SDL3 создаёт окно (REQUIRED для некоторых бэкендов)
if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    std::println("SDL Audio init failed: {}", SDL_GetError());
    return std::unexpected(AudioError::SDLInitFailed);
}

// 2. Vulkan инициализируется (стандартный порядок)
auto vulkan_result = vulkan::init_windowed(window);
if (!vulkan_result) {
    return std::unexpected(vulkan_result.error());
}


// 3. Flecs World создаётся
flecs::world ecs;
// Используем конфигурацию движка вместо std::thread::hardware_concurrency()
ecs.set_threads(get_engine_thread_count());

// 4. Audio Engine создаётся ПОСЛЕ Flecs
AudioEngine::Config audioConfig;
audioConfig.listenerCount = 1;  // Один игрок
audioConfig.sampleRate = 48000;
audioConfig.format = ma_format_f32;
audioConfig.channels = 2;

// Вспомогательная функция для получения количества потоков из конфигурации движка
size_t get_engine_thread_count() {
    // В ProjectV это значение берется из конфигурации движка
    // Например: projectv::core::config::getThreadPoolSize()
    // Для примера возвращаем разумное значение по умолчанию
    constexpr size_t DEFAULT_THREAD_COUNT = 4;
    return DEFAULT_THREAD_COUNT;
}

AudioEngine audioEngine;
auto init_result = audioEngine.init(audioConfig);
if (!init_result) {
    std::println("Audio engine init failed: {}", init_result.error());
    return std::unexpected(init_result.error());
}
```

### Класс-обёртка AudioEngine

```cpp
// projectv/audio/audio_engine.hpp
#pragma once

#include <miniaudio.h>
#include <expected>
#include <string>
#include <span>
#include <array>

namespace projectv::audio {

enum class AudioError {
    SDLInitFailed,
    EngineInitFailed,
    DeviceStartFailed,
    InvalidConfig,
    ResourceLoadFailed
};

class AudioEngine {
public:
    struct Config {
        ma_uint32 listenerCount = 1;
        ma_uint32 sampleRate = 48000;
        ma_format format = ma_format_f32;
        ma_uint32 channels = 2;
        ma_uint32 periodSizeInFrames = 512;
        bool noAutoStart = false;
    };

    AudioEngine() = default;
    ~AudioEngine();

    // Non-copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Movable
    AudioEngine(AudioEngine&& other) noexcept;
    AudioEngine& operator=(AudioEngine&& other) noexcept;

    std::expected<void, AudioError> init(const Config& config);
    void uninit();

    // Lifecycle
    void start();
    void stop();
    bool is_started() const;

    // Engine access
    ma_engine* engine() { return &m_engine; }
    const ma_engine* engine() const { return &m_engine; }

    // Listener management
    void set_listener_position(ma_uint32 index, float x, float y, float z);
    void set_listener_direction(ma_uint32 index, float forward_x, float forward_y, float forward_z,
                                  float up_x, float up_y, float up_z);

private:
    ma_engine m_engine{};
    bool m_initialized = false;
    bool m_started = false;
};

} // namespace projectv::audio
```

```cpp
// projectv/audio/audio_engine.cpp
#include "projectv/audio/audio_engine.hpp"
#include <print>

namespace projectv::audio {

AudioEngine::~AudioEngine() {
    uninit();
}

AudioEngine::AudioEngine(AudioEngine&& other) noexcept
    : m_engine(other.m_engine)
    , m_initialized(other.m_initialized)
    , m_started(other.m_started)
{
    other.m_initialized = false;
    other.m_started = false;
}

AudioEngine& AudioEngine::operator=(AudioEngine&& other) noexcept {
    if (this != &other) {
        uninit();
        m_engine = other.m_engine;
        m_initialized = other.m_initialized;
        m_started = other.m_started;
        other.m_initialized = false;
        other.m_started = false;
    }
    return *this;
}

std::expected<void, AudioError> AudioEngine::init(const Config& config) {
    if (m_initialized) {
        return std::unexpected(AudioError::InvalidConfig);
    }

    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.listenerCount = config.listenerCount;
    engineConfig.sampleRate = config.sampleRate;
    engineConfig.format = config.format;
    engineConfig.channels = config.channels;

    // Передаёмperiod size
    // miniaudio использует periods, не period напрямую
    engineConfig.periodSizeInFrames = config.periodSizeInFrames;
    engineConfig.periods = 2;  // Double buffering

    // Конфигурируем через device config
    // engineConfig.pDevice передаём nullptr = авто-выбор

    ma_result result = ma_engine_init(&engineConfig, &m_engine);
    if (result != MA_SUCCESS) {
        std::println("ma_engine_init failed: {}", ma_result_description(result));
        return std::unexpected(AudioError::EngineInitFailed);
    }

    m_initialized = true;

    if (!config.noAutoStart) {
        start();
    }

    return {};
}

void AudioEngine::uninit() {
    if (m_initialized) {
        stop();
        ma_engine_uninit(&m_engine);
        m_initialized = false;
    }
}

void AudioEngine::start() {
    if (m_initialized && !m_started) {
        ma_engine_start(&m_engine);
        m_started = true;
    }
}

void AudioEngine::stop() {
    if (m_initialized && m_started) {
        ma_engine_stop(&m_engine);
        m_started = false;
    }
}

bool AudioEngine::is_started() const {
    return m_started;
}

void AudioEngine::set_listener_position(ma_uint32 index, float x, float y, float z) {
    ma_engine_listener_set_position(&m_engine, index, x, y, z);
}

void AudioEngine::set_listener_direction(ma_uint32 index, float forward_x, float forward_y, float forward_z,
                                           float up_x, float up_y, float up_z) {
    ma_engine_listener_set_direction(&m_engine, index, forward_x, forward_y, forward_z);
    // Направление "вверх" задаётся через world_up
    // ma_engine_listener_set_world_up(...);
}

} // namespace projectv::audio
```

## ECS-интеграция: Компоненты и Системы

### Аудио-компоненты в ECS

```cpp
// projectv/ecs/components/audio_components.hpp
#pragma once

#include <miniaudio.h>
#include <flecs.h>
#include <cstdint>

namespace projectv::ecs {

// Тег: сущность издаёт звук
struct AudioSource {
    ma_sound sound{};  // Inline структура - не указатель!
    bool owns_sound = false;

    // Параметры spatial audio
    float position[3] = {0.0f, 0.0f, 0.0f};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float min_distance = 1.0f;
    float max_distance = 100.0f;
    float rolloff = 1.0f;
    bool spatial_enabled = true;
    bool looping = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
};

// Тег: сущность управляет группой звуков
struct AudioGroup {
    ma_sound_group group{};
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
};

// Тег: сущность - listener (игрок, камера)
struct AudioListener {
    ma_uint32 index = 0;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float forward[3] = {0.0f, 0.0f, -1.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
    float inner_angle = 6.283185f;  // 2*PI - полный круг
    float outer_angle = 6.283185f;
    float outer_gain = 0.0f;
};

// Группа звуков для категорий
struct SfxGroup {};      // Звуки эффектов
struct MusicGroup {};    // Музыка
struct VoiceGroup {};    // Голос

} // namespace projectv::ecs
```

### Регистрация компонентов

```cpp
// projectv/ecs/audio_register.cpp
#include "projectv/ecs/components/audio_components.hpp"

void register_audio_components(flecs::world& ecs) {
    // Регистрация компонентов (flecs сам определяет размер)
    ecs.component<AudioSource>("AudioSource");
    ecs.component<AudioGroup>("AudioGroup");
    ecs.component<AudioListener>("AudioListener");

    // Теги-маркеры
    ecs.component<SfxGroup>("SfxGroup");
    ecs.component<MusicGroup>("MusicGroup");
    ecs.component<VoiceGroup>("VoiceGroup");
}
```

### Система управления звуками

```cpp
// projectv/ecs/systems/audio_system.hpp
#pragma once

#include "projectv/audio/audio_engine.hpp"
#include <flecs.h>

namespace projectv::ecs {

class AudioSystem {
public:
    AudioSystem(audio::AudioEngine& engine, flecs::world& world);

    // Загрузка звука из файла
    std::expected<void, audio::AudioError> load_sound(
        flecs::entity entity,
        const char* filepath,
        bool spatial = false,
        bool looping = false
    );

    // Воспроизведение
    void play(flecs::entity entity);
    void stop(flecs::entity entity);
    void pause(flecs::entity entity);

    // Обновление (вызывается каждый кадр)
    void update(float delta_time);

private:
    audio::AudioEngine& m_engine;
    flecs::world& m_world;

    // Кэш загруженных звуков
    // Key: filepath, Value: decoded data или имя файла
    std::unordered_map<std::string, ma_sound*> m_sound_cache;
};

} // namespace projectv::ecs
```

```cpp
// projectv/ecs/systems/audio_system.cpp
#include "projectv/ecs/systems/audio_system.hpp"
#include "projectv/ecs/components/audio_components.hpp"
#include <print>

namespace projectv::ecs {

AudioSystem::AudioSystem(audio::AudioEngine& engine, flecs::world& world)
    : m_engine(engine)
    , m_world(world)
{
    // Регистрация системы в ECS
    m_world.system("AudioUpdate")
        .kind(flecs::OnUpdate)
        .each([this](flecs::entity e, AudioSource& src) {
            if (src.spatial_enabled) {
                // Обновляем позицию spatial звука
                ma_sound_set_position(&src.sound,
                    src.position[0], src.position[1], src.position[2]);
            }
        });
}

std::expected<void, audio::AudioError> AudioSystem::load_sound(
    flecs::entity entity,
    const char* filepath,
    bool spatial,
    bool looping
) {
    auto* e = m_engine.engine();

    AudioSource src;
    src.spatial_enabled = spatial;
    src.loopping = looping;

    // Инициализация звука
    ma_result result = ma_sound_init_from_file(
        e,
        filepath,
        spatial ? MA_SOUND_FLAG_NONE : MA_SOUND_FLAG_NO_SPATIALIZATION,
        nullptr,  // group - позже
        nullptr,   // fence
        &src.sound
    );

    if (result != MA_SUCCESS) {
        std::println("Failed to load sound '{}': {}",
            filepath, ma_result_description(result));
        return std::unexpected(audio::AudioError::ResourceLoadFailed);
    }

    // Настройка параметров
    if (looping) {
        ma_sound_set_looping(&src.sound, MA_TRUE);
    }

    // Сохраняем компонент
    entity.set<AudioSource>(src);

    return {};
}

void AudioSystem::play(flecs::entity entity) {
    if (auto* src = entity.get<AudioSource>()) {
        ma_sound_start(&src->sound);
    }
}

void AudioSystem::stop(flecs::entity entity) {
    if (auto* src = entity.get<AudioSource>()) {
        ma_sound_stop(&src->sound);
    }
}

void AudioSystem::pause(flecs::entity entity) {
    if (auto* src = entity.get<AudioSource>()) {
        ma_sound_stop(&src->sound);
    }
}

void AudioSystem::update(float /*delta_time*/) {
    // Обновление позиций listener из ECS-сущностей
    m_world.each<AudioListener>([this](flecs::entity e, AudioListener& listener) {
        ma_engine_listener_set_position(m_engine.engine(),
            listener.index,
            listener.position[0],
            listener.position[1],
            listener.position[2]);

        ma_engine_listener_set_direction(m_engine.engine(),
            listener.index,
            listener.forward[0],
            listener.forward[1],
            listener.forward[2]);
    });
}

} // namespace projectv::ecs
```

## Интеграция с Transform System

> **Для понимания:** Самый частый сценарий — звук должен следовать за объектом. Мы связываем AudioSource с Transform в
> ECS, и при обновлении позиции объекта автоматически обновляется позиция звука.

```cpp
// Компонент, связывающий звук с трансфором
struct AudioTransformLink {
    flecs::entity transform_entity;  // Ссылка на сущность с Transform
};

ecs.system<AudioSource>("SyncAudioTransform")
    .each([](AudioSource& audio, const AudioTransformLink& link) {
        if (auto* transform = link.transform_entity.get<Transform>()) {
            audio.position[0] = transform->position.x;
            audio.position[1] = transform->position.y;
            audio.position[2] = transform->position.z;

            // Опционально: velocity из Transform
            audio.velocity[0] = transform->velocity.x;
            audio.velocity[1] = transform->velocity.y;
            audio.velocity[2] = transform->velocity.z;
        }
    });
```

## Обработка SDL3 Events

SDL3 может переключать аудиоустройства (например, пользователь подключил наушники). Обрабатываем эти события:

```cpp
void handle_audio_device_events(const SDL_Event& event, AudioEngine& engine) {
    switch (event.type) {
        case SDL_AUDIODEVICEADDED:
            std::println("Audio device added: {}", event.adevice.which);
            // Обычно не требует действий - miniaudio автоматически
            // переключится на новое устройство
            break;

        case SDL_AUDIODEVICEREMOVED:
            std::println("Audio device removed: {}", event.adevice.which);
            // Остановить и переинициализировать engine
            engine.stop();
            // Miniaudio обрабатывает это автоматически для default device
            engine.start();
            break;
    }
}
```

## Vulkan-специфичные оптимизации

### Audio-Visual синхронизация

В воксельном движке важна синхронизация звука с визуалом:

```cpp
// Главный цикл
void main_loop() {
    // 1. SDL Input
    input_system.process_events();

    // 2. ECS Update
    float delta_time = calculate_delta_time();
    world.progress(delta_time);

    // 3. Audio Update ПОСЛЕ ECS, ДО Vulkan
    audio_system.update(delta_time);

    // 4. Vulkan Render
    renderer.render(frame_data);
}
```

### Zero-Copy для стриминга

Если нужно стримить аудио с диска:

```cpp
// Используем resource manager для асинхронной загрузки
ma_resource_manager_config rmConfig = ma_resource_manager_config_init();
rmConfig.jobThreadCount = 2;  // Отдельные потоки для загрузки

ma_resource_manager rm;
ma_resource_manager_init(&rmConfig, &rm);

// Передаём resource manager в engine
engineConfig.pResourceManager = &rm;

// Звук загружается в фоне
ma_sound_init_from_file(&engine, "music.ogg",
    MA_SOUND_FLAG_ASYNC,  // Асинхронная загрузка
    nullptr, nullptr, &sound);
```

## Типичные ошибки и их решения

### Ошибка: "No available audio device"

```cpp
// Проверка доступных устройств
ma_context context;
ma_context_init(nullptr, 0, nullptr, &context);

ma_device_info* playback_devices;
ma_uint32 playback_count;
ma_device_info* capture_devices;
ma_uint32 capture_count;

ma_context_get_devices(&context, &playback_devices, &playback_count,
                      &capture_devices, &capture_count);

std::println("Available playback devices: {}", playback_count);
for (ma_uint32 i = 0; i < playback_count; ++i) {
    std::println("  {}: {}", i, playback_devices[i].name);
}

ma_context_uninit(&context);
```

### Ошибка: Глитчи при большой нагрузке

```cpp
// Увеличьте период в конфиге
AudioEngine::Config config;
config.periodSizeInFrames = 1024;  // Было 512
config.periods = 3;  // Было 2
```

### Ошибка: Звук не позиционируется

```cpp
// Убедитесь что включен spatialization
ma_sound_set_spatialization_enabled(&sound, MA_TRUE);

// И listener настроен
ma_engine_listener_set_position(&engine, 0, 0, 0, 0);
```

## Резюме интеграции

1. **CMake**: FetchContent или submodule — без интерфейсной библиотеки
2. **Инициализация**: SDL3 → Vulkan → Flecs → AudioEngine
3. **ECS**: AudioSource, AudioListener, AudioGroup как компоненты
4. **Системы**: AudioSystem управляет звуками, синхронизирует с Transform
5. **Loop**: Input → ECS → Audio → Vulkan
6. **Device events**: SDL3 обрабатывает подключение/отключение устройств
