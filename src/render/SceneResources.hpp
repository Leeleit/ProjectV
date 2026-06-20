#pragma once

import projectv.math;
#include "core/Types.hpp"

#include <algorithm>
#include <cmath>

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
[[nodiscard]] inline bool FrustumCullVsCamera(
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
		return (projectv::math::dot(toOrigin, planeNormal) - planeOffset) + projectedRadiusOntoPlane(planeNormal) >= 0.0f;
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
		[&aabbMin, &aabbMax]() {
			return projectv::math::Vec3{
				(aabbMin.x + aabbMax.x) * 0.5f,
				(aabbMin.y + aabbMax.y) * 0.5f,
				(aabbMin.z + aabbMax.z) * 0.5f,
				0.0f,
			};
		},
		[&aabbMin, &aabbMax]() {
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
		[&chunkDescriptor]() {
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
		[&chunkDescriptor]() {
			return projectv::math::Vec3{
				static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[0]) * 0.5f,
				static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[1]) * 0.5f,
				static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[2]) * 0.5f,
				0.0f,
			};
		});
}

inline bool IsSceneChunkVisibleInShadowCascade(
	const PackedSceneChunkDescriptor &chunkDescriptor,
	const projectv::math::Mat4 &lightViewProjection)
{
	if (chunkDescriptor.chunkExtentAndNonAir[3] == 0u) {
		return false;
	}

	const projectv::math::Vec3 chunkMin{
		static_cast<float>(chunkDescriptor.chunkOrigin[0]),
		static_cast<float>(chunkDescriptor.chunkOrigin[1]),
		static_cast<float>(chunkDescriptor.chunkOrigin[2]),
		0.0f,
	};
	const projectv::math::Vec3 chunkMax{
		chunkMin.x + static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[0]),
		chunkMin.y + static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[1]),
		chunkMin.z + static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[2]),
		0.0f,
	};

	bool outsideLeft = true;
	bool outsideRight = true;
	bool outsideBottom = true;
	bool outsideTop = true;
	bool outsideNear = true;
	bool outsideFar = true;
	for (uint32_t cornerIndex = 0; cornerIndex < 8u; ++cornerIndex) {
		const projectv::math::Vec3 corner{
			(cornerIndex & 1u) != 0u ? chunkMax.x : chunkMin.x,
			(cornerIndex & 2u) != 0u ? chunkMax.y : chunkMin.y,
			(cornerIndex & 4u) != 0u ? chunkMax.z : chunkMin.z,
			0.0f,
		};

		const projectv::math::Vec4 clipCorner = lightViewProjection * projectv::math::Vec4{corner.x, corner.y, corner.z, 1.0f};
		outsideLeft = outsideLeft && clipCorner[0] < -clipCorner[3];
		outsideRight = outsideRight && clipCorner[0] > clipCorner[3];
		outsideBottom = outsideBottom && clipCorner[1] < -clipCorner[3];
		outsideTop = outsideTop && clipCorner[1] > clipCorner[3];
		outsideNear = outsideNear && clipCorner[2] < 0.0f;
		outsideFar = outsideFar && clipCorner[2] > clipCorner[3];
		if (!outsideLeft &&
			!outsideRight &&
			!outsideBottom &&
			!outsideTop &&
			!outsideNear &&
			!outsideFar) {
			return true;
		}
	}

	return !(outsideLeft || outsideRight || outsideBottom || outsideTop || outsideNear || outsideFar);
}

bool CreateSceneResources(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render);
bool UpdateSceneResources(
	WorldState *world,
	RenderState *render,
	const ChunkCullingParameters &shadowProjectionParameters,
	VkExtent2D swapchainExtent);
bool UploadSceneFrameResources(
	RenderState &render,
	uint32_t frameIndex);
bool UpdateSceneFrameChunkVisibility(
	RenderState &render,
	uint32_t frameIndex,
	const ChunkCullingParameters &parameters);
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
