module;

#include <cstdint>

export module projectv.types;

export import projectv.math;
export import projectv.string_id;

export namespace projectv::core {

struct AppState;
struct EcsState;
struct CameraState;
struct DebugState;
struct WorldState;
struct VoxelWorldStats;

struct RenderPassTiming {
	std::uint64_t labelId{};
	std::uint64_t startNs{};
	std::uint64_t endNs{};
};

} // namespace projectv::core