#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <miniaudio.h>

namespace projectv::audio {

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

void ParseArtistTitle(const std::string &filename,
					  std::string &artist, std::string &title);

enum class MusicState : uint8_t {
	Stopped = 0,
	Playing,
	Paused,
};

const char *MusicStateToString(MusicState state);

/// \brief **Audio engine class.** Holds one `ma_engine`, one
///
/// \details
///  `ma_sound_group` (for music volume bus-level control),

///  and one `ma_sound` (the currently-loaded track). The

///  engine is a single instance on `AppState` (mirrors

///  `legacy/docs/architecture/practice/02_engine_bootstrap_spec.md:533`)

///  — it is not an ECS system in v1, just a plain

///  singleton, because the music hotkeys fire from

///  `UpdateApp` and don't need per-entity iteration.

class AudioEngine {
  public:
	AudioEngine() = default;
	~AudioEngine();

	AudioEngine(const AudioEngine &) = delete;
	AudioEngine &operator=(const AudioEngine &) = delete;

	/// \brief Initialize the engine.
	///
	/// \details
	/// Format = 16-bit signed PCM,
	///  44.1 kHz, stereo. Non-fatal on failure (logs and

	///  stays uninited; the rest of the program keeps

	///  running). Calling `init` after a successful `init`

	///  is a no-op.

	bool init();

	/// \brief Symmetric teardown.
	///
	/// \details
	/// Safe to call multiple times.
	void shutdown();

	std::expected<size_t, projectv::audio::AudioLoadError> loadMusicFolder(const std::filesystem::path &folderPath);

	/// \brief **Per-frame tick.** Call from `AppUpdate` once per
	///
	/// \details
	///  frame. Refreshes the playlist on a 5-second

	///  interval and handles the "current track was

	///  removed from disk" case gracefully (uninit the

	///  sound, clamp the current index, do not auto-play).

	void tick();

	/// \brief **Hotkey actions.**
	///
	/// \details
	///  `togglePlayPause`: Stopped → load (if not loaded) +

	///  start. Playing → pause (save cursor). Paused → resume

	///  from saved cursor.

	///  `stop`: stop + cursor = 0 + state = Stopped.

	///  `increaseVolume` / `decreaseVolume`: clamp [0, 1] +

	///  apply to the music group. `step` is in the same

	///  0..1 units as `volume()` (0.05 matches the existing

	///  `kLightingExposureStepStops` style).

	///  `nextTrack` / `previousTrack`: cycle through the

	///  5-sec-refresh playlist with wrap-around

	///  (next from last → 0; prev from 0 → last).

	///  Behavior on each state:

	///  - Playing: stop current, reload, start new

	///    (interrupts the current track).

	///  - Paused: stop current, reload (state stays

	///    Paused; the new track is loaded but not

	///    playing). Resume would now play the new

	///    track.

	///  - Stopped: just update the index (no sound

	///    to reload; the next play will load the

	///    new track).

	///  Empty playlist: no-op (hotkey does

	///  nothing, same as the existing play/pause

	///  no-op behavior on an empty playlist).

	void togglePlayPause();
	void stop();
	void increaseVolume(float step);
	void decreaseVolume(float step);
	void nextTrack();
	void previousTrack();

	/// \brief **State accessors** for the HUD / DebugStats
	///
	/// \details
	///  mirror. Cheap: all inline reads, no miniaudio

	///  calls.

	[[nodiscard]] MusicState state() const { return m_state; }
	[[nodiscard]] float volume() const { return m_volume; }
	[[nodiscard]] bool isInitialized() const { return m_engineInitialized; }
	[[nodiscard]] bool isPlaylistEmpty() const { return m_playlist.empty(); }
	/// \brief Returns the filename of the current track, or
	///
	/// \details
	///  the empty string if the playlist is empty. The

	///  returned reference is valid until the next

	///  `tick` that re-scans the playlist.

	[[nodiscard]] const std::string &currentTrackName() const { return m_currentTrackName; }
	[[nodiscard]] const std::string &currentArtist() const { return m_currentArtist; }
	[[nodiscard]] const std::string &currentTitle() const { return m_currentTitle; }
	[[nodiscard]] float positionSeconds() const;
	[[nodiscard]] float durationSeconds() const;
	[[nodiscard]] const std::filesystem::path &musicFolder() const { return m_musicFolder; }
	[[nodiscard]] size_t playlistSize() const { return m_playlist.size(); }
	[[nodiscard]] size_t currentIndex() const { return m_currentIndex; }

  private:
	/// \brief Rebuild `m_playlist` from `m_musicFolder`.
	///
	/// \details
	/// Returns
	///  the new size. Sorts alphabetically (case-sensitive

	///  per `std::filesystem::path::compare`).

	size_t scanPlaylist();

	/// \brief Load the file at `m_playlist[m_currentIndex]`
	///
	/// \details
	///  into `m_sound`. Returns true on success. Unloads

	///  any previously-loaded sound first.

	bool loadCurrentTrack();

	/// \brief Tear down the current `m_sound` if loaded.
	void unloadCurrentTrack();

	/// \brief **Internal helper for `nextTrack` / `previousTrack`.**
	///
	/// \details
	///  Clamps `newIndex` to a valid position in

	///  the playlist, updates `m_currentIndex` and

	///  the cached `m_currentTrackName`, then

	///  re-loads the sound according to the

	///  current `m_state` (Playing → reload + start;

	///  Paused → reload; Stopped → just update

	///  index). No-op if the playlist is empty.

	///  Returns true on success (the new track

	///  loaded and, when state was Playing, started

	///  without error).

	bool goToTrack(size_t newIndex);

	/// \brief Apply `m_volume` to the music group.
	void applyVolume();

	/// \brief Stop the current sound and save the cursor (for
	///
	/// \details
	///  pause → resume).

	void pauseImpl();

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

	ma_uint64 m_pausedCursorMs = 0;

	/// \brief 5-second playlist refresh.
	///
	/// \details
	/// Initialized to "now"
	///  on the first scan so the operator gets a

	///  responsive update on `loadMusicFolder`.

	std::chrono::steady_clock::time_point m_lastPlaylistRefresh;

	/// \brief Cached so the HUD doesn't construct a `std::string`
	///
	/// \details
	///  every frame. Updated only when the playlist

	///  (re)scans or the current index changes.

	std::string m_currentTrackName;
	std::string m_currentArtist;
	std::string m_currentTitle;
};

} // namespace projectv::audio
