# Справочник API miniaudio

**🟡 Уровень 2: Средний**

Краткое описание основных функций и структур miniaudio с примерами использования. Полный
перечень — [miniaudio.h](../../external/miniaudio/miniaudio.h), [официальная документация miniaud.io](https://miniaud.io/docs).

## На этой странице

- [Когда что использовать](#когда-что-использовать)
- [ma_result](#ma_result)
- [ma_context](#ma_context)
- [ma_device](#ma_device)
- [ma_engine](#ma_engine)
- [ma_decoder](#ma_decoder)
- [ma_encoder](#ma_encoder)
- [ma_sound](#ma_sound)
- [ma_data_source](#ma_data_source)
- [Форматы и типы](#форматы-и-типы)
- [См. также](#см-также)

---

## Когда что использовать

| Задача                       | Функция / API                                                | Раздел                    |
|------------------------------|--------------------------------------------------------------|---------------------------|
| Перечислить устройства       | `ma_context_get_devices`                                     | [ma_context](#ma_context) |
| Воспроизведение (Low-level)  | `ma_device_config_init`, `ma_device_init`, `ma_device_start` | [ma_device](#ma_device)   |
| Воспроизведение (High-level) | `ma_engine_init`, `ma_engine_play_sound`                     | [ma_engine](#ma_engine)   |
| Управление экземпляром звука | `ma_sound_init_from_file`, `ma_sound_start`, `ma_sound_stop` | [ma_sound](#ma_sound)     |
| Декодирование файла          | `ma_decoder_init_file`, `ma_decoder_read_pcm_frames`         | [ma_decoder](#ma_decoder) |
| Запись capture в WAV         | `ma_encoder_init_file`, `ma_encoder_write_pcm_frames`        | [ma_encoder](#ma_encoder) |
| Описание ошибки              | `ma_result_description(result)`                              | [ma_result](#ma_result)   |

---

## ma_result

Коды результата. При успехе — `MA_SUCCESS`. При ошибке — один из:

### ma_result_description

```c
const char* ma_result_description(ma_result result);
```

Возвращает строку с описанием результата. Полезно для логирования:
`printf("Error: %s\n", ma_result_description(result));`

### Основные коды

| Код                                 | Описание                                                 |
|-------------------------------------|----------------------------------------------------------|
| `MA_SUCCESS`                        | Успех                                                    |
| `MA_INVALID_ARGS`                   | Невалидные аргументы                                     |
| `MA_INVALID_OPERATION`              | Недопустимая операция                                    |
| `MA_NO_MEMORY`                      | Недостаточно памяти                                      |
| `MA_AT_END`                         | Достигнут конец данных (data source, прочитано 0 frames) |
| `MA_TIMEOUT`                        | Таймаут                                                  |
| `MA_DEVICE_IN_USE`                  | Устройство занято                                        |
| `MA_DEVICE_NOT_INITIALIZED`         | Устройство не инициализировано                           |
| `MA_DEVICE_ALREADY_INITIALIZED`     | Устройство уже инициализировано                          |
| `MA_FORMAT_NOT_SUPPORTED`           | Формат не поддерживается                                 |
| `MA_NO_BACKEND`                     | Бэкенд не найден                                         |
| `MA_NO_DEVICE`                      | Устройство не найдено                                    |
| `MA_FAILED_TO_INIT_BACKEND`         | Не удалось инициализировать бэкенд                       |
| `MA_FAILED_TO_OPEN_BACKEND_DEVICE`  | Не удалось открыть устройство бэкенда                    |
| `MA_FAILED_TO_START_BACKEND_DEVICE` | Не удалось запустить устройство бэкенда                  |

---

## ma_context

### ma_context_init

```c
ma_result ma_context_init(const ma_backend backends[], ma_uint32 backendCount,
    const ma_context_config* pConfig, ma_context* pContext);
```

Инициализирует контекст. `backends` = NULL — приоритеты по умолчанию. `pConfig` = NULL — настройки по умолчанию.

### ma_context_uninit

```c
ma_result ma_context_uninit(ma_context* pContext);
```

Освобождает контекст.

### ma_context_get_devices

```c
ma_result ma_context_get_devices(ma_context* pContext,
    ma_device_info** ppPlaybackDeviceInfos, ma_uint32* pPlaybackCount,
    ma_device_info** ppCaptureDeviceInfos, ma_uint32* pCaptureCount);
```

Возвращает списки устройств playback и capture. Буферы принадлежат miniaudio — не освобождать. `ma_device_info.id`
передаётся в `config.playback.pDeviceID` или `config.capture.pDeviceID`.

### ma_context_config_init

```c
ma_context_config ma_context_config_init(void);
```

Инициализирует config контекста с значениями по умолчанию.

### ma_context_enumerate_devices

```c
ma_result ma_context_enumerate_devices(ma_context* pContext,
    ma_enum_devices_callback_proc callback, void* pUserData);
```

Перечисление устройств через callback. Не создаёт heap-аллокацию в отличие от `ma_context_get_devices`.

### Приоритеты бэкендов (кратко)

| ОС         | Порядок бэкендов             |
|------------|------------------------------|
| Windows    | WASAPI → DirectSound → WinMM |
| macOS/iOS  | Core Audio                   |
| Linux      | ALSA → PulseAudio → JACK     |
| OpenBSD    | sndio                        |
| FreeBSD    | OSS                          |
| Android    | AAudio → OpenSL\|ES          |
| Emscripten | Web Audio                    |

---

## ma_device

### ma_device_config_init

```c
ma_device_config ma_device_config_init(ma_device_type deviceType);
```

Создаёт config устройства. `deviceType`: `ma_device_type_playback`, `ma_device_type_capture`, `ma_device_type_duplex`,
`ma_device_type_loopback` (WASAPI).

### ma_device_init

```c
ma_result ma_device_init(ma_context* pContext, const ma_device_config* pConfig, ma_device* pDevice);
```

Инициализирует устройство. `pContext` = NULL — miniaudio создаёт контекст автоматически (тогда нельзя перечислять
устройства). Для выбора устройства передавайте явный context.

### ma_device_uninit

```c
void ma_device_uninit(ma_device* pDevice);
```

Освобождает устройство. Останавливает его, если было запущено.

### ma_device_start

```c
ma_result ma_device_start(ma_device* pDevice);
```

Запускает воспроизведение/захват. Не вызывать из callback.

### ma_device_stop

```c
ma_result ma_device_stop(ma_device* pDevice);
```

Останавливает устройство. Не вызывать из callback.

### ma_device_config (ключевые поля)

| Поле                                                      | Описание                                                                                 |
|-----------------------------------------------------------|------------------------------------------------------------------------------------------|
| `playback.format`                                         | `ma_format_f32`, `ma_format_s16`, ... или `ma_format_unknown` (формат устройства)        |
| `playback.channels`                                       | Число каналов, 0 = native                                                                |
| `playback.pDeviceID`                                      | ID устройства (из ma_device_info) или NULL = default                                     |
| `capture.format`, `capture.channels`, `capture.pDeviceID` | Аналогично для capture                                                                   |
| `sampleRate`                                              | 44100, 48000 и т.д., 0 = native                                                          |
| `periodSizeInFrames`                                      | Размер period в PCM frames. 0 — использовать periodSizeInMilliseconds.                   |
| `periodSizeInMilliseconds`                                | Размер period в мс. Влияет на latency.                                                   |
| `periods`                                                 | Число periods в буфере.                                                                  |
| `performanceProfile`                                      | `ma_performance_profile_low_latency` (default) или `ma_performance_profile_conservative` |
| `dataCallback`                                            | Callback для передачи/получения данных                                                   |
| `notificationCallback`                                    | Опциональный callback при start/stop/reroute                                             |
| `pUserData`                                               | Указатель, доступный в callback как `pDevice->pUserData`                                 |
| `noPreSilencedOutputBuffer`                               | true — не обнулять pOutput перед callback (экономия CPU)                                 |
| `noClip`                                                  | true — не клиповать f32 после callback                                                   |
| `noFixedSizedCallback`                                    | true — frameCount может меняться в callback                                              |

### Типы устройств

| Тип                       | Callback: pOutput | pInput          |
|---------------------------|-------------------|-----------------|
| `ma_device_type_playback` | Записывать        | NULL            |
| `ma_device_type_capture`  | NULL              | Читать          |
| `ma_device_type_duplex`   | Записывать        | Читать          |
| `ma_device_type_loopback` | NULL              | Читать (WASAPI) |

---

## ma_engine

### ma_engine_init

```c
ma_result ma_engine_init(const ma_engine_config* pConfig, ma_engine* pEngine);
```

Инициализирует engine. `pConfig` = NULL — настройки по умолчанию. Engine включает device, resource manager и node graph.

### ma_engine_uninit

```c
void ma_engine_uninit(ma_engine* pEngine);
```

Освобождает engine.

### ma_engine_play_sound

```c
ma_result ma_engine_play_sound(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup);
```

Воспроизводит файл один раз (inline sound). `pGroup` = NULL — default group.

### ma_engine_get_device

```c
ma_device* ma_engine_get_device(ma_engine* pEngine);
```

Возвращает внутренний device.

### ma_engine_get_resource_manager

```c
ma_resource_manager* ma_engine_get_resource_manager(ma_engine* pEngine);
```

Возвращает resource manager.

---

## ma_decoder

### ma_decoder_init_file

```c
ma_result ma_decoder_init_file(const char* pFilePath,
    const ma_decoder_config* pConfig, ma_decoder* pDecoder);
```

Инициализирует декодер из файла. Поддерживаются WAV, FLAC, MP3. `pConfig` = NULL — настройки по умолчанию.

### ma_decoder_read_pcm_frames

```c
ma_result ma_decoder_read_pcm_frames(ma_decoder* pDecoder, void* pFramesOut,
    ma_uint64 frameCount, ma_uint64* pFramesRead);
```

Читает декодированные PCM frames. `pFramesRead` может быть NULL.

### ma_decoder_uninit

```c
void ma_decoder_uninit(ma_decoder* pDecoder);
```

Освобождает декодер.

### ma_decoder_init / ma_decoder_init_memory

```c
ma_result ma_decoder_init(ma_read_proc onRead, ma_seek_proc onSeek, void* pUserData,
    const ma_decoder_config* pConfig, ma_decoder* pDecoder);
ma_result ma_decoder_init_memory(const void* pData, size_t dataSize,
    const ma_decoder_config* pConfig, ma_decoder* pDecoder);
```

Инициализация из stream (callbacks) или из памяти. Для файла — `ma_decoder_init_file`.

### ma_decoder_config_init

```c
ma_decoder_config ma_decoder_config_init(ma_format outputFormat, ma_uint32 outputChannels, ma_uint32 outputSampleRate);
```

Настройка формата вывода декодера.

### ma_decoder (поля после init)

| Поле               | Описание                    |
|--------------------|-----------------------------|
| `outputFormat`     | ma_format (f32, s16 и т.д.) |
| `outputChannels`   | Число каналов               |
| `outputSampleRate` | Sample rate                 |

---

## ma_encoder

Кодирование PCM в файл. Встроен только WAV.
Пример: [simple_capture.c](../../external/miniaudio/examples/simple_capture.c).

### ma_encoder_config_init

```c
ma_encoder_config ma_encoder_config_init(ma_encoding_format encodingFormat,
    ma_format format, ma_uint32 channels, ma_uint32 sampleRate);
```

### ma_encoder_init_file

```c
ma_result ma_encoder_init_file(const char* pFilePath, const ma_encoder_config* pConfig, ma_encoder* pEncoder);
```

Создаёт выходной файл для записи PCM. Поддерживается только `ma_encoding_format_wav`.

### ma_encoder_write_pcm_frames

```c
ma_result ma_encoder_write_pcm_frames(ma_encoder* pEncoder, const void* pFrames, ma_uint64 frameCount, ma_uint64* pFramesWritten);
```

Записывает PCM frames в файл. `pFramesWritten` может быть NULL.

### ma_encoder_uninit

```c
void ma_encoder_uninit(ma_encoder* pEncoder);
```

Освобождает энкодер и закрывает файл.

---

## ma_sound

### ma_sound_init_from_file

```c
ma_result ma_sound_init_from_file(ma_engine* pEngine, const char* pFilePath,
    ma_uint32 flags, ma_sound_group* pGroup, ma_fence* pFence, ma_sound* pSound);
```

Инициализирует звук из файла. `pGroup` = NULL — default. `pFence` = NULL — без fence.

**Флаги (`flags`):**

| Флаг                                  | Описание                                                     |
|---------------------------------------|--------------------------------------------------------------|
| `MA_SOUND_FLAG_DECODE`                | Декодировать при загрузке (иначе — на лету при микшировании) |
| `MA_SOUND_FLAG_ASYNC`                 | Асинхронная загрузка; init возвращается сразу                |
| `MA_SOUND_FLAG_STREAM`                | Стриминг (не загружать целиком в память)                     |
| `MA_SOUND_FLAG_NO_SPATIALIZATION`     | Отключить 3D-позиционирование                                |
| `MA_SOUND_FLAG_NO_PITCH`              | Оптимизация, если pitch не меняется                          |
| `MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT` | Не подключать к endpoint по умолчанию (для node graph)       |

### ma_sound_start

```c
ma_result ma_sound_start(ma_sound* pSound);
```

Запускает воспроизведение. Звук по умолчанию не запущен.

### ma_sound_stop

```c
ma_result ma_sound_stop(ma_sound* pSound);
```

Останавливает. Не перематывает к началу.

### ma_sound_uninit

```c
void ma_sound_uninit(ma_sound* pSound);
```

Освобождает звук.

### ma_sound_seek_to_pcm_frame

```c
ma_result ma_sound_seek_to_pcm_frame(ma_sound* pSound, ma_uint64 frameIndex);
```

Перематывает к frame. Для перезапуска: `ma_sound_seek_to_pcm_frame(&sound, 0)` затем `ma_sound_start`.

### ma_sound_is_playing / ma_sound_at_end

```c
ma_bool32 ma_sound_is_playing(const ma_sound* pSound);
ma_bool32 ma_sound_at_end(const ma_sound* pSound);
```

Проверка состояния воспроизведения.

### Дополнительные функции ma_sound

| Функция                                 | Описание                        |
|-----------------------------------------|---------------------------------|
| `ma_sound_set_volume`                   | Громкость (0.0–1.0 и выше)      |
| `ma_sound_set_pan`                      | Панорама (-1.0…1.0)             |
| `ma_sound_set_pitch`                    | Высота тона                     |
| `ma_sound_set_looping`                  | Включить/выключить зацикливание |
| `ma_sound_set_fade_in_pcm_frames`       | Плавное появление               |
| `ma_sound_set_start_time_in_pcm_frames` | Запланированное время старта    |
| `ma_sound_set_stop_time_in_pcm_frames`  | Запланированное время остановки |

---

## ma_data_source

Общий интерфейс чтения аудиоданных. Реализуют: `ma_decoder`, `ma_noise`, `ma_waveform`, буферы resource manager.

### Ключевые функции

| Функция                                       | Описание                                       |
|-----------------------------------------------|------------------------------------------------|
| `ma_data_source_read_pcm_frames`              | Читает PCM frames                              |
| `ma_data_source_seek_to_pcm_frame`            | Перемотка к позиции                            |
| `ma_data_source_get_length_in_pcm_frames`     | Длина в frames (не все источники поддерживают) |
| `ma_data_source_get_data_format`              | Формат, каналы, sample rate                    |
| `ma_data_source_set_range_in_pcm_frames`      | Ограничить чтение диапазоном                   |
| `ma_data_source_set_loop_point_in_pcm_frames` | Точки зацикливания                             |
| `ma_data_source_set_next`                     | Цепочка источников (chaining)                  |

---

## Форматы и типы

### ma_format

| Значение            | Описание                       | Диапазон                  |
|---------------------|--------------------------------|---------------------------|
| `ma_format_f32`     | 32-bit float                   | [-1, 1]                   |
| `ma_format_s16`     | 16-bit signed                  | [-32768, 32767]           |
| `ma_format_s24`     | 24-bit signed (tightly packed) | [-8388608, 8388607]       |
| `ma_format_s32`     | 32-bit signed                  | [-2147483648, 2147483647] |
| `ma_format_u8`      | 8-bit unsigned                 | [0, 255]                  |
| `ma_format_unknown` | Использовать формат устройства | —                         |

Все форматы native-endian.

### ma_standard_sample_rate

Перечисление стандартных sample rate: 48000, 44100, 32000, 24000, 22050, 88200, 96000, 176400, 192000, 16000, 11025,
8000, 352800, 384000. Рекомендованный диапазон 8000–384000 Hz.

### MA_MAX_CHANNELS

Константа **254** — максимальное число каналов ([miniaudio.h](../../external/miniaudio/miniaudio.h)). Значение по
умолчанию может быть переопределено с помощью `#define MA_MAX_CHANNELS <значение>` перед включением miniaudio.h.

**Примечание:** Для изменения значения необходимо определить макрос до включения заголовочного файла:

```c
#define MA_MAX_CHANNELS 512  // Пример увеличения максимального числа каналов
#include "miniaudio.h"
```

### data_callback

```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
```

Вызывается асинхронно. Playback: записать в `pOutput` до `frameCount` frames. Capture: прочитать из `pInput`.
Interleaved: для стерео — L,R,L,R,...

---

## См. также

- [Глоссарий](glossary.md) — терминология miniaudio
- [Основные понятия](concepts.md) — архитектура и паттерны
- [Быстрый старт](quickstart.md) — минимальные примеры кода
- [Decision Trees](decision-trees.md) — выбор правильных функций
- [Use Cases](use-cases.md) — практические сценарии
- [Интеграция](integration.md) — настройка сборки
- [Troubleshooting](troubleshooting.md) — решение проблем
- [Официальная документация](https://miniaud.io/docs) — полная документация miniaudio
