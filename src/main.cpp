#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "AppUpdate.hpp"
#include "Camera.hpp"
#include "Renderer.hpp"
#include "Types.hpp"
#include "VulkanInit.hpp"

SDL_AppResult SDL_AppInit(void **appstate, int, char **)
{
	auto state = std::make_unique<AppState>();
	if (!InitVulkan(state.get())) {
		return SDL_APP_FAILURE;
	}

	if (!SDL_SetWindowRelativeMouseMode(state->platform.window, true)) {
		SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
	}

	*appstate = state.release();
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	AppState *state = static_cast<AppState *>(appstate);

	if (event->type == SDL_EVENT_QUIT ||
		event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
		(event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)) {
		return SDL_APP_SUCCESS;
	}

	if (event->type == SDL_EVENT_WINDOW_RESIZED || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
		if (state) {
			state->platform.windowResized = true;
		}
	}

	HandleCameraEvent(&state->camera, &state->input, event);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	auto *state = static_cast<AppState *>(appstate);
	if (!UpdateApp(
			&state->platform,
			&state->simulation,
			&state->camera,
			&state->input,
			&state->world,
			&state->render,
			&state->debug)) {
		SDL_Log("UpdateApp failed");
		return SDL_APP_FAILURE;
	}
	return DrawFrame(state);
}

void SDL_AppQuit(void *appstate, SDL_AppResult)
{
	auto *state = static_cast<AppState *>(appstate);
	ShutdownVulkan(state);
	delete state;
}
