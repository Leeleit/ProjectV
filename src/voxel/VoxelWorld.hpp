#ifndef VOXEL_WORLD_HPP
#define VOXEL_WORLD_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

struct AppState;

enum class VoxelMaterial : uint8_t {
	Air = 0,
	Glass = 1,
	Fluid = 2,
	FloorWhite = 3,
	FloorGray = 4,
};
static_assert(sizeof(VoxelMaterial) == sizeof(uint8_t));

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

struct VoxelLabConfig {
	int floorSize = 18;
	int sphereRadius = 6;
	Int3 sphereCenter{0, 8, 0};
	int shellThickness = 1;
	float fluidFillLevel = 0.7f;
	int floorY = 0;
	int padding = 3;
	int chunkSize = 8;
};

struct VoxelWorld {
	VoxelLabConfig config{};
	Int3 min{};
	Int3 maxExclusive{};
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

bool CreateVoxelLabWorld(AppState *state);
void DestroyVoxelLabWorld(AppState *state);
bool IsInsideVoxelWorld(const VoxelWorld &world, Int3 position);
VoxelMaterial GetVoxelMaterial(const VoxelWorld &world, Int3 position);
size_t GetVoxelChunkIndex(const VoxelWorld &world, Int3 chunkCoord);
void SetVoxelMaterial(VoxelWorld &world, Int3 position, VoxelMaterial material);
void MarkVoxelChunkDirty(VoxelWorld &world, Int3 position);
void MarkVoxelRegionDirty(VoxelWorld &world, Int3 min, Int3 maxExclusive);
void MarkAllVoxelChunksDirty(VoxelWorld *world);
void CollectDirtyVoxelChunkRebuildRequests(VoxelWorld &world, std::vector<size_t> *outChunkIndices);
void CommitDirtyVoxelChunkRebuildRequests(VoxelWorld &world, const std::vector<size_t> &rebuiltChunkIndices);
uint32_t CountDirtyVoxelChunks(const VoxelWorld &world);
uint32_t CountActiveVoxelChunks(const VoxelWorld &world);
uint32_t CountVoxelsByMaterial(const VoxelWorld &world, VoxelMaterial material);

#endif
