// ------ include--блок ------
#define SDL_MAIN_USE_CALLBACKS 1 // Чтобы обходиться без main()
#include "SDL3/SDL.h"			 // SDL3 – библиотека для рисования окон и костяк для обработки рендера (с помощью него создаются базовые функции: event, iterate)
#include "SDL3/SDL_main.h"		 // Выше инклюдили базовые хэдеры, а тут – хэдеры для main цикла

#include "Renderer.hpp"
#include "Types.hpp"
#include "VulkanInit.hpp"
// --- Конец include-блока ---
// --- Коллбэки SDL3 ---

SDL_AppResult SDL_AppInit(void **appstate, int, char **) // Функция инициализации SDL и окна. Вызывается один раз.
{
	auto state = std::make_unique<AppState>(); // std::make_unique автоматически удалит объект, если произойдет ошибка и функция завершится досрочно (return SDL_APP_FAILURE). Это защита от утечек памяти

	if (!InitVulkan(state.get())) {
		return SDL_APP_FAILURE;
	}

	// Отпускаем владение из unique_ptr и передаем указатель внутрь SDL
	*appstate = state.release(); // Предположение: Нужно это окно засунуть в долгоживущую переменную, чтобы оно не закрылось при окончании данной функции.
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) // Функция обработки событий
{
	AppState *state = static_cast<AppState *>(appstate);

	if (event->type == SDL_EVENT_QUIT) // Обработка выхода через завершение процесса
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) // Обработка выхода через закрытие окна
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_KEY_DOWN &&
		event->key.key == SDLK_ESCAPE) // Обработка выхода через нажатие клавиши Escape
		return SDL_APP_SUCCESS;

	// Перехватываем изменение размеров
	if (event->type == SDL_EVENT_WINDOW_RESIZED || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
		if (state)
			state->windowResized = true;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) // Главный итерационный вызов, он крутится в цикле, пока не будет SDL_APP_SUCCESS
{
	return DrawFrame(static_cast<AppState *>(appstate)); // Главная функция рисовальщика кадров
}

void SDL_AppQuit(void *appstate, SDL_AppResult) // Функция завершения работы
{
	auto *state = static_cast<AppState *>(appstate);
	ShutdownVulkan(state);
	delete state;
}
