# miniaudio: Аудио движок для высокопроизводительных приложений

> **Для понимания:** miniaudio — это как швейцарский нож для звука. Представьте, что звук в компьютере — это вода в
> трубах. miniaudio умеет и читать звуковые файлы (как декодер), и управлять потоком данных от микрофона к динамикам (
> как
> менеджер трубопроводов), и даже создавать сложные звуковые цепочки с эффектами (как микшерный пульт). При этом он
> работает на всём: от телефона до веб-браузера.

## Что такое miniaudio?

**miniaudio** — это single-file библиотека на C для работы со звуком. Она предоставляет всё необходимое для
воспроизведения, захвата, декодирования и обработки аудио без внешних зависимостей.

## Два уровня API

miniaudio предоставляет два принципиально разных подхода к работе со звуком. Выбор между ними — ключевое архитектурное
решение.

### High-Level API (ma_engine)

> **Для понимания:** High-level API — это как взять готовый конструктор LEGO. Всё собрано за вас: есть и кубики (звуки),
> и инструкция (движок), и даже музыкальная подложка (spatial audio). Вы просто говорите «play this sound» и получаете
> результат.

Идеально для:

- Быстрого прототипирования
- Игр (80% случаев)
- Когда не нужен контроль над каждым семплом

```c
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <print>

int main() {
    ma_engine engine;
    ma_engine_init(NULL, &engine);

    ma_engine_play_sound(&engine, "explosion.wav", NULL);

    std::print("Press Enter to exit...\n");
    getchar();

    ma_engine_uninit(&engine);
    return 0;
}
```

### Low-Level API (ma_device)

> **Для понимания:** Low-level API — это как построить свой собственный синтезатор с нуля. Вы сами решаете, откуда брать
> звук, как его обрабатывать и куда отправлять. Полный контроль, но и полная ответственность за каждую микросекунду.

Идеально для:

- Собственного DSP (цифровая обработка сигнала)
- Минимальной задержки (DAW, VST плагины)
- Интеграции с существующей аудиосистемой

```c
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <print>
#include <math.h>

void data_callback(ma_device* pDevice, void* pOutput,
                   const void* pInput, ma_uint32 frameCount) {
    // Генерация синусоиды 440 Hz
    float* output = (float*)pOutput;
    static float phase = 0.0f;
    float frequency = 440.0f;
    float sampleRate = (float)pDevice->sampleRate;

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        output[i * 2]     = sinf(phase);         // Left
        output[i * 2 + 1] = sinf(phase);         // Right
        phase += 2.0f * 3.14159f * frequency / sampleRate;
        if (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;
    }
}

int main() {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.dataCallback      = data_callback;

    ma_device device;
    ma_device_init(NULL, &config, &device);
    ma_device_start(&device);

    std::print("Playing 440Hz sine wave. Press Enter to exit...\n");
    getchar();

    ma_device_uninit(&device);
    return 0;
}
```

## Основные концепции

### Sample (Семпл)

> **Для понимание:** Семпл — это одно измерение звука в конкретный момент времени. Как одна фотография в видео. Для
> стерео (2 канала) один момент времени содержит два семпла: левый и правый.

```c
// Семпл в формате f32: значение от -1.0 до +1.0
float sample = 0.5f;  // Половина громкости
```

### Frame (Кадр)

> **Для понимания:** Кадр — это один «срез» звука по времени, содержащий все каналы. Как один кадр в фильме: для стерео
> это два семпла (левый + правый), для 5.1 — шесть семплов одновременно.

| Конфигурация | Семплов в кадре |
|--------------|-----------------|
| Моно         | 1               |
| Стерео       | 2               |
| 5.1          | 6               |
| 7.1          | 8               |

### Sample Rate (Частота дискретизации)

> **Для понимания:** Частота дискретизации — это как часто мы «фотографируем» звук. Чем чаще, тем точнее можем
> воспроизвести высокие частоты. 48000 Hz означает 48000 фотографий в секунду.

| Частота  | Применение            |
|----------|-----------------------|
| 8000 Hz  | Телефония (TTS)       |
| 22050 Hz | Старые игры           |
| 44100 Hz | Audio CD              |
| 48000 Hz | Профессиональный звук |
| 96000 Hz | Hi-Fi                 |

### Period (Период)

> **Для понимания:** Период — это размер порции данных, которую звуковая система запрашивает за один раз. Маленький
> период = маленькая задержка, но большая нагрузка на CPU. Большой период = стабильность, но заметная задержка.

```
Period = 512 frames
Sample Rate = 48000 Hz
Callback вызывается: 48000 / 512 ≈ 93.75 раз в секунду
Интервал между вызовами: ~10.67 мс
```

### Interleaved vs Planar

miniaudio использует **interleaved** формат:

```
Стерео: [L0, R0, L1, R1, L2, R2, ...]
```

