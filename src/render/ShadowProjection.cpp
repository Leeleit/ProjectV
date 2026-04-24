#include "render/ShadowProjection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kMinShadowNearPlane = 0.1f;
constexpr float kShadowExtentPadding = 2.0f;
constexpr float kShadowDepthPadding = 8.0f;
constexpr float kMinShadowCoverageScale = 0.5f;
constexpr float kMaxShadowCoverageScale = 3.0f;
constexpr float kMinCascadeNearPlane = 0.01f;
constexpr float kDefaultCascadeNearPlane = 0.1f;
constexpr float kDefaultCascadeFarPlane = 128.0f;
constexpr uint32_t kDefaultShadowMapResolution = 2048u;
constexpr float kDefaultCascadeSplitLambda = 0.80f;

struct Float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

bool TryGetActiveSceneBounds(
	const VoxelWorld &world,
	Float3 &outMin,
	Float3 &outMax)
{
	bool hasActiveChunk = false;
	Float3 activeMin{
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
	};
	Float3 activeMax{
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
	};

	for (const VoxelChunk &chunk : world.chunks) {
		if (chunk.nonAirVoxelCount == 0) {
			continue;
		}

		hasActiveChunk = true;
		activeMin.x = std::min(activeMin.x, static_cast<float>(chunk.min.x));
		activeMin.y = std::min(activeMin.y, static_cast<float>(chunk.min.y));
		activeMin.z = std::min(activeMin.z, static_cast<float>(chunk.min.z));
		activeMax.x = std::max(activeMax.x, static_cast<float>(chunk.maxExclusive.x));
		activeMax.y = std::max(activeMax.y, static_cast<float>(chunk.maxExclusive.y));
		activeMax.z = std::max(activeMax.z, static_cast<float>(chunk.maxExclusive.z));
	}

	if (!hasActiveChunk) {
		return false;
	}

	outMin = activeMin;
	outMax = activeMax;
	return true;
}

Float3 Normalize(const Float3 vector)
{
	const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
	if (length <= 0.00001f) {
		return {0.0f, 1.0f, 0.0f};
	}

	return {vector.x / length, vector.y / length, vector.z / length};
}

