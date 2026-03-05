# miniaudio

**🟢 Уровень 1: Начинающий**

**miniaudio** — single-file библиотека на C для работы с аудио: воспроизведение, захват, декодирование, кодирование,
обработка. Не имеет зависимостей, кроме стандартной библиотеки. Поддерживает все основные платформы и бэкенды (WASAPI,
Core Audio, ALSA, PulseAudio, JACK, AAudio и др.).

Исходники: [miniaud.io](https://miniaud.io), [GitHub](https://github.com/mackron/miniaudio).

Версия: **0.11+**. Лицензия: Public Domain или MIT No Attribution.

---

## Содержание

### Базовая документация библиотеки

| Раздел                                       | Описание                                                               | Уровень |
|----------------------------------------------|------------------------------------------------------------------------|---------|
| [01. Быстрый старт](01_quickstart.md)        | Минимальные примеры: High-level API (Engine) и Low-level API (Device)  | 🟢      |
| [02. Основные понятия](02_concepts.md)       | Low-level vs High-level API, callback, конфигурация, паттерны          | 🟡      |
| [03. Справочник API](03_api-reference.md)    | Функции, структуры, константы с примерами использования                | 🟡      |
| [04. Источники данных](04_data-sources.md)   | Data sources, декодеры, энкодеры, ma_data_source interface             | 🟡      |
| [05. Продвинутые темы](05_advanced.md)       | Node Graph, Spatial Audio, Custom Decoders, Resource Manager           | 🔴      |
| [06. Решение проблем](06_troubleshooting.md) | Диагностика и исправление ошибок для всех платформ                     | 🟡      |
| [07. Глоссарий](07_glossary.md)              | Терминология аудио и miniaudio: Device, Context, Engine, Sample, Frame | 🟢      |

### Интеграция в ProjectV

| Раздел                                                  | Описание                                                      | Уровень |
|---------------------------------------------------------|---------------------------------------------------------------|---------|
| [08. Интеграция в ProjectV](08_projectv-integration.md) | CMake, SDL3, flecs, glm, Tracy — настройка и связка           | 🟡      |
| [09. Паттерны ProjectV](09_projectv-patterns.md)        | Sound pools, chunk audio, voxel spatial audio, ECS компоненты | 🟡      |

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
