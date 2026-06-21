#include "volk.h"

#include "render/vulkan/VulkanBootstrap.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include "SDL3/SDL_vulkan.h"
#include "fmt/format.h"

#define VK_KHR_swapchain_maintenance1 1
#define VK_EXT_dynamic_rendering_unused_attachments 1
#include <vulkan/vulkan.h>

#include <array>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {
inline constexpr char PROJECT_NAME[] = "ProjectV v0.0.1";

inline constexpr uint32_t kDefaultMinVulkanApiVersion = VK_API_VERSION_1_3;

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

uint32_t GetMinVulkanApiVersion()
{
	const char *overrideValue = std::getenv("PROJECTV_MIN_VULKAN_API_VERSION");
	if (overrideValue == nullptr) {
		return kDefaultMinVulkanApiVersion;
	}
	return ParseVulkanApiVersionString(std::string_view{overrideValue});
}

constexpr std::array<const char *, 1> kValidationLayers{"VK_LAYER_KHRONOS_validation"};
constexpr std::array<const char *, 1> kRequiredDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr char kOptionalTracyCalibratedTimestampsExtension[] = VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME;

constexpr char kOptionalSwapchainMaintenance1Extension[] = "VK_KHR_swapchain_maintenance1";

constexpr char kOptionalDynamicRenderingUnusedAttachmentsExtension[] =
	"VK_EXT_dynamic_rendering_unused_attachments";

constexpr char kMeshShaderExtension[] = "VK_EXT_mesh_shader";

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
#if PROJECTV_ENABLE_VALIDATION
	const VkDebugUtilsMessengerCreateInfoEXT info = MakeDebugMessengerCreateInfo();
	const VkResult createDebugMessengerResult =
		vkCreateDebugUtilsMessengerEXT(context->instance, &info, nullptr, &context->debugMessenger);
	if (createDebugMessengerResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateDebugMessenger.vkCreateDebugUtilsMessengerEXT", createDebugMessengerResult);
		return false;
	}
	return true;
#else
	(void)context;
	return true;
#endif
}

bool CheckDeviceExtensionSupport(const VkPhysicalDevice physicalDevice)
{
	uint32_t extensionCount = 0;
	if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
		return false;
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	if (vkEnumerateDeviceExtensionProperties(
			physicalDevice,
			nullptr,
			&extensionCount,
			availableExtensions.data()) != VK_SUCCESS) {
		return false;
	}

	for (const char *required : kRequiredDeviceExtensions) {
		bool found = false;
		for (const auto &[extensionName, specVersion] : availableExtensions) {
			if (std::strcmp(required, extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			runtime::LogRuntimeFailure(
				"Init",
				"CheckDeviceExtensionSupport",
				fmt::format("missing device extension: {}", required));
			return false;
		}
	}

	return true;
}

bool HasDeviceExtension(
	const VkPhysicalDevice physicalDevice,
	const char *extensionName)
{
	if (!extensionName) [[unlikely]] {
		return false;
	}

	uint32_t extensionCount = 0;
	const VkResult enumerateExtensionCountResult =
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	if (enumerateExtensionCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CheckDeviceExtensionSupport.vkEnumerateDeviceExtensionProperties(count)",
			enumerateExtensionCountResult);
		return false;
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	const VkResult enumerateExtensionDataResult = vkEnumerateDeviceExtensionProperties(
		physicalDevice,
		nullptr,
		&extensionCount,
		availableExtensions.data());
	if (enumerateExtensionDataResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CheckDeviceExtensionSupport.vkEnumerateDeviceExtensionProperties(data)",
			enumerateExtensionDataResult);
		return false;
	}

	for (const auto &[availableExtensionName, specVersion] : availableExtensions) {
		(void)specVersion;
		if (std::strcmp(extensionName, availableExtensionName) == 0) {
			return true;
		}
	}

	return false;
}

bool FindGraphicsPresentQueueFamily(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	uint32_t *outQueueFamilyIndex)
{
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
	if (familyCount == 0) {
		return false;
	}

	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());

	for (uint32_t i = 0; i < familyCount; ++i) {
		VkBool32 presentSupport = VK_FALSE;
		if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport) != VK_SUCCESS) {
			continue;
		}

		if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
			(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
			presentSupport) {
			*outQueueFamilyIndex = i;
			return true;
		}
	}

	return false;
}

