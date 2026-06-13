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
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <miniaudio.h>

namespace projectv::audio {

// **Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
// `AudioEngine::loadMusicFolder`. The success value is the
// track count (`size_t`); 0 is a valid result (empty folder
// or no `.mp3` files). Error variants surface recoverable
// failures (`create_directories` / `directory_iterator`)
// that the old `size_t` return couldn't distinguish from
// "folder is fine, just empty". Cold path (1× per
// startup / 5-second playlist refresh), so the
// `std::expected` cost is irrelevant.
enum class AudioLoadError : std::uint8_t {
	PreconditionFailed = 0,
	FolderCreateFailed,
	ScanFailed,
};

constexpr std::string_view toString(AudioLoadError e) noexcept {
	switch (e) {
	case AudioLoadError::PreconditionFailed: return "PreconditionFailed";
	case AudioLoadError::FolderCreateFailed: return "FolderCreateFailed";
	case AudioLoadError::ScanFailed: return "ScanFailed";
	}
	return "Unknown";
}

// **Artist / title parser, 2026-06-13.** Splits a
// filename like "Le1t - Palm Trees.mp3" into
// `("Le1t", "Palm Trees")` for the HUD. Convention
// is `<artist> - <title>.mp3` (space-dash-space
// separator), matching the operator's current music
// folder. The `.mp3` extension is stripped
// case-insensitively before splitting. On
// no-separator (e.g. "StandaloneTrack.mp3"):
// `artist` becomes "-" (em-dash) and `title` is the
// full stem. The function is exported so the
// `AudioEngine::updateCurrentTrackMetadata` helper
// and any future consumers (save/load, sidecar) can
// call it without duplicating the logic.
void ParseArtistTitle(const std::string &filename,
					  std::string &artist, std::string &title);

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
	// **Tier 1.B (`2026-06-13`).** Returns
	// `std::expected<size_t, AudioLoadError>`. The success
	// value is the number of `.mp3` tracks discovered; 0 is
	// a valid result (empty folder). The error variants
	// surface recoverable failures that the old `size_t`
	// return couldn't distinguish from the "empty folder"
	// case. Callers use `.value_or(0)` to preserve the
	// historical "0 is valid" contract.
	std::expected<size_t, projectv::audio::AudioLoadError> loadMusicFolder(const std::filesystem::path &folderPath);

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
	// `nextTrack` / `previousTrack`: cycle through the
	// 5-sec-refresh playlist with wrap-around
	// (next from last → 0; prev from 0 → last).
	// Behavior on each state:
	// - Playing: stop current, reload, start new
	//   (interrupts the current track).
	// - Paused: stop current, reload (state stays
	//   Paused; the new track is loaded but not
	//   playing). Resume would now play the new
	//   track.
	// - Stopped: just update the index (no sound
	//   to reload; the next play will load the
	//   new track).
	// Empty playlist: no-op (hotkey does
	// nothing, same as the existing play/pause
	// no-op behavior on an empty playlist).
	void togglePlayPause();
	void stop();
	void increaseVolume(float step);
	void decreaseVolume(float step);
	void nextTrack();
	void previousTrack();

