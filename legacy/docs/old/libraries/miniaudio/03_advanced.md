# miniaudio: Хардкорные оптимизации и DOD

> **Для понимания:** Аудио в играх — это невидимый фронт. Пока игрок смотрит на воксели, звуковой движок каждую
> миллисекунду гоняет данные между CPU, памятью и звуковой картой. Оптимизация аудио — это как настройка спортивного
> мотора: каждая доля секунды на счету, и никаких лишних движений.

## Zero-Allocation в Audio Thread

> **Для понимания:** Audio callback работает в реальном времени. Представьте, что это пожарная команда: когда звучит
> тревога (callback), у вас есть строго ограниченное время до следующего вызова. Любая аллокация памяти — как попытка
> вызвать мастерскую во время пожара. Никакой надежды успеть.

### Pre-allocated Audio Buffers

```cpp
// ХРАНИМ ВСЁ В STACK или статике
alignas(64) struct AudioFrame {
    float left;
    float right;
};

class AudioProcessor {
    // Статический буфер - никакой аллокации в рантайме
    static constexpr size_t BUFFER_SIZE = 2048;
    alignas(64) std::array<AudioFrame, BUFFER_SIZE> m_buffer;

    std::span<AudioFrame> process(std::span<const AudioFrame> input) {
        // Обработка без аллокаций
        for (size_t i = 0; i < input.size(); ++i) {
            // DSP манипуляции
            m_buffer[i].left = input[i].left * m_gain;
            m_buffer[i].right = input[i].right * m_gain;
        }
        return m_buffer.first(input.size());
    }
};
```

### Object Pool для звуков (lock-free версия)

```cpp
// Пул объектов - аллокация только при старте, lock-free доступ
class SoundPool {
    struct alignas(64) Node {
        ma_sound sound;
        std::atomic<bool> in_use{false};
        std::string name;

        // CAS операции для атомарного управления состоянием
        bool try_acquire(const char* new_name) {
            bool expected = false;
            if (in_use.compare_exchange_strong(expected, true,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
                name = new_name;
                return true;
            }
            return false;
        }

        void release() {
            in_use.store(false, std::memory_order_release);
        }
    };

    alignas(64) std::vector<Node> m_pool;

public:
    SoundPool(size_t size) {
        m_pool.resize(size);
    }

    // Аллокация при старте, не в рантайме (lock-free)
    ma_sound* allocate(const char* name) {
        // Пробуем найти свободный узел с помощью CAS операций
        for (auto& node : m_pool) {
            if (node.try_acquire(name)) {
                return &node.sound;
            }
        }
        return nullptr;  // Pool exhausted
    }

    void deallocate(ma_sound* sound) {
        // Находим узел по указателю и освобождаем его
        for (auto& node : m_pool) {
            if (&node.sound == sound) {
                node.release();
                return;
            }
        }
    }

    // Альтернативная версия с атомарным индексом для лучшей производительности
    class AtomicSoundPool {
        struct alignas(64) AtomicNode {
            ma_sound sound;
            std::atomic<bool> in_use{false};
            char name[64];  // Фиксированный буфер вместо std::string

            bool try_acquire(const char* new_name) {
                bool expected = false;
                if (in_use.compare_exchange_strong(expected, true)) {
                    std::strncpy(name, new_name, sizeof(name) - 1);
                    name[sizeof(name) - 1] = '\0';
                    return true;
                }
                return false;
            }
        };

        alignas(64) std::vector<AtomicNode> m_pool;
        alignas(64) std::atomic<size_t> m_next_index{0};

    public:
        AtomicSoundPool(size_t size) : m_pool(size) {}

        ma_sound* allocate(const char* name) {
            // Round-robin поиск с атомарным индексом
            const size_t start_idx = m_next_index.fetch_add(1, std::memory_order_relaxed) % m_pool.size();

            for (size_t i = 0; i < m_pool.size(); ++i) {
                size_t idx = (start_idx + i) % m_pool.size();
                if (m_pool[idx].try_acquire(name)) {
                    return &m_pool[idx].sound;
                }
            }
            return nullptr;
        }
    };
};
```

