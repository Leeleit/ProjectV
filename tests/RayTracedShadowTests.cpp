// Copyright (c) 2026 ProjectV
// SPDX-License-Identifier: MIT
// Stage 5.2 RTX foundation sub-tests (no Vulkan device required; pure CPU contracts).

#include <cstdlib>
#include <cstring>

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
	const char *previous = std::getenv("PROJECTV_HW_RAY_TRACING");
	unsetenv("PROJECTV_HW_RAY_TRACING");
	PROJECTV_RTX_EXPECT(!projectv::render::IsRayTracedShadowEnabled(), "env gate must be false by default");
	if (previous != nullptr) {
		setenv("PROJECTV_HW_RAY_TRACING", previous, 1);
	}
}

void TestEnvGateOnRespected()
{
	setenv("PROJECTV_HW_RAY_TRACING", "ON", 1);
	PROJECTV_RTX_EXPECT(projectv::render::IsRayTracedShadowEnabled(), "env gate must be true when PROJECTV_HW_RAY_TRACING=ON");
	setenv("PROJECTV_HW_RAY_TRACING", "1", 1);
	PROJECTV_RTX_EXPECT(projectv::render::IsRayTracedShadowEnabled(), "env gate must be true when PROJECTV_HW_RAY_TRACING=1");
	setenv("PROJECTV_HW_RAY_TRACING", "0", 1);
	PROJECTV_RTX_EXPECT(!projectv::render::IsRayTracedShadowEnabled(), "env gate must be false when PROJECTV_HW_RAY_TRACING=0");
	setenv("PROJECTV_HW_RAY_TRACING", "garbage", 1);
	PROJECTV_RTX_EXPECT(!projectv::render::IsRayTracedShadowEnabled(), "env gate must be false for any unrecognised value");
	unsetenv("PROJECTV_HW_RAY_TRACING");
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

void TestBuildChunkBlasGuardsForNullBuffers()
{
	projectv::render::RayTracedShadows shadows{};
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VulkanContextState context{};
	PROJECTV_RTX_EXPECT(!shadows.BuildChunkBlas(
		cmd,
		context,
		0u,
		64u,
		VK_NULL_HANDLE,
		0u,
		VK_NULL_HANDLE,
		0u,
		VK_INDEX_TYPE_UINT32,
		VK_FORMAT_R32G32B32_SFLOAT,
		sizeof(float) * 3u),
		"BuildChunkBlas must reject null vertex/index buffer when enabled is false");
}

void TestSetBlasDirtyQueueAndConsume()
{
	projectv::render::RayTracedShadows shadows{};
	std::vector<uint32_t> dirty{ 0u, 1u, 2u, 3u };
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

}  // namespace

int main()
{
	TestEnvGateDefaultsOff();
	TestEnvGateOnRespected();
	TestConfigDefaultValues();
	TestConfigZeroSizedAfterShutdown();
	TestComputeBlasBuildScratchSize();
	TestBuildChunkBlasGuardsForNullBuffers();
	TestSetBlasDirtyQueueAndConsume();
	TestUpdateTlasRespectsCapacity();

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
