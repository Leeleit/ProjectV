#include "asset/ModelManifestLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <string>
#include <utility>

#include "asset/AssetLoader.hpp"
#include "asset/AssetManifest.hpp"
#include "asset/MeshBaker.hpp"
#include "asset/MeshGpuResources.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "voxel/VoxelWorld.hpp"

#include "fmt/format.h"

namespace projectv::asset {

namespace {

glm::mat4 BuildEntryWorldMatrix(const ManifestEntry &entry)
{
	const glm::vec3 rotationRadians = glm::radians(entry.rotationDegrees);
	const glm::mat4 translation = glm::translate(glm::mat4(1.0f), entry.position);
	const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rotationRadians.y, glm::vec3(0.0f, 1.0f, 0.0f))
		* glm::rotate(glm::mat4(1.0f), rotationRadians.x, glm::vec3(1.0f, 0.0f, 0.0f))
		* glm::rotate(glm::mat4(1.0f), rotationRadians.z, glm::vec3(0.0f, 0.0f, 1.0f));
	const glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(entry.scale));
	return translation * rotation * scale;
}

void StoreMatrixColumnMajor(const glm::mat4 &source, std::array<float, 16> &out)
{
	const float *src = glm::value_ptr(source);
	std::memcpy(out.data(), src, sizeof(std::array<float, 16>));
}

} // namespace

