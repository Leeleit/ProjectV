#include "render/SceneResourcesInternal.hpp"

bool UpdateSceneResources(
	WorldState *world,
	RenderState *render,
	const ChunkCullingParameters &chunkCullingParameters,
	const VkExtent2D swapchainExtent)
{
	PV_PROFILE_ZONE_N("UpdateSceneResources");
	if (!world || !render || !world->voxelWorld) {
		return false;
	}

	const uint32_t dirtyChunkCount = world->voxelWorld->stats.dirtyChunkCount;
	const uint32_t activeChunkCount = world->voxelWorld->stats.activeChunkCount;
	uint32_t rebuiltChunkCount = 0;
	uint64_t repackedVoxelCount = 0;

	if (render->sceneChunkDescriptors.size() != world->voxelWorld->chunks.size() ||
		render->sceneChunkVoxelPayloadRanges.size() != world->voxelWorld->chunks.size()) {
		render->sceneFaceCapacity = GetMaxSceneFaceCount(*world->voxelWorld);
		render->sceneTransparentFaceBase = render->sceneFaceCapacity;
		InitializeSceneChunkDescriptorsAndVoxelPayloadLayout(*world->voxelWorld, *render);
		++render->sceneUploadVersion;
		++render->sceneVoxelPayloadVersion;
	}

	render->completedChunkRebuildIndices.clear();
	render->latestVoxelPayloadChunkIndices.clear();
	for (const size_t chunkIndex : render->pendingChunkRebuildIndices) {
		if (chunkIndex >= world->voxelWorld->chunks.size()) {
			continue;
		}

		RepackChunkVoxelPayload(*world->voxelWorld, chunkIndex, *render);
		++rebuiltChunkCount;
		repackedVoxelCount += GetChunkVoxelCount(world->voxelWorld->chunks[chunkIndex]);
		render->latestVoxelPayloadChunkIndices.push_back(chunkIndex);
		render->completedChunkRebuildIndices.push_back(chunkIndex);
	}
	render->pendingChunkRebuildIndices.clear();

	if (!render->completedChunkRebuildIndices.empty()) {

		++render->sceneVoxelPayloadVersion;
	}

	const bool fluidOnlyChunkRebuilds = world->voxelWorld->editVersion == render->lastNanoVdbSyncedEditVersion;
	if ((!render->completedChunkRebuildIndices.empty() && !fluidOnlyChunkRebuilds) ||
		render->sceneNanoVdbVersion == 0u) {
		const std::array<uint8_t, 256> materialLookup = []() {
			std::array<uint8_t, 256> lookup{};
			for (uint32_t i = 0; i < 256; ++i) {
				lookup[i] = static_cast<uint8_t>(i);
			}
			return lookup;
		}();
		projectv::voxel::nanovdb::BuildNanoVdbFlatten(
			world->voxelWorld->sparseStorage,
			materialLookup.data(),
			render->sceneNanoVdbFlatten);
		++render->sceneNanoVdbVersion;
		render->lastNanoVdbSyncedEditVersion = world->voxelWorld->editVersion;
		profiling::PlotValue(
			"NanoVDB Uppers",
			static_cast<int64_t>(render->sceneNanoVdbFlatten.upperCount));
		profiling::PlotValue(
			"NanoVDB Lowers",
			static_cast<int64_t>(render->sceneNanoVdbFlatten.lowerCount));
		profiling::PlotValue(
			"NanoVDB Leaves",
			static_cast<int64_t>(render->sceneNanoVdbFlatten.leafCount));
	}

	RefreshSceneLightingBuffer(*world->voxelWorld, *render, swapchainExtent);

	profiling::PlotValue("Dirty Chunks", static_cast<int64_t>(dirtyChunkCount));
	profiling::PlotValue("Active Chunks", static_cast<int64_t>(activeChunkCount));
	profiling::PlotValue("Rebuilt Chunks", static_cast<int64_t>(rebuiltChunkCount));
	profiling::PlotValue("Repacked Chunk Voxels", static_cast<int64_t>(repackedVoxelCount));
	profiling::PlotValue(
		"Scene Exposure x100",
		static_cast<int64_t>(render->currentSceneLighting.postProcess[0] * 100.0f));
	return true;
}

