// Пример: ImGui с SDL3 и Vulkan
// Документация: docs/imgui/quickstart.md, docs/imgui/integration.md
// Этот пример показывает полную интеграцию Dear ImGui в Vulkan-приложение с SDL3.
// Включает: инициализацию ImGui, загрузку шрифтов, обработку DPI, основные виджеты,
// интеграцию с Tracy для профилирования (опционально).

#define NOMINMAX
#define VK_NO_PROTOTYPES
#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_vulkan.h"
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "imgui_stdlib.h" // для InputText с std::string

// Tracy (опционально) - для профилирования ImGui
#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#endif

#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>
#include <vector>

// ============================================================================
// Структура состояния приложения
// ============================================================================
struct AppState {
	// SDL и Vulkan
	SDL_Window *window = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;

	// Swapchain
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D extent = {};
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkFramebuffer> framebuffers;

	// Render pass и pipeline
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	// Синхронизация
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;

	// Командный буфер
	VkCommandPool commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers;

	// Descriptor pool (для ImGui и возможных текстур)
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	// ImGui
	ImGuiContext *imguiContext = nullptr;
	bool showDemoWindow = true;
	bool showMetricsWindow = false;
	bool showStyleEditor = false;
	float sliderValue = 0.5f;
	std::string textInput = "Hello ImGui!";
	int counter = 0;

	// DPI масштабирование
	float dpiScale = 1.0f;

	// Состояние приложения
	bool running = true;
};

// ============================================================================
// Вспомогательные функции Vulkan
// ============================================================================
static std::vector<uint32_t> loadSpirv(const char *path)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f)
		return {};
	auto size = f.tellg();
	f.seekg(0);
	if (size <= 0 || (size % 4) != 0)
		return {};
	std::vector<uint32_t> code(size / 4);
	f.read(reinterpret_cast<char *>(code.data()), size);
	return code;
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t> &code)
{
	if (code.empty())
		return VK_NULL_HANDLE;
	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code.size() * sizeof(uint32_t);
	info.pCode = code.data();
	VkShaderModule module = VK_NULL_HANDLE;
	vkCreateShaderModule(device, &info, nullptr, &module);
	return module;
}