Float3 Cross(const Float3 a, const Float3 b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

float Dot(const Float3 a, const Float3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

std::array<float, 16> MultiplyMatrices(
	const std::array<float, 16> &a,
	const std::array<float, 16> &b)
{
	std::array<float, 16> result{};
	for (int column = 0; column < 4; ++column) {
		for (int row = 0; row < 4; ++row) {
			float value = 0.0f;
			for (int index = 0; index < 4; ++index) {
				value += a[index * 4 + row] * b[column * 4 + index];
			}
			result[column * 4 + row] = value;
		}
	}
	return result;
}

Float3 Add(const Float3 a, const Float3 b)
{
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Float3 Subtract(const Float3 a, const Float3 b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Float3 Scale(const Float3 vector, const float scalar)
{
	return {vector.x * scalar, vector.y * scalar, vector.z * scalar};
}

float Length(const Float3 vector)
{
	return std::sqrt(Dot(vector, vector));
}

float SnapToTexelGrid(
	const float value,
	const float texelSize)
{
	if (!std::isfinite(value) || !std::isfinite(texelSize) || texelSize <= 0.0f) {
		return value;
	}
	return std::round(value / texelSize) * texelSize;
}

void StoreCascadeMatrix(
	std::array<float, kSunShadowMatrixElementCount> &target,
	const uint32_t cascadeIndex,
	const std::array<float, 16> &matrix)
{
	const size_t matrixOffset = static_cast<size_t>(cascadeIndex) * matrix.size();
	std::copy_n(matrix.begin(), matrix.size(), target.data() + matrixOffset);
}

std::array<Float3, 8> BuildBoundsCorners(const Float3 minBounds, const Float3 maxBounds)
{
	std::array<Float3, 8> corners{};
	for (uint32_t cornerIndex = 0; cornerIndex < corners.size(); ++cornerIndex) {
		corners[cornerIndex] = {
			(cornerIndex & 1u) != 0u ? maxBounds.x : minBounds.x,
			(cornerIndex & 2u) != 0u ? maxBounds.y : minBounds.y,
			(cornerIndex & 4u) != 0u ? maxBounds.z : minBounds.z,
		};
	}
	return corners;
}

std::array<Float3, 8> BuildFrustumSliceCorners(
	const Float3 cameraPosition,
	const Float3 cameraForward,
	const Float3 cameraRight,
	const Float3 cameraUp,
	const float tanHalfVerticalFov,
	const float tanHalfHorizontalFov,
	const float nearDepth,
	const float farDepth)
{
	std::array<Float3, 8> corners{};
	uint32_t cornerIndex = 0;
	for (const float depth : {nearDepth, farDepth}) {
		const Float3 center = Add(cameraPosition, Scale(cameraForward, depth));
		const Float3 right = Scale(cameraRight, tanHalfHorizontalFov * depth);
		const Float3 up = Scale(cameraUp, tanHalfVerticalFov * depth);
		corners[cornerIndex++] = Add(Add(center, Scale(right, -1.0f)), Scale(up, -1.0f));
		corners[cornerIndex++] = Add(Add(center, right), Scale(up, -1.0f));
		corners[cornerIndex++] = Add(Add(center, Scale(right, -1.0f)), up);
		corners[cornerIndex++] = Add(Add(center, right), up);
	}
	return corners;
}

void ComputeBounds(
	const std::array<Float3, 8> &corners,
	Float3 &outMin,
	Float3 &outMax)
{
	Float3 minBounds{
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
	};
	Float3 maxBounds{
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
	};
	for (const auto &[x, y, z] : corners) {
		minBounds.x = std::min(minBounds.x, x);
		minBounds.y = std::min(minBounds.y, y);
		minBounds.z = std::min(minBounds.z, z);
		maxBounds.x = std::max(maxBounds.x, x);
		maxBounds.y = std::max(maxBounds.y, y);
		maxBounds.z = std::max(maxBounds.z, z);
	}

	outMin = minBounds;
	outMax = maxBounds;
}

void ExpandBoundsUpstreamForShadowCasters(
	Float3 &boundsMin,
	Float3 &boundsMax,
	const Float3 sceneMin,
	const Float3 sceneMax,
	const Float3 lightForward)
{
	const auto expandAxis = [](float &axisMin, float &axisMax, const float sceneAxisMin, const float sceneAxisMax, const float lightAxis) {
		if (lightAxis > 0.0001f) {
			axisMin = sceneAxisMin;
		} else if (lightAxis < -0.0001f) {
			axisMax = sceneAxisMax;
		}

		axisMin = std::clamp(axisMin, sceneAxisMin, sceneAxisMax);
		axisMax = std::clamp(axisMax, sceneAxisMin, sceneAxisMax);
		if (axisMax < axisMin) {
			axisMax = axisMin;
		}
	};

	expandAxis(boundsMin.x, boundsMax.x, sceneMin.x, sceneMax.x, lightForward.x);
	expandAxis(boundsMin.y, boundsMax.y, sceneMin.y, sceneMax.y, lightForward.y);
	expandAxis(boundsMin.z, boundsMax.z, sceneMin.z, sceneMax.z, lightForward.z);
}

float ComputeRequiredProjectedHalfExtent(
	const std::array<Float3, 8> &corners,
	const Float3 axis,
	const float snappedCenterCoordinate,
	const float minimumHalfExtent,
	const float coverageScale)
{
	float requiredHalfExtent = std::max(minimumHalfExtent, 0.5f);
	for (const Float3 corner : corners) {
		const float projectedDistance =
			std::abs(Dot(axis, corner) - snappedCenterCoordinate) + kShadowExtentPadding;
		requiredHalfExtent = std::max(requiredHalfExtent, projectedDistance * coverageScale);
	}
	return requiredHalfExtent;
}

void ComputeRelativeDepthRange(
	const std::array<Float3, 8> &corners,
	const Float3 origin,
	const Float3 axis,
	float &outMinDepth,
	float &outMaxDepth)
{
	for (const Float3 corner : corners) {
		const float relativeDepth = Dot(axis, Subtract(corner, origin));
		outMinDepth = std::min(outMinDepth, relativeDepth);
		outMaxDepth = std::max(outMaxDepth, relativeDepth);
	}
}
} // namespace

SunShadowCascadeSplits BuildSunShadowCascadeSplits(
	const float nearPlane,
	const float farPlane,
	const float splitLambda)
{
	const float safeNear =
		std::isfinite(nearPlane) && nearPlane >= kMinCascadeNearPlane ? nearPlane : kDefaultCascadeNearPlane;
	const float safeFar =
		std::isfinite(farPlane) && farPlane > safeNear + kMinCascadeNearPlane ? farPlane : std::max(kDefaultCascadeFarPlane, safeNear + 1.0f);
	const float clampedLambda = std::clamp(
		std::isfinite(splitLambda) ? splitLambda : kDefaultCascadeSplitLambda,
		0.0f,
		1.0f);
	const float depthRange = safeFar - safeNear;
	const float minDepthStep = std::max(depthRange * 0.0001f, 0.0001f);

	SunShadowCascadeSplits splits{};
	splits.splitLambda = clampedLambda;
	splits.nearPlane = safeNear;
	splits.farPlane = safeFar;

	float previousDepth = safeNear;
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		const float fraction =
			static_cast<float>(cascadeIndex + 1u) / static_cast<float>(kSunShadowCascadeCount);
		const float uniformDepth = safeNear + depthRange * fraction;
		const float logarithmicDepth = safeNear * std::pow(safeFar / safeNear, fraction);
		float splitDepth = logarithmicDepth * clampedLambda + uniformDepth * (1.0f - clampedLambda);
		if (cascadeIndex + 1u == kSunShadowCascadeCount) {
			splitDepth = safeFar;
		} else {
			splitDepth = std::clamp(splitDepth, previousDepth + minDepthStep, safeFar - minDepthStep);
		}

		splits.viewDepthSplits[cascadeIndex] = splitDepth;
		splits.normalizedSplits[cascadeIndex] = (splitDepth - safeNear) / depthRange;
		previousDepth = splitDepth;
	}
	splits.normalizedSplits.back() = 1.0f;
	splits.viewDepthSplits.back() = safeFar;

	return splits;
}

SunShadowProjection BuildSunShadowProjection(
	const VoxelWorld &world,
	const std::array<float, 3> &sunDirection,
	const float coverageScale)
{
	const float clampedCoverageScale = std::clamp(
		coverageScale,
		kMinShadowCoverageScale,
		kMaxShadowCoverageScale);
	// Scene lighting stores the vector toward the sun because the shading pass
	// evaluates N.L against that direction. The shadow camera needs the opposite
	// vector: the direction sunlight actually travels through the scene.
	const Float3 lightForward = Normalize({
		-sunDirection[0],
		-sunDirection[1],
		-sunDirection[2],
	});

	Float3 worldUp{0.0f, 1.0f, 0.0f};
	if (std::abs(Dot(lightForward, worldUp)) > 0.98f) {
		worldUp = {0.0f, 0.0f, 1.0f};
	}

	const Float3 lightRight = Normalize(Cross(lightForward, worldUp));
	const Float3 lightUp = Normalize(Cross(lightRight, lightForward));

	Float3 sceneMin{
		static_cast<float>(world.min.x),
		static_cast<float>(world.min.y),
		static_cast<float>(world.min.z),
	};
	Float3 sceneMax{
		static_cast<float>(world.maxExclusive.x),
		static_cast<float>(world.maxExclusive.y),
		static_cast<float>(world.maxExclusive.z),
	};
	TryGetActiveSceneBounds(world, sceneMin, sceneMax);
	const Float3 sceneCenter{
		(sceneMin.x + sceneMax.x) * 0.5f,
		(sceneMin.y + sceneMax.y) * 0.5f,
		(sceneMin.z + sceneMax.z) * 0.5f,
	};
	const Float3 sceneExtent{
		sceneMax.x - sceneMin.x,
		sceneMax.y - sceneMin.y,
		sceneMax.z - sceneMin.z,
	};
	const float sceneRadius =
		std::sqrt(sceneExtent.x * sceneExtent.x + sceneExtent.y * sceneExtent.y + sceneExtent.z * sceneExtent.z) * 0.5f;
	const Float3 lightPosition{
		sceneCenter.x - lightForward.x * (sceneRadius + kShadowDepthPadding),
		sceneCenter.y - lightForward.y * (sceneRadius + kShadowDepthPadding),
		sceneCenter.z - lightForward.z * (sceneRadius + kShadowDepthPadding),
	};

	const std::array lightView{
		lightRight.x,
		lightUp.x,
		-lightForward.x,
		0.0f,
		lightRight.y,
		lightUp.y,
		-lightForward.y,
		0.0f,
		lightRight.z,
		lightUp.z,
		-lightForward.z,
		0.0f,
		-Dot(lightRight, lightPosition),
		-Dot(lightUp, lightPosition),
		Dot(lightForward, lightPosition),
		1.0f,
	};

	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();
	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::lowest();

	for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
		const Float3 corner{
			(cornerIndex & 1) != 0 ? sceneMax.x : sceneMin.x,
			(cornerIndex & 2) != 0 ? sceneMax.y : sceneMin.y,
			(cornerIndex & 4) != 0 ? sceneMax.z : sceneMin.z,
		};
		const Float3 relativeCorner{
			corner.x - lightPosition.x,
			corner.y - lightPosition.y,
			corner.z - lightPosition.z,
		};
		const float lightX = Dot(lightRight, relativeCorner);
		const float lightY = Dot(lightUp, relativeCorner);
		const float lightZ = Dot(lightForward, relativeCorner);
		minX = std::min(minX, lightX);
		maxX = std::max(maxX, lightX);
		minY = std::min(minY, lightY);
		maxY = std::max(maxY, lightY);
		minZ = std::min(minZ, lightZ);
		maxZ = std::max(maxZ, lightZ);
	}

	minX -= kShadowExtentPadding;
	maxX += kShadowExtentPadding;
	minY -= kShadowExtentPadding;
	maxY += kShadowExtentPadding;
	const float centerX = (minX + maxX) * 0.5f;
	const float centerY = (minY + maxY) * 0.5f;
	const float centerZ = (minZ + maxZ) * 0.5f;
	const float halfExtentX = std::max((maxX - minX) * 0.5f * clampedCoverageScale, 0.5f);
	const float halfExtentY = std::max((maxY - minY) * 0.5f * clampedCoverageScale, 0.5f);
	const float halfExtentZ = std::max((maxZ - minZ) * 0.5f * clampedCoverageScale, 0.5f);
	minX = centerX - halfExtentX;
	maxX = centerX + halfExtentX;
	minY = centerY - halfExtentY;
	maxY = centerY + halfExtentY;
	minZ = centerZ - halfExtentZ;
	maxZ = centerZ + halfExtentZ;

	const float nearPlane = std::max(kMinShadowNearPlane, minZ - kShadowDepthPadding);
	const float farPlane = std::max(nearPlane + 1.0f, maxZ + kShadowDepthPadding);

	const std::array lightProjection{
		2.0f / (maxX - minX),
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		-2.0f / (maxY - minY),
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f / (nearPlane - farPlane),
		0.0f,
		-(maxX + minX) / (maxX - minX),
		-(maxY + minY) / (maxY - minY),
		nearPlane / (nearPlane - farPlane),
		1.0f,
	};

	SunShadowProjection projection{};
	projection.lightViewProjection = MultiplyMatrices(lightProjection, lightView);
	return projection;
}

SunShadowCascadeProjections BuildSunShadowCascadeProjections(
	const VoxelWorld &world,
	const std::array<float, 3> &sunDirection,
	const SunShadowCascadeProjectionInputs &inputs,
	const float coverageScale)
{
	const float clampedCoverageScale = std::clamp(
		coverageScale,
		kMinShadowCoverageScale,
		kMaxShadowCoverageScale);
	const Float3 lightForward = Normalize({
		-sunDirection[0],
		-sunDirection[1],
		-sunDirection[2],
	});

	Float3 worldUp{0.0f, 1.0f, 0.0f};
	if (std::abs(Dot(lightForward, worldUp)) > 0.98f) {
		worldUp = {0.0f, 0.0f, 1.0f};
	}

	const Float3 lightRight = Normalize(Cross(lightForward, worldUp));
	const Float3 lightUp = Normalize(Cross(lightRight, lightForward));
	const Float3 cameraPosition{
		inputs.cameraPosition[0],
		inputs.cameraPosition[1],
		inputs.cameraPosition[2],
	};
	const Float3 cameraForward = Normalize({
		inputs.cameraForward[0],
		inputs.cameraForward[1],
		inputs.cameraForward[2],
	});
	const Float3 cameraRight = Normalize({
		inputs.cameraRight[0],
		inputs.cameraRight[1],
		inputs.cameraRight[2],
	});
	const Float3 cameraUp = Normalize({
		inputs.cameraUp[0],
		inputs.cameraUp[1],
		inputs.cameraUp[2],
	});
	const float tanHalfVerticalFov = std::max(inputs.tanHalfVerticalFov, 0.01f);
	const float tanHalfHorizontalFov = std::max(inputs.tanHalfHorizontalFov, 0.01f);
	const uint32_t shadowMapResolution =
		inputs.shadowMapResolution > 0u ? inputs.shadowMapResolution : kDefaultShadowMapResolution;

	Float3 sceneMin{
		static_cast<float>(world.min.x),
		static_cast<float>(world.min.y),
		static_cast<float>(world.min.z),
	};
	Float3 sceneMax{
		static_cast<float>(world.maxExclusive.x),
		static_cast<float>(world.maxExclusive.y),
		static_cast<float>(world.maxExclusive.z),
	};
	TryGetActiveSceneBounds(world, sceneMin, sceneMax);

	SunShadowCascadeSplits splits = inputs.splits;
	if (splits.viewDepthSplits.back() <= splits.nearPlane) {
		splits = BuildSunShadowCascadeSplits(
			kDefaultCascadeNearPlane,
			kDefaultCascadeFarPlane,
			splits.splitLambda);
	}

	SunShadowCascadeProjections projections{};
	float previousSplitDepth = std::max(splits.nearPlane, kMinCascadeNearPlane);
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		const float cascadeNearDepth = previousSplitDepth;
		const float cascadeFarDepth = std::max(
			splits.viewDepthSplits[cascadeIndex],
			previousSplitDepth + kMinCascadeNearPlane);
		const std::array<Float3, 8> frustumCorners = BuildFrustumSliceCorners(
			cameraPosition,
			cameraForward,
			cameraRight,
			cameraUp,
			tanHalfVerticalFov,
			tanHalfHorizontalFov,
			previousSplitDepth,
			cascadeFarDepth);
		Float3 receiverBoundsMin{};
		Float3 receiverBoundsMax{};
		ComputeBounds(frustumCorners, receiverBoundsMin, receiverBoundsMax);
		Float3 casterBoundsMin = receiverBoundsMin;
		Float3 casterBoundsMax = receiverBoundsMax;
		ExpandBoundsUpstreamForShadowCasters(
			casterBoundsMin,
			casterBoundsMax,
			sceneMin,
			sceneMax,
			lightForward);
		const std::array<Float3, 8> casterCorners = BuildBoundsCorners(casterBoundsMin, casterBoundsMax);

		Float3 cascadeCenter{};
		for (const Float3 corner : frustumCorners) {
			cascadeCenter = Add(cascadeCenter, corner);
		}
		cascadeCenter = Scale(cascadeCenter, 1.0f / static_cast<float>(frustumCorners.size()));

		float cascadeRadius = 0.0f;
		for (const Float3 corner : frustumCorners) {
			cascadeRadius = std::max(cascadeRadius, Length(Subtract(corner, cascadeCenter)));
		}
		cascadeRadius = std::max(cascadeRadius, 0.5f);

		// Use a sphere fit for the receiver slice instead of a tight light-space AABB. It is less
		// aggressive on texel density, but keeps cascade extents stable under small camera rotations
		// and therefore reduces split-edge swimming/shimmer.
		const float cascadeCenterLightX = Dot(lightRight, cascadeCenter);
		const float cascadeCenterLightY = Dot(lightUp, cascadeCenter);
		const float minimumReceiverHalfExtent =
			std::max((cascadeRadius + kShadowExtentPadding) * clampedCoverageScale, 0.5f);
		float halfExtentX = minimumReceiverHalfExtent;
		float halfExtentY = minimumReceiverHalfExtent;
		float texelSizeX = halfExtentX * 2.0f / static_cast<float>(shadowMapResolution);
		float texelSizeY = halfExtentY * 2.0f / static_cast<float>(shadowMapResolution);
		float snappedCenterLightX = SnapToTexelGrid(cascadeCenterLightX, texelSizeX);
		float snappedCenterLightY = SnapToTexelGrid(cascadeCenterLightY, texelSizeY);
		halfExtentX = ComputeRequiredProjectedHalfExtent(
			casterCorners,
			lightRight,
			snappedCenterLightX,
			minimumReceiverHalfExtent,
			clampedCoverageScale);
		halfExtentY = ComputeRequiredProjectedHalfExtent(
			casterCorners,
			lightUp,
			snappedCenterLightY,
			minimumReceiverHalfExtent,
			clampedCoverageScale);
		texelSizeX = halfExtentX * 2.0f / static_cast<float>(shadowMapResolution);
		texelSizeY = halfExtentY * 2.0f / static_cast<float>(shadowMapResolution);
		snappedCenterLightX = SnapToTexelGrid(cascadeCenterLightX, texelSizeX);
		snappedCenterLightY = SnapToTexelGrid(cascadeCenterLightY, texelSizeY);
		halfExtentX = ComputeRequiredProjectedHalfExtent(
			casterCorners,
			lightRight,
			snappedCenterLightX,
			minimumReceiverHalfExtent,
			clampedCoverageScale);
		halfExtentY = ComputeRequiredProjectedHalfExtent(
			casterCorners,
			lightUp,
			snappedCenterLightY,
			minimumReceiverHalfExtent,
			clampedCoverageScale);
		const Float3 snappedCascadeCenter = Add(
			Add(
				cascadeCenter,
				Scale(lightRight, snappedCenterLightX - cascadeCenterLightX)),
			Scale(lightUp, snappedCenterLightY - cascadeCenterLightY));
		float minRelativeLightDepth = std::numeric_limits<float>::max();
		float maxRelativeLightDepth = std::numeric_limits<float>::lowest();
		ComputeRelativeDepthRange(
			frustumCorners,
			snappedCascadeCenter,
			lightForward,
			minRelativeLightDepth,
			maxRelativeLightDepth);
		ComputeRelativeDepthRange(
			casterCorners,
			snappedCascadeCenter,
			lightForward,
			minRelativeLightDepth,
			maxRelativeLightDepth);
		const float lightForwardOffset = std::max(
			cascadeRadius + kShadowDepthPadding,
			kShadowDepthPadding + kMinShadowNearPlane - minRelativeLightDepth);
		const Float3 lightPosition = Subtract(
			snappedCascadeCenter,
			Scale(lightForward, lightForwardOffset));
		const std::array lightView{
			lightRight.x,
			lightUp.x,
			-lightForward.x,
			0.0f,
			lightRight.y,
			lightUp.y,
			-lightForward.y,
			0.0f,
			lightRight.z,
			lightUp.z,
			-lightForward.z,
			0.0f,
			-Dot(lightRight, lightPosition),
			-Dot(lightUp, lightPosition),
			Dot(lightForward, lightPosition),
			1.0f,
		};

		float minZ = std::numeric_limits<float>::max();
		float maxZ = std::numeric_limits<float>::lowest();
		float casterMinZ = std::numeric_limits<float>::max();
		float casterMaxZ = std::numeric_limits<float>::lowest();
		const auto accumulateLightDepth = [&](const Float3 corner) {
			const Float3 relativeCorner = Subtract(corner, lightPosition);
			const float lightZ = Dot(lightForward, relativeCorner);
			minZ = std::min(minZ, lightZ);
			maxZ = std::max(maxZ, lightZ);
		};
		const auto accumulateCasterLightDepth = [&](const Float3 corner) {
			const Float3 relativeCorner = Subtract(corner, lightPosition);
			const float lightZ = Dot(lightForward, relativeCorner);
			casterMinZ = std::min(casterMinZ, lightZ);
			casterMaxZ = std::max(casterMaxZ, lightZ);
			minZ = std::min(minZ, lightZ);
			maxZ = std::max(maxZ, lightZ);
		};
		for (const Float3 corner : frustumCorners) {
			accumulateLightDepth(corner);
		}
		for (const Float3 corner : casterCorners) {
			accumulateCasterLightDepth(corner);
		}
		const float minX = -halfExtentX;
		const float maxX = halfExtentX;
		const float minY = -halfExtentY;
		const float maxY = halfExtentY;

		const float nearPlane = std::max(kMinShadowNearPlane, minZ - kShadowDepthPadding);
		const float farPlane = std::max(nearPlane + 1.0f, maxZ + kShadowDepthPadding);
		const std::array lightProjection{
			2.0f / (maxX - minX),
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			-2.0f / (maxY - minY),
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			1.0f / (nearPlane - farPlane),
			0.0f,
			-(maxX + minX) / (maxX - minX),
			-(maxY + minY) / (maxY - minY),
			nearPlane / (nearPlane - farPlane),
			1.0f,
		};
		StoreCascadeMatrix(
			projections.lightViewProjections,
			cascadeIndex,
			MultiplyMatrices(lightProjection, lightView));
		projections.diagnostics.viewNearDepths[cascadeIndex] = cascadeNearDepth;
		projections.diagnostics.viewFarDepths[cascadeIndex] = cascadeFarDepth;
		projections.diagnostics.orthoWidths[cascadeIndex] = halfExtentX * 2.0f;
		projections.diagnostics.orthoHeights[cascadeIndex] = halfExtentY * 2.0f;
		projections.diagnostics.texelWorldSizes[cascadeIndex] = std::max(texelSizeX, texelSizeY);
		projections.diagnostics.casterLightNearDepths[cascadeIndex] = casterMinZ;
		projections.diagnostics.casterLightFarDepths[cascadeIndex] = casterMaxZ;
		previousSplitDepth = cascadeFarDepth;
	}

	return projections;
}
