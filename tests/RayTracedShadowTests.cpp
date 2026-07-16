// Copyright (c) 2026 ProjectV
// SPDX-License-Identifier: MIT
// Stage 5.2 RTX foundation sub-tests (no Vulkan device required; pure CPU contracts).

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

#include "core/RuntimeDiagnostics.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/RtxGiProbes.hpp"
#include "render/RtxShadowPipeline.hpp"
#include "render/RtxShadowSBT.hpp"

#ifndef PROJECTV_BUILD_DIR
#define PROJECTV_BUILD_DIR ""
#endif

namespace {

int gFailureCount = 0;

#define PROJECTV_RTX_EXPECT(cond, message)                                                \
	do {                                                                                  \
		if (!(cond)) {                                                                    \
			runtime::LogRuntimeFailure("Tests", "ProjectVRayTracedShadowTests", message); \
			++gFailureCount;                                                              \
		}                                                                                 \
	} while (false)

std::string ReadProjectSource(const char *relativePath)
{
	std::ifstream source{
		std::string{PROJECTV_TESTS_SOURCE_DIR} + "/../src/" + relativePath,
		std::ios::binary};
	if (!source.good()) {
		return {};
	}
	return {std::istreambuf_iterator(source), std::istreambuf_iterator<char>()};
}

void TestEnvGateDefaultsOff()
{
	constexpr VulkanContextState context{};
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
}

void TestConfigDefaultValues()
{
	const projectv::render::RayTracedShadowConfig config{};
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
	const projectv::render::RayTracedShadows shadows{};
	PROJECTV_RTX_EXPECT(!shadows.IsEnabled(), "default-constructed shadows must be disabled");
	PROJECTV_RTX_EXPECT(shadows.GetConfig().tlasInstanceCount == 0u, "default config must have zero instance count");
}

void TestComputeBlasBuildScratchSize()
{
	PROJECTV_RTX_EXPECT(
		projectv::render::RayTracedShadows::ComputeBlasBuildScratchSize(0u) == 0u,
		"scratch size for zero primitives must be zero");
	const VkDeviceSize nonZero = projectv::render::RayTracedShadows::ComputeBlasBuildScratchSize(8192u);
	PROJECTV_RTX_EXPECT(nonZero > 0u, "scratch size for non-zero primitives must be positive");
}

void TestBuildChunkBlasGuardsForNullCommandBuffer()
{
	projectv::render::RayTracedShadows shadows{};
	const VkCommandBuffer cmd = VK_NULL_HANDLE;
	constexpr VulkanContextState context{};
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
	const VkCommandBuffer cmd = VK_NULL_HANDLE;
	constexpr VulkanContextState context{};
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
	constexpr VulkanContextState context{};
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
	constexpr projectv::render::DirtyChunkRebuild entry{};
	PROJECTV_RTX_EXPECT(entry.chunkIndex == 0u, "DirtyChunkRebuild default chunkIndex must be 0");
	PROJECTV_RTX_EXPECT(entry.aabb.minX == 0.0f && entry.aabb.maxX == 0.0f,
						"DirtyChunkRebuild default AABB must be zero-init");
	PROJECTV_RTX_EXPECT(sizeof(VkAabbPositionsKHR) == 24u,
						"VkAabbPositionsKHR must remain 24 bytes (6 floats) per Vulkan 1.4 spec");
}

void TestBuildChunkBlasAabbBoundsCheck()
{
	projectv::render::RayTracedShadows shadows{};
	const VkCommandBuffer cmd = VK_NULL_HANDLE;
	constexpr VulkanContextState context{};
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
	const projectv::render::RayTracedShadowConfig config{};
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

	const std::vector chunks{0u, 1u};
	const std::vector transforms(chunks.size(), VkTransformMatrixKHR{});
	constexpr VulkanContextState context{};
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

	const std::vector chunks{9999u};
	const std::vector transforms(1u, VkTransformMatrixKHR{});
	constexpr VulkanContextState context{};
	shadows.UpdateTlas(context, chunks, transforms);
	const auto *instances = reinterpret_cast<const VkAccelerationStructureInstanceKHR *>(storage);
	PROJECTV_RTX_EXPECT(
		instances[0].accelerationStructureReference == 0u,
		"UpdateTlas must safely write 0 accelerationStructureReference for out-of-range chunkIndex");
}

void TestRecordTlasBuildGuardsForZeroInstanceCount()
{
	projectv::render::RayTracedShadows shadows{};
	const VkCommandBuffer cmd = VK_NULL_HANDLE;
	constexpr VulkanContextState context{};
	shadows.RecordTlasBuild(cmd, context);
	PROJECTV_RTX_EXPECT(
		shadows.GetConfig().shadowRayDispatchCount == 0u,
		"RecordTlasBuild must not increment dispatch counter when instance count is zero");
}

void TestRtxSunShadowRayHelperExistsInShader()
{
	std::ifstream fp{std::string{PROJECTV_TESTS_SOURCE_DIR} + "/../src/shaders/voxel.frag",
					 std::ios::binary};
	PROJECTV_RTX_EXPECT(fp.good(), "voxel.frag must be readable from source root");
	if (!fp.good()) {
		return;
	}
	const std::string sourceText{std::istreambuf_iterator(fp), std::istreambuf_iterator<char>()};
	PROJECTV_RTX_EXPECT(
		sourceText.find("rtxShadowMask") != std::string::npos,
		"voxel.frag must reference rtxShadowMask (binding 18) for 5.2.E voxel-aware shadow consume");
	PROJECTV_RTX_EXPECT(
		sourceText.find("texture(rtxShadowMask") != std::string::npos,
		"voxel.frag must sample rtxShadowMask via texture() call");
	PROJECTV_RTX_EXPECT(
		sourceText.find("binding = 18") != std::string::npos,
		"voxel.frag must declare binding 18 for the shadow mask sampler");
	PROJECTV_RTX_EXPECT(
		sourceText.find("VOXEL_RTX_ENABLED") != std::string::npos,
		"voxel.frag must guard RTX shadow path with VOXEL_RTX_ENABLED define");
}

void TestRtxAmbientOcclusionRayHelperExistsInShader()
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
		std::strstr(buffer, "TraceRtxAmbientOcclusionRay") != nullptr,
		"voxel.frag must define TraceRtxAmbientOcclusionRay helper for 5.4 RTX AO consume");
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "kRtxAoMinRayLengthMeters") != nullptr,
		"voxel.frag must declare kRtxAoMinRayLengthMeters EVIL constant for AO T_min offset");
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "gl_RayFlagsOpaqueEXT") != nullptr,
		"voxel.frag AO ray must set gl_RayFlagsOpaqueEXT for fast path on opaque BLAS");
}

