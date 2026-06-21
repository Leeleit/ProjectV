#pragma once

#include "core/Types.hpp"

void ApplyStartupCameraOverrideFromEnvironment(CameraState *camera);
void ConfigureLookDevCaptureAutomationFromEnvironment(LookDevCaptureAutomationState *automation);
[[nodiscard]] bool UpdateLookDevCaptureAutomation(
	LookDevCaptureAutomationState *automation,
	RenderState *render);

