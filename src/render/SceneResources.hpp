#pragma once

#include "core/Math.hpp"
#include "core/Types.hpp"
#include "voxel/NanoVdb.hpp"

#include <algorithm>
#include <cmath>
#include <span>

inline PackedSceneChunkDescriptor MakeUploadedSceneChunkDescriptor(
	const PackedSceneChunkDescriptor &sourceDescriptor,
	const PackedSceneChunkDescriptor *existingDescriptor = nullptr)
{
	PackedSceneChunkDescriptor uploadedDescriptor = sourceDescriptor;
	if (existingDescriptor) {
		uploadedDescriptor.drawRanges[1] = existingDescriptor->drawRanges[1];
		uploadedDescriptor.drawRanges[3] = existingDescriptor->drawRanges[3];
	}
	return uploadedDescriptor;
}

template <typename GetOrigin, typename GetHalfExtent>
[[nodiscard]] bool FrustumCullVsCamera(
	const ChunkCullingParameters &parameters,
	GetOrigin &&getOrigin,
	GetHalfExtent &&getHalfExtent)
{
	const projectv::math::Vec3 cameraPosition{
		parameters.cameraPositionAndMaxDistance.x,
		parameters.cameraPositionAndMaxDistance.y,
		parameters.cameraPositionAndMaxDistance.z,
		0.0f,
	};
	const projectv::math::Vec3 cameraForward{
		parameters.cameraForwardAndTanHalfVerticalFov.x,
		parameters.cameraForwardAndTanHalfVerticalFov.y,
		parameters.cameraForwardAndTanHalfVerticalFov.z,
		0.0f,
	};
	const projectv::math::Vec3 cameraRight{
		parameters.cameraRightAndTanHalfHorizontalFov.x,
		parameters.cameraRightAndTanHalfHorizontalFov.y,
		parameters.cameraRightAndTanHalfHorizontalFov.z,
		0.0f,
	};
	const projectv::math::Vec3 cameraUp{
		parameters.cameraUpAndNearPlane.x,
		parameters.cameraUpAndNearPlane.y,
		parameters.cameraUpAndNearPlane.z,
		0.0f,
	};
	const float maxDistance = parameters.cameraPositionAndMaxDistance.w;
	const float tanHalfVerticalFov = std::max(parameters.cameraForwardAndTanHalfVerticalFov.w, 0.0f);
	const float tanHalfHorizontalFov = std::max(parameters.cameraRightAndTanHalfHorizontalFov.w, 0.0f);
	const float nearPlane = std::max(parameters.cameraUpAndNearPlane.w, 0.0f);
	[[assume(nearPlane >= 0.0f)]];
	[[assume(tanHalfVerticalFov >= 0.0f)]];
	[[assume(tanHalfHorizontalFov >= 0.0f)]];

	const projectv::math::Vec3 origin = getOrigin();
	const projectv::math::Vec3 halfExtent = getHalfExtent();
	const projectv::math::Vec3 toOrigin{
		origin.x - cameraPosition.x,
		origin.y - cameraPosition.y,
		origin.z - cameraPosition.z,
		0.0f,
	};
	const auto projectedRadiusOntoPlane = [&halfExtent](const projectv::math::Vec3 &planeNormal) {
		return std::abs(planeNormal.x) * halfExtent.x +
			   std::abs(planeNormal.y) * halfExtent.y +
			   std::abs(planeNormal.z) * halfExtent.z;
	};
	const auto passesPlane = [&](const projectv::math::Vec3 &planeNormal, const float planeOffset = 0.0f) {
		return projectv::math::dot(toOrigin, planeNormal) - planeOffset + projectedRadiusOntoPlane(planeNormal) >= 0.0f;
	};

	if (!passesPlane(cameraForward, nearPlane)) [[unlikely]] {
		return false;
	}

	if (maxDistance > 0.0f) [[likely]] {
		const float halfExtentRadius = std::sqrt(projectv::math::lengthSq(halfExtent));
		const float maxCenterDistance = maxDistance + halfExtentRadius;
		if (projectv::math::lengthSq(toOrigin) > maxCenterDistance * maxCenterDistance) [[unlikely]] {
			return false;
		}
	}

	const projectv::math::Vec3 leftPlane{
		cameraForward.x * tanHalfHorizontalFov + cameraRight.x,
		cameraForward.y * tanHalfHorizontalFov + cameraRight.y,
		cameraForward.z * tanHalfHorizontalFov + cameraRight.z,
		0.0f,
	};
	const projectv::math::Vec3 rightPlane{
		cameraForward.x * tanHalfHorizontalFov - cameraRight.x,
		cameraForward.y * tanHalfHorizontalFov - cameraRight.y,
		cameraForward.z * tanHalfHorizontalFov - cameraRight.z,
		0.0f,
	};
	const projectv::math::Vec3 bottomPlane{
		cameraForward.x * tanHalfVerticalFov + cameraUp.x,
		cameraForward.y * tanHalfVerticalFov + cameraUp.y,
		cameraForward.z * tanHalfVerticalFov + cameraUp.z,
		0.0f,
	};
	const projectv::math::Vec3 topPlane{
		cameraForward.x * tanHalfVerticalFov - cameraUp.x,
		cameraForward.y * tanHalfVerticalFov - cameraUp.y,
		cameraForward.z * tanHalfVerticalFov - cameraUp.z,
		0.0f,
	};
	if (!passesPlane(leftPlane) || !passesPlane(rightPlane)) [[unlikely]] {
		return false;
	}
	if (!passesPlane(bottomPlane) || !passesPlane(topPlane)) [[unlikely]] {
		return false;
	}

	return true;
}