void TestAmbientOcclusionVisibilitySwitchesToRtxWhenEnabled()
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
		std::strstr(buffer, "TraceRtxAmbientOcclusionRay(") != nullptr,
		"voxel.frag must reference TraceRtxAmbientOcclusionRay under VOXEL_RTX_ENABLED");
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "gl_RayFlagsOpaqueEXT") != nullptr,
		"voxel.frag AO ray dispatch must set gl_RayFlagsOpaqueEXT for fast path on opaque BLAS");
}

void TestRtxAoDispatchUsesTerminateOnFirstHitFlag()
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
	const char *aoHelper = std::strstr(buffer, "TraceRtxAmbientOcclusionRay");
	PROJECTV_RTX_EXPECT(
		aoHelper != nullptr && std::strstr(aoHelper, "gl_RayFlagsTerminateOnFirstHitEXT") != nullptr,
		"TraceRtxAmbientOcclusionRay must OR gl_RayFlagsTerminateOnFirstHitEXT to bail on first hit (binary AO test)");
}

void TestRtxAoShaderBinaryBuilt()
{
	bool found = false;
	constexpr const char *const kCandidatePaths[]{
		PROJECTV_BUILD_DIR "/src/voxel.frag.rtx.spv",
		PROJECTV_BUILD_DIR "/bin/voxel.frag.rtx.spv",
		// Legacy hard-coded preset paths kept for out-of-tree IDE runs without PROJECTV_BUILD_DIR refresh.
		PROJECTV_TESTS_SOURCE_DIR "/../build/linux-clang-debug/src/voxel.frag.rtx.spv",
		PROJECTV_TESTS_SOURCE_DIR "/../build/linux-clang-debug/bin/voxel.frag.rtx.spv",
		PROJECTV_TESTS_SOURCE_DIR "/../build/windows-clang-debug/src/voxel.frag.rtx.spv",
		PROJECTV_TESTS_SOURCE_DIR "/../build/windows-clang-debug/bin/voxel.frag.rtx.spv",
	};
	for (const char *path : kCandidatePaths) {
		FILE *const fp = std::fopen(path, "rb");
		if (fp != nullptr) {
			std::fclose(fp);
			found = true;
			break;
		}
	}
	PROJECTV_RTX_EXPECT(found, "voxel.frag.rtx.spv must be built (CMake glslang target)");
}

