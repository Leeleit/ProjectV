import projectv.math;

#include "render/ShadowProjection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

namespace {
constexpr float kMinShadowNearPlane = 0.1f;				// EVIL: 10cm near plane floor; below this triggers perspective divide singularity
constexpr float kShadowExtentPadding = 2.0f;			// EVIL: 2-voxel extent padding for shadow coverage; wider → wasted texels, narrower → culling pop
constexpr float kShadowDepthPadding = 8.0f;				// EVIL: 8-voxel depth padding; matches far plane precision vs near plane precision asymmetry
constexpr float kMinShadowCoverageScale = 0.5f;			// EVIL: 0.5x coverage floor; lower → texel starvation, higher → wasted GPU memory
constexpr float kMaxShadowCoverageScale = 3.0f;			// EVIL: 3x coverage ceiling; per legacy/docs/standards/04_evil-hacks-philosophy.md >3x destabilizes cascade splits
constexpr float kMinCascadeNearPlane = 0.01f;			// EVIL: 1cm absolute floor; below triggers Vulkan validation layer Z-fighting
constexpr float kDefaultCascadeNearPlane = 0.1f;		// EVIL: 10cm default; matches typical voxel game camera near plane
constexpr float kDefaultCascadeFarPlane = 128.0f;		// EVIL: 128-voxel default; matches TODO §4.3 draw distance baseline
constexpr uint32_t kDefaultShadowMapResolution = 2048u; // EVIL: 2048² default; per decisions.md §15 baseline CSM resolution, do not retune casually
constexpr float kDefaultCascadeSplitLambda = 0.80f;		// EVIL: 0.80 split lambda; per agent/knowledge.md §15 mid-late 2025 retune (was 0.65)

using Float3 = projectv::math::Vec3;

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
		0.0f,
	};
	Float3 activeMax{
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		0.0f,
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
	const projectv::math::Mat4 &matrix)
{
	const size_t matrixOffset = static_cast<size_t>(cascadeIndex) * 16u;
	std::memcpy(target.data() + matrixOffset, matrix.data(), sizeof(projectv::math::Mat4));
}

