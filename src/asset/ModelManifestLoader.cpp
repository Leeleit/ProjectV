#include "asset/ModelManifestLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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
	const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rotationRadians.y, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), rotationRadians.x, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), rotationRadians.z, glm::vec3(0.0f, 0.0f, 1.0f));
	const glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(entry.scale));
	return translation * rotation * scale;
}

void StoreMatrixColumnMajor(const glm::mat4 &source, projectv::math::Mat4 &out)
{
	const float *src = glm::value_ptr(source);
	std::memcpy(out.data(), src, sizeof(projectv::math::Mat4));
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
		const glm::vec3 srcMin = glm::vec3(reg.aabbMin[0], reg.aabbMin[1], reg.aabbMin[2]);
		const glm::vec3 srcMax = glm::vec3(reg.aabbMax[0], reg.aabbMax[1], reg.aabbMax[2]);
		const glm::vec3 srcDim = srcMax - srcMin;
		/// \brief Shift the model-space origin so AABB min lands at entry.position.
		///
		/// \details
		///  BuildEntryWorldMatrix assumed model-space origin == entry.position;

		///  we correct by `-srcMin * scale` to land AABB min at entry.position.

		const glm::mat4 aabbMinOffset = glm::translate(glm::mat4(1.0f), -srcMin * entry.scale);
		const glm::mat4 worldWithAabbMin = world * aabbMinOffset;
		StoreMatrixColumnMajor(worldWithAabbMin, instance.modelTransform);
		instance.worldAabbMin = {entry.position.x, entry.position.y, entry.position.z};
		instance.worldAabbMax = {
			entry.position.x + srcDim.x * entry.scale,
			entry.position.y + srcDim.y * entry.scale,
			entry.position.z + srcDim.z * entry.scale,
		};
		instance.vertexBuffer = reg.gpu.vertexBuffer;
		instance.indexBuffer = reg.gpu.indexBuffer;
		instance.indexCount = reg.gpu.indexCount;
		/// \brief Capture the source AABB min in model-local space.
		///
		/// \details
		///  The load path's `aabbMinOffset = T(-srcMin*scale)`

		///  shifts the model basis so the translation column of

		///  `modelTransform` ends up at `pos - srcAabbMin`. Snap

		///  and drag paths that mutate the AABB min must update

		///  `modelTransform[12..14]` by the same `−srcAabbMin`

		///  offset — see the docstring on `ModelInstanceData::sourceAabbMin`

		///  in `core/Types.hpp`. Without this, the rendered mesh

		///  is offset by `srcAabbMin` from the operator's chosen

		///  AABB min (e.g. dragging the lamp to AABB min.x=-9

		///  renders the model with its AABB at -9 + srcMin.x =

		///  -11.36, sticking 2.36 voxels out past -9).

		instance.sourceAabbMin = {
			static_cast<float>(reg.aabbMin[0]),
			static_cast<float>(reg.aabbMin[1]),
			static_cast<float>(reg.aabbMin[2]),
		};
	}
	render->modelInstances.clear();
	render->modelInstances.reserve(render->modelRegistry.size());
	const size_t instanceCount = std::min(render->modelRegistry.size(), manifest.size());
	for (size_t i = 0; i < instanceCount; ++i) {
		const ManifestEntry &entry = manifest[i];
		const ModelRegistryEntry &reg = render->modelRegistry[i];
		if (!reg.gpu.vertexBuffer) {
			continue;
		}
		const projectv::math::Vec3 srcDim{
			reg.aabbMax[0] - reg.aabbMin[0],
			reg.aabbMax[1] - reg.aabbMin[1],
			reg.aabbMax[2] - reg.aabbMin[2],
			0.0f,
		};
		projectv::math::Mat4 modelTransform = projectv::math::identity();
		modelTransform.c[0].x = entry.scale;
		modelTransform.c[1].y = entry.scale;
		modelTransform.c[2].z = entry.scale;
		modelTransform.c[3] = projectv::math::Vec4{
			entry.position.x - (reg.aabbMin[0] * entry.scale),
			entry.position.y - (reg.aabbMin[1] * entry.scale),
			entry.position.z - (reg.aabbMin[2] * entry.scale),
			1.0f,
		};
		ModelInstanceData instance{};
		instance.modelTransform = modelTransform;
		instance.worldAabbMin = {entry.position.x, entry.position.y, entry.position.z, 0.0f};
		instance.worldAabbMax = projectv::math::Vec3{
			entry.position.x + srcDim.x * entry.scale,
			entry.position.y + srcDim.y * entry.scale,
			entry.position.z + srcDim.z * entry.scale,
			0.0f,
		};
		instance.vertexBuffer = reg.gpu.vertexBuffer;
		instance.indexBuffer = reg.gpu.indexBuffer;
		instance.indexCount = reg.gpu.indexCount;
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

} // namespace
void SnapModelInstancesAboveGround(const VoxelWorld &world, RenderState *render)
{
	if (!render) {
		return;
	}

	const float worldMinX = static_cast<float>(world.floorMin.x);
	const float worldMinZ = static_cast<float>(world.floorMin.z);
	const float worldMaxX = static_cast<float>(world.floorMaxExclusive.x);
	const float worldMaxY = static_cast<float>(world.maxExclusive.y);
	const float worldMaxZ = static_cast<float>(world.floorMaxExclusive.z);

	for (ModelInstanceData &instance : render->modelInstances) {
		const float dimX = instance.worldAabbMax.x - instance.worldAabbMin.x;
		const float dimY = instance.worldAabbMax.y - instance.worldAabbMin.y;
		const float dimZ = instance.worldAabbMax.z - instance.worldAabbMin.z;
		if (dimX <= 0.0f || dimY <= 0.0f || dimZ <= 0.0f) {
			runtime::LogRuntimeFailure(
				"Model",
				"SnapModelInstancesAboveGround",
				fmt::format("degenerate AABB: aabbMin=({:.3f},{:.3f},{:.3f}) aabbMax=({:.3f},{:.3f},{:.3f}) dims=({:.3f},{:.3f},{:.3f})",
							instance.worldAabbMin.x, instance.worldAabbMin.y, instance.worldAabbMin.z,
							instance.worldAabbMax.x, instance.worldAabbMax.y, instance.worldAabbMax.z,
							dimX, dimY, dimZ));
			continue;
		}

		/// \brief Per-axis:
		///
		/// \details
		/// pick the snap path that satisfies both
		///  invariants on that axis. The "AABB max on grid" path

		///  wins when the operator's drag would make the model

		///  stick out; the "AABB min on grid" path wins otherwise.

		const float rawMaxX = instance.worldAabbMin.x + dimX;
		const float rawMaxY = instance.worldAabbMin.y + dimY;
		const float rawMaxZ = instance.worldAabbMin.z + dimZ;
		const bool xSticksOut = rawMaxX > worldMaxX;
		const bool ySticksOut = rawMaxY > worldMaxY;
		const bool zSticksOut = rawMaxZ > worldMaxZ;

		float newMinX = 0.0f;
		float newMinY = 0.0f;
		float newMinZ = 0.0f;
		float newMaxX = 0.0f;
		float newMaxY = 0.0f;
		float newMaxZ = 0.0f;

		if (xSticksOut) {
			newMaxX = std::floor(std::min(rawMaxX, worldMaxX));
			newMinX = newMaxX - dimX;
		} else {
			const float roundedX = std::round(std::clamp(instance.worldAabbMin.x, worldMinX, worldMaxX - dimX));
			if (roundedX + dimX <= worldMaxX) {
				newMinX = roundedX;
				newMaxX = roundedX + dimX;
			} else {
				/// \brief Rounded AABB min would push AABB max past
				///
				/// \details
				///  worldMax (dim is non-integer, rounded to

				///  the high side). Clamp AABB max to the

				///  world bound and derive AABB min.

				newMaxX = std::floor(worldMaxX);
				newMinX = newMaxX - dimX;
			}
		}

		/// \brief **Y axis.** The operator sets `position.y` explicitly;
		///
		/// \details
		///  the snap does NOT lift. If the AABB sticks out

		///  (the model is too tall for the world), snap the max

		///  to the world max. Otherwise leave Y alone — the

		///  operator's integer Y is preserved. Y doesn't get the

		///  post-round fit check because Y is operator-controlled

		///  and `position.y` is read verbatim (no `std::round`).

		if (ySticksOut) {
			newMaxY = std::floor(std::min(rawMaxY, worldMaxY));
			newMinY = newMaxY - dimY;
		} else {
			newMinY = instance.worldAabbMin.y;
			newMaxY = newMinY + dimY;
		}

		/// \brief **Z axis.** Same contract as X (with the post-round
		///
		/// \details
		///  fit check). For the lamp-post column with

		///  `dim.z=8.13` in an 18-voxel Z range, the operator

		///  can place the model at the front edge with

		///  `aabbMax.z=9` and `aabbMin.z=0.87` — the post-round

		///  fit check is what makes that placement survive the

		///  snap (without it, the snap would round `0.87 → 1`

		///  and the AABB would stick out 0.13 voxels).

		if (zSticksOut) {
			newMaxZ = std::floor(std::min(rawMaxZ, worldMaxZ));
			newMinZ = newMaxZ - dimZ;
		} else {
			const float roundedZ = std::round(std::clamp(instance.worldAabbMin.z, worldMinZ, worldMaxZ - dimZ));
			if (roundedZ + dimZ <= worldMaxZ) {
				newMinZ = roundedZ;
				newMaxZ = roundedZ + dimZ;
			} else {
				newMaxZ = std::floor(worldMaxZ);
				newMinZ = newMaxZ - dimZ;
			}
		}

		/// \brief Update the AABB in place.
		///
		/// \details
		/// AABB dim is preserved.
		instance.worldAabbMin.x = newMinX;
		instance.worldAabbMin.y = newMinY;
		instance.worldAabbMin.z = newMinZ;
		instance.worldAabbMax.x = newMaxX;
		instance.worldAabbMax.y = newMaxY;
		instance.worldAabbMax.z = newMaxZ;

		/// \brief Update the model basis translation column so the
		///
		/// \details
		///  rendered mesh stays aligned with the AABB. The load

		///  path's `aabbMinOffset = T(-srcMin*scale)` puts the

		///  translation at `pos - sourceAabbMin` (NOT `pos`); the

		///  GPU shader then adds `sourceAabbMin` back per vertex

		///  (or rather, the model-local vertex (0,0,0) lands at

		///  `pos` in world space). Re-derive the translation

		///  from the snapped AABB min by the same `-sourceAabbMin`

		///  offset, otherwise the rendered mesh would shift by

		///  `sourceAabbMin` from the operator's chosen AABB

		///  (e.g. snap to AABB min.x=-9 with srcMin.x=-2.36

		///  would render the model with its actual AABB at

		///  -9 + (-2.36) = -11.36).

		instance.modelTransform.c[3].x = newMinX - instance.sourceAabbMin[0];
		instance.modelTransform.c[3].y = newMinY - instance.sourceAabbMin[1];
		instance.modelTransform.c[3].z = newMinZ - instance.sourceAabbMin[2];
	}
}