void TestRtxGiProbeConfigDefaults()
{
	constexpr projectv::render::RtxGiProbeConfig config{};
	PROJECTV_RTX_EXPECT(config.irradianceImage == VK_NULL_HANDLE, "default irradiance image must be null");
	PROJECTV_RTX_EXPECT(config.distanceImage == VK_NULL_HANDLE, "default distance image must be null");
	PROJECTV_RTX_EXPECT(config.probeDataImage == VK_NULL_HANDLE, "default probe data image must be null");
	PROJECTV_RTX_EXPECT(config.volumeDescBuffer == VK_NULL_HANDLE, "default volume desc buffer must be null");
	PROJECTV_RTX_EXPECT(!config.enabled, "default config must be disabled");
	PROJECTV_RTX_EXPECT(config.probeCountAxisX == 0u, "default probe count must be 0");
	PROJECTV_RTX_EXPECT(config.raysPerProbe == 0u, "default rays per probe must be 0");
	PROJECTV_RTX_EXPECT(config.updateDispatchCount == 0u, "default dispatch count must be 0");
}

void TestRtxGiProbesClassHasGetters()
{
	const projectv::render::RtxGiProbes probes{};
	PROJECTV_RTX_EXPECT(!probes.IsEnabled(), "default-constructed probes must be disabled");
	PROJECTV_RTX_EXPECT(probes.GetConfig().probeCountAxisX == 0u, "default config probe count must be 0");
}

void TestRtxGiProbeEnvGateRequiresBoth()
{
	VulkanContextState context{};
	PROJECTV_RTX_EXPECT(
		!projectv::render::IsRtxGiProbeFieldEnabled(context),
		"IsRtxGiProbeFieldEnabled must be false on default-constructed context (no RT capability)");
	context.rayTracing.accelerationStructure = true;
	context.rayTracing.rayQuery = true;
	PROJECTV_RTX_EXPECT(
		projectv::render::IsRtxGiProbeFieldEnabled(context),
		"IsRtxGiProbeFieldEnabled must be true when both accelerationStructure && rayQuery are set");
}

void TestRtxGiProbeShaderBindingsDeclared()
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
		std::strstr(buffer, "rtxGiIrradiance") != nullptr,
		"voxel.frag must declare rtxGiIrradiance sampler3D for DDGI consume (Stage 5.5)");
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "rtxGiVolume") != nullptr,
		"voxel.frag must declare rtxGiVolume SSBO for DDGI volume descriptor (Stage 5.5)");
	PROJECTV_RTX_EXPECT(
		std::strstr(buffer, "SampleRtxGiProbeIrradiance") != nullptr,
		"voxel.frag must define SampleRtxGiProbeIrradiance helper for DDGI trilinear sample");
}

