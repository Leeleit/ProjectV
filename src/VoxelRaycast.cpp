#include "VoxelRaycast.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kEpsilon = 0.0001f;
constexpr float kDirectionEpsilon = 0.00001f;

struct Float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

Float3 Normalize(const Float3 vector)
{
	const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
	if (length <= kDirectionEpsilon) {
		return {};
	}

	return {
		vector.x / length,
		vector.y / length,
		vector.z / length,
	};
}

bool IsZeroVector(const Float3 vector)
{
	return std::abs(vector.x) <= kDirectionEpsilon &&
		   std::abs(vector.y) <= kDirectionEpsilon &&
		   std::abs(vector.z) <= kDirectionEpsilon;
}

Float3 MakeFloat3(const std::array<float, 3> &vector)
{
	return {vector[0], vector[1], vector[2]};
}

Float3 AddScaled(const Float3 origin, const Float3 direction, const float distance)
{
	return {
		origin.x + direction.x * distance,
		origin.y + direction.y * distance,
		origin.z + direction.z * distance,
	};
}

Int3 FloorToVoxel(const Float3 position)
{
	return {
		static_cast<int>(std::floor(position.x)),
		static_cast<int>(std::floor(position.y)),
		static_cast<int>(std::floor(position.z)),
	};
}

bool IntersectRayAabb(
	const Float3 origin,
	const Float3 direction,
	const Float3 boundsMin,
	const Float3 boundsMax,
	float *outEntryDistance,
	float *outExitDistance)
{
	float entryDistance = 0.0f;
	float exitDistance = std::numeric_limits<float>::infinity();

	const auto intersectAxis = [&](const float originAxis, const float directionAxis, const float minAxis, const float maxAxis) {
		if (std::abs(directionAxis) <= kDirectionEpsilon) {
			return originAxis >= minAxis && originAxis <= maxAxis;
		}

		float axisEntry = (minAxis - originAxis) / directionAxis;
		float axisExit = (maxAxis - originAxis) / directionAxis;
		if (axisEntry > axisExit) {
			std::swap(axisEntry, axisExit);
		}

		entryDistance = std::max(entryDistance, axisEntry);
		exitDistance = std::min(exitDistance, axisExit);
		return exitDistance >= entryDistance;
	};

	if (!intersectAxis(origin.x, direction.x, boundsMin.x, boundsMax.x) ||
		!intersectAxis(origin.y, direction.y, boundsMin.y, boundsMax.y) ||
		!intersectAxis(origin.z, direction.z, boundsMin.z, boundsMax.z)) {
		return false;
	}

	if (outEntryDistance) {
		*outEntryDistance = entryDistance;
	}
	if (outExitDistance) {
		*outExitDistance = exitDistance;
	}
	return true;
}

float ComputeAxisStepDistance(const float sampleAxis, const int voxelAxis, const float directionAxis, const int step)
{
	if (step > 0) {
		return (static_cast<float>(voxelAxis + 1) - sampleAxis) / directionAxis;
	}
	if (step < 0) {
		return (sampleAxis - static_cast<float>(voxelAxis)) / -directionAxis;
	}
	return std::numeric_limits<float>::infinity();
}

float ComputeAxisDeltaDistance(const float directionAxis, const int step)
{
	if (step == 0) {
		return std::numeric_limits<float>::infinity();
	}
	return 1.0f / std::abs(directionAxis);
}

bool IsSolidMaterial(const VoxelMaterial material)
{
	return material != VoxelMaterial::Air;
}
} // namespace

