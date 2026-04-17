# Продвинутые темы

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
