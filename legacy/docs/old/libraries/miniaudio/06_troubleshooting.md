# Решение проблем

**🟡 Уровень 2: Средний**

Диагностика и исправление ошибок для всех платформ.

---

## Общие проблемы

### MA_NO_BACKEND

**Симптом:** `ma_device_init` или `ma_context_init` возвращает `MA_NO_BACKEND`.

**Причины и решения:**

1. **Нет аудио драйверов**

- Windows: установите драйверы звуковой карты или проверьте WASAPI/DirectSound
- Linux: проверьте ALSA (`aplay -l`) или PulseAudio (`pactl info`)
- macOS: проверьте Core Audio

2. **Бэкенд явно отключен макросами**
   ```c
   // Проверьте, не определены ли:
   // MA_ENABLE_ONLY_SPECIFIC_BACKENDS
   // или MA_NO_WASAPI, MA_NO_DSOUND и т.д.
   ```

3. **Неправильный порядок бэкендов**
   ```c
   // Явно укажите бэкенды:
   ma_backend backends[] = { ma_backend_wasapi, ma_backend_dsound };
   ma_context_init(NULL, 2, backends, &context);
   ```

---

### MA_NO_DEVICE

**Симптом:** `MA_NO_DEVICE` при инициализации устройства.

**Причины и решения:**

1. **Устройство отключено или не существует**

- Проверьте список устройств через `ma_context_get_devices`

2. **Неверный Device ID**
   ```c
   // Убедитесь, что ID валиден:
   if (pPlaybackInfos[index].isDefault) {
       config.playback.pDeviceID = NULL;  // NULL = default
   } else {
       config.playback.pDeviceID = &pPlaybackInfos[index].id;
   }
   ```

---

### MA_DEVICE_IN_USE

**Симптом:** Устройство занято другим приложением.

**Решения:**

1. **Используйте shared mode (по умолчанию)**
   ```c
   config.playback.shareMode = ma_share_mode_shared;
   ```

2. **Попробуйте другое устройство**

3. **Закройте приложения, использующие аудио**

---

### MA_FORMAT_NOT_SUPPORTED

**Симптом:** Формат не поддерживается устройством.

**Решения:**

1. **Используйте ma_format_f32 или ma_format_s16**
   ```c
   config.playback.format = ma_format_f32;  // Обычно поддерживается
   ```

2. **Декодируйте в поддерживаемый формат**
   ```c
   ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);
   ```

---

### Нет звука

**Симптом:** Ошибок нет, но звука не слышно.

**Диагностика:**

1. **Устройство запущено?**
   ```c
   if (!ma_device_is_started(&device)) {
       ma_device_start(&device);
   }
   ```

2. **Callback вызывается?**
   ```c
   void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
       static int callCount = 0;
       callCount++;  // Поставьте breakpoint или лог
       // ...
   }
   ```

3. **Данные не нулевые?**
   ```c
   // Проверьте, что буфер заполняется:
   for (ma_uint32 i = 0; i < frameCount * channels; i++) {
       ((float*)pOutput)[i] = 0.1f;  // Тестовый тон
   }
   ```

4. **Громкость в системе**

- Проверьте микшер Windows / PulseAudio / Core Audio
- Проверьте, не muted ли приложение

5. **Громкость в miniaudio**
   ```c
   ma_sound_set_volume(&sound, 1.0f);  // Полная громкость
   ma_sound_group_set_volume(&group, 1.0f);
   ```

---

## Платформенные проблемы

### Windows

#### WASAPI exclusive mode зависает

**Решение:** Используйте shared mode или корректно обрабатывайте переключение:

```c
config.playback.shareMode = ma_share_mode_shared;
```

#### WinMM: высокий latency

**Решение:** Используйте WASAPI или DirectSound:

```c
ma_backend backends[] = { ma_backend_wasapi, ma_backend_dsound };
```

#### Проблемы с Unicode путями

**Решение:** Используйте широкие символы:

```c
ma_decoder_init_file_w(L"путь/к/файлу.wav", NULL, &decoder);
```

---

### Linux

#### Нет звука в ALSA

**Диагностика:**

```bash
aplay -l              # Список устройств
aplay test.wav        # Тест ALSA напрямую
```

**Решения:**

1. Проверьте права доступа к `/dev/snd/*`
2. Добавьте пользователя в группу `audio`
3. Используйте PulseAudio:
   ```c
   ma_backend backends[] = { ma_backend_pulseaudio, ma_backend_alsa };
   ```

#### PulseAudio: устройство занято

**Решение:**

```bash
pulseaudio -k         # Перезапуск PulseAudio
```

#### JACK: не может подключиться

**Решения:**

1. Убедитесь, что JACK сервер запущен
2. Используйте ALSA напрямую или через PulseAudio bridge

#### Ошибки линковки

```
undefined reference to `pthread_create'
undefined reference to `dlopen'
```

**Решение:** Добавьте библиотеки:

```cmake
target_link_libraries(YourApp PRIVATE pthread dl m)
```

---

### macOS / iOS

#### Нет звука на macOS

**Проверки:**

1. System Preferences → Sound → Output
2. Разрешения приложения (Microphone для capture)

#### iOS: AVAudioSession

**Важно:** На iOS нужно настроить AVAudioSession:

```objc
#import <AVFoundation/AVFoundation.h>

AVAudioSession* session = [AVAudioSession sharedInstance];
[session setCategory:AVAudioSessionCategoryPlayback error:nil];
[session setActive:YES error:nil];
```