## Cache-Line Alignment

> **Для понимания:** CPU читает память «строками» по 64 байта. Если ваши данные пересекают границы этих строк — CPU
> приходится читать два раза. Для аудио, где данные гоняются туда-сюда сотни раз в секунду, это критично.

### SoA для Audio Processing

> **Для понимания:** Традиционный подход (AoS): `struct Frame { float l, r; }` → массив таких структур. SoA: два массива
`left[]` и `right[]`. SoA лучше, потому что CPU может читать каналы параллельно.

```cpp
// AoS (Array of Structures) - плохо для SIMD
struct AudioFrameAoS {
    float left;
    float right;
};
std::vector<AudioFrameAoS> frames_aos;

// SoA (Structure of Arrays) - хорошо для SIMD и кэша
struct alignas(64) AudioFrameSoA {
    // Каждый канал - отдельный массив (кэш-линия = 64 байт = 16 float)
    alignas(64) std::span<float> left;
    alignas(64) std::span<float> right;

    // Или фиксированный размер
    static constexpr size_t CHANNELS = 2;
    static constexpr size_t CAPACITY = 2048;

    alignas(64) float left_data[CAPACITY];
    alignas(64) float right_data[CAPACITY];

    AudioFrameSoA()
        : left(left_data, CAPACITY)
        , right(right_data, CAPACITY)
    {}

    // SIMD-friendly обработка
    void process_gain(float gain) {
        // SSE/AVX может обработать 4/8 float за раз
        for (size_t i = 0; i < CAPACITY; i += 4) {
            __m128 l = _mm_load_ps(&left_data[i]);
            __m128 r = _mm_load_ps(&right_data[i]);
            __m128 g = _mm_set1_ps(gain);
            l = _mm_mul_ps(l, g);
            r = _mm_mul_ps(r, g);
            _mm_store_ps(&left_data[i], l);
            _mm_store_ps(&right_data[i], r);
        }
    }
};
```

### DSP State Alignment

```cpp
// Состояние DSP - выравниваем для SIMD
struct alignas(64) DSPState {
    // Filter state
    float b0 = 0, b1 = 0, b2 = 0;  // Feedforward
    float a1 = 0, a2 = 0;           // Feedback

    // Должно быть кратно 16 байтам для AVX
    float delay[8] = {0};           // Delay line

    // Громкость и pitch
    float volume = 1.0f;
    float pitch = 1.0f;

    // Padding до 64 байт
    float _padding[3] = {0};
};
static_assert(sizeof(DSPState) == 64, "DSPState must be 64 bytes");

// Массив DSP-состояний для нескольких каналов/источников
alignas(64) std::array<DSPState, MAX_VOICES> dsp_states;
```

## Thread-Local Audio Processing

> **Для понимание:** Аудио callback однопоточен по своей природе. Но подготовка данных (декодирование, микширование)
> может и должна быть многопоточной. Thread-local хранилище избавляет от блокировок.

### Thread-Local Decoder Pool

```cpp
// Thread-local пул декодеров - каждый поток имеет свой набор
class ThreadLocalDecoderPool {
    // TLS - у каждого потока своя копия
    static thread_local std::vector<ma_decoder> s_decoders;
    static thread_local size_t s_current_index;

public:
    static ma_decoder* acquire() {
        if (s_current_index >= s_decoders.size()) {
            // Расширяем пул при необходимости
            s_decoders.emplace_back();
        }
        return &s_decoders[s_current_index++];
    }

    static void release() {
        // Не уменьшаем - пул растёт только
        // Для сброса между кадрами:
        // s_current_index = 0;
    }

    static void reset() {
        s_current_index = 0;
    }
};

thread_local std::vector<ma_decoder> ThreadLocalDecoderPool::s_decoders;
thread_local size_t ThreadLocalDecoderPool::s_current_index = 0;
```

### Job System для Audio

> **Для понимания:** Наш Job System (который мы ещё напишем) работает на основных потоках. Audio thread — отдельный.
> Задача: подготовить данные в Jobs до того, как audio callback их запросит.

