## Источники данных

<!-- anchor: 04_data-sources -->

**🟡 Уровень 2: Средний**

Data source — абстракция для любых источников аудиоданных: файлы, память, генераторы, кастомные реализации.

---

## Интерфейс ma_data_source

Все data sources реализуют общий интерфейс. Это позволяет использовать декодер, генератор или кастомный источник везде,
где ожидается `ma_data_source`.

### Основные функции интерфейса

```c
ma_result ma_data_source_read_pcm_frames(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
ma_result ma_data_source_seek_to_pcm_frame(ma_data_source* pDataSource, ma_uint64 frameIndex);
ma_result ma_data_source_get_length_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pLength);
ma_result ma_data_source_get_cursor_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pCursor);
ma_result ma_data_source_get_available_frames(ma_data_source* pDataSource, ma_uint64* pAvailableFrames);

ma_format ma_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
```

### Использование с декодером

```c
ma_decoder decoder;
ma_decoder_init_file("audio.wav", NULL, &decoder);

// Декодер — это data source
ma_data_source* pSource = (ma_data_source*)&decoder;

ma_uint64 framesRead;
float buffer[1024];
ma_data_source_read_pcm_frames(pSource, buffer, 1024, &framesRead);
```

---

## Декодеры

### Поддерживаемые форматы

| Формат | Встроенная поддержка | Примечание                             |
|--------|----------------------|----------------------------------------|
| WAV    | Да                   | Все варианты (PCM, float, ADPCM и др.) |
| FLAC   | Да                   | Без внешних зависимостей               |
| MP3    | Да                   | Без внешних зависимостей               |
| Vorbis | Нет                  | Требует внешний декодер                |
| Opus   | Нет                  | Требует внешний декодер                |
| AAC    | Нет                  | Требует внешний декодер                |

### Инициализация декодера

```c
// Из файла
ma_result ma_decoder_init_file(const char* pFilePath, const ma_decoder_config* pConfig, ma_decoder* pDecoder);

// Из памяти
ma_result ma_decoder_init_memory(const void* pData, size_t dataSize, const ma_decoder_config* pConfig, ma_decoder* pDecoder);

// С кастомными callbacks (для виртуальных файлов)
ma_result ma_decoder_init(ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek, void* pUserData, const ma_decoder_config* pConfig, ma_decoder* pDecoder);
```

### Конфигурация декодера

```c
ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);
// outputFormat = ma_format_f32  — выходной формат
// outputChannels = 2            — выходные каналы (resample если нужно)
// outputSampleRate = 48000      — выходная частота

// Дополнительные поля
config.allocationCallbacks = NULL;  // Кастомный аллокатор
config.encodingFormat = ma_encoding_format_unknown;  // Явно указать формат
```

### Пример: декодирование в память

```c
#include "miniaudio.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    ma_decoder decoder;
    ma_result result;

    result = ma_decoder_init_file("sound.wav", NULL, &decoder);
    if (result != MA_SUCCESS) {
        printf("Ошибка: %s\n", ma_result_description(result));
        return -1;
    }

    // Получить длину
    ma_uint64 length;
    ma_decoder_get_length_in_pcm_frames(&decoder, &length);

    // Выделить буфер
    size_t bytesPerFrame = ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
    void* pBuffer = malloc((size_t)(length * bytesPerFrame));

    // Прочитать все кадры
    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(&decoder, pBuffer, length, &framesRead);

    printf("Декодировано %llu кадров\n", (unsigned long long)framesRead);

    free(pBuffer);
    ma_decoder_uninit(&decoder);
    return 0;
}
```

---

## Кастомные декодеры (Vorbis, Opus)

Для форматов без встроенной поддержки можно подключить внешний декодер.

### Структура callbacks

```c
typedef ma_result (*ma_decoder_read_proc)(ma_decoder* pDecoder, void* pBuffer, size_t bytesToRead, size_t* pBytesRead);
typedef ma_result (*ma_decoder_seek_proc)(ma_decoder* pDecoder, ma_int64 offset, ma_seek_origin origin);
```

### Пример: интеграция stb_vorbis

```c
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.h"

typedef struct {
    ma_data_source_base base;
    stb_vorbis* pVorbis;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
} vorbis_decoder;

static ma_result vorbis_read_pcm_frames(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
    vorbis_decoder* pDec = (vorbis_decoder*)pDataSource;
    int framesRead = stb_vorbis_get_samples_float_interleaved(
        pDec->pVorbis,
        pDec->channels,
        (float*)pFramesOut,
        (int)(frameCount * pDec->channels)
    );
    if (pFramesRead) *pFramesRead = framesRead;
    return (framesRead > 0) ? MA_SUCCESS : MA_AT_END;
}

// ... реализация остальных методов data source
```