bool UploadSceneFrameResources(
	VulkanContextState *context,
	RenderState &render,
	const uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("UploadSceneFrameResources");
	if (static_cast<size_t>(frameIndex) >= render.sceneFrameResources.size()) {
		return false;
	}

	SceneFrameResources &frameResources = render.sceneFrameResources[frameIndex];
	if (frameResources.sceneLightingMappedData) {
		std::memcpy(
			frameResources.sceneLightingMappedData,
			&render.currentSceneLighting,
			sizeof(render.currentSceneLighting));
	}
	UpdateGeneratedFaceStatsFromFrameResources(render, frameResources);

	uint32_t uploadedChunkDescriptorCount = 0;
	uint32_t uploadedVoxelChunkCount = 0;
	uint32_t uploadedChunkVoxelWordCount = 0;
	uint32_t meshingDirtyChunkCount = 0;
	bool descriptorBufferUploaded = false;

	if (frameResources.uploadedSceneVersion != render.sceneUploadVersion) {
		if (!render.sceneChunkDescriptors.empty() && frameResources.chunkDescriptorMappedData) {
			auto *uploadedChunkDescriptors =
				static_cast<PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
			const bool preserveGeneratedFaceCounts =
				frameResources.chunkDescriptorCount == render.sceneChunkDescriptors.size();
			for (size_t chunkIndex = 0; chunkIndex < render.sceneChunkDescriptors.size(); ++chunkIndex) {
				const PackedSceneChunkDescriptor *existingDescriptor =
					preserveGeneratedFaceCounts ? &uploadedChunkDescriptors[chunkIndex] : nullptr;
				uploadedChunkDescriptors[chunkIndex] = MakeUploadedSceneChunkDescriptor(
					render.sceneChunkDescriptors[chunkIndex],
					existingDescriptor);
			}
			uploadedChunkDescriptorCount = static_cast<uint32_t>(render.sceneChunkDescriptors.size());
			descriptorBufferUploaded = true;
		}
		frameResources.uploadedSceneVersion = render.sceneUploadVersion;
	}

	if (frameResources.uploadedVoxelPayloadVersion != render.sceneVoxelPayloadVersion) {
		const bool canPatchLatestDirtyChunks =
			frameResources.uploadedVoxelPayloadVersion + 1u == render.sceneVoxelPayloadVersion &&
			!render.latestVoxelPayloadChunkIndices.empty();

		if (canPatchLatestDirtyChunks) {
			for (const size_t chunkIndex : render.latestVoxelPayloadChunkIndices) {
				if (chunkIndex >= render.sceneChunkVoxelPayloadRanges.size()) {
					continue;
				}

				const SceneChunkVoxelPayloadRange &payloadRange = render.sceneChunkVoxelPayloadRanges[chunkIndex];
				if (payloadRange.wordOffset + payloadRange.wordCount > render.sceneChunkVoxelPayloadWords.size()) {
					continue;
				}

				if (payloadRange.wordCount > 0) {
					std::memcpy(
						static_cast<uint32_t *>(frameResources.chunkVoxelPayloadMappedData) + payloadRange.wordOffset,
						render.sceneChunkVoxelPayloadWords.data() + payloadRange.wordOffset,
						static_cast<size_t>(payloadRange.wordCount) * sizeof(uint32_t));
					uploadedChunkVoxelWordCount += payloadRange.wordCount;
				}

				if (!descriptorBufferUploaded && frameResources.chunkDescriptorMappedData) {
					auto *uploadedChunkDescriptors =
						static_cast<PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
					uploadedChunkDescriptors[chunkIndex] = MakeUploadedSceneChunkDescriptor(
						render.sceneChunkDescriptors[chunkIndex],
						&uploadedChunkDescriptors[chunkIndex]);
				}

				++uploadedVoxelChunkCount;
			}
		} else {
			if (!render.sceneChunkVoxelPayloadWords.empty()) {
				std::memcpy(
					frameResources.chunkVoxelPayloadMappedData,
					render.sceneChunkVoxelPayloadWords.data(),
					render.sceneChunkVoxelPayloadWords.size() * sizeof(uint32_t));
				uploadedChunkVoxelWordCount = render.sceneChunkVoxelPayloadWordCount;
			}

			if (!descriptorBufferUploaded &&
				!render.sceneChunkDescriptors.empty() &&
				frameResources.chunkDescriptorMappedData) {
				auto *uploadedChunkDescriptors =
					static_cast<PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
				const bool preserveGeneratedFaceCounts =
					frameResources.chunkDescriptorCount == render.sceneChunkDescriptors.size();
				for (size_t chunkIndex = 0; chunkIndex < render.sceneChunkDescriptors.size(); ++chunkIndex) {
					const PackedSceneChunkDescriptor *existingDescriptor =
						preserveGeneratedFaceCounts ? &uploadedChunkDescriptors[chunkIndex] : nullptr;
					uploadedChunkDescriptors[chunkIndex] = MakeUploadedSceneChunkDescriptor(
						render.sceneChunkDescriptors[chunkIndex],
						existingDescriptor);
				}
			}

			uploadedVoxelChunkCount = static_cast<uint32_t>(render.sceneChunkVoxelPayloadRanges.size());
		}

		// EVIL: missing vmaFlushAllocation caused GPU to read stale chunkVoxelPayload / chunkDescriptor data on non-coherent drivers → DDA in rint saw old voxel occupancy → sliced shadows / leaks through visible blocks, intermittent on edits (P1A-4). Flush both allocations after every payload upload.
		if (context != nullptr && context->allocator != nullptr) {
			if (frameResources.chunkVoxelPayloadAllocation != nullptr && uploadedChunkVoxelWordCount > 0u) {
				vmaFlushAllocation(context->allocator, frameResources.chunkVoxelPayloadAllocation, 0u, VK_WHOLE_SIZE);
			}
			if (frameResources.chunkDescriptorAllocation != nullptr) {
				vmaFlushAllocation(context->allocator, frameResources.chunkDescriptorAllocation, 0u, VK_WHOLE_SIZE);
			}
		}

		frameResources.uploadedVoxelPayloadVersion = render.sceneVoxelPayloadVersion;
	}

	if (frameResources.uploadedNanoVdbVersion != render.sceneNanoVdbVersion) {
		const VkDeviceSize upperRequired =
			static_cast<VkDeviceSize>(render.sceneNanoVdbFlatten.uppers.size()) *
				sizeof(projectv::voxel::nanovdb::NanoVdbUpper);
		const VkDeviceSize lowerRequired =
			static_cast<VkDeviceSize>(render.sceneNanoVdbFlatten.lowers.size()) *
				sizeof(projectv::voxel::nanovdb::NanoVdbLower);
		const VkDeviceSize leafRequired =
			static_cast<VkDeviceSize>(render.sceneNanoVdbFlatten.leaves.size()) *
				sizeof(projectv::voxel::nanovdb::NanoVdbLeaf);
		const VkDeviceSize materialRequired =
			static_cast<VkDeviceSize>(render.sceneNanoVdbFlatten.materials.size()) *
				sizeof(uint8_t);
		const bool flattenedWithinCapacity =
			frameResources.nanovdbUpperCapacityBytes >= upperRequired &&
			frameResources.nanovdbLowerCapacityBytes >= lowerRequired &&
			frameResources.nanovdbLeafCapacityBytes >= leafRequired &&
			frameResources.nanovdbMaterialCapacityBytes >= materialRequired;
		if (flattenedWithinCapacity) {
			(void)RefreshNanoVdbFlattenBuffers(render.sceneNanoVdbFlatten, frameResources);
		} else {
			const bool grewUpper = upperRequired > frameResources.nanovdbUpperCapacityBytes
				? GrowNanoVdbBuffer(
					context,
					render,
					frameIndex,
					frameResources.nanovdbUpperBuffer,
					frameResources.nanovdbUpperAllocation,
					frameResources.nanovdbUpperMappedData,
					frameResources.nanovdbUpperCapacityBytes,
					ComputeGrownNanoVdbCapacity(frameResources.nanovdbUpperCapacityBytes, upperRequired),
					"SceneNanoVdbUpperBufferAllocation")
				: true;
			const bool grewLower = lowerRequired > frameResources.nanovdbLowerCapacityBytes
				? GrowNanoVdbBuffer(
					context,
					render,
					frameIndex,
					frameResources.nanovdbLowerBuffer,
					frameResources.nanovdbLowerAllocation,
					frameResources.nanovdbLowerMappedData,
					frameResources.nanovdbLowerCapacityBytes,
					ComputeGrownNanoVdbCapacity(frameResources.nanovdbLowerCapacityBytes, lowerRequired),
					"SceneNanoVdbLowerBufferAllocation")
				: true;
			const bool grewLeaf = leafRequired > frameResources.nanovdbLeafCapacityBytes
				? GrowNanoVdbBuffer(
					context,
					render,
					frameIndex,
					frameResources.nanovdbLeafBuffer,
					frameResources.nanovdbLeafAllocation,
					frameResources.nanovdbLeafMappedData,
					frameResources.nanovdbLeafCapacityBytes,
					ComputeGrownNanoVdbCapacity(frameResources.nanovdbLeafCapacityBytes, leafRequired),
					"SceneNanoVdbLeafBufferAllocation")
				: true;
			const bool grewMaterial = materialRequired > frameResources.nanovdbMaterialCapacityBytes
				? GrowNanoVdbBuffer(
					context,
					render,
					frameIndex,
					frameResources.nanovdbMaterialBuffer,
					frameResources.nanovdbMaterialAllocation,
					frameResources.nanovdbMaterialMappedData,
					frameResources.nanovdbMaterialCapacityBytes,
					ComputeGrownNanoVdbCapacity(frameResources.nanovdbMaterialCapacityBytes, materialRequired),
					"SceneNanoVdbMaterialBufferAllocation")
				: true;
			if (grewUpper && grewLower && grewLeaf && grewMaterial) {
				(void)RefreshNanoVdbFlattenBuffers(render.sceneNanoVdbFlatten, frameResources);
			} else {
				runtime::LogRuntimeFailure(
					"UploadSceneFrameResources",
					"NanoVdbFlatten",
					"GrowAndRefreshFailed");
			}
		}
		frameResources.uploadedNanoVdbVersion = render.sceneNanoVdbVersion;
	}

	frameResources.chunkDescriptorCount = static_cast<uint32_t>(render.sceneChunkDescriptors.size());
	frameResources.dirtyChunkCount = PrepareDirtyChunkMeshingList(render, frameResources);
	frameResources.opaqueFaceCount = render.sceneOpaqueFaceCount;
	frameResources.transparentFaceCount = render.sceneTransparentFaceCount;
	meshingDirtyChunkCount = frameResources.dirtyChunkCount;

	profiling::PlotValue("Scene Triangles", static_cast<int64_t>(render.sceneTriangleCount));
	profiling::PlotValue("Generated Opaque Faces", static_cast<int64_t>(render.sceneOpaqueFaceCount));
	profiling::PlotValue("Generated Transparent Faces", static_cast<int64_t>(render.sceneTransparentFaceCount));
	profiling::PlotValue("Meshing Dirty Chunks", static_cast<int64_t>(meshingDirtyChunkCount));
	profiling::PlotValue("Uploaded Chunk Descriptors", static_cast<int64_t>(uploadedChunkDescriptorCount));
	profiling::PlotValue("Uploaded Voxel Payload Chunks", static_cast<int64_t>(uploadedVoxelChunkCount));
	profiling::PlotValue("Chunk Voxel Words", static_cast<int64_t>(render.sceneChunkVoxelPayloadWordCount));
	profiling::PlotValue("Uploaded Chunk Voxel Words", static_cast<int64_t>(uploadedChunkVoxelWordCount));
	profiling::PlotValue(
		"Upload Descriptor Bytes",
		static_cast<int64_t>(uploadedChunkDescriptorCount * sizeof(PackedSceneChunkDescriptor)));
	profiling::PlotValue(
		"Upload Chunk Voxel Bytes",
		static_cast<int64_t>(uploadedChunkVoxelWordCount * sizeof(uint32_t)));
	return true;
}

