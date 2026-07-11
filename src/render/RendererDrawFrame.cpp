#include "render/RendererInternal.hpp"

SDL_AppResult DrawFrame(
	AppState *state,
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

	DrainDeferredNanoVdbDestroysForFrame(context, *render, currentFrameIndex);

	const VkCommandBuffer cmd = frame->commandBuffers[currentFrameIndex];
	const VkFence inFlightFence = frame->inFlightFences[currentFrameIndex];
	const VkSemaphore imageAvailableSemaphore = frame->imageAvailableSemaphores[currentFrameIndex];
	const VkExtent2D captureExtent = swapchain->extent;
	const VkFormat captureFormat = swapchain->format;

	uint32_t imageIndex = 0;

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

	const VkResult waitFencesResult = vkWaitForFences(context->device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
	if (waitFencesResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkWaitForFences", waitFencesResult);
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

	if (render->meshShaderEnabled && frame->renderData.chunkDescriptorCount > 0) {
		const projectv::render::MeshCullPushConstants cullPush =
			projectv::render::BuildMeshCullPushConstants(
				frame->renderData.chunkCullingParameters,
				frame->renderData.chunkDescriptorCount);
		projectv::render::RecordMeshShaderPreCull(
			cmd,
			context,
			*render,
			render->sceneFrameResources[frame->currentFrame],
			cullPush);
	}

	if (render->rayTracedShadows != nullptr) {
		PV_PROFILE_ZONE_N("CollectAndBuildBlasChunks");
		if (state->world().voxelWorld != nullptr) {
			const VoxelWorld &world = *state->world().voxelWorld;
			const auto &rtxConfig = render->rayTracedShadows->GetConfig();

			// Combine dirty (pendingBlasRebuildIndices) + initial (non-empty without BLAS).
			// The initial path covers scene-load chunks that never received a voxel edit
			// and therefore never landed in pendingBlasRebuildIndices.
			std::vector<uint32_t> blasChunks{};
			CollectDirtyVoxelChunkBlasRebuildRequests(
				*state->world().voxelWorld,
				&blasChunks);
			std::vector<uint32_t> initialChunks{};
			projectv::render::CollectNonBuiltBlasChunksForRayTracing(
				world,
				rtxConfig.blasDeviceAddresses,
				&initialChunks);
			for (const uint32_t index : initialChunks) {
				if (std::ranges::find(blasChunks, index) == blasChunks.end()) {
					blasChunks.push_back(index);
				}
			}

			if (!blasChunks.empty()) {
				std::vector<projectv::render::DirtyChunkRebuild> dirtyRebuilds{};
				dirtyRebuilds.reserve(blasChunks.size());
				for (const uint32_t chunkIndex : blasChunks) {
					if (chunkIndex >= world.chunks.size()) {
						continue;
					}
					const VoxelChunk &chunk = world.chunks[chunkIndex];
					projectv::render::DirtyChunkRebuild entry{};
					entry.chunkIndex = chunkIndex;
					entry.aabb.minX = static_cast<float>(chunk.min.x);
					entry.aabb.minY = static_cast<float>(chunk.min.y);
					entry.aabb.minZ = static_cast<float>(chunk.min.z);
					entry.aabb.maxX = static_cast<float>(chunk.maxExclusive.x);
					entry.aabb.maxY = static_cast<float>(chunk.maxExclusive.y);
					entry.aabb.maxZ = static_cast<float>(chunk.maxExclusive.z);
					dirtyRebuilds.push_back(entry);
				}
				if (!dirtyRebuilds.empty()) {
					render->rayTracedShadows->SetBlasDirtyQueue(std::move(dirtyRebuilds));
				}
			}
		}
		render->rayTracedShadows->BuildDirtyBlases(*context, context->commandPool);

		// EVIL: visibleChunkIndices/transforms assembled from non-empty chunks whose BLAS
		// is already built (blasDeviceAddresses[i] != 0). For each frame this drives TLAS
		// population; ray query in voxel.frag.rtx.spv reads the resulting instance list.
		// Per Stage 5.2.A DoD: smoke log must show tlasInstanceCount > 0 (now wired).
		if (state->world().voxelWorld != nullptr) {
			std::vector<uint32_t> visibleChunkIndices{};
			std::vector<VkTransformMatrixKHR> visibleChunkTransforms{};
			const VoxelWorld &world = *state->world().voxelWorld;
			const auto &rtxConfig = render->rayTracedShadows->GetConfig();
			VkTransformMatrixKHR identityMatrix{};
			identityMatrix.matrix[0][0] = 1.0f;
			identityMatrix.matrix[1][1] = 1.0f;
			identityMatrix.matrix[2][2] = 1.0f;
			visibleChunkIndices.reserve(world.chunks.size());
			visibleChunkTransforms.reserve(world.chunks.size());
			for (size_t i = 0; i < world.chunks.size(); ++i) {
				const VoxelChunk &chunk = world.chunks[i];
				if (chunk.nonAirVoxelCount == 0u) {
					continue;
				}
				if (i >= rtxConfig.blasDeviceAddresses.size() || rtxConfig.blasDeviceAddresses[i] == 0u) {
					continue;
				}
				visibleChunkIndices.push_back(static_cast<uint32_t>(i));
				visibleChunkTransforms.push_back(identityMatrix);
			}
			if (!visibleChunkIndices.empty()) {
				render->rayTracedShadows->UpdateTlas(
					*context,
					visibleChunkIndices,
					visibleChunkTransforms);
			}
		}
	}

	if (render->rayTracedShadows != nullptr) {
		if (render->rayTracedShadows->IsEnabled()) {
			PV_PROFILE_GPU_ZONE(render->tracyGraphicsContext, cmd, "TLAS Build");
			render->rayTracedShadows->RecordTlasBuild(cmd, *context); // EVIL: build TLAS on current frame before RT shadow trace — kills 1-frame latency / shimmer (P1A-3b).
		}
		PV_PROFILE_ZONE_N("RecordVoxelAwareRtxShadowPass");
		const SceneFrameResources &shadowFrameResources = render->sceneFrameResources[frame->currentFrame];
		projectv::math::Mat4 inverseViewProjection =
			projectv::math::inverse(frame->renderData.graphicsPushConstants.viewProjection);
		std::array<float, 16> inverseViewProjectionFlat{};
		for (uint32_t i = 0; i < 16u; ++i) {
			inverseViewProjectionFlat[i] = inverseViewProjection.data()[i];
		}
		const float cameraPosition[3] = {
			frame->renderData.graphicsPushConstants.cameraPosition.x,
			frame->renderData.graphicsPushConstants.cameraPosition.y,
			frame->renderData.graphicsPushConstants.cameraPosition.z};
		const float cameraForward[3] = {
			frame->renderData.graphicsPushConstants.cameraForward.x,
			frame->renderData.graphicsPushConstants.cameraForward.y,
			frame->renderData.graphicsPushConstants.cameraForward.z};
		render->rayTracedShadows->RecordVoxelAwareRtxShadowPass(
			cmd,
			*context,
			frame->currentFrame,
			frame->renderData.chunkDescriptorBuffer,
			shadowFrameResources.sceneLightingBuffer,
			frame->renderData.chunkVoxelPayloadBuffer,
			inverseViewProjectionFlat.data(),
			cameraPosition,
			cameraForward,
			swapchain->extent.width,
			swapchain->extent.height);
	}

	if (render->rtxGiProbes != nullptr && render->rtxGiProbes->IsEnabled()) {
		VkAccelerationStructureKHR tlas = render->rayTracedShadows != nullptr ? render->rayTracedShadows->GetTlas() : VK_NULL_HANDLE;
		if (tlas != VK_NULL_HANDLE) {
			PV_PROFILE_ZONE_N("RecordRtxGiProbeUpdatePass");
			const SceneFrameResources &frameResources = render->sceneFrameResources[frame->currentFrame];
			projectv::render::RecordRtxGiProbeUpdatePass(
				cmd,
				render->rtxGiProbes,
				*context,
				frame->currentFrame,
				frame->renderData.chunkDescriptorBuffer,
				frameResources.sceneLightingBuffer,
				frame->renderData.chunkVoxelPayloadBuffer,
				render->materialVisualBuffer,
				tlas,
				frame->renderData);
		}
	}

	RecordGraphicsCommands(*render, *swapchain, frame->renderData, *context, cmd, imageIndex);

	if (render->rayTracedShadows != nullptr) {
		render->rayTracedShadows->RecordDebugReport();
	}

	const bool asyncComputeHzbPathActive =
		projectv::render::IsAsyncComputeEnabled() &&
		projectv::render::IsAsyncComputeResourcesAllocated(*context) &&
		projectv::render::IsHzbCullingEnabled() &&
		render->hizBuffer.image != VK_NULL_HANDLE &&
		frame->renderData.hizCullingDescriptorSet != VK_NULL_HANDLE;

	if (projectv::render::IsHzbCullingEnabled() &&
		render->hizBuffer.image != VK_NULL_HANDLE) {
		projectv::render::BuildHizMipChain(
			cmd,
			render->depthImage,
			render->depthImageCurrentLayout,
			render->hizBuffer);
		if (render->hizBuffer.imageView != VK_NULL_HANDLE &&
			render->hizBuffer.sampler != VK_NULL_HANDLE &&
			frame->renderData.hizCullingDescriptorSet != VK_NULL_HANDLE &&
			!asyncComputeHzbPathActive) {
			projectv::math::Mat4 inverseViewProjection =
				projectv::math::inverse(frame->renderData.graphicsPushConstants.viewProjection);
			std::array<float, 16> inverseViewProjectionFlat{};
			for (uint32_t i = 0; i < 16u; ++i) {
				inverseViewProjectionFlat[i] = inverseViewProjection.data()[i];
			}
			projectv::render::RecordHzbCullingDispatch(
				cmd,
				context,
				*render,
				render->sceneFrameResources[frame->currentFrame],
				*reinterpret_cast<const float (*)[16]>(inverseViewProjectionFlat.data()),
				frame->renderData.chunkDescriptorCount);
		}
	}

	const bool asyncComputePathActive =
		asyncComputeHzbPathActive ||
		(projectv::render::IsAsyncComputeEnabled() &&
		 projectv::render::IsAsyncComputeResourcesAllocated(*context) &&
		 (render->fluidCaPipelineEnabled || render->worldGenPipelineEnabled));

	if (!asyncComputePathActive && render->fluidCaPipelineEnabled && state->simulation().fluidGpuTicksPending > 0u) {
		PV_PROFILE_ZONE_N("RecordFluidCaCommands");
		VoxelWorld *voxelWorld = state->world().voxelWorld.get();
		if (voxelWorld != nullptr) {
			const std::vector<uint32_t> activeChunkIds = BuildActiveChunkIdsForFluidCa(*voxelWorld);
			SceneFrameResources &frameResources = render->sceneFrameResources[frame->currentFrame];
			if (frameResources.fluidCaActiveChunkIdMappedData != nullptr && !activeChunkIds.empty()) {
				std::memcpy(
					frameResources.fluidCaActiveChunkIdMappedData,
					activeChunkIds.data(),
					activeChunkIds.size() * sizeof(uint32_t));
			}
			projectv::render::FluidCaPushConstants fluidCaPush{};
			fluidCaPush.chunkDimensions = {
				static_cast<uint32_t>(voxelWorld->chunkSize),
				static_cast<uint32_t>(voxelWorld->chunkSize),
				static_cast<uint32_t>(voxelWorld->chunkSize),
				0u,
			};
			fluidCaPush.chunkCountAndFlags = {
				static_cast<uint32_t>(activeChunkIds.size()),
				0u,
				0u,
				0u,
			};
			fluidCaPush.fluidTickInterval = 1.0f / std::max(state->simulation().fluidTickRateHz, 1.0f);
			for (uint32_t tickIndex = 0; tickIndex < state->simulation().fluidGpuTicksPending; ++tickIndex) {
				projectv::render::RecordFluidCaDispatch(
					cmd,
					*render,
					frameResources,
					fluidCaPush,
					static_cast<uint32_t>(activeChunkIds.size()));
			}
			state->simulation().fluidGpuTicksPending = 0u;
		}
	}

	if (!asyncComputePathActive && render->worldGenPipelineEnabled && state->world().voxelWorld != nullptr) {
		PV_PROFILE_ZONE_N("RecordWorldGenCommands");
		VoxelWorld *voxelWorld = state->world().voxelWorld.get();
		std::vector<uint32_t> activeWorldGenChunkIds;
		const uint32_t worldGenChunkCount = projectv::render::BuildActiveChunkIdsForWorldGen(
			*voxelWorld,
			activeWorldGenChunkIds);
		SceneFrameResources &worldGenFrameResources = render->sceneFrameResources[frame->currentFrame];
		if (worldGenChunkCount > 0u && worldGenFrameResources.worldGenVoxelBuffer != VK_NULL_HANDLE) {
			if (worldGenFrameResources.worldGenVoxelMappedData != nullptr) {
				std::memset(
					worldGenFrameResources.worldGenVoxelMappedData,
					0,
					static_cast<size_t>(worldGenChunkCount) *
						static_cast<size_t>(projectv::render::kWorldGenVoxelBufferBytesPerChunk));
			}
			projectv::render::WorldGenPushConstants worldGenPush{};
			worldGenPush.chunkOriginAndChunkSize = {
				0,
				0,
				0,
				voxelWorld->chunkSize,
			};
			worldGenPush.chunkCountAndFlags = {
				worldGenChunkCount,
				0u,
				0u,
				0u,
			};
			worldGenPush.noiseParams = {
				0.5f,
				0.5f,
				4u,
				2.0f,
			};
			worldGenPush.seed = static_cast<uint32_t>(state->simulation().simulationTick);
			projectv::render::RecordWorldGenDispatch(
				cmd,
				*render,
				worldGenFrameResources,
				worldGenPush,
				worldGenChunkCount);
		}
	}

	const VkResult endCommandBufferResult = vkEndCommandBuffer(cmd);
	if (endCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkEndCommandBuffer", endCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	if (asyncComputePathActive) {
		PV_PROFILE_ZONE_N("AsyncCompute.Submit");
		if (projectv::render::RecordAsyncComputePass(
				context->asyncComputeCommandBuffer,
				*context,
				*render,
				state,
				frame)) {
			uint64_t newTimelineValue = 0u;
			if (projectv::render::SubmitToComputeQueue(context, context->asyncComputeCommandBuffer, &newTimelineValue)) {
				context->asyncComputeLastTimelineValue = newTimelineValue;
			}
		}
	}

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

	waitSemaphoreInfo.semaphore = imageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo computeWaitSemaphoreInfo{};
	std::array<VkSemaphoreSubmitInfo, 2> allWaitSemaphoreInfos{};
	uint32_t waitSemaphoreInfoCount = 1u;
	allWaitSemaphoreInfos[0] = waitSemaphoreInfo;
	if (asyncComputePathActive && context->asyncComputeLastTimelineValue > 0u) {
		computeWaitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		computeWaitSemaphoreInfo.semaphore = context->renderTimelineSemaphore;
		computeWaitSemaphoreInfo.value = context->asyncComputeLastTimelineValue;
		computeWaitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		allWaitSemaphoreInfos[1] = computeWaitSemaphoreInfo;
		waitSemaphoreInfoCount = 2u;
	}

	const VkSemaphore submitSemaphore = swapchain->submitSemaphores[imageIndex];
	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = submitSemaphore;

	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	std::array<VkSemaphoreSubmitInfo, 2> allSignalSemaphoreInfos{};
	allSignalSemaphoreInfos[0] = signalSemaphoreInfo;
	uint32_t signalSemaphoreInfoCount = 1u;

	VkSemaphoreSubmitInfo hzbSignalSemaphoreInfo{};
	if (asyncComputeHzbPathActive && context->hzbBuildTimelineSemaphore != VK_NULL_HANDLE) {
		context->hzbBuildLastTimelineValue += 1u;
		hzbSignalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		hzbSignalSemaphoreInfo.semaphore = context->hzbBuildTimelineSemaphore;
		hzbSignalSemaphoreInfo.value = context->hzbBuildLastTimelineValue;
		hzbSignalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		allSignalSemaphoreInfos[1] = hzbSignalSemaphoreInfo;
		signalSemaphoreInfoCount = 2u;
	}

	VkCommandBufferSubmitInfo cmdBufferInfo{};
	cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdBufferInfo.commandBuffer = cmd;

	VkSubmitInfo2 submitInfo2{};
	submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo2.waitSemaphoreInfoCount = waitSemaphoreInfoCount;
	submitInfo2.pWaitSemaphoreInfos = allWaitSemaphoreInfos.data();
	submitInfo2.commandBufferInfoCount = 1;
	submitInfo2.pCommandBufferInfos = &cmdBufferInfo;
	submitInfo2.signalSemaphoreInfoCount = signalSemaphoreInfoCount;
	submitInfo2.pSignalSemaphoreInfos = allSignalSemaphoreInfos.data();
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
