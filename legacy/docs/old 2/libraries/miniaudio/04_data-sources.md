# Источники данных

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
