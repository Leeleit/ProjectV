#pragma once

#include "core/Math.hpp"
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

inline bool IsSceneChunkVisible(
	const PackedSceneChunkDescriptor &chunkDescriptor,
	const ChunkCullingParameters &parameters)
{
	if (chunkDescriptor.chunkExtentAndNonAir[3] == 0u) [[unlikely]] {
		return false;
	}

	const auto loadFloat3 = [](const projectv::math::Vec4 &values) {
		return projectv::math::Vec3{values.x, values.y, values.z, 0.0f};
	};
	const auto dot = [](const projectv::math::Vec3 &a, const projectv::math::Vec3 &b) {
		return projectv::math::dot(a, b);
	};
	const auto lengthSquared = [&](const projectv::math::Vec3 &vector) {
		return projectv::math::lengthSq(vector);
	};

	const projectv::math::Vec3 cameraPosition = loadFloat3(parameters.cameraPositionAndMaxDistance);
	const projectv::math::Vec3 cameraForward = loadFloat3(parameters.cameraForwardAndTanHalfVerticalFov);
	const projectv::math::Vec3 cameraRight = loadFloat3(parameters.cameraRightAndTanHalfHorizontalFov);
	const projectv::math::Vec3 cameraUp = loadFloat3(parameters.cameraUpAndNearPlane);
	const float maxDistance = parameters.cameraPositionAndMaxDistance.w;
	const float tanHalfVerticalFov = std::max(parameters.cameraForwardAndTanHalfVerticalFov.w, 0.0f);
	const float tanHalfHorizontalFov = std::max(parameters.cameraRightAndTanHalfHorizontalFov.w, 0.0f);
	const float nearPlane = std::max(parameters.cameraUpAndNearPlane.w, 0.0f);

	const projectv::math::Vec3 chunkHalfExtent{
		static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[0]) * 0.5f,
		static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[1]) * 0.5f,
		static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[2]) * 0.5f,
		0.0f,
	};
	const projectv::math::Vec3 chunkCenter{
		static_cast<float>(chunkDescriptor.chunkOrigin[0]) + chunkHalfExtent.x,
		static_cast<float>(chunkDescriptor.chunkOrigin[1]) + chunkHalfExtent.y,
		static_cast<float>(chunkDescriptor.chunkOrigin[2]) + chunkHalfExtent.z,
		0.0f,
	};
	const projectv::math::Vec3 toChunkCenter{
		chunkCenter.x - cameraPosition.x,
		chunkCenter.y - cameraPosition.y,
		chunkCenter.z - cameraPosition.z,
		0.0f,
	};
	const auto projectedRadiusOntoPlane = [&](const projectv::math::Vec3 &planeNormal) {
		return std::abs(planeNormal.x) * chunkHalfExtent.x +
			   std::abs(planeNormal.y) * chunkHalfExtent.y +
			   std::abs(planeNormal.z) * chunkHalfExtent.z;
	};
	const auto passesPlane = [&](const projectv::math::Vec3 &planeNormal, const float planeOffset = 0.0f) {
		const float centerDistance = dot(toChunkCenter, planeNormal) - planeOffset;
		return centerDistance + projectedRadiusOntoPlane(planeNormal) >= 0.0f;
	};
	const float chunkRadius = std::sqrt(lengthSquared(chunkHalfExtent));
	if (!passesPlane(cameraForward, nearPlane)) [[unlikely]] {
		return false;
	}

	if (maxDistance > 0.0f) {
		const float maxCenterDistance = maxDistance + chunkRadius;
		/// \brief `[[unlikely]]` on the max-distance cull.
		///
		/// \details
		///  Chunks within 200 world units of the camera

		///  are by far the dominant case; the cull only

		///  fires for chunks the user has loaded far

		///  past the visible frustum.

		if (lengthSquared(toChunkCenter) > maxCenterDistance * maxCenterDistance) [[unlikely]] {
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

/// \brief M5:
///
/// \details
/// world-space AABB vs the same camera frustum that
///  `IsSceneChunkVisible` builds. Used by `FramePreparation` to cull

///  polygon-model instances per frame without paying the cost of an

///  indirect buffer + GPU readback. Planes are constructed inline rather

///  than refactoring the chunk helper because the chunk hot path is owned

///  by another agent's work-in-progress and the cost of an additional ~30

///  lines of math is negligible compared to touching a shared function.

inline bool IsAabbVisibleAgainstCameraFrustum(
	const projectv::math::Vec3 &aabbMin,
	const projectv::math::Vec3 &aabbMax,
	const ChunkCullingParameters &parameters)
{
	const auto loadFloat3 = [](const projectv::math::Vec4 &values) {
		return projectv::math::Vec3{values.x, values.y, values.z, 0.0f};
	};
	const auto dot = [](const projectv::math::Vec3 &a, const projectv::math::Vec3 &b) {
		return projectv::math::dot(a, b);
	};
	const auto lengthSquared = [&](const projectv::math::Vec3 &vector) {
		return projectv::math::lengthSq(vector);
	};

	const projectv::math::Vec3 cameraPosition = loadFloat3(parameters.cameraPositionAndMaxDistance);
	const projectv::math::Vec3 cameraForward = loadFloat3(parameters.cameraForwardAndTanHalfVerticalFov);
	const projectv::math::Vec3 cameraRight = loadFloat3(parameters.cameraRightAndTanHalfHorizontalFov);
	const projectv::math::Vec3 cameraUp = loadFloat3(parameters.cameraUpAndNearPlane);
	const float maxDistance = parameters.cameraPositionAndMaxDistance.w;
	const float tanHalfVerticalFov = std::max(parameters.cameraForwardAndTanHalfVerticalFov.w, 0.0f);
	const float tanHalfHorizontalFov = std::max(parameters.cameraRightAndTanHalfHorizontalFov.w, 0.0f);
	const float nearPlane = std::max(parameters.cameraUpAndNearPlane.w, 0.0f);

	const projectv::math::Vec3 aabbCenter{
		(aabbMin.x + aabbMax.x) * 0.5f,
		(aabbMin.y + aabbMax.y) * 0.5f,
		(aabbMin.z + aabbMax.z) * 0.5f,
		0.0f,
	};
	const projectv::math::Vec3 aabbHalfExtent{
		(aabbMax.x - aabbMin.x) * 0.5f,
		(aabbMax.y - aabbMin.y) * 0.5f,
		(aabbMax.z - aabbMin.z) * 0.5f,
		0.0f,
	};
	const projectv::math::Vec3 toAabbCenter{
		aabbCenter.x - cameraPosition.x,
		aabbCenter.y - cameraPosition.y,
		aabbCenter.z - cameraPosition.z,
		0.0f,
	};
	const auto projectedRadiusOntoPlane = [&](const projectv::math::Vec3 &planeNormal) {
		return std::abs(planeNormal.x) * aabbHalfExtent.x +
			   std::abs(planeNormal.y) * aabbHalfExtent.y +
			   std::abs(planeNormal.z) * aabbHalfExtent.z;
	};
	const auto passesPlane = [&](const projectv::math::Vec3 &planeNormal, const float planeOffset = 0.0f) {
		const float centerDistance = dot(toAabbCenter, planeNormal) - planeOffset;
		return centerDistance + projectedRadiusOntoPlane(planeNormal) >= 0.0f;
	};
	const float aabbRadius = std::sqrt(lengthSquared(aabbHalfExtent));
	if (!passesPlane(cameraForward, nearPlane)) {
		return false;
	}

	if (maxDistance > 0.0f) {
		const float maxCenterDistance = maxDistance + aabbRadius;
		if (lengthSquared(toAabbCenter) > maxCenterDistance * maxCenterDistance) {
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
	if (!passesPlane(leftPlane) || !passesPlane(rightPlane)) {
		return false;
	}
	if (!passesPlane(bottomPlane) || !passesPlane(topPlane)) {
		return false;
	}

	return true;
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

	/// \brief Two valid contracts feed this culling, both rely on `clip.z < 0` for the
	///
	/// \details
	///  near-plane test:

	///    1. The real shadow projection, which uses an inverted light-space Z

	///       (`lightView` row 2 has `-lightForward.xyz`, and the orthographic

	///       `m[2][2] = 1/(near - far)` is negative). For points in front of the

	///       light `clip.z > 0`; for points *behind* the near plane `clip.z < 0`.

	///    2. A vanilla identity projection (used in unit tests). In that case

	///       `clip.z = corner.z` and standard NDC applies, so `clip.z < 0` culls

	///       behind the near plane as expected.

	///  Both contracts want the same predicate here, so we keep it. A near-only

	///  visibility test was previously proposed and removed because it read as

	///  a no-op under the inverted-Z contract, but the analysis was wrong: the

	///  inversion is in `lightView`'s row 2 only, *not* in the resulting `clip.z`

	///  sign. Removing the test regresses `TestIsSceneChunkVisibleInShadowCascade`

	///  (line 2157 of `tests/VoxelWorldTests.cpp`), which feeds an identity

	///  projection and expects a chunk at z=-2..-1 to be culled by the near plane.

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
		/// \brief Column-major mat4 * homogeneous vec4 (w=1.0).
		///
		/// \details
		///  `math::operator*(Mat4, Vec4)` matches the original

		///  `std::array` formula `m[col*4+row] * v[col]` (which is

		///  the standard column-major mat*vec contract).

		const projectv::math::Vec4 clipCorner = lightViewProjection
			* projectv::math::Vec4{corner.x, corner.y, corner.z, 1.0f};
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
	const auto posX = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[0]);
	const auto posY = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[1]);
	const auto posZ = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[2]);
	const auto fwdX = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[0]);
	const auto fwdY = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[1]);
	const auto fwdZ = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[2]);

	/// \brief splitmix64-style fold.
	///
	/// \details
	/// The exact constants don't matter
	///  for correctness — only that (a) the hash is

	///  deterministic and (b) the bits of each component get

	///  mixed into the high bits, so a 1-bit change in any input

	///  flips roughly half the hash bits.

	uint64_t hash = static_cast<uint64_t>(posX) * 0x9E3779B185EBCA87ULL;
	hash ^= static_cast<uint64_t>(posY) * 0xC2B2AE3D27D4EB4FULL;
	hash ^= static_cast<uint64_t>(posZ) * 0x165667B19E3779F9ULL;
	hash ^= static_cast<uint64_t>(fwdX) * 0x94D049BB133111EBULL;
	hash ^= static_cast<uint64_t>(fwdY) * 0xD1342543DE82EF95ULL;
	hash ^= static_cast<uint64_t>(fwdZ) * 0xB45BCA9F4D2D9B33ULL;
	hash ^= sceneVoxelPayloadVersion * 0x27D4EB2F165667C5ULL;
	hash ^= static_cast<uint64_t>(chunkDescriptorCount) * 0x9C2A8E3F4D2D9B3BULL;

	/// \brief Final avalanche.
	///
	/// \details
	/// Same mix as splitmix64.
	hash ^= hash >> 30;
	hash *= 0xBF58476D1CE4E5B9ULL;
	hash ^= hash >> 27;
	hash *= 0x94D049BB133111EBULL;
	hash ^= hash >> 31;
	return hash;
}
} // namespace projectv::visibility_cache