// ============================================================================
// Очистка ресурсов Vulkan и ImGui
// ============================================================================
static void cleanupAppState(AppState *state)
{
	// Ждём завершения всех операций на GPU
	vkDeviceWaitIdle(state->device);

	// Shutdown ImGui (важен порядок!)
	if (state->imguiContext) {
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext(state->imguiContext);
		state->imguiContext = nullptr;
	}

	// Vulkan ресурсы
	if (state->descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(state->device, state->descriptorPool, nullptr);
	}

	vkDestroySemaphore(state->device, state->imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(state->device, state->renderFinishedSemaphore, nullptr);
	vkDestroyFence(state->device, state->inFlightFence, nullptr);

	vkDestroyCommandPool(state->device, state->commandPool, nullptr);

	for (const VkFramebuffer fb : state->framebuffers) {
		vkDestroyFramebuffer(state->device, fb, nullptr);
	}

	vkDestroyPipeline(state->device, state->pipeline, nullptr);
	vkDestroyPipelineLayout(state->device, state->pipelineLayout, nullptr);
	vkDestroyRenderPass(state->device, state->renderPass, nullptr);

	for (const VkImageView iv : state->swapchainImageViews) {
		vkDestroyImageView(state->device, iv, nullptr);
	}

	vkDestroySwapchainKHR(state->device, state->swapchain, nullptr);
	vmaDestroyAllocator(state->allocator);
	vkDestroyDevice(state->device, nullptr);
	vkDestroySurfaceKHR(state->instance, state->surface, nullptr);

	if (state->window) {
		SDL_DestroyWindow(state->window);
	}

	vkDestroyInstance(state->instance, nullptr);

	SDL_Quit();
}

// ============================================================================
// Инициализация ImGui
// ============================================================================
static bool initImGui(AppState *state)
{
	// Проверка версии ImGui (рекомендуется)
	IMGUI_CHECKVERSION();

	// Создание контекста ImGui
	state->imguiContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(state->imguiContext);
	ImGuiIO &io = ImGui::GetIO();

	// Конфигурация ImGui
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Включить навигацию с клавиатуры
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Включить навигацию с геймпада
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	  // Включить докинг (опционально)

	// Настройка стилей
	ImGui::StyleColorsDark(); // Тёмная тема (также доступны StyleColorsLight, StyleColorsClassic)

	// Масштабирование для DPI
	ImGuiStyle &style = ImGui::GetStyle();
	style.ScaleAllSizes(state->dpiScale);
	io.FontGlobalScale = state->dpiScale;

	// Загрузка шрифтов (пример)
	// io.Fonts->AddFontDefault();  // Шрифт по умолчанию
	// io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 16.0f * state->dpiScale);

	// Инициализация Platform Backend (SDL3)
	if (!ImGui_ImplSDL3_InitForVulkan(state->window)) {
		return false;
	}

	// Инициализация Renderer Backend (Vulkan)
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = state->instance;
	init_info.PhysicalDevice = state->physicalDevice;
	init_info.Device = state->device;
	init_info.QueueFamily = state->queueFamilyIndex;
	init_info.Queue = state->queue;
	init_info.DescriptorPool = state->descriptorPool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = (uint32_t)state->swapchainImages.size();
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	// Render pass для ImGui (должен совпадать с тем, что используется для рендеринга)
	init_info.PipelineInfoMain.RenderPass = state->renderPass;
	init_info.PipelineInfoMain.Subpass = 0;

	if (!ImGui_ImplVulkan_Init(&init_info)) {
		return false;
	}

	// Загрузка шрифтовой текстуры в GPU
	ImGui_ImplVulkan_CreateFontsTexture();

	return true;
}

// ============================================================================
// Основные callback'и SDL
// ============================================================================
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	AppState *state = new AppState();
	*appstate = state;

	// Инициализация SDL
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
		delete state;
		return SDL_APP_FAILURE;
	}

	// Создание окна с поддержкой Vulkan
	state->window = SDL_CreateWindow("ImGui + SDL3 + Vulkan", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!state->window) {
		SDL_Log("Failed to create window: %s", SDL_GetError());
		delete state;
		return SDL_APP_FAILURE;
	}

	// Получение DPI масштаба
	state->dpiScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	SDL_Log("DPI scale: %.2f", state->dpiScale);

	// Инициализация volk
	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("Failed to initialize volk");
		cleanupAppState(state);
		return SDL_APP_FAILURE;
	}

	// Создание Vulkan instance (упрощённо, для примера)
	// В реальном приложении нужно получить расширения через SDL_Vulkan_GetInstanceExtensions
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "ImGui Example";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "ProjectV";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	const char *instanceExtensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME // для отладки
	};

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = sizeof(instanceExtensions) / sizeof(instanceExtensions[0]);
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions;

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &state->instance) != VK_SUCCESS) {
		SDL_Log("Failed to create Vulkan instance");
		cleanupAppState(state);
		return SDL_APP_FAILURE;
	}

	// Загрузка функций instance через volk
	volkLoadInstance(state->instance);

	// Создание поверхности
	if (!SDL_Vulkan_CreateSurface(state->window, state->instance, nullptr, &state->surface)) {
		SDL_Log("Failed to create Vulkan surface");
		cleanupAppState(state);
		return SDL_APP_FAILURE;
	}

	// Выбор физического устройства (упрощённо)
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(state->instance, &physicalDeviceCount, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(state->instance, &physicalDeviceCount, physicalDevices.data());

	if (physicalDevices.empty()) {
		SDL_Log("No Vulkan physical devices found");
		cleanupAppState(state);
		return SDL_APP_FAILURE;
	}

	state->physicalDevice = physicalDevices[0]; // Просто берём первое устройство

	// Создание логического устройства (упрощённо)
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = 0; // Должен быть определён правильно
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

	if (vkCreateDevice(state->physicalDevice, &deviceCreateInfo, nullptr, &state->device) != VK_SUCCESS) {
		SDL_Log("Failed to create Vulkan device");
		cleanupAppState(state);
		return SDL_APP_FAILURE;
	}

	// Загрузка функций device через volk
	volkLoadDevice(state->device);

	// Получение очереди
	vkGetDeviceQueue(state->device, 0, 0, &state->queue);
	state->queueFamilyIndex = 0;

	// Создание VMA аллокатора
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = state->physicalDevice;
	allocatorInfo.device = state->device;
	allocatorInfo.instance = state->instance;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

	if (vmaCreateAllocator(&allocatorInfo, &state->allocator) != VK_SUCCESS) {
		SDL_Log("Failed to create VMA allocator");
		cleanupAppState(state);
		return SDL_APP_FAILURE;
	}

	// Создание swapchain, render pass, framebuffers и т.д.
	// (упрощённо, в реальном приложении нужно реализовать полностью)
	// Для примера просто создадим минимальные структуры

	// Создание descriptor pool для ImGui
	VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = pool_sizes;

	if (vkCreateDescriptorPool(state->device, &pool_info, nullptr, &state->descriptorPool) != VK_SUCCESS) {
		SDL_Log("Failed to create descriptor pool");
		cleanupAppState(state);
		return SDL_APP_FAILURE;
	}

	// Инициализация ImGui
	if (!initImGui(state)) {
		SDL_Log("Failed to initialize ImGui");
		cleanupAppState(state);
		return SDL_APP_FAILURE;
	}

	SDL_Log("ImGui + SDL3 + Vulkan initialized successfully");
	return SDL_APP_SUCCESS;
}

