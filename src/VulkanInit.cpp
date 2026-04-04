#include "Camera.hpp"
#include "SceneResources.hpp"
#include "VoxelWorld.hpp"
#include "VulkanBootstrap.hpp"
#include "VulkanGraphicsPipeline.hpp"
#include "VulkanSwapchain.hpp"

bool InitVulkan(AppState *state)
{
	if (!InitializeVulkanBase(state)) {
		return false;
	}

	if (!RecreateSwapchain(state)) {
		return false;
	}

	if (!CreateVoxelLabWorld(state)) {
		SDL_Log("CreateVoxelLabWorld failed");
		return false;
	}

	InitializeCamera(state);

	if (!CreateSceneResources(state)) {
		SDL_Log("CreateSceneResources failed");
		return false;
	}

	if (!CreateGraphicsPipeline(state)) {
		SDL_Log("CreateGraphicsPipeline failed");
		return false;
	}

	return true;
}
