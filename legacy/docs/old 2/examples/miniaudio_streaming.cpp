// Пример streaming аудио с использованием resource manager miniaudio
// Демонстрирует: асинхронную загрузку, streaming больших файлов,
// управление памятью, кэширование ресурсов
//
// Компиляция: g++ -std=c++11 miniaudio_streaming.cpp -lminiaudio -ldl -lpthread -lm

#include "miniaudio.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define MAX_STREAMS 8
#define STREAM_BUFFER_SIZE 65536 // 64KB буфер для streaming

// Структура для streaming звука
typedef struct {
	ma_sound sound;
	std::string filename;
	bool is_streaming;
	bool is_playing;
	float volume;
} StreamInfo;

// Глобальные объекты
ma_engine g_engine;
ma_resource_manager g_resource_mgr;
StreamInfo g_streams[MAX_STREAMS];
int g_active_streams = 0;

// Callback для событий resource manager
void resource_manager_notification_callback(const ma_resource_manager_notification *pNotification)
{
	switch (pNotification->type) {
	case ma_resource_manager_notification_type_started:
		printf("🚀 Начало загрузки: %s\n", pNotification->data.started.pFilePath);
		break;

	case ma_resource_manager_notification_type_processed:
		printf("✅ Загружено: %s (%.1f%%)\n", pNotification->data.processed.pFilePath,
			   pNotification->data.processed.progress * 100.0f);
		break;

	case ma_resource_manager_notification_type_decoded:
		printf("🎵 Декодировано: %s\n", pNotification->data.decoded.pFilePath);
		break;

	case ma_resource_manager_notification_type_failed:
		printf("❌ Ошибка загрузки: %s\n", pNotification->data.failed.pFilePath);
		break;

	default:
		break;
	}
}

// Инициализация resource manager
bool init_resource_manager()
{
	ma_resource_manager_config config = ma_resource_manager_config_init();

	// Настройка для streaming
	config.decodedFormat = ma_format_f32;
	config.decodedChannels = 2;
	config.decodedSampleRate = 48000;

	// Включение notification callback
	config.notificationCallback = resource_manager_notification_callback;

	// Настройка потоков для декодирования
	config.decodingThreadCount = 2; // 2 потока для декодирования

	// Настройка кэширования
	config.maxCacheSizeInBytes = 100 * 1024 * 1024; // 100MB кэш

	ma_result result = ma_resource_manager_init(&config, &g_resource_mgr);
	if (result != MA_SUCCESS) {
		printf("Ошибка инициализации resource manager: %s\n", ma_result_description(result));
		return false;
	}

	printf("✅ Resource manager инициализирован\n");
	printf("   Потоков декодирования: %d\n", config.decodingThreadCount);
	printf("   Размер кэша: %.1f MB\n", config.maxCacheSizeInBytes / (1024.0f * 1024.0f));

	return true;
}

// Инициализация audio engine с resource manager
bool init_audio_engine()
{
	ma_engine_config config = ma_engine_config_init();
	config.pResourceManager = &g_resource_mgr;

	ma_result result = ma_engine_init(&config, &g_engine);
	if (result != MA_SUCCESS) {
		printf("Ошибка инициализации audio engine: %s\n", ma_result_description(result));
		return false;
	}

	printf("✅ Audio engine инициализирован с resource manager\n");
	return true;
}

