import projectv.math;
import projectv.string_id;

#include "app/ModelGravigun.hpp"

#include "app/Camera.hpp"
#include "app/InputActions.hpp"
#include "asset/ModelManifestLoader.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace projectv::app {

namespace {

struct Ray {
	glm::vec3 origin;
	glm::vec3 direction; // unit length
};

bool RayAabbIntersect(const Ray &ray, const glm::vec3 &aabbMin, const glm::vec3 &aabbMax, float &outTNear, float &outTFar)
{
	float tNear = -std::numeric_limits<float>::infinity();
	float tFar = std::numeric_limits<float>::infinity();
	for (int axis = 0; axis < 3; ++axis) {
		const float origin = ray.origin[axis];
		const float dir = ray.direction[axis];
		const float min = aabbMin[axis];
		const float max = aabbMax[axis];
		if (std::abs(dir) < 1e-6f) {
			if (origin < min || origin > max) {
				return false;
			}
			continue;
		}
		float t1 = (min - origin) / dir;
		float t2 = (max - origin) / dir;
		if (t1 > t2) {
			std::swap(t1, t2);
		}
		tNear = std::max(tNear, t1);
		tFar = std::min(tFar, t2);
		if (tNear > tFar) {
			return false;
		}
	}
	if (tFar < 0.0f) {
		return false;
	}
	outTNear = std::max(0.0f, tNear);
	outTFar = tFar;
	return true;
}

std::optional<glm::vec3> IntersectRayHorizontalPlane(const Ray &ray, const float planeY)
{
	if (std::abs(ray.direction.y) < 1e-6f) {
		return std::nullopt;
	}
	const float t = (planeY - ray.origin.y) / ray.direction.y;
	if (t < 0.0f) {
		return std::nullopt;
	}
	return ray.origin + t * ray.direction;
}

Ray BuildCameraRay(const CameraState &camera)
{
	const std::array<float, 3> forward = GetCameraForwardVector(camera);
	Ray ray;
	ray.origin = glm::vec3(camera.position[0], camera.position[1], camera.position[2]);
	ray.direction = glm::normalize(glm::vec3(forward[0], forward[1], forward[2]));
	return ray;
}

bool GravigunSnapEnabled()
{
	static const bool kSnapEnabled = [] {
		const char *value = std::getenv("PROJECTV_GRAVIGUN_SNAP");
		if (value == nullptr) {
			return false;
		}
		const std::string v(value);
		return v == "on" || v == "1" || v == "true";
	}();
	return kSnapEnabled;
}

} // namespace

