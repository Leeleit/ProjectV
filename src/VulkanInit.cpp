#include "Camera.hpp"
#include "SceneResources.hpp"
#include "VoxelWorld.hpp"
#include "VulkanBootstrap.hpp"
#include "VulkanGraphicsPipeline.hpp"
#include "VulkanSwapchain.hpp"

bool InitVulkan(AppState *state)
{
	if (!InitializeVulkanBase(&state->platform, &state->context, &state->frame)) {
		return false;
	}

	if (!RecreateSwapchain(&state->platform, &state->context, &state->swapchain, &state->render)) {
		return false;
	}

	if (!CreateVoxelLabWorld(state)) {
		SDL_Log("CreateVoxelLabWorld failed");
		return false;
	}

	InitializeCamera(&state->camera, &state->simulation, &state->input);

	if (!CreateSceneResources(&state->context, &state->world, &state->render)) {
		SDL_Log("CreateSceneResources failed");
		return false;
	}

	if (!CreateGraphicsPipeline(&state->context, &state->swapchain, &state->render)) {
		SDL_Log("CreateGraphicsPipeline failed");
		return false;
	}

	return true;
}