// Загрузка звука для streaming
int load_streaming_sound(const char *filename, float volume = 1.0f, bool stream = true)
{
	if (g_active_streams >= MAX_STREAMS) {
		printf("❌ Достигнут лимит streaming звуков (%d)\n", MAX_STREAMS);
		return -1;
	}

	int index = g_active_streams;
	StreamInfo *stream = &g_streams[index];

	// Настройка флагов для streaming
	ma_uint32 flags = 0;
	if (stream) {
		flags |= MA_SOUND_FLAG_STREAM;
		printf("📥 Загрузка для streaming: %s\n", filename);
	} else {
		printf("📥 Загрузка в память: %s\n", filename);
	}

	// Инициализация звука через resource manager
	ma_result result = ma_sound_init_from_file(&g_engine, filename, flags,
											   NULL, // Группа (опционально)
											   NULL, // Fade (опционально)
											   &stream->sound);

	if (result != MA_SUCCESS) {
		printf("❌ Ошибка загрузки '%s': %s\n", filename, ma_result_description(result));
		return -1;
	}

	stream->filename = filename;
	stream->is_streaming = stream;
	stream->is_playing = false;
	stream->volume = volume;
	ma_sound_set_volume(&stream->sound, volume);

	g_active_streams++;

	printf("✅ Звук загружен [%d]: %s (%s)\n", index, filename, stream ? "streaming" : "in-memory");

	// Получение информации о звуке
	ma_uint64 length_in_frames;
	ma_sound_get_length_in_pcm_frames(&stream->sound, &length_in_frames);
	float duration = (float)length_in_frames / 48000.0f;
	printf("   Длительность: %.2f секунд\n", duration);

	return index;
}

// Предзагрузка звука в кэш
bool preload_sound(const char *filename)
{
	printf("⏳ Предзагрузка в кэш: %s\n", filename);

	ma_result result = ma_resource_manager_register_file(
		&g_resource_mgr, filename,
		MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC,
		NULL // job (опционально)
	);

	if (result != MA_SUCCESS) {
		printf("❌ Ошибка предзагрузки: %s\n", ma_result_description(result));
		return false;
	}

	return true;
}

// Воспроизведение streaming звука
void play_stream(int index, bool loop = false)
{
	if (index < 0 || index >= g_active_streams) {
		printf("❌ Неверный индекс: %d\n", index);
		return;
	}

	StreamInfo *stream = &g_streams[index];

	if (stream->is_playing) {
		ma_sound_stop(&stream->sound);
		ma_sound_seek_to_pcm_frame(&stream->sound, 0);
		printf("⏹️ Остановлен stream [%d]\n", index);
	}

	ma_sound_set_looping(&stream->sound, loop);
	ma_sound_start(&stream->sound);
	stream->is_playing = true;

	printf("▶️ Воспроизведение stream [%d]: %s\n", index, stream->filename.c_str());
	if (stream->is_streaming) {
		printf("   (Режим: streaming с диска)\n");
	} else {
		printf("   (Режим: из памяти)\n");
	}
}

// Пауза/возобновление stream
void toggle_stream_pause(int index)
{
	if (index < 0 || index >= g_active_streams)
		return;

	StreamInfo *stream = &g_streams[index];
	if (!stream->is_playing)
		return;

	if (ma_sound_is_playing(&stream->sound)) {
		ma_sound_stop(&stream->sound);
		printf("⏸️ Пауза stream [%d]\n", index);
	} else {
		ma_sound_start(&stream->sound);
		printf("▶️ Возобновление stream [%d]\n", index);
	}
}

// Изменение громкости stream
void set_stream_volume(int index, float volume)
{
	if (index < 0 || index >= g_active_streams)
		return;

	StreamInfo *stream = &g_streams[index];
	stream->volume = volume;
	ma_sound_set_volume(&stream->sound, volume);
	printf("🔊 Громкость stream [%d]: %.0f%%\n", index, volume * 100.0f);
}