void TickModelGravigun(
	ModelGravigunState *state,
	const VoxelWorld &world,
	const CameraState &camera,
	const VkExtent2D extent,
	RenderState *render,
	InputState *input)
{
	(void)extent;
	(void)world;
	if (!state || !render || !input) {
		return;
	}

	const bool fDown = IsInputActionDown(*input, InputAction::PickModel);
	const bool fPressed = ConsumeInputActionPressed(*input, InputAction::PickModel);

	if (fPressed && state->pickedInstanceIndex < 0) {

		const Ray ray = BuildCameraRay(camera);
		bool hasBest = false;
		size_t bestIndex = 0;
		float bestTNear = 0.0f;
		for (size_t i = 0; i < render->modelInstances.size(); ++i) {
			const ModelInstanceData &inst = render->modelInstances[i];
			if (inst.indexCount == 0) {
				continue;
			}
			float tNear = 0.0f;
			float tFar = 0.0f;
			// noinspection DfaConstantConditions, DfaUnreachableCode
			if (RayAabbIntersect(ray,
								 glm::vec3(inst.worldAabbMin[0], inst.worldAabbMin[1], inst.worldAabbMin[2]),
								 glm::vec3(inst.worldAabbMax[0], inst.worldAabbMax[1], inst.worldAabbMax[2]),
								 tNear, tFar)) {
				if (!hasBest || tNear < bestTNear) {
					bestTNear = tNear;
					bestIndex = i;
					hasBest = true;
				}
			}
		}
		// noinspection DfaConstantConditions
		if (hasBest) {
			state->pickedInstanceIndex = static_cast<int>(bestIndex);

			state->targetY = 0.0f;
			const ModelInstanceData &inst = render->modelInstances[bestIndex];

			state->pickAnchorAabbMin = glm::vec3(
				inst.worldAabbMin[0], inst.worldAabbMin[1], inst.worldAabbMin[2]);
			const std::optional<glm::vec3> anchorHit =
				IntersectRayHorizontalPlane(ray, state->targetY);
			if (anchorHit.has_value()) {
				state->pickAnchorHit = *anchorHit;
			} else {

				state->pickAnchorHit = state->pickAnchorAabbMin;
			}
			std::fprintf(stderr,
						 "[Gravigun-DBG] PICKED: index=%zu aabbMin=(%.3f,%.3f,%.3f) aabbMax=(%.3f,%.3f,%.3f) anchorHit=(%.3f,%.3f,%.3f) targetY=%.1f\n",
						 bestIndex,
						 inst.worldAabbMin[0], inst.worldAabbMin[1], inst.worldAabbMin[2],
						 inst.worldAabbMax[0], inst.worldAabbMax[1], inst.worldAabbMax[2],
						 state->pickAnchorHit.x, state->pickAnchorHit.y, state->pickAnchorHit.z,
						 state->targetY);
		} else {
			std::fprintf(stderr, "[Gravigun-DBG] no model under crosshair\n");
		}
	}

	if (state->pickedInstanceIndex >= 0 && !fDown) {
		const bool snapOnDrop = GravigunSnapEnabled();
		if (snapOnDrop) {
			asset::SnapModelInstancesAboveGround(world, render);
		}
		const ModelInstanceData &inst = render->modelInstances[state->pickedInstanceIndex];
		std::fprintf(stderr,
					 "[Gravigun-DBG] DROPPED: index=%d final_aabbMin=(%.3f,%.3f,%.3f) final_aabbMax=(%.3f,%.3f,%.3f) manifest_position=(%.3f,%.3f,%.3f) targetY=%.1f snap=%s\n",
					 state->pickedInstanceIndex,
					 inst.worldAabbMin.x, inst.worldAabbMin.y, inst.worldAabbMin.z,
					 inst.worldAabbMax.x, inst.worldAabbMax.y, inst.worldAabbMax.z,
					 inst.modelTransform.c[3].x, inst.modelTransform.c[3].y, inst.modelTransform.c[3].z,
					 state->targetY,
					 snapOnDrop ? "on" : "off");
		state->pickedInstanceIndex = -1;
		state->targetY = 0.0f;
		state->pickAnchorAabbMin = glm::vec3(0.0f);
		state->pickAnchorHit = glm::vec3(0.0f);
	}

	if (state->pickedInstanceIndex >= 0 && fDown) {
		const Ray ray = BuildCameraRay(camera);
		const std::optional<glm::vec3> hit = IntersectRayHorizontalPlane(ray, state->targetY);
		if (hit.has_value()) {
			ModelInstanceData &inst = render->modelInstances[state->pickedInstanceIndex];
			const float dimX = inst.worldAabbMax[0] - inst.worldAabbMin[0];
			const float dimY = inst.worldAabbMax[1] - inst.worldAabbMin[1];
			const float dimZ = inst.worldAabbMax[2] - inst.worldAabbMin[2];
			const float deltaX = hit->x - state->pickAnchorHit.x;
			const float deltaZ = hit->z - state->pickAnchorHit.z;

			const bool snapOnDrag = GravigunSnapEnabled();
			const float rawMinX = state->pickAnchorAabbMin.x + deltaX;
			const float rawMinZ = state->pickAnchorAabbMin.z + deltaZ;
			const float newMinX = snapOnDrag ? std::round(rawMinX) : rawMinX;
			const float newMinY = state->targetY;
			const float newMinZ = snapOnDrag ? std::round(rawMinZ) : rawMinZ;
			inst.worldAabbMin[0] = newMinX;
			inst.worldAabbMin[1] = newMinY;
			inst.worldAabbMin[2] = newMinZ;
			inst.worldAabbMax[0] = newMinX + dimX;
			inst.worldAabbMax[1] = newMinY + dimY;
			inst.worldAabbMax[2] = newMinZ + dimZ;

			inst.modelTransform.c[3].x = newMinX - inst.sourceAabbMin[0];
			inst.modelTransform.c[3].y = newMinY - inst.sourceAabbMin[1];
			inst.modelTransform.c[3].z = newMinZ - inst.sourceAabbMin[2];
		}
	}
}

} // namespace projectv::app