---

## Энкодеры

miniaudio поддерживает кодирование только в WAV.

### Основные функции

```c
ma_encoder_config ma_encoder_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate);
ma_result ma_encoder_init_file(const char* pFilePath, const ma_encoder_config* pConfig, ma_encoder* pEncoder);
ma_result ma_encoder_init_write_callbacks(ma_encoder_write_proc onWrite, ma_encoder_seek_proc onSeek, void* pUserData, const ma_encoder_config* pConfig, ma_encoder* pEncoder);

ma_result ma_encoder_write_pcm_frames(ma_encoder* pEncoder, const void* pFramesIn, ma_uint64 frameCount, ma_uint64* pFramesWritten);

void ma_encoder_uninit(ma_encoder* pEncoder);
```

### Пример: запись в WAV файл

```c
#include "miniaudio.h"

int main() {
    ma_encoder encoder;
    ma_encoder_config config = ma_encoder_config_init(ma_format_s16, 2, 44100);

    ma_result result = ma_encoder_init_file("output.wav", &config, &encoder);
    if (result != MA_SUCCESS) {
        return -1;
    }

    // Генерация или захват аудио
    int16_t buffer[1024 * 2];  // 1024 стерео кадров
    for (int i = 0; i < 1024; i++) {
        // Синус 440 Гц
        float t = (float)i / 44100.0f;
        int16_t sample = (int16_t)(32767.0f * 0.1f * sinf(2.0f * 3.14159f * 440.0f * t));
        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }

    ma_uint64 framesWritten;
    ma_encoder_write_pcm_frames(&encoder, buffer, 1024, &framesWritten);

    ma_encoder_uninit(&encoder);
    return 0;
}
```

---

## Генераторы

miniaudio включает базовые генераторы волн.

### ma_waveform

```c
ma_waveform_config ma_waveform_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_waveform_type type, double amplitude, double frequency);
ma_result ma_waveform_init(const ma_waveform_config* pConfig, ma_waveform* pWaveform);
void ma_waveform_uninit(ma_waveform* pWaveform);

ma_result ma_waveform_read_pcm_frames(ma_waveform* pWaveform, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);

void ma_waveform_set_frequency(ma_waveform* pWaveform, double frequency);
void ma_waveform_set_amplitude(ma_waveform* pWaveform, double amplitude);
void ma_waveform_set_type(ma_waveform* pWaveform, ma_waveform_type type);
```

### Типы волн

```c
typedef enum {
    ma_waveform_type_sine,       // Синус
    ma_waveform_type_square,     // Прямоугольная
    ma_waveform_type_triangle,   // Треугольная
    ma_waveform_type_sawtooth    // Пилообразная
} ma_waveform_type;
```

### Пример: генерация синусоиды

```c
ma_waveform waveform;
ma_waveform_config config = ma_waveform_config_init(ma_format_f32, 2, 48000, ma_waveform_type_sine, 0.1, 440.0);
ma_waveform_init(&config, &waveform);

// Чтение кадров
float buffer[1024 * 2];
ma_uint64 framesRead;
ma_waveform_read_pcm_frames(&waveform, buffer, 1024, &framesRead);
```

---

## ma_noise

Генератор шума.

```c
ma_noise_config ma_noise_config_init(ma_format format, ma_uint32 channels, ma_noise_type type, ma_int32 seed, double amplitude);
ma_result ma_noise_init(const ma_noise_config* pConfig, ma_noise* pNoise);
void ma_noise_uninit(ma_noise* pNoise);

ma_result ma_noise_read_pcm_frames(ma_noise* pNoise, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
```

### Типы шума

```c
typedef enum {
    ma_noise_type_white,   // Белый шум
    ma_noise_type_pink,    // Розовый шум
    ma_noise_type_brownian // Броуновский шум
} ma_noise_type;
```

---

## Создание кастомного data source

Для создания своего источника данных нужно реализовать интерфейс `ma_data_source`.

### Структура

```c
typedef struct {
    ma_data_source_base base;  // Базовая структура (обязательно первой)
    // Ваши данные
    const float* pData;
    ma_uint64 frameCount;
    ma_uint64 cursor;
    ma_uint32 channels;
    ma_uint32 sampleRate;
} my_data_source;
```