// Получение информации о stream
void print_stream_info(int index)
{
	if (index < 0 || index >= g_active_streams) {
		printf("❌ Неверный индекс: %d\n", index);
		return;
	}

	StreamInfo *stream = &g_streams[index];

	printf("\n=== Информация о stream [%d] ===\n", index);
	printf("Файл: %s\n", stream->filename.c_str());
	printf("Режим: %s\n", stream->is_streaming ? "streaming" : "in-memory");
	printf("Статус: %s\n", stream->is_playing ? "playing" : "stopped");
	printf("Громкость: %.0f%%\n", stream->volume * 100.0f);

	// Текущая позиция
	ma_uint64 cursor;
	ma_sound_get_cursor_in_pcm_frames(&stream->sound, &cursor);

	ma_uint64 length;
	ma_sound_get_length_in_pcm_frames(&stream->sound, &length);

	if (length > 0) {
		float position_sec = (float)cursor / 48000.0f;
		float length_sec = (float)length / 48000.0f;
		float percent = (float)cursor / length * 100.0f;

		printf("Позиция: %.1f / %.1f сек (%.1f%%)\n", position_sec, length_sec, percent);
	}

	// Информация о кэше (только для streaming)
	if (stream->is_streaming) {
		ma_bool32 is_fully_cached;
		ma_sound_is_fully_cached(&stream->sound, &is_fully_cached);
		printf("Кэширован: %s\n", is_fully_cached ? "полностью" : "частично");
	}

	printf("===============================\n");
}

// Информация о resource manager
void print_resource_manager_info()
{
	ma_resource_manager_stats stats;
	ma_resource_manager_get_stats(&g_resource_manager, &stats);

	printf("\n=== Resource Manager Статистика ===\n");
	printf("Всего файлов: %u\n", stats.totalFileCount);
	printf("Кэшировано: %u\n", stats.cachedFileCount);
	printf("Активные загрузки: %u\n", stats.activeJobCount);
	printf("Использовано памяти: %.2f MB\n", (float)stats.totalDataSize / (1024.0f * 1024.0f));
	printf("Кэш памяти: %.2f MB\n", (float)stats.cachedDataSize / (1024.0f * 1024.0f));
	printf("==================================\n");
}

// Отображение помощи
void print_help()
{
	printf("\n=== Управление Streaming Audio Demo ===\n");
	printf("load <file> [stream] - Загрузить файл (1=stream, 0=memory)\n");
	printf("preload <file>       - Предзагрузить в кэш\n");
	printf("play <index> [loop]  - Воспроизвести stream\n");
	printf("pause <index>        - Пауза/возобновление\n");
	printf("stop <index>         - Остановить stream\n");
	printf("volume <index> <0-1> - Установить громкость\n");
	printf("seek <index> <sec>   - Перейти к позиции\n");
	printf("info <index>         - Информация о stream\n");
	printf("list                 - Список загруженных streams\n");
	printf("stats                - Статистика resource manager\n");
	printf("help                 - Показать это сообщение\n");
	printf("quit                 - Выход\n");
	printf("=======================================\n\n");
}