#### iOS: нет звука в фоне

**Решение:** Включите background audio в Info.plist:

```xml
<key>UIBackgroundModes</key>
<array>
    <string>audio</string>
</array>
```

---

### Android

#### OpenSL|ES: не работает на новых Android

**Решение:** Используйте AAudio (Android 8.0+):

```c
ma_backend backends[] = { ma_backend_aaudio, ma_backend_opensl };
```

#### Нет разрешения на запись

**Решение:** Добавьте в AndroidManifest.xml:

```xml
<uses-permission android:name="android.permission.RECORD_AUDIO" />
<uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS" />
```

#### Проблемы с потоком

На Android callback может вызываться из Java-потока. Убедитесь, что callback не блокируется.

---

### Emscripten / Web

#### Не работает в браузере

**Требования:**

1. User interaction для разблокировки аудио
2. HTTPS или localhost для некоторых функций

**Решение:**

```javascript
// Разблокировка после клика
document.addEventListener('click', function() {
    // Воспроизвести звук через miniaudio
}, { once: true });
```

#### Проблемы с размером буфера

Web Audio может требовать определённые размеры буферов. Используйте auto настройки.

---

## Проблемы производительности

### Glitches / Dropouts

**Причины:**

1. Слишком большой период обработки
2. Блокирующие операции в callback
3. Недостаточный приоритет потока

**Решения:**

1. **Уменьшите период**
   ```c
   config.periodSizeInFrames = 256;  // Меньше = ниже latency, но больше CPU
   ```

2. **Не блокируйте в callback**
   ```c
   // ПЛОХО:
   void data_callback(...) {
       malloc(...);      // Блокирует!
       file_read(...);   // Блокирует!
       mutex_lock(...);  // Блокирует!
   }

   // ХОРОШО:
   void data_callback(...) {
       // Только обработка данных
   }
   ```

3. **Повысить приоритет потока**
   ```c
   ma_context_config contextConfig = ma_context_config_init();
   contextConfig.threadPriority = ma_thread_priority_realtime;
   ```

---

### Высокое потребление CPU

**Диагностика:**

1. Профилируйте callback
2. Проверьте количество активных звуков

**Решения:**

1. **Используйте High-level API** вместо ручного микширования
2. **Остановите неиспользуемые звуки**
   ```c
   ma_sound_stop(&sound);
   ```
3. **Уменьшите sample rate**
   ```c
   config.sampleRate = 44100;  // Вместо 48000
   ```

---

### Утечки памяти

**Проверьте:**

1. Все `ma_xxx_init` имеют парные `ma_xxx_uninit`
2. Звуки, созданные через `ma_engine_play_sound`, управляются автоматически
3. Звуки, созданные через `ma_sound_init_from_file`, требуют `ma_sound_uninit`

```c
// Не забывайте:
ma_sound_uninit(&sound);
ma_engine_uninit(&engine);
ma_device_uninit(&device);
ma_decoder_uninit(&decoder);
```

---

## Отладка

### Логирование

```c
void log_callback(void* pUserData, ma_uint32 level, const char* pMessage) {
    printf("[miniaudio] %s\n", pMessage);
}

ma_context_config config = ma_context_config_init();
config.logCallback = log_callback;
```

### Проверка callback

```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Занулить буфер в начале (если noPreZeroedOutputBuffer = false, это уже сделано)
    // memset(pOutput, 0, frameCount * bytesPerFrame);

    // Ваш код...

    // Проверка на clipping
    float* pOut = (float*)pOutput;
    for (ma_uint32 i = 0; i < frameCount * channels; i++) {
        if (pOut[i] > 1.0f) pOut[i] = 1.0f;
        if (pOut[i] < -1.0f) pOut[i] = -1.0f;
    }
}
```

### Тестовый тон

Если нет звука, попробуйте сгенерировать простой тон:

```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    static float phase = 0.0f;
    float* pOut = (float*)pOutput;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        float sample = 0.1f * sinf(phase);
        pOut[i * 2] = sample;
        pOut[i * 2 + 1] = sample;

        phase += 2.0f * 3.14159f * 440.0f / pDevice->sampleRate;
        if (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;
    }
}
```

---

## Частые ошибки

### Копирование структур

```c
// ОШИБКА: Копирование структуры
ma_sound sound1;
ma_sound_init_from_file(&engine, "test.wav", 0, NULL, NULL, &sound1);
ma_sound sound2 = sound1;  // ОШИБКА! Указатель внутри невалиден

// ПРАВИЛЬНО: Используйте ma_sound_init_copy
ma_sound sound2;
ma_sound_init_copy(&engine, &sound1, 0, NULL, &sound2);
```

### Инициализация в callback

```c
// ОШИБКА: Инициализация в callback
void data_callback(...) {
    ma_device_start(&device);  // DEADLOCK!
}

// ПРАВИЛЬНО: Сигнализация из callback
volatile bool shouldStart = false;
void data_callback(...) {
    shouldStart = true;
}
// В другом потоке:
if (shouldStart) ma_device_start(&device);
```

### Неправильный формат декодера

```c
// ОШИБКА: Формат не совпадает с устройством
ma_decoder_init_file("sound.wav", NULL, &decoder);  // Format может быть любым
config.playback.format = decoder.outputFormat;  // Может не поддерживаться!

// ПРАВИЛЬНО: Явно укажите формат декодера
ma_decoder_config decConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);
ma_decoder_init_file("sound.wav", &decConfig, &decoder);
