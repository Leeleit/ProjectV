#ifndef VOXEL_WORLD_HPP
#define VOXEL_WORLD_HPP

#include <array>
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

struct VoxelChunkMeshVertex {
	std::array<float, 3> position{};
	std::array<float, 3> normal{};
	VoxelMaterial material = VoxelMaterial::Air;
};

struct VoxelChunk {
	Int3 min{};
	Int3 maxExclusive{};
	bool dirty = true;
	std::vector<VoxelChunkMeshVertex> meshVertices;
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
};

bool CreateVoxelLabWorld(AppState *state);
void DestroyVoxelLabWorld(AppState *state);
bool IsInsideVoxelWorld(const VoxelWorld &world, Int3 position);
VoxelMaterial GetVoxelMaterial(const VoxelWorld &world, Int3 position);
size_t GetVoxelChunkIndex(const VoxelWorld &world, Int3 chunkCoord);
void MarkAllVoxelChunksDirty(VoxelWorld *world);

#endif