bool RefreshChunkAabbBuffer(
	std::span<const VoxelChunk> chunks,
	std::span<const PackedSceneChunkDescriptor> descriptors,
	SceneFrameResources &frameResources)
{
	(void)descriptors;
	if (!frameResources.chunkAabbMappedData) {
		return false;
	}
	const size_t chunkCount = std::min(chunks.size(), descriptors.size());
	auto *packedAabbs = static_cast<PackedSceneChunkAabb *>(frameResources.chunkAabbMappedData);
	for (size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
		const VoxelChunk &chunk = chunks[chunkIndex];
		const float minX = static_cast<float>(chunk.min.x);
		const float minY = static_cast<float>(chunk.min.y);
		const float minZ = static_cast<float>(chunk.min.z);
		const float maxX = static_cast<float>(chunk.maxExclusive.x);
		const float maxY = static_cast<float>(chunk.maxExclusive.y);
		const float maxZ = static_cast<float>(chunk.maxExclusive.z);
		const float centerX = (minX + maxX) * 0.5f;
		const float centerY = (minY + maxY) * 0.5f;
		const float centerZ = (minZ + maxZ) * 0.5f;
		const float halfExtent =
			std::max({maxX - minX, maxY - minY, maxZ - minZ}) * 0.5f;
		packedAabbs[chunkIndex].centerAndHalfExtent = {
			centerX,
			centerY,
			centerZ,
			halfExtent,
		};
		packedAabbs[chunkIndex].originAndPadding = {
			minX,
			minY,
			minZ,
			0.0f,
		};
	}
	for (size_t chunkIndex = chunkCount;
		chunkIndex < static_cast<size_t>(frameResources.chunkDescriptorCount);
		++chunkIndex) {
		packedAabbs[chunkIndex] = {};
	}
	if (frameResources.visibilityMaskMappedData) {
		const uint32_t wordCount = (static_cast<uint32_t>(chunkCount) + 31u) / 32u;
		std::memset(frameResources.visibilityMaskMappedData, 0, sizeof(uint32_t) * wordCount);
	}
	return true;
}


