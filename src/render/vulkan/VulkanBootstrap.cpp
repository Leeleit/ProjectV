#include "render/vulkan/VulkanBootstrap.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <cstring>

// volk.h must be visible *before* `vk_mem_alloc.h` (transitively pulled
// in by `core/Types.hpp`) so that `VULKAN_API_VERSION_1_4` /
// `VOLK_HEADER_VERSION` are defined when VMA's volk-aware import
// helpers are declared. Without this, `vmaImportVulkanFunctionsFromVolk`
// shows up as undeclared the moment any TU starts a clean build of
// `VulkanBootstrap.cpp.o` (VMA's
// `vmaImportVulkanFunctionsFromVolk` is gated on
// `#ifdef VOLK_HEADER_VERSION`).
#include "volk.h"

#include "SDL3/SDL_vulkan.h"
#include "fmt/format.h"

// `volk.h` does not enable extension-specific feature structs (only the
// function pointers) and `<vulkan/vulkan.h>` requires the extension
// preprocessor `#define` to be set *before* its own include. We need
// `VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR` and
// `VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR`
// unconditionally, even on devices that do not have the extension, so
// that the optional `pNext` chain in `VkDeviceCreateInfo` can compile
// without an `#ifdef`. The feature struct itself is a no-op when the
// extension is not enabled (the loader only acts on `sType`).
// `volk.h` is transitively included via `core/Types.hpp`; we re-include
// it here *after* the `#define` so the extension function pointers stay
// visible.
#define VK_KHR_swapchain_maintenance1 1
#define VK_EXT_dynamic_rendering_unused_attachments 1
#include "volk.h" // IWYU pragma: keep — see the comment above (already pulled in above)
#include <vulkan/vulkan.h>

#include <array>
#include <vector>

