# Advanced Topics

**🔴 Уровень 3: Продвинутый**

## На этой странице

- [Node Graph и эффекты](#node-graph-и-эффекты)
- [Spatial Audio (3D звук)](#spatial-audio-3d-звук)
- [Custom Decoders (кастомные форматы)](#custom-decoders-кастомные-форматы)
- [Resource Manager (ресурсный менеджер)](#resource-manager-ресурсный-менеджер)
- [Кастомные бэкенды](#кастомные-бэкенды)
- [Мультипоточность и thread-safety](#мультипоточность-и-thread-safety)
- [Производительность (продвинутая)](#производительность-продвинутая)

---

## Node Graph и эффекты

**Node Graph** — это основа High-level API miniaudio. Граф состоит из узлов (nodes), которые соединяются входами и
выходами, образуя цепочку обработки аудио. Каждый узел может быть источником (source), эффектом (effect) или конечной
точкой (endpoint).

### Основные типы узлов

| Тип узла     | Описание                   | Примеры                                                 |
|--------------|----------------------------|---------------------------------------------------------|
| **Source**   | Источник аудиоданных       | `ma_data_source_node`, `ma_decoder_node`                |
| **Effect**   | Обрабатывает данные        | `ma_biquad_node` (фильтр), `ma_lpf_node`, `ma_hpf_node` |
| **Mixer**    | Смешивает несколько входов | `ma_splitter_node`, `ma_audio_buffer_node`              |
| **Endpoint** | Конечная точка графа       | `ma_device_node` (выход на устройство)                  |

### Создание простого графа с эффектом

```c
ma_engine engine;
ma_engine_init(NULL, &engine);

// Получаем корневой граф
ma_node_graph* pGraph = ma_engine_get_node_graph(&engine);

// Создаём эффект (low-pass filter)
ma_biquad_config biquadConfig = ma_biquad_config_init(ma_format_f32, 2, 44100, 1000.0f, 0.707f, ma_biquad_type_lowpass);
ma_biquad_node biquadNode;
ma_biquad_node_init(pGraph, &biquadConfig, NULL, &biquadNode);

// Загружаем звук
ma_sound sound;
ma_sound_init_from_file(&engine, "music.wav", 0, NULL, NULL, &sound);

// Получаем узел звука
ma_sound_node* pSoundNode = ma_sound_get_node(&sound);

// Подключаем цепочку: sound → biquad → endpoint
ma_node_attach_output_bus(pSoundNode, 0, &biquadNode, 0);
ma_node_attach_output_bus(&biquadNode, 0, ma_node_graph_get_endpoint(pGraph), 0);

// Запускаем звук
ma_sound_start(&sound);

// Меняем параметры эффекта в реальном времени
ma_biquad_node_set_cutoff(&biquadNode, 500.0f); // Изменяем частоту среза
```

### Кастомные эффекты

Вы можете создавать собственные узлы, реализуя интерфейс `ma_node_vtable`:

```c
typedef struct {
    ma_node_base base;           // Базовый класс
    float volume;                // Пользовательские параметры
    // ... другие данные
} my_custom_node;

ma_result my_custom_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut) {
    my_custom_node* pCustom = (my_custom_node*)pNode;
    
    for (ma_uint32 i = 0; i < *pFrameCountOut; i++) {
        for (ma_uint32 ch = 0; ch < pNode->outputBuses[0].channels; ch++) {
            ppFramesOut[ch][i] = ppFramesIn[ch][i] * pCustom->volume;
        }
    }
    
    return MA_SUCCESS;
}

// Инициализация VTable
ma_node_vtable g_myCustomNodeVTable = {
    my_custom_node_process_pcm_frames,
    NULL, // onNodeInit (опционально)
    NULL, // onNodeUninit (опционально)
    sizeof(my_custom_node)
};

// Создание узла
my_custom_node customNode;
ma_node_config nodeConfig = ma_node_config_init(&g_myCustomNodeVTable);
ma_node_init(ma_engine_get_node_graph(&engine), &nodeConfig, NULL, &customNode.base);
customNode.volume = 0.5f;
```

### Примеры использования Node Graph

1. **Микширование нескольких звуков с разными эффектами**
2. **Sidechain compression** (дыхание музыки при речи)
3. **Рекурсивные эффекты** (реверберация с обратной связью)
4. **Анализаторы спектра** (визуализация)

---

## Spatial Audio (3D звук)

**Spatial Audio** позволяет позиционировать звуки в 3D пространстве. miniaudio поддерживает базовый 3D звук через
панорамирование (panning) и более продвинутые методы через HRTF (Head-Related Transfer Function).

### Базовая 3D позиционизация

```c
ma_sound sound;
ma_sound_init_from_file(&engine, "explosion.wav", 0, NULL, NULL, &sound);

// Устанавливаем позицию в 3D пространстве
ma_vec3f position = { 10.0f, 0.0f, 0.0f }; // 10 метров справа
ma_sound_set_position(&sound, position.x, position.y, position.z);

// Устанавливаем позицию слушателя
ma_vec3f listenerPos = { 0.0f, 0.0f, 0.0f }; // Центр
ma_vec3f listenerForward = { 0.0f, 0.0f, 1.0f }; // Смотрит вперёд
ma_vec3f listenerUp = { 0.0f, 1.0f, 0.0f }; // Вверх
ma_engine_listener_set_position(&engine, 0, listenerPos.x, listenerPos.y, listenerPos.z);
ma_engine_listener_set_direction(&engine, 0, listenerForward.x, listenerForward.y, listenerForward.z);
ma_engine_listener_set_world_up(&engine, 0, listenerUp.x, listenerUp.y, listenerUp.z);

// Запускаем звук
ma_sound_start(&sound);
```

### Конфигурация Spatial Audio

```c
ma_engine_config engineConfig = ma_engine_config_init();
engineConfig.channels = 2;  // Стерео
engineConfig.sampleRate = 48000;

// Настройки spatial audio
engineConfig.spatializationEnabled = MA_TRUE;
engineConfig.spatializationAlgorithm = ma_spatialization_algorithm_panning; // или ma_spatialization_algorithm_hrtf

// HRTF требует загрузки данных
if (engineConfig.spatializationAlgorithm == ma_spatialization_algorithm_hrtf) {
    // Загружаем HRTF данные (например, из файла)
    // ma_hrtf_init_from_file(...);
}

ma_engine engine;
ma_engine_init(&engineConfig, &engine);
```

### Параметры Spatial Audio

| Параметр                    | Описание                                        | Диапазон                         |
|-----------------------------|-------------------------------------------------|----------------------------------|
| **Позиция**                 | Координаты звука в мировом пространстве         | Любые float                      |
| **Направление**             | Направление звука (для направленных источников) | Нормализованный вектор           |
| **Конус**                   | Угол конуса направленности                      | innerAngle, outerAngle (радианы) |
| **Дистанционное затухание** | Модель затухания с расстоянием                  | minDistance, maxDistance         |
| **Скорость**                | Доплеровский эффект                             | Вектор скорости                  |

```c
// Настройка дистанционного затухания (по умолчанию обратное расстояние)
ma_sound_set_min_distance(&sound, 1.0f);  // Минимальная дистанция (полная громкость)
ma_sound_set_max_distance(&sound, 100.0f); // Максимальная дистанция (тишина)

// Настройка конуса направленности
ma_sound_set_cone(&sound, 
    MA_PI / 6.0f,  // innerAngle (30 градусов) - полная громкость
    MA_PI / 2.0f,  // outerAngle (90 градусов) - затухание начинается
    0.3f           // outerGain - громкость вне outerAngle (30%)
);

// Доплеровский эффект
ma_sound_set_doppler_factor(&sound, 1.0f); // Множитель доплеровского сдвига
ma_sound_set_velocity(&sound, 10.0f, 0.0f, 0.0f); // Скорость источника
```

### HRTF (Head-Related Transfer Function)

HRTF обеспечивает реалистичное 3D позиционирование, учитывая анатомию ушей и головы. Для использования HRTF:

1. **Загрузка HRTF данных**: miniaudio поддерживает стандартные форматы HRTF.
2. **Выбор алгоритма**: `ma_spatialization_algorithm_hrtf`.
3. **Настройка для слушателя**: можно использовать несколько слушателей с разными HRTF наборами.

```c
// Загрузка HRTF данных (упрощённый пример)
ma_hrtf hrtf;
ma_hrtf_config hrtfConfig = ma_hrtf_config_init(ma_format_f32, 48000, 512, NULL, 0);
ma_hrtf_init(&hrtfConfig, &hrtf);

// Использование в engine
engineConfig.spatializationEnabled = MA_TRUE;
engineConfig.spatializationAlgorithm = ma_spatialization_algorithm_hrtf;
engineConfig.pHRTF = &hrtf;
```

### Оптимизация для множества 3D звуков

При сотнях звуков одновременно:

1. **Дистанционный culling**: Игнорировать звуки дальше maxDistance.
2. **Приоритизация**: Сортируйте звуки по важности (громкость, дистанция).
3. **Пулинг узлов**: Используйте предсозданные узлы звуков.
4. **Batch processing**: Обрабатывайте звуки группами.

---

## Custom Decoders (кастомные форматы)

Вы можете добавить поддержку собственных аудиоформатов, реализовав интерфейс `ma_decoding_backend_vtable`.

### Реализация кастомного декодера

```c
typedef struct {
    ma_data_source_base base;
    FILE* pFile;
    // ... специфичные для формата данные
} my_custom_decoder;

ma_result my_custom_decoder_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
    my_custom_decoder* pDecoder = (my_custom_decoder*)pDataSource;
    
    // Чтение и декодирование данных из pFile в pFramesOut
    // ...
    
    if (pFramesRead) *pFramesRead = framesActuallyRead;
    return MA_SUCCESS;
}

ma_result my_custom_decoder_seek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
    // Перемещение к определённому кадру
    return MA_SUCCESS;
}

ma_result my_custom_decoder_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate) {
    // Возвращаем формат данных
    *pFormat = ma_format_f32;
    *pChannels = 2;
    *pSampleRate = 44100;
    return MA_SUCCESS;
}

// Инициализация VTable
ma_decoding_backend_vtable g_myCustomDecoderVTable = {
    my_custom_decoder_read,
    my_custom_decoder_seek,
    my_custom_decoder_get_data_format,
    NULL, // onInit (опционально)
    NULL, // onUninit (опционально)
    sizeof(my_custom_decoder)
};

// Регистрация декодера
ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 44100);
config.pCustomBackendVTable = &g_myCustomDecoderVTable;

ma_decoder decoder;
ma_decoder_init(&config, &decoder);
```

### Поддержка новых форматов

1. **Чтение заголовка**: Определение формата, каналов, sample rate.
2. **Декодирование кадров**: Преобразование из формата в PCM.
3. **Seek**: Быстрый переход к произвольной позиции.
4. **Метаданные**: Извлечение тегов (artist, title и т.д.).

### Использование с Resource Manager

```c
// Регистрация кастомного декодера в Resource Manager
ma_resource_manager_config resourceManagerConfig = ma_resource_manager_config_init();
resourceManagerConfig.pCustomDecodingBackendVTables = &g_myCustomDecoderVTable;
resourceManagerConfig.customDecodingBackendCount = 1;

ma_resource_manager resourceManager;
ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
```

---

## Resource Manager (ресурсный менеджер)

**Resource Manager** обеспечивает асинхронную загрузку, кэширование и стриминг аудиоресурсов.

### Основные возможности

| Функция                  | Описание                                 |
|--------------------------|------------------------------------------|
| **Асинхронная загрузка** | Загрузка в фоновом потоке без блокировки |
| **Кэширование**          | Хранение декодированных данных в памяти  |
| **Стриминг**             | Постепенная загрузка больших файлов      |
| **Пулинг данных**        | Переиспользование буферов                |
| **Job система**          | Управление задачами загрузки             |

### Инициализация

```c
ma_resource_manager_config config = ma_resource_manager_config_init();
config.decodedFormat = ma_format_f32;   // Формат для декодирования
config.decodedChannels = 2;              // Количество каналов
config.decodedSampleRate = 48000;        // Sample rate
config.jobThreadCount = 2;               // Количество фоновых потоков

// Размеры кэшей
config.decodedCacheCapInFrames = 10 * 48000; // 10 секунд при 48kHz

ma_resource_manager resourceManager;
ma_resource_manager_init(&config, &resourceManager);
```

### Загрузка звука

```c
// Синхронная загрузка (блокирует до завершения)
ma_resource_manager_data_source dataSource;
ma_result result = ma_resource_manager_data_source_init(
    &resourceManager,
    "sound.wav",
    0,  // flags
    NULL, // pNotification
    &dataSource
);

if (result == MA_SUCCESS) {
    // Использование dataSource
    ma_sound_init_from_data_source(&engine, &dataSource, 0, NULL, &sound);
}

// Асинхронная загрузка
ma_resource_manager_data_source_callbacks callbacks = {
    .onInit = my_on_init_callback,
    .onDone = my_on_done_callback
};

ma_resource_manager_data_source_init_ex(
    &resourceManager,
    "sound.wav",
    0,
    &callbacks,
    sizeof(my_custom_user_data),
    &dataSource
);
```

### Кэширование

Resource Manager поддерживает два уровня кэширования:

1. **Декодированный кэш**: Хранит PCM данные в памяти.
2. **Encoded кэш**: Хранит закодированные данные (меньше памяти, но требует декодирования при использовании).

```c
// Настройка кэширования
config.decodedCacheCapInFrames = 60 * 48000; // 60 секунд
config.encodedCacheCapInBytes = 100 * 1024 * 1024; // 100 MB

// Принудительное кэширование
ma_resource_manager_cache_data(&resourceManager, "music.flac");
```

### Стриминг больших файлов

Для файлов, которые не помещаются в память:

```c
ma_resource_manager_data_source_config dataSourceConfig = ma_resource_manager_data_source_config_init("large_file.wav");
dataSourceConfig.flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM;

ma_resource_manager_data_source dataSource;
ma_resource_manager_data_source_init(&resourceManager, &dataSourceConfig, &dataSource);

// Стриминг происходит автоматически при чтении
```

### Job система

Resource Manager использует систему задач (jobs) для управления загрузкой:

```c
// Создание кастомной job
ma_job job;
ma_job_init(&job, ma_job_type_custom, &my_custom_job_data);

// Отправка job
ma_resource_manager_post_job(&resourceManager, &job);

// Ожидание завершения
ma_resource_manager_wait_for_job(&resourceManager, &job);
```

---

## Кастомные бэкенды

Вы можете реализовать собственный аудиобэкенд, если стандартные не подходят (например, для embedded систем).

### Реализация бэкенда

```c
typedef struct {
    ma_device_base base;
    // ... платформо-специфичные данные
} my_custom_device;

ma_result my_custom_device_init(ma_device* pDevice, const ma_device_config* pConfig, my_custom_device* pCustomDevice) {
    // Инициализация оборудования
    return MA_SUCCESS;
}

ma_result my_custom_device_uninit(ma_device* pDevice, my_custom_device* pCustomDevice) {
    // Очистка
    return MA_SUCCESS;
}

ma_result my_custom_device_start(ma_device* pDevice, my_custom_device* pCustomDevice) {
    // Запуск устройства
    return MA_SUCCESS;
}

ma_result my_custom_device_stop(ma_device* pDevice, my_custom_device* pCustomDevice) {
    // Остановка устройства
    return MA_SUCCESS;
}

// Регистрация бэкенда
ma_backend_callbacks g_myCustomBackendCallbacks = {
    sizeof(my_custom_device),
    my_custom_device_init,
    my_custom_device_uninit,
    my_custom_device_start,
    my_custom_device_stop,
    NULL, // onDataLoop (опционально)
    NULL  // onNotification (опционально)
};

// Использование
ma_context_config contextConfig = ma_context_config_init();
contextConfig.pCustomBackendCallbacks = &g_myCustomBackendCallbacks;

ma_context context;
ma_context_init(&contextConfig, &context);
```

---

## Мультипоточность и thread-safety

### Thread-safe операции

miniaudio обеспечивает thread-safety для большинства операций, но есть важные ограничения:

| Операция                 | Thread-safe? | Примечания                        |
|--------------------------|--------------|-----------------------------------|
| `ma_engine_play_sound`   | Да           | Можно вызывать из любого потока   |
| `ma_sound_set_volume`    | Да           |                                   |
| `ma_resource_manager_*`  | Да           | Внутренняя синхронизация          |
| `ma_device_init/uninit`  | Нет          | Вызывать из главного потока       |
| `ma_engine_init/uninit`  | Нет          | Вызывать из главного потока       |
| Операции в data callback | Ограничено   | Нельзя вызывать device start/stop |

### Синхронизация данных

```c
// Пример thread-safe передачи данных в callback
typedef struct {
    ma_spinlock lock;
    float volume;
    // ... другие данные
} thread_shared_data;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    thread_shared_data* pShared = (thread_shared_data*)pDevice->pUserData;
    
    float currentVolume;
    ma_spinlock_lock(&pShared->lock);
    currentVolume = pShared->volume;
    ma_spinlock_unlock(&pShared->lock);
    
    // Использование currentVolume
}

// В другом потоке
ma_spinlock_lock(&sharedData.lock);
sharedData.volume = newVolume;
ma_spinlock_unlock(&sharedData.lock);
```

### Lock-free структуры

Для максимальной производительности в real-time потоках:

```c
// Использование atomic операций
#include <stdatomic.h>

atomic_float g_volume = ATOMIC_VAR_INIT(1.0f);

// В callback
float volume = atomic_load(&g_volume);

// В другом потоке
atomic_store(&g_volume, 0.5f);
```

---

## Производительность (продвинутая)

### Оптимизация для реального времени

1. **Избегайте аллокаций в audio thread**:
   ```c
   // Плохо:
   float* buffer = malloc(frameCount * sizeof(float));
   
   // Хорошо:
   float buffer[4096]; // на стеке
   // или предвыделенный пул
   ```

2. **Minimize branching в hot paths**:
   ```c
   // Плохо:
   if (condition) {
       // путь A
   } else {
       // путь B
   }
   
   // Лучше:
   // Используйте branchless код
   result = a * condition + b * (1 - condition);
   ```

3. **Используйте SIMD при возможности**:
   ```c
   #ifdef MA_SUPPORT_SSE2
   #include <emmintrin.h>
   // SSE оптимизации
   #endif
   ```

### Профилирование

Используйте Tracy для профилирования audio thread:

```c
#include "tracy/TracyC.h"

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ZoneScoped; // Tracy zone
    
    // ... обработка
    
    FrameMark; // Отметка кадра
}
```

### Оптимизация памяти

1. **Выравнивание буферов**: Используйте `ma_aligned_malloc` для SIMD.
2. **Пулинг объектов**: Переиспользуйте `ma_sound`, `ma_decoder` и т.д.
3. **Кэширование данных**: Храните часто используемые звуки в декодированном виде.

### Latency оптимизации

```c
ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.performanceProfile = ma_performance_profile_low_latency;
config.periodSizeInFrames = 256;  // Маленький буфер для low latency
config.periods = 2;               // Двойная буферизация
```

---

## Пример: продвинутая аудиосистема для игры

```c
// Комплексный пример, объединяющий несколько advanced тем
typedef struct {
    ma_engine engine;
    ma_resource_manager resourceManager;
    ma_sound_group sfxGroup;
    ma_sound_group musicGroup;
    ma_biquad_node lowPassFilter;
    
    // Spatial audio
    ma_vec3f listenerPosition;
    ma_hrtf hrtf;
    
    // Производительность
    ma_sound soundPool[MAX_SOUNDS];
    uint32_t nextSoundIndex;
} audio_system;

void audio_system_init(audio_system* pSystem) {
    // Инициализация resource manager с кэшированием
    ma_resource_manager_config rmConfig = ma_resource_manager_config_init();
    rmConfig.decodedCacheCapInFrames = 30 * 48000; // 30 секунд
    rmConfig.jobThreadCount = 2;
    ma_resource_manager_init(&rmConfig, &pSystem->resourceManager);
    
    // Инициализация engine с HRTF
    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.pResourceManager = &pSystem->resourceManager;
    engineConfig.spatializationEnabled = MA_TRUE;
    engineConfig.spatializationAlgorithm = ma_spatialization_algorithm_hrtf;
    ma_hrtf_init_default(&engineConfig.pHRTF);
    
    ma_engine_init(&engineConfig, &pSystem->engine);
    
    // Группы звуков
    ma_engine_get_sound_group(&pSystem->engine, "sfx", &pSystem->sfxGroup);
    ma_engine_get_sound_group(&pSystem->engine, "music", &pSystem->musicGroup);
    
    // Эффект для музыки
    ma_biquad_config filterConfig = ma_biquad_config_init(
        ma_format_f32, 2, 48000, 1000.0f, 0.707f, ma_biquad_type_lowpass
    );
    ma_biquad_node_init(ma_engine_get_node_graph(&pSystem->engine), 
                       &filterConfig, NULL, &pSystem->lowPassFilter);
    
    // Подключение: musicGroup → lowPassFilter → endpoint
    ma_node_attach_output_bus(ma_sound_group_get_node(&pSystem->musicGroup), 0, 
                             &pSystem->lowPassFilter, 0);
    ma_node_attach_output_bus(&pSystem->lowPassFilter, 0,
                             ma_node_graph_get_endpoint(ma_engine_get_node_graph(&pSystem->engine)), 0);
}

// Использование пула звуков для оптимизации
ma_sound* audio_system_play_sfx(audio_system* pSystem, const char* filepath, ma_vec3f position) {
    ma_sound* pSound = &pSystem->soundPool[pSystem->nextSoundIndex];
    pSystem->nextSoundIndex = (pSystem->nextSoundIndex + 1) % MAX_SOUNDS;
    
    if (ma_sound_is_playing(pSound)) {
        ma_sound_stop(pSound);
    }
    
    ma_sound_init_from_file(&pSystem->engine, filepath, 
                           MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_STREAM,
                           &pSystem->sfxGroup, NULL, pSound);
    
    ma_sound_set_position(pSound, position.x, position.y, position.z);
    ma_sound_set_min_distance(pSound, 1.0f);
    ma_sound_set_max_distance(pSound, 50.0f);
    ma_sound_start(pSound);
    
    return pSound;
}
```

---

## Дальше

**Следующие разделы:**

- [Решение проблем](troubleshooting.md) — диагностика и исправление ошибок
- [Use Cases](use-cases.md) — практические сценарии использования
- [Decision Trees](decision-trees.md) — руководство по принятию решений

**← [Назад к содержанию](README.md)**

**Ресурсы:**

- [Официальная документация miniaudio](https://miniaud.io/docs)
- [Примеры в репозитории](https://github.com/mackron/miniaudio/tree/master/examples)
- [miniaudio.h](https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h) — полная документация в коде