bool RefreshNanoVdbFlattenBuffers(
	const projectv::voxel::nanovdb::NanoVdbFlattenResult &flatten,
	SceneFrameResources &frameResources)
{
	if (!flatten.uppers.empty() && !frameResources.nanovdbUpperMappedData) {
		return false;
	}
	if (!flatten.lowers.empty() && !frameResources.nanovdbLowerMappedData) {
		return false;
	}
	if (!flatten.leaves.empty() && !frameResources.nanovdbLeafMappedData) {
		return false;
	}
	if (!flatten.materials.empty() && !frameResources.nanovdbMaterialMappedData) {
		return false;
	}

	projectv::voxel::nanovdb::PackNanoVdbFlattenData(
		flatten,
		frameResources.nanovdbUpperMappedData,
		frameResources.nanovdbLowerMappedData,
		frameResources.nanovdbLeafMappedData,
		frameResources.nanovdbMaterialMappedData);
	return true;
}

uint64_t ComputeGrownNanoVdbCapacity(const uint64_t currentCapacityBytes, const uint64_t requiredCapacityBytes)
{
	if (currentCapacityBytes == 0u) {
		return std::max<uint64_t>(requiredCapacityBytes, 1u);
	}
	if (requiredCapacityBytes <= currentCapacityBytes) {
		return currentCapacityBytes;
	}
	const uint64_t grown = currentCapacityBytes + currentCapacityBytes / 2u;
	return std::max<uint64_t>(grown, requiredCapacityBytes);
}

