#pragma once

#include "core/Types.hpp"

namespace projectv::render {

void SetRayMarchEnabled(bool enabled);
bool IsRayMarchEnabled();


void RequestRayMarchPipelineRecreate();


bool IsRayMarchPipelineRecreatePending();


void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData);

}  // namespace projectv::render

