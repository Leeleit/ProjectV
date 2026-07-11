import projectv.math;
import projectv.string_id;

#include "voxel/VoxelWorldInternal.hpp"

#include "SDL3/SDL_log.h"
#include "SDL3/SDL_filesystem.h"
#include "core/RuntimeDiagnostics.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>

namespace {
void ClearSnapshotReservedFields(VoxelWorldSnapshotHeader *header)
{
	PV_ASSERT(
		header != nullptr,
		"VoxelWorld",
		"ClearSnapshotReservedFields.Preconditions",
		"snapshot header is null");
	header->reserved = 0;
	for (uint8_t &reservedByte : header->reservedBytes) {
		reservedByte = 0;
	}
}

bool HasClearSnapshotReservedFields(const VoxelWorldSnapshotHeader &header)
{
	if (header.reserved != 0) {
		return false;
	}

	for (const uint8_t reservedByte : header.reservedBytes) {
		if (reservedByte != 0) {
			return false;
		}
	}

	return true;
}

std::filesystem::path ResolveVoxelWorldSnapshotPath(const std::string_view snapshotPath)
{
	return std::filesystem::path(std::string(snapshotPath));
}
} // namespace

std::string GetVoxelWorldSnapshotPath()
{
	const char *requestedSnapshotPath = SDL_getenv("PROJECTV_SNAPSHOT_PATH");
	if (requestedSnapshotPath && *requestedSnapshotPath) {
		return requestedSnapshotPath;
	}

	const char *basePath = SDL_GetBasePath();
	if (basePath && *basePath) {
		const std::filesystem::path snapshotPath =
			std::filesystem::path(basePath) / kDefaultVoxelWorldSnapshotFilename;
		return snapshotPath.string();
	}
	return kDefaultVoxelWorldSnapshotFilename;
}

std::expected<bool, projectv::voxel::VoxelSnapshotError> SaveVoxelWorldSnapshot(const VoxelWorld &world, const std::string_view snapshotPath)
{
	const auto fail = [](projectv::voxel::VoxelSnapshotError e, const std::string_view step, const std::string_view detail) {
		runtime::LogRuntimeFailure("VoxelWorld", step, detail);
		return std::unexpected(e);
	};
	if (snapshotPath.empty()) {
		return fail(projectv::voxel::VoxelSnapshotError::EmptyPath,
					"SaveVoxelWorldSnapshot.Path", "snapshot path is empty");
	}

	const std::filesystem::path resolvedPath = ResolveVoxelWorldSnapshotPath(snapshotPath);
	std::error_code createDirectoriesError;
	const std::filesystem::path parentPath = resolvedPath.parent_path();
	if (!parentPath.empty() &&
		!std::filesystem::create_directories(parentPath, createDirectoriesError) &&
		createDirectoriesError) {
		return fail(projectv::voxel::VoxelSnapshotError::CreateDirectoriesFailed,
					"SaveVoxelWorldSnapshot.CreateDirectories", createDirectoriesError.message());
	}

	VoxelWorldSnapshotHeader header{};
	header.magic = kVoxelWorldSnapshotMagic;
	header.version = kVoxelWorldSnapshotVersion;
	ClearSnapshotReservedFields(&header);
	header.voxelByteCount = 0;
	header.scenePreset = static_cast<uint8_t>(world.scenePreset);
	header.config = world.config;
	header.min = world.min;
	header.maxExclusive = world.maxExclusive;
	header.editVersion = world.editVersion;

	std::ofstream file(resolvedPath, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		return fail(projectv::voxel::VoxelSnapshotError::OpenForWriteFailed,
					"SaveVoxelWorldSnapshot.Open", "failed to open snapshot file for write: " + resolvedPath.string());
	}

	file.write(reinterpret_cast<const char *>(&header), sizeof(header));

	const std::size_t numNodes = world.sparseStorage.NodeCount();
	if (numNodes > std::numeric_limits<uint32_t>::max()) {
		return fail(projectv::voxel::VoxelSnapshotError::VoxelBufferTooLarge,
					"SaveVoxelWorldSnapshot.Size", "sparse node count exceeds snapshot format limit");
	}
	const uint32_t numNodesU32 = static_cast<uint32_t>(numNodes);
	file.write(reinterpret_cast<const char *>(&numNodesU32), sizeof(numNodesU32));
	const uint32_t rootSlotU32 = world.sparseStorage.RootSlot();
	file.write(reinterpret_cast<const char *>(&rootSlotU32), sizeof(rootSlotU32));
	for (std::size_t i = 0; i < numNodes; ++i) {
		const projectv::voxel::Sparse64Tree::Node &node = world.sparseStorage.GetNodes()[i];
		file.write(reinterpret_cast<const char *>(&node), sizeof(node));
	}

	if (!file.good()) {
		return fail(projectv::voxel::VoxelSnapshotError::WriteFailed,
					"SaveVoxelWorldSnapshot.Write", "failed to write snapshot file: " + resolvedPath.string());
	}

	SDL_Log("Saved voxel world snapshot: %s (sparse nodes=%zu)", resolvedPath.string().c_str(), numNodes);
	return true;
}