inline bool IsAabbVisibleAgainstCameraFrustum(
	const projectv::math::Vec3 &aabbMin,
	const projectv::math::Vec3 &aabbMax,
	const ChunkCullingParameters &parameters)
{
	return FrustumCullVsCamera(
		parameters,
		[&aabbMin, &aabbMax] {
			return projectv::math::Vec3{
				(aabbMin.x + aabbMax.x) * 0.5f,
				(aabbMin.y + aabbMax.y) * 0.5f,
				(aabbMin.z + aabbMax.z) * 0.5f,
				0.0f,
			};
		},
		[&aabbMin, &aabbMax] {
			return projectv::math::Vec3{
				(aabbMax.x - aabbMin.x) * 0.5f,
				(aabbMax.y - aabbMin.y) * 0.5f,
				(aabbMax.z - aabbMin.z) * 0.5f,
				0.0f,
			};
		});
}

inline bool IsSceneChunkVisible(
	const PackedSceneChunkDescriptor &chunkDescriptor,
	const ChunkCullingParameters &parameters)
{
	if (chunkDescriptor.chunkExtentAndNonAir[3] == 0u) [[unlikely]] {
		return false;
	}
	[[assume(chunkDescriptor.chunkExtentAndNonAir[3] > 0u)]];

	return FrustumCullVsCamera(
		parameters,
		[&chunkDescriptor] {
			return projectv::math::Vec3{
				static_cast<float>(chunkDescriptor.chunkOrigin[0]) +
					static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[0]) * 0.5f,
				static_cast<float>(chunkDescriptor.chunkOrigin[1]) +
					static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[1]) * 0.5f,
				static_cast<float>(chunkDescriptor.chunkOrigin[2]) +
					static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[2]) * 0.5f,
				0.0f,
			};
		},
		[&chunkDescriptor] {
			return projectv::math::Vec3{
				static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[0]) * 0.5f,
				static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[1]) * 0.5f,
				static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[2]) * 0.5f,
				0.0f,
			};
		});
}

bool CreateSceneResources(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render);
bool UpdateSceneResources(
	WorldState *world,
	RenderState *render,
	const ChunkCullingParameters &chunkCullingParameters,
	VkExtent2D swapchainExtent);
bool UploadSceneFrameResources(
	VulkanContextState *context,
	RenderState &render,
	uint32_t frameIndex);
bool UpdateSceneFrameChunkVisibility(
	RenderState &render,
	uint32_t frameIndex,
	const ChunkCullingParameters &parameters);
bool RefreshChunkAabbBuffer(
	std::span<const VoxelChunk> chunks,
	std::span<const PackedSceneChunkDescriptor> descriptors,
	SceneFrameResources &frameResources);
bool RefreshNanoVdbFlattenBuffers(
	const projectv::voxel::nanovdb::NanoVdbFlattenResult &flatten,
	SceneFrameResources &frameResources);
uint64_t ComputeGrownNanoVdbCapacity(uint64_t currentCapacityBytes, uint64_t requiredCapacityBytes);
bool GrowNanoVdbBuffer(
	VulkanContextState *context,
	RenderState &render,
	uint32_t currentFrameIndex,
	VkBuffer &buffer,
	VmaAllocation &allocation,
	void *&mappedData,
	uint64_t &capacityBytes,
	uint64_t newCapacityBytes,
	const char *profilingTag);
void EnqueueDeferredNanoVdbDestroy(
	RenderState &render,
	uint32_t frameIndex,
	VkBuffer buffer,
	VmaAllocation allocation);
void DrainDeferredNanoVdbDestroysForFrame(
	VulkanContextState *context,
	RenderState &render,
	uint32_t frameIndex);
void DrainAllDeferredNanoVdbDestroys(
	VulkanContextState *context,
	RenderState &render);
void DestroySceneResources(
	VulkanContextState *context,
	RenderState *render);

namespace projectv::visibility_cache {
constexpr float kCameraPositionQuantization = 0.25f;
constexpr float kCameraForwardQuantization = 0.005f;

inline int32_t QuantizeCameraPositionComponent(const float value)
{
	return static_cast<int32_t>(std::floor(value / kCameraPositionQuantization));
}

inline int32_t QuantizeCameraForwardComponent(const float value)
{
	const float clamped = std::clamp(value, -1.0f, 1.0f);
	return static_cast<int32_t>(std::lround(clamped / kCameraForwardQuantization));
}
inline uint64_t ComputeVisibilityCacheHash(
	const ChunkCullingParameters &parameters,
	const uint64_t sceneVoxelPayloadVersion,
	const uint32_t chunkDescriptorCount)
{
	uint64_t hash = static_cast<uint64_t>(QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[0])) * 0x9E3779B185EBCA87ULL;
	hash ^= static_cast<uint64_t>(QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[1])) * 0xC2B2AE3D27D4EB4FULL;
	hash ^= static_cast<uint64_t>(QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[2])) * 0x165667B19E3779F9ULL;
	hash ^= static_cast<uint64_t>(QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[0])) * 0x94D049BB133111EBULL;
	hash ^= static_cast<uint64_t>(QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[1])) * 0xD1342543DE82EF95ULL;
	hash ^= static_cast<uint64_t>(QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[2])) * 0xB45BCA9F4D2D9B33ULL;
	hash ^= sceneVoxelPayloadVersion * 0x27D4EB2F165667C5ULL;
	hash ^= static_cast<uint64_t>(chunkDescriptorCount) * 0x9C2A8E3F4D2D9B3BULL;

	hash ^= hash >> 30;
	hash *= 0xBF58476D1CE4E5B9ULL;
	hash ^= hash >> 27;
	hash *= 0x94D049BB133111EBULL;
	hash ^= hash >> 31;
	return hash;
}
} // namespace projectv::visibility_cache
