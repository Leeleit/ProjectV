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
#include "render/RayTracedShadows.hpp"
#include "render/RtxGiProbes.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanAsyncCompute.hpp"
#include "render/vulkan/VulkanFluidCaPipeline.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanMeshShaderPipeline.hpp"
#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp"
#include "render/vulkan/VulkanWorldGenPipeline.hpp"
#include "render/SkyAtmosphere.hpp"
#include "render/VolumetricFog.hpp"
#include "render/Cloudscape.hpp"
#include "voxel/VoxelWorld.hpp"

void ShutdownVulkan(AppState *state)
{
	if (!state || state->shutdownDone()) {
		return;
	}
	state->shutdownDone() = true;

	if (state->context().device) {
		vkDeviceWaitIdle(state->context().device);
		profiling::CollectVulkanGpuHost(state->render().tracyGraphicsContext);
		profiling::DestroyVulkanGpuContext(state->render().tracyGraphicsContext);
		state->render().tracyGraphicsContext = nullptr;
		state->render().tracyGraphicsContextCalibrated = false;
		DestroyVoxelMeshingPipeline(&state->context(), &state->render());
		projectv::render::DestroyMeshShaderPipelines(&state->context(), &state->render());
		projectv::render::DestroyFluidCaPipelines(&state->context(), &state->render());
		projectv::render::DestroyWorldGenPipelines(&state->context(), &state->render());
		projectv::render::DestroySkyAtmospherePipelines(&state->context(), &state->render());
		projectv::render::DestroyVolumetricFogResources(&state->context(), &state->render());
		projectv::render::DestroySkyLutResources(&state->context(), &state->render());
		projectv::render::DestroyRayTracedShadowResources(&state->context(), &state->render());
		delete state->render().rayTracedShadows;
		state->render().rayTracedShadows = nullptr;
		projectv::render::DestroyRtxShadowMaskFallbackOnly(&state->context(), &state->render());
		projectv::render::DestroyRtxGiProbeResources(&state->context(), &state->render());
		delete state->render().rtxGiProbes;
		state->render().rtxGiProbes = nullptr;
		// EVIL: DestroyVoxelizePipelines also destroys vctClipmapSampler (8x V1) but
		// it's only called when VCT gate is ON. The fallback sampler created in
		// CreateVctClipmapFallbackSamplerOnly must be destroyed unconditionally
		// to satisfy VUID-vkDestroyDevice-device-05137.
		if (state->render().vctClipmapSampler != VK_NULL_HANDLE) {
			vkDestroySampler(state->context().device, state->render().vctClipmapSampler, nullptr);
			state->render().vctClipmapSampler = VK_NULL_HANDLE;
		}
		DestroyGraphicsPipeline(&state->context(), &state->render());
		DestroyDepthResources(&state->context(), &state->render());
		DestroyShadowResources(&state->context(), &state->render());
		DestroyScreenshotReadbackResources(&state->context(), &state->render());
		if (state->render().sceneColorImageView != VK_NULL_HANDLE) {
			vkDestroyImageView(state->context().device, state->render().sceneColorImageView, nullptr);
			state->render().sceneColorImageView = VK_NULL_HANDLE;
		}
		if (state->render().sceneColorImage != VK_NULL_HANDLE) {
			vmaDestroyImage(state->context().allocator, state->render().sceneColorImage, static_cast<VmaAllocation>(state->render().sceneColorAllocation));
			state->render().sceneColorImage = VK_NULL_HANDLE;
			state->render().sceneColorAllocation = nullptr;
		}
		DestroySceneResources(&state->context(), &state->render());
		projectv::asset::UnloadAllModels(&state->context(), &state->render());
		projectv::asset::DestroyModelPipeline(&state->context(), &state->render());
	}

	state->physics().reset();
	DestroyVoxelSceneWorld(state);

	if (state->context().device) {
		for (const VkSemaphore semaphore : state->swapchain().submitSemaphores) {
			if (semaphore != VK_NULL_HANDLE) {
				vkDestroySemaphore(state->context().device, semaphore, nullptr);
			}
		}
		for (const auto iv : state->swapchain().imageViews) {
			vkDestroyImageView(state->context().device, iv, nullptr);
		}
		if (state->swapchain().handle) {
			vkDestroySwapchainKHR(state->context().device, state->swapchain().handle, nullptr);
		}
	}

	if (state->context().device) {
		for (const auto sem : state->frame().imageAvailableSemaphores) {
			vkDestroySemaphore(state->context().device, sem, nullptr);
		}
		for (const auto sem : state->frame().renderFinishedSemaphores) {
			vkDestroySemaphore(state->context().device, sem, nullptr);
		}
		for (const auto fence : state->frame().inFlightFences) {
			vkDestroyFence(state->context().device, fence, nullptr);
		}

		if (state->context().commandPool) {
			vkDestroyCommandPool(state->context().device, state->context().commandPool, nullptr);
		}

		projectv::render::DestroyAsyncComputeResources(&state->context());

		if (state->context().renderTimelineSemaphore) {
			vkDestroySemaphore(state->context().device, state->context().renderTimelineSemaphore, nullptr);
			state->context().renderTimelineSemaphore = VK_NULL_HANDLE;
		}

		if (state->context().hzbBuildTimelineSemaphore) {
			vkDestroySemaphore(state->context().device, state->context().hzbBuildTimelineSemaphore, nullptr);
			state->context().hzbBuildTimelineSemaphore = VK_NULL_HANDLE;
		}
	}

	if (state->context().allocator) {
		vmaDestroyAllocator(state->context().allocator);
		state->context().allocator = VK_NULL_HANDLE;
	}
	if (state->context().device) {
		vkDestroyDevice(state->context().device, nullptr);
		state->context().device = VK_NULL_HANDLE;
	}
	if (state->context().surface && state->context().instance) {
		vkDestroySurfaceKHR(state->context().instance, state->context().surface, nullptr);
		state->context().surface = VK_NULL_HANDLE;
	}

	if (state->context().debugMessenger && state->context().instance) {
		vkDestroyDebugUtilsMessengerEXT(state->context().instance, state->context().debugMessenger, nullptr);
		state->context().debugMessenger = VK_NULL_HANDLE;
	}

	if (state->platform().window) {
		SDL_DestroyWindow(state->platform().window);
		state->platform().window = nullptr;
	}
	if (state->context().instance) {
		vkDestroyInstance(state->context().instance, nullptr);
		state->context().instance = VK_NULL_HANDLE;
	}
}