### Реализация callbacks

```c
static ma_result my_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
    my_data_source* pSrc = (my_data_source*)pDataSource;
    ma_uint64 framesToRead = frameCount;

    if (pSrc->cursor + framesToRead > pSrc->frameCount) {
        framesToRead = pSrc->frameCount - pSrc->cursor;
    }

    size_t bytesPerFrame = sizeof(float) * pSrc->channels;
    memcpy(pFramesOut, pSrc->pData + pSrc->cursor * pSrc->channels, (size_t)(framesToRead * bytesPerFrame));

    pSrc->cursor += framesToRead;
    if (pFramesRead) *pFramesRead = framesToRead;

    return (framesToRead > 0) ? MA_SUCCESS : MA_AT_END;
}

static ma_result my_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
    my_data_source* pSrc = (my_data_source*)pDataSource;
    if (frameIndex > pSrc->frameCount) return MA_INVALID_ARGS;
    pSrc->cursor = frameIndex;
    return MA_SUCCESS;
}

static ma_result my_data_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength) {
    my_data_source* pSrc = (my_data_source*)pDataSource;
    *pLength = pSrc->frameCount;
    return MA_SUCCESS;
}

static ma_result my_data_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor) {
    my_data_source* pSrc = (my_data_source*)pDataSource;
    *pCursor = pSrc->cursor;
    return MA_SUCCESS;
}
```

### Инициализация

```c
ma_result my_data_source_init(my_data_source* pSrc, const float* pData, ma_uint64 frameCount, ma_uint32 channels, ma_uint32 sampleRate) {
    ma_data_source_config baseConfig = ma_data_source_config_init();
    baseConfig.vtable = &g_my_data_source_vtable;  // Таблица виртуальных функций

    ma_result result = ma_data_source_init(&baseConfig, &pSrc->base);
    if (result != MA_SUCCESS) return result;

    pSrc->pData = pData;
    pSrc->frameCount = frameCount;
    pSrc->cursor = 0;
    pSrc->channels = channels;
    pSrc->sampleRate = sampleRate;

    return MA_SUCCESS;
}

// Таблица виртуальных функций
static ma_data_source_vtable g_my_data_source_vtable = {
    my_data_source_read,
    my_data_source_seek,
    my_data_source_get_length,
    my_data_source_get_cursor,
    NULL  // get_available_frames (опционально)
};
```

### Использование с ma_sound

```c
my_data_source mySource;
my_data_source_init(&mySource, audioData, frameCount, 2, 48000);

ma_sound sound;
ma_sound_init_from_data_source(&engine, &mySource, 0, NULL, &sound);
ma_sound_start(&sound);

---

## Продвинутые темы

<!-- anchor: 05_advanced -->

**🔴 Уровень 3: Продвинутый**

Node Graph, Spatial Audio, Custom Decoders, Resource Manager.

---

## Node Graph

Node Graph — система для построения сложных аудио-цепочек: микширование, эффекты, маршрутизация.

### Основные понятия

- **Node** — узел графа (звук, эффект, микшер)
- **Input bus** — входы узла (куда приходят данные)
- **Output bus** — выходы узла (откуда уходят данные)
- **Endpoint** — финальный узел, подключённый к устройству

### Структура ma_node_graph

```c
ma_node_graph_config ma_node_graph_config_init(ma_uint32 channels);
ma_result ma_node_graph_init(const ma_node_graph_config* pConfig, ma_allocation_callbacks* pAllocationCallbacks, ma_node_graph* pNodeGraph);
void ma_node_graph_uninit(ma_node_graph* pNodeGraph);

// Чтение данных из графа
ma_result ma_node_graph_read_pcm_frames(ma_node_graph* pNodeGraph, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);

// Доступ к endpoint
ma_node* ma_node_graph_get_endpoint(ma_node_graph* pNodeGraph);
```

### Структура ma_node

```c
ma_node_config ma_node_config_init();
ma_result ma_node_init(ma_node_graph* pNodeGraph, const ma_node_config* pConfig, ma_node* pNode);
void ma_node_uninit(ma_node* pNode);

// Подключение узлов
ma_result ma_node_attach_output_bus(ma_node* pNode, ma_uint32 outputBusIndex, ma_node* pOtherNode, ma_uint32 otherNodeInputBusIndex);
void ma_node_detach_output_bus(ma_node* pNode, ma_uint32 outputBusIndex);

