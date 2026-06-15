#pragma once

#include "core/Types.hpp"

void ApplyStartupCameraOverrideFromEnvironment(CameraState *camera);
void ConfigureLookDevCaptureAutomationFromEnvironment(LookDevCaptureAutomationState *automation);
bool UpdateLookDevCaptureAutomation(
	LookDevCaptureAutomationState *automation,
	RenderState *render);

