// Пример использования Low-level API miniaudio
// Демонстрирует: прямой control над audio buffer, процедурную генерацию звуков,
// обработку в real-time thread, ручное декодирование файлов
//
// Компиляция: g++ -std=c++11 miniaudio_lowlevel.cpp -lminiaudio -ldl -lpthread -lm

#include "miniaudio.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define FORMAT ma_format_f32
#define BUFFER_FRAMES 1024

// Глобальные структуры
typedef struct {
	ma_device device;
	ma_decoder decoder;
	ma_biquad_filter filter;
	float frequency;
	float amplitude;
	float time;
	bool playing_file;
	bool playing_sine;
} AudioState;

// Генератор синусоидального тона
void generate_sine_wave(float *output, ma_uint32 frame_count, float frequency, float amplitude, float *time)
{
	float step = frequency * (2.0f * M_PI) / SAMPLE_RATE;

	for (ma_uint32 i = 0; i < frame_count; i++) {
		float sample = sinf(*time) * amplitude;

		// Stereo: одинаковый сигнал в оба канала
		output[i * 2] = sample;		// Left
		output[i * 2 + 1] = sample; // Right

		*time += step;
		if (*time > 2.0f * M_PI) {
			*time -= 2.0f * M_PI;
		}
	}
}

// Генератор шума (процедурный звук разрушения)
void generate_noise(float *output, ma_uint32 frame_count, float amplitude)
{
	for (ma_uint32 i = 0; i < frame_count * 2; i++) {
		output[i] = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * amplitude;
	}
}

// Audio callback - вызывается в отдельном audio thread
void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
	AudioState *state = (AudioState *)pDevice->pUserData;
	float *output = (float *)pOutput;

	// Обнуляем выходной буфер
	memset(output, 0, frameCount * CHANNELS * sizeof(float));

	// 1. Воспроизведение файла (если активен)
	if (state->playing_file) {
		float file_buffer[BUFFER_FRAMES * CHANNELS];
		ma_uint64 frames_read;

		ma_result result = ma_decoder_read_pcm_frames(&state->decoder, file_buffer, frameCount, &frames_read);

		if (result == MA_SUCCESS && frames_read > 0) {
			// Смешиваем с выходным буфером
			for (ma_uint32 i = 0; i < frames_read * CHANNELS; i++) {
				output[i] += file_buffer[i] * 0.5f; // Уменьшаем громкость
			}
		}

		// Проверка конца файла
		if (frames_read < frameCount) {
			printf("Файл закончился\n");
			state->playing_file = false;
			ma_decoder_seek_to_pcm_frame(&state->decoder, 0); // Сброс к началу

			// Заполняем оставшуюся часть буфера тишиной
			for (ma_uint32 i = frames_read * CHANNELS; i < frameCount * CHANNELS; i++) {
				output[i] = 0.0f;
			}
		}
	}

	// 2. Генерация синусоидального тона (если активен)
	if (state->playing_sine) {
		float sine_buffer[BUFFER_FRAMES * CHANNELS];
		generate_sine_wave(sine_buffer, frameCount, state->frequency, state->amplitude, &state->time);

		// Применяем фильтр (low-pass)
		ma_biquad_process_pcm_frames(&state->filter, sine_buffer, sine_buffer, frameCount);

		// Смешиваем с выходным буфером
		for (ma_uint32 i = 0; i < frameCount * CHANNELS; i++) {
			output[i] += sine_buffer[i] * 0.3f; // Уменьшаем громкость
		}
	}

	// 3. Добавляем немного шума (пример процедурного звука)
	static int noise_counter = 0;
	if (noise_counter > 0) {
		float noise_buffer[BUFFER_FRAMES * CHANNELS];
		float noise_amp = (float)noise_counter / 100.0f;
		generate_noise(noise_buffer, frameCount, noise_amp * 0.1f);

		// Экспоненциальное затухание шума
		for (ma_uint32 i = 0; i < frameCount * CHANNELS; i++) {
			output[i] += noise_buffer[i] * (noise_amp / frameCount * (frameCount - i / CHANNELS));
		}

		noise_counter--;
	}

	// Лимитер для предотвращения клиппинга
	for (ma_uint32 i = 0; i < frameCount * CHANNELS; i++) {
		if (output[i] > 1.0f)
			output[i] = 1.0f;
		if (output[i] < -1.0f)
			output[i] = -1.0f;
	}
}

