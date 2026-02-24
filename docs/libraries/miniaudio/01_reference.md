## miniaudio

<!-- anchor: 00_overview -->


**miniaudio** — single-file библиотека на C для работы с аудио: воспроизведение, захват, декодирование, кодирование,
обработка. Не имеет зависимостей, кроме стандартной библиотеки. Поддерживает все основные платформы и бэкенды (WASAPI,
Core Audio, ALSA, PulseAudio, JACK, AAudio и др.).

Исходники: [miniaud.io](https://miniaud.io), [GitHub](https://github.com/mackron/miniaudio).

Версия: **0.11+**. Лицензия: Public Domain или MIT No Attribution.

---

## Выбор уровня API

### Когда использовать High-level API (ma_engine)

- Быстрое прототипирование игр или приложений
- Игры (большинство случаев)
- Медиаплееры и простые аудиоприложения
- Когда не нужен полный контроль над аудиопотоком
- Использование встроенных функций (spatial audio, resource manager)

```c
ma_engine engine;
ma_engine_init(NULL, &engine);
ma_engine_play_sound(&engine, "sound.wav", NULL);
getchar();
ma_engine_uninit(&engine);
```

### Когда использовать Low-level API (ma_device)

- Реализация собственного микшера для специфичных нужд
- Кастомная обработка аудио в реальном времени (DSP)
- Минимально возможная latency (DAW, VST плагины)
- Интеграция с существующими аудиосистемами
- Эксперименты с нестандартными форматами данных

```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Заполнить pOutput данными
}

ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.dataCallback = data_callback;

ma_device device;
ma_device_init(NULL, &config, &device);
ma_device_start(&device);
```

### Гибридный подход

Можно комбинировать оба подхода: High-level API для музыки, UI звуков, ambient звуков; Low-level API для критичных по
latency звуков (геймплейные звуки, голосовой чат).

---

## Требования

- **C11** или **C++11** (или новее)
- **Стандартная библиотека C**
- **Платформенные зависимости** (линкуются автоматически):
  - Linux: `-lpthread -lm -ldl` (опционально `-latomic`)
  - Windows: нет зависимостей
  - macOS/iOS: `-framework CoreFoundation -framework CoreAudio -framework AudioToolbox`
  - Android: `-lOpenSLES` (если не используется runtime linking)

---

## Поддерживаемые платформы и бэкенды

| Платформа     | Основные бэкенды                                  | Дополнительные                 |
|---------------|---------------------------------------------------|--------------------------------|
| Windows       | WASAPI, DirectSound, WinMM                        |                                |
| macOS/iOS     | Core Audio                                        |                                |
| Linux         | ALSA, PulseAudio, JACK                            | sndio (OpenBSD), OSS (FreeBSD) |
| Android       | AAudio, OpenSL\|ES                                |                                |
| BSD           | sndio (OpenBSD), audio(4) (NetBSD), OSS (FreeBSD) |                                |
| Emscripten    | Web Audio                                         |                                |
| Универсальный | Null (тишина), Custom (своя реализация)           |                                |

### Приоритеты бэкендов по умолчанию

| Платформа  | Порядок бэкендов (приоритет) |
|------------|------------------------------|
| Windows    | WASAPI → DirectSound → WinMM |
| macOS/iOS  | Core Audio                   |
| Linux      | ALSA → PulseAudio → JACK     |
| Android    | AAudio → OpenSL\|ES          |
| Emscripten | Web Audio                    |

---

## Особенности и возможности

### Основные возможности

- **Single-file**: `miniaudio.h` + `miniaudio.c` (или `#define MINIAUDIO_IMPLEMENTATION`)
- **Нет зависимостей**: только стандартная библиотека C
- **Поддержка множества форматов**: WAV, FLAC, MP3 (встроенные декодеры)
- **Два уровня API**: Low-level (полный контроль) и High-level (простота)
- **Ресурсный менеджер**: асинхронная загрузка, кэширование, стриминг
- **Node Graph**: продвинутое микширование и эффекты
- **Spatial Audio**: 3D позиционирование звуков
- **Кроссплатформенность**: Windows, macOS, Linux, iOS, Android, BSD, Emscripten

### Производительность и оптимизации

- **Low-latency** по умолчанию
- **Zero-allocation** в audio thread (если настроено)
- **SIMD оптимизации** (если доступно)
- **Режимы работы**: shared (несколько приложений) и exclusive (минимальная latency)

### Архитектурные принципы

- **Transparent structures**: все объекты — обычные C структуры
- **Config/init pattern**: единый паттерн инициализации
- **Thread-safe callback**: данные обрабатываются в отдельном потоке
- **No global state**: полный контроль над состоянием

---

## Начало работы за 5 минут

### 1. Добавление в проект

```c
// В одном файле (.c или .cpp):
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// Или через CMake:
// add_subdirectory(external/miniaudio)
// target_link_libraries(YourApp PRIVATE miniaudio)
```

### 2. Минимальный пример (High-level API)

```c
#include "miniaudio.h"

int main() {
    ma_engine engine;
    ma_engine_init(NULL, &engine);
    ma_engine_play_sound(&engine, "sound.wav", NULL);
    getchar();
    ma_engine_uninit(&engine);
    return 0;
}
```

### 3. Минимальный пример (Low-level API)

```c
#include "miniaudio.h"

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Генерация или обработка аудиоданных
}

int main() {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.dataCallback = data_callback;

    ma_device device;
    ma_device_init(NULL, &config, &device);
    ma_device_start(&device);

    getchar();
    ma_device_uninit(&device);
    return 0;
}
```

---

## Дополнительные ресурсы

### Официальная документация

- **[miniaud.io/docs](https://miniaud.io/docs)** — онлайн документация
- **[miniaudio.h](https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h)** — полная документация в коде
- **[Примеры в репозитории](https://github.com/mackron/miniaudio/tree/master/examples)** — официальные примеры

### Сообщество и поддержка

- **[GitHub Issues](https://github.com/mackron/miniaudio/issues)** — багрепорты и вопросы
- **[Discord](https://discord.gg/9vpqbjU)** — канал для обсуждения

---

## Лицензия

miniaudio распространяется под лицензией **Public Domain** или **MIT No Attribution**.

Вы можете использовать библиотеку в коммерческих и некоммерческих проектах без ограничений.

---

## Основные понятия

<!-- anchor: 02_concepts -->


Краткое введение в архитектуру miniaudio. Термины — в глоссарии.

---

## Low-level vs High-level API

miniaudio предоставляет два уровня API:

| Уровень        | Когда использовать                                                                    | Основные объекты                          |
|----------------|---------------------------------------------------------------------------------------|-------------------------------------------|
| **Low-level**  | Прямой контроль над аудиоданными, свой микс, кастомная обработка.                     | `ma_context`, `ma_device`, data callback  |
| **High-level** | Простое воспроизведение звуков, управление группами (SFX, музыка), встроенный микшер. | `ma_engine`, `ma_sound`, `ma_sound_group` |

**Low-level:** вы реализуете `data_callback`, куда miniaudio передаёт буфер. В playback вы записываете PCM frames в
`pOutput`. В capture читаете из `pInput`. Декодирование (если нужно) делаете сами через `ma_decoder` или другой data
source.

**High-level:** `ma_engine` включает устройство, resource manager и node graph. Вы вызываете
`ma_engine_play_sound(&engine, "sound.wav", NULL)` или инициализируете `ma_sound` для контроля над экземпляром.
Декодирование и микширование выполняет miniaudio.

---

## Config/init pattern

Во всей библиотеке используется один паттерн:

1. Создать config: `ma_xxx_config config = ma_xxx_config_init(...);`
2. Настроить нужные поля config
3. Вызвать init: `ma_xxx_init(&config, &object);`

Config можно выделить на стеке и не хранить после init. Новые поля в config добавляются без поломки API.

```c
ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.playback.format   = ma_format_f32;
config.playback.channels = 2;
config.sampleRate        = 48000;
config.dataCallback      = data_callback;
config.pUserData         = pMyData;

ma_device device;
ma_device_init(NULL, &config, &device);
```

---

## Transparent structures

В miniaudio нет opaque handle. Все объекты — обычные C-структуры. Память выделяете вы:

```c
ma_device device;           // на стеке
// или
ma_engine* pEngine = malloc(sizeof(ma_engine));
```

**Важно:** адрес объекта не должен меняться в течение его жизненного цикла. miniaudio хранит указатель на объект; при
копировании или перемещении структуры указатель станет невалидным. Не копируйте `ma_device`, `ma_engine`, `ma_sound` и
т.д.

---

## Data callback

Callback вызывается асинхронно в отдельном потоке (audio thread). miniaudio запрашивает определённое число frames; вы
должны заполнить (playback) или прочитать (capture) не более `frameCount` frames.

```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    // Playback: записать в pOutput, pInput = NULL
    // Capture: прочитать из pInput, pOutput = NULL
    // Duplex: оба валидны
}
```

Поведение по типу устройства:

| Device Type               | pOutput    | pInput                 |
|---------------------------|------------|------------------------|
| `ma_device_type_playback` | Записывать | NULL                   |
| `ma_device_type_capture`  | NULL       | Читать                 |
| `ma_device_type_duplex`   | Записывать | Читать                 |
| `ma_device_type_loopback` | NULL       | Читать (только WASAPI) |

Данные — interleaved: для стерео первые два сэмпла — left, right первого frame, следующие два — left, right второго
frame и т.д.

---

## Period и Latency

Размер буфера устройства задаётся через `periodSizeInFrames` или `periodSizeInMilliseconds` и `periods` в
`ma_device_config`. Частота вызова callback зависит от этих значений.

- **Меньший period** — меньше latency (важно для игр, real-time), выше нагрузка на CPU, выше риск глитчей.
- **Больший period** — выше latency, меньше нагрузка (подходит для медиаплееров).

По умолчанию miniaudio использует `ma_performance_profile_low_latency`. Запрашиваемый размер — лишь подсказка; бэкенд
может вернуть другой.

---

## Sample rate

В config задайте `sampleRate` (Hz). **0** — использовать native устройства. Типичные значения: **44100**, **48000**.
Рекомендованный диапазон: **8000–384000** Hz. В full-duplex sample rate одинаков для playback и capture.

---

## Контекст и перечисление устройств

Для выбора конкретного устройства (не default) нужен `ma_context`:

```c
ma_context context;
ma_context_init(NULL, 0, NULL, &context);

ma_device_info* pPlaybackInfos;
ma_uint32 playbackCount;
ma_device_info* pCaptureInfos;
ma_uint32 captureCount;
ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount);

ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.playback.pDeviceID = &pPlaybackInfos[chosenIndex].id;
// ... остальные поля ...

ma_device device;
ma_device_init(&context, &config, &device);
```

Буферы, возвращаемые `ma_context_get_devices`, принадлежат miniaudio — не освобождать.

Если передать `NULL` в `ma_device_init` вместо context, miniaudio создаст контекст внутри себя. Для выбора устройства
нужно передавать явный context.

Альтернатива `ma_context_get_devices` — `ma_context_enumerate_devices` (callback без heap-аллокации).

---

## Full-duplex

В `ma_device_type_duplex` форматы playback и capture задаются отдельно (`config.playback.*` и `config.capture.*`).
Можно, например, захватывать моно и выводить стерео. **Разные форматы** playback и capture требуют ручной конвертации в
callback — miniaudio не выполняет её автоматически между двумя направлениями в duplex.

---

## notificationCallback

Опциональный callback в `ma_device_config.notificationCallback` вызывается при изменении состояния устройства: старт,
остановка, reroute (смена устройства). Полезен для синхронизации UI или логики с аудиопотоком.

---

## Ограничения в callback

Внутри `data_callback` **нельзя** вызывать:

- `ma_device_init` / `ma_device_init_ex`
- `ma_device_uninit`
- `ma_device_start`
- `ma_device_stop`

Это приведёт к deadlock. Вместо этого устанавливайте флаг или сигнализируйте событие, а остановку/старт выполняйте в
другом потоке.

Также избегайте тяжёлых операций в callback (malloc, file I/O, блокирующие вызовы) — callback выполняется в real-time
потоке.

---

## Node graph и engine

High-level API построен на node graph. Узлы (nodes) соединяются входами и выходами; данные проходят по графу.
`ma_engine` — это `ma_node_graph` с endpoint, к которому подключены звуки и группы. Для продвинутой настройки графа
используйте `ma_engine_get_node_graph()` и API `ma_node_graph`.

- **ma_sound** и **ma_sound_group** — узлы графа
- Группы позволяют разделять громкость и эффекты (SFX, музыка, голос)
- Можно подключать кастомные effect nodes

Для простого воспроизведения достаточно `ma_engine_play_sound` или `ma_sound_init_from_file` + `ma_sound_start`. Node
graph используется при необходимости продвинутого микширования.

---

## Справочник API

<!-- anchor: 03_api-reference -->


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

---

## Глоссарий

<!-- anchor: 07_glossary -->


Терминология аудио и miniaudio.

---

## Основные понятия

### Sample (Сэмпл)

Одно измерение аудиосигнала в определённый момент времени. Значение от -1.0 до +1.0 (для float) или от -32768 до
+32767 (для 16-bit int).

**Пример:** Для стерео один frame = два сэмпла (левый + правый).

---

### Frame (Кадр)

Набор сэмплов для всех каналов в один момент времени.

| Каналы     | Сэмплов в frame |
|------------|-----------------|
| Моно (1)   | 1               |
| Стерео (2) | 2               |
| 5.1 (6)    | 6               |
| 7.1 (8)    | 8               |

**Пример:** При 48000 Hz sample rate одна секунда аудио = 48000 frames.

---

### Sample Rate (Частота дискретизации)

Количество frames в секунду. Измеряется в Hz.

| Частота   | Применение                   |
|-----------|------------------------------|
| 8000 Hz   | Телефония                    |
| 22050 Hz  | Старые игры                  |
| 44100 Hz  | Audio CD                     |
| 48000 Hz  | Профессиональное аудио, игры |
| 96000 Hz  | Hi-Fi, студии                |
| 192000 Hz | Mastering                    |

**Выше частота** → выше качество, больше данных.

---

### PCM (Pulse Code Modulation)

Метод представления аналогового сигнала в цифровом виде. Последовательность сэмплов.

---

### Interleaved

Способ хранения многоканального аудио, при котором сэмплы разных каналов чередуются.

**Пример для стерео:** `[L0, R0, L1, R1, L2, R2, ...]`

Альтернатива — planar: `[L0, L1, L2, ...] [R0, R1, R2, ...]`

miniaudio использует interleaved формат.

---

## Архитектура miniaudio

### Device (Устройство)

Представление физического или виртуального аудиоустройства (динамики, микрофон). Управляет потоком аудиоданных через
callback.

---

### Context (Контекст)

Верхнеуровневый объект, представляющий аудио бэкенд. Нужен для перечисления устройств и управления несколькими
устройствами.

```

Context
├── Device 1 (playback)
├── Device 2 (capture)
└── Device 3 (duplex)

```

---

### Backend (Бэкенд)

Платформенно-зависимая реализация аудио.

| Платформа | Бэкенды                    |
|-----------|----------------------------|
| Windows   | WASAPI, DirectSound, WinMM |
| macOS/iOS | Core Audio                 |
| Linux     | ALSA, PulseAudio, JACK     |
| Android   | AAudio, OpenSL             |ES |
| Web       | Web Audio                  |

---

### Engine

High-level объект, объединяющий устройство, resource manager и node graph. Основной объект для простого воспроизведения
звуков.

```

Engine
├── Device
├── Resource Manager
└── Node Graph
└── Sounds, Groups

```

---

### Sound

Управляемый экземпляр воспроизводимого аудио. Подключается к node graph, поддерживает управление громкостью, позицией,
питчем и др.

---

### Sound Group

Группа звуков для совместного управления (громкость, стоп, пауза). Примеры: SFX, Music, Voice.

---

### Data Source

Абстракция для любых источников аудиоданных. Реализуют общий интерфейс: декодеры, генераторы, кастомные источники.

---

### Decoder

Data source для чтения и декодирования аудиофайлов (WAV, FLAC, MP3).

---

### Encoder

Объект для кодирования аудиоданных в файл. miniaudio поддерживает только WAV.

---

### Resource Manager

Система для асинхронной загрузки, кэширования и управления аудиоресурсами.

---

### Node Graph

Система для построения сложных аудио-цепочек. Узлы (nodes) соединяются входами и выходами.

---

### Node

Узел в node graph. Может быть звуком, эффектом, микшером, группой.

---

### Endpoint

Финальный узел в node graph, подключённый к устройству вывода.

---

## Аудио характеристики

### Latency (Задержка)

Время между генерацией звука и его воспроизведением.

| Тип     | Значение | Применение       |
|---------|----------|------------------|
| Низкая  | < 10 ms  | Игры, интерактив |
| Средняя | 10-50 ms | Медиаплееры      |
| Высокая | > 50 ms  | Стриминг         |

---

### Period

Размер буфера устройства в frames. Определяет частоту вызова callback.

```

Period = 1024 frames
Sample Rate = 48000 Hz
Callback frequency = 48000 / 1024 ≈ 46.8 Hz (каждые ~21 ms)

```

Меньше period → меньше latency, больше CPU.

---

### Buffer

Область памяти для хранения аудиоданных перед воспроизведением или после захвата.

---

### Format (Формат)

Способ представления отдельного сэмпла.

| Формат | Диапазон                 | Точность |
|--------|--------------------------|----------|
| u8     | 0..255                   | Низкая   |
| s16    | -32768..+32767           | Средняя  |
| s24    | -8388608..+8388607       | Хорошая  |
| s32    | -2147483648..+2147483647 | Высокая  |
| f32    | -1.0..+1.0               | Высокая  |

**Рекомендация:** Используйте `ma_format_f32` для обработки.

---

### Channels (Каналы)

Количество независимых аудиопотоков.

| Название | Каналы |
|----------|--------|
| Моно     | 1      |
| Стерео   | 2      |
| 2.1      | 3      |
| 4.0      | 4      |
| 5.1      | 6      |
| 7.1      | 8      |

---

### Channel Map

Массив, определяющий назначение каждого канала (front left, front right, LFE и т.д.).

---

## Spatial Audio

### Listener

"Слушатель" в 3D пространстве. Определяет позицию и ориентацию для расчёта spatial audio.

---

### Attenuation (Затухание)

Уменьшение громкости с расстоянием.

| Модель      | Формула                             |
|-------------|-------------------------------------|
| None        | Нет затухания                       |
| Inverse     | gain = 1 / distance                 |
| Linear      | gain = 1 - (distance / maxDistance) |
| Exponential | gain = e^(-rolloff * distance)      |

---

### Doppler Effect

Изменение частоты звука при движении источника или слушателя. Рассчитывается на основе скорости.

---

### Cone

Направленная область звука или слушателя.

```

          Inner Cone (full gain)
               /\
              /  \
             /    \
            /      \
           /________\
          Outer Cone (reduced gain)

```

---

## Эффекты

### Filter (Фильтр)

Изменение спектра звука.

| Тип       | Действие                   |
|-----------|----------------------------|
| Low-pass  | Пропускает низкие частоты  |
| High-pass | Пропускает высокие частоты |
| Band-pass | Пропускает полосу частот   |
| Notch     | Подавляет полосу частот    |

---

### Biquad

Универсальный фильтр второго порядка. Основа для большинства audio filters.

---

### Pan (Панорамирование)

Распределение звука между каналами (обычно стерео: левый-правый).

---

### Resampling

Изменение sample rate аудиоданных.

---

## Концепции API

### Callback

Функция, вызываемая miniaudio для получения (playback) или отправки (capture) аудиоданных. Выполняется в отдельном
потоке.

---

### Config/init pattern

Паттерн инициализации объектов в miniaudio:

1. Создать config
2. Настроить поля config
3. Вызвать init с config

---

### Transparent structure

Структура без инкапсуляции. Память выделяется пользователем, поля доступны напрямую.

---

### Fire and forget

Воспроизведение звука без управления экземпляром. Звук загружается и воспроизводится автоматически.

```c
ma_engine_play_sound(&engine, "sound.wav", NULL);
```

---

### Duplex

Режим одновременной работы playback и capture.

---

### Loopback

Захват системного звука (только WASAPI).

---

### Exclusive mode

Режим эксклюзивного доступа к устройству. Минимальная latency, но устройство недоступно другим приложениям.

---

### Shared mode

Режим совместного доступа к устройству. Стандартный режим.

---

## Производительность

### Glitch

Артефакт звука (щелчок, заикание) из-за пропуска данных в callback.

---

### Dropout

Пропуск части аудиоданных из-за опоздания callback.

---

### Real-time

Режим работы с жёсткими требованиями к времени. Audio callback должен завершиться до следующего вызова.

---

### Zero-allocation

Отсутствие динамического выделения памяти в критичном коде (callback).

---

## Форматы файлов

### WAV

Waveform Audio File Format. Несжатый аудиоформат. miniaudio поддерживает все варианты.

---

### FLAC

Free Lossless Audio Codec. Сжатие без потерь. miniaudio поддерживает встроенно.

---

### MP3

MPEG Audio Layer III. Сжатие с потерями. miniaudio поддерживает встроенно.

---

### Vorbis

Сжатие с потерями, альтернатива MP3. Требует внешний декодер.

---

### Opus

Современный кодек для голоса и музыки. Требует внешний декодер.
