# Справочник API

**🟡 Уровень 2: Средний**

Краткий справочник по основным функциям и структурам. Полная документация — в `miniaudio.h`.

---

## ma_result

Все функции возвращают `ma_result`. `MA_SUCCESS` (0) — успех, остальные — ошибки.

### Основные коды ошибок

| Код                            | Описание                           |
|--------------------------------|------------------------------------|
| `MA_SUCCESS`                   | Успех                              |
| `MA_ERROR`                     | Общая ошибка                       |
| `MA_INVALID_ARGS`              | Неверные аргументы                 |
| `MA_INVALID_OPERATION`         | Операция не применима              |
| `MA_NO_BACKEND`                | Бэкенд не найден                   |
| `MA_NO_DEVICE`                 | Устройство не найдено              |
| `MA_ACCESS_DENIED`             | Доступ запрещён                    |
| `MA_DEVICE_IN_USE`             | Устройство занято                  |
| `MA_DEVICE_NOT_INITIALIZED`    | Устройство не инициализировано     |
| `MA_DEVICE_NOT_STARTED`        | Устройство не запущено             |
| `MA_OUT_OF_MEMORY`             | Не хватает памяти                  |
| `MA_OUT_OF_RANGE`              | Значение вне диапазона             |
| `MA_FORMAT_NOT_SUPPORTED`      | Формат не поддерживается           |
| `MA_DEVICE_TYPE_NOT_SUPPORTED` | Тип устройства не поддерживается   |
| `MA_SHARE_MODE_NOT_SUPPORTED`  | Режим разделения не поддерживается |
| `MA_NO_DATA`                   | Нет данных (не ошибка)             |
| `MA_AT_END`                    | Достигнут конец данных             |

### Получение сообщения об ошибке

```c
const char* ma_result_description(ma_result result);
```

---

## ma_format

Форматы PCM данных:

| Формат              | Описание              | Размер  |
|---------------------|-----------------------|---------|
| `ma_format_unknown` | Неизвестен            | —       |
| `ma_format_u8`      | Unsigned 8-bit        | 1 байт  |
| `ma_format_s16`     | Signed 16-bit         | 2 байта |
| `ma_format_s24`     | Signed 24-bit         | 3 байта |
| `ma_format_s32`     | Signed 32-bit         | 4 байта |
| `ma_format_f32`     | Floating point 32-bit | 4 байта |

**Рекомендация:** Используйте `ma_format_f32` для внутренних вычислений и обработки.

---

## ma_context

Контекст — верхнеуровневый объект, представляющий бэкенд. Нужен для перечисления устройств или управления несколькими
устройствами с общим контекстом.

### Основные функции

```c
ma_result ma_context_init(const ma_context_config* pConfig, ma_uint32 backendCount, const ma_backend* pBackends, ma_context* pContext);
ma_result ma_context_uninit(ma_context* pContext);
ma_result ma_context_get_devices(ma_context* pContext, ma_device_info** ppPlaybackInfos, ma_uint32* pPlaybackCount, ma_device_info** ppCaptureInfos, ma_uint32* pCaptureCount);
ma_result ma_context_enumerate_devices(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pUserData);
ma_result ma_context_is_loopback_supported(ma_context* pContext);
```

### Конфигурация

```c
ma_context_config config = ma_context_config_init();
config.logCallback = my_log_callback;  // Кастомный логгер
config.threadPriority = ma_thread_priority_realtime;  // Приоритет потока

ma_context context;
ma_context_init(&config, 0, NULL, &context);
```

---

## ma_device

Устройство — представляет физическое или виртуальное аудиоустройство. Самый важный объект Low-level API.

### Типы устройств

```c
typedef enum {
    ma_device_type_playback,  // Воспроизведение
    ma_device_type_capture,   // Захват
    ma_device_type_duplex,    // Одновременно playback и capture
    ma_device_type_loopback   // Захват системного звука (WASAPI only)
} ma_device_type;
```

### Типы callback'ов

```c
typedef void (*ma_device_callback_proc)(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
typedef void (*ma_device_notification_proc)(ma_device* pDevice, ma_device_notification_type type);
```

### Основные функции

