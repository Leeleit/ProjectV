#ifndef SCENE_RESOURCES_HPP
#define SCENE_RESOURCES_HPP

#include "core/Types.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

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
	if (chunkDescriptor.chunkExtentAndNonAir[3] == 0u) {
		return false;
	}

	const auto loadFloat3 = [](const std::array<float, 4> &values) {
		return std::array{values[0], values[1], values[2]};
	};
	const auto dot = [](const std::array<float, 3> &a, const std::array<float, 3> &b) {
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	};
	const auto lengthSquared = [&](const std::array<float, 3> &vector) {
		return dot(vector, vector);
	};

	const std::array<float, 3> cameraPosition = loadFloat3(parameters.cameraPositionAndMaxDistance);
	const std::array<float, 3> cameraForward = loadFloat3(parameters.cameraForwardAndTanHalfVerticalFov);
	const std::array<float, 3> cameraRight = loadFloat3(parameters.cameraRightAndTanHalfHorizontalFov);
	const std::array<float, 3> cameraUp = loadFloat3(parameters.cameraUpAndNearPlane);
	const float maxDistance = parameters.cameraPositionAndMaxDistance[3];
	const float tanHalfVerticalFov = std::max(parameters.cameraForwardAndTanHalfVerticalFov[3], 0.0f);
	const float tanHalfHorizontalFov = std::max(parameters.cameraRightAndTanHalfHorizontalFov[3], 0.0f);
	const float nearPlane = std::max(parameters.cameraUpAndNearPlane[3], 0.0f);

	const std::array chunkHalfExtent{
		static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[0]) * 0.5f,
		static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[1]) * 0.5f,
		static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[2]) * 0.5f,
	};
	const std::array chunkCenter{
		static_cast<float>(chunkDescriptor.chunkOrigin[0]) + chunkHalfExtent[0],
		static_cast<float>(chunkDescriptor.chunkOrigin[1]) + chunkHalfExtent[1],
		static_cast<float>(chunkDescriptor.chunkOrigin[2]) + chunkHalfExtent[2],
	};
	const std::array toChunkCenter{
		chunkCenter[0] - cameraPosition[0],
		chunkCenter[1] - cameraPosition[1],
		chunkCenter[2] - cameraPosition[2],
	};
	const auto projectedRadiusOntoPlane = [&](const std::array<float, 3> &planeNormal) {
		return std::abs(planeNormal[0]) * chunkHalfExtent[0] +
			   std::abs(planeNormal[1]) * chunkHalfExtent[1] +
			   std::abs(planeNormal[2]) * chunkHalfExtent[2];
	};
	const auto passesPlane = [&](const std::array<float, 3> &planeNormal, const float planeOffset = 0.0f) {
		const float centerDistance = dot(toChunkCenter, planeNormal) - planeOffset;
		return centerDistance + projectedRadiusOntoPlane(planeNormal) >= 0.0f;
	};
	const float chunkRadius = std::sqrt(lengthSquared(chunkHalfExtent));
	if (!passesPlane(cameraForward, nearPlane)) {
		return false;
	}

	if (maxDistance > 0.0f) {
		const float maxCenterDistance = maxDistance + chunkRadius;
		if (lengthSquared(toChunkCenter) > maxCenterDistance * maxCenterDistance) {
			return false;
		}
	}

	const std::array leftPlane{
		cameraForward[0] * tanHalfHorizontalFov + cameraRight[0],
		cameraForward[1] * tanHalfHorizontalFov + cameraRight[1],
		cameraForward[2] * tanHalfHorizontalFov + cameraRight[2],
	};
	const std::array rightPlane{
		cameraForward[0] * tanHalfHorizontalFov - cameraRight[0],
		cameraForward[1] * tanHalfHorizontalFov - cameraRight[1],
		cameraForward[2] * tanHalfHorizontalFov - cameraRight[2],
	};
	const std::array bottomPlane{
		cameraForward[0] * tanHalfVerticalFov + cameraUp[0],
		cameraForward[1] * tanHalfVerticalFov + cameraUp[1],
		cameraForward[2] * tanHalfVerticalFov + cameraUp[2],
	};
	const std::array topPlane{
		cameraForward[0] * tanHalfVerticalFov - cameraUp[0],
		cameraForward[1] * tanHalfVerticalFov - cameraUp[1],
		cameraForward[2] * tanHalfVerticalFov - cameraUp[2],
	};
	if (!passesPlane(leftPlane) || !passesPlane(rightPlane)) {
		return false;
	}
	if (!passesPlane(bottomPlane) || !passesPlane(topPlane)) {
		return false;
	}

	return true;
}