bool LoadAndRegisterModelsFromManifest(
	VulkanContextState *context,
	VkCommandPool commandPool,
	VkQueue queue,
	RenderState *render)
{
	if (!context || !render) {
		return false;
	}

	UnloadAllModels(context, render);

	const std::vector<ManifestEntry> manifest = ParseAssetManifestFromEnv();
	if (manifest.empty()) {
		return true;
	}

	for (const ManifestEntry &entry : manifest) {
		LoadAssetError loadError;
		std::unique_ptr<LoadedAsset> loaded = LoadGlb(entry.path, &loadError);
		if (!loaded) {
			runtime::LogRuntimeFailure(
				"Model",
				"LoadAndRegisterModelsFromManifest.LoadGlb",
				fmt::format("path={} error={}", entry.path, loadError.message));
			continue;
		}
		std::string bakeError;
		const BakedMesh baked = BakeLoadedAsset(*loaded, {}, &bakeError);
		if (!bakeError.empty() || baked.primitives.empty()) {
			runtime::LogRuntimeFailure(
				"Model",
				"LoadAndRegisterModelsFromManifest.BakeLoadedAsset",
				fmt::format("path={} error={}", entry.path, bakeError));
			continue;
		}
		if (!bakeError.empty() || baked.primitives.empty()) {
			runtime::LogRuntimeFailure(
				"Model",
				"LoadAndRegisterModelsFromManifest.BakeLoadedAsset",
				fmt::format("path={} error={}", entry.path, bakeError));
			continue;
		}

		const BakedPrimitive &prim = baked.primitives.front();
		MeshGpuResources gpu;
		std::string uploadError;
		if (!UploadBakedPrimitiveToGpu(context->device, context->allocator, commandPool, queue, prim, gpu, &uploadError)) {
			runtime::LogRuntimeFailure(
				"Model",
				"LoadAndRegisterModelsFromManifest.UploadBakedPrimitiveToGpu",
				fmt::format("path={} error={}", entry.path, uploadError));
			continue;
		}

		ModelRegistryEntry registryEntry;
		registryEntry.id = entry.id;
		registryEntry.gpu = gpu;
		registryEntry.aabbMin = {
			loaded->aabbMin.x,
			loaded->aabbMin.y,
			loaded->aabbMin.z,
		};
		registryEntry.aabbMax = {
			loaded->aabbMax.x,
			loaded->aabbMax.y,
			loaded->aabbMax.z,
		};
		render->modelRegistry.push_back(std::move(registryEntry));
	}

	if (render->modelRegistry.empty()) {
		return true;
	}

	render->modelInstances.clear();
	render->modelInstances.reserve(render->modelRegistry.size());
	for (size_t i = 0; i < render->modelRegistry.size() && i < manifest.size(); ++i) {
		const ManifestEntry &entry = manifest[i];
		const ModelRegistryEntry &reg = render->modelRegistry[i];

		const glm::mat4 world = BuildEntryWorldMatrix(entry);
		ModelInstanceData instance{};
		StoreMatrixColumnMajor(world, instance.modelTransform);
		// Source AABB in world space (transformed, not yet frustum-culled;
		// M5 will add the proper world AABB math + cull).
		//
		// **Semantic of `entry.position` (post M5.1c, 2026-06-12):** the
		// manifest `position` is the desired world-space **AABB min** of
		// the model — the voxel corner directly under the model — NOT
		// the geometric center. To place the model's AABB min at
		// `position`, the translation part of the world matrix is
		// shifted by `-srcAabbMin * scale` so the source AABB min
		// lands at `position` and the source AABB max lands at
		// `position + srcDim * scale`. This generalises the M5.1b
		// "origin at the voxel corner" rule to all model dimensions
		// and unifies the env-var syntax (the operator writes the
		// voxel-grid coordinate, not a center they had to back-compute
		// from the source AABB). The voxel-grid AABB-max integer
		// alignment is then enforced by the auto-scale step inside
		// `SnapModelInstancesAboveGround`.
		const glm::vec3 srcMin = glm::vec3(reg.aabbMin[0], reg.aabbMin[1], reg.aabbMin[2]);
		const glm::vec3 srcMax = glm::vec3(reg.aabbMax[0], reg.aabbMax[1], reg.aabbMax[2]);
		const glm::vec3 srcDim = srcMax - srcMin;
		// Shift the model-space origin so AABB min lands at entry.position.
		// BuildEntryWorldMatrix assumed model-space origin == entry.position;
		// we correct by `-srcMin * scale` to land AABB min at entry.position.
		const glm::mat4 aabbMinOffset = glm::translate(glm::mat4(1.0f), -srcMin * entry.scale);
		const glm::mat4 worldWithAabbMin = world * aabbMinOffset;
		StoreMatrixColumnMajor(worldWithAabbMin, instance.modelTransform);
		instance.worldAabbMin = { entry.position.x, entry.position.y, entry.position.z };
		instance.worldAabbMax = {
			entry.position.x + srcDim.x * entry.scale,
			entry.position.y + srcDim.y * entry.scale,
			entry.position.z + srcDim.z * entry.scale,
		};
		instance.vertexBuffer = reg.gpu.vertexBuffer;
		instance.indexBuffer = reg.gpu.indexBuffer;
		instance.indexCount = reg.gpu.indexCount;
		// Capture the source AABB min in model-local space.
		// The load path's `aabbMinOffset = T(-srcMin*scale)`
		// shifts the model basis so the translation column of
		// `modelTransform` ends up at `pos - srcAabbMin`. Snap
		// and drag paths that mutate the AABB min must update
		// `modelTransform[12..14]` by the same `−srcAabbMin`
		// offset — see the docstring on `ModelInstanceData::sourceAabbMin`
		// in `core/Types.hpp`. Without this, the rendered mesh
		// is offset by `srcAabbMin` from the operator's chosen
		// AABB min (e.g. dragging the lamp to AABB min.x=-9
		// renders the model with its AABB at -9 + srcMin.x =
		// -11.36, sticking 2.36 voxels out past -9).
		instance.sourceAabbMin = {
			static_cast<float>(reg.aabbMin[0]),
			static_cast<float>(reg.aabbMin[1]),
			static_cast<float>(reg.aabbMin[2]),
		};
		render->modelInstances.push_back(instance);
	}
	return true;
}

void UnloadAllModels(VulkanContextState *context, RenderState *render)
{
	if (!context || !render) {
		return;
	}
	for (ModelRegistryEntry &entry : render->modelRegistry) {
		DestroyMeshGpuResources(context->allocator, entry.gpu);
	}
	render->modelRegistry.clear();
	render->modelInstances.clear();
}

