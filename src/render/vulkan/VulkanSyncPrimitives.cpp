#include "render/vulkan/VulkanSyncPrimitives.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"

namespace projectv::render {

bool IsTimelineSemaphoreSupported()
{
	return VK_HEADER_VERSION >= 200;
}

bool CreateTimelineSemaphore(
	const VkDevice device,
	const uint64_t initialValue,
	VkSemaphore *outSemaphore)
{
	if (device == VK_NULL_HANDLE || outSemaphore == nullptr) {
		return false;
	}
	VkSemaphoreTypeCreateInfo typeInfo{};
	typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	typeInfo.initialValue = initialValue;
	VkSemaphoreCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = &typeInfo;
	const VkResult result = vkCreateSemaphore(device, &info, nullptr, outSemaphore);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("CreateTimelineSemaphore.vkCreateSemaphore", result);
		return false;
	}
	return true;
}

uint64_t ReadTimelineSemaphoreValue(const VkDevice device, const VkSemaphore semaphore)
{
	if (device == VK_NULL_HANDLE || semaphore == VK_NULL_HANDLE) {
		return 0u;
	}
	uint64_t value = 0u;
	const VkResult result = vkGetSemaphoreCounterValue(device, semaphore, &value);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("ReadTimelineSemaphoreValue.vkGetSemaphoreCounterValue", result);
		return 0u;
	}
	return value;
}

bool WaitTimelineSemaphore(
	const VkDevice device,
	const VkSemaphore semaphore,
	const uint64_t waitValue,
	const uint64_t timeoutNs)
{
	if (device == VK_NULL_HANDLE || semaphore == VK_NULL_HANDLE) {
		return false;
	}
	VkSemaphoreWaitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	info.semaphoreCount = 1u;
	info.pSemaphores = &semaphore;
	info.pValues = &waitValue;
	const VkResult result = vkWaitSemaphores(device, &info, timeoutNs);
	if (result != VK_SUCCESS && result != VK_TIMEOUT) {
		runtime::LogVkFailure("WaitTimelineSemaphore.vkWaitSemaphores", result);
		return false;
	}
	return result == VK_SUCCESS;
}

bool SignalTimelineSemaphoreOnQueue(
	const VkQueue queue,
	const VkSemaphore semaphore,
	const uint64_t signalValue)
{
	if (queue == VK_NULL_HANDLE || semaphore == VK_NULL_HANDLE) {
		return false;
	}
	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = semaphore;
	signalSemaphoreInfo.value = signalValue;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 info{};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	info.signalSemaphoreInfoCount = 1u;
	info.pSignalSemaphoreInfos = &signalSemaphoreInfo;
	const VkResult result = vkQueueSubmit2(queue, 1u, &info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("SignalTimelineSemaphoreOnQueue.vkQueueSubmit2", result);
		return false;
	}
	return true;
}

bool DestroyTimelineSemaphore(const VkDevice device, const VkSemaphore semaphore)
{
	if (device == VK_NULL_HANDLE || semaphore == VK_NULL_HANDLE) {
		return false;
	}
	vkDestroySemaphore(device, semaphore, nullptr);
	return true;
}

} // namespace projectv::render