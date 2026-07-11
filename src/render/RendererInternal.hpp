#pragma once

#include "volk.h"

#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"
#include "render/Renderer.hpp"
#include "render/vulkan/VulkanResult.hpp"

#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/ScreenshotCapture.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "render/vulkan/VulkanMeshShaderPipeline.hpp"
#include "render/vulkan/VulkanFluidCaPipeline.hpp"
#include "render/vulkan/VulkanWorldGenPipeline.hpp"
#include "render/vulkan/VulkanAsyncCompute.hpp"
#include "render/SkyAtmosphere.hpp"
#include "render/Cloudscape.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/RtxGiProbes.hpp"
#include "voxel/VoxelMaterials.hpp"

#include "fmt/format.h"

#include <filesystem>
#include <SDL3/SDL.h>

struct SwapchainState;
struct FrameRenderData;
struct DebugOverlayBox;

// RAII timer for render pass profiling
class ScopedPassTimer {
  public:
	explicit ScopedPassTimer(float &outMs)
		: outMs_(outMs), start_(SDL_GetPerformanceCounter()) {}

	~ScopedPassTimer()
	{
		const Uint64 end = SDL_GetPerformanceCounter();
		const Uint64 freq = SDL_GetPerformanceFrequency();
		const double seconds = static_cast<double>(end - start_) / static_cast<double>(freq);
		outMs_ = static_cast<float>(seconds * 1000.0);
	}

	ScopedPassTimer(const ScopedPassTimer &) = delete;
	ScopedPassTimer &operator=(const ScopedPassTimer &) = delete;

  private:
	float &outMs_;
	Uint64 start_;
};

// Internal declarations for functions that cross TU boundaries
void RecordShadowCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	VkCommandBuffer cmd);

void RecordDebugOverlayCommands(
	RenderState &render,
	const SwapchainState &swapchain,
	const FrameRenderData &frameRenderData,
	VkCommandBuffer cmd);

void RecordDebugHudCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	VkCommandBuffer cmd);

void TransitionImage(
	VkCommandBuffer cmd,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	VkPipelineStageFlags2 srcStageMask,
	VkAccessFlags2 srcAccessMask,
	VkPipelineStageFlags2 dstStageMask,
	VkAccessFlags2 dstAccessMask,
	uint32_t layerCount = 1u);

void RecordGraphicsCommands(
	RenderState &render,
	const SwapchainState &swapchain,
	const FrameRenderData &frameRenderData,
	const VulkanContextState &context,
	VkCommandBuffer cmd,
	uint32_t imageIndex);

bool ShouldCaptureScreenshot(const RenderState &render);
void RecordSwapchainScreenshotCopy(
	const SwapchainState &swapchain,
	RenderState &render,
	VkCommandBuffer cmd,
	uint32_t imageIndex);
bool SaveRequestedScreenshot(
	VulkanContextState &context,
	RenderState &render,
	VkFence inFlightFence,
	VkExtent2D captureExtent,
	VkFormat captureFormat);
