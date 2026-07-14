#pragma once

#include "core/Types.hpp"

#include "render/vulkan/VulkanMeshShaderPipeline.hpp"

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

// Selection/placement wireframe boxes are drawn inside the main voxel color pass (see
// RecordGraphicsCommands) so they pick up the same MSAA/SMAA/progressive/SSAA as voxel geometry.
DebugOverlayPushConstants BuildBoxOverlayPushConstants(
	const FrameRenderData &frameRenderData,
	const DebugOverlayBox &box);

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
	VulkanContextState &context,
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