void SDL_AppEvent(void *appstate, const SDL_Event *event)
{
	AppState *state = static_cast<AppState *>(appstate);

	// Передача события в ImGui
	ImGui_ImplSDL3_ProcessEvent(event);

	// Обработка событий приложения
	switch (event->type) {
	case SDL_EVENT_QUIT:
		state->running = false;
		break;

	case SDL_EVENT_KEY_DOWN:
		if (event->key.key == SDLK_ESCAPE) {
			state->running = false;
		}
		break;

	case SDL_EVENT_WINDOW_RESIZED:
		// При изменении размера окна нужно пересоздать swapchain
		// (в этом примере не реализовано для простоты)
		break;
	}
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
#ifdef TRACY_ENABLE
	ZoneScoped; // Tracy: профилирование фрейма
#endif

	AppState *state = static_cast<AppState *>(appstate);

	if (!state->running) {
		return SDL_APP_SUCCESS; // Завершение приложения
	}

	// ============================================================================
	// 1. Начало кадра ImGui (важен порядок!)
	// ============================================================================
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

#ifdef TRACY_ENABLE
	ZoneScopedN("ImGui Widgets"); // Tracy: профилирование виджетов
#endif

	// ============================================================================
	// 2. Создание UI (ваши виджеты)
	// ============================================================================

	// Главное меню
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit", "Esc")) {
				state->running = false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Windows")) {
			ImGui::MenuItem("Demo Window", nullptr, &state->showDemoWindow);
			ImGui::MenuItem("Metrics Window", nullptr, &state->showMetricsWindow);
			ImGui::MenuItem("Style Editor", nullptr, &state->showStyleEditor);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("About")) {
				ImGui::OpenPopup("About");
			}
			ImGui::EndMenu();
		}

		// Статус в меню-баре
		ImGui::SameLine(ImGui::GetWindowWidth() - 200);
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

		ImGui::EndMainMenuBar();
	}

	// Демо-окно ImGui (показывает все виджеты)
	if (state->showDemoWindow) {
		ImGui::ShowDemoWindow(&state->showDemoWindow);
	}

	// Окно метрик
	if (state->showMetricsWindow) {
		ImGui::ShowMetricsWindow(&state->showMetricsWindow);
	}

	// Редактор стилей
	if (state->showStyleEditor) {
		ImGui::Begin("Style Editor", &state->showStyleEditor);
		ImGui::ShowStyleEditor();
		ImGui::End();
	}

	// Пример окна с различными виджетами
	{
		ImGui::Begin("ImGui Example Window");

		ImGui::Text("Hello, world!");
		ImGui::Text("DPI Scale: %.2f", state->dpiScale);
		ImGui::Separator();

		// Кнопка с счётчиком
		if (ImGui::Button("Click me!")) {
			state->counter++;
		}
		ImGui::SameLine();
		ImGui::Text("Counter: %d", state->counter);

		// Слайдер
		ImGui::SliderFloat("Slider", &state->sliderValue, 0.0f, 1.0f);

		// Ввод текста (требует imgui_stdlib.cpp в сборке)
		ImGui::InputText("Text input", &state->textInput);

		// Чекбоксы
		static bool checkbox1 = true;
		static bool checkbox2 = false;
		ImGui::Checkbox("Checkbox 1", &checkbox1);
		ImGui::Checkbox("Checkbox 2", &checkbox2);

		// Радио-кнопки
		static int radioOption = 0;
		ImGui::RadioButton("Option A", &radioOption, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Option B", &radioOption, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Option C", &radioOption, 2);

		// Цветовой редактор
		static ImVec4 color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		ImGui::ColorEdit3("Color", (float *)&color);

		// Таблица
		if (ImGui::BeginTable("table", 3, ImGuiTableFlags_Borders)) {
			ImGui::TableSetupColumn("Column A");
			ImGui::TableSetupColumn("Column B");
			ImGui::TableSetupColumn("Column C");
			ImGui::TableHeadersRow();

			for (int row = 0; row < 4; row++) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("A%d", row);
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("B%d", row);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("C%d", row);
			}

			ImGui::EndTable();
		}

		// Прогресс-бар
		ImGui::ProgressBar(state->sliderValue, ImVec2(-FLT_MIN, 0), "Progress");

		ImGui::End();
	}

	// Окно "About"
	if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("ImGui + SDL3 + Vulkan Example");
		ImGui::Text("Part of ProjectV documentation");
		ImGui::Separator();
		ImGui::Text("Version: 1.0.0");
		ImGui::Text("ImGui Version: %s", IMGUI_VERSION);

		if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// ============================================================================
	// 3. Рендеринг ImGui
	// ============================================================================
