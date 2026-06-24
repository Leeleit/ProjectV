#pragma once

#include <atomic> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <miniaudio.h>

namespace projectv::audio {

enum class AudioLoadError : std::uint8_t {
	PreconditionFailed = 0,
	FolderCreateFailed,
	ScanFailed,
};

constexpr std::string_view toString(AudioLoadError const e) noexcept
{
	switch (e) {
	case AudioLoadError::PreconditionFailed:
		return "PreconditionFailed";
	case AudioLoadError::FolderCreateFailed:
		return "FolderCreateFailed";
	case AudioLoadError::ScanFailed:
		return "ScanFailed";
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

class AudioEngine {
  public:
	AudioEngine() = default;
	~AudioEngine();

	AudioEngine(const AudioEngine &) = delete;
	AudioEngine &operator=(const AudioEngine &) = delete;

	[[nodiscard]] bool init();

	void shutdown();

	std::expected<size_t, AudioLoadError> loadMusicFolder(const std::filesystem::path &folderPath);

	void RefreshPlaylistAsync();

	void tick();

	void togglePlayPause();
	void stop();
	void increaseVolume(float step);
	void decreaseVolume(float step);
	void nextTrack();
	void previousTrack();

	[[nodiscard]] MusicState state() const { return m_state; }
	[[nodiscard]] float volume() const { return m_volume; }
	[[nodiscard]] bool isInitialized() const { return m_engineInitialized; }
	[[nodiscard]] bool isPlaylistEmpty() const { return m_playlist.empty(); }

	[[nodiscard]] const std::string &currentTrackName() const { return m_currentTrackName; }
	[[nodiscard]] const std::string &currentArtist() const { return m_currentArtist; }
	[[nodiscard]] const std::string &currentTitle() const { return m_currentTitle; }
	[[nodiscard]] float positionSeconds() const;
	[[nodiscard]] float durationSeconds() const;
	[[nodiscard]] const std::filesystem::path &musicFolder() const { return m_musicFolder; }
	[[nodiscard]] size_t playlistSize() const { return m_playlist.size(); }
	[[nodiscard]] size_t currentIndex() const { return m_currentIndex; }

  private:
	size_t scanPlaylist();

	bool loadCurrentTrack();

	void unloadCurrentTrack();

	bool goToTrack(size_t newIndex);

	void applyVolume();

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

	std::chrono::steady_clock::time_point m_lastPlaylistRefresh;

	std::mutex m_playlistMutex;
	std::atomic<bool> m_scanInProgress{false};
	std::jthread m_scanThread;

	std::string m_currentTrackName;
	std::string m_currentArtist;
	std::string m_currentTitle;
};

} // namespace projectv::audio
