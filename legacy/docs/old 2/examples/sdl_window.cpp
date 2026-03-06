// Пример: минимальное окно SDL3 с callbacks
// Документация: docs/sdl/quickstart.md

#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_vulkan.h"

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	SDL_Window *window = SDL_CreateWindow("ProjectV", 1280, 720, SDL_WINDOW_VULKAN);
	if (!window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	*appstate = window;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (event->type == SDL_EVENT_QUIT)
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
		return SDL_APP_SUCCESS;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	SDL_Window *window = static_cast<SDL_Window *>(appstate);
	if (window)
		SDL_DestroyWindow(window);
}
