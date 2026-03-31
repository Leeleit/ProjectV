#include "VulkanBootstrap.hpp"
#include "VulkanMesh.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanSwapchain.hpp"

bool InitVulkan(AppState *state)
{
	// Сначала поднимаем базу: окно, instance, device, allocator и синхронизацию.
	if (!InitializeVulkanBase(state)) {
		return false;
	}

	// Потом создаём swapchain, потому что именно он задаёт размер и формат кадра.
	if (!RecreateSwapchain(state)) {
		return false;
	}

	// Загружаем учебную геометрию на GPU, пока у нас ещё нет сложной сцены.
	const MeshCpu triangleCpu = CreateTriangleMeshCpu();
	if (!UploadMeshToGpu(state, triangleCpu, &state->sceneMesh)) {
		SDL_Log("UploadMeshToGpu failed");
		return false;
	}

	// Финальный шаг инициализации: собираем графический pipeline под формат swapchain.
	if (!CreateGraphicsPipeline(state)) {
		SDL_Log("CreateGraphicsPipeline failed");
		return false;
	}

	return true;
}