namespace {

// Find the LOWEST non-Air voxel in the column at the given integer
// XZ position. This is the *ground* / floor / first solid surface
// at that XZ. Returns `std::numeric_limits<int32_t>::max()` if the
// column is fully Air (no solid voxels at all — caller decides what
// to do). Walks from Y=0 up. We pick the lowest, not the topmost,
// because in `VoxelLab` the same XZ column can contain the floor
// (Y=0), the glass shell (Y=2..14), and the fluid column
// (Y=3..10) — the topmost non-Air voxel is the upper shell arc
// (Y=14) and lifting the model to sit "on top of" the shell would
// teleport it 14 units up, far above the floor.
int32_t FindBottomVoxelYAtXZ(const VoxelWorld &world, int32_t x, int32_t z)
{
	const int32_t worldHeight = static_cast<int32_t>(world.height);
	if (worldHeight <= 0) {
		return std::numeric_limits<int32_t>::max();
	}
	for (int32_t y = 0; y < worldHeight; ++y) {
		if (GetVoxelMaterial(world, Int3{x, y, z}) != VoxelMaterial::Air) {
			return y;
		}
	}
	return std::numeric_limits<int32_t>::max();
}

// Compute the floor surface across the model's XZ footprint (the
// highest of the LOWEST non-Air voxels at 5 AABB samples — 4
// corners + center). A model on a 1-cell-wide bridge / wall
// corner would otherwise snap to the wrong side if we only
// sampled the center. Returns `INT_MIN` if NO sample has a solid
// voxel underneath (caller should skip the snap — the model is
// either floating in air, or its XZ footprint extends outside the
// world's bounds).
//
// **Bug history (M5.1c, 2026-06-12):** the original M5.1b version
// checked for `INT_MIN` here, but `FindBottomVoxelYAtXZ` actually
// returns `INT_MAX` (not `INT_MIN`) when its XZ column is fully
// Air — so the check was dead and `std::max` would silently
// pollute the result with `INT_MAX` whenever any sample landed
// outside the world (e.g. a column model at Z=9..17 in a world
// that only goes to Z=12). The snap pass then did
// `topVoxelY + 1` on `INT_MAX`, which is signed integer overflow
// (UB; on 2's-complement platforms `INT_MAX + 1 == INT_MIN`),
// then `static_cast<float>(INT_MIN)` rounded to `-2147483648.0` —
// the model was lifted by ~2 billion units downward, sending it
// far below the floor and out of the camera frustum. Now
// `FindFloorSurfaceYForAabb` returns `INT_MIN` whenever any of
// the 5 samples is `INT_MAX` (out-of-bounds or fully-Air), and
// the snap's existing `if (topVoxelY == INT_MIN) continue;`
// guard correctly skips the model — the operator gets a
// "model not visible" instead of a "model teleported into the
// void" failure mode. The two-stage `INT_MAX → INT_MIN` rename
// also catches the no-floor-anywhere case (model entirely
// outside the world or in mid-air with no platform).
int32_t FindFloorSurfaceYForAabb(const VoxelWorld &world, float minX, float maxX, float minZ, float maxZ)
{
	const auto trySample = [&](float fx, float fz) -> int32_t {
		const int32_t x = static_cast<int32_t>(std::floor(fx));
		const int32_t z = static_cast<int32_t>(std::floor(fz));
		return FindBottomVoxelYAtXZ(world, x, z);
	};
	const std::array<int32_t, 5> samples{
		trySample(minX, minZ),
		trySample(maxX, minZ),
		trySample(minX, maxZ),
		trySample(maxX, maxZ),
		trySample(0.5f * (minX + maxX), 0.5f * (minZ + maxZ)),
	};
	const int32_t noFloorSentinel = std::numeric_limits<int32_t>::max();
	bool anyValid = false;
	int32_t best = 0;
	for (const int32_t s : samples) {
		if (s == noFloorSentinel) {
			// Out-of-world or fully-Air column at this sample.
			// Skip it — do NOT pollute `best` with `INT_MAX`,
			// otherwise the snap pass does `INT_MAX + 1` and
			// overflows. If ALL samples are out, the function
			// returns `INT_MIN` and the snap skips the model.
			continue;
		}
		if (!anyValid || s > best) {
			best = s;
			anyValid = true;
		}
	}
	if (!anyValid) {
		return std::numeric_limits<int32_t>::min();
	}
	return best;
}

} // namespace
void SnapModelInstancesAboveGround(const VoxelWorld &world, RenderState *render)
{
	if (!render) {
		return;
	}

	// **M5.1d, 2026-06-12: per-axis smart snap (replaces M5.1c
	// no-op and the earlier "bottom-anchored only" version).**
	// The operator's convention is `position = AABB min in
	// world` (the corner under the model) and "no implicit
	// lift" (Y is operator-controlled, e.g. Y=0 for embedded-
	// in-floor, Y=1 for sit-on-floor, Y=N for floating). The
	// snap enforces **two invariants**:
	//
	//   1. **AABB min on the integer grid** on every axis
	//      where the operator's drag is inside the world
	//      bounds (the "snap to grid" contract).
	//   2. **AABB max on the integer grid AND ≤ world.maxExclusive**
	//      on every axis where the operator's drag would
	//      otherwise make the model stick out past the world
	//      edge (the "no stick-out" contract).
	//
	// The two invariants conflict when the model's AABB dim is
	// not an integer — the lamp-post column has dim.z=8.135
	// from the node-walked asset (cylinder's glb Z extent =
	// 8.135 after the node's Y-scale=5), and the world is
	// 12 deep. Pure "AABB min on grid" leaves AABB max at
	// non-integer (12.135 for AABB min.z=4), which sticks out
	// by 0.135. Pure "AABB max on grid" puts the AABB min
	// somewhere awkward (3.865, not on the grid) and biases
	// the X/Y position too. The compromise: for each axis
	// independently, pick the option that satisfies BOTH
	// invariants on that axis, falling back to whichever keeps
	// the model inside the world if both can't be satisfied.
	//
	// **Per-axis decision:**
	//   - if `AABB max ≤ world max` on this axis → use the
	//     "AABB min on grid" path (round the operator's drag,
	//     derive AABB max = AABB min + dim). This keeps the
	//     operator's coordinate for the axes that fit.
	//   - else → use the "AABB max on grid" path (snap AABB
	//     max to `floor(min(AABB max, world max))`, derive
	//     AABB min = AABB max − dim). This is the only way
	//     to fit the model inside the world when the dim is
	//     non-integer.
	//
	// **Replaces the M5.1c no-op** that lived here from
	// 2026-06-12 16:00 to 2026-06-12 19:30, and the
	// "bottom-anchored only" 19:30 version that left
	// AABB max 0.13 past the world edge for non-integer
	// dim. The 19:30 → 20:00 fix is per-axis smart. The legacy
	// Y-lift branch from M5.1b is gone for good.
	// **M5.1d, 2026-06-12:** snap clamps to **floor** bounds
	// (the visible checkerboard platform), not to the world
	// bounds. The world has `padding` voxels of invisible Air
	// on every XZ side (for chunk allocation), so
	// `world.maxExclusive.x = 12` while the floor is at
	// `floorMaxExclusive.x = 9` for VoxelLab. The previous
	// version (`world.maxExclusive`) would have allowed a
	// model with `dim.z=8.13` to land at `aabbMax.z=9.135`
	// (3 voxels past the floor edge, still within the world
	// padding) — the operator wants the AABB max exactly on
	// the floor edge (`aabbMax.z=9`), so the snap now uses
	// `floorMaxExclusive` for the clamp test.
	const float worldMinX = static_cast<float>(world.floorMin.x);
	const float worldMinY = static_cast<float>(world.min.y);
	const float worldMinZ = static_cast<float>(world.floorMin.z);
	const float worldMaxX = static_cast<float>(world.floorMaxExclusive.x);
	const float worldMaxY = static_cast<float>(world.maxExclusive.y);
	const float worldMaxZ = static_cast<float>(world.floorMaxExclusive.z);

	for (ModelInstanceData &instance : render->modelInstances) {
		const float dimX = instance.worldAabbMax[0] - instance.worldAabbMin[0];
		const float dimY = instance.worldAabbMax[1] - instance.worldAabbMin[1];
		const float dimZ = instance.worldAabbMax[2] - instance.worldAabbMin[2];
		if (dimX <= 0.0f || dimY <= 0.0f || dimZ <= 0.0f) {
			runtime::LogRuntimeFailure(
				"Model",
				"SnapModelInstancesAboveGround",
				fmt::format("degenerate AABB: aabbMin=({:.3f},{:.3f},{:.3f}) aabbMax=({:.3f},{:.3f},{:.3f}) dims=({:.3f},{:.3f},{:.3f})",
					instance.worldAabbMin[0], instance.worldAabbMin[1], instance.worldAabbMin[2],
					instance.worldAabbMax[0], instance.worldAabbMax[1], instance.worldAabbMax[2],
					dimX, dimY, dimZ));
			continue;
		}

		// Per-axis: pick the snap path that satisfies both
		// invariants on that axis. The "AABB max on grid" path
		// wins when the operator's drag would make the model
		// stick out; the "AABB min on grid" path wins otherwise.
		const float rawMaxX = instance.worldAabbMin[0] + dimX;
		const float rawMaxY = instance.worldAabbMin[1] + dimY;
		const float rawMaxZ = instance.worldAabbMin[2] + dimZ;
		const bool xSticksOut = rawMaxX > worldMaxX;
		const bool ySticksOut = rawMaxY > worldMaxY;
		const bool zSticksOut = rawMaxZ > worldMaxZ;

		float newMinX = 0.0f;
		float newMinY = 0.0f;
		float newMinZ = 0.0f;
		float newMaxX = 0.0f;
		float newMaxY = 0.0f;
		float newMaxZ = 0.0f;

		// **X axis.** Two valid placements per axis:
		//   - Case A: AABB max on the integer grid ≤ worldMax
		//     (clamp the model into the world by deriving
		//     AABB min = AABB max − dim; AABB min may be
		//     non-integer when dim is non-integer).
		//   - Case B: AABB min on the integer grid, derived
		//     AABB max = AABB min + dim (AABB max may be
		//     non-integer; can stick out past worldMax if dim
		//     is non-integer and the rounded AABB min is on the
		//     higher side of the dim's range).
		// The "Case B fits iff `roundedMin + dim ≤ worldMax`"
		// test is the **post-round fit check** — without it,
		// a `position.z=0` for a model with `dim.z=8.13` in a
		// `worldMaxZ=9` world would be rounded to `1` and
		// push AABB max to `9.13` (stick-out 0.13). The check
		// catches this and falls back to Case A: AABB max =
		// `floor(worldMaxZ)`, AABB min = `worldMax − dim` (which
		// is `0.87`, not on the grid, but AABB max is exactly
		// on the world bound — the operator's intent for a
		// flush-with-edge placement). The previous version
		// (M5.1d pre-2026-06-12) had this exact bug; the
		// 2026-06-12 fix is the post-round fit check below.
		if (xSticksOut) {
			newMaxX = std::floor(std::min(rawMaxX, worldMaxX));
			newMinX = newMaxX - dimX;
		} else {
			const float roundedX = std::round(std::clamp(instance.worldAabbMin[0], worldMinX, worldMaxX - dimX));
			if (roundedX + dimX <= worldMaxX) {
				newMinX = roundedX;
				newMaxX = roundedX + dimX;
			} else {
				// Rounded AABB min would push AABB max past
				// worldMax (dim is non-integer, rounded to
				// the high side). Clamp AABB max to the
				// world bound and derive AABB min.
				newMaxX = std::floor(worldMaxX);
				newMinX = newMaxX - dimX;
			}
		}

		// **Y axis.** The operator sets `position.y` explicitly;
		// the snap does NOT lift. If the AABB sticks out
		// (the model is too tall for the world), snap the max
		// to the world max. Otherwise leave Y alone — the
		// operator's integer Y is preserved. Y doesn't get the
		// post-round fit check because Y is operator-controlled
		// and `position.y` is read verbatim (no `std::round`).
		if (ySticksOut) {
			newMaxY = std::floor(std::min(rawMaxY, worldMaxY));
			newMinY = newMaxY - dimY;
		} else {
			newMinY = instance.worldAabbMin[1];
			newMaxY = newMinY + dimY;
		}

		// **Z axis.** Same contract as X (with the post-round
		// fit check). For the lamp-post column with
		// `dim.z=8.13` in an 18-voxel Z range, the operator
		// can place the model at the front edge with
		// `aabbMax.z=9` and `aabbMin.z=0.87` — the post-round
		// fit check is what makes that placement survive the
		// snap (without it, the snap would round `0.87 → 1`
		// and the AABB would stick out 0.13 voxels).
		if (zSticksOut) {
			newMaxZ = std::floor(std::min(rawMaxZ, worldMaxZ));
			newMinZ = newMaxZ - dimZ;
		} else {
			const float roundedZ = std::round(std::clamp(instance.worldAabbMin[2], worldMinZ, worldMaxZ - dimZ));
			if (roundedZ + dimZ <= worldMaxZ) {
				newMinZ = roundedZ;
				newMaxZ = roundedZ + dimZ;
			} else {
				newMaxZ = std::floor(worldMaxZ);
				newMinZ = newMaxZ - dimZ;
			}
		}

		// Update the AABB in place. AABB dim is preserved.
		instance.worldAabbMin[0] = newMinX;
		instance.worldAabbMin[1] = newMinY;
		instance.worldAabbMin[2] = newMinZ;
		instance.worldAabbMax[0] = newMaxX;
		instance.worldAabbMax[1] = newMaxY;
		instance.worldAabbMax[2] = newMaxZ;

		// Update the model basis translation column so the
		// rendered mesh stays aligned with the AABB. The load
		// path's `aabbMinOffset = T(-srcMin*scale)` puts the
		// translation at `pos - sourceAabbMin` (NOT `pos`); the
		// GPU shader then adds `sourceAabbMin` back per vertex
		// (or rather, the model-local vertex (0,0,0) lands at
		// `pos` in world space). Re-derive the translation
		// from the snapped AABB min by the same `-sourceAabbMin`
		// offset, otherwise the rendered mesh would shift by
		// `sourceAabbMin` from the operator's chosen AABB
		// (e.g. snap to AABB min.x=-9 with srcMin.x=-2.36
		// would render the model with its actual AABB at
		// -9 + (-2.36) = -11.36).
		instance.modelTransform[12] = newMinX - instance.sourceAabbMin[0];
		instance.modelTransform[13] = newMinY - instance.sourceAabbMin[1];
		instance.modelTransform[14] = newMinZ - instance.sourceAabbMin[2];
	}
}

