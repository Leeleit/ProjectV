// Пример интеграции miniaudio с SDL3
// Демонстрирует: паузу аудио при потере фокуса окна, управление звуками через High-level API
//
// Компиляция (Linux/macOS): g++ -std=c++11 miniaudio_sdl.cpp -lSDL3 -lminiaudio -ldl -lpthread -lm
// Windows: линковать с SDL3.lib и miniaudio.lib

#include "miniaudio.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>

#define MAX_SIMULTANEOUS_SOUNDS 16

// Глобальные объекты
ma_engine g_engine;
ma_sound g_sounds[MAX_SIMULTANEOUS_SOUNDS];
int g_next_sound_index = 0;
bool g_audio_paused = false;

// Callback для окончания звука
void sound_finished_callback(void *pUserData, ma_sound *pSound)
{
	printf("Звук завершён\n");
}

// Инициализация аудиосистемы
bool init_audio()
{
	ma_result result = ma_engine_init(NULL, &g_engine);
	if (result != MA_SUCCESS) {
		printf("Ошибка инициализации audio engine: %s\n", ma_result_description(result));
		return false;
	}

	printf("✅ Audio engine инициализирован\n");
	printf("   Частота дискретизации: %d Hz\n", (int)ma_engine_get_sample_rate(&g_engine));
	printf("   Устройство: %s\n", ma_engine_get_device_name(&g_engine, ma_engine_get_device(&g_engine)));

	return true;
}

// Загрузка звуков
bool load_sounds()
{
	const char *sound_files[] = {"assets/sounds/break.wav", "assets/sounds/place.wav", "assets/sounds/ui_click.wav",
								 NULL};

	for (int i = 0; sound_files[i] != NULL && i < MAX_SIMULTANEOUS_SOUNDS; i++) {
		ma_result result = ma_sound_init_from_file(&g_engine, sound_files[i], MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
												   NULL, &g_sounds[i]);

		if (result == MA_SUCCESS) {
			printf("✅ Звук загружен: %s\n", sound_files[i]);
			ma_sound_set_end_callback(&g_sounds[i], sound_finished_callback, NULL);
		} else {
			printf("⚠️ Не удалось загрузить: %s (%s)\n", sound_files[i], ma_result_description(result));
			// Используем silence как fallback
			ma_sound_init_with_data_source(&g_engine, NULL, 0, NULL, &g_sounds[i]);
		}
	}

	return true;
}

// Воспроизведение звука по индексу
void play_sound(int index, float volume = 1.0f)
{
	if (index < 0 || index >= MAX_SIMULTANEOUS_SOUNDS)
		return;

	// Если звук уже играет, перезапускаем
	if (ma_sound_is_playing(&g_sounds[index])) {
		ma_sound_stop(&g_sounds[index]);
		ma_sound_seek_to_pcm_frame(&g_sounds[index], 0);
	}

	ma_sound_set_volume(&g_sounds[index], volume);
	ma_sound_start(&g_sounds[index]);

	printf("▶️ Воспроизведение звука %d\n", index);
}

// Пауза/возобновление всей аудиосистемы
void toggle_audio_pause()
{
	if (g_audio_paused) {
		ma_engine_start(&g_engine);
		printf("▶️ Аудио возобновлено\n");
	} else {
		ma_engine_stop(&g_engine);
		printf("⏸️ Аудио приостановлено\n");
	}
	g_audio_paused = !g_audio_paused;
}