// Управление
void ma_node_set_volume(ma_node* pNode, float volume);
float ma_node_get_volume(ma_node* pNode);
void ma_node_set_enabled(ma_node* pNode, ma_bool32 isEnabled);
```

### Пример: простая цепочка

```c
// Создание node graph
ma_node_graph_config graphConfig = ma_node_graph_config_init(2);  // Стерео
ma_node_graph nodeGraph;
ma_node_graph_init(&graphConfig, NULL, &nodeGraph);

// Звук подключается к endpoint автоматически при создании через ma_sound
// Для кастомных узлов:
ma_node_attach_output_bus(&myEffectNode, 0, ma_node_graph_get_endpoint(&nodeGraph), 0);
```

---

## Spatial Audio

miniaudio поддерживает 3D позиционирование звуков.

### Listener

Listener — "слушатель" в 3D пространстве. По умолчанию один listener (индекс 0).

```c
// Установка позиции listener
ma_engine_listener_set_position(&engine, 0, x, y, z);

// Установка направления (вперёд)
ma_engine_listener_set_direction(&engine, 0, forwardX, forwardY, forwardZ);

// Установка "вверх" для ориентации
ma_engine_listener_set_world_up(&engine, 0, upX, upY, upZ);

// Направленный listener (cone)
ma_engine_listener_set_cone(&engine, 0, innerAngle, outerAngle, outerGain);
```

### Звук в пространстве

```c
ma_sound sound;
ma_sound_init_from_file(&engine, "explosion.wav", 0, NULL, NULL, &sound);

// Позиция звука
ma_sound_set_position(&sound, 10.0f, 0.0f, 5.0f);

// Направленный звук
ma_sound_set_direction(&sound, 1.0f, 0.0f, 0.0f);
ma_sound_set_cone(&sound, MA_PI/4, MA_PI/2, 0.5f);

// Скорость для эффекта Доплера
ma_sound_set_velocity(&sound, vx, vy, vz);

// Модель затухания
ma_sound_set_attenuation_model(&sound, ma_attenuation_model_inverse);
ma_sound_set_min_distance(&sound, 1.0f);
ma_sound_set_max_distance(&sound, 100.0f);
ma_sound_set_rolloff(&sound, 1.0f);

ma_sound_start(&sound);
```

### Модели затухания

```c
typedef enum {
    ma_attenuation_model_none,       // Нет затухания
    ma_attenuation_model_inverse,    // 1/distance (реалистичное)
    ma_attenuation_model_linear,     // Линейное
    ma_attenuation_model_exponential // Экспоненциальное
} ma_attenuation_model;
```

### Coordinate system

miniaudio использует правую систему координат:

- **+X** — вправо
- **+Y** — вверх
- **+Z** — назад (к слушателю)

Для смены системы координат используйте `ma_engine_listener_set_world_up`.

---

## Custom Decoders

Для форматов без встроенной поддержки (Vorbis, Opus, AAC).

### Подход 1: Реализация ma_data_source

Создайте структуру, первым полем которой является `ma_data_source_base`. Реализуйте vtable с функциями чтения, seek,
получения длины.

```c
typedef struct {
    ma_data_source_base base;
    YourDecoder decoder;
    // ...
} my_format_decoder;

static ma_data_source_vtable g_my_vtable = {
    my_read_pcm_frames,
    my_seek_to_pcm_frame,
    my_get_length_in_pcm_frames,
    my_get_cursor_in_pcm_frames,
    NULL  // get_available_frames
};
```

### Подход 2: Использование ma_decoder_init с callbacks

```c
typedef struct {
    ma_decoder decoder;
    YourDecoderState* pState;
} my_decoder_wrapper;

ma_result my_on_read(ma_decoder* pDecoder, void* pBuffer, size_t bytesToRead, size_t* pBytesRead) {
    my_decoder_wrapper* pWrapper = (my_decoder_wrapper*)pDecoder;
    // Чтение из pWrapper->pState
}

ma_result my_on_seek(ma_decoder* pDecoder, ma_int64 offset, ma_seek_origin origin) {
    my_decoder_wrapper* pWrapper = (my_decoder_wrapper*)pDecoder;
    // Seek в pWrapper->pState
}
```

---

## Resource Manager

Resource manager управляет загрузкой, кэшированием и выгрузкой аудиоресурсов.

### Конфигурация

```c
ma_resource_manager_config config = ma_resource_manager_config_init();

// Формат по умолчанию
config.decodedFormat = ma_format_f32;
config.decodedChannels = 2;
config.decodedSampleRate = 48000;