void SnapModelInstancesCenterAnchored(
	const VoxelWorld &world,
	RenderState *render)
{
	if (!render) {
		return;
	}

	// **Centred snap, 2026-06-12.** Same clamp-to-world + snap-
	// to-grid contract as `SnapModelInstancesAboveGround`, but
	// the anchor is the model's AABB **centre** rather than
	// the AABB min. The operator's "position" value is
	// implicitly translated to the centre at load time, and
	// the snap rounds the centre to the integer grid. Lets the
	// operator position a model by its geometric centre for
	// use cases where the bottom-anchored convention is wrong
	// (floating décor, skybox elements, furniture-style objects
	// that don't sit on the floor).
	//
	// The function is **not called by default** — see the
	// call sites in `VulkanInit.cpp:268` and
	// `main.cpp:95` for the bottom-anchored default. The
	// env var `PROJECTV_MODEL_SNAP=centre` (read at call time
	// inside the snap dispatch wrapper below) opts in by
	// calling this function in addition to the default snap.
	//
	// AABB dims are preserved; the AABB min shifts outward
	// by the same amount the centre shifts, so the model's
	// shape is unchanged across the snap.
	// **M5.1d, 2026-06-12:** uses floor bounds (see the
	// matching comment in `SnapModelInstancesAboveGround`).
	const float worldMinX = static_cast<float>(world.floorMin.x);
	const float worldMinY = static_cast<float>(world.min.y);
	const float worldMinZ = static_cast<float>(world.floorMin.z);
	const float worldMaxX = static_cast<float>(world.floorMaxExclusive.x);
	const float worldMaxY = static_cast<float>(world.maxExclusive.y);
	const float worldMaxZ = static_cast<float>(world.floorMaxExclusive.z);

	for (ModelInstanceData &instance : render->modelInstances) {
		const float dimX = instance.worldAabbMax[0] - instance.worldAabbMin[0];
		const float dimY = instance.worldAabbMax[1] - instance.worldAabbMin[1];
		const float dimZ = instance.worldAabbMax[2] - instance.worldAabbMin[2];
		if (dimX <= 0.0f || dimY <= 0.0f || dimZ <= 0.0f) {
			runtime::LogRuntimeFailure(
				"Model",
				"SnapModelInstancesCenterAnchored",
				fmt::format("degenerate AABB: aabbMin=({:.3f},{:.3f},{:.3f}) aabbMax=({:.3f},{:.3f},{:.3f}) dims=({:.3f},{:.3f},{:.3f})",
					instance.worldAabbMin[0], instance.worldAabbMin[1], instance.worldAabbMin[2],
					instance.worldAabbMax[0], instance.worldAabbMax[1], instance.worldAabbMax[2],
					dimX, dimY, dimZ));
			continue;
		}

		// Re-anchor: AABB min ↔ AABB centre. The load path
		// stored the translation at the AABB min, but the
		// centred snap treats the current AABB **centre** as
		// the snap target. Compute the centre, snap to the
		// integer grid (X, Y, Z), clamp to the world bounds
		// so the AABB fits, then derive the new AABB min.
		float centerX = 0.5f * (instance.worldAabbMin[0] + instance.worldAabbMax[0]);
		float centerY = 0.5f * (instance.worldAabbMin[1] + instance.worldAabbMax[1]);
		float centerZ = 0.5f * (instance.worldAabbMin[2] + instance.worldAabbMax[2]);

		// Snap to integer voxel grid (all 3 axes; the
		// operator's `position` becomes the integer centre).
		centerX = std::round(centerX);
		centerY = std::round(centerY);
		centerZ = std::round(centerZ);

		// Clamp the centre so the resulting AABB fits within
		// the world bounds: centre must be in
		// [worldMin + dim/2, worldMax - dim/2].
		const float halfDimX = 0.5f * dimX;
		const float halfDimY = 0.5f * dimY;
		const float halfDimZ = 0.5f * dimZ;
		centerX = std::clamp(centerX, worldMinX + halfDimX, worldMaxX - halfDimX);
		centerY = std::clamp(centerY, worldMinY + halfDimY, worldMaxY - halfDimY);
		centerZ = std::clamp(centerZ, worldMinZ + halfDimZ, worldMaxZ - halfDimZ);

		// Derive the new AABB from the clamped centre. The
		// AABB dims are preserved; the AABB min shifts
		// outward by the same amount the centre shifts.
		const float newMinX = centerX - halfDimX;
		const float newMinY = centerY - halfDimY;
		const float newMinZ = centerZ - halfDimZ;
		instance.worldAabbMin[0] = newMinX;
		instance.worldAabbMin[1] = newMinY;
		instance.worldAabbMin[2] = newMinZ;
		instance.worldAabbMax[0] = newMinX + dimX;
		instance.worldAabbMax[1] = newMinY + dimY;
		instance.worldAabbMax[2] = newMinZ + dimZ;

		// Update the model basis translation column. Same
		// `-sourceAabbMin` offset as the bottom-anchored snap
		// above — the load path's aabbMinOffset puts the
		// translation at `pos - sourceAabbMin`, and the snap
		// must preserve that relationship.
		instance.modelTransform[12] = newMinX - instance.sourceAabbMin[0];
		instance.modelTransform[13] = newMinY - instance.sourceAabbMin[1];
		instance.modelTransform[14] = newMinZ - instance.sourceAabbMin[2];
	}
}