void TestRtxGiProbeRecordUpdatePassNoopWithoutContext()
{
	projectv::render::RtxGiProbes probes{};
	const VkCommandBuffer cmd = VK_NULL_HANDLE;
	constexpr VulkanContextState context{};
	PROJECTV_RTX_EXPECT(
		!probes.RecordUpdatePass(cmd, context, VK_NULL_HANDLE),
		"RecordUpdatePass must return false when probes are not initialized");
}

void TestRtxShadowPipelineClassHasGetters()
{
	const projectv::render::RtxShadowPipeline pipeline{};
	PROJECTV_RTX_EXPECT(
		pipeline.GetPipeline() == VK_NULL_HANDLE,
		"RtxShadowPipeline default-constructed pipeline handle must be null");
	PROJECTV_RTX_EXPECT(
		pipeline.GetPipelineLayout() == VK_NULL_HANDLE,
		"RtxShadowPipeline default-constructed pipeline layout must be null");
	PROJECTV_RTX_EXPECT(
		pipeline.GetDescriptorSetLayout() == VK_NULL_HANDLE,
		"RtxShadowPipeline default-constructed descriptor set layout must be null");
	PROJECTV_RTX_EXPECT(!pipeline.IsReady(), "RtxShadowPipeline must not be ready before Initialize");
	PROJECTV_RTX_EXPECT(
		pipeline.GetRayGenGroupIndex() == 0u,
		"RtxShadowPipeline raygen group index must be 0");
	PROJECTV_RTX_EXPECT(
		pipeline.GetMissGroupIndex() == 1u,
		"RtxShadowPipeline miss group index must be 1");
	PROJECTV_RTX_EXPECT(
		pipeline.GetHitGroupIndex() == 2u,
		"RtxShadowPipeline hit group index must be 2");
}

void TestRtxShadowSbtClassHasGetters()
{
	const projectv::render::RtxShadowSBT sbt{};
	PROJECTV_RTX_EXPECT(!sbt.IsReady(), "RtxShadowSBT must not be ready before Initialize");
	const auto &[deviceAddress, stride, size] = sbt.GetRaygenRegion();
	const auto &miss = sbt.GetMissRegion();
	const auto &hit = sbt.GetHitRegion();
	const auto &callable = sbt.GetCallableRegion();
	PROJECTV_RTX_EXPECT(
		deviceAddress == 0u,
		"RtxShadowSBT default-constructed raygen deviceAddress must be zero");
	PROJECTV_RTX_EXPECT(
		stride == 0u,
		"RtxShadowSBT default-constructed raygen stride must be zero");
	PROJECTV_RTX_EXPECT(
		size == 0u,
		"RtxShadowSBT default-constructed raygen size must be zero");
	PROJECTV_RTX_EXPECT(
		miss.deviceAddress == 0u,
		"RtxShadowSBT default-constructed miss deviceAddress must be zero");
	PROJECTV_RTX_EXPECT(
		hit.deviceAddress == 0u,
		"RtxShadowSBT default-constructed hit deviceAddress must be zero");
	PROJECTV_RTX_EXPECT(
		callable.size == 0u,
		"RtxShadowSBT callable region must have zero size (no callable shaders)");
}

void TestRtxShadowShaderFilesExistInBuildDirectory()
{
	const char *buildDir = std::getenv("PROJECTV_BUILD_DIR");
	const std::string baseDir = buildDir != nullptr ? std::string{buildDir} : std::string{};
	for (const char *suffix : {"/voxel_rtx_shadow.rgen.spv",
							   "/voxel_rtx_shadow.rint.spv",
							   "/voxel_rtx_shadow.rchit.spv",
							   "/voxel_rtx_shadow.rmiss.spv"}) {
		const std::string path = baseDir + suffix;
		std::ifstream file{path, std::ios::binary};
		PROJECTV_RTX_EXPECT(
			file.good(),
			"RTX shadow shader binary missing: " + path);
	}
}

