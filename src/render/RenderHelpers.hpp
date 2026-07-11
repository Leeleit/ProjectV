#pragma once

#include "core/Types.hpp"

#include <SDL3/SDL.h>

namespace projectv::render {

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

bool ShouldCaptureScreenshot(const RenderState &render);

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

void RecordVoxelMeshingCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	VkCommandBuffer cmd);

} // namespace projectv::render