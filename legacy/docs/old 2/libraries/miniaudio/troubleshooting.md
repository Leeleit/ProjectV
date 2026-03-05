# Решение проблем

Частые ошибки при использовании miniaudio и способы их исправления.

## На этой странице

- [Диагностика](#диагностика)
- [Инициализация](#инициализация)
  - [ma_device_init / ma_context_init возвращают ошибку](#ma_device_init--ma_context_init-возвращают-ошибку)
  - [ma_engine_init возвращает ошибку](#ma_engine_init-возвращает-ошибку)
- [Runtime](#runtime)
  - [Зависание / deadlock в callback](#зависание--deadlock-в-callback)
  - [Нет звука при воспроизведении](#нет-звука-при-воспроизведении)
  - [Щелчки или тишина в конце файла](#щелчки-или-тишина-в-конце-файла)
  - [Full-duplex глитчи](#full-duplex-глитчи)
  - [Большая latency](#большая-latency)
- [Сборка и линковка](#сборка-и-линковка)
  - [Ошибка undefined reference __atomic_load_8](#ошибка-undefined-reference-__atomic_load_8)
  - [Ошибка dlopen / не найдена библиотека](#ошибка-dlopen--не-найдена-библиотека)
  - [UWP: ActivateAudioInterfaceAsync](#uwp-activateaudiointerfaceasync)
  - [Emscripten: -std=c* не использовать](#emscripten--stdc-не-использовать)
- [Платформы](#платформы)
  - [macOS notarization](#macos-notarization)
  - [Android OpenSL\|ES не инициализируется](#android-opensles-не-инициализируется)
- [Коды ma_result](#коды-ma_result)
- [См. также](#см-также)

---

## Диагностика

Для получения текстового описания ошибки используйте `ma_result_description(result)`:

```c
ma_result result = ma_device_init(NULL, &config, &device);
if (result != MA_SUCCESS) {
    printf("ma_device_init failed: %s\n", ma_result_description(result));
    return -1;
}
```

---

## Инициализация

### ma_device_init / ma_context_init возвращают ошибку

**Причины:** Нет доступного аудиоустройства, устройство занято другим приложением, бэкенд не поддерживается на
платформе.

**Решение:**

1. Проверьте код результата: `if (ma_device_init(...) != MA_SUCCESS) { /* ... */ }`. Можно использовать
   `ma_result_description()` для текстового описания.
2. Убедитесь, что нет другого приложения, эксклюзивно использующего устройство (некоторые программы блокируют WASAPI в
   exclusive mode).
3. На Linux: проверьте, что ALSA или PulseAudio доступны. Установите `libasound2-dev` / `libpulse-dev` при сборке с
   этими бэкендами.
4. На виртуальной машине или headless-системе: аудио может быть недоступно. Можно включить null backend для тестов.
5. При `MA_DEVICE_IN_USE` — устройство уже используется; попробуйте другое или закройте приложение, его захватившее.

---

### ma_engine_init возвращает ошибку

**Причины:** Engine внутри создаёт device и resource manager. Ошибка обычно та же, что и при `ma_device_init` или
нехватка памяти.

**Решение:**

1. Следуйте рекомендациям для [ma_device_init](#ma_device_init--ma_context_init-возвращают-ошибку).
2. Проверьте, что не отключены `MA_NO_ENGINE`, `MA_NO_RESOURCE_MANAGER`, `MA_NO_NODE_GRAPH` (если используете опции
   сборки).

---

## Runtime

### Зависание / deadlock в callback

**Причина:** Внутри `data_callback` вызываются `ma_device_start`, `ma_device_stop`, `ma_device_uninit` или
`ma_device_init`. Это приводит к deadlock — callback выполняется в том же потоке, который управляет устройством.

**Решение:** Никогда не вызывайте в callback:

- `ma_device_init` / `ma_device_init_ex`
- `ma_device_uninit`
- `ma_device_start`
- `ma_device_stop`

Вместо остановки установите флаг или сигнализируйте событие, а остановку выполняйте в другом потоке (например, в main
loop):

```c
// В callback:
volatile int g_stop_requested = 0;
void data_callback(...) {
    if (g_stop_requested) { /* заполнить тишиной и выйти */ return; }
    // ...
}

// В main/другом потоке:
g_stop_requested = 1;
// Подождать завершения callback, затем:
ma_device_stop(&device);
ma_device_uninit(&device);
```

Также избегайте блокирующих операций в callback (malloc, file I/O, мьютексы с ожиданием) — callback выполняется в
real-time потоке.

---

### Нет звука при воспроизведении

**Причины:** Устройство не запущено; в callback не записываются данные; неверный формат; громкость 0.

**Решение:**

1. После `ma_device_init` обязательно вызовите `ma_device_start`. Устройство по умолчанию в stopped.
2. В data_callback для playback записывайте данные в `pOutput`. Если ничего не писать — тишина.
3. Убедитесь, что формат и число каналов в config совпадают с тем, что вы записываете.
4. Проверьте системную громкость и громкость приложения. `ma_device_set_master_volume` / `ma_device_get_master_volume`
   для Low-level; для engine — настройки звуков и групп.

---

### Щелчки или тишина в конце файла

**Причина:** `ma_decoder_read_pcm_frames` возвращает меньше `frameCount` (конец файла) или 0 frames (`MA_AT_END`). Если
не заполнить оставшуюся часть `pOutput` тишиной — будет мусор или щелчок.

**Решение:** В data_callback после чтения из декодера проверяйте `pFramesRead`. Если меньше `frameCount`, обнулите
остаток буфера:

```c
ma_uint64 framesRead;
ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, &framesRead);
if (framesRead < frameCount)
    memset((char*)pOutput + framesRead * bytesPerFrame, 0, (frameCount - framesRead) * bytesPerFrame);
```

---

### Full-duplex глитчи

**Причина:** В duplex режиме форматы playback и capture могут отличаться. miniaudio не выполняет конвертацию между
ними — нужно вручную.

**Решение:** Либо задайте одинаковые форматы в `config.playback.*` и `config.capture.*`, либо реализуйте конвертацию в
callback (например, через `ma_channel_converter`, `ma_resampler`).

---

### Большая latency

**Причина:** Размер буфера (period) слишком большой.

**Решение:** Уменьшите `periodSizeInFrames` или `periodSizeInMilliseconds` в `ma_device_config`. Убедитесь, что
`performanceProfile` = `ma_performance_profile_low_latency` (default). Учтите: меньший буфер — выше нагрузка на CPU и
риск глитчей.

---

## Сборка и линковка

### Ошибка undefined reference __atomic_load_8

**Причина:** На 32-bit ARM (и некоторых других платформах) для 64-bit атомарных операций нужна библиотека `libatomic`.

**Решение:** Добавьте линковку с `-latomic`:

```cmake
target_link_libraries(ProjectV PRIVATE atomic)
```

Или для GCC/Clang: `-latomic` в `target_link_libraries` или `link_libraries`.

---

### Ошибка dlopen / не найдена библиотека

**Причина:** miniaudio по умолчанию использует runtime linking (`dlopen`) для загрузки бэкендов (ALSA, PulseAudio и
т.д.). Если библиотека не найдена или путь неверный — инициализация бэкенда может провалиться.

**Решение:**

1. Убедитесь, что нужные библиотеки установлены (например, `libasound.so`, `libpulse.so` на Linux).
2. При необходимости отключите runtime linking и линкуйте явно:
   ```c
   #define MA_NO_RUNTIME_LINKING
   ```
   Затем линкуйте с `-lpulse`, `-lasound` и т.д. вручную.
   См. [Интеграция — Опции сборки](integration.md#5-опции-сборки).
3. На Linux при ручной сборке нужны `-ldl`, `-lpthread`, `-lm`.

---

### UWP: ActivateAudioInterfaceAsync

**Причина:** При сборке под UWP (Universal Windows Platform) может возникать unresolved external symbol для
`ActivateAudioInterfaceAsync`.

**Решение:** Добавьте линковку с `mmdevapi.lib`.

---

### Emscripten: -std=c* не использовать

**Причина:** Emscripten не поддерживает флаги `-std=c89`, `-std=c99`, `-ansi` при компиляции с miniaudio (конфликты с
некоторыми типами).

**Решение:** Не передавайте эти флаги для файлов, включающих miniaudio.

---

## Платформы

### macOS notarization

**Причина:** Apple notarization может отклонять приложения, использующие runtime linking (`dlopen`) для загрузки
фреймворков.

**Решение (один из вариантов):**

1. Отключите runtime linking: `#define MA_NO_RUNTIME_LINKING` (или `MINIAUDIO_NO_RUNTIME_LINKING` в CMake). Затем
   линкуйте явно:
   ```
   -framework CoreFoundation -framework CoreAudio -framework AudioToolbox
   ```
   На старых iOS попробуйте `-framework AudioUnit` вместо AudioToolbox.
2. Либо добавьте в entitlements:
   ```
   <key>com.apple.security.cs.allow-dyld-environment-variables</key>
   <true/>
   <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
   <true/>
   ```
   См. [обсуждение на GitHub](https://github.com/mackron/miniaudio/issues/203).

---

### Android OpenSL|ES не инициализируется

**Причина:** На некоторых устройствах `dlopen("libOpenSLES.so")` не срабатывает.

**Решение:** Отключите runtime linking и линкуйте с OpenSL ES явно:

```c
#define MA_NO_RUNTIME_LINKING
```

Добавьте `-lOpenSLES` в линковку.

---

## Коды ma_result

Типичные коды ошибок при инициализации и работе с устройствами:

| Код                                 | Когда                           | Что проверить                              |
|-------------------------------------|---------------------------------|--------------------------------------------|
| `MA_NO_BACKEND`                     | Нет подходящего бэкенда         | Поддержка платформы, опции MA_NO_*         |
| `MA_NO_DEVICE`                      | Устройство не найдено           | Наличие аудиоустройства                    |
| `MA_DEVICE_IN_USE`                  | Устройство занято               | Закрыть другие приложения                  |
| `MA_FAILED_TO_INIT_BACKEND`         | Ошибка инициализации бэкенда    | Драйверы, библиотеки (libpulse, libasound) |
| `MA_FAILED_TO_OPEN_BACKEND_DEVICE`  | Не удалось открыть устройство   | Права доступа, эксклюзивный режим          |
| `MA_FAILED_TO_START_BACKEND_DEVICE` | Не удалось запустить устройство | Драйвер, конфликт с другим приложением     |
| `MA_FORMAT_NOT_SUPPORTED`           | Формат не поддерживается        | Выбрать другой ma_format                   |
| `MA_INVALID_ARGS`                   | Неверные аргументы              | Проверить config, указатели                |
| `MA_AT_END`                         | Data source: прочитано 0 frames | Конец файла — заполнять буфер тишиной      |

## Советы по отладке из FIXES.md

### Дополнительная диагностика проблем

**Проверка MA_MAX_CHANNELS:**
Значение по умолчанию — 254, но вы можете переопределить его перед включением miniaudio.h:

```c
#define MA_MAX_CHANNELS 512  // Увеличить максимальное число каналов
#include "miniaudio.h"
```

**Полные примеры использования ma_result_description:**

```c
ma_result result = ma_sound_init_from_file(&engine, "sound.wav", 0, NULL, NULL, &sound);
if (result != MA_SUCCESS) {
    printf("Ошибка загрузки звука: %s (код: %d)\n",
           ma_result_description(result), result);

    // Дополнительная диагностика
    if (result == MA_NO_BACKEND) {
        printf("Проверьте аудиодрайверы на вашей системе\n");
    } else if (result == MA_FILE_NOT_FOUND) {
        printf("Файл 'sound.wav' не найден\n");
    }
}
```

**Проверка работы ma_engine_play_sound:**

```c
// Правильное использование с проверкой
ma_result result = ma_engine_play_sound(&engine, "sound.wav", NULL);
if (result != MA_SUCCESS) {
    printf("Ошибка воспроизведения: %s\n", ma_result_description(result));

    // Альтернативный подход с явной инициализацией sound
    ma_sound sound;
    result = ma_sound_init_from_file(&engine, "sound.wav", 0, NULL, NULL, &sound);
    if (result == MA_SUCCESS) {
        ma_sound_start(&sound);
        // ... управление звуком ...
        ma_sound_uninit(&sound);
    }
}
```

### Общие рекомендации по отладке

1. **Включите отладочный вывод:**

```c
#define MA_DEBUG_OUTPUT  // Вывод отладочных сообщений в stdout
#include "miniaudio.h"
```

2. **Проверьте версию miniaudio:**

```c
printf("miniaudio версия: %s\n", MA_VERSION_STRING);
```

3. **Тестирование с минимальным примером:**
   Создайте минимальную программу для изоляции проблемы:

```c
#include "miniaudio.h"
#include <stdio.h>

int main() {
    ma_engine engine;
    ma_result result = ma_engine_init(NULL, &engine);
    if (result == MA_SUCCESS) {
        printf("✅ Базовая инициализация работает\n");
        ma_engine_uninit(&engine);
        return 0;
    } else {
        printf("❌ Ошибка: %s\n", ma_result_description(result));
        return 1;
    }
}
```

---

## См. также

- [Интеграция](integration.md) — CMake, опции сборки
- [Основные понятия — Ограничения в callback](concepts.md#ограничения-в-callback)
- [Справочник API](api-reference.md) — коды ma_result
- [Быстрый старт](quickstart.md) — примеры кода с полной обработкой ошибок
- [Примеры кода](../examples/) — дополнительные примеры для отладки
