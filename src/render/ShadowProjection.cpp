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

struct Float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

bool TryGetActiveSceneBounds(
	const VoxelWorld &world,
	Float3 *outMin,
	Float3 *outMax)
{
	if (!outMin || !outMax) {
		return false;
	}

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

	*outMin = activeMin;
	*outMax = activeMax;
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
} // namespace

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
	TryGetActiveSceneBounds(world, &sceneMin, &sceneMax);
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

	const std::array<float, 16> lightView{
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

	const std::array<float, 16> lightProjection{
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
