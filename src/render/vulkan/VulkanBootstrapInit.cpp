#include "volk.h" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "render/vulkan/VulkanBootstrapInternal.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include "SDL3/SDL_vulkan.h"
#include "fmt/format.h"

#include "core/EnvUtils.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {
inline constexpr char PROJECT_NAME[] = "ProjectV v0.0.1";

inline constexpr uint32_t kDefaultMinVulkanApiVersion = VK_API_VERSION_1_4;

bool EnvTokenEqualsIgnoreCase(const char *value, const char *token)
{
	if (value == nullptr || token == nullptr) {
		return false;
	}
	while (*value != '\0' && *token != '\0') {
		const unsigned char left = static_cast<unsigned char>(*value);
		const unsigned char right = static_cast<unsigned char>(*token);
		if (std::tolower(left) != std::tolower(right)) {
			return false;
		}
		++value;
		++token;
	}
	return *value == '\0' && *token == '\0';
}

bool IsEnvTruthy(const char *value)
{
	return EnvTokenEqualsIgnoreCase(value, "1") || EnvTokenEqualsIgnoreCase(value, "true") ||
		   EnvTokenEqualsIgnoreCase(value, "yes") || EnvTokenEqualsIgnoreCase(value, "on");
}

bool IsEnvFalsy(const char *value)
{
	return EnvTokenEqualsIgnoreCase(value, "0") || EnvTokenEqualsIgnoreCase(value, "false") ||
		   EnvTokenEqualsIgnoreCase(value, "no") || EnvTokenEqualsIgnoreCase(value, "off");
}

// Compile-time ON builds still honor PROJECTV_ENABLE_VALIDATION=OFF at runtime (Nsight/RenderDoc).
bool WantValidationLayers()
{
#if !PROJECTV_ENABLE_VALIDATION
	return false;
#else
	const char *const value = projectv::core::GetEnvVar("PROJECTV_ENABLE_VALIDATION");
	if (value == nullptr || *value == '\0') {
		return true;
	}
	if (IsEnvFalsy(value)) {
		return false;
	}
	if (IsEnvTruthy(value)) {
		return true;
	}
	return true;
#endif
}

uint32_t ParseVulkanApiVersionString(const std::string_view raw)
{
	if (raw.empty()) {
		return kDefaultMinVulkanApiVersion;
	}
	uint32_t major = 0;
	uint32_t minor = 0;
	uint32_t patch = 0;
	const std::size_t firstDot = raw.find('.');
	if (firstDot == std::string_view::npos) {
		try {
			major = static_cast<uint32_t>(std::stoul(std::string{raw}));
		} catch (const std::exception &) {
			return kDefaultMinVulkanApiVersion;
		}
	} else {
		const std::string_view majorPart = raw.substr(0, firstDot);
		const std::string_view rest = raw.substr(firstDot + 1);
		const std::size_t secondDot = rest.find('.');
		try {
			major = static_cast<uint32_t>(std::stoul(std::string{majorPart}));
			if (secondDot == std::string_view::npos) {
				minor = static_cast<uint32_t>(std::stoul(std::string{rest}));
			} else {
				minor = static_cast<uint32_t>(std::stoul(std::string{rest.substr(0, secondDot)}));
				patch = static_cast<uint32_t>(std::stoul(std::string{rest.substr(secondDot + 1)}));
			}
		} catch (const std::exception &) {
			return kDefaultMinVulkanApiVersion;
		}
	}
	return VK_MAKE_API_VERSION(0, major, minor, patch);
}

constexpr std::array<const char *, 1> kValidationLayers{"VK_LAYER_KHRONOS_validation"};

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	const VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *)
{
	SDL_Log("Vulkan validation: [%u][%u] %s",
			static_cast<unsigned>(messageSeverity),
			static_cast<unsigned>(messageTypes),
			pCallbackData && pCallbackData->pMessage ? pCallbackData->pMessage : "no message");
	return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = DebugCallback;
	return info;
}

bool CreateDebugMessenger(VulkanContextState *context)
{
	if (!WantValidationLayers()) {
		(void)context;
		return true;
	}
	const VkDebugUtilsMessengerCreateInfoEXT info = MakeDebugMessengerCreateInfo();
	const VkResult createDebugMessengerResult =
		vkCreateDebugUtilsMessengerEXT(context->instance, &info, nullptr, &context->debugMessenger);
	if (createDebugMessengerResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateDebugMessenger.vkCreateDebugUtilsMessengerEXT", createDebugMessengerResult);
		return false;
	}
	return true;
}

