#include "render/RendererInternal.hpp"

#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"

DebugOverlayPushConstants BuildBoxOverlayPushConstants(
	const FrameRenderData &frameRenderData,
	const DebugOverlayBox &box)
{
	DebugOverlayPushConstants pushConstants{};
	pushConstants.viewProjection = frameRenderData.graphicsPushConstants.viewProjection;
	pushConstants.overlayData0 = {
		static_cast<float>(box.min.x) - 0.01f,
		static_cast<float>(box.min.y) - 0.01f,
		static_cast<float>(box.min.z) - 0.01f,
		0.0f,
	};
	pushConstants.overlayData1 = {
		static_cast<float>(box.maxExclusive.x) + 0.01f,
		static_cast<float>(box.maxExclusive.y) + 0.01f,
		static_cast<float>(box.maxExclusive.z) + 0.01f,
		0.0f,
	};
	pushConstants.overlayColor = box.color;
	return pushConstants;
}

DebugOverlayPushConstants BuildCrosshairOverlayPushConstants(const SwapchainState &swapchain)
{
	DebugOverlayPushConstants pushConstants{};
	const float halfWidthNdc = 9.0f * 2.0f / static_cast<float>(swapchain.extent.width);
	const float halfHeightNdc = 9.0f * 2.0f / static_cast<float>(swapchain.extent.height);
	const float halfThicknessXNdc = 1.5f * 2.0f / static_cast<float>(swapchain.extent.width);
	const float halfThicknessYNdc = 1.5f * 2.0f / static_cast<float>(swapchain.extent.height);
	pushConstants.overlayData0 = {
		halfWidthNdc,
		halfHeightNdc,
		halfThicknessXNdc,
		1.0f,
	};
	pushConstants.overlayData1 = {halfThicknessYNdc, 0.0f, 0.0f, 0.0f};
	pushConstants.overlayColor = {1.0f, 1.0f, 1.0f, 1.0f};
	return pushConstants;
}
void RecordDebugOverlayCommands(
	RenderState &render,
	const SwapchainState &swapchain,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.debugOverlayMs);
	PV_PROFILE_ZONE_N("RecordDebugOverlayCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Debug Overlay");
	if (!frameRenderData.debugUiVisible || render.debugOverlayPipelineLayout == VK_NULL_HANDLE) {
		return;
	}

	if (render.debugOverlayPipeline != VK_NULL_HANDLE &&
		!frameRenderData.debugOverlayBoxes.empty()) {
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Debug Overlay");
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.debugOverlayPipeline);
		for (const DebugOverlayBox &box : frameRenderData.debugOverlayBoxes) {
			const DebugOverlayPushConstants pushConstants = BuildBoxOverlayPushConstants(frameRenderData, box);
			vkCmdPushConstants(
				cmd,
				render.debugOverlayPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(pushConstants),
				&pushConstants);
			vkCmdDraw(cmd, 24, 1, 0, 0);
		}
	}

	if (render.debugCrosshairPipeline != VK_NULL_HANDLE &&
		swapchain.extent.width > 0 &&
		swapchain.extent.height > 0) {
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Crosshair Overlay");
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.debugCrosshairPipeline);
		const DebugOverlayPushConstants pushConstants = BuildCrosshairOverlayPushConstants(swapchain);
		vkCmdPushConstants(
			cmd,
			render.debugOverlayPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(pushConstants),
			&pushConstants);
		vkCmdDraw(cmd, 18, 1, 0, 0);
	}
}
