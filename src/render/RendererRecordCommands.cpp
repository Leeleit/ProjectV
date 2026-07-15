#include "render/RendererInternal.hpp"

#include "core/Math.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/Cloudscape.hpp"
#include "render/AaPass.hpp"
#include "render/HizCulling.hpp"
#include "render/PostFx.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/SkyAtmosphere.hpp"
#include "ui/ImGuiLayer.hpp"

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

	VkBufferMemoryBarrier2 bufferBarriers[4]{};
	bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	bufferBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	bufferBarriers[0].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	bufferBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	if (render.meshShaderEnabled) {
		bufferBarriers[0].dstStageMask |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
	}
	bufferBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	bufferBarriers[0].buffer = frameRenderData.packedFaceBuffer;
	bufferBarriers[0].offset = 0;
	bufferBarriers[0].size = VK_WHOLE_SIZE;

	bufferBarriers[1] = bufferBarriers[0];
	bufferBarriers[1].dstStageMask =
		VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	bufferBarriers[1].dstAccessMask =
		VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	bufferBarriers[1].buffer = frameRenderData.opaqueIndirectBuffer;

	bufferBarriers[2] = bufferBarriers[1];
	bufferBarriers[2].buffer = frameRenderData.transparentIndirectBuffer;

	bufferBarriers[3] = bufferBarriers[0];
	bufferBarriers[3].buffer = frameRenderData.chunkDescriptorBuffer;

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
	VulkanContextState &context,
	const VkCommandBuffer cmd,
	const uint32_t imageIndex,
	const std::function<void()> &markOpaqueEnd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.graphicsMs);
	render.renderPassTimings.aaMs = 0.0f;
	render.renderPassTimings.postFxMs = 0.0f;
	PV_PROFILE_ZONE_N("RecordGraphicsCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Graphics Pass");

	const bool hzbCullingActive =
		projectv::render::IsHzbCullingEnabled() &&
		render.hizCullingEnabled &&
		render.hizMinifyEnabled &&
		frameRenderData.opaqueHzbDrawIndirectBuffer != VK_NULL_HANDLE &&
		frameRenderData.chunkDescriptorCount > 0u;
	// Pass A apply runs after meshing (below) so HZB draw buffers see this frame's indirect.

	const VkExtent2D renderExtent = render.internalRenderExtent.width > 0u &&
											render.internalRenderExtent.height > 0u
										? render.internalRenderExtent
										: swapchain.extent;
	{
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Graphics Frame");

		RecordVoxelMeshingCommands(render, frameRenderData, cmd);
		SceneFrameResources *meshFrameResources = nullptr;
		projectv::render::MeshCullPushConstants meshCullPush{};
		if (render.meshShaderEnabled && frameRenderData.chunkDescriptorCount > 0) {
			meshFrameResources = &render.sceneFrameResources[frameRenderData.frameIndex];
			projectv::render::RecordMeshShaderClusterize(
				cmd,
				&context,
				render,
				*meshFrameResources,
				frameRenderData.chunkDescriptorCount);
			meshCullPush = projectv::render::BuildMeshCullPushConstants(
				frameRenderData.chunkCullingParameters,
				render.faceClusterCapacity);
			if (!hzbCullingActive) {
				projectv::render::RecordMeshShaderPreCull(
					cmd,
					&context,
					render,
					*meshFrameResources,
					meshCullPush);
			}
		}

		if (hzbCullingActive) {
			const projectv::math::Vec3 cameraForward{
				frameRenderData.graphicsPushConstants.cameraForward.x,
				frameRenderData.graphicsPushConstants.cameraForward.y,
				frameRenderData.graphicsPushConstants.cameraForward.z,
			};
			const bool cameraCut = projectv::render::DetectHzbCameraCut(render, cameraForward);
			projectv::render::SeedHzbSlotVisibilityFromUnified(
				&context,
				render,
				frameRenderData.frameIndex);
			// Unified host mask → all FIF slots Pass-A the same completed cull (no even/odd flicker).
			const projectv::render::HzbApplyMode passAMode =
				(cameraCut || !render.hzbMaskValid)
					? projectv::render::HzbApplyMode::ForceAll
					: projectv::render::HzbApplyMode::PassA;
			const bool passAApplied = projectv::render::RecordHzbApplyVisibility(
				cmd,
				&context,
				render,
				render.sceneFrameResources[frameRenderData.frameIndex],
				frameRenderData.chunkDescriptorCount,
				passAMode);
			if (meshFrameResources != nullptr) {
				meshCullPush.visibilityMaskMode = {
					static_cast<uint32_t>(
						passAApplied ? passAMode : projectv::render::HzbApplyMode::ForceAll),
					0u,
					0u,
					0u};
				projectv::render::RecordMeshShaderPreCull(
					cmd,
					&context,
					render,
					*meshFrameResources,
					meshCullPush);
			}
		}

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
		if (render.msaaSampleCount > 1u && render.sceneColorMsImage != VK_NULL_HANDLE) {
			TransitionImage(cmd, render.sceneColorMsImage, VK_IMAGE_ASPECT_COLOR_BIT,
							render.sceneColorMsCurrentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
							VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
							VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
			render.sceneColorMsCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

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
		if (render.msaaSampleCount > 1u && render.depthResolveImage != VK_NULL_HANDLE) {
			TransitionImage(cmd, render.depthResolveImage, VK_IMAGE_ASPECT_DEPTH_BIT,
							render.depthResolveCurrentLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
							VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
							VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
							VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
			render.depthResolveCurrentLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		}

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

		const bool msaaColor =
			render.msaaSampleCount > 1u && render.sceneColorMsImageView != VK_NULL_HANDLE;
		const bool msaaDepth =
			render.msaaSampleCount > 1u && render.depthResolveImageView != VK_NULL_HANDLE;
		// HZB Pass B LOADs MS attachments — must STORE them on pass A (DONT_CARE → psychedelia).
		const bool storeMsaaForHzbPassB = hzbCullingActive && msaaColor;
		const VkRenderingAttachmentInfo colorAttachment0{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = msaaColor ? render.sceneColorMsImageView : render.sceneColorImageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = msaaColor ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
			.resolveImageView = msaaColor ? render.sceneColorImageView : VK_NULL_HANDLE,
			.resolveImageLayout = msaaColor ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = (!msaaColor || storeMsaaForHzbPassB) ? VK_ATTACHMENT_STORE_OP_STORE
															: VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachments[1] = {colorAttachment0};
		const VkRenderingAttachmentInfo depthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = render.depthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode = msaaDepth ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT : VK_RESOLVE_MODE_NONE,
			.resolveImageView = msaaDepth ? render.depthResolveImageView : VK_NULL_HANDLE,
			.resolveImageLayout = msaaDepth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = (!msaaDepth || storeMsaaForHzbPassB) ? VK_ATTACHMENT_STORE_OP_STORE
															: VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearDepthValue,
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderArea = {{0, 0}, renderExtent},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachments = colorAttachments,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = nullptr,
		};

		vkCmdBeginRendering(cmd, &renderingInfo);

		const VkViewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(renderExtent.width),
			.height = static_cast<float>(renderExtent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		const VkRect2D scissor{
			.offset = {0, 0},
			.extent = renderExtent,
		};
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

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
			const float aspectRatio = static_cast<float>(renderExtent.width) /
									  static_cast<float>(std::max(renderExtent.height, 1u));
			const float tanHalfFovY = std::max(
				frameRenderData.chunkCullingParameters.cameraForwardAndTanHalfVerticalFov.w,
				1.0e-4f);
			skyPush.viewParams = {
				render.progressiveHaltonNdcX,
				aspectRatio,
				tanHalfFovY,
				render.progressiveHaltonNdcY,
			};
			projectv::render::RecordSkyAtmosphereDraw(cmd, render, skyPush, frameRenderData.frameIndex);
		}

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
				projectv::render::RecordMeshShaderDraw(
					cmd,
					render,
					render.sceneFrameResources[frameRenderData.frameIndex],
					frameRenderData.graphicsPushConstants,
					render.faceClusterCapacity);
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
				const VkBuffer opaqueDrawBuffer =
					hzbCullingActive ? frameRenderData.opaqueHzbDrawIndirectBuffer
									 : frameRenderData.opaqueIndirectBuffer;
				vkCmdDrawIndirect(
					cmd,
					opaqueDrawBuffer,
					0,
					frameRenderData.chunkDescriptorCount,
					sizeof(VkDrawIndirectCommand));
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
			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				render.graphicsPipelineLayout,
				0u,
				1u,
				&frameRenderData.graphicsDescriptorSet,
				0u,
				nullptr);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline);
			vkCmdPushConstants(
				cmd,
				render.graphicsPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(frameRenderData.graphicsPushConstants),
				&frameRenderData.graphicsPushConstants);
			const VkBuffer transparentDrawBuffer =
				hzbCullingActive ? frameRenderData.transparentHzbDrawIndirectBuffer
								 : frameRenderData.transparentIndirectBuffer;
			vkCmdDrawIndirect(
				cmd,
				transparentDrawBuffer,
				0,
				frameRenderData.chunkDescriptorCount,
				sizeof(VkDrawIndirectCommand));
		}

		// Selection/placement wireframe boxes draw here (still inside the MSAA color attachment)
		// instead of the post-blit UI pass, so they resolve through the same AA stack as voxels.
		if (frameRenderData.debugUiVisible &&
			render.debugOverlayPipeline != VK_NULL_HANDLE &&
			!frameRenderData.debugOverlayBoxes.empty()) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Debug Overlay Boxes");
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

		vkCmdEndRendering(cmd);

		if (hzbCullingActive && render.hizBuffer.image != VK_NULL_HANDLE) {
			const bool useDepthResolve =
				render.msaaSampleCount > 1u && render.depthResolveImage != VK_NULL_HANDLE;
			const VkImageLayout hizDepthLayout =
				useDepthResolve ? render.depthResolveCurrentLayout : render.depthImageCurrentLayout;
			projectv::render::BuildHizMipChain(
				cmd,
				useDepthResolve ? render.depthResolveImage : render.depthImage,
				hizDepthLayout,
				render.hizBuffer,
				&render,
				&context);
			if (render.hizBuffer.imageView != VK_NULL_HANDLE &&
				render.hizBuffer.sampler != VK_NULL_HANDLE &&
				frameRenderData.hizCullingDescriptorSet != VK_NULL_HANDLE) {
				projectv::math::Mat4 viewProjectionMat = frameRenderData.graphicsPushConstants.viewProjection;
				std::array<float, 16> viewProjectionFlat{};
				for (uint32_t i = 0; i < 16u; ++i) {
					viewProjectionFlat[i] = viewProjectionMat.data()[i];
				}
				projectv::render::RecordHzbCullingDispatch(
					cmd,
					&context,
					render,
					render.sceneFrameResources[frameRenderData.frameIndex],
					*reinterpret_cast<const float (*)[16]>(viewProjectionFlat.data()),
					frameRenderData.chunkDescriptorCount);
				// Pass B: same-frame disocclusion (!prev && now). MS attachments STOREd above.
				const bool passBSafe = !render.hzbCameraCutThisFrame && render.hzbMaskValid;
				if (passBSafe) {
					const bool passBApplied = projectv::render::RecordHzbApplyVisibility(
						cmd,
						&context,
						render,
						render.sceneFrameResources[frameRenderData.frameIndex],
						frameRenderData.chunkDescriptorCount,
						projectv::render::HzbApplyMode::PassB);
					if (meshFrameResources != nullptr && passBApplied) {
						meshCullPush.visibilityMaskMode = {
							static_cast<uint32_t>(projectv::render::HzbApplyMode::PassB),
							0u,
							0u,
							0u};
						projectv::render::RecordMeshShaderPreCull(
							cmd,
							&context,
							render,
							*meshFrameResources,
							meshCullPush);
					}
					const bool passBMsaa =
						render.msaaSampleCount > 1u && render.sceneColorMsImageView != VK_NULL_HANDLE;
					const VkRenderingAttachmentInfo passBColor{
						.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
						.imageView = passBMsaa ? render.sceneColorMsImageView : render.sceneColorImageView,
						.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.resolveMode = passBMsaa ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
						.resolveImageView = passBMsaa ? render.sceneColorImageView : VK_NULL_HANDLE,
						.resolveImageLayout =
							passBMsaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = passBMsaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
					};
					const bool passBMsaaDepth =
						render.msaaSampleCount > 1u && render.depthResolveImageView != VK_NULL_HANDLE;
					const VkRenderingAttachmentInfo passBDepth{
						.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
						.imageView = render.depthImageView,
						.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
						.resolveMode = passBMsaaDepth ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT : VK_RESOLVE_MODE_NONE,
						.resolveImageView = passBMsaaDepth ? render.depthResolveImageView : VK_NULL_HANDLE,
						.resolveImageLayout =
							passBMsaaDepth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = passBMsaaDepth ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
					};
					const VkRenderingAttachmentInfo passBColors[1] = {passBColor};
					const VkRenderingInfo passBInfo{
						.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
						.renderArea = {{0, 0}, renderExtent},
						.layerCount = 1,
						.colorAttachmentCount = 1,
						.pColorAttachments = passBColors,
						.pDepthAttachment = &passBDepth,
					};
					vkCmdBeginRendering(cmd, &passBInfo);
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
					if (render.meshShaderEnabled && passBApplied) {
						projectv::render::RecordMeshShaderDraw(
							cmd,
							render,
							render.sceneFrameResources[frameRenderData.frameIndex],
							frameRenderData.graphicsPushConstants,
							render.faceClusterCapacity);
					} else {
						const bool rtxPathActive =
							render.rayTracedShadows != nullptr &&
							render.rayTracedShadows->IsEnabled() &&
							render.rayTracedShadows->GetConfig().tlas != VK_NULL_HANDLE;
						VkPipeline opaquePipeline = rtxPathActive ? render.graphicsPipelineRtx : VK_NULL_HANDLE;
						if (opaquePipeline == VK_NULL_HANDLE) {
							opaquePipeline = render.graphicsPipeline;
						}
						if (opaquePipeline != VK_NULL_HANDLE &&
							frameRenderData.opaqueHzbDrawIndirectBuffer != VK_NULL_HANDLE) {
							vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipeline);
							vkCmdPushConstants(
								cmd,
								render.graphicsPipelineLayout,
								VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
								0,
								sizeof(frameRenderData.graphicsPushConstants),
								&frameRenderData.graphicsPushConstants);
							vkCmdDrawIndirect(
								cmd,
								frameRenderData.opaqueHzbDrawIndirectBuffer,
								0,
								frameRenderData.chunkDescriptorCount,
								sizeof(VkDrawIndirectCommand));
						}
					}
					vkCmdEndRendering(cmd);
				}
			}
		}

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
			const float aspectRatio = static_cast<float>(renderExtent.width) /
									  static_cast<float>(std::max(renderExtent.height, 1u));
			const float tanHalfFovY = std::max(
				frameRenderData.chunkCullingParameters.cameraForwardAndTanHalfVerticalFov.w,
				1.0e-4f);
			cloudPush.viewParams = {
				render.currentSceneLighting.sunContactShadowParams[1],
				aspectRatio,
				tanHalfFovY,
				render.progressiveHaltonNdcX,
			};
			cloudPush.cloudLayerParams = {
				0.0f,
				0.0f,
				projectv::render::kDefaultCloudContrast,
				render.progressiveHaltonNdcY,
			};

			const VkImageView cloudDepthView = render.msaaSampleCount > 1u && render.depthResolveImageView != VK_NULL_HANDLE
												   ? render.depthResolveImageView
												   : render.depthImageView;
			projectv::render::RecordCloudscapeRaymarchPass(
				cmd,
				render,
				cloudPush,
				render.sceneColorImageView,
				cloudDepthView,
				renderExtent,
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

		bool postFxActive = projectv::render::IsPostFxEnabled();
		if (markOpaqueEnd) {
			markOpaqueEnd(); // GPU ts: end opaque / start AA+post
		}
		if (postFxActive) {
			ScopedPassTimer postFxTimer(render.renderPassTimings.postFxMs);
			postFxActive = projectv::render::RecordPostFxPass(
				cmd, context, render, render.currentSceneLighting, frameRenderData, renderExtent, frameRenderData.frameIndex);
		}
		VkImage hdrSourceImage = postFxActive ? render.postFxOutputImage : render.sceneColorImage;
		VkImageView hdrSourceView = postFxActive ? render.postFxOutputImageView : render.sceneColorImageView;
		VkImageLayout postFxOutputLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		VkImageLayout &hdrSourceLayout = postFxActive ? postFxOutputLayout : render.sceneColorCurrentLayout;
		{
			ScopedPassTimer aaTimer(render.renderPassTimings.aaMs);
			if (!projectv::render::RecordAaResolvePass(cmd, context, render, hdrSourceImage, hdrSourceView, hdrSourceLayout, renderExtent, frameRenderData.frameIndex)) {
				return;
			}
		}
		TransitionImage(cmd, swapchain.images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
						VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
		const VkImageBlit blitRegion{
			.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
			.srcOffsets = {{0, 0, 0}, {static_cast<int32_t>(renderExtent.width), static_cast<int32_t>(renderExtent.height), 1}},
			.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
			.dstOffsets = {{0, 0, 0}, {static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1}},
		};
		vkCmdBlitImage(cmd, render.aaPresentImage, render.aaPresentLayout, swapchain.images[imageIndex],
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_LINEAR);
		const bool needUiPass = frameRenderData.debugUiVisible;
		if (needUiPass) {
			TransitionImage(cmd, swapchain.images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
							VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

			{
				const VkRenderingAttachmentInfo uiColorAttachment{
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = swapchain.imageViews[imageIndex],
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.resolveImageView = VK_NULL_HANDLE,
					.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = {},
				};
				const VkRenderingInfo uiRenderingInfo{
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea = {{0, 0}, swapchain.extent},
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachments = &uiColorAttachment,
					.pDepthAttachment = nullptr,
					.pStencilAttachment = nullptr,
				};
				vkCmdBeginRendering(cmd, &uiRenderingInfo);
				const VkViewport uiViewport{
					.x = 0.0f,
					.y = 0.0f,
					.width = static_cast<float>(swapchain.extent.width),
					.height = static_cast<float>(swapchain.extent.height),
					.minDepth = 0.0f,
					.maxDepth = 1.0f,
				};
				const VkRect2D uiScissor{.offset = {0, 0}, .extent = swapchain.extent};
				vkCmdSetViewport(cmd, 0, 1, &uiViewport);
				vkCmdSetScissor(cmd, 0, 1, &uiScissor);
				RecordDebugOverlayCommands(render, swapchain, frameRenderData, cmd);
				projectv::ui::ImGuiRenderDrawData(cmd);
				vkCmdEndRendering(cmd);
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
		} else {
			projectv::ui::ImGuiEndFrameIfOpen(); // drain ImGui frame without GPU submit
			if (ShouldCaptureScreenshot(render)) {
				TransitionImage(cmd, swapchain.images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
								VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
				RecordSwapchainScreenshotCopy(swapchain, render, cmd, imageIndex);
			} else {
				TransitionImage(
					cmd,
					swapchain.images[imageIndex],
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_2_NONE,
					0);
			}
		}
	}

	profiling::CollectVulkanGpu(render.tracyGraphicsContext, cmd);
}
