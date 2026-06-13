#include "render/vulkan/VulkanDebug.hpp"

void SetVulkanObjectName(
	const VulkanContextState &context,
	const uint64_t handle,
	const VkObjectType objectType,
	const char *name)
{
#if !PROJECTV_ENABLE_RENDERDOC_MARKERS
	(void)context;
	(void)handle;
	(void)objectType;
	(void)name;
#else
	if (!context.device || !handle || !name || !vkSetDebugUtilsObjectNameEXT) {
		return;
	}

	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = objectType;
	nameInfo.objectHandle = handle;
	nameInfo.pObjectName = name;
	vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);
#endif
}
