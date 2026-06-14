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

// **Fluid cellular automata (defense r0, 2026-06-13; audited 2026-06-13;
// spread restored 2026-06-13).**
// One CA tick per call: every `VoxelMaterial::Fluid` voxel **first**
// attempts to fall straight down by one cell (`f_fall` rule). If the
// fall is blocked (cell below is `Glass`, `FloorWhite`, `FloorGray`,
// or another `Fluid`), the fluid instead **spreads horizontally** to
// one of the four cardinal neighbours (`f_spread` rule, radius 1, no
// support check). The direction is hash-determined from `(x, y, z)` so
// the spread pattern is reproducible. The 2026-06-13 audit removed the
// "concave ground" support check because the operator's follow-up
// "сделать, чтобы она растекалась по горизонтали ещё" required
// unrestricted spread; the original "respawn off platform" perception
// was a **symptom** of the commit-loop coordinate bug
// (`decisions.md §30` "CRITICAL"), not a feature of the spread rule
// itself. See `agent/decisions.md §30` for the full audit + rule set,
// and `agent/memory.md §12` for the bug history.
//
// **Determinism contract (verified by `TestFluidCA*Deterministic`):**
//   * Single-threaded; no atomics, no shared state.
//   * No floating-point arithmetic; all math is `int` / `uint8_t` /
//     `uint32_t` (the spread-hash constants were removed with the spread
//     rule, so the only integer arithmetic left is `idx` math).
//   * Iteration order is **fixed** at `z, y, x` ascending in both the
//     CA loop and the commit loop. Combined with the bottom-up `y` pass
//     this guarantees: a column of fluid falls **one cell per tick**,
//     with no double-step (a fluid at `(x, 5, z)` reads `world.voxels`
//     (the immutable snapshot) at `(x, 4, z)` and never sees the
//     `(x, 4, z)` fluid that was already written to `next` by the
//     `(x, 3, z)` step in the same tick).
//   * No system calls (`rand`, `time`, `/dev/urandom`); no allocator
//     dependence on pointer identity (only on contents).
//   * `stats.fluidVoxelCount` is verified equal to
//     `std::count(voxels, == Fluid)` after every commit (debug build).
//
// **Coordinate convention (critical, do not regress):** the CA pass and
// the commit loop iterate **local** indices `x ∈ [0, width)`, etc.
// The commit loop adds `world.min` to convert local → world before
// calling `SetVoxelMaterial` (which expects world coordinates). A
// pre-2026-06-13 bug had the commit loop passing local indices directly
// as world coords, which silently dropped fall commits at the world
// edge (`local.x == width - world.min.x` mapped to `world.x ==
// maxExclusive.x`, rejected by `IsInsideVoxelWorld`) and corrupted
// the rest. The VoxelLab scene's `min = (-12, 0, -12)` was hit hardest:
// the user reported "water doesn't fall", which the
// `TestFluidCAVoxelLabSphereFallOnGlassBreak` test pins.
//
// **Pre-conditions (asserted in debug builds):**
//   * `world.voxels.size() == width * height * depth` (post-condition
//     of `CreateEmptyVoxelWorld`, asserted here as a defence in depth).
//   * `width, height, depth > 0`.
//
// **Caller contract:** invoke at a fixed step (typically 1/60 s) from
// the main app loop. The throttle lives at the call site, not in the
// function, so tests can drive any tick cadence. Zero-fluid worlds
// short-circuit on the `stats.fluidVoxelCount == 0u` check.
uint32_t UpdateFluidCA(VoxelWorld &world);

#endif