bool GrowNanoVdbBuffer(
	VulkanContextState *context,
	RenderState &render,
	uint32_t currentFrameIndex,
	VkBuffer &buffer,
	VmaAllocation &allocation,
	void *&mappedData,
	uint64_t &capacityBytes,
	const uint64_t newCapacityBytes,
	const char *profilingTag)
{
	if (context == nullptr || context->device == VK_NULL_HANDLE || context->allocator == VK_NULL_HANDLE) {
		return false;
	}
	if (newCapacityBytes == 0u) {
		return false;
	}
	if (buffer != VK_NULL_HANDLE && allocation != nullptr) {
		profiling::RecordFree(allocation, profilingTag);
		EnqueueDeferredNanoVdbDestroy(render, currentFrameIndex, buffer, allocation);
		buffer = VK_NULL_HANDLE;
		allocation = nullptr;
		mappedData = nullptr;
		capacityBytes = 0u;
	}
	VmaAllocationCreateInfo allocationCreateInfo{};
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = newCapacityBytes;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	const VkResult createResult = vmaCreateBuffer(
		context->allocator,
		&bufferCreateInfo,
		&allocationCreateInfo,
		&buffer,
		&allocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("GrowNanoVdbBuffer.vmaCreateBuffer", createResult);
		buffer = VK_NULL_HANDLE;
		allocation = nullptr;
		return false;
	}
	VmaAllocationInfo allocInfo{};
	vmaGetAllocationInfo(context->allocator, allocation, &allocInfo);
	mappedData = allocInfo.pMappedData;
	capacityBytes = allocInfo.size;
	profiling::RecordAllocation(allocation, allocInfo.size, profilingTag);
	return true;
}