```c
ma_device_config ma_device_config_init(ma_device_type deviceType);
ma_result ma_device_init(ma_context* pContext, const ma_device_config* pConfig, ma_device* pDevice);
ma_result ma_device_init_ex(ma_context* pContext, const ma_device_config* pConfig, ma_device* pDevice);  // Расширенная
void ma_device_uninit(ma_device* pDevice);

ma_result ma_device_start(ma_device* pDevice);
ma_result ma_device_stop(ma_device* pDevice);

ma_bool32 ma_device_is_started(ma_device* pDevice);

ma_uint32 ma_device_get_sample_rate(ma_device* pDevice);
ma_uint32 ma_device_get_period_size_in_frames(ma_device* pDevice);
```

### Конфигурация устройства

```c
ma_device_config config = ma_device_config_init(ma_device_type_playback);

// Playback
config.playback.format        = ma_format_f32;
config.playback.channels      = 2;
config.playback.pDeviceID     = NULL;  // NULL = default device
config.playback.shareMode     = ma_share_mode_shared;  // или _exclusive
config.playback.deviceName    = NULL;

// Capture (для duplex/capture)
config.capture.format         = ma_format_f32;
config.capture.channels       = 1;
config.capture.pDeviceID      = NULL;

// Общее
config.sampleRate             = 48000;
config.periodSizeInFrames     = 0;     // 0 = auto
config.periodSizeInMilliseconds = 0;   // Альтернатива frames
config.periods                = 0;     // 0 = auto
config.performanceProfile     = ma_performance_profile_low_latency;
config.noPreZeroedOutputBuffer = MA_FALSE;  // Не занулять буфер перед callback
config.noClip                 = MA_FALSE;   // Не клиповать после callback

// Callback'и
config.dataCallback           = my_data_callback;
config.notificationCallback   = my_notification_callback;
config.pUserData              = pMyData;
```

### Поля структуры ma_device

```c
struct ma_device {
    ma_context* pContext;
    ma_device_type type;
    ma_uint32 sampleRate;
    // playback/capture info...
    void* pUserData;
    // ... внутренние поля
};
```

---

## ma_engine

Engine — главный объект High-level API. Включает устройство, resource manager, node graph.

### Основные функции

```c
ma_engine_config ma_engine_config_init();
ma_result ma_engine_init(const ma_engine_config* pConfig, ma_engine* pEngine);
void ma_engine_uninit(ma_engine* pEngine);

// Воспроизведение звука "fire and forget"
ma_result ma_engine_play_sound(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup);

// Доступ к внутренним объектам
ma_device* ma_engine_get_device(ma_engine* pEngine);
ma_resource_manager* ma_engine_get_resource_manager(ma_engine* pEngine);
ma_node_graph* ma_engine_get_node_graph(ma_engine* pEngine);

// Управление listener (для spatial audio)
void ma_engine_listener_set_position(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z);
void ma_engine_listener_set_direction(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z);
void ma_engine_listener_set_world_up(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z);
void ma_engine_listener_set_cone(ma_engine* pEngine, ma_uint32 listenerIndex, float innerAngleInRadians, float outerAngleInRadians, float outerGain);
```

### Конфигурация engine

```c
ma_engine_config config = ma_engine_config_init();

config.pDevice              = NULL;  // NULL = engine создаст устройство
config.pResourceManager     = NULL;  // NULL = engine создаст resource manager
config.pContext             = NULL;  // NULL = auto
config.pDeviceUserID        = NULL;  // UserData для устройства

config.listenerCount        = 1;     // Количество listeners для spatial audio
config.defaultSpatializationScale = 1.0f;

// Форматы по умолчанию
config.format               = ma_format_f32;
config.channels             = 2;
config.sampleRate           = 48000;
config.periodSizeInFrames   = 0;     // Auto

config.noDevice             = MA_FALSE;  // True = offline rendering
config.noAutoStart          = MA_FALSE;  // True = не стартовать устройство

ma_engine engine;
ma_engine_init(&config, &engine);
```

---

## ma_sound

Звук — управляемый экземпляр воспроизводимого аудио. Звуки подключаются к node graph.

### Основные функции

