#include "audio/AudioEngine.hpp"

#include "core/RuntimeDiagnostics.hpp"

#include "fmt/format.h"

#include <algorithm>

void DestroyAudioEngine(projectv::audio::AudioEngine *engine)
{
	delete engine;
}

namespace projectv::audio {

const char *MusicStateToString(const MusicState state)
{
	switch (state) {
	case MusicState::Stopped:
		return "STOP";
	case MusicState::Playing:
		return "PLAY";
	case MusicState::Paused:
		return "PAUSE";
	}
	return "STOP";
}

void ParseArtistTitle(const std::string &filename,
					  std::string &artist, std::string &title)
{
	std::string stem = filename;
	if (stem.size() >= 4) {
		std::string ext = stem.substr(stem.size() - 4);
		std::transform(ext.begin(), ext.end(), ext.begin(),
					   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (ext == ".mp3") {
			stem = stem.substr(0, stem.size() - 4);
		}
	}
	constexpr std::string_view kSeparator = " - ";
	const auto pos = stem.find(kSeparator);
	if (pos == std::string::npos) {
		artist = "-";
		title = stem;
	} else {
		artist = stem.substr(0, pos);
		title = stem.substr(pos + kSeparator.size());
	}
}

AudioEngine::~AudioEngine() noexcept
{
	try {
		shutdown();
	} catch (...) {
		runtime::LogRuntimeFailure(
			"Audio",
			"AudioEngine.~AudioEngine",
			"shutdown() threw during destruction; exception swallowed to satisfy noexcept");
	}
}

bool AudioEngine::init()
{
	if (m_engineInitialized) {
		return true;
	}

	ma_engine_config config = ma_engine_config_init();

	config.sampleRate = 44100;
	config.channels = 2;
	config.listenerCount = 1; // 3D-ready; unused in v1.

	const ma_result initResult = ma_engine_init(&config, &m_engine);
	if (initResult != MA_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Audio",
			"AudioEngine.init.ma_engine_init",
			fmt::format("ma_engine_init failed: {}", ma_result_description(initResult)));
		return false;
	}
	m_engineInitialized = true;

	const ma_result groupResult =
		ma_sound_group_init(&m_engine, /*flags=*/0, /*pParentGroup=*/nullptr, &m_musicGroup);
	if (groupResult != MA_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Audio",
			"AudioEngine.init.ma_sound_group_init",
			fmt::format("ma_sound_group_init failed: {}", ma_result_description(groupResult)));

		m_musicGroupInitialized = false;
	} else {
		m_musicGroupInitialized = true;
		ma_sound_group_set_volume(&m_musicGroup, m_volume);
	}

	return true;
}

void AudioEngine::shutdown()
{
	unloadCurrentTrack();

	if (m_musicGroupInitialized) {
		ma_sound_group_uninit(&m_musicGroup);
		m_musicGroupInitialized = false;
	}
	if (m_engineInitialized) {
		ma_engine_uninit(&m_engine);
		m_engineInitialized = false;
	}
	m_state = MusicState::Stopped;
	m_pausedCursorMs = 0;
	m_playlist.clear();
	m_currentTrackName.clear();
	updateCurrentTrackMetadata();
}

std::expected<size_t, AudioLoadError> AudioEngine::loadMusicFolder(const std::filesystem::path &folderPath)
{
	m_musicFolder = folderPath;
	if (m_musicFolder.empty()) {
		return std::unexpected(AudioLoadError::PreconditionFailed);
	}
	std::error_code createEc;
	std::filesystem::create_directories(m_musicFolder, createEc);
	if (createEc) {

		runtime::LogRuntimeFailure("Audio", "loadMusicFolder.create_directories", createEc.message());
		return std::unexpected(AudioLoadError::FolderCreateFailed);
	}
	return scanPlaylist();
}

