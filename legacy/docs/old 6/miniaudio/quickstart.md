# Быстрый старт miniaudio

**🟢 Уровень 1: Начинающий**

## На этой странице

- [CMake интеграция](#cmake-интеграция)
- [High-Level API (Engine)](#high-level-api-engine)
- [Low-Level API (Device)](#low-level-api-device)
- [Декодер для файлов](#декодер-для-файлов)
- [Обработка ошибок](#обработка-ошибок)
- [Следующие шаги](#следующие-шаги)
- [См. также](#см-также)

---

## CMake интеграция

### Простейший способ

```cmake
add_subdirectory(external/miniaudio)
target_link_libraries(YourApp PRIVATE miniaudio)
```

### Single-file реализация

Если вы не используете CMake, добавьте в один файл вашего проекта:

```c
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

---

## High-Level API (Engine)

### Простейшее воспроизведение звука

```c
#include "miniaudio.h"
#include <stdio.h>

int main() {
    ma_engine engine;
    ma_result result;
    
    // 1. Инициализация engine
    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Ошибка инициализации engine: %s\n", ma_result_description(result));
        return -1;
    }
    
    // 2. Воспроизведение звука
    result = ma_engine_play_sound(&engine, "sound.wav", NULL);
    if (result != MA_SUCCESS) {
        printf("Ошибка воспроизведения: %s\n", ma_result_description(result));
        ma_engine_uninit(&engine);
        return -1;
    }
    
    // 3. Ожидание (для демонстрации)
    printf("Воспроизведение звука... Нажмите Enter для выхода.\n");
    getchar();
    
    // 4. Очистка
    ma_engine_uninit(&engine);
    return 0;
}
```

### Управление экземпляром звука

```c
#include "miniaudio.h"
#include <stdio.h>

int main() {
    ma_engine engine;
    ma_sound sound;
    
    // Инициализация engine
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
        printf("Ошибка инициализации engine\n");
        return -1;
    }
    
    // Инициализация звука с файла
    if (ma_sound_init_from_file(&engine, "music.wav", 0, NULL, NULL, &sound) != MA_SUCCESS) {
        printf("Ошибка загрузки звука\n");
        ma_engine_uninit(&engine);
        return -1;
    }
    
    // Запуск воспроизведения
    ma_sound_start(&sound);
    
    // Ожидание 5 секунд
    printf("Воспроизведение...\n");
    ma_sleep(5000);
    
    // Остановка и очистка
    ma_sound_stop(&sound);
    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
    
    return 0;
}
```

---

## Low-Level API (Device)

### Простой playback

```c
#include "miniaudio.h"
#include <stdio.h>
#include <string.h>

// Генерация простого синусоидального тона
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    static float phase = 0.0f;
    float* pOutputF32 = (float*)pOutput;
    float frequency = 440.0f; // Ля (440 Гц)
    float amplitude = 0.1f;
    
    for (ma_uint32 i = 0; i < frameCount; i++) {
        // Синусоидальный тон
        pOutputF32[i * 2] = sinf(phase) * amplitude;     // Левый канал
        pOutputF32[i * 2 + 1] = sinf(phase) * amplitude; // Правый канал
        
        phase += 2.0f * MA_PI * frequency / pDevice->sampleRate;
        if (phase > 2.0f * MA_PI) {
            phase -= 2.0f * MA_PI;
        }
    }
}

int main() {
    ma_device_config config;
    ma_device device;
    ma_result result;
    
    // 1. Конфигурация устройства
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;      // 32-bit floating point
    config.playback.channels = 2;                // Стерео
    config.sampleRate = 48000;                   // 48 kHz
    config.dataCallback = data_callback;         // Наш callback
    config.pUserData = NULL;                     // Пользовательские данные
    
    // 2. Инициализация устройства
    result = ma_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {
        printf("Ошибка инициализации устройства: %s\n", ma_result_description(result));
        return -1;
    }
    
    // 3. Запуск устройства
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("Ошибка запуска устройства: %s\n", ma_result_description(result));
        ma_device_uninit(&device);
        return -1;
    }
    
    // 4. Ожидание
    printf("Воспроизведение тона 440 Гц... Нажмите Enter для выхода.\n");
    getchar();
    
    // 5. Остановка и очистка
    ma_device_stop(&device);
    ma_device_uninit(&device);
    
    return 0;
}
```

### Воспроизведение WAV файла

```c
#include "miniaudio.h"
#include <stdio.h>
#include <string.h>

int main() {
    ma_decoder decoder;
    ma_device_config config;
    ma_device device;
    ma_result result;
    
    // 1. Декодирование WAV файла
    result = ma_decoder_init_file("sound.wav", NULL, &decoder);
    if (result != MA_SUCCESS) {
        printf("Ошибка декодирования файла: %s\n", ma_result_description(result));
        return -1;
    }
    
    // 2. Конфигурация устройства
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = decoder.outputFormat;
    config.playback.channels = decoder.outputChannels;
    config.sampleRate = decoder.outputSampleRate;
    
    // Callback для чтения из декодера
    config.dataCallback = [](ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
        if (pDecoder) {
            ma_uint64 framesRead;
            ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, &framesRead);
            
            // Если прочитано меньше кадров (конец файла), заполняем тишиной
            if (framesRead < frameCount) {
                size_t bytesPerFrame = ma_get_bytes_per_frame(pDecoder->outputFormat, pDecoder->outputChannels);
                memset((char*)pOutput + framesRead * bytesPerFrame, 0, (frameCount - framesRead) * bytesPerFrame);
            }
        }
    };
    
    config.pUserData = &decoder;
    
    // 3. Инициализация устройства
    result = ma_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {
        printf("Ошибка инициализации устройства: %s\n", ma_result_description(result));
        ma_decoder_uninit(&decoder);
        return -1;
    }
    
    // 4. Запуск устройства
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("Ошибка запуска устройства: %s\n", ma_result_description(result));
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
        return -1;
    }
    
    // 5. Ожидание окончания файла (примерно)
    printf("Воспроизведение файла... Нажмите Enter для досрочного выхода.\n");
    getchar();
    
    // 6. Остановка и очистка
    ma_device_stop(&device);
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
    
    return 0;
}
```

---

## Декодер для файлов

### Чтение и декодирование файла

```c
#include "miniaudio.h"
#include <stdio.h>

int main() {
    ma_decoder decoder;
    ma_result result;
    
    // Инициализация декодера
    result = ma_decoder_init_file("audio.wav", NULL, &decoder);
    if (result != MA_SUCCESS) {
        printf("Ошибка: %s\n", ma_result_description(result));
        return -1;
    }
    
    // Информация о файле
    printf("Формат: %s\n", (decoder.outputFormat == ma_format_f32) ? "f32" : "s16");
    printf("Каналы: %u\n", decoder.outputChannels);
    printf("Частота: %u Hz\n", decoder.outputSampleRate);
    
    // Чтение первых 1024 кадров
    float buffer[1024 * 2]; // Для стерео
    ma_uint64 framesRead;
    
    result = ma_decoder_read_pcm_frames(&decoder, buffer, 1024, &framesRead);
    if (result == MA_SUCCESS) {
        printf("Прочитано %llu кадров\n", framesRead);
    }
    
    // Очистка
    ma_decoder_uninit(&decoder);
    return 0;
}
```

---

## Обработка ошибок

### Правильная проверка ошибок

```c
ma_result result;

result = ma_engine_init(NULL, &engine);
if (result != MA_SUCCESS) {
    // Используйте ma_result_description для понятного сообщения
    printf("Ошибка: %s (код: %d)\n", ma_result_description(result), result);
    
    // Специфичная обработка
    switch (result) {
        case MA_NO_BACKEND:
            printf("Аудио бэкенд не найден. Проверьте драйверы.\n");
            break;
        case MA_NO_DEVICE:
            printf("Аудио устройство не найдено.\n");
            break;
        case MA_DEVICE_IN_USE:
            printf("Устройство занято другим приложением.\n");
            break;
        default:
            printf("Неизвестная ошибка.\n");
    }
    return -1;
}
```

---

## Следующие шаги

### После освоения быстрого старта:

1. **Изучите архитектуру**: [Основные понятия](concepts.md) — Low-level vs High-level API, callback паттерны
2. **Настройте сборку**: [Интеграция](integration.md) — CMake, линковка для разных платформ
3. **Решите проблемы**: [Troubleshooting](troubleshooting.md) — диагностика и исправление ошибок
4. **Выберите оптимальный подход**: [Decision Trees](decision-trees.md) — руководство по выбору API
5. **Изучите практические сценарии**: [Use Cases](use-cases.md) — примеры для игр, медиаплееров и инструментов

---

## См. также

- [Основные понятия](concepts.md) — архитектура miniaudio
- [Глоссарий](glossary.md) — терминология и определения
- [Справочник API](api-reference.md) — полный список функций
- [Интеграция](integration.md) — настройка сборки для разных платформ
- [Troubleshooting](troubleshooting.md) — решение проблем и отладка
- [Примеры кода](../examples/) — готовые примеры интеграции

```
