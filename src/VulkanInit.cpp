#include "VulkanBootstrap.hpp"
#include "VulkanComputePipeline.hpp"
#include "VulkanSwapchain.hpp"

bool InitVulkan(AppState *state)
{
	if (!InitializeVulkanBase(state)) {
		return false;
	}

	if (!RecreateSwapchain(state)) {
		return false;
	}

	if (!CreateComputePipeline(state)) {
		SDL_Log("CreateComputePipeline failed");
		return false;
	}

	return true;
}
