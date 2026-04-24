#ifndef LOOK_DEV_CAPTURE_AUTOMATION_HPP
#define LOOK_DEV_CAPTURE_AUTOMATION_HPP

#include "core/Types.hpp"

void ApplyStartupCameraOverrideFromEnvironment(CameraState *camera);
void ConfigureLookDevCaptureAutomationFromEnvironment(LookDevCaptureAutomationState *automation);
bool UpdateLookDevCaptureAutomation(
	LookDevCaptureAutomationState *automation,
	RenderState *render);

#endif
