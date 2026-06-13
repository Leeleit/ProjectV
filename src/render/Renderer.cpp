#include "render/Renderer.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/ScreenshotCapture.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "render/vulkan/VulkanResult.hpp"
#include "voxel/VoxelMaterials.hpp"

#include "fmt/format.h"

#include <cstring>
#include <filesystem>

namespace {
// **Per-pass CPU timing helper, 2026-06-12.** RAII wrapper
// that converts `SDL_GetPerformanceCounter` ticks at
// destruction into a millisecond float and writes it to the
// referenced output slot. Used by each `Record*Commands`
// function below to populate
// `RenderState::renderPassTimings::*Ms`. RAII matters for
// the early-return paths in `RecordShadowCommands` /
// `RecordVoxelMeshingCommands` / `RecordDebugOverlayCommands`
// / `RecordDebugHudCommands` — without RAII each early
// return would need its own `writeTiming()` call site, and
// one missed call would silently leave the previous frame's
// stale number on the HUD.
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

// 4x4 matrix inverse via Gauss-Jordan elimination with partial pivoting.
// Column-major layout, same as the rest of the project (the
// `MultiplyMatrices` helper in `Camera.cpp` uses the same convention).
// Only used by the TAA resolve pass to build
// `inverseCurrentViewProjection`; called at most once per frame so the
// cost is irrelevant. The check that `det != 0` would be a real concern
// for a singular matrix, but the projection matrix produced by
// `BuildGraphicsPushConstants` is non-singular for any sensible near/far
// pair, and the resolve pass is downstream of the voxel pass, so a
// singular input would already have failed before reaching here.
std::array<float, 16> InvertColumnMajorMat4(const std::array<float, 16> &matrix)
{
	std::array<float, 16> inverse{};
	std::array<float, 16> augmented = matrix;
	for (int column = 0; column < 4; ++column) {
		inverse[column * 4 + 0] = column == 0 ? 1.0f : 0.0f;
		inverse[column * 4 + 1] = column == 1 ? 1.0f : 0.0f;
		inverse[column * 4 + 2] = column == 2 ? 1.0f : 0.0f;
		inverse[column * 4 + 3] = column == 3 ? 1.0f : 0.0f;
	}
	for (int pivot = 0; pivot < 4; ++pivot) {
		int bestRow = pivot;
		float bestAbs = std::fabs(augmented[pivot * 4 + pivot]);
		for (int row = pivot + 1; row < 4; ++row) {
			const float candidateAbs = std::fabs(augmented[row * 4 + pivot]);
			if (candidateAbs > bestAbs) {
				bestAbs = candidateAbs;
				bestRow = row;
			}
		}
		if (bestRow != pivot) {
			for (int swapCol = 0; swapCol < 4; ++swapCol) {
				std::swap(augmented[pivot * 4 + swapCol], augmented[bestRow * 4 + swapCol]);
				std::swap(inverse[pivot * 4 + swapCol], inverse[bestRow * 4 + swapCol]);
			}
		}
		const float pivotValue = augmented[pivot * 4 + pivot];
		if (pivotValue == 0.0f) {
			// Singular matrix; the resolve pass would produce
			// undefined output, but the TAA-on path is currently a
			// no-op (gate off) so this branch is unreachable in
			// mainline. If the gate flips on without a non-singular
			// viewProjection, `taa_resolve.frag` will read garbage
			// reprojection — but the same is true of the previous
			// pre-rewrite `inverse` path.
			return inverse;
		}
		const float invPivot = 1.0f / pivotValue;
		for (int scaleCol = 0; scaleCol < 4; ++scaleCol) {
			augmented[pivot * 4 + scaleCol] *= invPivot;
			inverse[pivot * 4 + scaleCol] *= invPivot;
		}
		for (int row = 0; row < 4; ++row) {
			if (row == pivot) {
				continue;
			}
			const float factor = augmented[row * 4 + pivot];
			if (factor == 0.0f) {
				continue;
			}
			for (int elimCol = 0; elimCol < 4; ++elimCol) {
				augmented[row * 4 + elimCol] -= factor * augmented[pivot * 4 + elimCol];
				inverse[row * 4 + elimCol] -= factor * inverse[pivot * 4 + elimCol];
			}
		}
	}
	return inverse;
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
	const uint32_t layerCount = 1u)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcStageMask = srcStageMask;
	imageBarrier.srcAccessMask = srcAccessMask;
	imageBarrier.dstStageMask = dstStageMask;
	imageBarrier.dstAccessMask = dstAccessMask;
	imageBarrier.oldLayout = oldLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.image = image;
	imageBarrier.subresourceRange = {aspectMask, 0, 1, 0, layerCount};

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

bool HasShadowCascadeImageViews(const RenderState &render)
{
	for (const VkImageView cascadeImageView : render.shadowCascadeImageViews) {
		if (cascadeImageView == VK_NULL_HANDLE) {
			return false;
		}
	}
	return true;
}

bool ShouldCaptureScreenshot(const RenderState &render)
{
	return render.screenshotCaptureRequested &&
		   render.screenshotCaptureSupported &&
		   render.screenshotReadbackBuffer != VK_NULL_HANDLE &&
		   render.screenshotReadbackAllocation != VK_NULL_HANDLE &&
		   render.screenshotReadbackMappedData != nullptr;
}

void RecordSwapchainScreenshotCopy(
	const SwapchainState &swapchain,
	RenderState &render,
	const VkCommandBuffer cmd,
	const uint32_t imageIndex)
{
	PV_PROFILE_ZONE_N("RecordSwapchainScreenshotCopy");
	if (!ShouldCaptureScreenshot(render) || imageIndex >= swapchain.images.size()) {
		return;
	}

	TransitionImage(
		cmd,
		swapchain.images[imageIndex],
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT);

	const VkBufferImageCopy copyRegion{
		.bufferOffset = 0u,
		.bufferRowLength = 0u,
		.bufferImageHeight = 0u,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0u,
			.baseArrayLayer = 0u,
			.layerCount = 1u,
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {swapchain.extent.width, swapchain.extent.height, 1u},
	};
	vkCmdCopyImageToBuffer(
		cmd,
		swapchain.images[imageIndex],
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		render.screenshotReadbackBuffer,
		1,
		&copyRegion);

	TransitionImage(
		cmd,
		swapchain.images[imageIndex],
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_2_NONE,
		0);
}

bool SaveRequestedScreenshot(
	VulkanContextState &context,
	RenderState &render,
	const VkFence inFlightFence,
	const VkExtent2D captureExtent,
	const VkFormat captureFormat)
{
	PV_PROFILE_ZONE_N("SaveRequestedScreenshot");
	if (!ShouldCaptureScreenshot(render)) {
		return true;
	}

	const VkResult waitResult = vkWaitForFences(context.device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
	if (waitResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.ScreenshotWaitFence", waitResult);
		render.screenshotCaptureRequested = false;
		return false;
	}

	const uint64_t requiredSize =
		static_cast<uint64_t>(captureExtent.width) *
		static_cast<uint64_t>(captureExtent.height) *
		4u;
	if (requiredSize > render.screenshotReadbackBufferSize) {
		runtime::LogRuntimeFailure(
			"Capture",
			"DrawFrame.ScreenshotBufferSize",
			"screenshot readback buffer is smaller than the captured frame");
		render.screenshotCaptureRequested = false;
		return true;
	}
	const VkResult invalidateResult =
		vmaInvalidateAllocation(context.allocator, render.screenshotReadbackAllocation, 0u, requiredSize);
	if (invalidateResult != VK_SUCCESS) {
		runtime::LogVmaFailure("DrawFrame.ScreenshotInvalidate", invalidateResult);
		render.screenshotCaptureRequested = false;
		return true;
	}

	const std::filesystem::path screenshotPath =
		BuildScreenshotCapturePath(render.currentScenePreset, ++render.screenshotCaptureSequence);
	const std::filesystem::path metadataPath = BuildScreenshotCaptureMetadataPath(screenshotPath.string());
	const bool savedImage = SaveScreenshotCaptureBmp(
		render.screenshotReadbackMappedData,
		captureExtent.width,
		captureExtent.height,
		captureFormat,
		screenshotPath.string());
	const bool savedMetadata = savedImage &&
							   SaveScreenshotCaptureMetadata(
								   render,
								   render.currentScenePreset,
								   screenshotPath.string(),
								   metadataPath.string());
	if (savedImage && savedMetadata) {
		SDL_Log(
			"[ProjectV][Capture][SaveRequestedScreenshot] saved image=%s metadata=%s",
			screenshotPath.string().c_str(),
			metadataPath.string().c_str());
	}

	render.screenshotCaptureRequested = false;
	return true;
}

void RecordShadowCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.shadowMs);
	PV_PROFILE_ZONE_N("RecordShadowCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Shadow Pass");
	if (render.shadowGraphicsPipeline == VK_NULL_HANDLE ||
		render.shadowPipelineLayout == VK_NULL_HANDLE ||
		render.shadowImage == VK_NULL_HANDLE ||
		render.shadowImageView == VK_NULL_HANDLE ||
		!HasShadowCascadeImageViews(render)) {
		return;
	}

