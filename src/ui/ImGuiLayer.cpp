#include "ui/ImGuiLayer.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanBootstrap.hpp"
#include "ui/HudStyle.hpp"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include "volk.h"

#include <cstdio>

namespace projectv::ui {
namespace {
bool g_imguiReady = false;
bool g_imguiFrameOpen = false;
VkFormat g_imguiColorFormat = VK_FORMAT_UNDEFINED;

void CheckVkResult(const VkResult err)
{
	if (err == VK_SUCCESS) {
		return;
	}
	std::fprintf(stderr, "[ProjectV][ImGui] Vulkan error: %d\n", static_cast<int>(err));
	if (err < 0) {
		runtime::LogVkFailure("ImGuiLayer.CheckVkResult", err);
	}
}

void ConfigureDynamicRenderingPipeline(const VkFormat colorFormat)
{
	g_imguiColorFormat = colorFormat;
	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachmentFormats = &g_imguiColorFormat;

	ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
	pipelineInfo.PipelineRenderingCreateInfo = renderingInfo;
	pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
}
} // namespace

bool InitImGuiLayer(AppState &state)
{
	if (g_imguiReady) {
		return true;
	}
	if (!state.platform().window ||
		state.context().device == VK_NULL_HANDLE ||
		state.swapchain().images.empty()) {
		runtime::LogRuntimeFailure("UI", "InitImGuiLayer", "window/device/swapchain unavailable");
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr; // EVIL: skip imgui.ini writes into cwd during sandbox/dev runs

	ApplyGameDevHudStyle();

	if (!ImGui_ImplSDL3_InitForVulkan(state.platform().window)) {
		runtime::LogRuntimeFailure("UI", "InitImGuiLayer", "ImGui_ImplSDL3_InitForVulkan failed");
		ImGui::DestroyContext();
		return false;
	}

	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.ApiVersion = projectv::render::GetMinVulkanApiVersion();
	initInfo.Instance = state.context().instance;
	initInfo.PhysicalDevice = state.context().physicalDevice;
	initInfo.Device = state.context().device;
	initInfo.QueueFamily = state.context().queueFamilyIndex;
	initInfo.Queue = state.context().queue;
	initInfo.DescriptorPoolSize = 64;
	initInfo.MinImageCount = 2;
	initInfo.ImageCount = static_cast<uint32_t>(state.swapchain().images.size());
	initInfo.PipelineCache = state.context().pipelineCache;
	initInfo.UseDynamicRendering = true;
	initInfo.CheckVkResultFn = CheckVkResult;
	initInfo.MinAllocationSize = 1024 * 1024;

	g_imguiColorFormat = state.swapchain().format;
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &g_imguiColorFormat;
	initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	if (!ImGui_ImplVulkan_Init(&initInfo)) {
		runtime::LogRuntimeFailure("UI", "InitImGuiLayer", "ImGui_ImplVulkan_Init failed");
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	g_imguiReady = true;
	return true;
}

void ShutdownImGuiLayer(AppState &state)
{
	(void)state;
	if (!g_imguiReady) {
		return;
	}
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	g_imguiReady = false;
	g_imguiFrameOpen = false;
	g_imguiColorFormat = VK_FORMAT_UNDEFINED;
}

void OnImGuiSwapchainRecreated(AppState &state)
{
	if (!g_imguiReady || state.swapchain().images.empty()) {
		return;
	}
	ImGui_ImplVulkan_SetMinImageCount(2);
	ConfigureDynamicRenderingPipeline(state.swapchain().format);
}

void ImGuiProcessEvent(const SDL_Event *event)
{
	if (!g_imguiReady || !event) {
		return;
	}
	ImGui_ImplSDL3_ProcessEvent(event);
}

void ImGuiNewFrame()
{
	if (!g_imguiReady) {
		return;
	}
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	g_imguiFrameOpen = true;
}

void ImGuiRenderDrawData(VkCommandBuffer cmd)
{
	if (!g_imguiReady || cmd == VK_NULL_HANDLE) {
		return;
	}
	if (g_imguiFrameOpen) {
		ImGui::Render();
		g_imguiFrameOpen = false;
	}
	ImDrawData *drawData = ImGui::GetDrawData();
	if (drawData != nullptr && drawData->CmdListsCount > 0) {
		ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
	}
}

void ImGuiEndFrameIfOpen()
{
	if (g_imguiReady && g_imguiFrameOpen) {
		ImGui::EndFrame();
		g_imguiFrameOpen = false;
	}
}

bool ImGuiWantCaptureKeyboard()
{
	return g_imguiReady && ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGuiWantCaptureMouse()
{
	return g_imguiReady && ImGui::GetIO().WantCaptureMouse;
}

bool IsImGuiLayerReady()
{
	return g_imguiReady;
}

} // namespace projectv::ui