```cpp
// Готовим микс заранее, в main thread
class AudioMixJob {
    // Входные данные - SoA формат
    struct VoiceInput {
        std::span<float> left;
        std::span<float> right;
        float volume;
        float pan;
    };

    // Выходной буфер
    alignas(64) std::vector<float> mix_buffer;

public:
    void add_voice(const VoiceInput& voice) {
        m_voices.push_back(voice);
    }

    // Job function - запускается в Job System
    void execute() {
        // Очистка выхода
        std::fill(mix_buffer.begin(), mix_buffer.end(), 0.0f);

        // Микширование всех voice
        for (const auto& voice : m_voices) {
            for (size_t i = 0; i < voice.left.size(); ++i) {
                mix_buffer[i] += voice.left[i] * voice.volume * (1.0f - voice.pan);
                mix_buffer[i] += voice.right[i] * voice.volume * (1.0f + voice.pan);
            }
        }
    }

    std::span<float> get_output() { return mix_buffer; }
};
```

## Lock-Free Audio Queue

> **Для понимание:** Когда main thread хочет передать команду в audio thread (например, "play sound"), нужна очередь без
> блокировок. Иначе — race condition или deadlock.

### Single-Producer Single-Consumer Ring Buffer

```cpp
template<typename T, size_t N>
class alignas(64) SPSCQueue {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");

    alignas(64) T m_data[N];
    alignas(64) std::atomic<size_t> m_head{0};  // Пишет producer
    alignas(64) std::atomic<size_t> m_tail{0};  // Читает consumer

public:
    // Producer (main thread) - неблокирующая запись
    bool try_push(const T& value) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next_head = (head + 1) & (N - 1);

        if (next_head == m_tail.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }

        m_data[head] = value;
        m_head.store(next_head, std::memory_order_release);
        return true;
    }

    // Consumer (audio thread) - неблокирующее чтение
    bool try_pop(T& value) {
        size_t tail = m_tail.load(std::memory_order_relaxed);

        if (tail == m_head.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        value = m_data[tail];
        m_tail.store((tail + 1) & (N - 1), std::memory_order_release);
        return true;
    }
};

// Команды для audio thread
struct AudioCommand {
    enum class Type : uint8_t {
        Play,
        Stop,
        SetVolume,
        SetPosition
    };

    Type type;
    uint32_t sound_id;
    union {
        float volume;
        float position[3];
    } params;
};
```

### Интеграция с audio callback

```cpp
SPSCQueue<AudioCommand, 256> g_audio_commands;

void audio_callback(ma_device* /*device*/, void* pOutput,
                    const void* /*pInput*/, ma_uint32 frameCount) {
    float* output = (float*)pOutput;

    // Обрабатываем команды
    AudioCommand cmd;
    while (g_audio_commands.try_pop(cmd)) {
        switch (cmd.type) {
            case AudioCommand::Type::Stop:
                // Остановить звук
                break;
            case AudioCommand::Type::SetVolume:
                // Установить громкость
                break;
            // ...
        }
    }

    // Заполняем выход
    std::fill_n(output, frameCount * 2, 0.0f);
}
```

## SIMD Audio Processing

> **Для понимания:** Современные CPU могут обрабатывать несколько значений за такт. Для аудио это критично — чем больше
> каналов, тем больше вычислений. SIMD (Single Instruction Multiple Data) — это как конвейер на заводе: одна команда
> обрабатывает сразу несколько деталей.

### Stereo Gain с AVX

```cpp
#include <immintrin.h>

void apply_gain_stereo_avx(float* left, float* right,
                           float gain, size_t sample_count) {
    __m256 g = _mm256_set1_ps(gain);

    size_t i = 0;
    for (; i + 8 <= sample_count; i += 8) {
        // Загрузить 8 float
        __m256 l = _mm256_loadu_ps(&left[i]);
        __m256 r = _mm256_loadu_ps(&right[i]);

        // Умножить
        l = _mm256_mul_ps(l, g);
        r = _mm256_mul_ps(r, g);

        // Сохранить
        _mm256_storeu_ps(&left[i], l);
        _mm256_storeu_ps(&right[i], r);
    }

    // Остаток
    for (; i < sample_count; ++i) {
        left[i] *= gain;
        right[i] *= gain;
    }
}
```