size_t AudioEngine::scanPlaylist()
{
	m_playlist.clear();
	if (m_musicFolder.empty()) {
		return 0;
	}

	std::error_code ec;
	if (!std::filesystem::is_directory(m_musicFolder, ec)) {
		return 0;
	}

	for (const auto &entry : std::filesystem::directory_iterator(m_musicFolder, ec)) {
		if (ec) {
			break;
		}
		if (!entry.is_regular_file(ec)) {
			continue;
		}
		const auto &path = entry.path();
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
					   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (ext != ".mp3") {
			continue;
		}
		m_playlist.push_back(path);
	}

	std::sort(m_playlist.begin(), m_playlist.end());

	if (m_playlist.empty()) {
		m_currentIndex = 0;
	} else if (m_currentIndex >= m_playlist.size()) {
		m_currentIndex = m_playlist.size() - 1;
	}

	if (!m_playlist.empty()) {
		m_currentTrackName = m_playlist[m_currentIndex].filename().string();
	} else {
		m_currentTrackName.clear();
	}

	updateCurrentTrackMetadata();

	m_lastPlaylistRefresh = std::chrono::steady_clock::now();
	return m_playlist.size();
}

bool AudioEngine::loadCurrentTrack()
{
	if (!m_engineInitialized) {
		return false;
	}
	if (m_playlist.empty()) {
		return false;
	}
	if (m_currentIndex >= m_playlist.size()) {
		return false;
	}

	unloadCurrentTrack();

	const std::filesystem::path &trackPath = m_playlist[m_currentIndex];

	const ma_result initResult = ma_sound_init_from_file(
		&m_engine,
		trackPath.string().c_str(),
		MA_SOUND_FLAG_STREAM,
		m_musicGroupInitialized ? &m_musicGroup : nullptr,
		/*pDoneFence=*/nullptr,
		&m_sound);

	if (initResult != MA_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Audio",
			"AudioEngine.loadCurrentTrack.ma_sound_init_from_file",
			fmt::format("ma_sound_init_from_file({}) failed: {}",
						trackPath.string(),
						ma_result_description(initResult)));
		m_soundLoaded = false;
		m_currentTrackName.clear();
		updateCurrentTrackMetadata();
		return false;
	}

	m_soundLoaded = true;
	m_currentTrackName = trackPath.filename().string();
	updateCurrentTrackMetadata();

	ma_sound_set_looping(&m_sound, MA_TRUE);

	applyVolume();
	return true;
}

void AudioEngine::unloadCurrentTrack()
{
	if (m_soundLoaded) {
		ma_sound_uninit(&m_sound);
		m_soundLoaded = false;
	}
	m_pausedCursorMs = 0;
}

void AudioEngine::applyVolume()
{
	if (m_musicGroupInitialized) {
		ma_sound_group_set_volume(&m_musicGroup, m_volume);
	}
	if (m_soundLoaded) {

		ma_sound_set_volume(&m_sound, m_volume);
	}
}

void AudioEngine::togglePlayPause()
{
	if (!m_engineInitialized) {
		return;
	}

	switch (m_state) {
	case MusicState::Stopped:

		if (!m_soundLoaded) {
			if (!loadCurrentTrack()) {
				return;
			}
		}
		if (ma_sound_start(&m_sound) != MA_SUCCESS) {
			runtime::LogRuntimeFailure(
				"Audio",
				"AudioEngine.togglePlayPause.ma_sound_start",
				"ma_sound_start returned non-success");
			return;
		}
		m_state = MusicState::Playing;
		break;

	case MusicState::Playing:
		pauseImpl();
		break;

	case MusicState::Paused:
		if (!m_soundLoaded) {
			if (!loadCurrentTrack()) {
				return;
			}
		}
		if (ma_sound_start(&m_sound) != MA_SUCCESS) {
			runtime::LogRuntimeFailure(
				"Audio",
				"AudioEngine.togglePlayPause.ma_sound_start",
				"ma_sound_start (resume) returned non-success");
			return;
		}
		m_state = MusicState::Playing;
		break;
	}
}

void AudioEngine::pauseImpl()
{
	if (!m_soundLoaded) {
		m_state = MusicState::Stopped;
		return;
	}
	ma_sound_stop(&m_sound);
	m_pausedCursorMs = 0;
	m_state = MusicState::Paused;
}

