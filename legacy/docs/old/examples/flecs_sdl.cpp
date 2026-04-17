// Пример: flecs + SDL3 callbacks
// Документация: docs/flecs/quickstart.md

#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "flecs.h"

struct Position {
	float x, y;
};
struct Velocity {
	float x, y;
};

static flecs::world *g_ecs = nullptr;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (!SDL_Init(SDL_INIT_VIDEO))
		return SDL_APP_FAILURE;

	g_ecs = new flecs::world;
	g_ecs->system<Position, const Velocity>().each([](Position &p, const Velocity &v) {
		p.x += v.x;
		p.y += v.y;
	});
	g_ecs->entity().set<Position>({10, 20}).set<Velocity>({1, 2});

	SDL_Window *window = SDL_CreateWindow("flecs + SDL", 1280, 720, 0);
	if (!window) {
		delete g_ecs;
		SDL_Quit();
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
	if (g_ecs)
		g_ecs->progress(1.0f / 60.0f);
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	delete g_ecs;
	g_ecs = nullptr;
	SDL_Window *window = static_cast<SDL_Window *>(appstate);
	if (window)
		SDL_DestroyWindow(window);
	SDL_Quit();
}