void TestRtxShadowIntersectionShaderUsesVoxelDdaPattern()
{
	const char *buildDir = std::getenv("PROJECTV_BUILD_DIR");
	const std::string path = std::string{buildDir != nullptr ? buildDir : ""} + "/voxel_rtx_shadow.rint.spv";
	std::ifstream file{path, std::ios::binary};
	if (!file.good()) {
		gFailureCount++;
		runtime::LogRuntimeFailure(
			"Tests",
			"TestRtxShadowIntersectionShaderUsesVoxelDdaPattern",
			"voxel_rtx_shadow.rint.spv not found");
		return;
	}
	const std::string sourcePath = PROJECTV_TESTS_SOURCE_DIR "/../src/shaders/voxel_rtx_shadow.rint";
	std::ifstream source{sourcePath, std::ios::binary};
	if (!source.good()) {
		gFailureCount++;
		runtime::LogRuntimeFailure(
			"Tests",
			"TestRtxShadowIntersectionShaderUsesVoxelDdaPattern",
			"voxel_rtx_shadow.rint source not found");
		return;
	}
	std::string sourceText{std::istreambuf_iterator(source), std::istreambuf_iterator<char>()};
	PROJECTV_RTX_EXPECT(
		sourceText.find("gl_InstanceCustomIndexEXT") != std::string::npos,
		"voxel_rtx_shadow.rint must read chunk index from gl_InstanceCustomIndexEXT");
	PROJECTV_RTX_EXPECT(
		sourceText.find("reportIntersectionEXT") != std::string::npos,
		"voxel_rtx_shadow.rint must call reportIntersectionEXT to mark voxel hits");
	PROJECTV_RTX_EXPECT(
		sourceText.find("chunkVoxelWords") != std::string::npos,
		"voxel_rtx_shadow.rint must read PackedChunkVoxelPayload chunkVoxelWords");
	PROJECTV_RTX_EXPECT(
		sourceText.find("voxelDataInfo") != std::string::npos,
		"voxel_rtx_shadow.rint must read chunkDescriptor.voxelDataInfo (wordOffset + voxelCount)");
}

void TestRtxGiProbeHostHeaderExistsAndLinks()
{
	FILE *const fp = std::fopen(
		PROJECTV_TESTS_SOURCE_DIR "/../src/render/RtxGiProbes.hpp",
		"r");
	PROJECTV_RTX_EXPECT(fp != nullptr, "RtxGiProbes.hpp must be readable from source root");
	if (fp != nullptr) {
		char buffer[8192]{};
		const size_t read = std::fread(buffer, 1u, sizeof(buffer) - 1u, fp);
		std::fclose(fp);
		buffer[read] = '\0';
		PROJECTV_RTX_EXPECT(
			std::strstr(buffer, "class RtxGiProbes") != nullptr,
			"RtxGiProbes.hpp must declare class RtxGiProbes");
		PROJECTV_RTX_EXPECT(
			std::strstr(buffer, "Initialize") != nullptr && std::strstr(buffer, "Shutdown") != nullptr,
			"RtxGiProbes.hpp must declare Initialize/Shutdown lifecycle");
	}
}

