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
	const VkCommandBuffer cmd,
	const VkImage image,
	const VkImageAspectFlags aspectMask,
	const VkImageLayout oldLayout,
	const VkImageLayout newLayout,
	const VkPipelineStageFlags2 srcStageMask,
	const VkAccessFlags2 srcAccessMask,
	const VkPipelineStageFlags2 dstStageMask,
	const VkAccessFlags2 dstAccessMask,
	const uint32_t layerCount = 1u);

bool ShouldCaptureScreenshot(const RenderState &render);

void RecordShadowCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd);

void RecordDebugOverlayCommands(
	RenderState &render,
	const SwapchainState &swapchain,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd);

void RecordDebugHudCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd);

void RecordVoxelMeshingCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd);

} // namespace projectv::render