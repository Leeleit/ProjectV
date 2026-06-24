#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

bool InitializeVulkanBase(
	PlatformState *platform,
	VulkanContextState *context,
	FrameState *frame);