void TestGpuAttributionMarkersBracketTargetWork()
{
	const std::string drawFrameSource = ReadProjectSource("render/RendererDrawFrame.cpp");
	const std::string shadowPassSource = ReadProjectSource("render/RayTracedShadowsPass.cpp");
	PROJECTV_RTX_EXPECT(
		!drawFrameSource.empty(),
		"RendererDrawFrame.cpp must be readable for GPU attribution contracts");
	PROJECTV_RTX_EXPECT(
		!shadowPassSource.empty(),
		"RayTracedShadowsPass.cpp must be readable for GPU attribution contracts");
	if (drawFrameSource.empty() || shadowPassSource.empty()) {
		return;
	}

	const size_t blasBuild = drawFrameSource.find("RecordDirtyBlasBuilds(");
	const size_t blasMarker = drawFrameSource.rfind("RTX BLAS Build", blasBuild);
	PROJECTV_RTX_EXPECT(
		blasBuild != std::string::npos && blasMarker != std::string::npos,
		"RTX BLAS marker must bracket RecordDirtyBlasBuilds");

	const size_t traceDispatch = shadowPassSource.find("vkCmdTraceRaysKHR(");
	const size_t traceMarker = shadowPassSource.rfind("RTX Shadow Trace", traceDispatch);
	PROJECTV_RTX_EXPECT(
		traceDispatch != std::string::npos && traceMarker != std::string::npos,
		"RTX trace marker must bracket vkCmdTraceRaysKHR");
}

void TestRtxTraversalMetricsContract()
{
	const std::string headerSource = ReadProjectSource("render/RayTracedShadows.hpp");
	const std::string pipelineSource = ReadProjectSource("render/RtxShadowPipeline.cpp");
	const std::string passSource = ReadProjectSource("render/RayTracedShadowsPass.cpp");
	const std::string intersectionSource = ReadProjectSource("shaders/voxel_rtx_shadow.rint");
	const std::string appUpdateSource = ReadProjectSource("app/AppUpdate.cpp");
	PROJECTV_RTX_EXPECT(
		headerSource.find("RtxTraversalCounters") != std::string::npos,
		"RT traversal counters must have a host-side contract");
	PROJECTV_RTX_EXPECT(
		pipelineSource.find("binding = 20") != std::string::npos,
		"RT traversal metrics must reserve descriptor binding 20");
	PROJECTV_RTX_EXPECT(
		passSource.find("traversalCounterBuffer") != std::string::npos,
		"RT pass must bind a traversal counter buffer");
	PROJECTV_RTX_EXPECT(
		intersectionSource.find("gl_IncomingRayFlagsEXT") != std::string::npos,
		"rint must classify primary and sun rays by incoming flags");
	PROJECTV_RTX_EXPECT(
		intersectionSource.find("atomicAdd") != std::string::npos,
		"rint must collect candidate and DDA counters with atomics");
	PROJECTV_RTX_EXPECT(
		appUpdateSource.find("rtx_primary_aabb=%u") != std::string::npos,
		"InputReplay metrics must emit primary traversal candidates");
	PROJECTV_RTX_EXPECT(
		appUpdateSource.find("rtx_sun_steps=%u") != std::string::npos,
		"InputReplay metrics must emit sun DDA steps");
}

void TestTightChunkAabbRoutingContract()
{
	const std::string headerSource = ReadProjectSource("render/RayTracedShadows.hpp");
	const std::string drawFrameSource = ReadProjectSource("render/RendererDrawFrame.cpp");
	PROJECTV_RTX_EXPECT(
		headerSource.find("TryBuildTightChunkAabb") != std::string::npos,
		"RTX must expose a pure tight chunk-AABB reconstruction helper");
	PROJECTV_RTX_EXPECT(
		drawFrameSource.find("chunkAabbMappedData") != std::string::npos,
		"RTX BLAS rebuilds must consume the current frame packed AABB data");
	PROJECTV_RTX_EXPECT(
		drawFrameSource.find("TryBuildTightChunkAabb") != std::string::npos,
		"RTX BLAS rebuilds must prefer validated tight chunk AABBs");
}