```c
// Инициализация
ma_result ma_sound_init_from_file(ma_engine* pEngine, const char* pFilePath, ma_uint32 flags, ma_sound_group* pGroup, ma_fence* pDoneFence, ma_sound* pSound);
ma_result ma_sound_init_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound);
ma_result ma_sound_init_copy(ma_engine* pEngine, const ma_sound* pExistingSound, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound);
void ma_sound_uninit(ma_sound* pSound);

// Управление воспроизведением
ma_result ma_sound_start(ma_sound* pSound);
ma_result ma_sound_stop(ma_sound* pSound);
ma_bool32 ma_sound_is_playing(ma_sound* pSound);
ma_result ma_sound_seek_to_pcm_frame(ma_sound* pSound, ma_uint64 frameIndex);
ma_uint64 ma_sound_get_cursor_in_pcm_frames(ma_sound* pSound);
ma_uint64 ma_sound_get_length_in_pcm_frames(ma_sound* pSound);

// Громкость и панорамирование
void ma_sound_set_volume(ma_sound* pSound, float volume);
float ma_sound_get_volume(ma_sound* pSound);
void ma_sound_set_pan(ma_sound* pSound, float pan);  // -1..+1
float ma_sound_get_pan(ma_sound* pSound);

// Питч
void ma_sound_set_pitch(ma_sound* pSound, float pitch);
float ma_sound_get_pitch(ma_sound* pSound);

// Зацикливание
void ma_sound_set_looping(ma_sound* pSound, ma_bool32 isLooping);
ma_bool32 ma_sound_is_looping(ma_sound* pSound);

// Spatial audio
void ma_sound_set_position(ma_sound* pSound, float x, float y, float z);
void ma_sound_get_position(ma_sound* pSound, float* pX, float* pY, float* pZ);
void ma_sound_set_direction(ma_sound* pSound, float x, float y, float z);
void ma_sound_set_velocity(ma_sound* pSound, float x, float y, float z);
void ma_sound_set_attenuation_model(ma_sound* pSound, ma_attenuation_model model);
void ma_sound_set_positioning(ma_sound* pSound, ma_positioning positioning);
void ma_sound_set_min_distance(ma_sound* pSound, float minDistance);
void ma_sound_set_max_distance(ma_sound* pSound, float maxDistance);
void ma_sound_set_rolloff(ma_sound* pSound, float rolloff);
void ma_sound_set_spatialization_enabled(ma_sound* pSound, ma_bool32 enabled);

// Дополнительно
void ma_sound_set_fade_in_pcm_frames(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames);
void ma_sound_set_fade_in_milliseconds(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds);
void ma_sound_set_current_fade_volume(ma_sound* pSound, float volume);
ma_result ma_sound_set_stop_time_in_pcm_frames(ma_sound* pSound, ma_uint64 absoluteTimeInFrames);
ma_result ma_sound_set_stop_time_in_milliseconds(ma_sound* pSound, ma_uint64 absoluteTimeInMilliseconds);

// Fade и stop
void ma_sound_fade_in_pcm_frames(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames);
void ma_sound_fade_in_milliseconds(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds);
```

### Флаги инициализации

```c
#define MA_SOUND_FLAG_DECODE    0x00000001  // Декодировать в память (для стриминга = 0)
#define MA_SOUND_FLAG_ASYNC     0x00000002  // Асинхронная загрузка
#define MA_SOUND_FLAG_WAIT_INIT 0x00000004  // Ждать завершения инициализации
#define MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT 0x00000008  // Не подключать к endpoint
```

---

## ma_sound_group

Группа звуков для управления несколькими звуками одновременно (SFX, музыка, голос).

### Основные функции

```c
ma_result ma_sound_group_init(ma_engine* pEngine, ma_uint32 flags, ma_sound_group* pParentGroup, ma_sound_group* pGroup);
void ma_sound_group_uninit(ma_sound_group* pGroup);

// Управление воспроизведением
ma_result ma_sound_group_start(ma_sound_group* pGroup);
ma_result ma_sound_group_stop(ma_sound_group* pGroup);
ma_bool32 ma_sound_group_is_playing(ma_sound_group* pGroup);

// Громкость, панорамирование, питч
void ma_sound_group_set_volume(ma_sound_group* pGroup, float volume);
float ma_sound_group_get_volume(ma_sound_group* pGroup);
void ma_sound_group_set_pan(ma_sound_group* pGroup, float pan);
float ma_sound_group_get_pan(ma_sound_group* pGroup);
void ma_sound_group_set_pitch(ma_sound_group* pGroup, float pitch);
float ma_sound_group_get_pitch(ma_sound_group* pGroup);

// Fade
void ma_sound_group_set_fade_in_pcm_frames(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames);
```

