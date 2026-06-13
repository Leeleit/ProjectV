#ifndef VOXEL_WORLD_HPP
#define VOXEL_WORLD_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "voxel/VoxelSnapshotError.hpp"

struct AppState;

enum class VoxelMaterial : uint8_t {
	Air = 0,
	Glass = 1,
	Fluid = 2,
	FloorWhite = 3,
	FloorGray = 4,
};
static_assert(sizeof(VoxelMaterial) == sizeof(uint8_t));

enum class VoxelScenePreset : uint8_t {
	VoxelLab = 0,
	FlatBenchmark,
	TransparencyStress,
	ChunkGrid,
	MeshingStress,
};
static_assert(sizeof(VoxelScenePreset) == sizeof(uint8_t));

struct Int3 {
	int x = 0;
	int y = 0;
	int z = 0;
};
static_assert(std::is_standard_layout_v<Int3>);
static_assert(std::is_trivially_copyable_v<Int3>);
static_assert(sizeof(Int3) == 12);

struct VoxelChunk {
	Int3 min{};
	Int3 maxExclusive{};
	bool rebuildQueued = true;
	uint32_t nonAirVoxelCount = 0;
};
static_assert(std::is_standard_layout_v<VoxelChunk>);
static_assert(std::is_trivially_copyable_v<VoxelChunk>);
static_assert(sizeof(VoxelChunk) == 32);
static_assert(offsetof(VoxelChunk, min) == 0);
static_assert(offsetof(VoxelChunk, maxExclusive) == 12);
static_assert(offsetof(VoxelChunk, rebuildQueued) == 24);
static_assert(offsetof(VoxelChunk, nonAirVoxelCount) == 28);

struct VoxelWorldStats {
	uint32_t dirtyChunkCount = 0;
	uint32_t activeChunkCount = 0;
	uint32_t nonAirVoxelCount = 0;
	uint32_t glassVoxelCount = 0;
	uint32_t fluidVoxelCount = 0;
	uint32_t floorWhiteVoxelCount = 0;
	uint32_t floorGrayVoxelCount = 0;
};

struct VoxelWorldConfig {
	int floorSize = 18;
	int floorY = 0;
	int worldTopY = 14;
	int padding = 3;
	int chunkSize = 8;
};

struct VoxelWorld {
	VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
	VoxelWorldConfig config{};
	Int3 min{};
	Int3 maxExclusive{};
	// **Floor bounds (M5.1d, 2026-06-12):** the XZ extent of
	// the visible checkerboard floor (the "platform"), without
	// the world-bound padding that `min` / `maxExclusive`
	// include for chunk allocation. For VoxelLab with
	// `floorSize=18, padding=3`, `min=(-12,0,-12)`,
	// `maxExclusive=(12,17,12)`, `floorMin=(-9,0,-9)`,
	// `floorMaxExclusive=(9,17,9)`. The model's snap should
	// clamp to the floor (the visible platform), not the world
	// (which extends 3 voxels into invisible Air padding
	// around the floor). Y is the same as `maxExclusive.y` —
	// there's no horizontal padding for the height; the only
	// thing that changes is the XZ floor extent.
	Int3 floorMin{};
	Int3 floorMaxExclusive{};
	int width = 0;
	int height = 0;
	int depth = 0;
	std::vector<uint8_t> voxels;
	int chunkSize = 0;
	int chunkCountX = 0;
	int chunkCountY = 0;
	int chunkCountZ = 0;
	uint64_t editVersion = 0;
	std::vector<VoxelChunk> chunks;
	std::vector<size_t> pendingChunkRebuildIndices;
	VoxelWorldStats stats{};
};