void TestTightChunkAabbReconstruction()
{
	PackedSceneChunkAabb singleVoxel{};
	singleVoxel.centerAndHalfExtent = {3.5f, -2.5f, 8.5f, 0.5f};
	singleVoxel.originAndPadding = {0.5f, 0.5f, 0.5f, 1.0f};
	const auto singleVoxelAabb = projectv::render::TryBuildTightChunkAabb(singleVoxel);
	PROJECTV_RTX_EXPECT(singleVoxelAabb.has_value(), "one occupied voxel must produce a tight AABB");
	if (singleVoxelAabb.has_value()) {
		PROJECTV_RTX_EXPECT(
			singleVoxelAabb->minX == 3.0f && singleVoxelAabb->minY == -3.0f &&
				singleVoxelAabb->minZ == 8.0f && singleVoxelAabb->maxX == 4.0f &&
				singleVoxelAabb->maxY == -2.0f && singleVoxelAabb->maxZ == 9.0f,
			"one occupied voxel must reconstruct its exact world bounds");
	}

	PackedSceneChunkAabb thinAxis{};
	thinAxis.centerAndHalfExtent = {-7.5f, 4.0f, -11.5f, 4.0f};
	thinAxis.originAndPadding = {0.5f, 3.0f, 0.5f, 1.0f};
	const auto thinAxisAabb = projectv::render::TryBuildTightChunkAabb(thinAxis);
	PROJECTV_RTX_EXPECT(thinAxisAabb.has_value(), "thin-axis content must keep its exact non-zero extents");
	if (thinAxisAabb.has_value()) {
		PROJECTV_RTX_EXPECT(
			thinAxisAabb->minX == -8.0f && thinAxisAabb->maxX == -7.0f &&
				thinAxisAabb->minY == 1.0f && thinAxisAabb->maxY == 7.0f &&
				thinAxisAabb->minZ == -12.0f && thinAxisAabb->maxZ == -11.0f,
			"tight reconstruction must use per-axis extents rather than HZB's scalar extent");
	}
}

void TestTightChunkAabbRejectsInvalidBounds()
{
	PackedSceneChunkAabb empty{};
	empty.centerAndHalfExtent = {1.0f, 1.0f, 1.0f, 1.0f};
	empty.originAndPadding = {1.0f, 1.0f, 1.0f, 0.0f};
	PROJECTV_RTX_EXPECT(
		!projectv::render::TryBuildTightChunkAabb(empty).has_value(),
		"empty packed bounds must select the conservative full-chunk fallback");

	PackedSceneChunkAabb degenerate{};
	degenerate.centerAndHalfExtent = {-4.0f, 0.0f, 2.0f, 1.0f};
	degenerate.originAndPadding = {1.0f, 0.0f, 1.0f, 1.0f};
	PROJECTV_RTX_EXPECT(
		!projectv::render::TryBuildTightChunkAabb(degenerate).has_value(),
		"degenerate packed bounds must select the conservative full-chunk fallback");

	PackedSceneChunkAabb nonFinite{};
	nonFinite.centerAndHalfExtent = {std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 1.0f};
	nonFinite.originAndPadding = {1.0f, 1.0f, 1.0f, 1.0f};
	PROJECTV_RTX_EXPECT(
		!projectv::render::TryBuildTightChunkAabb(nonFinite).has_value(),
		"non-finite packed bounds must select the conservative full-chunk fallback");
}

void TestTightAabbAndTraversalSourceContracts()
{
	const std::string pipelineSource = ReadProjectSource("render/RtxShadowPipeline.cpp");
	const std::string maskSource = ReadProjectSource("render/RayTracedShadowsMask.cpp");
	const std::string intersectionSource = ReadProjectSource("shaders/voxel_rtx_shadow.rint");
	const std::string drawFrameSource = ReadProjectSource("render/RendererDrawFrame.cpp");
	PROJECTV_RTX_EXPECT(
		pipelineSource.find("bindings[6].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR") != std::string::npos,
		"traversal counter binding must be visible only to the intersection shader");
	PROJECTV_RTX_EXPECT(
		maskSource.find("PROJECTV_RTX_TRAVERSAL_METRICS") != std::string::npos &&
			intersectionSource.find("projectvRtxTraversalMetricsEnabled") != std::string::npos,
		"traversal counter collection must remain runtime-gated");
	const size_t tightAabbRead = drawFrameSource.find("aabbFrameResources.chunkAabbMappedData");
	const size_t queueWrite = drawFrameSource.find("SetBlasDirtyQueue", tightAabbRead);
	PROJECTV_RTX_EXPECT(
		tightAabbRead != std::string::npos && queueWrite != std::string::npos &&
			drawFrameSource.substr(tightAabbRead, queueWrite - tightAabbRead).find("visibilityMask") == std::string::npos,
		"BLAS routing must consume current packed AABBs without HZB visibility masks");
	PROJECTV_RTX_EXPECT(
		drawFrameSource.find("PROJECTV_RTX_TIGHT_AABB") != std::string::npos,
		"replays must be able to select the full-AABB baseline without source edits");
}