void AudioEngine::stop()
{
	if (m_soundLoaded) {
		ma_sound_stop(&m_sound);
		const ma_result seekResult = ma_sound_seek_to_pcm_frame(&m_sound, 0);
		if (seekResult != MA_SUCCESS) {

			if (seekResult != MA_NOT_IMPLEMENTED) {
				runtime::LogRuntimeFailure(
					"Audio",
					"AudioEngine.stop.ma_sound_seek_to_pcm_frame",
					fmt::format("ma_sound_seek_to_pcm_frame(0) failed: {}",
								ma_result_description(seekResult)));
			}
		}
	}
	m_pausedCursorMs = 0;
	m_state = MusicState::Stopped;
}

void AudioEngine::increaseVolume(const float step)
{
	m_volume = std::clamp(m_volume + step, 0.0f, 1.0f);
	applyVolume();
}

void AudioEngine::decreaseVolume(const float step)
{
	m_volume = std::clamp(m_volume - step, 0.0f, 1.0f);
	applyVolume();
}

bool AudioEngine::goToTrack(size_t newIndex)
{
	if (m_playlist.empty()) {

		return false;
	}

	newIndex = newIndex % m_playlist.size();
	m_currentIndex = newIndex;
	m_currentTrackName = m_playlist[m_currentIndex].filename().string();
	m_pausedCursorMs = 0;

	unloadCurrentTrack();

	switch (m_state) {
	case MusicState::Playing:

		if (!loadCurrentTrack()) {
			m_state = MusicState::Stopped;
			return false;
		}
		if (ma_sound_start(&m_sound) != MA_SUCCESS) {
			runtime::LogRuntimeFailure(
				"Audio",
				"AudioEngine.goToTrack.ma_sound_start",
				"ma_sound_start returned non-success after track switch");
			m_state = MusicState::Stopped;
			return false;
		}
		return true;

	case MusicState::Paused:

		if (!loadCurrentTrack()) {
			m_state = MusicState::Stopped;
			return false;
		}
		return true;

	case MusicState::Stopped:

		return true;
	}
	return false;
}

void AudioEngine::nextTrack()
{
	if (m_playlist.empty()) {
		return;
	}

	const size_t next = (m_currentIndex + 1u) % m_playlist.size();
	goToTrack(next);
}

void AudioEngine::previousTrack()
{
	if (m_playlist.empty()) {
		return;
	}

	const size_t prev = (m_currentIndex + m_playlist.size() - 1u) % m_playlist.size();
	goToTrack(prev);
}

void AudioEngine::tick()
{
	if (!m_engineInitialized) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (now - m_lastPlaylistRefresh >= std::chrono::seconds(5)) {
		const size_t prevSize = m_playlist.size();
		const std::filesystem::path prevTrack =
			m_currentIndex < m_playlist.size() ? m_playlist[m_currentIndex] : std::filesystem::path{};
		scanPlaylist();

		if (m_soundLoaded && !prevTrack.empty() && !m_playlist.empty()) {
			const auto it = std::ranges::find(m_playlist, prevTrack);
			if (it == m_playlist.end()) {

				if (m_state == MusicState::Playing) {
					stop();
				} else {
					unloadCurrentTrack();
				}
			} else {

				m_currentIndex = static_cast<size_t>(std::distance(m_playlist.begin(), it));
			}
		}

		(void)prevSize;
	}
}

void AudioEngine::updateCurrentTrackMetadata()
{

	ParseArtistTitle(m_currentTrackName, m_currentArtist, m_currentTitle);
}

float AudioEngine::positionSeconds() const
{

	if (!m_soundLoaded) {
		return 0.0f;
	}
	float cursorSeconds = 0.0f;
	const ma_result result = ma_sound_get_cursor_in_seconds(&m_sound, &cursorSeconds);
	if (result != MA_SUCCESS) {

		return 0.0f;
	}
	return cursorSeconds;
}

float AudioEngine::durationSeconds() const
{

	if (!m_soundLoaded) {
		return 0.0f;
	}
	float lengthSeconds = 0.0f;
	const ma_result result = ma_sound_get_length_in_seconds(&m_sound, &lengthSeconds);
	if (result != MA_SUCCESS) {
		return 0.0f;
	}
	return lengthSeconds;
}

} // namespace projectv::audio
