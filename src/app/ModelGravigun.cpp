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

/// \brief Camera ray:
///
/// \details
/// position = camera.position, direction = normalised
///  forward vector. (We shoot from the camera's eye, not the

///  crosshair — the FOV is wide enough that a single ray per

///  model is enough to pick.)

struct Ray {
	glm::vec3 origin;
	glm::vec3 direction; // unit length
};

/// \brief Returns the entry (t_near) and exit (t_far) intersection
///
/// \details
///  parameters of a ray with an AABB. Returns false if the ray

///  misses the AABB. (Standard slab test.)

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

/// \brief Intersect a ray with a horizontal plane at y=planeY.
///
/// \details
/// Returns
///  the intersection point or std::nullopt if the ray is parallel

///  to the plane or the intersection is behind the ray origin.

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
	const char *value = std::getenv("PROJECTV_GRAVIGUN_SNAP");
	if (value == nullptr) {
		return false;
	}
	const std::string v(value);
	return v == "on" || v == "1" || v == "true";
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

	/// \brief **F key transitions.** Pressed → pick.
	///
	/// \details
	/// Released → drop.
	///  The pickedInstanceIndex persists across frames while F

	///  is held, even between press/release edges.

	const bool fDown = IsInputActionDown(*input, InputAction::PickModel);
	const bool fPressed = ConsumeInputActionPressed(*input, InputAction::PickModel);

	if (fPressed && state->pickedInstanceIndex < 0) {
		/// \brief **Pick the closest model** whose AABB intersects the
		///
		/// \details
		///  camera ray. The AABB is in world space (after

		///  `SnapModelInstancesAboveGround` ran), so the pick

		///  uses the same volume the renderer does. On a

		///  successful pick, also capture the **pick anchor**:

		///  the model's current AABB min and the cursor's

		///  current ground-plane hit. The drag then uses these

		///  as a relative reference (newMin = anchorAabbMin +

		///  (currentHit - anchorHit)) so the model only moves

		///  when the cursor moves — it does NOT teleport on

		///  the first frame of F-held just because the cursor

		///  happened to be at a non-integer ground-plane hit

		///  (e.g. picking a column at AABB min.x=-8 with the

		///  crosshair at the column's visual centre x=-5

		///  would otherwise set AABB min.x to round(-5) = -5

		///  immediately, shifting the model by 3 voxels to

		///  the right the moment F was pressed — and worse,

		///  picking with the crosshair near the column edge

		///  would shift by a full voxel away from the

		///  intended pick position).

		const Ray ray = BuildCameraRay(camera);
		int bestIndex = -1;
		float bestTNear = std::numeric_limits<float>::infinity();
		for (size_t i = 0; i < render->modelInstances.size(); ++i) {
			const ModelInstanceData &inst = render->modelInstances[i];
			if (inst.indexCount == 0) {
				continue;
			}
			float tNear = 0.0f;
			float tFar = 0.0f;
			if (!RayAabbIntersect(ray,
								  glm::vec3(inst.worldAabbMin[0], inst.worldAabbMin[1], inst.worldAabbMin[2]),
								  glm::vec3(inst.worldAabbMax[0], inst.worldAabbMax[1], inst.worldAabbMax[2]),
								  tNear, tFar)) {
				continue;
			}
			if (tNear < bestTNear) {
				bestTNear = tNear;
				bestIndex = static_cast<int>(i);
			}
		}
		if (bestIndex >= 0) {
			state->pickedInstanceIndex = bestIndex;
			/// \brief We don't reuse `FindFloorSurfaceYForAabb` here
			///
			/// \details
			///  (it's a `static` function in ModelManifestLoader.cpp

			///  — not exported). Use the world floor default Y=0

			///  (the bottom of the VoxelLab floor voxel) to honour

			///  the manifest convention `position = AABB min` (no

			///  implicit +1-voxel lift).

			state->targetY = 0.0f;
			const ModelInstanceData &inst = render->modelInstances[bestIndex];
			/// \brief Capture the pick anchor:
			///
			/// \details
			/// where the model is
			///  right now, and where the cursor's ground-plane

			///  ray hit is right now. The drag computes its

			///  new AABB min relative to this anchor.

			state->pickAnchorAabbMin = glm::vec3(
				inst.worldAabbMin[0], inst.worldAabbMin[1], inst.worldAabbMin[2]);
			const std::optional<glm::vec3> anchorHit =
				IntersectRayHorizontalPlane(ray, state->targetY);
			if (anchorHit.has_value()) {
				state->pickAnchorHit = *anchorHit;
			} else {
				/// \brief Ray parallel to ground plane or pointing
				///
				/// \details
				///  away — fall back to the current AABB min

				///  as the anchor (so a degenerate pick

				///  doesn't accidentally re-position the

				///  model when the operator starts dragging).

				state->pickAnchorHit = state->pickAnchorAabbMin;
			}
			std::fprintf(stderr,
						 "[Gravigun-DBG] PICKED: index=%d aabbMin=(%.3f,%.3f,%.3f) aabbMax=(%.3f,%.3f,%.3f) anchorHit=(%.3f,%.3f,%.3f) targetY=%.1f\n",
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
			/// \brief Preserve the model's current dims.
			const float dimX = inst.worldAabbMax[0] - inst.worldAabbMin[0];
			const float dimY = inst.worldAabbMax[1] - inst.worldAabbMin[1];
			const float dimZ = inst.worldAabbMax[2] - inst.worldAabbMin[2];
			/// \brief Crosshair delta from the pick anchor.
			const float deltaX = hit->x - state->pickAnchorHit.x;
			const float deltaZ = hit->z - state->pickAnchorHit.z;
			/// \brief New AABB min = pick anchor + delta.
			///
			/// \details
			/// By default
			///  (no snap), the raw delta is preserved so the

			///  operator's exact placement is logged. With

			///  `PROJECTV_GRAVIGUN_SNAP=on`, the AABB min is

			///  rounded to the integer voxel grid on X and Z

			///  (Y is operator-controlled via `targetY` and

			///  not affected by the cursor XZ delta).

			const bool snapOnDrag = GravigunSnapEnabled();
			const float rawMinX = state->pickAnchorAabbMin.x + deltaX;
			const float rawMinZ = state->pickAnchorAabbMin.z + deltaZ;
			const float newMinX = snapOnDrag ? std::round(rawMinX) : rawMinX;
			const float newMinY = state->targetY;
			const float newMinZ = snapOnDrag ? std::round(rawMinZ) : rawMinZ;
			/// \brief Update the AABB.
			inst.worldAabbMin[0] = newMinX;
			inst.worldAabbMin[1] = newMinY;
			inst.worldAabbMin[2] = newMinZ;
			inst.worldAabbMax[0] = newMinX + dimX;
			inst.worldAabbMax[1] = newMinY + dimY;
			inst.worldAabbMax[2] = newMinZ + dimZ;
			/// \brief Update the model basis translation column.
			///
			/// \details
			///  We don't change the rotation/scale columns — the

			///  model keeps its shape. (For an asset loaded with

			///  node transforms baked in — like the lamp-post

			///  column from M5.1d — this means dragging changes

			///  only the world AABB position, not the shape.)

			///  The translation must include the `-sourceAabbMin`

			///  offset so the GPU's per-vertex transform puts

			///  vertex `sourceAabbMin` exactly at `newMin` in

			///  world space.

			inst.modelTransform.c[3].x = newMinX - inst.sourceAabbMin[0];
			inst.modelTransform.c[3].y = newMinY - inst.sourceAabbMin[1];
			inst.modelTransform.c[3].z = newMinZ - inst.sourceAabbMin[2];
		}
	}
}

} // namespace projectv::app