	const VkImageLayout oldShadowLayout =
		render.shadowImageNeedsInit ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
	const VkPipelineStageFlags2 oldShadowStage =
		render.shadowImageNeedsInit ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	const VkAccessFlags2 oldShadowAccess =
		render.shadowImageNeedsInit ? 0 : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	TransitionImage(
		cmd,
		render.shadowImage,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		oldShadowLayout,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		oldShadowStage,
		oldShadowAccess,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		kSunShadowCascadeCount);
	render.shadowImageNeedsInit = false;

	constexpr VkClearValue clearDepthValue{.depthStencil = {1.0f, 0}};
	const VkViewport shadowViewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(render.shadowMapExtent.width),
		.height = static_cast<float>(render.shadowMapExtent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D shadowScissor{
		.offset = {0, 0},
		.extent = render.shadowMapExtent,
	};

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.shadowGraphicsPipeline);
	if (frameRenderData.shadowDescriptorSet != VK_NULL_HANDLE) {
		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			render.shadowPipelineLayout,
			0,
			1,
			&frameRenderData.shadowDescriptorSet,
			0,
			nullptr);
	}

	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		const VkRenderingAttachmentInfo shadowDepthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = render.shadowCascadeImageViews[cascadeIndex],
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clearDepthValue,
		};
		const VkRenderingInfo shadowRenderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderArea = {{0, 0}, render.shadowMapExtent},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 0,
			.pColorAttachments = nullptr,
			.pDepthAttachment = &shadowDepthAttachment,
			.pStencilAttachment = nullptr,
		};

		vkCmdBeginRendering(cmd, &shadowRenderingInfo);
		vkCmdSetViewport(cmd, 0, 1, &shadowViewport);
		vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

		const ShadowPushConstants shadowPushConstants{
			.cascadeIndex = cascadeIndex,
		};
		vkCmdPushConstants(
			cmd,
			render.shadowPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(shadowPushConstants),
			&shadowPushConstants);

		const bool canSkipEmptyCascadeDraw =
			frameRenderData.dirtyChunkCount == 0 &&
			frameRenderData.shadowCascadeVisibleChunkCounts[cascadeIndex] == 0u;
		if (frameRenderData.shadowDescriptorSet != VK_NULL_HANDLE &&
			frameRenderData.shadowIndirectCommandCount > 0 &&
			!canSkipEmptyCascadeDraw &&
			frameRenderData.shadowIndirectBuffer != VK_NULL_HANDLE &&
			frameRenderData.packedFaceBuffer != VK_NULL_HANDLE) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Shadow Cascade");
			// Shadows follow sparse per-chunk opaque face ranges. Transparent
			// casters follow the current material policy encoded by the shadow shader.
			const VkDeviceSize shadowIndirectOffset =
				static_cast<VkDeviceSize>(cascadeIndex) *
				static_cast<VkDeviceSize>(frameRenderData.shadowIndirectCommandCount) *
				sizeof(VkDrawIndirectCommand);
			vkCmdDrawIndirect(
				cmd,
				frameRenderData.shadowIndirectBuffer,
				shadowIndirectOffset,
				frameRenderData.shadowIndirectCommandCount,
				sizeof(VkDrawIndirectCommand));
		}

		vkCmdEndRendering(cmd);
	}

	TransitionImage(
		cmd,
		render.shadowImage,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		kSunShadowCascadeCount);
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
	// Snapshot the dirty chunk count for HUD/sidecar at the
	// start of the function (so the value is what was
	// requested this frame, even if the function early-
	// returns because the pipeline is null). On a real
	// dispatch path the value is the same as the
	// `vkCmdDispatch(cmd, frameRenderData.dirtyChunkCount, 1, 1)`
	// count at line ~540 below.
	render.renderPassTimings.dirtyChunkRebuiltCount = frameRenderData.dirtyChunkCount;
	PV_PROFILE_ZONE_N("RecordVoxelMeshingCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Voxel Meshing");
	if (render.voxelMeshingPipeline == VK_NULL_HANDLE ||
		render.voxelMeshingPipelineLayout == VK_NULL_HANDLE ||
		frameRenderData.voxelMeshingDescriptorSet == VK_NULL_HANDLE ||
		frameRenderData.packedFaceBuffer == VK_NULL_HANDLE ||
		frameRenderData.opaqueIndirectBuffer == VK_NULL_HANDLE ||
		frameRenderData.shadowIndirectBuffer == VK_NULL_HANDLE ||
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

	VkBufferMemoryBarrier2 bufferBarriers[4]{};
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
	bufferBarriers[2].buffer = frameRenderData.shadowIndirectBuffer;

	bufferBarriers[3] = bufferBarriers[0];
	bufferBarriers[3].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	bufferBarriers[3].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[3].buffer = frameRenderData.transparentIndirectBuffer;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.bufferMemoryBarrierCount = 4;
	depInfo.pBufferMemoryBarriers = bufferBarriers;
	vkCmdPipelineBarrier2(cmd, &depInfo);
}