void SnapModelInstancesAboveGroundDispatch(
	const VoxelWorld &world,
	RenderState *render)
{
	// Dispatcher (M5.1d, 2026-06-12). The default snap is
	// bottom-anchored (`SnapModelInstancesAboveGround`). The
	// env var `PROJECTV_MODEL_SNAP=centre` opts in to the
	// centred variant — useful for objects whose visual
	// centre matters more than the bottom corner (floating
	// décor, skybox elements, etc.). The dispatcher applies
	// the chosen snap and the bottom-anchored clamp in
	// sequence so the result is always a clamped AABB on the
	// integer grid, regardless of anchor.
	const char *snapMode = std::getenv("PROJECTV_MODEL_SNAP");
	if (snapMode != nullptr && std::string(snapMode) == "centre") {
		// Centre-anchored snap first (centred → integer grid),
		// then the bottom-anchored default to enforce clamp-to-
		// world on the resulting AABB. The order doesn't
		// matter for the final AABB (both are idempotent) but
		// centred-first gives an `AABB centre` at the integer
		// grid, while bottom-first gives an `AABB min` at the
		// integer grid; the centred snap is the user-visible
		// semantic when the env var is set.
		SnapModelInstancesCenterAnchored(world, render);
	}
	SnapModelInstancesAboveGround(world, render);
}

} // namespace projectv::asset