### mixing с SSE

```cpp
// Смешивание двух стерео буферов
void mix_buffers_sse(float* __restrict__ dst,
                      const float* __restrict__ src,
                      float volume, size_t frames) {
    __m256 v = _mm256_set1_ps(volume);
    size_t i = 0;

    for (; i + 8 <= frames; i += 8) {
        // dst[0-7] = dst[0-7] + src[0-7] * volume
        __m256 d = _mm256_loadu_ps(&dst[i]);
        __m256 s = _mm256_loadu_ps(&src[i]);
        d = _mm256_fmadd_ps(s, v, d);  // FMA: d + s * v
        _mm256_storeu_ps(&dst[i], d);
    }

    // Обработка остатка
    for (; i < frames; ++i) {
        dst[i] += src[i] * volume;
    }
}
```

## Memory Mapping для больших аудиофайлов

> **Для понимания:** Если у вас 10-минутный саундтрек, загружать его весь в память — расточительство. Memory mapping (
> mmap) позволяет обращаться к файлу как к памяти, а OS сама подгружает нужные страницы.

```cpp
#include <sys/mman.h>
#include <unistd.h>

class MappedAudioFile {
    int m_fd = -1;
    void* m_mapping = nullptr;
    size_t m_size = 0;

public:
    std::expected<void, AudioError> open(const char* path) {
        m_fd = open(path, O_RDONLY);
        if (m_fd < 0) {
            return std::unexpected(AudioError::ResourceLoadFailed);
        }

        m_size = lseek(m_fd, 0, SEEK_END);

        m_mapping = mmap(nullptr, m_size, PROT_READ, MAP_PRIVATE, m_fd, 0);
        if (m_mapping == MAP_FAILED) {
            close(m_fd);
            return std::unexpected(AudioError::ResourceLoadFailed);
        }

        // Подсказка OS о паттерне доступа
        madvise(m_mapping, m_size, MADAV_SEQUENTIAL);

        return {};
    }

    ~MappedAudioFile() {
        if (m_mapping) munmap(m_mapping, m_size);
        if (m_fd >= 0) close(m_fd);
    }

    // Передаём указатель в miniaudio
    ma_result init_decoder(ma_decoder* decoder) {
        return ma_decoder_init_memory(
            m_mapping,
            m_size,
            nullptr,  // config
            decoder
        );
    }

    void* data() const { return m_mapping; }
    size_t size() const { return m_size; }
};
```

## Spatial Audio Optimization

> **Для понимания:** Расчёт spatial audio (panner) для каждого звука — дорого. Оптимизация: обновлять только те звуки,
> которые изменили позицию, и кэшировать результаты.

### Dirty Flag для Spatial

```cpp
class SpatialAudioSystem {
    struct SpatialSource {
        ma_sound sound;
        float last_position[3];
        bool dirty = true;
        bool spatial_enabled = true;
    };

    std::vector<SpatialSource> m_sources;

public:
    void update_positions() {
        for (auto& src : m_sources) {
            if (!src.dirty) continue;

            // Обновляем только если изменилась позиция
            ma_sound_set_position(&src.sound,
                src.last_position[0],
                src.last_position[1],
                src.last_position[2]);

            src.dirty = false;
        }
    }

    void set_position(size_t index, float x, float y, float z) {
        auto& src = m_sources[index];

        // Проверяем, изменилась ли позиция
        if (src.last_position[0] != x ||
            src.last_position[1] != y ||
            src.last_position[2] != z) {

            src.last_position[0] = x;
            src.last_position[1] = y;
            src.last_position[2] = z;
            src.dirty = true;
        }
    }
};
```

### Frustum Culling для Spatial Audio

