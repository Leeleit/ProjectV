#include "render/RendererInternal.hpp"

#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/Cloudscape.hpp"
#include "render/HizCulling.hpp"
#include "render/PostFx.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/SkyAtmosphere.hpp"

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

void RecordGraphicsCommands(
	RenderState &render,
	const SwapchainState &swapchain,
	const FrameRenderData &frameRenderData,
	VulkanContextState &context,
	const VkCommandBuffer cmd,
	const uint32_t imageIndex)
{
	ScopedPassTimer passTimer(render.renderPassTimings.graphicsMs);
	PV_PROFILE_ZONE_N("RecordGraphicsCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Graphics Pass");
	{
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Graphics Frame");

		RecordVoxelMeshingCommands(render, frameRenderData, cmd);
		RecordShadowCommands(render, frameRenderData, cmd);

		TransitionImage(
			cmd,
			render.sceneColorImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			render.sceneColorCurrentLayout,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			render.sceneColorCurrentLayout == VK_IMAGE_LAYOUT_UNDEFINED
				? VK_PIPELINE_STAGE_2_NONE
				: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			render.sceneColorCurrentLayout == VK_IMAGE_LAYOUT_UNDEFINED
				? 0
				: VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
		render.sceneColorCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		render.sceneColorNeedsInit = false;

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

		const bool skyPassActive = projectv::render::IsSkyAtmosphereEnabled() &&
								   render.skyAtmospherePipelineEnabled &&
								   render.sceneColorImageView != VK_NULL_HANDLE;
		const VkAttachmentLoadOp sceneColorLoadOp = skyPassActive ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
		const VkAttachmentLoadOp depthLoadOp = skyPassActive ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;

		const VkRenderingAttachmentInfo colorAttachment0{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = render.sceneColorImageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = sceneColorLoadOp,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachments[1] = {colorAttachment0};
		const VkRenderingAttachmentInfo depthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = render.depthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = depthLoadOp,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clearDepthValue,
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderArea = {{0, 0}, swapchain.extent},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachments = colorAttachments,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = nullptr,
		};

		if (skyPassActive) {
			projectv::render::SkyAtmospherePushConstants skyPush{};
			skyPush.zenithColorAndIntensity = {
				render.currentSceneLighting.skyColorAndFogDensity[0],
				render.currentSceneLighting.skyColorAndFogDensity[1],
				render.currentSceneLighting.skyColorAndFogDensity[2],
				1.0f,
			};
			skyPush.horizonColorAndSunIntensity = {
				render.currentSceneLighting.horizonColorAndFogStart[0],
				render.currentSceneLighting.horizonColorAndFogStart[1],
				render.currentSceneLighting.horizonColorAndFogStart[2],
				render.currentSceneLighting.sunColorAndIntensity[3],
			};
			skyPush.sunDirectionAndAngularSize = {
				render.currentSceneLighting.sunDirectionAndWrap[0],
				render.currentSceneLighting.sunDirectionAndWrap[1],
				render.currentSceneLighting.sunDirectionAndWrap[2],
				0.045f,
			};
			const float aspectRatio = static_cast<float>(swapchain.extent.width) /
									  static_cast<float>(std::max(swapchain.extent.height, 1u));
			const float tanHalfFovY = std::tan(frameRenderData.graphicsPushConstants.viewProjection.data()[5] * 0.5f);
			skyPush.viewParams = {0.1f, aspectRatio, tanHalfFovY, 0.0f};

			projectv::render::RecordSkyAtmospherePass(
				cmd,
				render,
				skyPush,
				render.sceneColorImageView,
				render.depthImageView,
				swapchain.extent,
				imageIndex);
		}

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
			if (render.meshShaderEnabled) {
				projectv::render::MeshDrawPushConstants meshDrawPush{};
				const auto &viewProj = frameRenderData.graphicsPushConstants.viewProjection;
				for (size_t i = 0; i < 16; ++i) {
					meshDrawPush.viewProjection[i] = viewProj.data()[i];
				}
				meshDrawPush.worldMinAndChunkSize = frameRenderData.voxelMeshingPushConstants.worldMinAndChunkSize;
				meshDrawPush.worldMaxExclusiveAndChunkCount = frameRenderData.voxelMeshingPushConstants.worldMaxExclusiveAndChunkCount;
				meshDrawPush.chunkGridAndTransparentFaceBase = frameRenderData.voxelMeshingPushConstants.chunkGridAndTransparentFaceBase;
				meshDrawPush.faceCapacities = frameRenderData.voxelMeshingPushConstants.faceCapacities;
				projectv::render::RecordMeshShaderDraw(
					cmd,
					render,
					render.sceneFrameResources[frameRenderData.frameIndex],
					meshDrawPush,
					frameRenderData.chunkDescriptorCount);
			} else {
				const bool rtxPathActive = render.rayTracedShadows != nullptr && render.rayTracedShadows->IsEnabled() && render.rayTracedShadows->GetConfig().tlas != VK_NULL_HANDLE;
				VkPipeline opaquePipeline = VK_NULL_HANDLE;
				if (rtxPathActive) {
					opaquePipeline = render.graphicsPipelineRtx;
				}
				if (opaquePipeline == VK_NULL_HANDLE) {
					opaquePipeline = render.graphicsPipeline;
				}
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipeline);
				vkCmdPushConstants(
					cmd,
					render.graphicsPipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(frameRenderData.graphicsPushConstants),
					&frameRenderData.graphicsPushConstants);
				const bool hzbCullingActive =
					projectv::render::IsHzbCullingEnabled() &&
					frameRenderData.hzbVisibleCountBuffer != VK_NULL_HANDLE;
				if (hzbCullingActive) {
					VkBufferMemoryBarrier2 indirectBarrier{};
					indirectBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
					indirectBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
					indirectBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
					indirectBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
					indirectBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
					indirectBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					indirectBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					indirectBarrier.buffer = frameRenderData.hzbVisibleCountBuffer;
					indirectBarrier.offset = 0u;
					indirectBarrier.size = sizeof(uint32_t);

					VkDependencyInfo indirectDepInfo{};
					indirectDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
					indirectDepInfo.bufferMemoryBarrierCount = 1u;
					indirectDepInfo.pBufferMemoryBarriers = &indirectBarrier;
					vkCmdPipelineBarrier2(cmd, &indirectDepInfo);

					vkCmdDrawIndirectCountKHR(
						cmd,
						frameRenderData.opaqueIndirectBuffer,
						0u,
						frameRenderData.hzbVisibleCountBuffer,
						0u,
						frameRenderData.chunkDescriptorCount,
						sizeof(VkDrawIndirectCommand));
				} else {
					vkCmdDrawIndirect(
						cmd,
						frameRenderData.opaqueIndirectBuffer,
						0,
						frameRenderData.chunkDescriptorCount,
						sizeof(VkDrawIndirectCommand));
				}
			}
		}

		if (render.modelPipeline != VK_NULL_HANDLE && !render.visibleModelInstances.empty()) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Model Pass");
			vkCmdBindPipeline(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				render.modelPipeline);
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
				if (context.maintenance5) {
					vkCmdBindIndexBuffer2(cmd, instance.indexBuffer, 0, VK_WHOLE_SIZE, instance.indexType);
				} else {
					vkCmdBindIndexBuffer(cmd, instance.indexBuffer, 0, instance.indexType);
				}
				vkCmdDrawIndexed(cmd, instance.indexCount, 1, 0, 0, 0);
			}
		}

		if (render.transparentGraphicsPipeline &&
			frameRenderData.graphicsDescriptorSet != VK_NULL_HANDLE &&
			frameRenderData.chunkDescriptorCount > 0 &&
			frameRenderData.transparentIndirectBuffer != VK_NULL_HANDLE &&
			frameRenderData.packedFaceBuffer != VK_NULL_HANDLE) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Transparent Pass");
			const bool greedyDebug = render.lightingDebugControls.debugView == LightingDebugView::GreedyMeshing;
			const VkPipeline transparentPipeline = greedyDebug && render.transparentDebugGraphicsPipeline
													   ? render.transparentDebugGraphicsPipeline
													   : render.transparentGraphicsPipeline;
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline);
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

		RecordDebugOverlayCommands(render, swapchain, frameRenderData, cmd);
		RecordDebugHudCommands(render, frameRenderData, cmd);

		vkCmdEndRendering(cmd);

		const bool cloudscapePassActive = projectv::render::IsCloudscapeEnabled() &&
										  render.cloudscapePipelineEnabled;
		if (cloudscapePassActive && render.sceneColorImageView != VK_NULL_HANDLE) {
			projectv::render::CloudscapePushConstants cloudPush{};
			cloudPush.cloudColorAndCoverage = {
				projectv::render::kDefaultCloudColorR,
				projectv::render::kDefaultCloudColorG,
				projectv::render::kDefaultCloudColorB,
				projectv::render::kDefaultCloudCoverage,
			};
			cloudPush.sunDirectionAndIntensity = {
				render.currentSceneLighting.sunDirectionAndWrap[0],
				render.currentSceneLighting.sunDirectionAndWrap[1],
				render.currentSceneLighting.sunDirectionAndWrap[2],
				render.currentSceneLighting.sunColorAndIntensity[3],
			};
			cloudPush.cloudLayerParams = {
				0.0f,
				0.0f,
				projectv::render::kDefaultCloudContrast,
				0.0f,
			};
			const float aspectRatio = static_cast<float>(swapchain.extent.width) /
									  static_cast<float>(std::max(swapchain.extent.height, 1u));
			const float tanHalfFovY = std::tan(frameRenderData.graphicsPushConstants.viewProjection.data()[5] * 0.5f);
			cloudPush.viewParams = {
				render.currentSceneLighting.sunContactShadowParams[1],
				aspectRatio,
				tanHalfFovY,
				0.0f,
			};

			projectv::render::RecordCloudscapeRaymarchPass(
				cmd,
				render,
				cloudPush,
				render.sceneColorImageView,
				render.depthImageView,
				swapchain.extent,
				imageIndex);
		}

		if (render.rayTracedShadows != nullptr) {
			PV_PROFILE_ZONE_N("RecordRayTracedShadowPass");
			projectv::render::RecordRayTracedShadowPass(
				cmd,
				render.rayTracedShadows,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_ACCESS_SHADER_READ_BIT);
		}

		// Scene color → swapchain blit (replaces former TAA resolve pass)
		bool postFxActive = projectv::render::IsPostFxEnabled();
		if (postFxActive) {
			postFxActive = projectv::render::CreatePostFxResources(&context, &render, swapchain.extent);
			if (postFxActive) {
				projectv::render::RecordPostFxPass(
					cmd,
					context,
					render,
					render.currentSceneLighting,
					frameRenderData,
					swapchain.extent,
					frameRenderData.frameIndex);

				TransitionImage(
					cmd,
					swapchain.images[imageIndex],
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_PIPELINE_STAGE_2_NONE,
					0,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT);

				const VkImageBlit blitRegion{
					.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.srcOffsets = {{0, 0, 0}, {static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1}},
					.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.dstOffsets = {{0, 0, 0}, {static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1}},
				};
				vkCmdBlitImage(
					cmd,
					render.postFxOutputImage,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					swapchain.images[imageIndex],
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&blitRegion,
					VK_FILTER_LINEAR);

				TransitionImage(
					cmd,
					swapchain.images[imageIndex],
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
			}
		}

		if (!postFxActive) {
			TransitionImage(
				cmd,
				render.sceneColorImage,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_COPY_BIT,
				VK_ACCESS_2_TRANSFER_READ_BIT);

			TransitionImage(
				cmd,
				swapchain.images[imageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_2_NONE,
				0,
				VK_PIPELINE_STAGE_2_COPY_BIT,
				VK_ACCESS_2_TRANSFER_WRITE_BIT);

			const VkImageBlit blitRegion{
				.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
				.srcOffsets = {{0, 0, 0}, {static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1}},
				.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
				.dstOffsets = {{0, 0, 0}, {static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1}},
			};
			vkCmdBlitImage(
				cmd,
				render.sceneColorImage,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				swapchain.images[imageIndex],
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&blitRegion,
				VK_FILTER_LINEAR);

			TransitionImage(
				cmd,
				render.sceneColorImage,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COPY_BIT,
				VK_ACCESS_2_TRANSFER_READ_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.sceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			TransitionImage(
				cmd,
				swapchain.images[imageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_COPY_BIT,
				VK_ACCESS_2_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
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