bool FindDedicatedComputeQueueFamily(
	const VkPhysicalDevice physicalDevice,
	uint32_t graphicsFamilyIndex,
	uint32_t *outQueueFamilyIndex)
{
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
	if (familyCount == 0) {
		return false;
	}

	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());

	for (uint32_t i = 0; i < familyCount; ++i) {
		if (i == graphicsFamilyIndex) {
			continue;
		}
		const VkQueueFamilyProperties &family = families[i];
		if ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) {
			continue;
		}
		if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
			continue;
		}
		if (family.queueCount == 0) {
			continue;
		}
		*outQueueFamilyIndex = i;
		return true;
	}

	return false;
}

bool CheckSwapchainSurfaceSupport(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface)
{
	uint32_t formatCount = 0;
	const VkResult formatCountResult =
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	if (formatCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CheckSwapchainSurfaceSupport.vkGetPhysicalDeviceSurfaceFormatsKHR(count)",
			formatCountResult);
		return false;
	}
	if (formatCount == 0) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckSwapchainSurfaceSupport.SurfaceFormats",
			"no surface formats found");
		return false;
	}

	uint32_t presentModeCount = 0;
	const VkResult presentModeCountResult =
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	if (presentModeCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CheckSwapchainSurfaceSupport.vkGetPhysicalDeviceSurfacePresentModesKHR(count)",
			presentModeCountResult);
		return false;
	}
	if (presentModeCount == 0) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckSwapchainSurfaceSupport.PresentModes",
			"no present modes found");
		return false;
	}

	return true;
}

bool CheckRequiredFeatures(
	const VkPhysicalDevice physicalDevice,
	VkPhysicalDeviceFeatures *outFeatures,
	VkPhysicalDeviceVulkan12Features *outFeatures12,
	VkPhysicalDeviceVulkan13Features *outFeatures13,
	VkPhysicalDeviceMeshShaderFeaturesEXT *outMeshShaderFeatures,
	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR *outSwapchainMaintenance1Features,
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT *outDynamicRenderingUnusedAttachmentsFeatures)
{
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenanceFeatures{};
	VkBaseOutStructure *lastChainTarget = reinterpret_cast<VkBaseOutStructure *>(&features12);
	if (outSwapchainMaintenance1Features != nullptr) {
		maintenanceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
		features12.pNext = &maintenanceFeatures;
		lastChainTarget = reinterpret_cast<VkBaseOutStructure *>(&maintenanceFeatures);
	}

	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentsFeatures{};
	if (outDynamicRenderingUnusedAttachmentsFeatures != nullptr) {
		dynamicRenderingUnusedAttachmentsFeatures.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
		lastChainTarget->pNext = reinterpret_cast<VkBaseOutStructure *>(&dynamicRenderingUnusedAttachmentsFeatures);
	}

	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
	meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	if (outMeshShaderFeatures != nullptr) {
		lastChainTarget->pNext = reinterpret_cast<VkBaseOutStructure *>(&meshShaderFeatures);
	}
	features13.pNext = &features12;
	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features13;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
	const VkPhysicalDeviceFeatures supportedFeatures = features2.features;

	if (!supportedFeatures.multiDrawIndirect) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.multiDrawIndirect",
			"device does not support multiDrawIndirect");
		return false;
	}

	if (!supportedFeatures.drawIndirectFirstInstance) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.drawIndirectFirstInstance",
			"device does not support drawIndirectFirstInstance");
		return false;
	}

	if (!features12.drawIndirectCount) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.drawIndirectCount",
			"device does not support drawIndirectCount");
		return false;
	}

	if (!features12.hostQueryReset) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.hostQueryReset",
			"device does not support hostQueryReset");
		return false;
	}

	if (!features13.dynamicRendering) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.dynamicRendering",
			"device does not support dynamicRendering");
		return false;
	}

	if (!features13.synchronization2) {
		runtime::LogRuntimeFailure(
			"Init",
			"CheckRequiredFeatures.synchronization2",
			"device does not support synchronization2");
		return false;
	}

	*outFeatures = supportedFeatures;
	*outFeatures12 = features12;
	*outFeatures13 = features13;
	if (outSwapchainMaintenance1Features != nullptr) {
		*outSwapchainMaintenance1Features = maintenanceFeatures;
	}
	if (outDynamicRenderingUnusedAttachmentsFeatures != nullptr) {
		*outDynamicRenderingUnusedAttachmentsFeatures = dynamicRenderingUnusedAttachmentsFeatures;
	}
	if (outMeshShaderFeatures != nullptr) {
		*outMeshShaderFeatures = meshShaderFeatures;
	}
	return true;
}