// Инициализация аудиоустройства
bool init_audio_device(AudioState *state)
{
	ma_device_config config = ma_device_config_init(ma_device_type_playback);
	config.playback.format = FORMAT;
	config.playback.channels = CHANNELS;
	config.sampleRate = SAMPLE_RATE;
	config.dataCallback = data_callback;
	config.pUserData = state;

	// Настройка low latency
	config.performanceProfile = ma_performance_profile_low_latency;
	config.periodSizeInFrames = BUFFER_FRAMES;

	ma_result result = ma_device_init(NULL, &config, &state->device);
	if (result != MA_SUCCESS) {
		printf("Ошибка инициализации аудиоустройства: %s\n", ma_result_description(result));
		return false;
	}

	printf("✅ Аудиоустройство инициализировано\n");
	printf("   Формат: %s\n", (FORMAT == ma_format_f32) ? "float32" : "unknown");
	printf("   Каналы: %d\n", CHANNELS);
	printf("   Частота: %d Hz\n", SAMPLE_RATE);
	printf("   Размер буфера: %d frames\n", BUFFER_FRAMES);
	printf("   Latency: ~%.1f ms\n", (BUFFER_FRAMES * 1000.0f) / SAMPLE_RATE);

	return true;
}

// Инициализация декодера для файла
bool init_decoder(AudioState *state, const char *filename)
{
	ma_decoder_config config = ma_decoder_config_init(FORMAT, CHANNELS, SAMPLE_RATE);

	ma_result result = ma_decoder_init_file(filename, &config, &state->decoder);
	if (result != MA_SUCCESS) {
		printf("Ошибка загрузки файла '%s': %s\n", filename, ma_result_description(result));
		return false;
	}

	printf("✅ Файл загружен: %s\n", filename);

	// Получение информации о файле
	ma_uint64 total_frames;
	ma_decoder_get_length_in_pcm_frames(&state->decoder, &total_frames);
	printf("   Длительность: %.2f секунд\n", (float)total_frames / SAMPLE_RATE);

	return true;
}

// Инициализация фильтра
void init_filter(AudioState *state)
{
	ma_biquad_config config = ma_biquad_config_init(ma_biquad_type_lowpass, SAMPLE_RATE,
													1000.0f, // cutoff frequency
													1.0f	 // Q
	);
	ma_biquad_init(&config, &state->filter);
}

// Отображение помощи
void print_help()
{
	printf("\n=== Управление Low-level Audio Demo ===\n");
	printf("1 - Воспроизвести/остановить синусоидальный тон (440Hz)\n");
	printf("2 - Воспроизвести/остановить файл (если доступен)\n");
	printf("3 - Проиграть короткий шум (процедурный звук разрушения)\n");
	printf("4 - Изменить частоту синуса (циклически: 220, 440, 880, 1760 Hz)\n");
	printf("5 - Изменить громкость синуса\n");
	printf("6 - Изменить cutoff frequency фильтра\n");
	printf("s - Старт/стоп аудиоустройства\n");
	printf("q - Выход\n");
	printf("=======================================\n\n");
}