// (Tier 1.E: replaced by `ParseVoxelScenePreset(std::string_view)` below
// returning `std::optional<VoxelScenePreset>`. The out-param form is gone.)
VoxelScenePreset GetNextVoxelScenePreset(VoxelScenePreset preset);
VoxelScenePreset GetRequestedVoxelScenePreset();
std::string GetVoxelWorldSnapshotPath();
bool CreateVoxelSceneWorld(AppState *state);
bool CreateVoxelSceneWorld(AppState *state, VoxelScenePreset preset);
void DestroyVoxelSceneWorld(AppState *state);
// **Tier 1.B (`2026-06-13`).** `std::expected<T, VoxelSnapshotError>`
// for the snapshot save / load path. Replaces the old `bool` /
// `nullptr` return + per-step log-line pattern with a strongly-typed
// error enum. See `VoxelSnapshotError.hpp` for the per-variant
// taxonomy. Cold path only (1× per snapshot), so the ~2× cost of
// `std::expected` over a raw `bool` is irrelevant.
std::expected<bool, projectv::voxel::VoxelSnapshotError> SaveVoxelWorldSnapshot(const VoxelWorld &world, std::string_view snapshotPath);
std::expected<std::unique_ptr<VoxelWorld>, projectv::voxel::VoxelSnapshotError> LoadVoxelWorldSnapshot(std::string_view snapshotPath);
// **Tier 1.E (`2026-06-13`).** `VoxelScenePresetToString` returns
// `std::string_view` (was `const char *`) so callers can use the
// result in `string_view` contexts (fmt, fmtlog, unordered_map
// with `string_view` key) without an implicit conversion. Body
// stays in the .cpp — the function is only used at runtime
// (SDL_Log, sidecar metadata, JSON serialization) so
// `constexpr` would not buy anything and would force the body
// into the header.
// `ParseVoxelScenePreset` is `std::optional<VoxelScenePreset>`
// (was `bool TryParseVoxelScenePreset(text, &out)` with out-param).
std::string_view VoxelScenePresetToString(VoxelScenePreset preset);
std::optional<VoxelScenePreset> ParseVoxelScenePreset(std::string_view text);
bool IsInsideVoxelWorld(const VoxelWorld &world, Int3 position);
VoxelMaterial GetVoxelMaterial(const VoxelWorld &world, Int3 position);
Int3 GetVoxelChunkCoord(const VoxelWorld &world, Int3 position);
size_t GetVoxelChunkIndex(const VoxelWorld &world, Int3 chunkCoord);
void SetVoxelMaterial(VoxelWorld &world, Int3 position, VoxelMaterial material);
uint32_t FillVoxelMaterial(VoxelWorld &world, Int3 start, VoxelMaterial material);
uint32_t FillVoxelBox(VoxelWorld &world, Int3 first, Int3 second, VoxelMaterial material);
void MarkVoxelChunkDirty(VoxelWorld &world, Int3 position);
void MarkVoxelRegionDirty(VoxelWorld &world, Int3 min, Int3 maxExclusive);
void MarkAllVoxelChunksDirty(VoxelWorld *world);
void CollectDirtyVoxelChunkRebuildRequests(VoxelWorld &world, std::vector<size_t> *outChunkIndices);
void CommitDirtyVoxelChunkRebuildRequests(VoxelWorld &world, const std::vector<size_t> &rebuiltChunkIndices);
uint32_t CountDirtyVoxelChunks(const VoxelWorld &world);
uint32_t CountActiveVoxelChunks(const VoxelWorld &world);
uint32_t CountVoxelsByMaterial(const VoxelWorld &world, VoxelMaterial material);

// **Fluid cellular automata (defense r0, 2026-06-13).** One CA tick per call:
// each `VoxelMaterial::Fluid` voxel attempts to fall straight down by one
// cell, falling back to the four cardinal neighbours if the cell below is
// already solid. Uses a double-buffer copy of `voxels` so a single tick is
// deterministic and free of read-after-write hazards. Marks every changed
// chunk dirty via `MarkVoxelChunkDirty` so meshing picks the changes up on
// the next frame. Caller is expected to invoke this at a fixed step
// (typically 1/60 s) from the main app loop.
uint32_t UpdateFluidCA(VoxelWorld &world);

#endif