struct PhysicalDeviceCandidate {
	VkPhysicalDevice device = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = UINT32_MAX;
	uint32_t dedicatedComputeQueueFamilyIndex = UINT32_MAX;
	VkPhysicalDeviceFeatures features{};
	VkPhysicalDeviceVulkan12Features features12{};
	VkPhysicalDeviceVulkan13Features features13{};
	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR swapchainMaintenance1Features{};
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentsFeatures{};
	bool supportsTracyCalibratedTimestamps = false;
	bool supportsSwapchainMaintenance1 = false;
	bool supportsDynamicRenderingUnusedAttachments = false;
	bool supportsDedicatedComputeQueue = false;
	bool supportsMeshShader = false;
};

VkPhysicalDeviceFeatures BuildEnabledFeatures(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceFeatures enabled{};
	enabled.multiDrawIndirect = selected.features.multiDrawIndirect ? VK_TRUE : VK_FALSE;
	enabled.drawIndirectFirstInstance = selected.features.drawIndirectFirstInstance ? VK_TRUE : VK_FALSE;
	enabled.logicOp = selected.features.logicOp ? VK_TRUE : VK_FALSE;
	return enabled;
}

VkPhysicalDeviceVulkan12Features BuildEnabledFeatures12(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceVulkan12Features enabled{};
	enabled.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	enabled.drawIndirectCount = selected.features12.drawIndirectCount ? VK_TRUE : VK_FALSE;
	enabled.hostQueryReset = selected.features12.hostQueryReset ? VK_TRUE : VK_FALSE;
	enabled.timelineSemaphore = selected.features12.timelineSemaphore ? VK_TRUE : VK_FALSE;
	return enabled;
}

VkPhysicalDeviceVulkan13Features BuildEnabledFeatures13(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceVulkan13Features enabled{};
	enabled.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	enabled.dynamicRendering = selected.features13.dynamicRendering ? VK_TRUE : VK_FALSE;
	enabled.synchronization2 = selected.features13.synchronization2 ? VK_TRUE : VK_FALSE;
	enabled.shaderDemoteToHelperInvocation = selected.features13.shaderDemoteToHelperInvocation ? VK_TRUE : VK_FALSE;
	return enabled;
}