bool CheckValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> available(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, available.data());

	for (const char *requiredLayer : kValidationLayers) {
		bool found = false;
		for (const auto &layer : available) {
			if (std::strcmp(requiredLayer, layer.layerName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			runtime::LogRuntimeFailure(
				"Init",
				"CheckValidationLayerSupport",
				fmt::format("missing validation layer: {}", requiredLayer));
			return false;
		}
	}

	return true;
}

} // namespace

namespace projectv::render {

uint32_t GetMinVulkanApiVersion()
{
	const char *overrideValue = projectv::core::GetEnvVar("PROJECTV_MIN_VULKAN_API_VERSION");
	if (overrideValue == nullptr) {
		return kDefaultMinVulkanApiVersion;
	}
	return ParseVulkanApiVersionString(std::string_view{overrideValue});
}

} // namespace projectv::render

bool InitializeVulkanInstance(
	PlatformState *platform,
	VulkanContextState *context)
{
	PV_CHECK_OR_RETURN(
		platform && context,
		"Init",
		"InitializeVulkanInstance.Preconditions",
		"platform/context is null");
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		runtime::LogSdlFailure("InitializeVulkanInstance.SDL_Init");
		return false;
	}

	platform->window = SDL_CreateWindow(PROJECT_NAME, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!platform->window) {
		runtime::LogSdlFailure("InitializeVulkanInstance.SDL_CreateWindow");
		return false;
	}
	{
		const char *const fullscreenEnv = projectv::core::GetEnvVar("PROJECTV_FULLSCREEN");
		if (IsEnvTruthy(fullscreenEnv)) {
			if (!SDL_SetWindowFullscreen(platform->window, true)) {
				runtime::LogSdlFailure("InitializeVulkanInstance.SDL_SetWindowFullscreen");
			} else {
				std::fprintf(stderr, "[ProjectV][Window] PROJECTV_FULLSCREEN=1 → borderless desktop fullscreen\n");
			}
		}
	}

	const VkResult volkInitializeResult = volkInitialize();
	if (volkInitializeResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanInstance.volkInitialize", volkInitializeResult);
		return false;
	}

	Uint32 extCount = 0;
	const char *const *sdlExtNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
	if (!sdlExtNames) {
		runtime::LogSdlFailure("InitializeVulkanInstance.SDL_Vulkan_GetInstanceExtensions");
		return false;
	}

	std::vector instanceExtensions(sdlExtNames, sdlExtNames + extCount);
#if PROJECTV_ENABLE_VALIDATION || PROJECTV_ENABLE_RENDERDOC_MARKERS
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
#endif

	const bool enableValidation = WantValidationLayers();
	if (enableValidation) {
		if (!CheckValidationLayerSupport()) {
			runtime::LogRuntimeFailure(
				"Init",
				"InitializeVulkanInstance.ValidationLayers",
				"validation layers requested, but not available");
			return false;
		}
	} else if (projectv::core::GetEnvVar("PROJECTV_ENABLE_VALIDATION") != nullptr) {
		SDL_Log("[ProjectV] Vulkan validation layers disabled via PROJECTV_ENABLE_VALIDATION");
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = projectv::render::GetMinVulkanApiVersion();

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidation) {
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = kValidationLayers.data();
		debugCreateInfo = MakeDebugMessengerCreateInfo();
		instanceCreateInfo.pNext = &debugCreateInfo;
	}

	const VkResult createInstanceResult = vkCreateInstance(&instanceCreateInfo, nullptr, &context->instance);
	if (createInstanceResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanInstance.vkCreateInstance", createInstanceResult);
		return false;
	}

	volkLoadInstance(context->instance);

	if (!CreateDebugMessenger(context)) {
		return false;
	}

	if (!SDL_Vulkan_CreateSurface(platform->window, context->instance, nullptr, &context->surface)) {
		runtime::LogSdlFailure("InitializeVulkanInstance.SDL_Vulkan_CreateSurface");
		return false;
	}

	return true;
}
