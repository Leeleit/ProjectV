#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "app/AppUpdate.hpp"
#include "app/Camera.hpp"
#include "app/FramePreparation.hpp"
#include "app/InputActions.hpp"
#include "platform/PlatformEvents.hpp"
#include "debug/Profiling.hpp"
#include "render/Renderer.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "voxel/VoxelInteraction.hpp"

SDL_AppResult SDL_AppInit(void **appstate, int, char **)
{
	PV_PROFILE_ZONE_N("SDL_AppInit");
	profiling::SetThreadName("Main Thread");
	profiling::ConfigureDefaultPlots();

	auto state = std::make_unique<AppState>();
	if (!InitVulkan(state.get())) {
		runtime::LogRuntimeFailure("App", "SDL_AppInit.InitVulkan", "InitVulkan returned false");
		return SDL_APP_FAILURE;
	}

	if (!SDL_SetWindowRelativeMouseMode(state->platform.window, state->input.relativeMouseModeEnabled)) {
		runtime::LogSdlFailure("SDL_AppInit.SDL_SetWindowRelativeMouseMode");
		state->input.relativeMouseModeEnabled = false;
	}

	*appstate = state.release();
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (!event) {
		return SDL_APP_CONTINUE;
	}

	if (event->type == SDL_EVENT_QUIT ||
		event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
		(event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)) {
		return SDL_APP_SUCCESS;
	}

	AppState *state = static_cast<AppState *>(appstate);
	if (!state) {
		return SDL_APP_CONTINUE;
	}

	if (ShouldRequestSwapchainRefreshForWindowEvent(event->type)) {
		state->platform.windowResized = true;
	}

	HandleInputActionEvent(&state->input, event);
	HandleCameraEvent(&state->camera, &state->input, event);
	HandleInteractionEvent(&state->input, event);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	PV_PROFILE_ZONE_N("SDL_AppIterate");
	auto *state = static_cast<AppState *>(appstate);
	if (!state) {
		runtime::LogRuntimeFailure("App", "SDL_AppIterate", "appstate is null");
		return SDL_APP_FAILURE;
	}
	SDL_AppResult result = SDL_APP_FAILURE;
	if (!UpdateApp(
			&state->platform,
			&state->simulation,
			&state->camera,
			&state->input,
			&state->interaction,
			&state->world,
			&state->render,
			&state->debug)) {
		runtime::LogRuntimeFailure("App", "SDL_AppIterate.UpdateApp", "UpdateApp returned false");
	} else if (!PrepareFrameRenderData(
				   &state->context,
				   &state->swapchain,
				   &state->camera,
				   &state->interaction,
				   &state->debug,
				   &state->world,
				   &state->render,
				   &state->frame)) {
		runtime::LogRuntimeFailure(
			"App",
			"SDL_AppIterate.PrepareFrameRenderData",
			"PrepareFrameRenderData returned false");
	} else {
		result = DrawFrame(
			&state->platform,
			&state->context,
			&state->swapchain,
			&state->render,
			&state->frame);
	}

	PV_PROFILE_FRAME_MARK();
	return result;
}

void SDL_AppQuit(void *appstate, SDL_AppResult)
{
	auto *state = static_cast<AppState *>(appstate);
	ShutdownVulkan(state);
	delete state;
}