// Обработка событий SDL для аудио
void handle_audio_events(SDL_Event *event)
{
	switch (event->type) {
	case SDL_EVENT_WINDOW_FOCUS_LOST:
	case SDL_EVENT_WINDOW_MINIMIZED:
		if (!g_audio_paused) {
			ma_engine_stop(&g_engine);
			g_audio_paused = true;
			printf("⏸️ Аудио приостановлено (потеря фокуса)\n");
		}
		break;

	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_RESTORED:
		if (g_audio_paused) {
			ma_engine_start(&g_engine);
			g_audio_paused = false;
			printf("▶️ Аудио возобновлено (получен фокус)\n");
		}
		break;

	case SDL_EVENT_KEY_DOWN:
		switch (event->key.key) {
		case SDLK_SPACE:
			// Проиграть случайный звук
			play_sound(g_next_sound_index % 3, 0.5f);
			g_next_sound_index++;
			break;

		case SDLK_p:
			// Пауза/возобновление
			toggle_audio_pause();
			break;

		case SDLK_m:
			// Mute/unmute
			static bool muted = false;
			muted = !muted;
			ma_engine_set_volume(&g_engine, muted ? 0.0f : 1.0f);
			printf("%s мастер-громкость\n", muted ? "🔇 Mute" : "🔊 Unmute");
			break;

		case SDLK_UP:
			// Увеличить громкость
			{
				float volume = ma_engine_get_volume(&g_engine);
				volume = (volume + 0.1f > 1.0f) ? 1.0f : volume + 0.1f;
				ma_engine_set_volume(&g_engine, volume);
				printf("🔊 Громкость: %.1f%%\n", volume * 100.0f);
			}
			break;

		case SDLK_DOWN:
			// Уменьшить громкость
			{
				float volume = ma_engine_get_volume(&g_engine);
				volume = (volume - 0.1f < 0.0f) ? 0.0f : volume - 0.1f;
				ma_engine_set_volume(&g_engine, volume);
				printf("🔉 Громкость: %.1f%%\n", volume * 100.0f);
			}
			break;
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		// Проиграть звук при клике мыши
		if (event->button.button == SDL_BUTTON_LEFT) {
			play_sound(2, 0.3f); // UI звук
		}
		break;
	}
}

// Основная функция
int main(int argc, char *argv[])
{
	// Инициализация SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		printf("Ошибка инициализации SDL: %s\n", SDL_GetError());
		return 1;
	}

	// Создание окна
	SDL_Window *window = SDL_CreateWindow("miniaudio + SDL3 Demo", 800, 600, 0);
	if (!window) {
		printf("Ошибка создания окна: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	printf("✅ SDL3 инициализирован\n");

	// Инициализация аудио
	if (!init_audio()) {
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// Загрузка звуков (в реальном проекте должны существовать файлы)
	load_sounds();

	// Инструкции
	printf("\n=== Управление ===\n");
	printf("SPACE - проиграть звук разрушения\n");
	printf("P - пауза/возобновление всей аудиосистемы\n");
	printf("M - mute/unmute мастер-громкость\n");
	printf("UP/DOWN - регулировка громкости\n");
	printf("ЛКМ - проиграть UI звук\n");
	printf("Закройте окно для выхода\n");
	printf("==================\n\n");

	// Основной цикл
	bool running = true;
	SDL_Event event;

	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
			handle_audio_events(&event);
		}

		// Обновление позиции слушателя (камеры)
		// В реальной игре здесь обновляем позицию на основе камеры
		static float angle = 0.0f;
		angle += 0.01f;
		float listener_x = sinf(angle) * 5.0f;
		float listener_z = cosf(angle) * 5.0f;

		// В этом демо не используем spatial audio, но показываем возможность
		ma_engine_listener_set_position(&g_engine, 0, listener_x, 0.0f, listener_z);

		// Небольшая задержка для снижения нагрузки на CPU
		SDL_Delay(16);
	}

	// Очистка
	printf("\n🛑 Завершение...\n");

	// Остановка всех звуков
	for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
		ma_sound_uninit(&g_sounds[i]);
	}

	// Остановка audio engine
	ma_engine_uninit(&g_engine);

	// Закрытие SDL
	SDL_DestroyWindow(window);
	SDL_Quit();

	printf("✅ Программа завершена корректно\n");
	return 0;
}

// Заметки для интеграции в ProjectV:
//
// 1. В реальном проекте проверяйте существование звуковых файлов:
//    if (!file_exists("assets/sounds/break.wav")) {
//        printf("Предупреждение: файл звука не найден\n");
//        // Можно сгенерировать тестовый звук или использовать silence
//    }
//
// 2. Для воксельной игры добавьте spatial audio:
//    ma_sound_set_position(sound, x, y, z);
//    ma_sound_set_spatialization_enabled(sound, true);
//
// 3. Для оптимизации используйте пул звуков вместо отдельных ma_sound
//
// 4. Интегрируйте с Tracy для профилирования:
//    #include "tracy/Tracy.hpp"
//    ZoneScopedN("AudioUpdate");
//
// 5. В главном игровом цикле ProjectV вызывайте handle_audio_events
//    вместе с остальной обработкой событий SDL