// Ограничения памяти
config.decodedBufferCap = 0;  // 0 = без ограничения

// Функции для работы с файлами
config.pVFS = NULL;  // Виртуальная файловая система

ma_resource_manager resourceManager;
ma_resource_manager_init(&config, &resourceManager);
```

### Использование с engine

```c
ma_engine_config engineConfig = ma_engine_config_init();
engineConfig.pResourceManager = &resourceManager;

ma_engine engine;
ma_engine_init(&engineConfig, &engine);
```

### Асинхронная загрузка

```c
// Создание fence для ожидания
ma_fence fence;
ma_fence_init(&fence);

ma_sound sound;
ma_sound_init_from_file(&engine, "big_sound.wav", MA_SOUND_FLAG_ASYNC, NULL, &fence, &sound);

// ... делать другие вещи ...

// Ожидание завершения загрузки
ma_fence_wait(&fence);
ma_fence_uninit(&fence);

ma_sound_start(&sound);
```

---

## Эффекты и фильтры

miniaudio включает базовые эффекты.

### Biquad Filter (EQ)

```c
ma_biquad_config config = ma_biquad_config_init(ma_format_f32, 2, b0, b1, b2, a0, a1, a2);
ma_biquad biquad;
ma_biquad_init(&config, &biquad);

// Применение
ma_biquad_process_pcm_frames(&biquad, pFramesOut, pFramesIn, frameCount);
```

### Predefined filters

```c
// Low-pass
ma_lpf_config lpfConfig = ma_lpf_config_init(ma_format_f32, 2, 48000, 2000, 0);
ma_lpf lpf;
ma_lpf_init(&lpfConfig, &lpf);

// High-pass
ma_hpf_config hpfConfig = ma_hpf_config_init(ma_format_f32, 2, 48000, 200, 0);
ma_hpf hpf;
ma_hpf_init(&hpfConfig, &hpf);

// Band-pass
ma_bpf_config bpfConfig = ma_bpf_config_init(ma_format_f32, 2, 48000, 1000, 2.0);
ma_bpf bpf;
ma_bpf_init(&bpfConfig, &bpf);

// Notch
ma_notch_config notchConfig = ma_notch_config_init(ma_format_f32, 2, 48000, 60, 10);
ma_notch notch;
ma_notch_init(&notchConfig, &notch);

// Peaking EQ
ma_peak_config peakConfig = ma_peak_config_init(ma_format_f32, 2, 48000, 1000, 2.0, 6.0);
ma_peak peak;
ma_peak_init(&peakConfig, &peak);
```

### Пример: применение фильтра в callback

```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    my_state* pState = (my_state*)pDevice->pUserData;

    // Чтение из data source
    ma_data_source_read_pcm_frames(pState->pDataSource, pOutput, frameCount, NULL);

    // Применение фильтра
    ma_lpf_process_pcm_frames(&pState->lpf, pOutput, pOutput, frameCount);
}
```

---

## Resampling

miniaudio поддерживает передискретизацию (resampling).

### ma_resampler

```c
ma_resampler_config config = ma_resampler_config_init(
    ma_format_f32,
    2,
    44100,   // Входная частота
    48000,   // Выходная частота
    ma_resample_algorithm_linear  // Алгоритм
);

ma_resampler resampler;
ma_resampler_init(&config, &resampler);

// Передискретизация
ma_uint64 framesIn = 1024;
ma_uint64 framesOut;
ma_resampler_process_pcm_frames(&resampler, pInput, &framesIn, pOutput, &framesOut);
```

### Алгоритмы resampling

```c
typedef enum {
    ma_resample_algorithm_linear,    // Линейный (быстрый)
    ma_resample_algorithm_speex      // Speex (качественный)
} ma_resample_algorithm;
```

---

## Data Conversion

Конвертация форматов, каналов, sample rate.

### Формат

```c
ma_pcm_format_converter_config config = ma_pcm_format_converter_config_init(
    ma_format_f32,  // Выходной формат
    ma_format_s16,  // Входной формат
    2               // Каналы
);

ma_pcm_format_converter converter;
ma_pcm_format_converter_init(&config, &converter);

ma_pcm_format_converter_process_pcm_frames(&converter, pOut, pIn, frameCount);
```

### Каналы (channel converter)

```c
ma_channel_converter_config config = ma_channel_converter_config_init(
    ma_format_f32,
    2,              // Входные каналы
    inChannelMap,
    6,              // Выходные каналы (5.1)
    outChannelMap,
    ma_channel_mix_mode_default
);

