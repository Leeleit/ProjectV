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
		const glm::vec3 srcMin = glm::vec3(reg.aabbMin[0], reg.aabbMin[1], reg.aabbMin[2]);
		const glm::vec3 srcMax = glm::vec3(reg.aabbMax[0], reg.aabbMax[1], reg.aabbMax[2]);
		const glm::vec3 center = (srcMin + srcMax) * 0.5f * entry.scale + entry.position;
		const glm::vec3 half = (srcMax - srcMin) * 0.5f * entry.scale;
		instance.worldAabbMin = { center.x - half.x, center.y - half.y, center.z - half.z };
		instance.worldAabbMax = { center.x + half.x, center.y + half.y, center.z + half.z };
		instance.vertexBuffer = reg.gpu.vertexBuffer;
		instance.indexBuffer = reg.gpu.indexBuffer;
		instance.indexCount = reg.gpu.indexCount;
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
// sampled the center. Returns `INT_MIN` if any sample lands
// over fully-Air.
int32_t FindFloorSurfaceYForAabb(const VoxelWorld &world, float minX, float maxX, float minZ, float maxZ)
{
	const auto trySample = [&](float fx, float fz) -> int32_t {
		const int32_t x = static_cast<int32_t>(std::floor(fx));
		const int32_t z = static_cast<int32_t>(std::floor(fz));
		return FindBottomVoxelYAtXZ(world, x, z);
	};
	const int32_t a = trySample(minX, minZ);
	const int32_t b = trySample(maxX, minZ);
	const int32_t c = trySample(minX, maxZ);
	const int32_t d = trySample(maxX, maxZ);
	const int32_t e = trySample(0.5f * (minX + maxX), 0.5f * (minZ + maxZ));
	if (a == std::numeric_limits<int32_t>::min() ||
		b == std::numeric_limits<int32_t>::min() ||
		c == std::numeric_limits<int32_t>::min() ||
		d == std::numeric_limits<int32_t>::min() ||
		e == std::numeric_limits<int32_t>::min()) {
		return std::numeric_limits<int32_t>::min();
	}
	return std::max({a, b, c, d, e});
}

} // namespace

void SnapModelInstancesAboveGround(const VoxelWorld &world, RenderState *render)
{
	if (!render) {
		return;
	}
	// Column-major `glm::mat4` storage in `ModelInstanceData::modelTransform`
	// (matches `StoreMatrixColumnMajor` in this file), so translation
	// lives at indices [12, 13, 14]. Updating those three floats
	// is enough to translate the instance; the rotation / scale
	// basis (columns 0..2) is left untouched.
	for (ModelInstanceData &instance : render->modelInstances) {
		const float currentBottomY = instance.worldAabbMin[1];
		const float currentTopY = instance.worldAabbMax[1];
		const float modelHeight = std::max(currentTopY - currentBottomY, 0.0f);
		const int32_t topVoxelY = FindFloorSurfaceYForAabb(
			world,
			instance.worldAabbMin[0],
			instance.worldAabbMax[0],
			instance.worldAabbMin[2],
			instance.worldAabbMax[2]);
		if (topVoxelY == std::numeric_limits<int32_t>::min()) {
			// No ground under this instance (empty scene / floating
			// in air). Skip — the operator-visible position is left
			// at whatever the manifest said, no spurious snapping to
			// Y=0 or Y=INT_MIN.
			continue;
		}
		// The voxel at `topVoxelY` is the solid floor; the world
		// surface is the TOP of that voxel, i.e. integer
		// `topVoxelY + 1`. The model's bottom should land there.
		const float targetBottomY = static_cast<float>(topVoxelY + 1);
		const float liftY = targetBottomY - currentBottomY;
		if (std::abs(liftY) > 1e-4f) {
			// Translate the model by liftY. Column-major: translation
			// is the 4th column.
			instance.modelTransform[13] += liftY;
			instance.worldAabbMin[1] += liftY;
			instance.worldAabbMax[1] += liftY;
		}

		// Voxel-grid XZ snap ("нормировка gltf по гриду вокселей"):
		// the manifest `position` is the model's geometric center
		// (per `BuildEntryWorldMatrix`), but for a 1x1x1 box centered
		// at integer coordinates the box's vertices are at
		// `integer ± 0.5`, which means the box straddles 4 voxel
		// columns and the model sits on the seam between voxels, not
		// on a single voxel column ("его центр это центр четырёх
		// вокселей, это не по сетке"). Snap XZ to `floor + 0.5` so
		// vertices align with the voxel grid (vertices at integer
		// coordinates, box sits cleanly on a single voxel column).
		// The Y is already snapped to `targetBottomY = topVoxelY + 1`
		// above, so the liftY in the Y is unchanged — but the XZ
		// shift is independent of the lift.
		const float currentCenterX = 0.5f * (instance.worldAabbMin[0] + instance.worldAabbMax[0]);
		const float currentCenterZ = 0.5f * (instance.worldAabbMin[2] + instance.worldAabbMax[2]);
		const float targetCenterX = std::floor(currentCenterX) + 0.5f;
		const float targetCenterZ = std::floor(currentCenterZ) + 0.5f;
		const float shiftX = targetCenterX - currentCenterX;
		const float shiftZ = targetCenterZ - currentCenterZ;
		if (std::abs(shiftX) > 1e-4f) {
			instance.modelTransform[12] += shiftX;
			instance.worldAabbMin[0] += shiftX;
			instance.worldAabbMax[0] += shiftX;
		}
		if (std::abs(shiftZ) > 1e-4f) {
			instance.modelTransform[14] += shiftZ;
			instance.worldAabbMin[2] += shiftZ;
			instance.worldAabbMax[2] += shiftZ;
		}
	}
}

} // namespace projectv::asset