VoxelRaycastHit RaycastVoxelWorld(
	const VoxelWorld &world,
	const std::array<float, 3> &origin,
	const std::array<float, 3> &direction,
	const float maxDistance)
{
	VoxelRaycastHit result{};
	if (maxDistance <= 0.0f) {
		return result;
	}

	const Float3 rayOrigin = MakeFloat3(origin);
	const Float3 rayDirection = Normalize(MakeFloat3(direction));
	if (IsZeroVector(rayDirection)) {
		return result;
	}

	float entryDistance = 0.0f;
	float exitDistance = 0.0f;
	if (!IntersectRayAabb(
			rayOrigin,
			rayDirection,
			{static_cast<float>(world.min.x), static_cast<float>(world.min.y), static_cast<float>(world.min.z)},
			{static_cast<float>(world.maxExclusive.x), static_cast<float>(world.maxExclusive.y), static_cast<float>(world.maxExclusive.z)},
			&entryDistance,
			&exitDistance)) {
		return result;
	}

	if (exitDistance < 0.0f || entryDistance > maxDistance) {
		return result;
	}

	const float startDistance = std::max(entryDistance, 0.0f);
	const float traceLimit = std::min(exitDistance, maxDistance);
	float sampleDistance = startDistance;
	if (startDistance > 0.0f && startDistance + kEpsilon < traceLimit) {
		sampleDistance += kEpsilon;
	}

	const Float3 sampleOrigin = AddScaled(rayOrigin, rayDirection, sampleDistance);
	Int3 currentVoxel = FloorToVoxel(sampleOrigin);
	if (!IsInsideVoxelWorld(world, currentVoxel)) {
		return result;
	}

	VoxelMaterial material = GetVoxelMaterial(world, currentVoxel);
	if (IsSolidMaterial(material)) {
		result.hasHit = true;
		result.voxel = currentVoxel;
		result.material = material;
		result.distance = startDistance;
		return result;
	}

	const int stepX = rayDirection.x > kDirectionEpsilon ? 1 : rayDirection.x < -kDirectionEpsilon ? -1 : 0;
	const int stepY = rayDirection.y > kDirectionEpsilon ? 1 : rayDirection.y < -kDirectionEpsilon ? -1 : 0;
	const int stepZ = rayDirection.z > kDirectionEpsilon ? 1 : rayDirection.z < -kDirectionEpsilon ? -1 : 0;

	float tMaxX = ComputeAxisStepDistance(sampleOrigin.x, currentVoxel.x, rayDirection.x, stepX);
	float tMaxY = ComputeAxisStepDistance(sampleOrigin.y, currentVoxel.y, rayDirection.y, stepY);
	float tMaxZ = ComputeAxisStepDistance(sampleOrigin.z, currentVoxel.z, rayDirection.z, stepZ);
	const float tDeltaX = ComputeAxisDeltaDistance(rayDirection.x, stepX);
	const float tDeltaY = ComputeAxisDeltaDistance(rayDirection.y, stepY);
	const float tDeltaZ = ComputeAxisDeltaDistance(rayDirection.z, stepZ);

	while (true) {
		Int3 previousVoxel = currentVoxel;
		Int3 hitNormal{};
		float localDistance = tMaxX;

		if (tMaxY < localDistance) {
			localDistance = tMaxY;
		}
		if (tMaxZ < localDistance) {
			localDistance = tMaxZ;
		}

		if (startDistance + localDistance > traceLimit + kEpsilon) {
			break;
		}

		if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
			currentVoxel.x += stepX;
			tMaxX += tDeltaX;
			hitNormal = {-stepX, 0, 0};
		} else if (tMaxY <= tMaxZ) {
			currentVoxel.y += stepY;
			tMaxY += tDeltaY;
			hitNormal = {0, -stepY, 0};
		} else {
			currentVoxel.z += stepZ;
			tMaxZ += tDeltaZ;
			hitNormal = {0, 0, -stepZ};
		}

		if (!IsInsideVoxelWorld(world, currentVoxel)) {
			break;
		}

		material = GetVoxelMaterial(world, currentVoxel);
		if (!IsSolidMaterial(material)) {
			continue;
		}

		result.hasHit = true;
		result.voxel = currentVoxel;
		result.material = material;
		result.hitNormal = hitNormal;
		result.distance = startDistance + localDistance;
		if (IsInsideVoxelWorld(world, previousVoxel)) {
			result.hasPlacementVoxel = true;
			result.placementVoxel = previousVoxel;
		}
		return result;
	}

	return result;
}