Альтернатива (planar): `[L0, L1, L2, ...][R0, R1, R2, ...]`

## Форматы данных

### ma_format

| Формат          | Размер  | Описание                |
|-----------------|---------|-------------------------|
| `ma_format_u8`  | 1 байт  | Unsigned 8-bit (0..255) |
| `ma_format_s16` | 2 байта | Signed 16-bit           |
| `ma_format_s24` | 3 байта | Signed 24-bit           |
| `ma_format_s32` | 4 байта | Signed 32-bit           |
| `ma_format_f32` | 4 байта | Floating point (-1..1)  |

> **Для понимания:** f32 — стандарт для любой обработки звука. Целочисленные форматы нужны только для записи/чтения
> файлов. Используйте f32 внутри своего движка.

## Config/Init Pattern

> **Для понимания:** Это унифицированный паттерн инициализации. Сначала вы получаете «конфиг» (как рецепт), настраиваете
> его под себя, затем передаёте на «фабрику» (функцию init), которая создаёт объект. Конфиг можно выбросить после
> инициализации.

```c
// Шаг 1: Получить конфиг
ma_device_config config = ma_device_config_init(ma_device_type_playback);

// Шаг 2: Настроить
config.playback.format   = ma_format_f32;
config.playback.channels = 2;
config.sampleRate       = 48000;
config.dataCallback      = my_callback;

// Шаг 3: Инициализировать
ma_device device;
ma_result result = ma_device_init(NULL, &config, &device);
if (result != MA_SUCCESS) {
    std::println("Failed to init device: {}", ma_result_description(result));
    return;
}
```

## Transparent Structures

> **Для понимания:** В miniaudio нет «чёрных ящиков». Все структуры — обычные C-структуры, видимые целиком. Вы сами
> решаете, где выделять память: на стеке, в куче, в своём пуле.

```c
// Вариант 1: На стеке (для короткой жизни)
ma_device device;
ma_device_init(NULL, &config, &device);

// Вариант 2: В куче (для долгоживущих объектов)
ma_engine* pEngine = new ma_engine();
ma_engine_init(NULL, pEngine);

// Вариант 3: В своей памяти (DOD-подход)
alignas(64) unsigned char engine_buffer[sizeof(ma_engine)];
ma_engine* engine = new (engine_buffer) ma_engine();
```

## Data Callback

> **Для понимания:** Callback — это «точка входа» в ваш звуковой код. miniaudio вызывает её в специальном
> realtime-потоке и передаёт вам буфер. Вы должны заполнить его данными (playback) или прочитать из него (capture).

```c
void data_callback(ma_device* pDevice,
                   void* pOutput,      // Куда писать (playback) / откуда читать (capture)
                   const void* pInput, // Откуда читать (capture) / NULL (playback)
                   ma_uint32 frameCount) {

    // Тип устройства определяет поведение:
    // playback: pOutput = ваши данные, pInput = NULL
    // capture:  pOutput = игнорируется, pInput = данные с микрофона
    // duplex:   и то, и другое

    float* out = (float*)pOutput;
    for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {
        out[i] = 0.0f;  // Тишина
    }
}
```

### Ограничения в callback

> **Важно:** Callback работает в realtime потоке. Категорически запрещено:

- Выделять память (malloc, new)
- Вызывать функции ввода/вывода
- Использовать блокирующие примитивы синхронизации
- Вызывать ma_device_start/stop/uninit

Любое нарушение — звуковые глитчи (щелчки, заикания) или deadlock.

## Поддерживаемые платформы и бэкенды

| Платформа | Бэкенды                    |
|-----------|----------------------------|
| Windows   | WASAPI, DirectSound, WinMM |
| macOS/iOS | Core Audio                 |
| Linux     | ALSA, PulseAudio, JACK     |
| Android   | AAudio, OpenSL             |ES                   |
| Web       | Web Audio (Emscripten)     |
| BSD       | sndio, audio(4), OSS       |

### Приоритеты по умолчанию

```
Windows:    WASAPI → DirectSound → WinMM
macOS/iOS:  Core Audio
Linux:      ALSA → PulseAudio → JACK
Android:    AAudio → OpenSL|ES
```

## Декодирование (ma_decoder)

> **Для понимания:** Декодер — это «читатель» звуковых файлов. miniaudio умеет читать WAV, FLAC и MP3 «из коробки». Вы
> даёте ему файл, а он отдаёт вам чистые PCM-данные.

```c
ma_decoder_config decoderConfig = ma_decoder_config_init(
    ma_format_f32,  // В какой формат конвертировать
    2,              // Каналы
    48000           // Частота
);

ma_decoder decoder;
ma_result result = ma_decoder_init_file("music.flac", &decoderConfig, &decoder);
if (result != MA_SUCCESS) {
    std::println("Failed to load file: {}", ma_result_description(result));
    return;
}

// Читаем данные
float pcmData[1024];
ma_uint64 framesRead;
ma_decoder_read_pcm_frames(&decoder, pcmData, 1024, &framesRead);

ma_decoder_uninit(&decoder);
```

