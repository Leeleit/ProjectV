#pragma once

#include <cstddef> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "voxel/Sparse64Tree.hpp"
#include "voxel/VoxelSnapshotError.hpp"

struct PhysicsState;

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
	bool isStatic = false;
	uint32_t nonAirVoxelCount = 0;
	uint32_t ticksSinceLastEdit = 0;
	uint8_t lodLevel = 0;
	uint8_t lodDownsampledNonAirCount = 0;
	uint8_t reserved1 = 0;
	uint8_t reserved2 = 0;
};
static_assert(std::is_standard_layout_v<VoxelChunk>);
static_assert(std::is_trivially_copyable_v<VoxelChunk>);
static_assert(sizeof(VoxelChunk) == 40);
static_assert(offsetof(VoxelChunk, min) == 0);
static_assert(offsetof(VoxelChunk, maxExclusive) == 12);
static_assert(offsetof(VoxelChunk, rebuildQueued) == 24);
static_assert(offsetof(VoxelChunk, isStatic) == 25);
static_assert(offsetof(VoxelChunk, nonAirVoxelCount) == 28);
static_assert(offsetof(VoxelChunk, ticksSinceLastEdit) == 32);
static_assert(offsetof(VoxelChunk, lodLevel) == 36);
static_assert(offsetof(VoxelChunk, lodDownsampledNonAirCount) == 37);

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
	Int3 floorMin{};
	Int3 floorMaxExclusive{};
	int width = 0;
	int height = 0;
	int depth = 0;
	projectv::voxel::Sparse64Tree sparseStorage;
	int chunkSize = 0;
	int chunkCountX = 0;
	int chunkCountY = 0;
	int chunkCountZ = 0;
	uint64_t editVersion = 0;
	std::vector<VoxelChunk> chunks;
	std::vector<size_t> pendingChunkRebuildIndices;
	std::vector<size_t> pendingBlasRebuildIndices;
	VoxelWorldStats stats{};
};

VoxelScenePreset GetNextVoxelScenePreset(VoxelScenePreset preset);
VoxelScenePreset GetRequestedVoxelScenePreset();
std::string GetVoxelWorldSnapshotPath();
bool CreateVoxelSceneWorld(AppState *state);
bool CreateVoxelSceneWorld(AppState *state, VoxelScenePreset preset);
void DestroyVoxelSceneWorld(AppState *state);
std::expected<bool, projectv::voxel::VoxelSnapshotError> SaveVoxelWorldSnapshot(const VoxelWorld &world, std::string_view snapshotPath);
std::expected<std::unique_ptr<VoxelWorld>, projectv::voxel::VoxelSnapshotError> LoadVoxelWorldSnapshot(std::string_view snapshotPath);
std::string_view VoxelScenePresetToString(VoxelScenePreset preset);
std::optional<VoxelScenePreset> ParseVoxelScenePreset(std::string_view text);
bool IsInsideVoxelWorld(const VoxelWorld &world, Int3 position);
VoxelMaterial GetVoxelMaterial(const VoxelWorld &world, Int3 position);
Int3 GetVoxelChunkCoord(const VoxelWorld &world, Int3 position);
size_t GetVoxelChunkIndex(const VoxelWorld &world, Int3 chunkCoord);
void SetVoxelMaterial(VoxelWorld &world, Int3 position, VoxelMaterial material, PhysicsState *physics = nullptr);

uint32_t GetVoxelChunkStaticPromotionThreshold();
void TickVoxelChunkStaticPromotion(VoxelWorld &world, uint32_t threshold);
uint32_t CountStaticVoxelChunks(const VoxelWorld &world);

uint8_t SelectLodLevelForDistance(float distanceMeters);
void AssignLodLevels(VoxelWorld &world, float cameraX, float cameraY, float cameraZ);
uint32_t CountChunksAtLod(const VoxelWorld &world, uint8_t lodLevel);
uint32_t LodDownsampleStepForLod(uint8_t lodLevel);
uint32_t LodDownsampledExtentForLod(uint8_t lodLevel, uint8_t chunkSize);
void DownsampleChunkForLodSurfacePreserve(
	const VoxelWorld &world,
	size_t chunkIndex,
	uint8_t lodLevel,
	std::vector<uint8_t> &outDownsampled);
uint32_t RunLodDownsampleJobs(VoxelWorld &world);
bool IsLodDownsampleEnabled();
uint32_t FillVoxelMaterial(VoxelWorld &world, Int3 start, VoxelMaterial material);
uint32_t FillVoxelBox(VoxelWorld &world, Int3 first, Int3 second, VoxelMaterial material);
void MarkVoxelChunkDirty(VoxelWorld &world, Int3 position);
void MarkVoxelRegionDirty(VoxelWorld &world, Int3 min, Int3 maxExclusive);
void MarkAllVoxelChunksDirty(VoxelWorld *world);
void CollectDirtyVoxelChunkRebuildRequests(VoxelWorld &world, std::vector<size_t> *outChunkIndices);
void CollectDirtyVoxelChunkBlasRebuildRequests(VoxelWorld &world, std::vector<uint32_t> *outChunkIndices);
void CommitDirtyVoxelChunkRebuildRequests(VoxelWorld &world, const std::vector<size_t> &rebuiltChunkIndices);
uint32_t CountDirtyVoxelChunks(const VoxelWorld &world);
uint32_t CountActiveVoxelChunks(const VoxelWorld &world);
uint32_t CountVoxelsByMaterial(const VoxelWorld &world, VoxelMaterial material);
std::vector<uint8_t> BuildFlatVoxelSnapshot(const VoxelWorld &world);