void SnapModelInstancesCenterAnchored(
	const VoxelWorld &world,
	RenderState *render)
{
	if (!render) {
		return;
	}

	const float worldMinX = static_cast<float>(world.floorMin.x);
	const float worldMinY = static_cast<float>(world.min.y);
	const float worldMinZ = static_cast<float>(world.floorMin.z);
	const float worldMaxX = static_cast<float>(world.floorMaxExclusive.x);
	const float worldMaxY = static_cast<float>(world.maxExclusive.y);
	const float worldMaxZ = static_cast<float>(world.floorMaxExclusive.z);

	for (ModelInstanceData &instance : render->modelInstances) {
		const float dimX = instance.worldAabbMax.x - instance.worldAabbMin.x;
		const float dimY = instance.worldAabbMax.y - instance.worldAabbMin.y;
		const float dimZ = instance.worldAabbMax.z - instance.worldAabbMin.z;
		if (dimX <= 0.0f || dimY <= 0.0f || dimZ <= 0.0f) {
			runtime::LogRuntimeFailure(
				"Model",
				"SnapModelInstancesCenterAnchored",
				fmt::format("degenerate AABB: aabbMin=({:.3f},{:.3f},{:.3f}) aabbMax=({:.3f},{:.3f},{:.3f}) dims=({:.3f},{:.3f},{:.3f})",
							instance.worldAabbMin.x, instance.worldAabbMin.y, instance.worldAabbMin.z,
							instance.worldAabbMax.x, instance.worldAabbMax.y, instance.worldAabbMax.z,
							dimX, dimY, dimZ));
			continue;
		}

		/// \brief Re-anchor:
		///
		/// \details
		/// AABB min ↔ AABB centre. The load path
		///  stored the translation at the AABB min, but the

		///  centred snap treats the current AABB **centre** as

		///  the snap target. Compute the centre, snap to the

		///  integer grid (X, Y, Z), clamp to the world bounds

		///  so the AABB fits, then derive the new AABB min.

		float centerX = 0.5f * (instance.worldAabbMin.x + instance.worldAabbMax.x);
		float centerY = 0.5f * (instance.worldAabbMin.y + instance.worldAabbMax.y);
		float centerZ = 0.5f * (instance.worldAabbMin.z + instance.worldAabbMax.z);

		/// \brief Snap to integer voxel grid (all 3 axes; the
		///
		/// \details
		///  operator's `position` becomes the integer centre).

		centerX = std::round(centerX);
		centerY = std::round(centerY);
		centerZ = std::round(centerZ);

		/// \brief Clamp the centre so the resulting AABB fits within
		///
		/// \details
		///  the world bounds: centre must be in

		///  [worldMin + dim/2, worldMax - dim/2].

		const float halfDimX = 0.5f * dimX;
		const float halfDimY = 0.5f * dimY;
		const float halfDimZ = 0.5f * dimZ;
		centerX = std::clamp(centerX, worldMinX + halfDimX, worldMaxX - halfDimX);
		centerY = std::clamp(centerY, worldMinY + halfDimY, worldMaxY - halfDimY);
		centerZ = std::clamp(centerZ, worldMinZ + halfDimZ, worldMaxZ - halfDimZ);

		/// \brief Derive the new AABB from the clamped centre.
		///
		/// \details
		/// The
		///  AABB dims are preserved; the AABB min shifts

		///  outward by the same amount the centre shifts.

		const float newMinX = centerX - halfDimX;
		const float newMinY = centerY - halfDimY;
		const float newMinZ = centerZ - halfDimZ;
		instance.worldAabbMin.x = newMinX;
		instance.worldAabbMin.y = newMinY;
		instance.worldAabbMin.z = newMinZ;
		instance.worldAabbMax.x = newMinX + dimX;
		instance.worldAabbMax.y = newMinY + dimY;
		instance.worldAabbMax.z = newMinZ + dimZ;

		/// \brief Update the model basis translation column.
		///
		/// \details
		/// Same
		///  `-sourceAabbMin` offset as the bottom-anchored snap

		///  above — the load path's aabbMinOffset puts the

		///  translation at `pos - sourceAabbMin`, and the snap

		///  must preserve that relationship.

		instance.modelTransform.c[3].x = newMinX - instance.sourceAabbMin[0];
		instance.modelTransform.c[3].y = newMinY - instance.sourceAabbMin[1];
		instance.modelTransform.c[3].z = newMinZ - instance.sourceAabbMin[2];
	}
}

void SnapModelInstancesAboveGroundDispatch(
	const VoxelWorld &world,
	RenderState *render)
{
	const char *snapMode = std::getenv("PROJECTV_MODEL_SNAP");
	if (snapMode != nullptr && std::string(snapMode) == "centre") {
		/// \brief Centre-anchored snap first (centred → integer grid),
		///
		/// \details
		///  then the bottom-anchored default to enforce clamp-to-

		///  world on the resulting AABB. The order doesn't

		///  matter for the final AABB (both are idempotent) but

		///  centred-first gives an `AABB centre` at the integer

		///  grid, while bottom-first gives an `AABB min` at the

		///  integer grid; the centred snap is the user-visible

		///  semantic when the env var is set.

		SnapModelInstancesCenterAnchored(world, render);
	}
	SnapModelInstancesAboveGround(world, render);
}

} // namespace projectv::asset