---

## ma_decoder

Декодер — data source для чтения аудиофайлов. Встроенная поддержка WAV, FLAC, MP3.

### Основные функции

```c
ma_decoder_config ma_decoder_config_init(ma_format outputFormat, ma_uint32 outputChannels, ma_uint32 outputSampleRate);

ma_result ma_decoder_init(const void* pData, size_t dataSize, const ma_decoder_config* pConfig, ma_decoder* pDecoder);
ma_result ma_decoder_init_file(const char* pFilePath, const ma_decoder_config* pConfig, ma_decoder* pDecoder);
ma_result ma_decoder_init_file_w(const wchar_t* pFilePath, const ma_decoder_config* pConfig, ma_decoder* pDecoder);
ma_result ma_decoder_init_memory(const void* pData, size_t dataSize, const ma_decoder_config* pConfig, ma_decoder* pDecoder);

void ma_decoder_uninit(ma_decoder* pDecoder);

// Чтение данных
ma_result ma_decoder_read_pcm_frames(ma_decoder* pDecoder, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
ma_result ma_decoder_seek_to_pcm_frame(ma_decoder* pDecoder, ma_uint64 frameIndex);
ma_result ma_decoder_get_length_in_pcm_frames(ma_decoder* pDecoder, ma_uint64* pLength);

// Информация
ma_format ma_decoder_get_output_format(ma_decoder* pDecoder);
ma_uint32 ma_decoder_get_output_channels(ma_decoder* pDecoder);
ma_uint32 ma_decoder_get_output_sample_rate(ma_decoder* pDecoder);
```

### Структура ma_decoder

```c
struct ma_decoder {
    ma_data_source_base base;      // Base data source
    ma_format outputFormat;
    ma_uint32 outputChannels;
    ma_uint32 outputSampleRate;
    // ... внутренние поля
};
```

---

## ma_resource_manager

Resource manager — асинхронная загрузка, кэширование и управление аудиоресурсами.

### Основные функции

```c
ma_resource_manager_config ma_resource_manager_config_init();
ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager);
void ma_resource_manager_uninit(ma_resource_manager* pResourceManager);

// Загрузка данных
ma_result ma_resource_manager_register_data(ma_resource_manager* pResourceManager, const char* pName, ma_data_source* pDataSource);
ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName);

// Работа с buffer'ами
ma_result ma_resource_manager_load_buffer(ma_resource_manager* pResourceManager, const char* pFilePath, void** ppData, size_t* pSize);
ma_result ma_resource_manager_free_buffer(ma_resource_manager* pResourceManager, void* pData);
```

---

## Утилиты

### Форматирование времени

```c
ma_uint64 ma_calculate_buffer_size_in_frames_from_milliseconds(ma_uint32 milliseconds, ma_uint32 sampleRate);
ma_uint64 ma_calculate_buffer_size_in_milliseconds_from_frames(ma_uint64 frames, ma_uint32 sampleRate);
```

### Размеры данных

```c
size_t ma_get_bytes_per_sample(ma_format format);
size_t ma_get_bytes_per_frame(ma_format format, ma_uint32 channels);
```

### Конвертация данных

```c
ma_result ma_pcm_format_conversion(void* pDst, ma_format formatDst, const void* pSrc, ma_format formatSrc, ma_uint64 frames, ma_uint32 channels);
```

### Sleep

```c
void ma_sleep(ma_uint32 milliseconds);
```

---

## Callback-типы

```c
// Device data callback
typedef void (*ma_device_callback_proc)(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

// Device notification callback
typedef void (*ma_device_notification_proc)(ma_device* pDevice, ma_device_notification_type type);

// Log callback
typedef void (*ma_log_callback_proc)(void* pUserData, ma_uint32 level, const char* pMessage);

// Enum devices callback
typedef ma_bool32 (*ma_enum_devices_callback_proc)(ma_context* pContext, ma_device_type deviceType, const ma_device_info* pInfo, void* pUserData);