bool TryPickPhysicalDevice(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	PhysicalDeviceCandidate *outCandidate)
{
	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion < GetMinVulkanApiVersion()) {
		return false;
	}

	uint32_t queueFamilyIndex = UINT32_MAX;
	if (!FindGraphicsPresentQueueFamily(physicalDevice, surface, &queueFamilyIndex)) {
		return false;
	}

	uint32_t dedicatedComputeQueueFamilyIndex = UINT32_MAX;
	const bool supportsDedicatedComputeQueue =
		FindDedicatedComputeQueueFamily(
			physicalDevice,
			queueFamilyIndex,
			&dedicatedComputeQueueFamilyIndex);

	if (!CheckDeviceExtensionSupport(physicalDevice)) {
		return false;
	}

	if (!CheckSwapchainSurfaceSupport(physicalDevice, surface)) {
		return false;
	}

	VkPhysicalDeviceFeatures supportedFeatures{};
	VkPhysicalDeviceVulkan12Features supportedFeatures12{};
	VkPhysicalDeviceVulkan13Features supportedFeatures13{};
	const bool deviceHasSwapchainMaintenance1 = HasDeviceExtension(physicalDevice, kOptionalSwapchainMaintenance1Extension);
	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR supportedSwapchainMaintenance1Features{};
	const bool deviceHasDynamicRenderingUnusedAttachments =
		HasDeviceExtension(physicalDevice, kOptionalDynamicRenderingUnusedAttachmentsExtension);
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT supportedDynamicRenderingUnusedAttachmentsFeatures{};
	const bool deviceHasMeshShader = HasDeviceExtension(physicalDevice, kMeshShaderExtension);
	VkPhysicalDeviceMeshShaderFeaturesEXT supportedMeshShaderFeatures{};
	if (!CheckRequiredFeatures(
			physicalDevice,
			&supportedFeatures,
			&supportedFeatures12,
			&supportedFeatures13,
			deviceHasMeshShader ? &supportedMeshShaderFeatures : nullptr,
			deviceHasSwapchainMaintenance1 ? &supportedSwapchainMaintenance1Features : nullptr,
			deviceHasDynamicRenderingUnusedAttachments ? &supportedDynamicRenderingUnusedAttachmentsFeatures : nullptr)) {
		return false;
	}

	outCandidate->device = physicalDevice;
	outCandidate->queueFamilyIndex = queueFamilyIndex;
	outCandidate->dedicatedComputeQueueFamilyIndex = dedicatedComputeQueueFamilyIndex;
	outCandidate->supportsDedicatedComputeQueue = supportsDedicatedComputeQueue;
	outCandidate->features = supportedFeatures;
	outCandidate->features12 = supportedFeatures12;
	outCandidate->features13 = supportedFeatures13;
	outCandidate->meshShaderFeatures = supportedMeshShaderFeatures;
	outCandidate->supportsMeshShader = deviceHasMeshShader;
	outCandidate->swapchainMaintenance1Features = supportedSwapchainMaintenance1Features;
	outCandidate->dynamicRenderingUnusedAttachmentsFeatures = supportedDynamicRenderingUnusedAttachmentsFeatures;
	outCandidate->supportsTracyCalibratedTimestamps =
		HasDeviceExtension(physicalDevice, kOptionalTracyCalibratedTimestampsExtension);
	outCandidate->supportsSwapchainMaintenance1 = deviceHasSwapchainMaintenance1;
	outCandidate->supportsDynamicRenderingUnusedAttachments = deviceHasDynamicRenderingUnusedAttachments;
	return true;
}
} // namespace

bool InitializeVulkanBase(
	PlatformState *platform,
	VulkanContextState *context,
	FrameState *frame)
{
	PV_CHECK_OR_RETURN(
		platform && context && frame,
		"Init",
		"InitializeVulkanBase.Preconditions",
		"platform/context/frame is null");
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		runtime::LogSdlFailure("InitializeVulkanBase.SDL_Init");
		return false;
	}

	platform->window = SDL_CreateWindow(PROJECT_NAME, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!platform->window) {
		runtime::LogSdlFailure("InitializeVulkanBase.SDL_CreateWindow");
		return false;
	}

	const VkResult volkInitializeResult = volkInitialize();
	if (volkInitializeResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanBase.volkInitialize", volkInitializeResult);
		return false;
	}

	Uint32 extCount = 0;
	const char *const *sdlExtNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
	if (!sdlExtNames) {
		runtime::LogSdlFailure("InitializeVulkanBase.SDL_Vulkan_GetInstanceExtensions");
		return false;
	}

	std::vector instanceExtensions(sdlExtNames, sdlExtNames + extCount);
#if PROJECTV_ENABLE_VALIDATION || PROJECTV_ENABLE_RENDERDOC_MARKERS
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
#endif

	instanceExtensions.push_back("VK_KHR_get_surface_capabilities2");
	instanceExtensions.push_back("VK_KHR_surface_maintenance1");
#if PROJECTV_ENABLE_VALIDATION
	{
		if (!CheckValidationLayerSupport()) {
			runtime::LogRuntimeFailure(
				"Init",
				"InitializeVulkanBase.ValidationLayers",
				"validation layers requested, but not available");
			return false;
		}
	}
#endif

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = GetMinVulkanApiVersion();

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
#if PROJECTV_ENABLE_VALIDATION
	{
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = kValidationLayers.data();
		debugCreateInfo = MakeDebugMessengerCreateInfo();
		instanceCreateInfo.pNext = &debugCreateInfo;
	}
