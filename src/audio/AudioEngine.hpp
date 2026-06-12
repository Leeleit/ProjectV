#pragma once

// **Audio engine, 2026-06-12.** Thin wrapper over
// miniaudio (https://github.com/mackron/miniaudio) for
// ProjectV's music-player slice. miniaudio is already
// vendored as a submodule at `external/miniaudio/` and is
// wired into `src/CMakeLists.txt` in the same session that
// introduced this file. The engine uses miniaudio's
// PulseAudio backend on Linux, which routes through the
// `pipewire-pulse` shim to the active PipeWire server on
// this host (per `pactl info` → `Server String:
// /run/user/1000/pulse/native`). The playback format is
// hard-coded to 16-bit signed PCM at 44.1 kHz stereo per
// the v1 spec (the user said "16/44100"). See
// `decisions.md §28` for the full per-field contract.

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <miniaudio.h>

namespace projectv::audio {

// **Playback state, 2026-06-12.** Three-valued enum so
// the HUD line and sidecar can show the current state
// without poking into `ma_sound_is_playing` from the
// stats mirror (which is a 1-frame-stale read at best).
// `Stopped` means no `ma_sound` is loaded OR the cursor
// has been reset to 0 (next play starts from beginning).
// `Paused` means the cursor is preserved (next play
// resumes from there). `Playing` is the obvious
// third state. The transition rules are enforced by
// `AudioEngine::togglePlayPause` / `AudioEngine::stop`.
enum class MusicState : uint8_t {
	Stopped = 0,
	Playing,
	Paused,
};

const char *MusicStateToString(MusicState state);

// **Audio engine class.** Holds one `ma_engine`, one
// `ma_sound_group` (for music volume bus-level control),
// and one `ma_sound` (the currently-loaded track). The
// engine is a single instance on `AppState` (mirrors
// `legacy/docs/architecture/practice/02_engine_bootstrap_spec.md:533`)
// — it is not an ECS system in v1, just a plain
// singleton, because the music hotkeys fire from
// `UpdateApp` and don't need per-entity iteration.
class AudioEngine {
public:
	AudioEngine() = default;
	~AudioEngine();

	AudioEngine(const AudioEngine &) = delete;
	AudioEngine &operator=(const AudioEngine &) = delete;

	// Initialize the engine. Format = 16-bit signed PCM,
	// 44.1 kHz, stereo. Non-fatal on failure (logs and
	// stays uninited; the rest of the program keeps
	// running). Calling `init` after a successful `init`
	// is a no-op.
	bool init();

	// Symmetric teardown. Safe to call multiple times.
	void shutdown();

	// Set the music folder and trigger the first
	// playlist scan. Returns the number of `.mp3` files
	// found (0 is a valid result; the engine just stays
	// in `Stopped` state). The folder is created on
	// disk if it doesn't exist so the operator can
	// always `cd` into it and drop files.
	size_t loadMusicFolder(const std::filesystem::path &folderPath);

	// **Per-frame tick.** Call from `AppUpdate` once per
	// frame. Refreshes the playlist on a 5-second
	// interval and handles the "current track was
	// removed from disk" case gracefully (uninit the
	// sound, clamp the current index, do not auto-play).
	void tick();

	// **Hotkey actions.**
	// `togglePlayPause`: Stopped → load (if not loaded) +
	// start. Playing → pause (save cursor). Paused → resume
	// from saved cursor.
	// `stop`: stop + cursor = 0 + state = Stopped.
	// `increaseVolume` / `decreaseVolume`: clamp [0, 1] +
	// apply to the music group. `step` is in the same
	// 0..1 units as `volume()` (0.05 matches the existing
	// `kLightingExposureStepStops` style).
	void togglePlayPause();
	void stop();
	void increaseVolume(float step);
	void decreaseVolume(float step);

	// **State accessors** for the HUD / DebugStats
	// mirror. Cheap: all inline reads, no miniaudio
	// calls.
	MusicState state() const { return m_state; }
	float volume() const { return m_volume; }
	bool isInitialized() const { return m_engineInitialized; }
	bool isPlaylistEmpty() const { return m_playlist.empty(); }
	// Returns the filename of the current track, or
	// the empty string if the playlist is empty. The
	// returned reference is valid until the next
	// `tick` that re-scans the playlist.
	const std::string &currentTrackName() const { return m_currentTrackName; }
	const std::filesystem::path &musicFolder() const { return m_musicFolder; }
	size_t playlistSize() const { return m_playlist.size(); }
	size_t currentIndex() const { return m_currentIndex; }

private:
	// Rebuild `m_playlist` from `m_musicFolder`. Returns
	// the new size. Sorts alphabetically (case-sensitive
	// per `std::filesystem::path::compare`).
	size_t scanPlaylist();

	// Load the file at `m_playlist[m_currentIndex]`
	// into `m_sound`. Returns true on success. Unloads
	// any previously-loaded sound first.
	bool loadCurrentTrack();

	// Tear down the current `m_sound` if loaded.
	void unloadCurrentTrack();

	// Apply `m_volume` to the music group.
	void applyVolume();

	// Stop the current sound and save the cursor (for
	// pause → resume).
	void pauseImpl();

	ma_engine m_engine{};
	ma_sound_group m_musicGroup{};
	ma_sound m_sound{};

	bool m_engineInitialized = false;
	bool m_musicGroupInitialized = false;
	bool m_soundLoaded = false;

	std::filesystem::path m_musicFolder;
	std::vector<std::filesystem::path> m_playlist;
	size_t m_currentIndex = 0;
	float m_volume = 0.8f;
	MusicState m_state = MusicState::Stopped;

	// Saved cursor for pause → resume. 0 when not
	// paused. **Currently unused** in v1: miniaudio
	// does not expose `ma_sound_set_time` (the API
	// was removed in 0.10+), so v1 pause = stop +
	// forget cursor, and the next play starts from
	// 0. The field is kept (and reset to 0 in
	// `unloadCurrentTrack`) so v2 can re-introduce
	// the cursor-save path without a stale-read
	// hazard. See `decisions.md §28` for the v1
	// limitation.
	ma_uint64 m_pausedCursorMs = 0;

	// 5-second playlist refresh. Initialized to "now"
	// on the first scan so the operator gets a
	// responsive update on `loadMusicFolder`.
	std::chrono::steady_clock::time_point m_lastPlaylistRefresh{};

	// Cached so the HUD doesn't construct a `std::string`
	// every frame. Updated only when the playlist
	// (re)scans or the current index changes.
	std::string m_currentTrackName;
};

} // namespace projectv::audio