std::expected<std::unique_ptr<VoxelWorld>, projectv::voxel::VoxelSnapshotError> LoadVoxelWorldSnapshot(const std::string_view snapshotPath)
{
	const auto fail = [](projectv::voxel::VoxelSnapshotError e, const std::string_view step, const std::string_view detail) {
		runtime::LogRuntimeFailure("VoxelWorld", step, detail);
		return std::unexpected(e);
	};
	if (snapshotPath.empty()) {
		return fail(projectv::voxel::VoxelSnapshotError::EmptyPath,
					"LoadVoxelWorldSnapshot.Path", "snapshot path is empty");
	}

	const std::filesystem::path resolvedPath = ResolveVoxelWorldSnapshotPath(snapshotPath);
	std::error_code fileSizeError;
	const std::uintmax_t fileSize = std::filesystem::file_size(resolvedPath, fileSizeError);
	if (fileSizeError) {
		return fail(projectv::voxel::VoxelSnapshotError::FileSizeQueryFailed,
					"LoadVoxelWorldSnapshot.FileSize", fileSizeError.message());
	}
	if (fileSize < sizeof(VoxelWorldSnapshotHeader)) {
		return fail(projectv::voxel::VoxelSnapshotError::FileTooSmall,
					"LoadVoxelWorldSnapshot.FileSize", "snapshot file is smaller than the header");
	}

	std::ifstream file(resolvedPath, std::ios::binary);
	if (!file.is_open()) {
		return fail(projectv::voxel::VoxelSnapshotError::OpenForReadFailed,
					"LoadVoxelWorldSnapshot.Open", "failed to open snapshot file for read: " + resolvedPath.string());
	}

	VoxelWorldSnapshotHeader header{};
	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (!file.good()) {
		return fail(projectv::voxel::VoxelSnapshotError::ReadHeaderFailed,
					"LoadVoxelWorldSnapshot.ReadHeader", "failed to read snapshot header: " + resolvedPath.string());
	}

	if (header.magic != kVoxelWorldSnapshotMagic) {
		return fail(projectv::voxel::VoxelSnapshotError::MagicMismatch,
					"LoadVoxelWorldSnapshot.Header", "snapshot magic mismatch");
	}
	if (header.version != kVoxelWorldSnapshotVersion) {
		return fail(projectv::voxel::VoxelSnapshotError::UnsupportedVersion,
					"LoadVoxelWorldSnapshot.Header", "unsupported snapshot version");
	}
	if (!IsValidVoxelScenePresetValue(header.scenePreset)) {
		return fail(projectv::voxel::VoxelSnapshotError::InvalidScenePreset,
					"LoadVoxelWorldSnapshot.Header", "snapshot scene preset is invalid");
	}
	if (!HasClearSnapshotReservedFields(header)) {
		return fail(projectv::voxel::VoxelSnapshotError::ReservedFieldsNonZero,
					"LoadVoxelWorldSnapshot.Header", "snapshot reserved fields must stay zero");
	}
	if (header.config.chunkSize <= 0) {
		return fail(projectv::voxel::VoxelSnapshotError::InvalidChunkSize,
					"LoadVoxelWorldSnapshot.Header", "snapshot chunk size must stay positive");
	}

	std::unique_ptr<VoxelWorld> world = CreateEmptyVoxelWorld(
		header.config,
		static_cast<VoxelScenePreset>(header.scenePreset),
		header.min,
		header.maxExclusive);
	if (!world) {
		return fail(projectv::voxel::VoxelSnapshotError::CreateWorldFailed,
					"LoadVoxelWorldSnapshot.CreateWorld", "failed to create world layout for snapshot");
	}

	uint32_t numNodesU32 = 0;
	if (!file.read(reinterpret_cast<char *>(&numNodesU32), sizeof(numNodesU32))) {
		return fail(projectv::voxel::VoxelSnapshotError::ReadPayloadFailed,
					"LoadVoxelWorldSnapshot.ReadNodeCount", "failed to read sparse node count");
	}
	uint32_t rootSlotU32 = 0;
	if (!file.read(reinterpret_cast<char *>(&rootSlotU32), sizeof(rootSlotU32))) {
		return fail(projectv::voxel::VoxelSnapshotError::ReadPayloadFailed,
					"LoadVoxelWorldSnapshot.ReadRootSlot", "failed to read sparse root slot");
	}

	std::vector<projectv::voxel::Sparse64Tree::Node> nodes;
	nodes.reserve(numNodesU32);
	for (uint32_t i = 0; i < numNodesU32; ++i) {
		projectv::voxel::Sparse64Tree::Node node{};
		if (!file.read(reinterpret_cast<char *>(&node), sizeof(node))) {
			return fail(projectv::voxel::VoxelSnapshotError::ReadPayloadFailed,
						"LoadVoxelWorldSnapshot.ReadNode", "failed to read sparse node data");
		}
		nodes.push_back(node);
	}

	world->sparseStorage.RestoreFrom(rootSlotU32, std::move(nodes));

	world->editVersion = header.editVersion;

	RebuildVoxelWorldDerivedState(*world);
	MarkAllVoxelChunksDirty(world.get()); // Mark all chunks dirty to force payload and BLAS rebuild
	SDL_Log("Loaded voxel world snapshot: %s", resolvedPath.string().c_str());
	return world;
}

std::vector<uint8_t> BuildFlatVoxelSnapshot(const VoxelWorld &world)
{
	std::vector<uint8_t> flat;
	flat.reserve(static_cast<size_t>(world.width) * world.height * world.depth);
	for (int z = 0; z < world.depth; ++z) {
		for (int y = 0; y < world.height; ++y) {
			for (int x = 0; x < world.width; ++x) {
				flat.push_back(static_cast<uint8_t>(GetVoxelMaterial(world, {world.min.x + x, world.min.y + y, world.min.z + z})));
			}
		}
	}
	return flat;
}
