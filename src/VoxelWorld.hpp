#ifndef VOXEL_WORLD_HPP
#define VOXEL_WORLD_HPP

#include <cstdint>
#include <vector>

struct AppState;

enum class VoxelMaterial : uint8_t {
	Air = 0,
	Glass = 1,
	Fluid = 2,
	FloorWhite = 3,
	FloorGray = 4,
};

struct Int3 {
	int x = 0;
	int y = 0;
	int z = 0;
};

struct VoxelChunk {
	Int3 min{};
	Int3 maxExclusive{};
	bool dirty = true;
	uint32_t nonAirVoxelCount = 0;
};

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
	std::vector<VoxelChunk> chunks;
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
uint32_t CountDirtyVoxelChunks(const VoxelWorld &world);
uint32_t CountActiveVoxelChunks(const VoxelWorld &world);
uint32_t CountVoxelsByMaterial(const VoxelWorld &world, VoxelMaterial material);

#endif