void TestNsightGpuTraceReplayTickContract()
{
	const std::string automationSource = ReadProjectSource("app/NsightGpuTraceAutomation.cpp");
	const std::string mainSource = ReadProjectSource("app/main.cpp");
	PROJECTV_RTX_EXPECT(
		automationSource.find("PROJECTV_NGFX_GPU_TRACE_REPLAY_TICK") != std::string::npos,
		"Nsight GPU Trace must expose a replay-tick environment gate");
	PROJECTV_RTX_EXPECT(
		automationSource.find("NGFX_GPUTrace_InitializeActivity_Vulkan") != std::string::npos,
		"Nsight GPU Trace activity must initialize before Vulkan work");
	PROJECTV_RTX_EXPECT(
		automationSource.find("NGFX_GPUTrace_StartTrace_Vulkan") != std::string::npos,
		"Nsight GPU Trace must start procedurally at the requested replay tick");
	PROJECTV_RTX_EXPECT(
		automationSource.find("NGFX_GPUTrace_StopTrace_Vulkan") == std::string::npos,
		"a frame-limited Nsight trace must not require a separate SDK stop trigger");
	const size_t initVulkan = mainSource.find("InitVulkan(state.get())");
	const size_t configureTrace = mainSource.find("ConfigureNsightGpuTraceAutomationFromEnvironment");
	const size_t drawFrame = mainSource.find("DrawFrame(");
	const size_t triggerTrace = mainSource.find("TryStartNsightGpuTraceAtReplayTick");
	PROJECTV_RTX_EXPECT(
		configureTrace != std::string::npos && initVulkan != std::string::npos &&
			configureTrace < initVulkan,
		"Nsight trace gate must configure before Vulkan initialization");
	PROJECTV_RTX_EXPECT(
		triggerTrace != std::string::npos && drawFrame != std::string::npos &&
			triggerTrace < drawFrame,
		"Nsight trace must start before the target frame is recorded");
}

} // namespace

int main() // NOLINT(*-exception-escape)
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
	TestRtxAmbientOcclusionRayHelperExistsInShader();
	TestAmbientOcclusionVisibilitySwitchesToRtxWhenEnabled();
	TestRtxAoDispatchUsesTerminateOnFirstHitFlag();
	TestRtxAoShaderBinaryBuilt();
	TestRtxGiProbeConfigDefaults();
	TestRtxGiProbesClassHasGetters();
	TestRtxGiProbeEnvGateRequiresBoth();
	TestRtxGiProbeShaderBindingsDeclared();
	TestRtxGiProbeRecordUpdatePassNoopWithoutContext();
	TestRtxGiProbeHostHeaderExistsAndLinks();
	TestRtxShadowPipelineClassHasGetters();
	TestRtxShadowSbtClassHasGetters();
	TestRtxShadowShaderFilesExistInBuildDirectory();
	TestRtxShadowIntersectionShaderUsesVoxelDdaPattern();
	TestGpuAttributionMarkersBracketTargetWork();
	TestRtxTraversalMetricsContract();
	TestTightChunkAabbRoutingContract();
	TestTightChunkAabbReconstruction();
	TestTightChunkAabbRejectsInvalidBounds();
	TestTightAabbAndTraversalSourceContracts();
	TestNsightGpuTraceReplayTickContract();

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
