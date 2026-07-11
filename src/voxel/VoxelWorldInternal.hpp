#pragma once

#include "voxel/VoxelWorld.hpp"
#include <array>
#include <memory>
#include <type_traits>

struct VoxelLabShellConfig {
	int radius = 6;
	Int3 center{0, 8, 0};
	int shellThickness = 1;
	float fluidFillLevel = 0.7f;
};

struct VoxelWorldSnapshotHeader {
	std::array<char, 8> magic{};
	uint32_t version = 0;
	uint32_t voxelByteCount = 0;
	uint32_t reserved = 0;
	uint8_t scenePreset = 0;
	uint8_t reservedBytes[3]{};
	VoxelWorldConfig config{};
	Int3 min{};
	Int3 maxExclusive{};
	uint64_t editVersion = 0;
};
static_assert(std::is_standard_layout_v<VoxelWorldSnapshotHeader>);
static_assert(std::is_trivially_copyable_v<VoxelWorldSnapshotHeader>);
static_assert(sizeof(VoxelWorldSnapshotHeader) == 80);

constexpr VoxelScenePreset kDefaultVoxelScenePreset = VoxelScenePreset::VoxelLab;
constexpr char kDefaultVoxelWorldSnapshotFilename[] = "ProjectV.snapshot.bin";
constexpr std::array kVoxelWorldSnapshotMagic{'P', 'V', 'S', 'N', 'A', 'P', '0', '1'};
constexpr uint32_t kVoxelWorldSnapshotVersion = 2u;

void AccumulateMaterialCount(VoxelWorldStats &stats, VoxelMaterial material, int delta);
uint8_t ReadVoxelFromSparseStorage(const VoxelWorld &world, Int3 position);
bool IsValidVoxelScenePresetValue(uint8_t presetValue);
void RebuildVoxelWorldDerivedState(VoxelWorld &world);

std::unique_ptr<VoxelWorld> CreateEmptyVoxelWorld(
	const VoxelWorldConfig &config,
	VoxelScenePreset scenePreset,
	Int3 min,
	Int3 maxExclusive);

std::unique_ptr<VoxelWorld> CreateEmptyVoxelWorld(
	const VoxelWorldConfig &config,
	VoxelScenePreset scenePreset);
