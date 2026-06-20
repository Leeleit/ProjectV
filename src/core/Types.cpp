import projectv.math;
import projectv.string_id;

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "volk.h"
#include "core/Types.hpp"
#include "asset/ModelManifestLoader.hpp"
#include "asset/ModelPass.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/SceneResources.hpp"
#include "render/TaaRenderTargets.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp"
#include "voxel/VoxelWorld.hpp"

void ShutdownVulkan(AppState *state)
{
	if (!state || state->shutdownDone) {
		return;
	}
	state->shutdownDone = true;

	if (state->context.device) {
		vkDeviceWaitIdle(state->context.device);
		profiling::CollectVulkanGpuHost(state->render.tracyGraphicsContext);
		profiling::DestroyVulkanGpuContext(state->render.tracyGraphicsContext);
		state->render.tracyGraphicsContext = nullptr;
		state->render.tracyGraphicsContextCalibrated = false;
		DestroyVoxelMeshingPipeline(&state->context, &state->render);
		DestroyGraphicsPipeline(&state->context, &state->render);
		DestroyDepthResources(&state->context, &state->render);
		DestroyShadowResources(&state->context, &state->render);
		DestroyScreenshotReadbackResources(&state->context, &state->render);
		DestroySceneResources(&state->context, &state->render);
		projectv::asset::UnloadAllModels(&state->context, &state->render);
		projectv::asset::DestroyModelPipeline(&state->context, &state->render);

		if (state->render.taaSceneColorTarget != nullptr || state->render.taaHistoryColorTarget != nullptr || state->render.taaLayerSceneColorTarget != nullptr || state->render.taaLayerHistoryColorTarget != nullptr) {
			projectv::taa::DestroyTaaRenderTargets(
				&state->context,
				*state->render.taaSceneColorTarget,
				*state->render.taaHistoryColorTarget,
				*state->render.taaLayerSceneColorTarget,
				*state->render.taaLayerHistoryColorTarget,
				state->render.taaLinearSampler);
			delete state->render.taaSceneColorTarget;
			state->render.taaSceneColorTarget = nullptr;
			delete state->render.taaHistoryColorTarget;
			state->render.taaHistoryColorTarget = nullptr;

			delete state->render.taaLayerSceneColorTarget;
			state->render.taaLayerSceneColorTarget = nullptr;
			delete state->render.taaLayerHistoryColorTarget;
			state->render.taaLayerHistoryColorTarget = nullptr;
		}
	}

	state->physics.reset();
	DestroyVoxelSceneWorld(state);

	if (state->context.device) {
		for (const VkSemaphore semaphore : state->swapchain.submitSemaphores) {
			if (semaphore != VK_NULL_HANDLE) {
				vkDestroySemaphore(state->context.device, semaphore, nullptr);
			}
		}
		for (const auto iv : state->swapchain.imageViews) {
			vkDestroyImageView(state->context.device, iv, nullptr);
		}
		if (state->swapchain.handle) {
			vkDestroySwapchainKHR(state->context.device, state->swapchain.handle, nullptr);
		}
	}

	if (state->context.device) {
		for (const auto sem : state->frame.imageAvailableSemaphores) {
			vkDestroySemaphore(state->context.device, sem, nullptr);
		}
		for (const auto sem : state->frame.renderFinishedSemaphores) {
			vkDestroySemaphore(state->context.device, sem, nullptr);
		}
		for (const auto fence : state->frame.inFlightFences) {
			vkDestroyFence(state->context.device, fence, nullptr);
		}

		if (state->context.commandPool) {
			vkDestroyCommandPool(state->context.device, state->context.commandPool, nullptr);
		}
	}

	if (state->context.allocator) {
		vmaDestroyAllocator(state->context.allocator);
		state->context.allocator = VK_NULL_HANDLE;
	}
	if (state->context.device) {
		vkDestroyDevice(state->context.device, nullptr);
		state->context.device = VK_NULL_HANDLE;
	}
	if (state->context.surface && state->context.instance) {
		vkDestroySurfaceKHR(state->context.instance, state->context.surface, nullptr);
		state->context.surface = VK_NULL_HANDLE;
	}

	if (state->context.debugMessenger && state->context.instance) {
		vkDestroyDebugUtilsMessengerEXT(state->context.instance, state->context.debugMessenger, nullptr);
		state->context.debugMessenger = VK_NULL_HANDLE;
	}

	if (state->platform.window) {
		SDL_DestroyWindow(state->platform.window);
		state->platform.window = nullptr;
	}
	if (state->context.instance) {
		vkDestroyInstance(state->context.instance, nullptr);
		state->context.instance = VK_NULL_HANDLE;
	}
}