	// **State accessors** for the HUD / DebugStats
	// mirror. Cheap: all inline reads, no miniaudio
	// calls.
	[[nodiscard]] MusicState state() const { return m_state; }
	[[nodiscard]] float volume() const { return m_volume; }
	[[nodiscard]] bool isInitialized() const { return m_engineInitialized; }
	[[nodiscard]] bool isPlaylistEmpty() const { return m_playlist.empty(); }
	// Returns the filename of the current track, or
	// the empty string if the playlist is empty. The
	// returned reference is valid until the next
	// `tick` that re-scans the playlist.
	[[nodiscard]] const std::string &currentTrackName() const { return m_currentTrackName; }
	// **Artist / title for the HUD, 2026-06-13.**
	// Cached parsed form of `currentTrackName()` per
	// the `ParseArtistTitle` convention. `artist` is
	// `"-"` (em-dash) when the filename has no
	// ` - ` separator; `title` is the stem with the
	// `.mp3` stripped. Both are empty when the
	// playlist is empty. Re-parsed only when
	// `m_currentTrackName` changes (see
	// `updateCurrentTrackMetadata`).
	[[nodiscard]] const std::string &currentArtist() const { return m_currentArtist; }
	[[nodiscard]] const std::string &currentTitle() const { return m_currentTitle; }
	// **Playback position / duration, 2026-06-13.**
	// Position is the cursor in seconds (0.0f when
	// no sound is loaded; otherwise read via
	// `ma_sound_get_cursor_in_seconds`). Duration
	// is the track length in seconds (0.0f when no
	// sound is loaded or the decoder does not
	// expose length — the latter is rare for MP3
	// with `MA_SOUND FLAG_STREAM` but possible for
	// malformed streams). Both are cheap (one
	// miniaudio call each, no allocation).
	[[nodiscard]] float positionSeconds() const;
	[[nodiscard]] float durationSeconds() const;
	[[nodiscard]] const std::filesystem::path &musicFolder() const { return m_musicFolder; }
	[[nodiscard]] size_t playlistSize() const { return m_playlist.size(); }
	[[nodiscard]] size_t currentIndex() const { return m_currentIndex; }

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

	// **Internal helper for `nextTrack` / `previousTrack`.**
	// Clamps `newIndex` to a valid position in
	// the playlist, updates `m_currentIndex` and
	// the cached `m_currentTrackName`, then
	// re-loads the sound according to the
	// current `m_state` (Playing → reload + start;
	// Paused → reload; Stopped → just update
	// index). No-op if the playlist is empty.
	// Returns true on success (the new track
	// loaded and, when state was Playing, started
	// without error).
	bool goToTrack(size_t newIndex);

	// Apply `m_volume` to the music group.
	void applyVolume();

	// Stop the current sound and save the cursor (for
	// pause → resume).
	void pauseImpl();

	// **Artist / title cache updater, 2026-06-13.**
	// Re-parses `m_currentTrackName` into
	// `m_currentArtist` / `m_currentTitle` via
	// `ParseArtistTitle`. Called from every site
	// that mutates `m_currentTrackName`
	// (`scanPlaylist`, `loadCurrentTrack` on both
	// success and failure, `shutdown`). Cheap
	// (O(filename.length) string search + two
	// `substr` copies) and only runs on track
	// changes, not per frame.
	void updateCurrentTrackMetadata();

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

	// **Dead code, 2026-06-13.** Saved cursor
	// for pause → resume, 0 when not paused.
	// Miniaudio's `ma_sound_stop` preserves the
	// cursor in-place on the `ma_sound` struct
	// (it only sets node state to stopped, see
	// miniaudio.h:78774), so a subsequent
	// `ma_sound_start` resumes from the
	// preserved position naturally — no manual
	// cursor save is needed. The field is
	// never read in v1; the multiple reset-to-0
	// sites (`pauseImpl`, `stop`, `unloadCurrentTrack`,
	// `goToTrack`) are no-ops. Kept for
	// field-shape stability (removing it would
	// shift the binary layout and break any
	// out-of-tree debug tools reading the
	// `AudioEngine` memory). v2 cleanup can
	// remove it along with a comment sweep.
	ma_uint64 m_pausedCursorMs = 0;

	// 5-second playlist refresh. Initialized to "now"
	// on the first scan so the operator gets a
	// responsive update on `loadMusicFolder`.
	std::chrono::steady_clock::time_point m_lastPlaylistRefresh;

	// Cached so the HUD doesn't construct a `std::string`
	// every frame. Updated only when the playlist
	// (re)scans or the current index changes.
	std::string m_currentTrackName;
	// **Artist / title cache, 2026-06-13.** Parsed
	// from `m_currentTrackName` via
	// `updateCurrentTrackMetadata` (called from
	// the same sites that update the name). Both
	// are empty when the playlist is empty;
	// `m_currentArtist` is `"-"` (em-dash) when
	// the filename has no ` - ` separator. The
	// parser does not allocate unless the inputs
	// differ from the previous track.
	std::string m_currentArtist;
	std::string m_currentTitle;
};

} // namespace projectv::audio