std::array<Float3, 8> BuildBoundsCorners(const Float3 minBounds, const Float3 maxBounds)
{
	std::array<Float3, 8> corners{};
	for (uint32_t cornerIndex = 0; cornerIndex < corners.size(); ++cornerIndex) {
		corners[cornerIndex] = Float3{
			(cornerIndex & 1u) != 0u ? maxBounds.x : minBounds.x,
			(cornerIndex & 2u) != 0u ? maxBounds.y : minBounds.y,
			(cornerIndex & 4u) != 0u ? maxBounds.z : minBounds.z,
			0.0f,
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
		const Float3 center = cameraPosition + cameraForward * depth;
		const Float3 right = cameraRight * (tanHalfHorizontalFov * depth);
		const Float3 up = cameraUp * (tanHalfVerticalFov * depth);
		corners[cornerIndex++] = center - right - up;
		corners[cornerIndex++] = center + right - up;
		corners[cornerIndex++] = center - right + up;
		corners[cornerIndex++] = center + right + up;
	}
	return corners;
}

void ComputeBounds(
	const std::span<const Float3> corners,
	Float3 &outMin,
	Float3 &outMax)
{
	Float3 minBounds{
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		0.0f,
	};
	Float3 maxBounds{
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		0.0f,
	};
	for (const Float3 &corner : corners) {
		minBounds.x = std::min(minBounds.x, corner.x);
		minBounds.y = std::min(minBounds.y, corner.y);
		minBounds.z = std::min(minBounds.z, corner.z);
		maxBounds.x = std::max(maxBounds.x, corner.x);
		maxBounds.y = std::max(maxBounds.y, corner.y);
		maxBounds.z = std::max(maxBounds.z, corner.z);
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
	const std::span<const Float3> corners,
	const Float3 axis,
	const float snappedCenterCoordinate,
	const float minimumHalfExtent,
	const float coverageScale)
{
	float requiredHalfExtent = std::max(minimumHalfExtent, 0.5f);
	for (const Float3 &corner : corners) {
		const float projectedDistance =
			std::abs(projectv::math::dot(axis, corner) - snappedCenterCoordinate) + kShadowExtentPadding;
		requiredHalfExtent = std::max(requiredHalfExtent, projectedDistance * coverageScale);
	}
	return requiredHalfExtent;
}

void ComputeRelativeDepthRange(
	const std::span<const Float3> corners,
	const Float3 origin,
	const Float3 axis,
	float &outMinDepth,
	float &outMaxDepth)
{
	for (const Float3 &corner : corners) {
		const float relativeDepth = projectv::math::dot(axis, corner - origin);
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

	const Float3 lightForward = projectv::math::normalize(Float3{
		-sunDirection[0],
		-sunDirection[1],
		-sunDirection[2],
		0.0f,
	});
	Float3 worldUp{0.0f, 1.0f, 0.0f};
	if (std::abs(projectv::math::dot(lightForward, worldUp)) > 0.98f) {
		worldUp = {0.0f, 0.0f, 1.0f};
	}

	const Float3 lightRight = projectv::math::normalize(projectv::math::cross(lightForward, worldUp));
	const Float3 lightUp = projectv::math::normalize(projectv::math::cross(lightRight, lightForward));

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
	const float sceneDiagonalSquared =
		sceneExtent.x * sceneExtent.x + sceneExtent.y * sceneExtent.y + sceneExtent.z * sceneExtent.z;
	// Guard: even though sceneExtent components are non-negative (subtraction order), floating-point
	// could in theory produce negative underflow. Clamp to 0 to avoid NaN from sqrt.
	const float sceneRadius =
		std::sqrt(std::max(0.0f, sceneDiagonalSquared)) * 0.5f;
	const Float3 lightPosition{
		sceneCenter.x - lightForward.x * (sceneRadius + kShadowDepthPadding),
		sceneCenter.y - lightForward.y * (sceneRadius + kShadowDepthPadding),
		sceneCenter.z - lightForward.z * (sceneRadius + kShadowDepthPadding),
	};

	projectv::math::Mat4 lightView{};
	lightView.c[0] = projectv::math::Vec4{lightRight.x, lightUp.x, -lightForward.x, 0.0f};
	lightView.c[1] = projectv::math::Vec4{lightRight.y, lightUp.y, -lightForward.y, 0.0f};
	lightView.c[2] = projectv::math::Vec4{lightRight.z, lightUp.z, -lightForward.z, 0.0f};
	lightView.c[3] = projectv::math::Vec4{
		-projectv::math::dot(lightRight, lightPosition),
		-projectv::math::dot(lightUp, lightPosition),
		projectv::math::dot(lightForward, lightPosition),
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
		const float lightX = projectv::math::dot(lightRight, relativeCorner);
		const float lightY = projectv::math::dot(lightUp, relativeCorner);
		const float lightZ = projectv::math::dot(lightForward, relativeCorner);
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

	projectv::math::Mat4 lightProjection{};
	lightProjection.c[0] = projectv::math::Vec4{2.0f / (maxX - minX), 0.0f, 0.0f, 0.0f};
	lightProjection.c[1] = projectv::math::Vec4{0.0f, -2.0f / (maxY - minY), 0.0f, 0.0f};
	lightProjection.c[2] = projectv::math::Vec4{0.0f, 0.0f, 1.0f / (nearPlane - farPlane), 0.0f};
	lightProjection.c[3] = projectv::math::Vec4{
		-(maxX + minX) / (maxX - minX),
		-(maxY + minY) / (maxY - minY),
		nearPlane / (nearPlane - farPlane),
		1.0f,
	};

	SunShadowProjection projection{};
	projection.lightViewProjection = lightProjection * lightView;
	return projection;
}

struct CascadeCameraFrame {
	Float3 position;
	Float3 forward;
	Float3 right;
	Float3 up;
	float tanHalfVerticalFov;
	float tanHalfHorizontalFov;
};

CascadeCameraFrame BuildCascadeCameraFrame(
	const SunShadowCascadeProjectionInputs &inputs,
	uint32_t &outShadowMapResolution)
{
	CascadeCameraFrame frame{};
	frame.position = Float3{
		inputs.cameraPosition[0],
		inputs.cameraPosition[1],
		inputs.cameraPosition[2],
	};
	frame.forward = projectv::math::normalize(Float3{
		inputs.cameraForward[0],
		inputs.cameraForward[1],
		inputs.cameraForward[2],
	});
	frame.right = projectv::math::normalize(Float3{
		inputs.cameraRight[0],
		inputs.cameraRight[1],
		inputs.cameraRight[2],
		0.0f,
	});
	frame.up = projectv::math::normalize(Float3{
		inputs.cameraUp.x,
		inputs.cameraUp.y,
		inputs.cameraUp.z,
		0.0f,
	});
	frame.tanHalfVerticalFov = std::max(inputs.tanHalfVerticalFov, 0.01f);
	frame.tanHalfHorizontalFov = std::max(inputs.tanHalfHorizontalFov, 0.01f);
	outShadowMapResolution =
		inputs.shadowMapResolution > 0u ? inputs.shadowMapResolution : kDefaultShadowMapResolution;
	return frame;
}

struct CascadeBuildContext {
	const CascadeCameraFrame &camera;
	uint32_t shadowMapResolution;
	Float3 sceneMin;
	Float3 sceneMax;
	Float3 lightForward;
	Float3 lightRight;
	Float3 lightUp;
	float clampedCoverageScale;
};

void BuildSingleCascade(
	SunShadowCascadeProjections &projections,
	const uint32_t cascadeIndex,
	const float cascadeNearDepth,
	const float cascadeFarDepth,
	const CascadeBuildContext &ctx)
{
	const std::array<Float3, 8> frustumCorners = BuildFrustumSliceCorners(
		ctx.camera.position,
		ctx.camera.forward,
		ctx.camera.right,
		ctx.camera.up,
		ctx.camera.tanHalfVerticalFov,
		ctx.camera.tanHalfHorizontalFov,
		cascadeNearDepth,
		cascadeFarDepth);
	Float3 receiverBoundsMin{};
	Float3 receiverBoundsMax{};
	ComputeBounds(frustumCorners, receiverBoundsMin, receiverBoundsMax);
	Float3 casterBoundsMin = receiverBoundsMin;
	Float3 casterBoundsMax = receiverBoundsMax;
	ExpandBoundsUpstreamForShadowCasters(
		casterBoundsMin,
		casterBoundsMax,
		ctx.sceneMin,
		ctx.sceneMax,
		ctx.lightForward);
	const std::array<Float3, 8> casterCorners = BuildBoundsCorners(casterBoundsMin, casterBoundsMax);

	Float3 cascadeCenter{};
	for (const Float3 &corner : frustumCorners) {
		cascadeCenter = cascadeCenter + corner;
	}
	cascadeCenter = cascadeCenter * (1.0f / static_cast<float>(frustumCorners.size()));

	float cascadeRadius = 0.0f;
	for (const Float3 &corner : frustumCorners) {
		cascadeRadius = std::max(cascadeRadius, projectv::math::length(corner - cascadeCenter));
	}
	cascadeRadius = std::max(cascadeRadius, 0.5f);

	const float cascadeCenterLightX = projectv::math::dot(ctx.lightRight, cascadeCenter);
	const float cascadeCenterLightY = projectv::math::dot(ctx.lightUp, cascadeCenter);
	const float minimumReceiverHalfExtent =
		std::max((cascadeRadius + kShadowExtentPadding) * ctx.clampedCoverageScale, 0.5f);
	float halfExtentX = minimumReceiverHalfExtent;
	float halfExtentY = minimumReceiverHalfExtent;
	float texelSizeX = halfExtentX * 2.0f / static_cast<float>(ctx.shadowMapResolution);
	float texelSizeY = halfExtentY * 2.0f / static_cast<float>(ctx.shadowMapResolution);
	float snappedCenterLightX = SnapToTexelGrid(cascadeCenterLightX, texelSizeX);
	float snappedCenterLightY = SnapToTexelGrid(cascadeCenterLightY, texelSizeY);
	halfExtentX = ComputeRequiredProjectedHalfExtent(
		casterCorners,
		ctx.lightRight,
		snappedCenterLightX,
		minimumReceiverHalfExtent,
		ctx.clampedCoverageScale);
	halfExtentY = ComputeRequiredProjectedHalfExtent(
		casterCorners,
		ctx.lightUp,
		snappedCenterLightY,
		minimumReceiverHalfExtent,
		ctx.clampedCoverageScale);
	texelSizeX = halfExtentX * 2.0f / static_cast<float>(ctx.shadowMapResolution);
	texelSizeY = halfExtentY * 2.0f / static_cast<float>(ctx.shadowMapResolution);
	snappedCenterLightX = SnapToTexelGrid(cascadeCenterLightX, texelSizeX);
	snappedCenterLightY = SnapToTexelGrid(cascadeCenterLightY, texelSizeY);
	halfExtentX = ComputeRequiredProjectedHalfExtent(
		casterCorners,
		ctx.lightRight,
		snappedCenterLightX,
		minimumReceiverHalfExtent,
		ctx.clampedCoverageScale);
	halfExtentY = ComputeRequiredProjectedHalfExtent(
		casterCorners,
		ctx.lightUp,
		snappedCenterLightY,
		minimumReceiverHalfExtent,
		ctx.clampedCoverageScale);
	const Float3 snappedCascadeCenter = cascadeCenter + ctx.lightRight * (snappedCenterLightX - cascadeCenterLightX) + ctx.lightUp * (snappedCenterLightY - cascadeCenterLightY);
	float minRelativeLightDepth = std::numeric_limits<float>::max();
	float maxRelativeLightDepth = std::numeric_limits<float>::lowest();
	ComputeRelativeDepthRange(
		frustumCorners,
		snappedCascadeCenter,
		ctx.lightForward,
		minRelativeLightDepth,
		maxRelativeLightDepth);
	ComputeRelativeDepthRange(
		casterCorners,
		snappedCascadeCenter,
		ctx.lightForward,
		minRelativeLightDepth,
		maxRelativeLightDepth);
	const float lightForwardOffset = std::max(
		cascadeRadius + kShadowDepthPadding,
		kShadowDepthPadding + kMinShadowNearPlane - minRelativeLightDepth);
	const Float3 lightPosition = snappedCascadeCenter - ctx.lightForward * lightForwardOffset;
	projectv::math::Mat4 lightView{};
	lightView.c[0] = projectv::math::Vec4{ctx.lightRight.x, ctx.lightUp.x, -ctx.lightForward.x, 0.0f};
	lightView.c[1] = projectv::math::Vec4{ctx.lightRight.y, ctx.lightUp.y, -ctx.lightForward.y, 0.0f};
	lightView.c[2] = projectv::math::Vec4{ctx.lightRight.z, ctx.lightUp.z, -ctx.lightForward.z, 0.0f};
	lightView.c[3] = projectv::math::Vec4{
		-projectv::math::dot(ctx.lightRight, lightPosition),
		-projectv::math::dot(ctx.lightUp, lightPosition),
		projectv::math::dot(ctx.lightForward, lightPosition),
		1.0f,
	};

	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::lowest();
	float casterMinZ = std::numeric_limits<float>::max();
	float casterMaxZ = std::numeric_limits<float>::lowest();
	const auto accumulateLightDepth = [&](const Float3 corner) {
		const Float3 relativeCorner = corner - lightPosition;
		const float lightZ = projectv::math::dot(ctx.lightForward, relativeCorner);
		minZ = std::min(minZ, lightZ);
		maxZ = std::max(maxZ, lightZ);
	};
	const auto accumulateCasterLightDepth = [&](const Float3 corner) {
		const Float3 relativeCorner = corner - lightPosition;
		const float lightZ = projectv::math::dot(ctx.lightForward, relativeCorner);
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
	projectv::math::Mat4 lightProjection{};
	lightProjection.c[0] = projectv::math::Vec4{2.0f / (maxX - minX), 0.0f, 0.0f, 0.0f};
	lightProjection.c[1] = projectv::math::Vec4{0.0f, -2.0f / (maxY - minY), 0.0f, 0.0f};
	lightProjection.c[2] = projectv::math::Vec4{0.0f, 0.0f, 1.0f / (nearPlane - farPlane), 0.0f};
	lightProjection.c[3] = projectv::math::Vec4{
		-(maxX + minX) / (maxX - minX),
		-(maxY + minY) / (maxY - minY),
		nearPlane / (nearPlane - farPlane),
		1.0f,
	};
	StoreCascadeMatrix(
		projections.lightViewProjections,
		cascadeIndex,
		lightProjection * lightView);
	projections.diagnostics.viewNearDepths[cascadeIndex] = cascadeNearDepth;
	projections.diagnostics.viewFarDepths[cascadeIndex] = cascadeFarDepth;
	projections.diagnostics.orthoWidths[cascadeIndex] = halfExtentX * 2.0f;
	projections.diagnostics.orthoHeights[cascadeIndex] = halfExtentY * 2.0f;
	projections.diagnostics.texelWorldSizes[cascadeIndex] = std::max(texelSizeX, texelSizeY);
	projections.diagnostics.casterLightNearDepths[cascadeIndex] = casterMinZ;
	projections.diagnostics.casterLightFarDepths[cascadeIndex] = casterMaxZ;
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
	const Float3 lightForward = projectv::math::normalize(Float3{
		-sunDirection[0],
		-sunDirection[1],
		-sunDirection[2],
		0.0f,
	});
	Float3 worldUp{0.0f, 1.0f, 0.0f};
	if (std::abs(projectv::math::dot(lightForward, worldUp)) > 0.98f) {
		worldUp = {0.0f, 0.0f, 1.0f};
	}

	const Float3 lightRight = projectv::math::normalize(projectv::math::cross(lightForward, worldUp));
	const Float3 lightUp = projectv::math::normalize(projectv::math::cross(lightRight, lightForward));
	uint32_t shadowMapResolution = 0;
	const CascadeCameraFrame cameraFrame = BuildCascadeCameraFrame(inputs, shadowMapResolution);

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

	const CascadeBuildContext ctx{
		cameraFrame,
		shadowMapResolution,
		sceneMin,
		sceneMax,
		lightForward,
		lightRight,
		lightUp,
		clampedCoverageScale,
	};

	SunShadowCascadeProjections projections{};
	float previousSplitDepth = std::max(splits.nearPlane, kMinCascadeNearPlane);
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		const float cascadeNearDepth = previousSplitDepth;
		const float cascadeFarDepth = std::max(
			splits.viewDepthSplits[cascadeIndex],
			previousSplitDepth + kMinCascadeNearPlane);
		BuildSingleCascade(projections, cascadeIndex, cascadeNearDepth, cascadeFarDepth, ctx);
		previousSplitDepth = cascadeFarDepth;
	}

	return projections;
}
