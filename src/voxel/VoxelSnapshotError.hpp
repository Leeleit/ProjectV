#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace projectv::voxel {

enum class VoxelSnapshotError : std::uint8_t {
	// **Save-side errors.**
	EmptyPath = 0,
	CreateDirectoriesFailed,
	VoxelBufferTooLarge,
	OpenForWriteFailed,
	WriteFailed,
	// **Load-side errors.**
	FileSizeQueryFailed,
	FileTooSmall,
	OpenForReadFailed,
	ReadHeaderFailed,
	MagicMismatch,
	UnsupportedVersion,
	InvalidScenePreset,
	ReservedFieldsNonZero,
	InvalidChunkSize,
	CreateWorldFailed,
	VoxelCountMismatch,
	FileSizeMismatch,
	ReadPayloadFailed,
	InvalidVoxelMaterial,
};

// **Human-readable name** for the error variant. Used in log lines
// so the operator can grep for the variant without having to read
// the per-step log strings in `VoxelWorld.cpp`. The detail string
// is the original runtime-failure detail (e.g. errno message,
// file path), so the combination of `toString(variant)` + detail
// is what shows up in `runtime::LogRuntimeFailure`.
constexpr std::string_view toString(VoxelSnapshotError e) noexcept {
	switch (e) {
		case VoxelSnapshotError::EmptyPath: return "EmptyPath";
		case VoxelSnapshotError::CreateDirectoriesFailed: return "CreateDirectoriesFailed";
		case VoxelSnapshotError::VoxelBufferTooLarge: return "VoxelBufferTooLarge";
		case VoxelSnapshotError::OpenForWriteFailed: return "OpenForWriteFailed";
		case VoxelSnapshotError::WriteFailed: return "WriteFailed";
		case VoxelSnapshotError::FileSizeQueryFailed: return "FileSizeQueryFailed";
		case VoxelSnapshotError::FileTooSmall: return "FileTooSmall";
		case VoxelSnapshotError::OpenForReadFailed: return "OpenForReadFailed";
		case VoxelSnapshotError::ReadHeaderFailed: return "ReadHeaderFailed";
		case VoxelSnapshotError::MagicMismatch: return "MagicMismatch";
		case VoxelSnapshotError::UnsupportedVersion: return "UnsupportedVersion";
		case VoxelSnapshotError::InvalidScenePreset: return "InvalidScenePreset";
		case VoxelSnapshotError::ReservedFieldsNonZero: return "ReservedFieldsNonZero";
		case VoxelSnapshotError::InvalidChunkSize: return "InvalidChunkSize";
		case VoxelSnapshotError::CreateWorldFailed: return "CreateWorldFailed";
		case VoxelSnapshotError::VoxelCountMismatch: return "VoxelCountMismatch";
		case VoxelSnapshotError::FileSizeMismatch: return "FileSizeMismatch";
		case VoxelSnapshotError::ReadPayloadFailed: return "ReadPayloadFailed";
		case VoxelSnapshotError::InvalidVoxelMaterial: return "InvalidVoxelMaterial";
	}
	return "Unknown";
}

} // namespace projectv::voxel

