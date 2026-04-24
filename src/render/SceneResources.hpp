#ifndef SCENE_RESOURCES_HPP
#define SCENE_RESOURCES_HPP

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
	const ChunkCullingParameters &shadowProjectionParameters);
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

#endif