ma_channel_converter converter;
ma_channel_converter_init(&config, &converter);
```

---

## Channel Maps

Стандартные channel maps:

```c
// Стерео
MA_CHANNEL_FRONT_LEFT
MA_CHANNEL_FRONT_RIGHT

// 5.1
MA_CHANNEL_FRONT_LEFT
MA_CHANNEL_FRONT_RIGHT
MA_CHANNEL_FRONT_CENTER
MA_CHANNEL_LFE
MA_CHANNEL_BACK_LEFT
MA_CHANNEL_BACK_RIGHT

// 7.1
MA_CHANNEL_FRONT_LEFT
MA_CHANNEL_FRONT_RIGHT
MA_CHANNEL_FRONT_CENTER
MA_CHANNEL_LFE
MA_CHANNEL_SIDE_LEFT
MA_CHANNEL_SIDE_RIGHT
MA_CHANNEL_BACK_LEFT
MA_CHANNEL_BACK_RIGHT
```

### Получение стандартных карт

```c
ma_channel_map standardMap;
ma_channel_map_init_standard(ma_standard_channel_map_default, standardMap, 0, channelCount);
```

---

## Pan

Панорамирование для стерео.

```c
// Панорамирование: -1 (только левый) .. 0 (центр) .. +1 (только правый)
ma_pan_config config = ma_pan_config_init(ma_format_f32, 2, 0.5f);
ma_pan pan;
ma_pan_init(&config, &pan);

ma_pan_process_pcm_frames(&pan, pFramesOut, pFramesIn, frameCount);
```

---

## Offline Rendering

Рендеринг без устройства (например, для экспорта).

### Конфигурация engine без устройства

```c
ma_engine_config config = ma_engine_config_init();
config.noDevice = MA_TRUE;
config.format = ma_format_f32;
config.channels = 2;
config.sampleRate = 48000;

ma_engine engine;
ma_engine_init(&config, &engine);

// Загрузка и воспроизведение звуков
ma_sound sound;
ma_sound_init_from_file(&engine, "music.wav", 0, NULL, NULL, &sound);
ma_sound_start(&sound);

// Чтение данных
float buffer[1024 * 2];
ma_uint64 framesRead;
ma_engine_read_pcm_frames(&engine, buffer, 1024, &framesRead);

// Запись в файл через ma_encoder...

---

## Решение проблем

<!-- anchor: 06_troubleshooting -->

**🟡 Уровень 2: Средний**

Диагностика и исправление ошибок для всех платформ.

---

## Общие проблемы

### MA_NO_BACKEND

**Симптом:** `ma_device_init` или `ma_context_init` возвращает `MA_NO_BACKEND`.

**Причины и решения:**

1. **Нет аудио драйверов**
  - Windows: установите драйверы звуковой карты или проверьте WASAPI/DirectSound
  - Linux: проверьте ALSA (`aplay -l`) или PulseAudio (`pactl info`)
  - macOS: проверьте Core Audio

2. **Бэкенд явно отключен макросами**
   ```c
   // Проверьте, не определены ли:
   // MA_ENABLE_ONLY_SPECIFIC_BACKENDS
   // или MA_NO_WASAPI, MA_NO_DSOUND и т.д.
   ```

3. **Неправильный порядок бэкендов**
   ```c
   // Явно укажите бэкенды:
   ma_backend backends[] = { ma_backend_wasapi, ma_backend_dsound };
   ma_context_init(NULL, 2, backends, &context);
   ```

---

### MA_NO_DEVICE

**Симптом:** `MA_NO_DEVICE` при инициализации устройства.

**Причины и решения:**

1. **Устройство отключено или не существует**

- Проверьте список устройств через `ma_context_get_devices`

2. **Неверный Device ID**
   ```c
   // Убедитесь, что ID валиден:
   if (pPlaybackInfos[index].isDefault) {
       config.playback.pDeviceID = NULL;  // NULL = default
   } else {
       config.playback.pDeviceID = &pPlaybackInfos[index].id;
   }
   ```

---

### MA_DEVICE_IN_USE

**Симптом:** Устройство занято другим приложением.

**Решения:**

1. **Используйте shared mode (по умолчанию)**
   ```c
   config.playback.shareMode = ma_share_mode_shared;
   ```

2. **Попробуйте другое устройство**

3. **Закройте приложения, использующие аудио**

---