int main(int argc, char *argv[])
{
	printf("=== Демонстрация Streaming Audio с miniaudio ===\n");

	// Инициализация resource manager
	if (!init_resource_manager()) {
		return 1;
	}

	// Инициализация audio engine
	if (!init_audio_engine()) {
		ma_resource_manager_uninit(&g_resource_mgr);
		return 1;
	}

	print_help();

	// Основной цикл
	bool running = true;
	char input[256];

	while (running) {
		printf("stream> ");
		fflush(stdout);

		if (fgets(input, sizeof(input), stdin) == NULL) {
			break;
		}

		// Удаление символа новой строки
		input[strcspn(input, "\n")] = 0;

		// Парсинг команды
		char cmd[32];
		char arg1[256];
		char arg2[32];
		int args = sscanf(input, "%31s %255s %31s", cmd, arg1, arg2);

		if (args < 1)
			continue;

		if (strcmp(cmd, "load") == 0 && args >= 2) {
			bool stream = true;
			if (args >= 3) {
				stream = (atoi(arg2) != 0);
			}

			int index = load_streaming_sound(arg1, 1.0f, stream);
			if (index >= 0) {
				printf("✅ Загружен как stream [%d]\n", index);
			}

		} else if (strcmp(cmd, "preload") == 0 && args >= 2) {
			preload_sound(arg1);

		} else if (strcmp(cmd, "play") == 0 && args >= 2) {
			int index = atoi(arg1);
			bool loop = (args >= 3) ? (atoi(arg2) != 0) : false;
			play_stream(index, loop);

		} else if (strcmp(cmd, "pause") == 0 && args >= 2) {
			int index = atoi(arg1);
			toggle_stream_pause(index);

		} else if (strcmp(cmd, "stop") == 0 && args >= 2) {
			int index = atoi(arg1);
			if (index >= 0 && index < g_active_streams) {
				ma_sound_stop(&g_streams[index].sound);
				g_streams[index].is_playing = false;
				printf("⏹️ Остановлен stream [%d]\n", index);
			}

		} else if (strcmp(cmd, "volume") == 0 && args >= 3) {
			int index = atoi(arg1);
			float volume = atof(arg2);
			set_stream_volume(index, volume);

		} else if (strcmp(cmd, "seek") == 0 && args >= 3) {
			int index = atoi(arg1);
			float seconds = atof(arg2);
			if (index >= 0 && index < g_active_streams) {
				ma_uint64 frame = (ma_uint64)(seconds * 48000.0f);
				ma_sound_seek_to_pcm_frame(&g_streams[index].sound, frame);
				printf("↪️ Переход к %.1f сек\n", seconds);
			}

		} else if (strcmp(cmd, "info") == 0 && args >= 2) {
			int index = atoi(arg1);
			print_stream_info(index);

		} else if (strcmp(cmd, "list") == 0) {
			printf("\n=== Загруженные Streams ===\n");
			for (int i = 0; i < g_active_streams; i++) {
				StreamInfo *s = &g_streams[i];
				printf("[%d] %s - %s (%s)\n", i, s->filename.c_str(), s->is_streaming ? "streaming" : "memory",
					   s->is_playing ? "playing" : "stopped");
			}
			printf("===========================\n");

		} else if (strcmp(cmd, "stats") == 0) {
			print_resource_manager_info();

		} else if (strcmp(cmd, "help") == 0) {
			print_help();

		} else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
			running = false;
			printf("🛑 Завершение...\n");

		} else {
			printf("❓ Неизвестная команда: %s\n", cmd);
			printf("   Введите 'help' для списка команд\n");
		}
	}

	// Очистка
	printf("\n🧹 Очистка ресурсов...\n");

	// Остановка всех streams
	for (int i = 0; i < g_active_streams; i++) {
		ma_sound_uninit(&g_streams[i].sound);
	}

	// Остановка engine
	ma_engine_uninit(&g_engine);

	// Очистка resource manager
	ma_resource_manager_uninit(&g_resource_mgr);

	printf("✅ Streaming демо завершено\n");
	return 0;
}

// Применение в ProjectV:
//
// 1. Для фоновой музыки:
//    - Использовать streaming для больших файлов
//    - Предзагрузка следующего трека во время воспроизведения
//    - Плавные переходы между треками
//
// 2. Для амбиентных звуков:
//    - Streaming длинных лупов (ветер, вода)
//    - Динамическое переключение между разными ambients
//    - Кэширование часто используемых звуков
//
// 3. Для озвучки диалогов:
//    - Streaming по мере необходимости
//    - Приоритетная загрузка текущего диалога
//    - Освобождение памяти после проигрывания
//
// Преимущества resource manager:
//
// 1. Автоматическое кэширование
// 2. Асинхронная загрузка в фоне
// 3. Оптимальное использование памяти
// 4. Поддержка streaming с диска
// 5. Уведомления о прогрессе загрузки
//
// Оптимизации для воксельной игры:
//
// 1. Предзагружать часто используемые звуки (разрушение блоков)
// 2. Использовать streaming для музыки и длинных ambients
// 3. Настроить размер кэша в зависимости от доступной памяти
// 4. Мониторить использование памяти через статистику
