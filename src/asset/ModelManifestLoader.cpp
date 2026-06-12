#include "asset/ModelManifestLoader.hpp"

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

} // namespace projectv::asset