void RecordGraphicsCommands(
	RenderState &render,
	const SwapchainState &swapchain,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd,
	const uint32_t imageIndex)
{
	// Per-pass CPU timing for the outer `RecordGraphicsCommands`
	// body. Covers transitions, main pass recording, TAA
	// resolve setup, history copy, etc. — i.e. the
	// `Record*Commands` time minus the explicitly-timed
	// sub-passes (`shadowMs`, `meshingMs`, `taaResolveMs`,
	// `debugOverlayMs`, `debugHudMs`). Each sub-pass has its
	// own `ScopedPassTimer` further down so the HUD line can
	// show both the total and the breakdown.
	ScopedPassTimer passTimer(render.renderPassTimings.graphicsMs);
	PV_PROFILE_ZONE_N("RecordGraphicsCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Graphics Pass");
	{
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Graphics Frame");

		RecordVoxelMeshingCommands(render, frameRenderData, cmd);
		RecordShadowCommands(render, frameRenderData, cmd);

		// The main voxel pipeline is declared with two color attachment
		// formats (slot 0 = swapchain, slot 1 = R16G16B16A16_SFLOAT TAA
		// offscreen) and the `dynamicRenderingUnusedAttachments` feature
		// is enabled in `VulkanBootstrap.cpp`. Per-frame the *active* slot
		// is chosen here: in TAA-off mode slot 0 is the swapchain image
		// and slot 1 is `VK_NULL_HANDLE` (writes discarded), in TAA-on
		// mode slot 0 is `VK_NULL_HANDLE` and slot 1 is the TAA scene
		// color target. The previous code wrote straight to the
		// swapchain; the contract below keeps that exact behaviour for
		// the TAA-off path (slot 0 is the only used attachment, slot 1 =
		// NULL).
		const bool taaOn = render.taaEnabled &&
						   render.taaSceneColorTarget != nullptr && render.taaHistoryColorTarget != nullptr &&
						   render.taaResolvePipeline != VK_NULL_HANDLE && render.taaResolvePipelineLayout != VK_NULL_HANDLE;

		// === Voxel color attachment transitions ===
		// The TAA-on path skips the swapchain transition here; the
		// resolve pass writes to it. The TAA-on path *does* transition
		// the offscreen scene color (UNDEFINED or SHADER_READ_ONLY from
		// last frame → COLOR_ATTACHMENT) for the main pass write.
		if (!taaOn) {
			TransitionImage(
				cmd,
				swapchain.images[imageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_NONE,
				0,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
		} else {
			const VkImageLayout oldSceneLayout = render.taaSceneColorCurrentLayout;
			const VkPipelineStageFlags2 oldSceneStage =
				oldSceneLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? VK_PIPELINE_STAGE_2_NONE
					: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			const VkAccessFlags2 oldSceneAccess =
				oldSceneLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			TransitionImage(
				cmd,
				render.taaSceneColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				oldSceneLayout,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				oldSceneStage,
				oldSceneAccess,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
			render.taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			render.taaSceneColorNeedsInit = false;
		}

		// 1.5 anti-flicker: layer scene color transition. The
		// layer scene color is written by the voxel pass
		// (Location 2 in `vkCmdBeginRendering`) on BOTH the
		// TAA-on and TAA-off paths, so the transition runs
		// unconditionally. Same `oldLayout = current tracker`
		// pattern as the depth + TAA scene transitions above:
		// on the first frame `oldLayout = UNDEFINED` (matches
		// the image's `initialLayout`), on subsequent frames
		// `oldLayout = SHADER_READ_ONLY_OPTIMAL` (matches the
		// post-copy layout tracker). The rendering pass's
		// `imageLayout` declaration is `COLOR_ATTACHMENT_OPTIMAL`
		// so we transition to that here.
		{
			const VkImageLayout oldLayerSceneLayout = render.taaLayerSceneColorCurrentLayout;
			const VkPipelineStageFlags2 oldLayerSceneStage =
				oldLayerSceneLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? VK_PIPELINE_STAGE_2_NONE
					: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			const VkAccessFlags2 oldLayerSceneAccess =
				oldLayerSceneLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? 0
					: VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			TransitionImage(
				cmd,
				render.taaLayerSceneColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				oldLayerSceneLayout,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				oldLayerSceneStage,
				oldLayerSceneAccess,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
			render.taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		// 1.5 anti-flicker: layer history transition. The
		// layer history is read by the voxel pass (binding 6
		// as `sampler2D layerHistory`) on BOTH paths, so the
		// transition runs unconditionally. The descriptor's
		// `imageLayout` is `SHADER_READ_ONLY_OPTIMAL`, so we
		// transition to that here. On the first frame,
		// `oldLayout = UNDEFINED` (matches the image's
		// `initialLayout`); on subsequent frames,
		// `oldLayout = SHADER_READ_ONLY_OPTIMAL` (matches the
		// post-copy layout tracker).
		{
			const VkImageLayout oldLayerHistoryLayout = render.taaLayerHistoryColorCurrentLayout;
			const VkPipelineStageFlags2 oldLayerHistoryStage =
				oldLayerHistoryLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? VK_PIPELINE_STAGE_2_NONE
					: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			const VkAccessFlags2 oldLayerHistoryAccess =
				oldLayerHistoryLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? 0
					: VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			TransitionImage(
				cmd,
				render.taaLayerHistoryColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				oldLayerHistoryLayout,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				oldLayerHistoryStage,
				oldLayerHistoryAccess,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.taaLayerHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		// === Depth image transition (shared by both paths) ===
		// Per-frame: from `depthImageCurrentLayout` (UNDEFINED on the
		// very first frame, `DEPTH_ATTACHMENT_OPTIMAL` after a TAA-off
		// frame, `DEPTH_READ_ONLY_OPTIMAL` after a TAA-on frame) into
		// `DEPTH_ATTACHMENT_OPTIMAL` for the main pass write.
		const VkImageLayout oldDepthLayout = render.depthImageCurrentLayout;
		const VkPipelineStageFlags2 oldDepthStage =
			oldDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED
				? VK_PIPELINE_STAGE_2_NONE
			: oldDepthLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
				? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
				: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		const VkAccessFlags2 oldDepthAccess =
			oldDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED
				? 0
			: oldDepthLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
				? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
				: VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		TransitionImage(
			cmd,
			render.depthImage,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			oldDepthLayout,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			oldDepthStage,
			oldDepthAccess,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
		render.depthImageCurrentLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		render.depthImageNeedsInit = false;

		const std::array<float, 4> sceneClearColor = GetVoxelSceneClearColor(render.currentSceneLighting);
		VkClearValue clearColorValue{};
		clearColorValue.color = {
			{
				sceneClearColor[0],
				sceneClearColor[1],
				sceneClearColor[2],
				sceneClearColor[3],
			},
		};
		constexpr VkClearValue clearDepthValue{.depthStencil = {1.0f, 0}};

		// Main pass uses both pipeline slots. The `imageView` is NULL
		// on the slot that's not in use this frame; with
		// `dynamicRenderingUnusedAttachments` enabled, the driver
		// discards those writes.
		// 1.5 anti-flicker: the main pass also binds the
		// per-layer history pair (Location 2) so the voxel pass
		// can write `outLayerMask` (R = CTSH, G = AOCC, B = LOCL,
		// A = 1.0) into it. The image view is the layer scene
		// color target — the per-frame `vkCmdCopyImage` at the
		// bottom of the frame moves its contents into the layer
		// history target so the next frame's voxel pass can
		// sample them. The pipeline's
		// `pColorAttachmentFormats[2]` is
		// `projectv::taa::kTaaLayerHistoryColorFormat`
		// (`R8G8B8A8_UNORM`), so the format must match — the
		// `VulkanGraphicsPipeline.cpp` `colorAttachmentCount`
		// is now 3 to match. Without this 3rd binding here,
		// the voxel pass writes to Location 2 would be silently
		// dropped by the driver (no validation layer in the
		// current smoke path), and the next frame's history
		// sample would read uninitialised memory — which is
		// what caused the dim regression in the first 1.5
		// smoke before this fix.
		const VkImageView mainColor0View = taaOn ? VK_NULL_HANDLE : swapchain.imageViews[imageIndex];
		const VkImageView mainColor1View = taaOn ? render.taaSceneColorTarget->imageView : VK_NULL_HANDLE;
		const VkImageView mainColor2View = render.taaLayerSceneColorTarget != nullptr
											   ? render.taaLayerSceneColorTarget->imageView
											   : VK_NULL_HANDLE;
		// JetBrains DFA flags `mainColorXView != VK_NULL_HANDLE` as
		// "always false" because the indexer can't follow the runtime
		// `taaOn` ternary that produces these views. The branch IS
		// reachable: when `taaOn` is false, slot 0 is the swapchain
		// (non-null) and we must STORE; when TAA is on, slot 0 is
		// unused (`dynamicRenderingUnusedAttachments` feature)
		// and DONT_CARE is correct. The DFA path-insensitive
		// `?:` folding misses this, so suppress the
		// `CppDFAConstantConditions` / `CppDFAUnreachableCode`
		// report per-line. The actual condition is exercised in
		// production on every TAA-off frame.
		const VkRenderingAttachmentInfo colorAttachment0{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = mainColor0View,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			// noinspection CppDFAConstantConditions, CppDFAUnreachableCode
			.storeOp = mainColor0View != VK_NULL_HANDLE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachment1{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = mainColor1View,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = mainColor1View != VK_NULL_HANDLE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachment2{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = mainColor2View,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = mainColor2View != VK_NULL_HANDLE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachments[3] = {colorAttachment0, colorAttachment1, colorAttachment2};
		const VkRenderingAttachmentInfo depthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = render.depthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearDepthValue,
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderArea = {{0, 0}, swapchain.extent},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 3,
			.pColorAttachments = colorAttachments,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = nullptr,
		};

		vkCmdBeginRendering(cmd, &renderingInfo);

		const VkViewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(swapchain.extent.width),
			.height = static_cast<float>(swapchain.extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		const VkRect2D scissor{
			.offset = {0, 0},
			.extent = swapchain.extent,
		};
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		if (frameRenderData.graphicsDescriptorSet != VK_NULL_HANDLE) {
			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				render.graphicsPipelineLayout,
				0,
				1,
				&frameRenderData.graphicsDescriptorSet,
				0,
				nullptr);
		}

		if (frameRenderData.graphicsDescriptorSet != VK_NULL_HANDLE &&
			frameRenderData.chunkDescriptorCount > 0 &&
			frameRenderData.opaqueIndirectBuffer != VK_NULL_HANDLE &&
			frameRenderData.packedFaceBuffer != VK_NULL_HANDLE) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Opaque Pass");
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taaOn ? render.graphicsPipelineTaaOn : render.graphicsPipeline);
			vkCmdPushConstants(
				cmd,
				render.graphicsPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(frameRenderData.graphicsPushConstants),
				&frameRenderData.graphicsPushConstants);
			vkCmdDrawIndirect(
				cmd,
				frameRenderData.opaqueIndirectBuffer,
				0,
				frameRenderData.chunkDescriptorCount,
				sizeof(VkDrawIndirectCommand));
		}

		// M4: polygon-model import pass. Same depth attachment as the
		// opaque pass, same descriptor set (kept bound from the opaque
		// pass — the model pipeline reuses the same
		// `graphicsPipelineLayout`). Per-instance world transform
		// lives in `render.modelInstances`; `model.vert` reuses the
		// same push constant struct as the voxel pass and only reads
		// `viewProjection` (offset 0) and `modelTransform` (offset
		// 64).
		// M5: iterate `visibleModelInstances` (the per-frame
		// frustum-culled subset built in
		// `FramePreparation::BuildVisibleModelInstanceList`) instead
		// of the raw `modelInstances`. Off-screen / max-distance
		// instances never generate a draw call, so the GPU side
		// stays untouched even when the manifest grows.
		if (render.modelPipeline != VK_NULL_HANDLE && !render.visibleModelInstances.empty()) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Model Pass");
			vkCmdBindPipeline(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				taaOn && render.modelPipelineTaaOn != VK_NULL_HANDLE
					? render.modelPipelineTaaOn
					: render.modelPipeline);
			struct ModelPush {
				std::array<float, 16> viewProjection{};
				std::array<float, 16> modelTransform{};
			};
			ModelPush push{};
			std::memcpy(
				push.viewProjection.data(),
				frameRenderData.graphicsPushConstants.viewProjection.data(),
				sizeof(float) * 16);
			for (const ModelInstanceData &instance : render.visibleModelInstances) {
				if (instance.indexCount == 0 || instance.vertexBuffer == VK_NULL_HANDLE || instance.indexBuffer == VK_NULL_HANDLE) {
					continue;
				}
				std::memcpy(
					push.modelTransform.data(),
					instance.modelTransform.data(),
					sizeof(float) * 16);
				vkCmdPushConstants(
					cmd,
					render.graphicsPipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(ModelPush),
					&push);
				constexpr VkDeviceSize vertexOffset = 0;
				vkCmdBindVertexBuffers(cmd, 0, 1, &instance.vertexBuffer, &vertexOffset);
				vkCmdBindIndexBuffer(
					cmd,
					instance.indexBuffer,
					0,
					VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, instance.indexCount, 1, 0, 0, 0);
			}
		}

		if ((taaOn ? render.transparentGraphicsPipelineTaaOn : render.transparentGraphicsPipeline) &&
			frameRenderData.graphicsDescriptorSet != VK_NULL_HANDLE &&
			frameRenderData.chunkDescriptorCount > 0 &&
			frameRenderData.transparentIndirectBuffer != VK_NULL_HANDLE &&
			frameRenderData.packedFaceBuffer != VK_NULL_HANDLE) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Transparent Pass");
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taaOn ? render.transparentGraphicsPipelineTaaOn : render.transparentGraphicsPipeline);
			vkCmdPushConstants(
				cmd,
				render.graphicsPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(frameRenderData.graphicsPushConstants),
				&frameRenderData.graphicsPushConstants);
			vkCmdDrawIndirect(
				cmd,
				frameRenderData.transparentIndirectBuffer,
				0,
				frameRenderData.chunkDescriptorCount,
				sizeof(VkDrawIndirectCommand));
		}

		// Debug overlay / debug HUD are written *on top of* the final
		// image. TAA-off: they go straight to the swapchain in the same
		// main pass. TAA-on: the main pass writes to the offscreen scene
		// color, so we move the overlay / HUD into the resolve pass
		// block below where the swapchain is the active color
		// attachment.
		if (!taaOn) {
			RecordDebugOverlayCommands(render, swapchain, frameRenderData, cmd);
			RecordDebugHudCommands(render, frameRenderData, cmd);
		}

		vkCmdEndRendering(cmd);

		// 1.5 anti-flicker: sync the layout trackers with the actual
		// GPU image layouts after the main pass ends. The voxel pass
		// auto-transitioned the layer scene color target from
		// `UNDEFINED` to `COLOR_ATTACHMENT_OPTIMAL` (via the
		// rendering pass's `imageLayout` in `VkRenderingAttachmentInfo`),
		// and the layer history target is still in
		// `SHADER_READ_ONLY_OPTIMAL` (read-only during the voxel pass,
		// no transition triggered). The copy block below uses these
		// trackers as `oldLayout` for the `vkCmdPipelineBarrier2`
		// calls, so they have to match the actual GPU state — without
		// this sync, the first frame's barrier would have
		// `oldLayout = UNDEFINED` but actual = `COLOR_ATTACHMENT_OPTIMAL`
		// and validation would fail (VUID-VkImageMemoryBarrier2-oldLayout-01197).
		// The voxel pass writes to the layer scene color target
		// (Location 2) in BOTH the TAA-on and TAA-off paths, so the
		// post-pass layout is `COLOR_ATTACHMENT_OPTIMAL` for both.
		// The history target is read-only in both paths, so it
		// stays in its initial `SHADER_READ_ONLY_OPTIMAL` (set by
		// the `initialLayout` in `TaaRenderTargets.cpp` and tracked
		// in `taaLayerHistoryColorCurrentLayout`).
		render.taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// === TAA resolve pass + history copy (TAA on only) ===
		if (taaOn) {
			// Scene color: COLOR_ATTACHMENT → SHADER_READ_ONLY for
			// the resolve pass to sample it.
			TransitionImage(
				cmd,
				render.taaSceneColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Depth: DEPTH_ATTACHMENT → DEPTH_READ_ONLY so the
			// resolve pass can sample it for depth-based
			// reprojection.
			TransitionImage(
				cmd,
				render.depthImage,
				VK_IMAGE_ASPECT_DEPTH_BIT,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.depthImageCurrentLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

			// History: SHADER_READ_ONLY is the *default* end-of-frame
			// layout; if this is the very first frame after a
			// recreate / Taa toggle, the offscreen image is fresh
			// and is still UNDEFINED. Skip the layout transition in
			// that case and use UNDEFINED → SHADER_READ_ONLY
			// directly, so the resolve pass can sample it (it just
			// reads garbage; `taaHistoryValid == false` gates the
			// shader's blend factor to fall back to the current
			// frame).
			TransitionImage(
				cmd,
				render.taaHistoryColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				render.taaHistoryColorCurrentLayout,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				render.taaHistoryColorCurrentLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? VK_PIPELINE_STAGE_2_NONE
					: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				render.taaHistoryColorCurrentLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0
																				 : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			render.taaHistoryNeedsInit = false;

			// Swapchain: UNDEFINED (fresh from presentation engine) →
			// COLOR_ATTACHMENT_OPTIMAL for the resolve pass. The TAA-off
			// path transitions the swapchain at the top of the frame, but
			// the TAA-on path skips that because the main pass writes to
			// the offscreen target instead.
			TransitionImage(
				cmd,
				swapchain.images[imageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_NONE,
				0,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

			// === Begin resolve pass ===
			const VkRenderingAttachmentInfo resolveColorAttachment{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.pNext = nullptr,
				.imageView = swapchain.imageViews[imageIndex],
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.resolveMode = VK_RESOLVE_MODE_NONE,
				.resolveImageView = VK_NULL_HANDLE,
				.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = clearColorValue,
			};
			const VkRenderingInfo resolveRenderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.pNext = nullptr,
				.flags = 0,
				.renderArea = {{0, 0}, swapchain.extent},
				.layerCount = 1,
				.viewMask = 0,
				.colorAttachmentCount = 1,
				.pColorAttachments = &resolveColorAttachment,
				.pDepthAttachment = nullptr,
				.pStencilAttachment = nullptr,
			};
			vkCmdBeginRendering(cmd, &resolveRenderingInfo);
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "TAA Resolve");
			// noinspection CppShadowVariable, CppDeclaratorNeverUsed
			// clangd parser doesn't expand `__COUNTER__` correctly
			// inside the `PV_PROFILE_GPU_LABEL_COLOR` macro body, so
			// it sees the same `_pvGpuLabel<N>` identifier name twice
			// and reports "shadows a local variable". The real
			// expansion uses a fresh `__COUNTER__` value per call,
			// so each `_pvGpuLabel<N>` is unique and there is no
			// shadow. Build (clang++ 22) is green.
			PV_PROFILE_GPU_LABEL_COLOR(cmd, "TAA Resolve", 0.20f, 0.65f, 1.00f, 1.0f);
			// Per-pass CPU timing for the inline TAA resolve
			// section. Manual start/end (not `ScopedPassTimer`)
			// because the function is too large to wrap a
			// timer around the whole thing — the timer only
			// covers the resolve-push-constant build + bind +
			// draw, not the surrounding `vkCmdBeginRendering` /
			// `vkCmdSetViewport` / `vkCmdSetScissor` setup
			// (those are part of the TAA-on `graphicsMs` body
			// measurement instead, so the operator can still
			// see the cost when TAA is on).
			const Uint64 taaResolveStartCounter = SDL_GetPerformanceCounter();

			// Push constants for the resolve pass. The current
			// viewProjection comes from the per-frame
			// `graphicsPushConstants`; the inverse is built locally
			// via Gauss-Jordan on the column-major 4x4 (see
			// `InvertColumnMajorMat4` in the anonymous namespace
			// above). The resolve shader expects both in the same
			// column-major layout the CPU uses.
			const std::array<float, 16> currentViewProj = frameRenderData.graphicsPushConstants.viewProjection;
			const std::array<float, 16> inverseCurrentViewProj = InvertColumnMajorMat4(currentViewProj);
			ResolvePushConstants resolvePushConstants{};
			resolvePushConstants.inverseCurrentViewProjection = inverseCurrentViewProj;
			resolvePushConstants.currentViewProjection = currentViewProj;
			resolvePushConstants.renderExtentInverse = {
				1.0f / static_cast<float>(swapchain.extent.width),
				1.0f / static_cast<float>(swapchain.extent.height),
			};
			// CAS (1.3) inputs. `taaBlend` is 0 when TAA is off so the
			// sharpen derivation in `taa_resolve.frag` falls back to
			// `1.0 * taaCasSharpnessMax` (the user-authored ceiling)
			// without a separate toggle.
			resolvePushConstants.taaBlend = render.taaEnabled ? render.taaBlend : 0.0f;
			resolvePushConstants.taaCasSharpnessMax = render.taaCasSharpnessMax;

			if (frameRenderData.taaResolveDescriptorSet != VK_NULL_HANDLE) {
				vkCmdBindDescriptorSets(
					cmd,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					render.taaResolvePipelineLayout,
					0,
					1,
					&frameRenderData.taaResolveDescriptorSet,
					0,
					nullptr);
			}
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.taaResolvePipeline);
			vkCmdPushConstants(
				cmd,
				render.taaResolvePipelineLayout,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(resolvePushConstants),
				&resolvePushConstants);
			// Fullscreen triangle, no vertex buffer — `taa_resolve.vert`
			// synthesizes positions from `gl_VertexIndex` (0, 1, 2).
			vkCmdDraw(cmd, 3, 1, 0, 0);
			// Close out the TAA resolve CPU timing.
			{
				const Uint64 taaResolveEndCounter = SDL_GetPerformanceCounter();
				const double seconds = static_cast<double>(taaResolveEndCounter - taaResolveStartCounter) /
									   static_cast<double>(SDL_GetPerformanceFrequency());
				render.renderPassTimings.taaResolveMs = static_cast<float>(seconds * 1000.0);
			}

			// Debug overlay / debug HUD go on top of the resolved
			// swapchain image, in the same `vkCmdBeginRendering`
			// block as the resolve pass so they share the swapchain
			// attachment and the same `vkCmdSetViewport` /
			// `vkCmdSetScissor` already bound for the resolve pass.
			RecordDebugOverlayCommands(render, swapchain, frameRenderData, cmd);
			RecordDebugHudCommands(render, frameRenderData, cmd);

			vkCmdEndRendering(cmd);

			// === History copy (skip on the first frame after
			// recreate / Taa toggle so the shader's `taaHistoryValid`
			// flag is allowed to drop to zero) ===
			if (render.taaHistoryValid) {
				// Scene → TRANSFER_SRC, History → TRANSFER_DST.
				TransitionImage(
					cmd,
					render.taaSceneColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT);
				render.taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				TransitionImage(
					cmd,
					render.taaHistoryColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT);
				render.taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

				// Same-format copy is simpler than blit (no scaling,
				// no filter). The two images were created with the
				// same extent, format, and sample count.
				const VkImageCopy historyCopyRegion{
					.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.srcOffset = {0, 0, 0},
					.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.dstOffset = {0, 0, 0},
					.extent = {swapchain.extent.width, swapchain.extent.height, 1u},
				};
				vkCmdCopyImage(
					cmd,
					render.taaSceneColorTarget->image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					render.taaHistoryColorTarget->image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&historyCopyRegion);

				// Transition both back to SHADER_READ_ONLY so the
				// next frame's resolve pass can sample them.
				TransitionImage(
					cmd,
					render.taaSceneColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
				render.taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				TransitionImage(
					cmd,
					render.taaHistoryColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
				render.taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			} else {
				// First valid frame: from the next frame on, the
				// shader can blend against the freshly-resolved
				// history instead of falling back to the current
				// scene as the only sample.
				render.taaHistoryValid = true;
			}
		}

		// 1.5 anti-flicker: copy the current frame's
		// `outLayerMask` from the layer scene color target to the
		// layer history color target. Same ping-pong shape as
		// the colour history above, except the copy happens
		// every frame (the colour history copy is conditional
		// on `taaHistoryValid` for the resolve-pass init dance,
		// but the voxel pass always writes `outLayerMask` so
		// the layer history is unconditionally valid after the
		// first frame — `taaLayerHistoryValid` is set true on
		// the very first frame's copy, just like the colour
		// history's `else` branch above). The 1.5 init flag
		// still exists for the first-frame-after-recreate
		// case where the swapchain was just rebuilt and the
		// voxel pass shouldn't try to read a zero-initialised
		// history.
		if (render.taaLayerSceneColorTarget != nullptr && render.taaLayerHistoryColorTarget != nullptr && render.taaLayerSceneColorTarget->image != VK_NULL_HANDLE && render.taaLayerHistoryColorTarget->image != VK_NULL_HANDLE) {
			if (render.taaLayerHistoryValid) {
				TransitionImage(
					cmd,
					render.taaLayerSceneColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					render.taaLayerSceneColorCurrentLayout,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT);
				render.taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				TransitionImage(
					cmd,
					render.taaLayerHistoryColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					render.taaLayerHistoryColorCurrentLayout,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT);
				render.taaLayerHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

				const VkImageCopy layerHistoryCopyRegion{
					.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.srcOffset = {0, 0, 0},
					.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.dstOffset = {0, 0, 0},
					.extent = {swapchain.extent.width, swapchain.extent.height, 1u},
				};
				vkCmdCopyImage(
					cmd,
					render.taaLayerSceneColorTarget->image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					render.taaLayerHistoryColorTarget->image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&layerHistoryCopyRegion);

				TransitionImage(
					cmd,
					render.taaLayerSceneColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					render.taaLayerSceneColorCurrentLayout,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
				render.taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				TransitionImage(
					cmd,
					render.taaLayerHistoryColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					render.taaLayerHistoryColorCurrentLayout,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
				render.taaLayerHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			} else {
				// First valid frame: same pattern as the colour
				// history `else` branch above. The next frame's
				// voxel pass can sample the freshly-copied
				// history instead of falling back to the raw
				// current value (the `blend = mix(raw, history,
				// layerBlend)` then weights 0% history, 100% raw,
				// which is the no-temporal-smoothing baseline).
				render.taaLayerHistoryValid = true;
			}
		}

		if (ShouldCaptureScreenshot(render)) {
			RecordSwapchainScreenshotCopy(swapchain, render, cmd, imageIndex);
		} else {
			TransitionImage(
				cmd,
				swapchain.images[imageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_NONE,
				0);
		}
	}

	profiling::CollectVulkanGpu(render.tracyGraphicsContext, cmd);
}
} // namespace

SDL_AppResult DrawFrame(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render,
	FrameState *frame)
{
	PV_PROFILE_ZONE_N("DrawFrame");
	if (!platform || !context || !swapchain || !render || !frame) {
		return SDL_APP_CONTINUE;
	}

	if (swapchain->extent.width == 0 || swapchain->extent.height == 0) {
		if (!RecreateSwapchain(platform, context, swapchain, render)) {
			runtime::LogRuntimeFailure(
				"Render",
				"DrawFrame.RecreateSwapchainZeroExtent",
				"RecreateSwapchain returned false while swapchain extent is zero");
			return SDL_APP_FAILURE;
		}
		platform->windowResized = false;
		return SDL_APP_CONTINUE;
	}
	if (swapchain->handle == VK_NULL_HANDLE) {
		runtime::LogRuntimeFailure("Render", "DrawFrame.SwapchainHandle", "swapchain handle is null");
		return SDL_APP_FAILURE;
	}
	if (render->screenshotCaptureRequested && !render->screenshotCaptureSupported) {
		runtime::LogRuntimeFailure(
			"Capture",
			"DrawFrame.ScreenshotSupport",
			"screenshot capture is unavailable for the current swapchain");
		render->screenshotCaptureRequested = false;
	}

	const uint32_t currentFrame = frame->currentFrame;
	const size_t currentFrameIndex = currentFrame;
	if (currentFrameIndex >= frame->commandBuffers.size() ||
		currentFrameIndex >= frame->inFlightFences.size() ||
		currentFrameIndex >= frame->imageAvailableSemaphores.size() ||
		currentFrameIndex >= frame->renderFinishedSemaphores.size()) {
		runtime::LogRuntimeFailure("Render", "DrawFrame.FrameState", "FrameState is incomplete");
		return SDL_APP_FAILURE;
	}
	if (frame->renderData.frameIndex != currentFrame) {
		runtime::LogRuntimeFailure(
			"Render",
			"DrawFrame.FrameRenderData",
			fmt::format("FrameRenderData is not prepared for frame {}", currentFrame));
		return SDL_APP_FAILURE;
	}

	const VkCommandBuffer cmd = frame->commandBuffers[currentFrameIndex];
	const VkFence inFlightFence = frame->inFlightFences[currentFrameIndex];
	const VkSemaphore imageAvailableSemaphore = frame->imageAvailableSemaphores[currentFrameIndex];
	const VkExtent2D captureExtent = swapchain->extent;
	const VkFormat captureFormat = swapchain->format;

	uint32_t imageIndex = 0;
	// `vkAcquireNextImageKHR` is given the per-in-flight-frame
	// `imageAvailableSemaphore`. The driver signals it when the
	// returned `imageIndex` becomes writable. The submit pipeline (below)
	// waits on this same handle, so the command buffer does not start
	// rendering before the swapchain image is actually writable. The
	// guide `swapchain_semaphore_reuse.html` uses *exactly* this pattern:
	// per-frame acquire-semaphore, per-image submit-semaphore, no
	// per-image acquire fence (the fence pattern requires
	// `VK_KHR_swapchain_maintenance1`).
	const VkResult acquireRes = vkAcquireNextImageKHR(
		context->device,
		swapchain->handle,
		UINT64_MAX,
		imageAvailableSemaphore,
		VK_NULL_HANDLE,
		&imageIndex);
	if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
		if (!RecreateSwapchain(platform, context, swapchain, render)) {
			runtime::LogRuntimeFailure(
				"Render",
				"DrawFrame.RecreateSwapchainAfterAcquire",
				fmt::format(
					"RecreateSwapchain returned false after vkAcquireNextImageKHR returned {} ({})",
					VkResultToString(acquireRes),
					static_cast<int>(acquireRes)));
			return SDL_APP_FAILURE;
		}
		platform->windowResized = false;
		return SDL_APP_CONTINUE;
	}
	if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
		runtime::LogVkFailure("DrawFrame.vkAcquireNextImageKHR", acquireRes);
		return SDL_APP_FAILURE;
	}

	const VkResult resetFenceResult = vkResetFences(context->device, 1, &inFlightFence);
	if (resetFenceResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkResetFences", resetFenceResult);
		return SDL_APP_FAILURE;
	}
	const VkResult resetCommandBufferResult = vkResetCommandBuffer(cmd, 0);
	if (resetCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkResetCommandBuffer", resetCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	const VkResult beginCommandBufferResult = vkBeginCommandBuffer(cmd, &beginInfo);
	if (beginCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkBeginCommandBuffer", beginCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	RecordGraphicsCommands(*render, *swapchain, frame->renderData, cmd, imageIndex);

	const VkResult endCommandBufferResult = vkEndCommandBuffer(cmd);
	if (endCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkEndCommandBuffer", endCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	// `pWaitSemaphores[0]` is the per-in-flight-frame `imageAvailableSemaphore`,
	// signaled by the `vkAcquireNextImageKHR` call above when the swapchain
	// image became available. The submit pipeline waits on it so the
	// command buffer does not start rendering before the swapchain image
	// is actually writable.
	waitSemaphoreInfo.semaphore = imageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	// `pSignalSemaphores[0]` is the per-swapchain-image `submitSemaphore`,
	// *not* the per-in-flight-frame `renderFinishedSemaphore`. The
	// canonical Vulkan pattern (per the SDK 1.4 guide
	// `swapchain_semaphore_reuse.html`) is to index the submit-finished
	// semaphore by `imageIndex` rather than by frame counter, because
	// two consecutive in-flight frames can be handed the same
	// `imageIndex` before the first one's present has retired its
	// `pWaitSemaphores`. We then present the same `submitSemaphore`
	// below in `presentInfo.pWaitSemaphores`.
	const VkSemaphore submitSemaphore = swapchain->submitSemaphores[imageIndex];
	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = submitSemaphore;
	// Screenshot capture records a transfer copy after color rendering; present
	// must not observe the swapchain image before that copy and layout transition finish.
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkCommandBufferSubmitInfo cmdBufferInfo{};
	cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdBufferInfo.commandBuffer = cmd;

	VkSubmitInfo2 submitInfo2{};
	submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo2.waitSemaphoreInfoCount = 1;
	submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo2.commandBufferInfoCount = 1;
	submitInfo2.pCommandBufferInfos = &cmdBufferInfo;
	submitInfo2.signalSemaphoreInfoCount = 1;
	submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;
	const VkResult submitResult = vkQueueSubmit2(context->queue, 1, &submitInfo2, inFlightFence);
	if (submitResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkQueueSubmit2", submitResult);
		return SDL_APP_FAILURE;
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &submitSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain->handle;
	presentInfo.pImageIndices = &imageIndex;

	const VkResult presentRes = vkQueuePresentKHR(context->queue, &presentInfo);
	if (!SaveRequestedScreenshot(*context, *render, inFlightFence, captureExtent, captureFormat)) {
		return SDL_APP_FAILURE;
	}
	if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || platform->windowResized) {
		if (!RecreateSwapchain(platform, context, swapchain, render)) {
			runtime::LogRuntimeFailure(
				"Render",
				"DrawFrame.RecreateSwapchainAfterPresent",
				"RecreateSwapchain returned false after vkQueuePresentKHR/window lifecycle refresh");
			return SDL_APP_FAILURE;
		}
		platform->windowResized = false;
	} else if (presentRes != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkQueuePresentKHR", presentRes);
		return SDL_APP_FAILURE;
	}

	frame->currentFrame = (frame->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return SDL_APP_CONTINUE;
}