### MA_FORMAT_NOT_SUPPORTED

**Симптом:** Формат не поддерживается устройством.

**Решения:**

1. **Используйте ma_format_f32 или ma_format_s16**
   ```c
   config.playback.format = ma_format_f32;  // Обычно поддерживается
   ```

2. **Декодируйте в поддерживаемый формат**
   ```c
   ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);
   ```

---

### Нет звука

**Симптом:** Ошибок нет, но звука не слышно.

**Диагностика:**

1. **Устройство запущено?**
   ```c
   if (!ma_device_is_started(&device)) {
       ma_device_start(&device);
   }
   ```

2. **Callback вызывается?**
   ```c
   void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
       static int callCount = 0;
       callCount++;  // Поставьте breakpoint или лог
       // ...
   }
   ```

3. **Данные не нулевые?**
   ```c
   // Проверьте, что буфер заполняется:
   for (ma_uint32 i = 0; i < frameCount * channels; i++) {
       ((float*)pOutput)[i] = 0.1f;  // Тестовый тон
   }
   ```

4. **Громкость в системе**

- Проверьте микшер Windows / PulseAudio / Core Audio
- Проверьте, не muted ли приложение

5. **Громкость в miniaudio**
   ```c
   ma_sound_set_volume(&sound, 1.0f);  // Полная громкость
   ma_sound_group_set_volume(&group, 1.0f);
   ```

---

## Платформенные проблемы

### Windows

#### WASAPI exclusive mode зависает

**Решение:** Используйте shared mode или корректно обрабатывайте переключение:

```c
config.playback.shareMode = ma_share_mode_shared;
```

#### WinMM: высокий latency

**Решение:** Используйте WASAPI или DirectSound:

```c
ma_backend backends[] = { ma_backend_wasapi, ma_backend_dsound };
```

#### Проблемы с Unicode путями

**Решение:** Используйте широкие символы:

```c
ma_decoder_init_file_w(L"путь/к/файлу.wav", NULL, &decoder);
```

---

### Linux

#### Нет звука в ALSA

**Диагностика:**

```bash
aplay -l              # Список устройств
aplay test.wav        # Тест ALSA напрямую
```

**Решения:**

1. Проверьте права доступа к `/dev/snd/*`
2. Добавьте пользователя в группу `audio`
3. Используйте PulseAudio:
   ```c
   ma_backend backends[] = { ma_backend_pulseaudio, ma_backend_alsa };
   ```

#### PulseAudio: устройство занято

**Решение:**

```bash
pulseaudio -k         # Перезапуск PulseAudio
```

#### JACK: не может подключиться

**Решения:**

1. Убедитесь, что JACK сервер запущен
2. Используйте ALSA напрямую или через PulseAudio bridge

#### Ошибки линковки

```
undefined reference to `pthread_create'
undefined reference to `dlopen'
```

**Решение:** Добавьте библиотеки:

```cmake
target_link_libraries(YourApp PRIVATE pthread dl m)
```

---

### macOS / iOS

#### Нет звука на macOS

**Проверки:**

1. System Preferences → Sound → Output
2. Разрешения приложения (Microphone для capture)

#### iOS: AVAudioSession

**Важно:** На iOS нужно настроить AVAudioSession:

```objc
#import <AVFoundation/AVFoundation.h>

AVAudioSession* session = [AVAudioSession sharedInstance];
[session setCategory:AVAudioSessionCategoryPlayback error:nil];
[session setActive:YES error:nil];
```

#### iOS: нет звука в фоне

**Решение:** Включите background audio в Info.plist:

```xml
<key>UIBackgroundModes</key>
<array>
    <string>audio</string>
</array>
```

---

### Android

#### OpenSL|ES: не работает на новых Android

**Решение:** Используйте AAudio (Android 8.0+):

```c
ma_backend backends[] = { ma_backend_aaudio, ma_backend_opensl };
```

#### Нет разрешения на запись

**Решение:** Добавьте в AndroidManifest.xml:

```xml
<uses-permission android:name="android.permission.RECORD_AUDIO" />
<uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS" />
```

#### Проблемы с потоком

На Android callback может вызываться из Java-потока. Убедитесь, что callback не блокируется.

---

### Emscripten / Web

#### Не работает в браузере

**Требования:**

1. User interaction для разблокировки аудио
2. HTTPS или localhost для некоторых функций

**Решение:**

```javascript
// Разблокировка после клика
document.addEventListener('click', function() {
    // Воспроизвести звук через miniaudio
}, { once: true });
```

