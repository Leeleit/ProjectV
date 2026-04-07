#include "core/RuntimeProbe.hpp"

#include "SDL3/SDL.h"

namespace {
struct InitFailureStageMapping {
	InitFailureStage stage = InitFailureStage::None;
	std::string_view name;
};

constexpr InitFailureStageMapping kInitFailureStageMappings[]{
	{InitFailureStage::AfterBootstrap, "after_bootstrap"},
	{InitFailureStage::AfterWorld, "after_world"},
	{InitFailureStage::AfterSceneResources, "after_scene_resources"},
	{InitFailureStage::BeforeGraphicsPipeline, "before_graphics_pipeline"},
	{InitFailureStage::BeforeVoxelMeshingPipeline, "before_voxel_meshing_pipeline"},
};
} // namespace

bool TryParseInitFailureStage(const std::string_view value, InitFailureStage *outStage)
{
	if (!outStage) {
		return false;
	}

	for (const auto &[mappedStage, name] : kInitFailureStageMappings) {
		if (name == value) {
			*outStage = mappedStage;
			return true;
		}
	}

	*outStage = InitFailureStage::None;
	return false;
}

const char *InitFailureStageToString(const InitFailureStage stage)
{
	for (const auto &[mappedStage, name] : kInitFailureStageMappings) {
		if (mappedStage == stage) {
			return name.data();
		}
	}

	return "none";
}

InitFailureStage GetRequestedInitFailureStage()
{
	const char *requestedStage = SDL_getenv("PROJECTV_FAIL_INIT_STAGE");
	if (!requestedStage || !*requestedStage) {
		return InitFailureStage::None;
	}

	InitFailureStage stage = InitFailureStage::None;
	if (!TryParseInitFailureStage(requestedStage, &stage)) {
		return InitFailureStage::None;
	}

	return stage;
}

bool IsInitFailureStageRequested(const InitFailureStage stage)
{
	return stage != InitFailureStage::None && GetRequestedInitFailureStage() == stage;
}
