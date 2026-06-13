#ifndef VOXEL_SNAPSHOT_ERROR_HPP
#define VOXEL_SNAPSHOT_ERROR_HPP

// **Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
// `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot`. Replaces the
// old `bool` return + log-line-per-failure pattern with a
// `std::expected<T, VoxelSnapshotError>` return that callers can
// `match` / `.and_then` / `.or_else` on.
//
// **Cold path only.** These functions run at save / load time (1×
// per snapshot), not per frame, so the ~2× cost of `std::expected`
// over a raw `bool` is irrelevant. The win is in the API: callers
// see exactly *which* error variant occurred, can chain
// `.transform(...)` to convert the error into a fallback world,
// and don't have to dig through log lines to figure out which
// step failed.
//
// **Per-error log mapping** lives in `VoxelWorld.cpp` itself: the
// implementation maps each variant to the original
// `runtime::LogRuntimeFailure(subsystem, step, detail)` call so the
// diagnostic log output is unchanged. The error variant is the
// "machine-readable" part, the log line is the "human-readable"
// part.
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

#endif // VOXEL_SNAPSHOT_ERROR_HPP
