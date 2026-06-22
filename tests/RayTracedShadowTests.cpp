// Copyright (c) 2026 ProjectV
// SPDX-License-Identifier: MIT
// Stage 5.2 RTX foundation sub-tests (no Vulkan device required; pure CPU contracts).

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "core/RuntimeDiagnostics.hpp"
#include "render/RayTracedShadows.hpp"

namespace {

int gFailureCount = 0;

#define PROJECTV_RTX_EXPECT(cond, message)                                                              \
	do {                                                                                                  \
		if (!(cond)) {                                                                                    \
			runtime::LogRuntimeFailure("Tests", "ProjectVRayTracedShadowTests", message);               \
			++gFailureCount;                                                                              \
		}                                                                                                 \
	} while (false)

void TestEnvGateDefaultsOff()
{
	VulkanContextState context{};
	PROJECTV_RTX_EXPECT(
		!projectv::render::IsRayTracedShadowEnabled(context),
		"IsRayTracedShadowEnabled must be false on default-constructed context (no RT capability)");
}

void TestEnvGateOnRespected()
{
	VulkanContextState context{};
	context.rayTracing.accelerationStructure = true;
	context.rayTracing.rayQuery = true;
	PROJECTV_RTX_EXPECT(
		projectv::render::IsRayTracedShadowEnabled(context),
		"IsRayTracedShadowEnabled must be true when context.rayTracing.accelerationStructure && rayQuery");
	context.rayTracing.accelerationStructure = false;
	context.rayTracing.rayQuery = true;
	PROJECTV_RTX_EXPECT(
		!projectv::render::IsRayTracedShadowEnabled(context),
		"IsRayTracedShadowEnabled must be false when only rayQuery available (need both)");
	context.rayTracing.accelerationStructure = true;
	context.rayTracing.rayQuery = false;
	PROJECTV_RTX_EXPECT(
		!projectv::render::IsRayTracedShadowEnabled(context),
		"IsRayTracedShadowEnabled must be false when only accelerationStructure available (need both)");
	context = VulkanContextState{};
}

void TestConfigDefaultValues()
{
	projectv::render::RayTracedShadowConfig config{};
	PROJECTV_RTX_EXPECT(config.tlas == VK_NULL_HANDLE, "default tlas must be null");
	PROJECTV_RTX_EXPECT(config.tlasInstanceBuffer == VK_NULL_HANDLE, "default instance buffer must be null");
	PROJECTV_RTX_EXPECT(config.scratchBuffer == VK_NULL_HANDLE, "default scratch buffer must be null");
	PROJECTV_RTX_EXPECT(!config.enabled, "default enabled must be false");
	PROJECTV_RTX_EXPECT(!config.featureDetectionResult, "default feature detection result must be false");
	PROJECTV_RTX_EXPECT(config.tlasInstanceCapacityBytes == 0u, "default capacity must be zero");
	PROJECTV_RTX_EXPECT(config.minScratchAlignment == 1u, "default scratch alignment must be 1");
	PROJECTV_RTX_EXPECT(config.tlasInstanceCount == 0u, "default instance count must be zero");
}

void TestConfigZeroSizedAfterShutdown()
{
	projectv::render::RayTracedShadows shadows{};
	PROJECTV_RTX_EXPECT(!shadows.IsEnabled(), "default-constructed shadows must be disabled");
	PROJECTV_RTX_EXPECT(shadows.GetConfig().tlasInstanceCount == 0u, "default config must have zero instance count");
}

void TestComputeBlasBuildScratchSize()
{
	projectv::render::RayTracedShadows shadows{};
	PROJECTV_RTX_EXPECT(shadows.ComputeBlasBuildScratchSize(0u) == 0u, "scratch size for zero primitives must be zero");
	const VkDeviceSize nonZero = shadows.ComputeBlasBuildScratchSize(8192u);
	PROJECTV_RTX_EXPECT(nonZero > 0u, "scratch size for non-zero primitives must be positive");
}

void TestBuildChunkBlasGuardsForNullCommandBuffer()
{
	projectv::render::RayTracedShadows shadows{};
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VulkanContextState context{};
	projectv::render::DirtyChunkRebuild entry{};
	entry.chunkIndex = 0u;
	entry.aabb.minX = 0.0f;
	entry.aabb.minY = 0.0f;
	entry.aabb.minZ = 0.0f;
	entry.aabb.maxX = 1.0f;
	entry.aabb.maxY = 1.0f;
	entry.aabb.maxZ = 1.0f;
	PROJECTV_RTX_EXPECT(
		!shadows.BuildChunkBlas(cmd, context, 0u, entry.aabb),
		"BuildChunkBlas must reject null command buffer when disabled");
}

void TestBuildChunkBlasRejectsInvertedAabb()
{
	projectv::render::RayTracedShadows shadows{};
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VulkanContextState context{};
	projectv::render::DirtyChunkRebuild entry{};
	entry.chunkIndex = 0u;
	entry.aabb.minX = 1.0f;
	entry.aabb.minY = 0.0f;
	entry.aabb.minZ = 0.0f;
	entry.aabb.maxX = 0.0f;
	entry.aabb.maxY = 1.0f;
	entry.aabb.maxZ = 1.0f;
	PROJECTV_RTX_EXPECT(
		!shadows.BuildChunkBlas(cmd, context, 0u, entry.aabb),
		"BuildChunkBlas must reject inverted AABB");
}

void TestSetBlasDirtyQueueAndConsume()
{
	projectv::render::RayTracedShadows shadows{};
	std::vector<projectv::render::DirtyChunkRebuild> dirty{};
	dirty.reserve(4u);
	for (uint32_t i = 0u; i < 4u; ++i) {
		projectv::render::DirtyChunkRebuild entry{};
		entry.chunkIndex = i;
		entry.aabb.minX = static_cast<float>(i);
		entry.aabb.minY = 0.0f;
		entry.aabb.minZ = 0.0f;
		entry.aabb.maxX = static_cast<float>(i) + 1.0f;
		entry.aabb.maxY = 1.0f;
		entry.aabb.maxZ = 1.0f;
		dirty.push_back(entry);
	}
	shadows.SetBlasDirtyQueue(std::move(dirty));
	VulkanContextState context{};
	shadows.BuildDirtyBlases(context, VK_NULL_HANDLE);
	PROJECTV_RTX_EXPECT(
		shadows.GetConfig().blasRebuildCount == 4u,
		"BuildDirtyBlases must consume the dirty queue and bump counter");
}

void TestUpdateTlasRespectsCapacity()
{
	projectv::render::RayTracedShadowConfig config{};
	constexpr VkDeviceSize kCapacity = sizeof(VkAccelerationStructureInstanceKHR) * 4u;
	alignas(16) unsigned char storage[kCapacity]{};
	config.tlasInstanceMappedData = storage;
	config.tlasInstanceCapacityBytes = kCapacity;
	PROJECTV_RTX_EXPECT(
		sizeof(VkAccelerationStructureInstanceKHR) == 64u,
		"VkAccelerationStructureInstanceKHR must remain 64 bytes per Vulkan 1.4 KHR spec");
}

void TestDirtyChunkRebuildStructLayout()
{
	const projectv::render::DirtyChunkRebuild entry{};
	PROJECTV_RTX_EXPECT(entry.chunkIndex == 0u, "DirtyChunkRebuild default chunkIndex must be 0");
	PROJECTV_RTX_EXPECT(entry.aabb.minX == 0.0f && entry.aabb.maxX == 0.0f,
		"DirtyChunkRebuild default AABB must be zero-init");
	PROJECTV_RTX_EXPECT(sizeof(VkAabbPositionsKHR) == 24u,
		"VkAabbPositionsKHR must remain 24 bytes (6 floats) per Vulkan 1.4 spec");
}

void TestBuildChunkBlasAabbBoundsCheck()
{
	projectv::render::RayTracedShadows shadows{};
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VulkanContextState context{};
	VkAabbPositionsKHR negativeOrigin{};
	negativeOrigin.minX = -10.0f;
	negativeOrigin.minY = -10.0f;
	negativeOrigin.minZ = -10.0f;
	negativeOrigin.maxX = -5.0f;
	negativeOrigin.maxY = -5.0f;
	negativeOrigin.maxZ = -5.0f;
	PROJECTV_RTX_EXPECT(
		!shadows.BuildChunkBlas(cmd, context, 0u, negativeOrigin),
		"BuildChunkBlas must accept negative-world-origin AABB (voxels may extend below origin)");
}

void TestConfigHasBlasCacheFields()
{
	projectv::render::RayTracedShadowConfig config{};
	PROJECTV_RTX_EXPECT(config.blasHandles.empty(), "blasHandles must default to empty");
	PROJECTV_RTX_EXPECT(config.blasStorageBuffers.empty(), "blasStorageBuffers must default to empty");
	PROJECTV_RTX_EXPECT(config.blasStorageAllocations.empty(), "blasStorageAllocations must default to empty");
	PROJECTV_RTX_EXPECT(config.blasDeviceAddresses.empty(), "blasDeviceAddresses must default to empty");
	PROJECTV_RTX_EXPECT(config.blasStorageCapacityBytes.empty(), "blasStorageCapacityBytes must default to empty");
	PROJECTV_RTX_EXPECT(config.tlasBackingBuffer == VK_NULL_HANDLE, "tlasBackingBuffer must default to null");
	PROJECTV_RTX_EXPECT(config.tlasBackingAllocation == nullptr, "tlasBackingAllocation must default to null");
	PROJECTV_RTX_EXPECT(config.tlasBackingDeviceAddress == 0u, "tlasBackingDeviceAddress must default to 0");
	PROJECTV_RTX_EXPECT(config.tlasBackingCapacityBytes == 0u, "tlasBackingCapacityBytes must default to 0");
}

void TestUpdateTlasSafeWithoutBlasCache()
{
	projectv::render::RayTracedShadows shadows{};
	constexpr VkDeviceSize kCapacity = sizeof(VkAccelerationStructureInstanceKHR) * 4u;
	alignas(16) unsigned char storage[kCapacity]{};
	auto &config = projectv::render::RayTracedShadowTestAccess::Config(shadows);
	config.tlasInstanceMappedData = storage;
	config.tlasInstanceCapacityBytes = kCapacity;
	config.blasDeviceAddresses.assign(2u, 0u);

	std::vector<uint32_t> chunks{0u, 1u};
	std::vector<VkTransformMatrixKHR> transforms(chunks.size(), VkTransformMatrixKHR{});
	VulkanContextState context{};
	shadows.UpdateTlas(context, chunks, transforms);
	PROJECTV_RTX_EXPECT(
		shadows.GetConfig().tlasInstanceCount == 2u,
		"UpdateTlas must populate instance count from input");
	PROJECTV_RTX_EXPECT(
		shadows.GetConfig().tlasRebuildCount == 1u,
		"UpdateTlas must bump tlasRebuildCount counter");
	const auto *instances = reinterpret_cast<const VkAccelerationStructureInstanceKHR *>(storage);
	PROJECTV_RTX_EXPECT(
		instances[0].accelerationStructureReference == 0u,
		"accelerationStructureReference must be 0 when BLAS cache device address is 0 (no crash, no garbage)");
	PROJECTV_RTX_EXPECT(
		instances[0].instanceCustomIndex == 0u,
		"instanceCustomIndex must encode chunkIndex from input");
	PROJECTV_RTX_EXPECT(
		instances[1].instanceCustomIndex == 1u,
		"instanceCustomIndex must encode chunkIndex for slot 1");
	PROJECTV_RTX_EXPECT(
		instances[0].mask == 0xFFu,
		"instance mask must be 0xFF (visible to all rays)");
}

void TestUpdateTlasSafeForOversizedChunkIndex()
{
	projectv::render::RayTracedShadows shadows{};
	constexpr VkDeviceSize kCapacity = sizeof(VkAccelerationStructureInstanceKHR) * 4u;
	alignas(16) unsigned char storage[kCapacity]{};
	auto &config = projectv::render::RayTracedShadowTestAccess::Config(shadows);
	config.tlasInstanceMappedData = storage;
	config.tlasInstanceCapacityBytes = kCapacity;

	std::vector<uint32_t> chunks{9999u};
	std::vector<VkTransformMatrixKHR> transforms(1u, VkTransformMatrixKHR{});
	VulkanContextState context{};
	shadows.UpdateTlas(context, chunks, transforms);
	const auto *instances = reinterpret_cast<const VkAccelerationStructureInstanceKHR *>(storage);
	PROJECTV_RTX_EXPECT(
		instances[0].accelerationStructureReference == 0u,
		"UpdateTlas must safely write 0 accelerationStructureReference for out-of-range chunkIndex");
}

void TestRecordTlasBuildGuardsForZeroInstanceCount()
{
	projectv::render::RayTracedShadows shadows{};
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VulkanContextState context{};
	shadows.RecordTlasBuild(cmd, context);
	PROJECTV_RTX_EXPECT(
		shadows.GetConfig().shadowRayDispatchCount == 0u,
		"RecordTlasBuild must not increment dispatch counter when instance count is zero");
}

void TestRtxSunShadowRayHelperExistsInShader()
{
	FILE *const fp = std::fopen(
		PROJECTV_TESTS_SOURCE_DIR "/../src/shaders/voxel.frag",
		"r");
	PROJECTV_RTX_EXPECT(fp != nullptr, "voxel.frag must be readable from source root");
	if (fp == nullptr) {
		return;
	}
	char buffer[16384]{};
	const size_t read = std::fread(buffer, 1u, sizeof(buffer) - 1u, fp);
	std::fclose(fp);
	buffer[read] = '\0';
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "TraceRtxSunShadowRay") != nullptr,
		"voxel.frag must define TraceRtxSunShadowRay helper for 5.2.B RTX sun shadow consume");
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "rayQueryInitializeEXT") != nullptr,
		"voxel.frag must use rayQueryInitializeEXT for shadow ray dispatch");
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "kRtxSunShadowMaxDistanceMeters") != nullptr,
		"voxel.frag must declare kRtxSunShadowMaxDistanceMeters EVIL constant");
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "VOXEL_RTX_ENABLED") != nullptr,
		"voxel.frag must guard RTX shadow path with VOXEL_RTX_ENABLED define");
}

}  // namespace

int main()
{
	TestEnvGateDefaultsOff();
	TestEnvGateOnRespected();
	TestConfigDefaultValues();
	TestConfigZeroSizedAfterShutdown();
	TestComputeBlasBuildScratchSize();
	TestBuildChunkBlasGuardsForNullCommandBuffer();
	TestBuildChunkBlasRejectsInvertedAabb();
	TestBuildChunkBlasAabbBoundsCheck();
	TestDirtyChunkRebuildStructLayout();
	TestSetBlasDirtyQueueAndConsume();
	TestUpdateTlasRespectsCapacity();
	TestConfigHasBlasCacheFields();
	TestUpdateTlasSafeWithoutBlasCache();
	TestUpdateTlasSafeForOversizedChunkIndex();
	TestRecordTlasBuildGuardsForZeroInstanceCount();
	TestRtxSunShadowRayHelperExistsInShader();

	if (gFailureCount != 0) {
		runtime::LogRuntimeFailure(
			"Tests",
			"ProjectVRayTracedShadowTests",
			"one or more RTX sub-tests failed");
		return 1;
	}
	SDL_Log("ProjectVRayTracedShadowTests: all sub-tests passed");
	return 0;
}