#ifdef TRACY_ENABLE
	ZoneScopedN("ImGui Render"); // Tracy: профилирование рендеринга
#endif

	ImGui::Render();

	// ============================================================================
	// 4. Vulkan рендеринг (упрощённо)
	// ============================================================================
	// В реальном приложении здесь будет:
	// 1. vkAcquireNextImageKHR
	// 2. vkBeginCommandBuffer
	// 3. vkCmdBeginRenderPass
	// 4. ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer)
	// 5. vkCmdEndRenderPass
	// 6. vkEndCommandBuffer
	// 7. vkQueueSubmit
	// 8. vkQueuePresentKHR

	// Для примера просто проверяем, что данные для отрисовки валидны
	ImDrawData *draw_data = ImGui::GetDrawData();
	if (draw_data && draw_data->DisplaySize.x > 0 && draw_data->DisplaySize.y > 0) {
		// Здесь должен быть код Vulkan рендеринга
		// ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
	}

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	AppState *state = static_cast<AppState *>(appstate);

	SDL_Log("Shutting down...");
	cleanupAppState(state);

	delete state;
}

// ============================================================================
// Точка входа (SDL3 с callback'ами)
// ============================================================================
SDL_AppResult SDL_AppMain(int argc, char *argv[])
{
	void *appstate = nullptr;
	SDL_AppResult init_result = SDL_AppInit(&appstate, argc, argv);

	if (init_result != SDL_APP_SUCCESS) {
		return init_result;
	}

	SDL_AppResult iterate_result = SDL_APP_CONTINUE;
	while (iterate_result == SDL_APP_CONTINUE) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			SDL_AppEvent(appstate, &event);
		}

		iterate_result = SDL_AppIterate(appstate);

		// Небольшая задержка для снижения нагрузки на CPU
		SDL_Delay(16); // ~60 FPS
	}

	SDL_AppQuit(appstate, iterate_result);
	return iterate_result;
}