## High-Level: ma_engine и ma_sound

### ma_engine

> **Для понимания:** Engine — это «мозг» высокоуровневого API. Он объединяет устройство вывода, менеджер ресурсов и граф
> узлов (node graph) в одно целое.

```c
ma_engine_config config = ma_engine_config_init();
config.listenerCount = 1;              // Для spatial audio
config.sampleRate   = 48000;
config.format       = ma_format_f32;
config.channels     = 2;

ma_engine engine;
ma_engine_init(&config, &engine);
```

### ma_sound

> **Для понимания:** Sound — это «звуковой объект» с полным контролем. В отличие от fire-and-forget (
> ma_engine_play_sound), здесь вы управляете громкостью, позицией, зацикливанием.

```c
ma_sound sound;
ma_sound_init_from_file(&engine, "footstep.wav",
                        MA_SOUND_FLAG_DECODE, NULL, &sound);

ma_sound_set_volume(&sound, 0.8f);
ma_sound_set_pitch(&sound, 1.2f);  // Ускорение
ma_sound_set_looping(&sound, MA_TRUE);

ma_sound_start(&sound);
// ... позже ...
ma_sound_stop(&sound);
ma_sound_uninit(&sound);
```

### ma_sound_group

> **Для понимания:** Группа — это «пульт» для нескольких звуков сразу. Все SFX идут через одну группу, музыка — через
> другую. Изменили громкость группы — изменилась громкость всех входящих звуков.

```c
ma_sound_group sfxGroup;
ma_sound_group_init(&engine, 0, NULL, &sfxGroup);
ma_sound_group_set_volume(&sfxGroup, 0.5f);

// Звук в группе
ma_sound sound;
ma_sound_init_from_file(&engine, "explosion.wav", 0, &sfxGroup, &sound);
ma_sound_start(&sound);
```

## Spatial Audio

> **Для понимания:** Spatial audio — это 3D-звук. Представьте, что вы стоите посреди леса: птица слева, ручей справа,
> ветер сзади. miniaudio рассчитывает задержку и громкость для каждого канала, создавая иллюзию пространства.

```c
// Настройка listener (игрок)
ma_engine_listener_set_position(&engine, 0, 0.0f, 0.0f, 0.0f);
ma_engine_listener_set_direction(&engine, 0, 0.0f, 0.0f, -1.0f);

// Настройка звука (источник)
ma_sound sound;
ma_sound_init_from_file(&engine, "bird.wav", 0, NULL, &sound);
ma_sound_set_position(&sound, -5.0f, 2.0f, 0.0f);  // Слева от игрока
ma_sound_set_spatialization_enabled(&sound, MA_TRUE);
ma_sound_set_attenuation_model(&sound, ma_attenuation_model_inverse);
ma_sound_set_rolloff(&sound, 1.0f);
ma_sound_set_min_distance(&sound, 1.0f);
ma_sound_set_max_distance(&sound, 100.0f);
ma_sound_start(&sound);
```

## Коды ошибок (ma_result)

| Код                       | Описание                 |
|---------------------------|--------------------------|
| `MA_SUCCESS`              | Успех                    |
| `MA_ERROR`                | Общая ошибка             |
| `MA_INVALID_ARGS`         | Неверные аргументы       |
| `MA_NO_BACKEND`           | Бэкенд не найден         |
| `MA_NO_DEVICE`            | Устройство не найдено    |
| `MA_DEVICE_IN_USE`        | Устройство занято        |
| `MA_OUT_OF_MEMORY`        | Не хватает памяти        |
| `MA_FORMAT_NOT_SUPPORTED` | Формат не поддерживается |

```c
const char* desc = ma_result_description(result);
std::println("Audio error: {}", desc);
```

## Node Graph (продвинутый уровень)

> **Для понимания:** Node graph — это «конструктор» для звука. Каждый узел (node) что-то делает: читает файл, применяет
> фильтр, смешивает каналы. Соединяете узлы проводами — получаете сложную звуковую цепочку.

```
[Sound File] → [Gain Node] → [Spatial Node] → [Master Output]
                     ↑                    ↑
               (громкость)          (3D позиция)
```

Для базовых нужд достаточно ma_engine. Node graph используется когда нужно:

- Кастомные эффекты
- Сложное микширование
- Интеграция с другими аудиосистемами

## Резюме

1. **High-level API** (ma_engine) — для игр и простых приложений
2. **Low-level API** (ma_device + callback) — для DSP и минимальной latency
3. **Config/init** — единый паттерн инициализации
4. **Transparent structures** — вы контролируете память
5. **f32 format** — рекомендуемый формат для обработки
6. **Никаких аллокаций в callback** — realtime safety
