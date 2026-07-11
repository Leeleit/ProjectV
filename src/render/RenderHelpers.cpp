#include "render/RenderHelpers.hpp"

#include "debug/DebugOverlays.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "volk.h"

void projectv::render::TransitionImage(
	const VkCommandBuffer cmd,
	const VkImage image,
	const VkImageAspectFlags aspectMask,
	const VkImageLayout oldLayout,
	const VkImageLayout newLayout,
	const VkPipelineStageFlags2 srcStageMask,
	const VkAccessFlags2 srcAccessMask,
	const VkPipelineStageFlags2 dstStageMask,
	const VkAccessFlags2 dstAccessMask,
	const uint32_t layerCount)
{
	TransitionImage(
		cmd,
		image,
		aspectMask,
		oldLayout,
		newLayout,
		srcStageMask,
		srcAccessMask,
		dstStageMask,
		dstAccessMask,
		layerCount);
}

bool projectv::render::ShouldCaptureScreenshot(const RenderState &render)
{
	return render.screenshotCaptureRequested &&
		   render.screenshotCaptureSupported &&
		   render.screenshotReadbackBuffer != VK_NULL_HANDLE &&
		   render.screenshotReadbackAllocation != VK_NULL_HANDLE &&
		   render.screenshotReadbackMappedData != nullptr;
}

namespace {

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

} // namespace

namespace projectv::render {

void RecordShadowCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	(void)render;
	(void)frameRenderData;
	(void)cmd;
	// CSM removed per TODO.md §5.2.D (session 20x). RTX shadows are the
	// canonical sun shadow path; the shadow pass no longer renders.
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

void RecordDebugHudCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.debugHudMs);
	PV_PROFILE_ZONE_N("RecordDebugHudCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Debug HUD");
	if (render.debugHudPipeline == VK_NULL_HANDLE ||
		frameRenderData.debugHudVertexBuffer == VK_NULL_HANDLE ||
		frameRenderData.debugHudVertexCount == 0) {
		return;
	}

	PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Debug HUD");
	constexpr VkDeviceSize vertexBufferOffset = 0;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.debugHudPipeline);
	vkCmdBindVertexBuffers(cmd, 0, 1, &frameRenderData.debugHudVertexBuffer, &vertexBufferOffset);
	vkCmdDraw(cmd, frameRenderData.debugHudVertexCount, 1, 0, 0);
}

void RecordVoxelMeshingCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.meshingMs);

	render.renderPassTimings.dirtyChunkRebuiltCount = frameRenderData.dirtyChunkCount;
	PV_PROFILE_ZONE_N("RecordVoxelMeshingCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Voxel Meshing");
	if (render.voxelMeshingPipeline == VK_NULL_HANDLE ||
		render.voxelMeshingPipelineLayout == VK_NULL_HANDLE ||
		frameRenderData.voxelMeshingDescriptorSet == VK_NULL_HANDLE ||
		frameRenderData.packedFaceBuffer == VK_NULL_HANDLE ||
		frameRenderData.opaqueIndirectBuffer == VK_NULL_HANDLE ||
		frameRenderData.transparentIndirectBuffer == VK_NULL_HANDLE ||
		frameRenderData.dirtyChunkCount == 0) {
		return;
	}

	PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Voxel Meshing");

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, render.voxelMeshingPipeline);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.voxelMeshingPipelineLayout,
		0,
		1,
		&frameRenderData.voxelMeshingDescriptorSet,
		0,
		nullptr);
	vkCmdPushConstants(
		cmd,
		render.voxelMeshingPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,
		sizeof(frameRenderData.voxelMeshingPushConstants),
		&frameRenderData.voxelMeshingPushConstants);
	vkCmdDispatch(cmd, frameRenderData.dirtyChunkCount, 1, 1);

	VkBufferMemoryBarrier2 bufferBarriers[3]{};
	bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	bufferBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	bufferBarriers[0].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	bufferBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	bufferBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	bufferBarriers[0].buffer = frameRenderData.packedFaceBuffer;
	bufferBarriers[0].offset = 0;
	bufferBarriers[0].size = VK_WHOLE_SIZE;

	bufferBarriers[1] = bufferBarriers[0];
	bufferBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	bufferBarriers[1].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[1].buffer = frameRenderData.opaqueIndirectBuffer;

	bufferBarriers[2] = bufferBarriers[0];
	bufferBarriers[2].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	bufferBarriers[2].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[2].buffer = frameRenderData.transparentIndirectBuffer;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.bufferMemoryBarrierCount = 3;
	depInfo.pBufferMemoryBarriers = bufferBarriers;
	vkCmdPipelineBarrier2(cmd, &depInfo);
}

} // namespace projectv::render