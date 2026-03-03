# Use Cases для miniaudio

**🟡 Уровень 2: Средний**

Этот документ описывает практические сценарии использования miniaudio для различных типов приложений. Каждый use case
включает архитектурные решения, примеры кода и советы по оптимизации.

## Содержание

1. [Игры (Game Audio)](#1-игры-game-audio)
2. [Медиаплееры и Стриминг](#2-медиаплееры-и-стриминг)
3. [Аудио Инструменты и DAW](#3-аудио-инструменты-и-daw)
4. [Захват Аудио и Запись](#4-захват-аудио-и-запись)
5. [Образовательные и Научные Приложения](#5-образовательные-и-научные-приложения)
6. [UI Звуки и Оповещения](#6-ui-звуки-и-оповещения)
7. [Интеграция с Другими Библиотеками](#7-интеграция-с-другими-библиотеками)
8. [Встраиваемые Системы и IoT](#8-встраиваемые-системы-и-iot)

---

## 1. Игры (Game Audio)

**🟡 Уровень 2: Средний** — Система звуков для игр с поддержкой 3D аудио, микшированием и оптимизациями.

### Архитектурные решения

| Компонент            | Рекомендация                   | Альтернативы                       |
|----------------------|--------------------------------|------------------------------------|
| **API уровень**      | High-level (ma_engine)         | Low-level для специфичных нужд     |
| **Формат**           | s16 (баланс) или f32 (эффекты) | Зависит от целевой платформы       |
| **Resource Manager** | Обязательно для кэширования    | Простая загрузка для маленьких игр |
| **Spatial Audio**    | Включить для 3D игр            | Отключить для 2D/изометрических    |
| **Латентность**      | 10-20ms (low_latency)          | 5-10ms для ритм-игр                |

### Категории звуков и их обработка

#### 1.1 Фоновая музыка и Ambient

- **Загрузка**: Стриминг (MA_SOUND_FLAG_STREAM)
- **Приоритет**: Низкий
- **Особенности**: Зацикливание, плавные переходы
- **Оптимизация**: Предзагрузка следующего трека

```c
// Загрузка фоновой музыки со стримингом
ma_sound background_music;
ma_sound_init_from_file(&engine, "music/background.ogg",
    MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_DECODE,
    NULL, NULL, &background_music);

// Настройка зацикливания
ma_sound_set_looping(&background_music, MA_TRUE);

// Плавный переход громкости
ma_sound_set_fade_in_milliseconds(&background_music, 0.0f, 1.0f, 2000); // 2 секунды fade in
```

#### 1.2 Звуковые эффекты (SFX)

- **Загрузка**: В память (без стриминга)
- **Приоритет**: Высокий
- **Особенности**: Множественные экземпляры, пулы звуков
- **Оптимизация**: Пул звуков, дистанционный culling

```c
// Пул звуков для часто используемых SFX
class SoundPool {
    std::vector<ma_sound> sounds;
    std::vector<bool> available;
    ma_engine* engine;

public:
    SoundPool(ma_engine* eng, const char* filepath, size_t count) : engine(eng) {
        sounds.resize(count);
        available.resize(count, true);

        for (size_t i = 0; i < count; ++i) {
            ma_sound_init_from_file(engine, filepath, 0, NULL, NULL, &sounds[i]);
        }
    }

    ma_sound* acquire() {
        for (size_t i = 0; i < sounds.size(); ++i) {
            if (available[i]) {
                available[i] = false;
                ma_sound_seek_to_pcm_frame(&sounds[i], 0);
                return &sounds[i];
            }
        }
        return nullptr;
    }

    void release(ma_sound* sound) {
        for (size_t i = 0; i < sounds.size(); ++i) {
            if (&sounds[i] == sound) {
                ma_sound_stop(sound);
                available[i] = true;
                break;
            }
        }
    }
};

// Использование
SoundPool explosion_pool(&engine, "sounds/explosion.wav", 10);
ma_sound* explosion = explosion_pool.acquire();
if (explosion) {
    ma_sound_set_position(explosion, x, y, z);
    ma_sound_start(explosion);
    // Автоматический release после окончания воспроизведения
}
```

#### 1.3 3D Spatial Audio

- **Настройка**: Позиционирование, аттенюация, occlusion
- **Оптимизация**: Дистанционный culling, LOD для звуков
- **Особенности**: Raycasting для occlusion, материалы

```c
// Настройка слушателя (камеры)
ma_engine_listener_set_position(&engine, 0, camera_x, camera_y, camera_z);
ma_engine_listener_set_direction(&engine, 0, camera_forward_x, camera_forward_y, camera_forward_z);

// Настройка источника звука с параметрами 3D аудио
ma_sound_set_position(&sound, source_x, source_y, source_z);
ma_sound_set_attenuation_model(&sound, ma_attenuation_model_inverse);
ma_sound_set_rolloff(&sound, 1.0f); // Скорость затухания
ma_sound_set_min_distance(&sound, 1.0f); // Минимальная дистанция
ma_sound_set_max_distance(&sound, 100.0f); // Максимальная дистанция

// Cone attenuation для направленных звуков
ma_sound_set_cone(&sound, 30.0f * (MA_PI / 180.0f), 90.0f * (MA_PI / 180.0f), 0.5f);
```

#### 1.4 Голосовые реплики и диалоги

- **Загрузка**: Стриминг с буферизацией
- **Приоритет**: Средний
- **Особенности**: Субтитры, управление очередью
- **Оптимизация**: Предзагрузка следующих реплик

```c
// Система диалогов с очередью
class DialogueSystem {
    std::queue<std::pair<std::string, ma_sound>> dialogue_queue;
    ma_engine* engine;
    ma_sound current_sound;
    bool is_playing = false;

public:
    void play_dialogue(const std::string& filepath) {
        ma_sound sound;
        ma_sound_init_from_file(engine, filepath.c_str(),
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_DECODE,
            NULL, NULL, &sound);

        if (!is_playing) {
            ma_sound_start(&sound);
            current_sound = sound;
            is_playing = true;
        } else {
            dialogue_queue.push({filepath, sound});
        }
    }

    void update() {
        if (is_playing && ma_sound_at_end(&current_sound)) {
            ma_sound_uninit(&current_sound);
            is_playing = false;

            if (!dialogue_queue.empty()) {
                auto next = dialogue_queue.front();
                dialogue_queue.pop();
                current_sound = next.second;
                ma_sound_start(&current_sound);
                is_playing = true;
            }
        }
    }
};
```

### Оптимизации для игр

1. **Дистанционный culling**:

```c
// Отключаем звуки дальше определённой дистанции
float distance = calculate_distance(listener_pos, sound_pos);
if (distance > culling_distance && ma_sound_is_playing(&sound)) {
    ma_sound_stop(&sound);
}
```

2. **LOD для звуков**:
  - Высокое качество: ближние звуки (полный quality)
  - Среднее качество: средняя дистанция (пониженный битрейт)
  - Низкое качество: дальние звуки (моно, compressed)

3. **Асинхронная загрузка**:

```c
// Загрузка звуков в фоне во время загрузки уровня
ma_resource_manager* rm = ma_engine_get_resource_manager(&engine);
ma_resource_manager_data_source_callbacks callbacks = {};
ma_resource_manager_register_file(rm, "sounds/explosion.wav", 0, &callbacks, NULL);
```

---

## 2. Медиаплееры и Стриминг

**🟡 Уровень 2: Средний** — Воспроизведение аудиофайлов, управление плейлистами, стриминг.

### Архитектурные решения

| Компонент       | Рекомендация                  | Примечания                                 |
|-----------------|-------------------------------|--------------------------------------------|
| **API уровень** | High-level с Resource Manager | Low-level для кастомных декодеров          |
| **Формат**      | Автоматическое определение    | Поддержка всех встроенных codecs           |
| **Стриминг**    | Обязательно для файлов > 10MB | Регулируемый размер буфера                 |
| **Латентность** | 30-100ms                      | Баланс между отзывчивостью и стабильностью |

### 2.1 Базовый медиаплеер

```c
class MediaPlayer {
private:
    ma_engine engine;
    ma_sound current_sound;
    std::vector<std::string> playlist;
    size_t current_index = 0;
    bool is_playing = false;

public:
    MediaPlayer() {
        ma_engine_init(NULL, &engine);
    }

    ~MediaPlayer() {
        stop();
        ma_engine_uninit(&engine);
    }

    void load_playlist(const std::vector<std::string>& files) {
        playlist = files;
        current_index = 0;
        if (!playlist.empty()) {
            load_current();
        }
    }

    void load_current() {
        if (is_playing) {
            ma_sound_stop(&current_sound);
            ma_sound_uninit(&current_sound);
        }

        ma_sound_init_from_file(&engine, playlist[current_index].c_str(),
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_DECODE,
            NULL, NULL, &current_sound);
    }

    void play() {
        if (!playlist.empty()) {
            ma_sound_start(&current_sound);
            is_playing = true;
        }
    }

    void pause() {
        if (is_playing) {
            ma_sound_stop(&current_sound);
            is_playing = false;
        }
    }

    void stop() {
        if (is_playing) {
            ma_sound_stop(&current_sound);
            ma_sound_uninit(&current_sound);
            is_playing = false;
        }
    }

    void next() {
        if (playlist.empty()) return;

        current_index = (current_index + 1) % playlist.size();
        load_current();
        if (is_playing) play();
    }

    void prev() {
        if (playlist.empty()) return;

        current_index = (current_index == 0) ? playlist.size() - 1 : current_index - 1;
        load_current();
        if (is_playing) play();
    }

    // Геттеры для информации о треке
    ma_uint64 get_duration_ms() {
        ma_uint64 total_frames;
        ma_sound_get_length_in_pcm_frames(&current_sound, &total_frames);
        return (total_frames * 1000) / ma_engine_get_sample_rate(&engine);
    }

    ma_uint64 get_current_position_ms() {
        ma_uint64 cursor;
        ma_sound_get_cursor_in_pcm_frames(&current_sound, &cursor);
        return (cursor * 1000) / ma_engine_get_sample_rate(&engine);
    }

    void seek_to_ms(ma_uint64 position_ms) {
        ma_uint64 target_frames = (position_ms * ma_engine_get_sample_rate(&engine)) / 1000;
        ma_sound_seek_to_pcm_frame(&current_sound, target_frames);
    }
};
```

### 2.2 Продвинутый стриминг с буферизацией

```c
// Буферизованный стриминг для плавного воспроизведения
class BufferedStreamPlayer {
private:
    ma_decoder decoder;
    ma_device device;
    std::atomic<bool> stop_requested{false};
    std::thread decode_thread;
    std::queue<std::vector<float>> buffer_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    const size_t buffer_size = 4096; // frames
    const size_t max_buffers = 10; // максимальное число буферов в очереди

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        BufferedStreamPlayer* player = static_cast<BufferedStreamPlayer*>(pDevice->pUserData);
        player->audio_callback(pOutput, frameCount);
    }

    void audio_callback(void* pOutput, ma_uint32 frameCount) {
        std::unique_lock<std::mutex> lock(queue_mutex);

        if (buffer_queue.empty()) {
            // Буфер пуст - заполняем тишиной
            memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(ma_format_f32, 2));
            return;
        }

        std::vector<float> buffer = std::move(buffer_queue.front());
        buffer_queue.pop();
        lock.unlock();
        queue_cv.notify_one();

        // Копируем данные в выходной буфер
        size_t frames_to_copy = std::min(buffer.size() / 2, static_cast<size_t>(frameCount));
        memcpy(pOutput, buffer.data(), frames_to_copy * sizeof(float) * 2);

        // Если скопировали не всё, дополняем тишиной
        if (frames_to_copy < frameCount) {
            float* output_ptr = static_cast<float*>(pOutput);
            memset(output_ptr + frames_to_copy * 2, 0, (frameCount - frames_to_copy) * sizeof(float) * 2);
        }
    }

    void decode_worker() {
        std::vector<float> decode_buffer(buffer_size * 2); // стерео

        while (!stop_requested) {
            ma_uint64 frames_read;
            ma_result result = ma_decoder_read_pcm_frames(&decoder,
                decode_buffer.data(), buffer_size, &frames_read);

            if (result != MA_SUCCESS || frames_read == 0) {
                break; // Конец файла или ошибка
            }

            // Ждём, пока очередь не освободится
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this]() {
                return buffer_queue.size() < max_buffers || stop_requested;
            });

            if (stop_requested) break;

            // Добавляем буфер в очередь
            buffer_queue.push(std::vector<float>(
                decode_buffer.begin(),
                decode_buffer.begin() + frames_read * 2
            ));
        }
    }

public:
    bool open_file(const char* filepath) {
        ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, 2, 48000);
        if (ma_decoder_init_file(filepath, &decoder_config, &decoder) != MA_SUCCESS) {
            return false;
        }

        ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
        device_config.playback.format = ma_format_f32;
        device_config.playback.channels = 2;
        device_config.sampleRate = 48000;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;
        device_config.periodSizeInMilliseconds = 50; // Больший буфер для плавности

        if (ma_device_init(NULL, &device_config, &device) != MA_SUCCESS) {
            ma_decoder_uninit(&decoder);
            return false;
        }

        // Запускаем поток декодирования
        stop_requested = false;
        decode_thread = std::thread(&BufferedStreamPlayer::decode_worker, this);

        // Предзаполняем буфер перед стартом
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return true;
    }

    void play() {
        ma_device_start(&device);
    }

    void stop() {
        stop_requested = true;
        queue_cv.notify_all();

        ma_device_stop(&device);

        if (decode_thread.joinable()) {
            decode_thread.join();
        }

        // Очищаем очередь
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!buffer_queue.empty()) {
            buffer_queue.pop();
        }
    }

    ~BufferedStreamPlayer() {
        stop();
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
    }
};
```

### 2.3 Поддержка форматов и метаданных

```c
// Чтение метаданных аудиофайлов
struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    int duration_seconds = 0;
    int bitrate = 0;
    int sample_rate = 0;
    int channels = 0;
};

AudioMetadata read_audio_metadata(const char* filepath) {
    AudioMetadata metadata;

    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_unknown, 0, 0);

    if (ma_decoder_init_file(filepath, &config, &decoder) == MA_SUCCESS) {
        metadata.sample_rate = decoder.outputSampleRate;
        metadata.channels = decoder.outputChannels;

        // Получаем длительность
        ma_uint64 frame_count;
        if (ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count) == MA_SUCCESS) {
            metadata.duration_seconds = static_cast<int>(frame_count / decoder.outputSampleRate);
        }

        // Для MP3 можно попытаться получить битрейт через дополнительные методы
        // (miniaudio не предоставляет прямого доступа к метаданным ID3/MP3)

        ma_decoder_uninit(&decoder);
    }

    // Для более полных метаданных можно использовать специализированные библиотеки
    // или парсить заголовки файлов самостоятельно

    return metadata;
}
```

### 2.4 Управление эквалайзером

```c
// Простой 10-полосный графический эквалайзер
class GraphicEqualizer {
private:
    std::vector<ma_biquad_filter> filters;
    std::vector<float> frequencies = { 32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    std::vector<float> gains = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // dB

public:
    GraphicEqualizer() {
        filters.resize(frequencies.size());

        // Инициализация фильтров для каждой полосы
        for (size_t i = 0; i < frequencies.size(); ++i) {
            ma_biquad_filter_config config = ma_biquad_filter_config_init(
                ma_format_f32, 2, 48000,
                ma_biquad_filter_type_peak,
                frequencies[i],  // Центральная частота
                1.0f,            // Q-factor
                ma_db_to_linear(gains[i]) // Gain
            );
            ma_biquad_filter_init(&config, NULL, &filters[i]);
        }
    }

    void set_gain(size_t band, float gain_db) {
        if (band < gains.size()) {
            gains[band] = gain_db;
            ma_biquad_filter_set_gain_db(&filters[band], gain_db);
        }
    }

    void process(float* pFrames, ma_uint32 frameCount) {
        // Применяем все фильтры последовательно
        for (auto& filter : filters) {
            ma_biquad_process_pcm_frames(&filter, pFrames, frameCount, ma_format_f32, 2);
        }
    }

    // Presets
    void set_preset_flat() {
        for (size_t i = 0; i < gains.size(); ++i) {
            set_gain(i, 0.0f);
        }
    }

    void set_preset_rock() {
        std::vector<float> rock_gains = { 2, 1, 0, -1, 0, 1, 3, 4, 3, 2 };
        for (size_t i = 0; i < gains.size(); ++i) {
            set_gain(i, rock_gains[i]);
        }
    }

    void set_preset_jazz() {
        std::vector<float> jazz_gains = { 0, 0, 0, 1, 2, 1, 0, 0, 1, 2 };
        for (size_t i = 0; i < gains.size(); ++i) {
            set_gain(i, jazz_gains[i]);
        }
    }
};
```

---

## 3. Аудио Инструменты и DAW

**🔴 Уровень 3: Продвинутый** — Профессиональные инструменты для работы со звуком.

### Особенности

- **Минимальная latency** (< 10ms)
- **Многодорожечная запись и воспроизведение**
- **Реалтайм эффекты и обработка**
- **MIDI интеграция**
- **Поддержка VST/AU плагинов (через мост)**

### 3.1 Многодорожечный микшер

```c
class MultitrackMixer {
private:
    struct Track {
        ma_decoder decoder;
        std::vector<float> buffer;
        float volume = 1.0f;
        float pan = 0.0f; // -1.0 (left) to 1.0 (right)
        bool mute = false;
        bool solo = false;
    };

    std::vector<Track> tracks;
    ma_device device;
    float master_volume = 1.0f;

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        MultitrackMixer* mixer = static_cast<MultitrackMixer*>(pDevice->pUserData);
        mixer->mix_callback(static_cast<float*>(pOutput), frameCount);
    }

    void mix_callback(float* output, ma_uint32 frameCount) {
        // Обнуляем выходной буфер
        memset(output, 0, frameCount * sizeof(float) * 2);

        // Проверяем, есть ли solo дорожки
        bool has_solo = false;
        for (const auto& track : tracks) {
            if (track.solo) {
                has_solo = true;
                break;
            }
        }

        // Микшируем все дорожки
        for (auto& track : tracks) {
            if (track.mute) continue;
            if (has_solo && !track.solo) continue;

            // Читаем данные из декодера
            ma_uint64 frames_read;
            track.buffer.resize(frameCount * 2);
            ma_decoder_read_pcm_frames(&track.decoder, track.buffer.data(), frameCount, &frames_read);

            if (frames_read == 0) {
                // Конец дорожки - перематываем
                ma_decoder_seek_to_pcm_frame(&track.decoder, 0);
                ma_decoder_read_pcm_frames(&track.decoder, track.buffer.data(), frameCount, &frames_read);
            }

            // Применяем volume и pan, микшируем в выходной буфер
            for (ma_uint32 i = 0; i < frames_read; ++i) {
                float left = track.buffer[i * 2];
                float right = track.buffer[i * 2 + 1];

                // Применяем pan (простой balance)
                if (track.pan < 0) {
                    right *= (1.0f + track.pan); // pan left
                } else if (track.pan > 0) {
                    left *= (1.0f - track.pan); // pan right
                }

                // Применяем volume
                left *= track.volume;
                right *= track.volume;

                // Микшируем
                output[i * 2] += left;
                output[i * 2 + 1] += right;
            }
        }

        // Применяем master volume
        for (ma_uint32 i = 0; i < frameCount * 2; ++i) {
            output[i] *= master_volume;
        }
    }

public:
    bool add_track(const char* filepath) {
        Track track;
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);

        if (ma_decoder_init_file(filepath, &config, &track.decoder) != MA_SUCCESS) {
            return false;
        }

        tracks.push_back(std::move(track));
        return true;
    }

    bool init_device() {
        ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
        device_config.playback.format = ma_format_f32;
        device_config.playback.channels = 2;
        device_config.sampleRate = 48000;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;
        device_config.periodSizeInMilliseconds = 5; // Минимальная latency
        device_config.performanceProfile = ma_performance_profile_low_latency;

        return ma_device_init(NULL, &device_config, &device) == MA_SUCCESS;
    }

    void set_track_volume(size_t track_index, float volume) {
        if (track_index < tracks.size()) {
            tracks[track_index].volume = volume;
        }
    }

    void set_track_pan(size_t track_index, float pan) {
        if (track_index < tracks.size()) {
            tracks[track_index].pan = std::clamp(pan, -1.0f, 1.0f);
        }
    }

    void set_track_mute(size_t track_index, bool mute) {
        if (track_index < tracks.size()) {
            tracks[track_index].mute = mute;
        }
    }

    void set_track_solo(size_t track_index, bool solo) {
        if (track_index < tracks.size()) {
            tracks[track_index].solo = solo;
        }
    }

    void set_master_volume(float volume) {
        master_volume = volume;
    }

    void play() {
        ma_device_start(&device);
    }

    void stop() {
        ma_device_stop(&device);
    }

    ~MultitrackMixer() {
        stop();
        ma_device_uninit(&device);
        for (auto& track : tracks) {
            ma_decoder_uninit(&track.decoder);
        }
    }
};
```

### 3.2 Реалтайм эффекты процессор

```c
class RealTimeEffectProcessor {
private:
    ma_device device;

    // Эффекты
    ma_biquad_filter lowpass_filter;
    ma_biquad_filter highpass_filter;
    ma_delay_node delay;
    std::vector<ma_biquad_filter> eq_bands;

    // Параметры
    float lowpass_cutoff = 20000.0f;
    float highpass_cutoff = 20.0f;
    float delay_time = 0.5f; // секунды
    float delay_feedback = 0.5f;
    std::vector<float> eq_gains = {0, 0, 0, 0, 0};

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        RealTimeEffectProcessor* processor = static_cast<RealTimeEffectProcessor*>(pDevice->pUserData);

        // Копируем вход в выход для обработки
        memcpy(pOutput, pInput, frameCount * sizeof(float) * 2);

        // Применяем цепочку эффектов
        processor->process_effects(static_cast<float*>(pOutput), frameCount);
    }

    void process_effects(float* buffer, ma_uint32 frameCount) {
        // 1. Highpass фильтр
        ma_biquad_process_pcm_frames(&highpass_filter, buffer, frameCount, ma_format_f32, 2);

        // 2. EQ bands
        for (auto& eq : eq_bands) {
            ma_biquad_process_pcm_frames(&eq, buffer, frameCount, ma_format_f32, 2);
        }

        // 3. Lowpass фильтр
        ma_biquad_process_pcm_frames(&lowpass_filter, buffer, frameCount, ma_format_f32, 2);

        // 4. Delay (более сложная реализация через node graph)
        // Для простоты показана концепция
    }

public:
    RealTimeEffectProcessor() {
        // Инициализация фильтров
        ma_biquad_filter_config lowpass_config = ma_biquad_filter_config_init(
            ma_format_f32, 2, 48000,
            ma_biquad_filter_type_lowpass, lowpass_cutoff, 0.707f);
        ma_biquad_filter_init(&lowpass_config, NULL, &lowpass_filter);

        ma_biquad_filter_config highpass_config = ma_biquad_filter_config_init(
            ma_format_f32, 2, 48000,
            ma_biquad_filter_type_highpass, highpass_cutoff, 0.707f);
        ma_biquad_filter_init(&highpass_config, NULL, &highpass_filter);

        // Инициализация EQ bands
        std::vector<float> eq_frequencies = {100, 400, 1000, 4000, 10000};
        eq_bands.resize(eq_frequencies.size());
        for (size_t i = 0; i < eq_frequencies.size(); ++i) {
            ma_biquad_filter_config eq_config = ma_biquad_filter_config_init(
                ma_format_f32, 2, 48000,
                ma_biquad_filter_type_peak, eq_frequencies[i], 1.0f, ma_db_to_linear(eq_gains[i]));
            ma_biquad_filter_init(&eq_config, NULL, &eq_bands[i]);
        }
    }

    bool init_device() {
        ma_device_config device_config = ma_device_config_init(ma_device_type_duplex);
        device_config.playback.format = ma_format_f32;
        device_config.playback.channels = 2;
        device_config.capture.format = ma_format_f32;
        device_config.capture.channels = 2;
        device_config.sampleRate = 48000;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;
        device_config.periodSizeInMilliseconds = 10; // Low latency
        device_config.performanceProfile = ma_performance_profile_low_latency;

        return ma_device_init(NULL, &device_config, &device) == MA_SUCCESS;
    }

    void set_lowpass_cutoff(float cutoff_hz) {
        lowpass_cutoff = cutoff_hz;
        ma_biquad_filter_reinit(&lowpass_filter_config, &lowpass_filter);
    }

    void set_eq_gain(size_t band, float gain_db) {
        if (band < eq_gains.size()) {
            eq_gains[band] = gain_db;
            // Обновляем параметры фильтра
            // (нужно переинициализировать или использовать set_parameters)
        }
    }

    void start() {
        ma_device_start(&device);
    }

    void stop() {
        ma_device_stop(&device);
    }

    ~RealTimeEffectProcessor() {
        stop();
        ma_device_uninit(&device);
        ma_biquad_filter_uninit(&lowpass_filter, NULL);
        ma_biquad_filter_uninit(&highpass_filter, NULL);
        for (auto& eq : eq_bands) {
            ma_biquad_filter_uninit(&eq, NULL);
        }
    }
};
```

---

## 4. Захват Аудио и Запись

**🟡 Уровень 2: Средний** — Запись звука с микрофона или других источников.

### 4.1 Простой рекордер

```c
class AudioRecorder {
private:
    ma_device device;
    ma_encoder encoder;
    std::vector<float> recording_buffer;
    bool is_recording = false;

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        AudioRecorder* recorder = static_cast<AudioRecorder*>(pDevice->pUserData);
        recorder->capture_callback(static_cast<const float*>(pInput), frameCount);
    }

    void capture_callback(const float* input, ma_uint32 frameCount) {
        if (is_recording) {
            // Добавляем данные в буфер
            size_t old_size = recording_buffer.size();
            recording_buffer.resize(old_size + frameCount * 2); // стерео

            memcpy(recording_buffer.data() + old_size, input, frameCount * sizeof(float) * 2);
        }
    }

public:
    AudioRecorder() = default;

    bool init() {
        ma_device_config device_config = ma_device_config_init(ma_device_type_capture);
        device_config.capture.format = ma_format_f32;
        device_config.capture.channels = 2;
        device_config.sampleRate = 44100;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;

        if (ma_device_init(NULL, &device_config, &device) != MA_SUCCESS) {
            return false;
        }

        return true;
    }

    void start_recording() {
        if (!is_recording) {
            recording_buffer.clear();
            is_recording = true;
            ma_device_start(&device);
        }
    }

    void stop_recording() {
        if (is_recording) {
            ma_device_stop(&device);
            is_recording = false;
        }
    }

    bool save_to_wav(const char* filename) {
        if (recording_buffer.empty()) {
            return false;
        }

        ma_encoder_config encoder_config = ma_encoder_config_init(
            ma_encoding_format_wav,
            ma_format_f32,
            2, // channels
            44100 // sample rate
        );

        if (ma_encoder_init_file(filename, &encoder_config, &encoder) != MA_SUCCESS) {
            return false;
        }

        ma_uint64 frames_written;
        ma_encoder_write_pcm_frames(&encoder,
            recording_buffer.data(),
            recording_buffer.size() / 2, // frames (каждый frame = 2 сэмпла для стерео)
            &frames_written);

        ma_encoder_uninit(&encoder);
        return true;
    }

    ~AudioRecorder() {
        stop_recording();
        ma_device_uninit(&device);
    }
};
```

### 4.2 Запись с мониторингом (monitoring)

```c
class MonitoringRecorder : public AudioRecorder {
private:
    ma_device playback_device;
    float monitor_volume = 0.5f;
    bool monitoring_enabled = true;

    static void monitoring_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        MonitoringRecorder* recorder = static_cast<MonitoringRecorder*>(pDevice->pUserData);
        recorder->monitoring_mix(pOutput, pInput, frameCount);
    }

    void monitoring_mix(void* output, const void* input, ma_uint32 frameCount) {
        // Копируем вход в выход для мониторинга
        if (monitoring_enabled) {
            memcpy(output, input, frameCount * sizeof(float) * 2);

            // Применяем громкость мониторинга
            float* output_float = static_cast<float*>(output);
            for (ma_uint32 i = 0; i < frameCount * 2; ++i) {
                output_float[i] *= monitor_volume;
            }
        } else {
            // Без мониторинга - тишина
            memset(output, 0, frameCount * sizeof(float) * 2);
        }

        // Также записываем в буфер (через родительский класс)
        capture_callback(static_cast<const float*>(input), frameCount);
    }

public:
    bool init_with_monitoring() {
        // Инициализация duplex устройства (capture + playback)
        ma_device_config device_config = ma_device_config_init(ma_device_type_duplex);
        device_config.playback.format = ma_format_f32;
        device_config.playback.channels = 2;
        device_config.capture.format = ma_format_f32;
        device_config.capture.channels = 2;
        device_config.sampleRate = 44100;
        device_config.dataCallback = monitoring_callback;
        device_config.pUserData = this;

        if (ma_device_init(NULL, &device_config, &device) != MA_SUCCESS) {
            return false;
        }

        return true;
    }

    void set_monitoring_volume(float volume) {
        monitor_volume = std::clamp(volume, 0.0f, 1.0f);
    }

    void enable_monitoring(bool enabled) {
        monitoring_enabled = enabled;
    }
};
```

---

## 5. Образовательные и Научные Приложения

**🟡 Уровень 2: Средний** — Приложения для анализа, визуализации и обучения аудио.

### 5.1 Визуализатор спектра

```c
class SpectrumAnalyzer {
private:
    ma_device device;
    std::vector<float> capture_buffer;
    const size_t fft_size = 2048;
    std::vector<float> window; // Оконная функция
    std::vector<float> fft_real;
    std::vector<float> fft_imag;

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        SpectrumAnalyzer* analyzer = static_cast<SpectrumAnalyzer*>(pDevice->pUserData);
        analyzer->process_capture(static_cast<const float*>(pInput), frameCount);
    }

    void process_capture(const float* input, ma_uint32 frameCount) {
        // Добавляем данные в кольцевой буфер
        if (capture_buffer.size() < fft_size * 2) { // стерео
            capture_buffer.resize(fft_size * 2);
        }

        // Сдвигаем старые данные и добавляем новые
        size_t samples_to_add = frameCount * 2;
        size_t samples_to_keep = capture_buffer.size() - samples_to_add;

        if (samples_to_keep > 0) {
            memmove(capture_buffer.data(),
                   capture_buffer.data() + samples_to_add,
                   samples_to_keep * sizeof(float));
        }

        memcpy(capture_buffer.data() + samples_to_keep,
               input,
               samples_to_add * sizeof(float));
    }

    void apply_window(float* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            data[i] *= window[i];
        }
    }

public:
    SpectrumAnalyzer() {
        // Инициализация оконной функции (Ханна)
        window.resize(fft_size);
        for (size_t i = 0; i < fft_size; ++i) {
            window[i] = 0.5f * (1.0f - cosf(2.0f * MA_PI * i / (fft_size - 1)));
        }

        fft_real.resize(fft_size);
        fft_imag.resize(fft_size);
    }

    bool init() {
        ma_device_config device_config = ma_device_config_init(ma_device_type_capture);
        device_config.capture.format = ma_format_f32;
        device_config.capture.channels = 2;
        device_config.sampleRate = 44100;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;

        return ma_device_init(NULL, &device_config, &device) == MA_SUCCESS;
    }

    std::vector<float> get_spectrum() {
        std::vector<float> spectrum(fft_size / 2);

        if (capture_buffer.size() >= fft_size * 2) {
            // Берем только левый канал для анализа
            for (size_t i = 0; i < fft_size; ++i) {
                fft_real[i] = capture_buffer[i * 2]; // left channel
                fft_imag[i] = 0.0f;
            }

            // Применяем оконную функцию
            apply_window(fft_real.data(), fft_size);

            // Простой DFT (для реального использования лучше FFT библиотека)
            // Это упрощённый пример
            for (size_t k = 0; k < fft_size / 2; ++k) {
                float real_sum = 0.0f;
                float imag_sum = 0.0f;

                for (size_t n = 0; n < fft_size; ++n) {
                    float angle = 2.0f * MA_PI * k * n / fft_size;
                    real_sum += fft_real[n] * cosf(angle);
                    imag_sum += fft_real[n] * sinf(angle);
                }

                // Амплитуда (magnitude)
                spectrum[k] = sqrtf(real_sum * real_sum + imag_sum * imag_sum) / fft_size;
            }
        }

        return spectrum;
    }

    void start() {
        ma_device_start(&device);
    }

    void stop() {
        ma_device_stop(&device);
    }

    ~SpectrumAnalyzer() {
        stop();
        ma_device_uninit(&device);
    }
};
```

### 5.2 Генератор тонов и сигналов

```c
class ToneGenerator {
private:
    ma_device device;
    float frequency = 440.0f; // A4
    float amplitude = 0.5f;
    ma_waveform_type waveform = ma_waveform_type_sine;
    ma_waveform waveform_gen;

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        ToneGenerator* generator = static_cast<ToneGenerator*>(pDevice->pUserData);
        generator->generate_tone(static_cast<float*>(pOutput), frameCount);
    }

    void generate_tone(float* output, ma_uint32 frameCount) {
        ma_waveform_read_pcm_frames(&waveform_gen, output, frameCount, NULL);

        // Применяем амплитуду
        for (ma_uint32 i = 0; i < frameCount * 2; ++i) { // стерео
            output[i] *= amplitude;
        }
    }

public:
    ToneGenerator() {
        ma_waveform_config config = ma_waveform_config_init(
            ma_format_f32, 2, 44100, waveform, amplitude, frequency);
        ma_waveform_init(&config, &waveform_gen);
    }

    bool init() {
        ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
        device_config.playback.format = ma_format_f32;
        device_config.playback.channels = 2;
        device_config.sampleRate = 44100;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;

        return ma_device_init(NULL, &device_config, &device) == MA_SUCCESS;
    }

    void set_frequency(float freq_hz) {
        frequency = freq_hz;
        ma_waveform_set_frequency(&waveform_gen, frequency);
    }

    void set_amplitude(float amp) {
        amplitude = std::clamp(amp, 0.0f, 1.0f);
    }

    void set_waveform(ma_waveform_type type) {
        waveform = type;
        ma_waveform_set_type(&waveform_gen, waveform);
    }

    void start() {
        ma_device_start(&device);
    }

    void stop() {
        ma_device_stop(&device);
    }

    ~ToneGenerator() {
        stop();
        ma_device_uninit(&device);
        ma_waveform_uninit(&waveform_gen);
    }
};
```

---

## 6. UI Звуки и Оповещения

**🟢 Уровень 1: Начинающий** — Простые звуки для пользовательского интерфейса.

### 6.1 Система UI звуков

```c
class UISoundSystem {
private:
    ma_engine engine;
    std::unordered_map<std::string, ma_sound> sounds;

public:
    UISoundSystem() {
        ma_engine_init(NULL, &engine);
    }

    ~UISoundSystem() {
        for (auto& [name, sound] : sounds) {
            ma_sound_uninit(&sound);
        }
        ma_engine_uninit(&engine);
    }

    bool load_sound(const std::string& name, const char* filepath) {
        ma_sound sound;
        if (ma_sound_init_from_file(&engine, filepath, 0, NULL, NULL, &sound) != MA_SUCCESS) {
            return false;
        }

        // Настройки для UI звуков
        ma_sound_set_volume(&sound, 0.7f);
        ma_sound_set_pitch(&sound, 1.0f);
        ma_sound_set_looping(&sound, MA_FALSE);

        sounds[name] = sound;
        return true;
    }

    void play_sound(const std::string& name) {
        auto it = sounds.find(name);
        if (it != sounds.end()) {
            // Если звук уже играет, перематываем и запускаем снова
            if (ma_sound_is_playing(&it->second)) {
                ma_sound_seek_to_pcm_frame(&it->second, 0);
            }
            ma_sound_start(&it->second);
        }
    }

    void set_volume(const std::string& name, float volume) {
        auto it = sounds.find(name);
        if (it != sounds.end()) {
            ma_sound_set_volume(&it->second, volume);
        }
    }

    void set_master_volume(float volume) {
        ma_engine_set_volume(&engine, volume);
    }
};

// Использование
UISoundSystem ui_sounds;
ui_sounds.load_sound("click", "sounds/ui/click.wav");
ui_sounds.load_sound("hover", "sounds/ui/hover.wav");
ui_sounds.load_sound("notify", "sounds/ui/notification.wav");

// При клике кнопки
ui_sounds.play_sound("click");
```

### 6.2 Адаптивные UI звуки

```c
class AdaptiveUISounds {
private:
    ma_engine engine;
    ma_waveform beep_waveform;

public:
    AdaptiveUISounds() {
        ma_engine_init(NULL, &engine);

        // Инициализируем генератор тонов для динамических звуков
        ma_waveform_config wave_config = ma_waveform_config_init(
            ma_format_f32, 2, 44100,
            ma_waveform_type_sine, 0.3f, 800.0f);
        ma_waveform_init(&wave_config, &beep_waveform);
    }

    ~AdaptiveUISounds() {
        ma_waveform_uninit(&beep_waveform);
        ma_engine_uninit(&engine);
    }

    // Генерация тона разной частоты в зависимости от контекста
    void play_context_beep(UIContext context) {
        float frequency;
        float duration = 0.1f; // секунды

        switch (context) {
            case UIContext::Success:
                frequency = 800.0f; // Высокий тон для успеха
                break;
            case UIContext::Error:
                frequency = 400.0f; // Низкий тон для ошибки
                break;
            case UIContext::Warning:
                frequency = 600.0f; // Средний тон для предупреждения
                break;
            default:
                frequency = 500.0f;
        }

        ma_waveform_set_frequency(&beep_waveform, frequency);

        // Создаём временный sound из waveform
        ma_sound sound;
        ma_sound_init_from_data_source(&engine, &beep_waveform, 0, NULL, NULL, &sound);
        ma_sound_set_start_time_in_milliseconds(&sound, 0);
        ma_sound_set_stop_time_in_milliseconds(&sound, duration * 1000);
        ma_sound_start(&sound);

        // Автоматическое освобождение после воспроизведения
        // (в реальном коде нужно отслеживать завершение)
    }
};
```

---

## 7. Интеграция с Другими Библиотеками

**🟡 Уровень 2: Средний** — Совместное использование miniaudio с другими библиотеками.

### 7.1 Интеграция с SDL

```c
#include <SDL3/SDL.h>
#include "miniaudio.h"

class SDLAudioIntegration {
private:
    ma_engine engine;
    SDL_Window* window = nullptr;

public:
    bool init_sdl_and_audio() {
        // Инициализация SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
            return false;
        }

        window = SDL_CreateWindow("Audio Test", 800, 600, 0);
        if (!window) {
            SDL_Quit();
            return false;
        }

        // Инициализация miniaudio
        if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }

        return true;
    }

    void run_event_loop() {
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_KEY_DOWN) {
                    // Воспроизведение звука при нажатии клавиши
                    if (event.key.key == SDLK_SPACE) {
                        ma_engine_play_sound(&engine, "sounds/beep.wav", NULL);
                    }
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    // Звук при клике мыши
                    ma_engine_play_sound(&engine, "sounds/click.wav", NULL);
                }
            }

            // Отрисовка (упрощённо)
            SDL_Renderer* renderer = SDL_GetRenderer(window);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);

            SDL_Delay(16); // ~60 FPS
        }
    }

    ~SDLAudioIntegration() {
        ma_engine_uninit(&engine);
        if (window) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
};
```

### 7.2 Интеграция с ImGui

```c
#include "imgui.h"
#include "miniaudio.h"

class ImGuiAudioController {
private:
    ma_engine engine;
    ma_sound background_music;
    float music_volume = 0.5f;
    float sfx_volume = 0.7f;
    bool music_enabled = true;

public:
    ImGuiAudioController() {
        ma_engine_init(NULL, &engine);
        ma_sound_init_from_file(&engine, "music/background.ogg",
            MA_SOUND_FLAG_STREAM, NULL, NULL, &background_music);
        ma_sound_set_looping(&background_music, MA_TRUE);

        if (music_enabled) {
            ma_sound_start(&background_music);
        }
    }

    ~ImGuiAudioController() {
        ma_sound_uninit(&background_music);
        ma_engine_uninit(&engine);
    }

    void draw_audio_control_panel() {
        ImGui::Begin("Audio Controls");

        // Музыка
        ImGui::SeparatorText("Background Music");
        ImGui::Checkbox("Enable Music", &music_enabled);
        if (ImGui::IsItemEdited()) {
            if (music_enabled) {
                ma_sound_start(&background_music);
            } else {
                ma_sound_stop(&background_music);
            }
        }

        ImGui::SliderFloat("Music Volume", &music_volume, 0.0f, 1.0f);
        if (ImGui::IsItemEdited()) {
            ma_sound_set_volume(&background_music, music_volume);
        }

        // SFX
        ImGui::SeparatorText("Sound Effects");
        ImGui::SliderFloat("SFX Volume", &sfx_volume, 0.0f, 1.0f);
        if (ImGui::IsItemEdited()) {
            ma_engine_set_volume(&engine, sfx_volume);
        }

        // Тестовые звуки
        if (ImGui::Button("Test Beep")) {
            ma_engine_play_sound(&engine, "sounds/beep.wav", NULL);
        }

        if (ImGui::Button("Test Click")) {
            ma_engine_play_sound(&engine, "sounds/click.wav", NULL);
        }

        ImGui::End();
    }

    void play_ui_sound(const char* sound_name) {
        ma_engine_play_sound(&engine, sound_name, NULL);
    }
};
```

---

## 8. Встраиваемые Системы и IoT

**🟡 Уровень 2: Средний** — Использование miniaudio на устройствах с ограниченными ресурсами.

### Особенности для embedded:

- **Минимальный footprint** (отключение ненужных функций)
- **Low-power режимы**
- **Прямой доступ к аппаратному аудио**
- **Ограниченная память и CPU**

### 8.1 Конфигурация для embedded систем

```c
// Отключаем ненужные функции для уменьшения размера кода
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_ENGINE
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_GENERATION
#define MA_NO_ENCODING

// Включаем только необходимые бэкенды
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#ifdef MA_ENABLE_ONLY_SPECIFIC_BACKENDS
    #define MA_ENABLE_WASAPI      // Для Windows embedded
    #define MA_ENABLE_ALSA        // Для Linux embedded
    #define MA_ENABLE_AAUDIO      // Для Android embedded
    #define MA_ENABLE_NULL        // Для тестов
#endif

// Уменьшаем максимальное число каналов
#define MA_MAX_CHANNELS 2

// Уменьшаем размеры внутренних буферов
#define MA_MAX_FILTER_ORDER 2
#define MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS_LOW_LATENCY 20
#define MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS_CONSERVATIVE 40

#include "miniaudio.h"

// Embedded аудио плеер
class EmbeddedAudioPlayer {
private:
    ma_device device;
    ma_decoder decoder;

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        EmbeddedAudioPlayer* player = static_cast<EmbeddedAudioPlayer*>(pDevice->pUserData);
        player->playback_callback(static_cast<int16_t*>(pOutput), frameCount);
    }

    void playback_callback(int16_t* output, ma_uint32 frameCount) {
        ma_uint64 frames_read;
        ma_decoder_read_pcm_frames(&decoder, output, frameCount, &frames_read);

        if (frames_read < frameCount) {
            // Конец файла - заполняем тишиной
            memset(output + frames_read * 2, 0, (frameCount - frames_read) * sizeof(int16_t) * 2);

            // Перематываем к началу (зацикливание)
            ma_decoder_seek_to_pcm_frame(&decoder, 0);
        }
    }

public:
    bool init(const char* filepath) {
        // Конфигурация для embedded - низкое качество, экономия ресурсов
        ma_decoder_config decoder_config = ma_decoder_config_init(
            ma_format_s16,  // 16-bit для экономии памяти
            1,              // Моно для embedded (экономия 50% памяти)
            22050           // Пониженная частота дискретизации
        );

        if (ma_decoder_init_file(filepath, &decoder_config, &decoder) != MA_SUCCESS) {
            return false;
        }

        ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
        device_config.playback.format = ma_format_s16;
        device_config.playback.channels = 1; // Моно выход
        device_config.sampleRate = 22050;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;
        device_config.periodSizeInMilliseconds = 40; // Больше latency для стабильности
        device_config.performanceProfile = ma_performance_profile_conservative; // Экономия CPU

        return ma_device_init(NULL, &device_config, &device) == MA_SUCCESS;
    }

    void start() {
        ma_device_start(&device);
    }

    void stop() {
        ma_device_stop(&device);
    }

    ~EmbeddedAudioPlayer() {
        stop();
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
    }
};
```

### 8.2 Low-power аудио режим

```c
class LowPowerAudioPlayer {
private:
    ma_device device;
    ma_decoder decoder;
    bool is_playing = false;
    std::atomic<bool> pause_requested{false};

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        LowPowerAudioPlayer* player = static_cast<LowPowerAudioPlayer*>(pDevice->pUserData);
        player->low_power_callback(pOutput, frameCount);
    }

    void low_power_callback(void* output, ma_uint32 frameCount) {
        if (pause_requested) {
            // Режим паузы - заполняем тишиной
            memset(output, 0, frameCount * sizeof(int16_t) * 2);
            return;
        }

        if (!is_playing) {
            memset(output, 0, frameCount * sizeof(int16_t) * 2);
            return;
        }

        ma_uint64 frames_read;
        ma_decoder_read_pcm_frames(&decoder, output, frameCount, &frames_read);

        if (frames_read < frameCount) {
            memset((char*)output + frames_read * sizeof(int16_t) * 2, 0,
                  (frameCount - frames_read) * sizeof(int16_t) * 2);

            // Конец файла - останавливаем воспроизведение
            is_playing = false;
        }
    }

public:
    bool init_low_power(const char* filepath) {
        // Конфигурация для максимальной экономии энергии
        ma_decoder_config decoder_config = ma_decoder_config_init(
            ma_format_s16, 2, 22050); // Низкое качество для экономии

        if (ma_decoder_init_file(filepath, &decoder_config, &decoder) != MA_SUCCESS) {
            return false;
        }

        ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
        device_config.playback.format = ma_format_s16;
        device_config.playback.channels = 2;
        device_config.sampleRate = 22050;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;
        device_config.periodSizeInMilliseconds = 100; // Большие буферы = реже callback = экономия CPU
        device_config.performanceProfile = ma_performance_profile_conservative;
        device_config.noClip = true; // Не тратить CPU на клиппинг
        device_config.noPreSilencedOutputBuffer = true; // Не обнулять буфер перед callback

        return ma_device_init(NULL, &device_config, &device) == MA_SUCCESS;
    }

    void play() {
        is_playing = true;
        pause_requested = false;
        ma_device_start(&device);
    }

    void pause() {
        pause_requested = true;
    }

    void resume() {
        pause_requested = false;
    }

    void stop() {
        is_playing = false;
        ma_device_stop(&device);
        ma_decoder_seek_to_pcm_frame(&decoder, 0); // Перемотка к началу
    }

    // Режим ultra-low-power: останавливаем устройство полностью
    void enter_sleep_mode() {
        ma_device_stop(&device);
    }

    void exit_sleep_mode() {
        // Устройство уже инициализировано, можно запустить снова
        if (is_playing) {
            ma_device_start(&device);
        }
    }

    ~LowPowerAudioPlayer() {
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
    }
};
```

---

## Заключение

Этот документ охватывает основные use cases для miniaudio, от простых UI звуков до сложных профессиональных аудио
инструментов. Ключевые моменты:

1. **Выбирайте подходящий уровень API** для вашей задачи
2. **Оптимизируйте под целевую платформу** (игры, embedded, desktop)
3. **Используйте Resource Manager** для управления памятью и производительности
4. **Тестируйте на реальном железе**, особенно для embedded и low-latency приложений
5. **Начинайте с простого** и добавляйте сложность по мере необходимости

Для каждого use case представлены практические примеры, которые можно адаптировать под конкретные нужды. Дополнительные
детали и тонкости работы с API можно найти в соответствующих разделах документации.

---

**← [Назад к основной документации](README.md)**
