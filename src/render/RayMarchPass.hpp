#pragma once

namespace projectv::render {

void SetRayMarchEnabled(bool enabled);
bool IsRayMarchEnabled();


void RequestRayMarchPipelineRecreate();


bool IsRayMarchPipelineRecreatePending();

}  // namespace projectv::render