```cpp
// Не проигрываем звуки, которые далеко или за пределами слышимости
struct AudioCullingSystem {
    struct Listener {
        float position[3];
        float forward[3];
        float hearing_distance;
        float fov_angle;  // radians
    };

    bool is_audible(const Listener& listener, const float sound_pos[3]) {
        // Distance culling
        float dx = sound_pos[0] - listener.position[0];
        float dy = sound_pos[1] - listener.position[1];
        float dz = sound_pos[2] - listener.position[2];
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (dist > listener.hearing_distance) {
            return false;
        }

        // Direction culling (внутри ли конуса)
        // Dot product для угла
        // Вычисляем вектор от слушателя к источнику звука
        float to_sound[3] = {
            sound_pos[0] - listener.position[0],
            sound_pos[1] - listener.position[1],
            sound_pos[2] - listener.position[2]
        };

        // Нормализуем вектор
        float dist_to_sound = std::sqrt(to_sound[0]*to_sound[0] + to_sound[1]*to_sound[1] + to_sound[2]*to_sound[2]);
        if (dist_to_sound > 0.0f) {
            to_sound[0] /= dist_to_sound;
            to_sound[1] /= dist_to_sound;
            to_sound[2] /= dist_to_sound;
        }

        // Вычисляем dot product между forward вектором слушателя и направлением к звуку
        float dot = listener.forward[0]*to_sound[0] + listener.forward[1]*to_sound[1] + listener.forward[2]*to_sound[2];

        // Проверяем, находится ли звук в пределах FOV
        float cos_half_fov = std::cos(listener.fov_angle / 2.0f);
        if (dot < cos_half_fov) {
            return false;  // Звук вне поля слышимости
        }

        return true;
    }
};
```

## Profiling и Debug

> **Для понимания:** Tracy (наш профайлер) интегрируется с audio. Главное — не мерять audio callback напрямую (это
> исказит результаты), а мерять подготовку данных.

### Tracy для Audio Jobs

```cpp
#include <tracy/Tracy.hpp>

void audio_prepare_job() {
    ZoneScopedN("AudioPrepare");

    // Декодирование
    {
        ZoneScopedN("Decode");
        decode_audio_data();
    }

    // Микширование
    {
        ZoneScopedN("Mix");
        mix_audio_buffers();
    }

    // Обновление spatial
    {
        ZoneScopedN("SpatialUpdate");
        update_spatial_positions();
    }
}
```

### Latency Measurement

```cpp
// Замер задержки audio thread
class AudioLatencyTracker {
    std::chrono::high_resolution_clock::time_point m_last_callback;
    std::vector<double> m_latencies;

public:
    void on_callback() {
        auto now = std::chrono::high_resolution_clock::now();

        if (m_last_callback.time_since_epoch().count() > 0) {
            auto delta = std::chrono::duration<double, std::milli>(
                now - m_last_callback
            ).count();
            m_latencies.push_back(delta);

            if (m_latencies.size() > 1000) {
                // Логируем среднее/макс/мин
                double avg = 0, max = 0, min = 1e9;
                for (auto l : m_latencies) {
                    avg += l;
                    max = std::max(max, l);
                    min = std::min(min, l);
                }
                avg /= m_latencies.size();

                std::println("Audio latency: avg={:.2f}ms, min={:.2f}ms, max={:.2f}ms",
                    avg, min, max);
                m_latencies.clear();
            }
        }

        m_last_callback = now;
    }
};
```

## Резюме хардкора

1. **Zero-allocation** в audio thread — никаких new/malloc в callback
2. **SoA vs AoS** — структура массивов для SIMD и кэша
3. **Cache-line alignment** — 64-байтное выравнивание для AVX
4. **Thread-local** — TLS для декодеров, job system для подготовки
5. **SPSC queue** — lock-free коммуникация main ↔ audio thread
6. **SIMD** — AVX/SSE для gain, mixing, фильтров
7. **Memory mapping** — для больших файлов без загрузки в RAM
8. **Spatial culling** — не обновляем то, что не слышно
9. **Tracy integration** — профилирование подготовки, не callback