#### Проблемы с размером буфера

Web Audio может требовать определённые размеры буферов. Используйте auto настройки.

---

## Проблемы производительности

### Glitches / Dropouts

**Причины:**

1. Слишком большой период обработки
2. Блокирующие операции в callback
3. Недостаточный приоритет потока

**Решения:**

1. **Уменьшите период**
   ```c
   config.periodSizeInFrames = 256;  // Меньше = ниже latency, но больше CPU
   ```

2. **Не блокируйте в callback**
   ```c
   // ПЛОХО:
   void data_callback(...) {
       malloc(...);      // Блокирует!
       file_read(...);   // Блокирует!
       mutex_lock(...);  // Блокирует!
   }

   // ХОРОШО:
   void data_callback(...) {
       // Только обработка данных
   }
   ```

3. **Повысить приоритет потока**
   ```c
   ma_context_config contextConfig = ma_context_config_init();
   contextConfig.threadPriority = ma_thread_priority_realtime;
   ```

---

### Высокое потребление CPU

**Диагностика:**

1. Профилируйте callback
2. Проверьте количество активных звуков

**Решения:**

1. **Используйте High-level API** вместо ручного микширования
2. **Остановите неиспользуемые звуки**
   ```c
   ma_sound_stop(&sound);
   ```
3. **Уменьшите sample rate**
   ```c
   config.sampleRate = 44100;  // Вместо 48000
   ```

---

### Утечки памяти

**Проверьте:**

1. Все `ma_xxx_init` имеют парные `ma_xxx_uninit`
2. Звуки, созданные через `ma_engine_play_sound`, управляются автоматически
3. Звуки, созданные через `ma_sound_init_from_file`, требуют `ma_sound_uninit`

```c
// Не забывайте:
ma_sound_uninit(&sound);
ma_engine_uninit(&engine);
ma_device_uninit(&device);
ma_decoder_uninit(&decoder);
```

---

## Отладка

### Логирование

```c
void log_callback(void* pUserData, ma_uint32 level, const char* pMessage) {
    printf("[miniaudio] %s\n", pMessage);
}

ma_context_config config = ma_context_config_init();
config.logCallback = log_callback;
```

### Проверка callback

```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Занулить буфер в начале (если noPreZeroedOutputBuffer = false, это уже сделано)
    // memset(pOutput, 0, frameCount * bytesPerFrame);

    // Ваш код...

    // Проверка на clipping
    float* pOut = (float*)pOutput;
    for (ma_uint32 i = 0; i < frameCount * channels; i++) {
        if (pOut[i] > 1.0f) pOut[i] = 1.0f;
        if (pOut[i] < -1.0f) pOut[i] = -1.0f;
    }
}
```

### Тестовый тон

Если нет звука, попробуйте сгенерировать простой тон:

```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    static float phase = 0.0f;
    float* pOut = (float*)pOutput;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        float sample = 0.1f * sinf(phase);
        pOut[i * 2] = sample;
        pOut[i * 2 + 1] = sample;

        phase += 2.0f * 3.14159f * 440.0f / pDevice->sampleRate;
        if (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;
    }
}
```

---

## Частые ошибки

### Копирование структур

```c
// ОШИБКА: Копирование структуры
ma_sound sound1;
ma_sound_init_from_file(&engine, "test.wav", 0, NULL, NULL, &sound1);
ma_sound sound2 = sound1;  // ОШИБКА! Указатель внутри невалиден

// ПРАВИЛЬНО: Используйте ma_sound_init_copy
ma_sound sound2;
ma_sound_init_copy(&engine, &sound1, 0, NULL, &sound2);
```

### Инициализация в callback

```c
// ОШИБКА: Инициализация в callback
void data_callback(...) {
    ma_device_start(&device);  // DEADLOCK!
}

// ПРАВИЛЬНО: Сигнализация из callback
volatile bool shouldStart = false;
void data_callback(...) {
    shouldStart = true;
}
// В другом потоке:
if (shouldStart) ma_device_start(&device);
```

### Неправильный формат декодера

```c
// ОШИБКА: Формат не совпадает с устройством
ma_decoder_init_file("sound.wav", NULL, &decoder);  // Format может быть любым
config.playback.format = decoder.outputFormat;  // Может не поддерживаться!

// ПРАВИЛЬНО: Явно укажите формат декодера
ma_decoder_config decConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);
ma_decoder_init_file("sound.wav", &decConfig, &decoder);