#endif

	const VkResult createInstanceResult = vkCreateInstance(&instanceCreateInfo, nullptr, &context->instance);
	if (createInstanceResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanBase.vkCreateInstance", createInstanceResult);
		return false;
	}

	volkLoadInstance(context->instance);

	if (!CreateDebugMessenger(context)) {
		return false;
	}

	if (!SDL_Vulkan_CreateSurface(platform->window, context->instance, nullptr, &context->surface)) {
		runtime::LogSdlFailure("InitializeVulkanBase.SDL_Vulkan_CreateSurface");
		return false;
	}

	uint32_t deviceCount = 0;
	const VkResult enumeratePhysicalDevicesCountResult =
		vkEnumeratePhysicalDevices(context->instance, &deviceCount, nullptr);
	if (enumeratePhysicalDevicesCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"InitializeVulkanBase.vkEnumeratePhysicalDevices(count)",
			enumeratePhysicalDevicesCountResult);
		return false;
	}
	if (deviceCount == 0) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitializeVulkanBase.PhysicalDevices",
			"no physical devices found");
		return false;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	const VkResult enumeratePhysicalDevicesDataResult =
		vkEnumeratePhysicalDevices(context->instance, &deviceCount, devices.data());
	if (enumeratePhysicalDevicesDataResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"InitializeVulkanBase.vkEnumeratePhysicalDevices(data)",
			enumeratePhysicalDevicesDataResult);
		return false;
	}

	PhysicalDeviceCandidate selected{};
	for (VkPhysicalDevice physicalDevice : devices) {
		PhysicalDeviceCandidate candidate{};
		if (TryPickPhysicalDevice(physicalDevice, context->surface, &candidate)) {
			selected = candidate;
			break;
		}
	}

	if (selected.device == VK_NULL_HANDLE) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitializeVulkanBase.PhysicalDeviceSelection",
			"no suitable physical device found");
		return false;
	}

	context->physicalDevice = selected.device;
	context->queueFamilyIndex = selected.queueFamilyIndex;
	context->supportsDynamicRenderingUnusedAttachments = selected.supportsDynamicRenderingUnusedAttachments;
	context->hasDedicatedComputeQueue = selected.supportsDedicatedComputeQueue;
	context->dedicatedComputeQueueFamilyIndex = selected.dedicatedComputeQueueFamilyIndex;

	// EVIL: queuePriority = 1.0f is Vulkan's max priority; we request max priority for both graphics+compute
	// queues to keep scheduling decisions predictable. Lower values would be ignored on hosts that round up.
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo{};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = context->queueFamilyIndex;
	// EVIL: queueCount = 1 single-queue per family is intentional; multi-queue families invite priority inversion
	// and we don't currently exploit parallel queue submits. Re-evaluate when adding async compute dispatch.
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	queueCreateInfos.push_back(queueInfo);

	if (selected.supportsDedicatedComputeQueue) {
		context->dedicatedComputeQueueFamilyIndex = selected.dedicatedComputeQueueFamilyIndex;
		VkDeviceQueueCreateInfo computeQueueInfo{};
		computeQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		computeQueueInfo.queueFamilyIndex = selected.dedicatedComputeQueueFamilyIndex;
		// EVIL: computeQueueInfo.queueCount = 1 mirrors the graphics queue policy; future async-compute adoption
		// may need 2+ compute queues to overlap Fluid CA + HZB cull submissions.
		computeQueueInfo.queueCount = 1;
		computeQueueInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(computeQueueInfo);
	} else {
		// EVIL: UINT32_MAX sentinel for "no dedicated compute queue" is implicit; reading code must know the
		// convention. A std::optional<uint32_t> would be cleaner but introduces a transitive include cycle.
		context->dedicatedComputeQueueFamilyIndex = UINT32_MAX;
	}

	std::vector deviceExtensions(
		kRequiredDeviceExtensions.begin(),
		kRequiredDeviceExtensions.end());
	if (selected.supportsTracyCalibratedTimestamps) {
		deviceExtensions.push_back(kOptionalTracyCalibratedTimestampsExtension);
	}
	if (selected.supportsSwapchainMaintenance1) {
		deviceExtensions.push_back(kOptionalSwapchainMaintenance1Extension);
	}
	if (selected.supportsDynamicRenderingUnusedAttachments) {
		deviceExtensions.push_back(kOptionalDynamicRenderingUnusedAttachmentsExtension);
	}
	const bool meshShaderRequested = std::getenv("PROJECTV_MESH_SHADER_PIPELINE") != nullptr;
	if (meshShaderRequested && selected.supportsMeshShader) {
		deviceExtensions.push_back(kMeshShaderExtension);
	}

	VkPhysicalDeviceFeatures enabledFeatures = BuildEnabledFeatures(selected);
	VkPhysicalDeviceVulkan12Features enabledFeatures12 = BuildEnabledFeatures12(selected);
	VkPhysicalDeviceVulkan13Features enabledFeatures13 = BuildEnabledFeatures13(selected);

	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR enabledSwapchainMaintenance1Features{};
	if (selected.supportsSwapchainMaintenance1) {
		enabledSwapchainMaintenance1Features.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
	}

	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT enabledDynamicRenderingUnusedAttachmentsFeatures{};
	if (selected.supportsDynamicRenderingUnusedAttachments) {
		enabledDynamicRenderingUnusedAttachmentsFeatures.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
		enabledDynamicRenderingUnusedAttachmentsFeatures.dynamicRenderingUnusedAttachments = VK_TRUE;
	}

	VkPhysicalDeviceMeshShaderFeaturesEXT enabledMeshShaderFeatures{};
	const bool meshShaderEnabled = meshShaderRequested && selected.supportsMeshShader;
	if (meshShaderEnabled) {
		enabledMeshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
		enabledMeshShaderFeatures.meshShader = VK_TRUE;
		enabledMeshShaderFeatures.taskShader = VK_TRUE;
	}

	enabledFeatures13.pNext = &enabledFeatures12;
	enabledFeatures12.pNext = selected.supportsSwapchainMaintenance1
								  ? reinterpret_cast<VkBaseOutStructure *>(&enabledSwapchainMaintenance1Features)
								  : nullptr;
	enabledSwapchainMaintenance1Features.pNext = selected.supportsDynamicRenderingUnusedAttachments
													 ? reinterpret_cast<VkBaseOutStructure *>(&enabledDynamicRenderingUnusedAttachmentsFeatures)
													 : nullptr;
	enabledDynamicRenderingUnusedAttachmentsFeatures.pNext = meshShaderEnabled
														   ? reinterpret_cast<VkBaseOutStructure *>(&enabledMeshShaderFeatures)
														   : nullptr;
	enabledMeshShaderFeatures.pNext = nullptr;
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &enabledFeatures13;
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	const VkResult createDeviceResult =
		vkCreateDevice(context->physicalDevice, &deviceCreateInfo, nullptr, &context->device);
	if (createDeviceResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanBase.vkCreateDevice", createDeviceResult);
		return false;
	}

	volkLoadDevice(context->device);
	vkGetDeviceQueue(context->device, context->queueFamilyIndex, 0, &context->queue);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->queue),
		VK_OBJECT_TYPE_QUEUE,
		"GraphicsPresentQueue");

	if (context->hasDedicatedComputeQueue) {
		vkGetDeviceQueue(
			context->device,
			context->dedicatedComputeQueueFamilyIndex,
			0,
			&context->dedicatedComputeQueue);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(context->dedicatedComputeQueue),
			VK_OBJECT_TYPE_QUEUE,
			"DedicatedComputeQueue");
	}

	VkSemaphoreTypeCreateInfo timelineTypeInfo{};
	timelineTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	timelineTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timelineTypeInfo.initialValue = 0;
	VkSemaphoreCreateInfo timelineInfo{};
	timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	timelineInfo.pNext = &timelineTypeInfo;
	const VkResult timelineResult = vkCreateSemaphore(
		context->device,
		&timelineInfo,
		nullptr,
		&context->renderTimelineSemaphore);
	if (timelineResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"InitializeVulkanBase.vkCreateSemaphore",
			timelineResult);
		return false;
	}
	context->renderTimelineValue = 0;
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->renderTimelineSemaphore),
		VK_OBJECT_TYPE_SEMAPHORE,
		"RenderTimelineSemaphore");

	VkSemaphoreTypeCreateInfo hzbTimelineTypeInfo{};
	hzbTimelineTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	hzbTimelineTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	hzbTimelineTypeInfo.initialValue = 0;
	VkSemaphoreCreateInfo hzbTimelineInfo{};
	hzbTimelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	hzbTimelineInfo.pNext = &hzbTimelineTypeInfo;
	const VkResult hzbTimelineResult = vkCreateSemaphore(
		context->device,
		&hzbTimelineInfo,
		nullptr,
		&context->hzbBuildTimelineSemaphore);
	if (hzbTimelineResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"InitializeVulkanBase.vkCreateSemaphore.HzbBuild",
			hzbTimelineResult);
		return false;
	}
	context->hzbBuildLastTimelineValue = 0;
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->hzbBuildTimelineSemaphore),
		VK_OBJECT_TYPE_SEMAPHORE,
		"HzbBuildTimelineSemaphore");

	VmaAllocatorCreateInfo allocInfo{};
	allocInfo.physicalDevice = context->physicalDevice;
	allocInfo.device = context->device;
	allocInfo.instance = context->instance;
	allocInfo.vulkanApiVersion = GetMinVulkanApiVersion();

	VmaVulkanFunctions vulkanFunctions{};
	const VkResult importFunctionsResult = vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
	if (importFunctionsResult != VK_SUCCESS) {
		runtime::LogVmaFailure(
			"InitializeVulkanBase.vmaImportVulkanFunctionsFromVolk",
			importFunctionsResult);
		return false;
	}
	allocInfo.pVulkanFunctions = &vulkanFunctions;

	const VkResult createAllocatorResult = vmaCreateAllocator(&allocInfo, &context->allocator);
	if (createAllocatorResult != VK_SUCCESS) {
		runtime::LogVmaFailure("InitializeVulkanBase.vmaCreateAllocator", createAllocatorResult);
		return false;
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = context->queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	const VkResult createCommandPoolResult =
		vkCreateCommandPool(context->device, &poolInfo, nullptr, &context->commandPool);
	if (createCommandPoolResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanBase.vkCreateCommandPool", createCommandPoolResult);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->commandPool),
		VK_OBJECT_TYPE_COMMAND_POOL,
		"MainCommandPool");

	frame->commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo cmdAllocInfo{};
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = context->commandPool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	const VkResult allocateCommandBuffersResult =
		vkAllocateCommandBuffers(context->device, &cmdAllocInfo, frame->commandBuffers.data());
	if (allocateCommandBuffersResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanBase.vkAllocateCommandBuffers", allocateCommandBuffersResult);
		return false;
	}
	for (size_t i = 0; i < frame->commandBuffers.size(); ++i) {
		char name[64]{};
		std::snprintf(name, sizeof(name), "FrameCommandBuffer[%zu]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->commandBuffers[i]),
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			name);
	}

	frame->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frame->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frame->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		const VkResult imageAvailableSemaphoreResult =
			vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &frame->imageAvailableSemaphores[i]);
		if (imageAvailableSemaphoreResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"InitializeVulkanBase.vkCreateSemaphore(imageAvailable)",
				imageAvailableSemaphoreResult);
			return false;
		}
		const VkResult renderFinishedSemaphoreResult =
			vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &frame->renderFinishedSemaphores[i]);
		if (renderFinishedSemaphoreResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"InitializeVulkanBase.vkCreateSemaphore(renderFinished)",
				renderFinishedSemaphoreResult);
			return false;
		}
		const VkResult inFlightFenceResult =
			vkCreateFence(context->device, &fenceInfo, nullptr, &frame->inFlightFences[i]);
		if (inFlightFenceResult != VK_SUCCESS) {
			runtime::LogVkFailure("InitializeVulkanBase.vkCreateFence", inFlightFenceResult);
			return false;
		}

		char imageAvailableName[64]{};
		std::snprintf(imageAvailableName, sizeof(imageAvailableName), "ImageAvailableSemaphore[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->imageAvailableSemaphores[i]),
			VK_OBJECT_TYPE_SEMAPHORE,
			imageAvailableName);

		char renderFinishedName[64]{};
		std::snprintf(renderFinishedName, sizeof(renderFinishedName), "RenderFinishedSemaphore[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->renderFinishedSemaphores[i]),
			VK_OBJECT_TYPE_SEMAPHORE,
			renderFinishedName);

		char fenceName[64]{};
		std::snprintf(fenceName, sizeof(fenceName), "InFlightFence[%d]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frame->inFlightFences[i]),
			VK_OBJECT_TYPE_FENCE,
			fenceName);
	}

	return true;
}
