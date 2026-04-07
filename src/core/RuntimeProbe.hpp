#ifndef RUNTIME_PROBE_HPP
#define RUNTIME_PROBE_HPP

#include <cstdint>
#include <string_view>

enum class InitFailureStage : std::uint8_t {
	None = 0,
	AfterBootstrap,
	AfterWorld,
	AfterSceneResources,
	BeforeGraphicsPipeline,
	BeforeVoxelMeshingPipeline,
};

bool TryParseInitFailureStage(std::string_view value, InitFailureStage *outStage);
const char *InitFailureStageToString(InitFailureStage stage);
InitFailureStage GetRequestedInitFailureStage();
bool IsInitFailureStageRequested(InitFailureStage stage);

#endif