// M5: world-space AABB vs the same camera frustum that
// `IsSceneChunkVisible` builds. Used by `FramePreparation` to cull
// polygon-model instances per frame without paying the cost of an
// indirect buffer + GPU readback. Planes are constructed inline rather
// than refactoring the chunk helper because the chunk hot path is owned
// by another agent's work-in-progress and the cost of an additional ~30
// lines of math is negligible compared to touching a shared function.
inline bool IsAabbVisibleAgainstCameraFrustum(
	const std::array<float, 3> &aabbMin,
	const std::array<float, 3> &aabbMax,
	const ChunkCullingParameters &parameters)
{
	const auto loadFloat3 = [](const std::array<float, 4> &values) {
		return std::array{values[0], values[1], values[2]};
	};
	const auto dot = [](const std::array<float, 3> &a, const std::array<float, 3> &b) {
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	};
	const auto lengthSquared = [&](const std::array<float, 3> &vector) {
		return dot(vector, vector);
	};

	const std::array<float, 3> cameraPosition = loadFloat3(parameters.cameraPositionAndMaxDistance);
	const std::array<float, 3> cameraForward = loadFloat3(parameters.cameraForwardAndTanHalfVerticalFov);
	const std::array<float, 3> cameraRight = loadFloat3(parameters.cameraRightAndTanHalfHorizontalFov);
	const std::array<float, 3> cameraUp = loadFloat3(parameters.cameraUpAndNearPlane);
	const float maxDistance = parameters.cameraPositionAndMaxDistance[3];
	const float tanHalfVerticalFov = std::max(parameters.cameraForwardAndTanHalfVerticalFov[3], 0.0f);
	const float tanHalfHorizontalFov = std::max(parameters.cameraRightAndTanHalfHorizontalFov[3], 0.0f);
	const float nearPlane = std::max(parameters.cameraUpAndNearPlane[3], 0.0f);

	const std::array aabbCenter{
		(aabbMin[0] + aabbMax[0]) * 0.5f,
		(aabbMin[1] + aabbMax[1]) * 0.5f,
		(aabbMin[2] + aabbMax[2]) * 0.5f,
	};
	const std::array aabbHalfExtent{
		(aabbMax[0] - aabbMin[0]) * 0.5f,
		(aabbMax[1] - aabbMin[1]) * 0.5f,
		(aabbMax[2] - aabbMin[2]) * 0.5f,
	};
	const std::array toAabbCenter{
		aabbCenter[0] - cameraPosition[0],
		aabbCenter[1] - cameraPosition[1],
		aabbCenter[2] - cameraPosition[2],
	};
	const auto projectedRadiusOntoPlane = [&](const std::array<float, 3> &planeNormal) {
		return std::abs(planeNormal[0]) * aabbHalfExtent[0] +
			   std::abs(planeNormal[1]) * aabbHalfExtent[1] +
			   std::abs(planeNormal[2]) * aabbHalfExtent[2];
	};
	const auto passesPlane = [&](const std::array<float, 3> &planeNormal, const float planeOffset = 0.0f) {
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

	const std::array leftPlane{
		cameraForward[0] * tanHalfHorizontalFov + cameraRight[0],
		cameraForward[1] * tanHalfHorizontalFov + cameraRight[1],
		cameraForward[2] * tanHalfHorizontalFov + cameraRight[2],
	};
	const std::array rightPlane{
		cameraForward[0] * tanHalfHorizontalFov - cameraRight[0],
		cameraForward[1] * tanHalfHorizontalFov - cameraRight[1],
		cameraForward[2] * tanHalfHorizontalFov - cameraRight[2],
	};
	const std::array bottomPlane{
		cameraForward[0] * tanHalfVerticalFov + cameraUp[0],
		cameraForward[1] * tanHalfVerticalFov + cameraUp[1],
		cameraForward[2] * tanHalfVerticalFov + cameraUp[2],
	};
	const std::array topPlane{
		cameraForward[0] * tanHalfVerticalFov - cameraUp[0],
		cameraForward[1] * tanHalfVerticalFov - cameraUp[1],
		cameraForward[2] * tanHalfVerticalFov - cameraUp[2],
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
	const std::array<float, 16> &lightViewProjection)
{
	if (chunkDescriptor.chunkExtentAndNonAir[3] == 0u) {
		return false;
	}

	const std::array chunkMin{
		static_cast<float>(chunkDescriptor.chunkOrigin[0]),
		static_cast<float>(chunkDescriptor.chunkOrigin[1]),
		static_cast<float>(chunkDescriptor.chunkOrigin[2]),
	};
	const std::array chunkMax{
		chunkMin[0] + static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[0]),
		chunkMin[1] + static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[1]),
		chunkMin[2] + static_cast<float>(chunkDescriptor.chunkExtentAndNonAir[2]),
	};

	// Two valid contracts feed this culling, both rely on `clip.z < 0` for the
	// near-plane test:
	//   1. The real shadow projection, which uses an inverted light-space Z
	//      (`lightView` row 2 has `-lightForward.xyz`, and the orthographic
	//      `m[2][2] = 1/(near - far)` is negative). For points in front of the
	//      light `clip.z > 0`; for points *behind* the near plane `clip.z < 0`.
	//   2. A vanilla identity projection (used in unit tests). In that case
	//      `clip.z = corner.z` and standard NDC applies, so `clip.z < 0` culls
	//      behind the near plane as expected.
	// Both contracts want the same predicate here, so we keep it. A near-only
	// visibility test was previously proposed and removed because it read as
	// a no-op under the inverted-Z contract, but the analysis was wrong: the
	// inversion is in `lightView`'s row 2 only, *not* in the resulting `clip.z`
	// sign. Removing the test regresses `TestIsSceneChunkVisibleInShadowCascade`
	// (line 2157 of `tests/VoxelWorldTests.cpp`), which feeds an identity
	// projection and expects a chunk at z=-2..-1 to be culled by the near plane.
	bool outsideLeft = true;
	bool outsideRight = true;
	bool outsideBottom = true;
	bool outsideTop = true;
	bool outsideNear = true;
	bool outsideFar = true;
	for (uint32_t cornerIndex = 0; cornerIndex < 8u; ++cornerIndex) {
		const std::array corner{
			(cornerIndex & 1u) != 0u ? chunkMax[0] : chunkMin[0],
			(cornerIndex & 2u) != 0u ? chunkMax[1] : chunkMin[1],
			(cornerIndex & 4u) != 0u ? chunkMax[2] : chunkMin[2],
		};
		const std::array clipCorner{
			lightViewProjection[0] * corner[0] + lightViewProjection[4] * corner[1] + lightViewProjection[8] * corner[2] + lightViewProjection[12],
			lightViewProjection[1] * corner[0] + lightViewProjection[5] * corner[1] + lightViewProjection[9] * corner[2] + lightViewProjection[13],
			lightViewProjection[2] * corner[0] + lightViewProjection[6] * corner[1] + lightViewProjection[10] * corner[2] + lightViewProjection[14],
			lightViewProjection[3] * corner[0] + lightViewProjection[7] * corner[1] + lightViewProjection[11] * corner[2] + lightViewProjection[15],
		};
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
	const VkExtent2D swapchainExtent);
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

// **Two-level chunk visibility cache, 2026-06-12.** The hash
// used by `ChunkVisibilityCache::valid` to decide between
// cache-hit `memcpy` and full per-chunk rebuild. Quantization
// thresholds are picked to keep a static-camera look-dev /
// replay run entirely off the cull critical path:
//   * camera position → 0.25 voxel units (so a single-voxel
//     move always invalidates),
//   * camera forward → 0.005 (~0.3° steps) so sub-1° rotations
//     also invalidate.
// The hash is the XOR-fold of 6 quantized camera ints + the
// world payload version + the chunk descriptor count. Stored
// on `ChunkVisibilityCache` so the next frame can compare
// without recomputing.
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

	// splitmix64-style fold. The exact constants don't matter
	// for correctness — only that (a) the hash is
	// deterministic and (b) the bits of each component get
	// mixed into the high bits, so a 1-bit change in any input
	// flips roughly half the hash bits.
	uint64_t hash = static_cast<uint64_t>(posX) * 0x9E3779B185EBCA87ULL;
	hash ^= static_cast<uint64_t>(posY) * 0xC2B2AE3D27D4EB4FULL;
	hash ^= static_cast<uint64_t>(posZ) * 0x165667B19E3779F9ULL;
	hash ^= static_cast<uint64_t>(fwdX) * 0x94D049BB133111EBULL;
	hash ^= static_cast<uint64_t>(fwdY) * 0xD1342543DE82EF95ULL;
	hash ^= static_cast<uint64_t>(fwdZ) * 0xB45BCA9F4D2D9B33ULL;
	hash ^= sceneVoxelPayloadVersion * 0x27D4EB2F165667C5ULL;
	hash ^= static_cast<uint64_t>(chunkDescriptorCount) * 0x9C2A8E3F4D2D9B3BULL;

	// Final avalanche. Same mix as splitmix64.
	hash ^= hash >> 30;
	hash *= 0xBF58476D1CE4E5B9ULL;
	hash ^= hash >> 27;
	hash *= 0x94D049BB133111EBULL;
	hash ^= hash >> 31;
	return hash;
}
} // namespace projectv::visibility_cache

#endif