namespace {
// Имя приложения видно и в заголовке окна, и в логах Vulkan.
inline constexpr char PROJECT_NAME[] = "ProjectV v0.0.1";

// Базовый набор слоёв для отладки.
constexpr std::array<const char *, 1> kValidationLayers{"VK_LAYER_KHRONOS_validation"};
// Для нашего рендера достаточно swapchain-расширения.
constexpr std::array<const char *, 1> kRequiredDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr char kOptionalTracyCalibratedTimestampsExtension[] = VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME;
// `VK_KHR_swapchain_maintenance1` lets us use a `VkFence` for both the
// `vkAcquireNextImageKHR` "image available" signal and the present wait
// (instead of the in-use-by-swapchain semaphore that was the previous
// 20-per-smoke-run validation warning). Enabled opportunistically when
// supported; the fallback path (no maintenance1) is to use a single
// `imageInFlightFences[imageIndex]` ownership tracker and wait on the
// *previous* in-flight frame's fence after acquire.
// `volk.h` does not define `VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME`
// at the C-preprocessor level, so we use the literal name string.
constexpr char kOptionalSwapchainMaintenance1Extension[] = "VK_KHR_swapchain_maintenance1";
// `VK_EXT_dynamic_rendering_unused_attachments` (extension #500, ratified)
// is the device-level feature that lets one graphics pipeline declared with
// `pColorAttachmentFormats = {swapchain_format, R16G16B16A16_SFLOAT}` be
// used both for the TAA-on path (render to slot 1, slot 0 = NULL) and the
// TAA-off path (render to slot 0 = swapchain, slot 1 = NULL). Per the spec
// (chap54.html §VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT),
// any writes to a `pColorAttachments` slot with `imageView = VK_NULL_HANDLE`
// are discarded — so a single main pipeline serves both paths without
// building a second variant. Core in 1.3/1.4? No, still an extension; see
// `legacy/docs/libraries/vulkan/` for the per-version support matrix. We
// enable it opportunistically when supported; if the extension is missing
// the project still builds and runs the TAA-off path correctly, but the
// `taaEnabled` runtime toggle would fail with a VUID (we guard this in
// `VulkanGraphicsPipeline.cpp::CreateGraphicsPipeline`).
constexpr char kOptionalDynamicRenderingUnusedAttachmentsExtension[] =
	"VK_EXT_dynamic_rendering_unused_attachments";

// Callback, через который Vulkan присылает предупреждения и ошибки прямо в SDL-лог.
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

// Проверяем, доступны ли те слои, которые мы попросим у instance.
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

// Конфигурируем messenger заранее, чтобы Vulkan смог отправлять сообщения уже на этапе vkCreateInstance.
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

// Создаём сам messenger после instance.
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

// Проверяем, что у физического устройства есть нужные расширения.
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

// Ищем очередь, которая умеет и рисовать, и презентовать изображение в окно.
bool HasDeviceExtension(
	const VkPhysicalDevice physicalDevice,
	const char *extensionName)
{
	if (!extensionName) {
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

// Без форматов и present mode swapchain мы просто не соберём.
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

// Проверяем фичи Vulkan 1.3, которые использует рендерер.
bool CheckRequiredFeatures(
	const VkPhysicalDevice physicalDevice,
	VkPhysicalDeviceFeatures *outFeatures,
	VkPhysicalDeviceVulkan12Features *outFeatures12,
	VkPhysicalDeviceVulkan13Features *outFeatures13,
	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR *outSwapchainMaintenance1Features,
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT *outDynamicRenderingUnusedAttachmentsFeatures)
{
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	// The swapchain-maintenance1 features struct is *optionally* queried
	// (only when the extension is enabled on the device, see the caller's
	// pre-check). When the caller's `outSwapchainMaintenance1Features` is
	// non-null, the caller *has already* enabled the extension and we can
	// safely chain the struct; otherwise we omit it (VUID-04025 says we
	// cannot query a features struct for an extension the device does
	// not have).
	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenanceFeatures{};
	if (outSwapchainMaintenance1Features != nullptr) {
		maintenanceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
		features12.pNext = &maintenanceFeatures;
	}
	// The `dynamicRenderingUnusedAttachments` features struct follows the
	// same optional-query pattern as swapchain-maintenance1: the caller
	// passes a non-null out pointer only when the corresponding extension
	// has already been verified on the device.
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentsFeatures{};
	if (outDynamicRenderingUnusedAttachmentsFeatures != nullptr) {
		dynamicRenderingUnusedAttachmentsFeatures.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
		// Both branches below are reachable at runtime. The
		// caller in `SelectPhysicalDevice` passes
		// `deviceHasSwapchainMaintenance1 ? &xxx : nullptr` —
		// that is a runtime decision based on the device's
		// supported extension list, not a compile-time constant.
		// The DFA can't follow the call-site indirection and
		// reports `outSwapchainMaintenance1Features != nullptr`
		// as "always true" plus the `else` branch (line ~341)
		// as unreachable. Suppress per-line.
		// noinspection CppDFAConstantConditions
		// `outSwapchainMaintenance1Features != nullptr` is a runtime
		// decision based on the device's supported extension list
		// (see `VulkanBootstrap::QueryOptionalDeviceFeatures`).
		// The DFA can't follow the call-site indirection and reports
		// it as "always true" plus the `else` branch as unreachable.
		if (outSwapchainMaintenance1Features != nullptr) {
			maintenanceFeatures.pNext = &dynamicRenderingUnusedAttachmentsFeatures;
		} else {
			// noinspection CppDFAUnreachableCode
			features12.pNext = &dynamicRenderingUnusedAttachmentsFeatures;
		}
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
	return true;
}

// Кандидат на выбор физического устройства: сам устройство, его очередь и поддерживаемые фичи.
struct PhysicalDeviceCandidate {
	VkPhysicalDevice device = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = UINT32_MAX;
	VkPhysicalDeviceFeatures features{};
	VkPhysicalDeviceVulkan12Features features12{};
	VkPhysicalDeviceVulkan13Features features13{};
	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR swapchainMaintenance1Features{};
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentsFeatures{};
	bool supportsTracyCalibratedTimestamps = false;
	bool supportsSwapchainMaintenance1 = false;
	bool supportsDynamicRenderingUnusedAttachments = false;
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
	return enabled;
}

// Переносим в enabled только те фичи, которые реально поддержаны устройством.
VkPhysicalDeviceVulkan13Features BuildEnabledFeatures13(const PhysicalDeviceCandidate &selected)
{
	VkPhysicalDeviceVulkan13Features enabled{};
	enabled.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	enabled.dynamicRendering = selected.features13.dynamicRendering ? VK_TRUE : VK_FALSE;
	enabled.synchronization2 = selected.features13.synchronization2 ? VK_TRUE : VK_FALSE;
	return enabled;
}

// Полная проверка кандидата на роль GPU для нашего приложения.
bool TryPickPhysicalDevice(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	PhysicalDeviceCandidate *outCandidate)
{
	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion < VK_API_VERSION_1_4) {
		return false;
	}

	uint32_t queueFamilyIndex = UINT32_MAX;
	if (!FindGraphicsPresentQueueFamily(physicalDevice, surface, &queueFamilyIndex)) {
		return false;
	}

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
	if (!CheckRequiredFeatures(
			physicalDevice,
			&supportedFeatures,
			&supportedFeatures12,
			&supportedFeatures13,
			deviceHasSwapchainMaintenance1 ? &supportedSwapchainMaintenance1Features : nullptr,
			deviceHasDynamicRenderingUnusedAttachments ? &supportedDynamicRenderingUnusedAttachmentsFeatures : nullptr)) {
		return false;
	}

	outCandidate->device = physicalDevice;
	outCandidate->queueFamilyIndex = queueFamilyIndex;
	outCandidate->features = supportedFeatures;
	outCandidate->features12 = supportedFeatures12;
	outCandidate->features13 = supportedFeatures13;
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
	// SDL нужен нам до Vulkan, потому что именно он создаёт окно и surface-совместимость.
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		runtime::LogSdlFailure("InitializeVulkanBase.SDL_Init");
		return false;
	}

	platform->window = SDL_CreateWindow(PROJECT_NAME, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!platform->window) {
		runtime::LogSdlFailure("InitializeVulkanBase.SDL_CreateWindow");
		return false;
	}

	// Volk поднимает таблицу функций Vulkan, чтобы дальше не делать ручной загрузки указателей.
	const VkResult volkInitializeResult = volkInitialize();
	if (volkInitializeResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanBase.volkInitialize", volkInitializeResult);
		return false;
	}

	// SDL сообщает, какие instance extensions нужны именно для этого окна и платформы.
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
	// `VK_KHR_surface_maintenance1` is the instance-level dependency of
	// the (optional) device extension `VK_KHR_swapchain_maintenance1`.
	// It itself depends on `VK_KHR_get_surface_capabilities2` and
	// `VK_KHR_surface`. `VK_KHR_surface` is in `sdlExtNames` already; we
	// add `VK_KHR_get_surface_capabilities2` next. Every NVIDIA / AMD /
	// Intel Vulkan 1.3+ driver in `vulkaninfo` (including the one on
	// this host) reports both extensions.
	// The literals are used because `volk.h` does not define the
	// matching `VK_KHR_*_EXTENSION_NAME` macros.
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
	appInfo.apiVersion = VK_API_VERSION_1_4;

	// Базовая "анкета" для vkCreateInstance.
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

	// Instance — это верхний объект Vulkan, от которого стартует вся остальная графическая жизнь.
	const VkResult createInstanceResult = vkCreateInstance(&instanceCreateInfo, nullptr, &context->instance);
	if (createInstanceResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanBase.vkCreateInstance", createInstanceResult);
		return false;
	}

	volkLoadInstance(context->instance);

	if (!CreateDebugMessenger(context)) {
		return false;
	}

	// Surface связывает окно SDL и Vulkan-instance в один канал вывода.
	if (!SDL_Vulkan_CreateSurface(platform->window, context->instance, nullptr, &context->surface)) {
		runtime::LogSdlFailure("InitializeVulkanBase.SDL_Vulkan_CreateSurface");
		return false;
	}

	// Ищем физическое устройство, которое вообще умеет работать с нашим surface.
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

	// Фиксируем выбранную видеокарту и семейство очереди в общем состоянии.
	context->physicalDevice = selected.device;
	context->queueFamilyIndex = selected.queueFamilyIndex;
	context->supportsDynamicRenderingUnusedAttachments = selected.supportsDynamicRenderingUnusedAttachments;

	// Логическое устройство создаёт то API, которым мы будем реально пользоваться.
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo{};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = context->queueFamilyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

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

	VkPhysicalDeviceFeatures enabledFeatures = BuildEnabledFeatures(selected);
	VkPhysicalDeviceVulkan12Features enabledFeatures12 = BuildEnabledFeatures12(selected);
	VkPhysicalDeviceVulkan13Features enabledFeatures13 = BuildEnabledFeatures13(selected);
	// Chain the swapchain-maintenance1 features struct at the end of the
	// pNext list when the extension is enabled. There is no extra feature
	// bit to enable beyond "the extension exists"; we just need the
	// struct present in the chain so the loader treats it as an
	// enabled-extension request. The `swapchainMaintenance1` field is
	// intentionally left at its default-constructed value (zero, which
	// is also the only valid value for a feature bit that does not
	// exist on this extension).
	VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR enabledSwapchainMaintenance1Features{};
	if (selected.supportsSwapchainMaintenance1) {
		enabledSwapchainMaintenance1Features.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
	}
	// `dynamicRenderingUnusedAttachments` is the *only* opt-in feature bit
	// we actually want from the EXT — when the struct is present in the
	// device pNext chain the loader treats the request as "enable the
	// extension", and the feature bit is the runtime gate that allows
	// `pColorAttachments[i].imageView = VK_NULL_HANDLE` on the unused
	// slot of a `VkPipelineRenderingCreateInfo`-shaped pipeline.
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT enabledDynamicRenderingUnusedAttachmentsFeatures{};
	if (selected.supportsDynamicRenderingUnusedAttachments) {
		enabledDynamicRenderingUnusedAttachmentsFeatures.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
		enabledDynamicRenderingUnusedAttachmentsFeatures.dynamicRenderingUnusedAttachments = VK_TRUE;
	}
	enabledFeatures13.pNext = &enabledFeatures12;
	enabledFeatures12.pNext = selected.supportsSwapchainMaintenance1
								  ? reinterpret_cast<VkBaseOutStructure *>(&enabledSwapchainMaintenance1Features)
								  : nullptr;
	enabledSwapchainMaintenance1Features.pNext = selected.supportsDynamicRenderingUnusedAttachments
													 ? reinterpret_cast<VkBaseOutStructure *>(&enabledDynamicRenderingUnusedAttachmentsFeatures)
													 : nullptr;
	enabledDynamicRenderingUnusedAttachmentsFeatures.pNext = nullptr;
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &enabledFeatures13;
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueInfo;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	const VkResult createDeviceResult =
		vkCreateDevice(context->physicalDevice, &deviceCreateInfo, nullptr, &context->device);
	if (createDeviceResult != VK_SUCCESS) {
		runtime::LogVkFailure("InitializeVulkanBase.vkCreateDevice", createDeviceResult);
		return false;
	}

	// После создания device Vulkan-вызывам нужен device-level loader.
	volkLoadDevice(context->device);
	vkGetDeviceQueue(context->device, context->queueFamilyIndex, 0, &context->queue);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(context->queue),
		VK_OBJECT_TYPE_QUEUE,
		"GraphicsPresentQueue");

	// VMA берет на себя буферы и аллокации памяти, чтобы не писать это руками в каждом месте.
	VmaAllocatorCreateInfo allocInfo{};
	allocInfo.physicalDevice = context->physicalDevice;
	allocInfo.device = context->device;
	allocInfo.instance = context->instance;
	allocInfo.vulkanApiVersion = VK_API_VERSION_1_4;

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

	// Command pool хранит временные command buffer'ы.
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

	// На каждый кадр держим отдельный primary command buffer.
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

	// Семафоры и fence'ы синхронизируют CPU и GPU между кадрами.
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