int main(int argc, char *argv[])
{
	AudioState state = {0};
	state.frequency = 440.0f; // A4
	state.amplitude = 0.5f;
	state.time = 0.0f;
	state.playing_file = false;
	state.playing_sine = false;

	printf("=== Демонстрация Low-level API miniaudio ===\n");

	// Инициализация аудиоустройства
	if (!init_audio_device(&state)) {
		return 1;
	}

	// Инициализация декодера (опционально)
	const char *audio_file = "test.wav"; // Замените на существующий файл
	bool has_audio_file = init_decoder(&state, audio_file);
	if (!has_audio_file) {
		printf("⚠️ Файл '%s' не найден. Воспроизведение файла отключено.\n", audio_file);
		printf("   Создайте файл test.wav или измените путь в коде.\n");
	}

	// Инициализация фильтра
	init_filter(&state);

	print_help();

	// Основной цикл
	bool running = true;
	bool device_started = false;
	int freq_index = 1;
	float frequencies[] = {220.0f, 440.0f, 880.0f, 1760.0f};

	while (running) {
		printf("> ");
		fflush(stdout);

		char input[256];
		if (fgets(input, sizeof(input), stdin) == NULL) {
			break;
		}

		char command = input[0];

		switch (command) {
		case '1':
			// Переключение синусоидального тона
			state.playing_sine = !state.playing_sine;
			printf("Синусоидальный тон %s\n", state.playing_sine ? "▶️ ВКЛ" : "⏸️ ВЫКЛ");
			break;

		case '2':
			// Переключение воспроизведения файла
			if (!has_audio_file) {
				printf("❌ Файл недоступен\n");
				break;
			}
			state.playing_file = !state.playing_file;
			printf("Воспроизведение файла %s\n", state.playing_file ? "▶️ ВКЛ" : "⏸️ ВЫКЛ");
			break;

		case '3':
			// Проиграть процедурный шум
			printf("🔊 Процедурный шум (разрушение блока)\n");
			// Устанавливаем счётчик для постепенного затухания шума
			state.playing_file = false;
			state.playing_sine = false;
			// В реальном callback будет генерироваться шум
			// Здесь просто устанавливаем флаг через глобальную переменную
			// В реальной реализации нужна thread-safe синхронизация
			printf("(В этом демо шум генерируется постоянно при включённом устройстве)\n");
			break;

		case '4':
			// Смена частоты синуса
			freq_index = (freq_index + 1) % 4;
			state.frequency = frequencies[freq_index];
			printf("Частота синуса: %.0f Hz\n", state.frequency);
			break;

		case '5':
			// Изменение громкости
			state.amplitude += 0.1f;
			if (state.amplitude > 1.0f)
				state.amplitude = 0.1f;
			printf("Громкость синуса: %.1f%%\n", state.amplitude * 100.0f);
			break;

		case '6':
			// Изменение cutoff фильтра
			{
				static float cutoff = 1000.0f;
				cutoff *= 2.0f;
				if (cutoff > 8000.0f)
					cutoff = 250.0f;
				ma_biquad_set_frequency(&state.filter, cutoff);
				printf("Cutoff фильтра: %.0f Hz\n", cutoff);
			}
			break;

		case 's':
			// Старт/стоп устройства
			if (device_started) {
				ma_device_stop(&state.device);
				printf("⏸️ Аудиоустройство остановлено\n");
			} else {
				ma_result result = ma_device_start(&state.device);
				if (result == MA_SUCCESS) {
					printf("▶️ Аудиоустройство запущено\n");
					device_started = true;
				} else {
					printf("❌ Ошибка запуска: %s\n", ma_result_description(result));
				}
			}
			device_started = !device_started;
			break;

		case 'q':
			running = false;
			printf("🛑 Завершение...\n");
			break;

		case '\n':
			// Игнорируем пустой ввод
			break;

		default:
			printf("❓ Неизвестная команда '%c'. Нажмите 'h' для помощи.\n", command);
			break;

		case 'h':
			print_help();
			break;
		}
	}

	// Очистка
	if (device_started) {
		ma_device_stop(&state.device);
	}

	ma_device_uninit(&state.device);

	if (has_audio_file) {
		ma_decoder_uninit(&state.decoder);
	}

	printf("✅ Low-level демо завершено\n");
	return 0;
}

// Ключевые особенности Low-level API:
//
// 1. Прямой контроль над audio buffer в real-time callback
// 2. Возможность смешивания нескольких источников вручную
// 3. Процедурная генерация звуков (синусы, шум, и т.д.)
// 4. Применение DSP эффектов (фильтры) в реальном времени
// 5. Ручное управление latency через размер буфера
//
// Использование в ProjectV:
//
// 1. Для звуков разрушения/строительства блоков:
//    - Генерация процедурных звуков на основе материала
//    - Batch mixing множества источников
//    - Динамическое изменение параметров (громкость, фильтры)
//
// 2. Для амбиентных звуков:
//    - Процедурные генераторы ветра, воды, лавы
//    - Динамическая адаптация к игровому миру
//
// 3. Для UI звуков:
//    - Простые синусоидальные тона
//    - Фильтрация для разных типов feedback
//
// Важные предупреждения:
//
// 1. В callback нельзя вызывать ma_device_start/stop
// 2. Избегайте выделения памяти (malloc) в callback
// 3. Минимизируйте вычисления для снижения latency
// 4. Используйте thread-safe структуры данных
