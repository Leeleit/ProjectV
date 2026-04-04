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

struct VoxelLabConfig {
	int floorSize = 18;
	int sphereRadius = 6;
	Int3 sphereCenter{0, 8, 0};
	int shellThickness = 1;
	float fluidFillLevel = 0.7f;
	int floorY = 0;
	int padding = 3;
};

struct VoxelWorld {
	VoxelLabConfig config{};
	Int3 min{};
	Int3 maxExclusive{};
	int width = 0;
	int height = 0;
	int depth = 0;
	std::vector<uint8_t> voxels;
};

bool CreateVoxelLabWorld(AppState *state);
void DestroyVoxelLabWorld(AppState *state);
bool IsInsideVoxelWorld(const VoxelWorld &world, Int3 position);
VoxelMaterial GetVoxelMaterial(const VoxelWorld &world, Int3 position);

#endif
