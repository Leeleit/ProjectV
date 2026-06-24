#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

void ApplyStartupCameraOverrideFromEnvironment(CameraState *camera);
void ConfigureLookDevCaptureAutomationFromEnvironment(LookDevCaptureAutomationState *automation);
[[nodiscard]] bool UpdateLookDevCaptureAutomation(
	LookDevCaptureAutomationState *automation,
	RenderState *render);

