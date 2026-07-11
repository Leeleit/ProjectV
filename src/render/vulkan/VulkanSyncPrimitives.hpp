#pragma once

#include <cstdint> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <vulkan/vulkan.h>

namespace projectv::render {

bool IsTimelineSemaphoreSupported();

bool CreateTimelineSemaphore(
	VkDevice device,
	uint64_t initialValue,
	VkSemaphore *outSemaphore);

uint64_t ReadTimelineSemaphoreValue(
	VkDevice device,
	VkSemaphore semaphore);

bool WaitTimelineSemaphore(
	VkDevice device,
	VkSemaphore semaphore,
	uint64_t waitValue,
	uint64_t timeoutNs);

bool SignalTimelineSemaphoreOnQueue(
	VkQueue queue,
	VkSemaphore semaphore,
	uint64_t signalValue);

bool DestroyTimelineSemaphore(VkDevice device, VkSemaphore semaphore);

} // namespace projectv::render