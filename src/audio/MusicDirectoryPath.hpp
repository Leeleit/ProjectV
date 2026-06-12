#pragma once

#include <filesystem>
#include <string>

namespace projectv::audio {

// **Music folder resolution, 2026-06-12.** Mirrors the
// existing `GetScreenshotCaptureDirectoryPath` pattern
// (per `src/render/ScreenshotCapture.cpp:72`) and
// `GetVoxelWorldSnapshotPath` (per
// `src/voxel/VoxelWorld.cpp:756`): env-var override →
// `SDL_GetBasePath() / "music"` (next to the executable,
// matches the screenshot/snapshot convention) →
// CWD-relative `./music` (the project root when the
// operator runs from the repo root). The function never
// throws and never fails: the operator gets a valid path
// back even if no part of the chain exists on disk, and
// the engine's `loadMusicFolder` creates the directory on
// demand.
std::filesystem::path GetMusicDirectoryPath();

} // namespace projectv::audio
