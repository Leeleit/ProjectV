# Производительность miniaudio

**🟡 Уровень 2: Средний**

## На этой странице

- [Общие рекомендации](#общие-рекомендации)
- [Оптимизация Low-level API](#оптимизация-low-level-api)
- [Оптимизация High-level API](#оптимизация-high-level-api)
- [Resource Manager и кэширование](#resource-manager-и-кэширование)
- [Пул объектов и память](#пул-объектов-и-память)
- [Профилирование и отладка](#профилирование-и-отладка)
- [Platform-specific оптимизации](#platform-specific-оптимизации)
- [См. также](#см-также)

---

## Общие рекомендации

### Принципы оптимизации аудио

1. **Минимизация латенции** — критично для игр и интерактивных приложений
2. **Стабильность audio thread** — избегайте блокировок и тяжёлых операций в callback
3. **Эффективное использование памяти** — пулинг, кэширование, предзагрузка
4. **Баланс CPU/GPU** — выбор подходящих форматов и размеров буферов

### Ключевые метрики

| Метрика                            | Целевые значения | Комментарий                                              |
|------------------------------------|------------------|----------------------------------------------------------|
| **Латения**                        | 5-50 мс          | Зависит от приложения: игры < 20 мс, медиаплееры < 50 мс |
| **CPU usage в audio thread**       | < 10%            | Для стабильной работы                                    |
| **Пиковая память**                 | < 100 МБ         | Для декодированных аудио                                 |
| **Количество одновременно звуков** | 20-100           | Зависит от сложности микширования                        |

---

## Оптимизация Low-level API

### Data callback

**Критичные правила для data callback:**

1. **Избегайте аллокаций памяти**
   ```c
   // Плохо:
   float* buffer = malloc(frameCount * sizeof(float));
   
   // Хорошо:
   float buffer[4096]; // на стеке, если frameCount гарантированно мал
   // или предвыделенный буфер
   ```

2. **Минимизируйте branching**
   ```c
   // Плохо:
   if (condition) {
       // путь A
   } else {
       // путь B
   }
   
   // Лучше (где возможно):
   result = a * condition + b * (1 - condition);
   ```

3. **Используйте SIMD оптимизации**
   ```c
   #ifdef MA_SUPPORT_SSE2
   #include <emmintrin.h>
   // SSE оптимизации для обработки аудио
   #endif
   ```

### Настройка буферов и латенции

```c
ma_device_config config = ma_device_config_init(ma_device_type_playback);

// Low latency режим (для игр)
config.performanceProfile = ma_performance_profile_low_latency;
config.periodSizeInFrames = 256;  // Маленький буфер
config.periods = 2;               // Двойная буферизация

// Conservative режим (для медиаплееров)
config.performanceProfile = ma_performance_profile_conservative;
config.periodSizeInFrames = 1024; // Больший буфер
config.periods = 2;

// Автоматический выбор (по умолчанию)
config.performanceProfile = ma_performance_profile_low_latency;
config.periodSizeInFrames = 0; // Автовыбор
```

### Оптимизация форматов данных

| Формат          | Преимущества                      | Недостатки                    | Рекомендации               |
|-----------------|-----------------------------------|-------------------------------|----------------------------|
| `ma_format_f32` | Точность, простота обработки      | Больше памяти (4 байта/сэмпл) | Для обработки, фильтров    |
| `ma_format_s16` | Экономия памяти (2 байта/сэмпл)   | Меньшая точность              | Для воспроизведения музыки |
| `ma_format_u8`  | Минимальная память (1 байт/сэмпл) | Низкое качество               | Для embedded систем        |

---

## Оптимизация High-level API

### Engine конфигурация

```c
ma_engine_config engineConfig = ma_engine_config_init();

// Оптимизация для игр
engineConfig.listenerCount = 1;          // Минимум слушателей
engineConfig.channels = 2;               // Стерео достаточно
engineConfig.sampleRate = 48000;         // Стандартная частота
engineConfig.periodSizeInFrames = 512;   // Баланс latency/производительности

// Отключение неиспользуемых функций
// engineConfig.noAutoStart = MA_TRUE;   // Ручной запуск
// engineConfig.noDevice = MA_TRUE;      // Без устройства (для offline обработки)
```

### Управление звуками

**Пул звуков (Sound Pooling):**

```c
#define MAX_SIMULTANEOUS_SOUNDS 32
ma_sound g_soundPool[MAX_SIMULTANEOUS_SOUNDS];
uint32_t g_nextSoundIndex = 0;

ma_sound* get_available_sound(ma_engine* pEngine) {
    // Ищем свободный звук
    for (uint32_t i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
        uint32_t idx = (g_nextSoundIndex + i) % MAX_SIMULTANEOUS_SOUNDS;
        if (!ma_sound_is_playing(&g_soundPool[idx])) {
            g_nextSoundIndex = (idx + 1) % MAX_SIMULTANEOUS_SOUNDS;
            return &g_soundPool[idx];
        }
    }
    
    // Переиспользуем самый старый
    ma_sound_stop(&g_soundPool[g_nextSoundIndex]);
    ma_sound_seek_to_pcm_frame(&g_soundPool[g_nextSoundIndex], 0);
    ma_sound* pSound = &g_soundPool[g_nextSoundIndex];
    g_nextSoundIndex = (g_nextSoundIndex + 1) % MAX_SIMULTANEOUS_SOUNDS;
    return pSound;
}
```

### Группы звуков для микширования

```c
ma_sound_group sfxGroup;
ma_sound_group musicGroup;
ma_sound_group voiceGroup;

// Раздельная громкость и эффекты
ma_sound_group_init(pEngine, 0, NULL, &sfxGroup);
ma_sound_group_init(pEngine, 0, NULL, &musicGroup);
ma_sound_group_init(pEngine, 0, NULL, &voiceGroup);

// Быстрое отключение всех SFX
ma_sound_group_set_volume(&sfxGroup, 0.0f);
```

---

## Resource Manager и кэширование

### Конфигурация Resource Manager

```c
ma_resource_manager_config rmConfig = ma_resource_manager_config_init();

// Настройка кэшей
rmConfig.decodedCacheCapInFrames = 30 * 48000; // 30 секунд при 48kHz
rmConfig.encodedCacheCapInBytes = 50 * 1024 * 1024; // 50 MB

// Потоки для асинхронной загрузки
rmConfig.jobThreadCount = 2; // Оптимально: 1-4 потока

// Формат декодирования
rmConfig.decodedFormat = ma_format_f32;
rmConfig.decodedChannels = 2;
rmConfig.decodedSampleRate = 48000;

ma_resource_manager resourceManager;
ma_resource_manager_init(&rmConfig, &resourceManager);
```

### Стратегии загрузки

| Стратегия                | Когда использовать           | Плюсы                       | Минусы                    |
|--------------------------|------------------------------|-----------------------------|---------------------------|
| **Синхронная загрузка**  | Короткие звуки (< 1 сек)     | Простота, нет потоков       | Блокирует основной поток  |
| **Асинхронная загрузка** | Длинные звуки, музыка        | Не блокирует основной поток | Сложнее, требует callback |
| **Предзагрузка**         | Критичные звуки (UI, взрывы) | Нулевая задержка            | Тратит память             |
| **Стриминг**             | Фоновая музыка, диалоги      | Экономия памяти             | Сложная синхронизация     |

### Предзагрузка часто используемых звуков

```c
// При запуске приложения
ma_resource_manager_data_source dataSource;
ma_resource_manager_data_source_init(&resourceManager, "explosion.wav", 0, NULL, &dataSource);
ma_resource_manager_cache_data(&resourceManager, "explosion.wav");
```

---

## Пул объектов и память

### Переиспользование объектов

```c
typedef struct {
    ma_sound* sounds;
    uint32_t capacity;
    uint32_t nextIndex;
} sound_pool;

void sound_pool_init(sound_pool* pPool, ma_engine* pEngine, uint32_t capacity) {
    pPool->sounds = malloc(capacity * sizeof(ma_sound));
    pPool->capacity = capacity;
    pPool->nextIndex = 0;
    
    for (uint32_t i = 0; i < capacity; i++) {
        ma_sound_init_from_file(pEngine, "", MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT, NULL, NULL, &pPool->sounds[i]);
    }
}
```

### Аллокация в audio thread

**Никогда не делайте это в data callback:**

- `malloc`, `calloc`, `realloc`, `free`
- `new`, `delete` (C++)
- File I/O операции
- Системные вызовы (sleep, mutex lock)

**Безопасные операции:**

- Чтение/запись в предвыделенные буферы
- Атомарные операции
- Lock-free структуры данных
- SIMD вычисления

---

## Профилирование и отладка

### Tracy для audio thread

```c
#include "tracy/TracyC.h"

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ZoneScoped; // Tracy zone для audio thread
    
    // ... обработка аудио
    
    FrameMark; // Отметка кадра
}
```

### Статистика использования

```c
// Получение статистики resource manager
ma_resource_manager_stats stats;
ma_resource_manager_get_stats(&resourceManager, &stats);

printf("Decoded cache: %llu / %llu frames\n", 
       stats.decodedCacheUsageInFrames, stats.decodedCacheCapacityInFrames);
printf("Encoded cache: %llu / %llu bytes\n",
       stats.encodedCacheUsageInBytes, stats.encodedCacheCapacityInBytes);
printf("Active jobs: %u\n", stats.jobCount);
```

### Отладка латенции

```c
// Измерение реальной латенции
ma_device_info* pPlaybackInfo;
ma_uint32 playbackCount;
ma_context_get_devices(&context, &pPlaybackInfo, &playbackCount, NULL, NULL);

printf("Device latency: %f ms\n", 
       pPlaybackInfo[0].nativeDataFormat.periodSizeInMilliseconds);
```

---

## Platform-specific оптимизации

### Windows (WASAPI)

```c
// Exclusive mode для минимальной латенции
config.wasapi.noAutoConvertSRC = MA_TRUE; // Отключить SRC
config.wasapi.noDefaultQualitySRC = MA_TRUE;
config.wasapi.noHardwareOffloading = MA_TRUE;

// Shared mode для совместимости
config.performanceProfile = ma_performance_profile_conservative;
```

### Linux (ALSA/PulseAudio)

```c
// ALSA для низкой латенции
config.alsa.noMMap = MA_FALSE; // Использовать mmap для лучшей производительности
config.alsa.noAutoFormat = MA_TRUE; // Ручной выбор формата
config.alsa.noAutoChannels = MA_TRUE;
config.alsa.noAutoResample = MA_TRUE;
```

### macOS/iOS (Core Audio)

```c
// Настройки для iOS
config.coreaudio.sessionCategory = ma_ios_session_category_playback;
config.coreaudio.sessionCategoryOptions = ma_ios_session_category_option_mix_with_others;
config.coreaudio.noAudioSessionActivate = MA_FALSE;
config.coreaudio.noAudioSessionDeactivate = MA_FALSE;
```

### Android (AAudio/OpenSL|ES)

```c
// AAudio для Android 8.0+
config.aaudio.usage = ma_aaudio_usage_game;
config.aaudio.contentType = ma_aaudio_content_type_speech;
config.aaudio.noAutoStartAfterReroute = MA_TRUE;

// OpenSL|ES для совместимости
config.opensl.streamType = ma_opensl_stream_type_media;
config.opensl.recordingPreset = ma_opensl_recording_preset_generic;
```

---

## См. также

- [Основные понятия](concepts.md) — архитектура miniaudio
- [Advanced Topics](advanced-topics.md) — продвинутые темы производительности
- [Decision Trees](decision-trees.md) — выбор оптимальных параметров
- [Интеграция](integration.md) — настройка сборки
- [Troubleshooting](troubleshooting.md) — решение проблем производительности
- [Use Cases](use-cases.md) — практические сценарии оптимизации
