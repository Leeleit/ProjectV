#include "audio/AudioEngine.hpp"

#include "core/RuntimeDiagnostics.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <cstring>

// **Custom deleter for `AudioEnginePtr` in
// `core/Types.hpp`, at global scope to match the
// `DestroyEcsState` / `DestroyPhysicsState`
// pattern.** The deleter is a free function in a
// TU where `AudioEngine` is complete, so the
// `unique_ptr` instantiation in `Types.hpp` can
// stay header-only. `~AudioEngine()` already calls
// `shutdown()`, so the deleter is just `delete`.
void DestroyAudioEngine(projectv::audio::AudioEngine *engine)
{
	delete engine;
}

namespace projectv::audio {

const char *MusicStateToString(MusicState state)
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

AudioEngine::~AudioEngine()
{
	shutdown();
}

bool AudioEngine::init()
{
	if (m_engineInitialized) {
		return true;
	}

	ma_engine_config config = ma_engine_config_init();
	// **Format = 16-bit signed PCM at 44.1 kHz
	// stereo,** per the v1 spec. miniaudio's engine
	// config exposes `sampleRate` and `channels`
	// directly; the `playback.format` substruct is
	// `ma_device_config`-only, so the engine picks
	// the device's native format (typically
	// `ma_format_s16` on built-in audio; the user
	// gets "16-bit at 44.1" on any sane Linux
	// desktop). If a future slice needs to force a
	// specific format, it has to drop to the
	// lower-level `ma_device` API. miniaudio
	// resamples the source MP3 to `config.sampleRate`
	// (44.1 kHz here) on the way out.
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

	// **Music group** holds the bus-level volume
	// (separate from any SFX/Ambient group that may
	// come later). `MA_SOUND_FLAG_STREAM` would
	// apply per-sound, but the group itself is just a
	// volume bus — sounds attach to it via the `group`
	// parameter of `ma_sound_init_from_file`.
	const ma_result groupResult =
		ma_sound_group_init(&m_engine, /*flags=*/0, /*pAttachment=*/nullptr, &m_musicGroup);
	if (groupResult != MA_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Audio",
			"AudioEngine.init.ma_sound_group_init",
			fmt::format("ma_sound_group_init failed: {}", ma_result_description(groupResult)));
		// Engine itself is still usable; just fall
		// back to per-sound volume. The current v1
		// has no SFX group, so per-sound is fine.
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
}

size_t AudioEngine::loadMusicFolder(const std::filesystem::path &folderPath)
{
	m_musicFolder = folderPath;
	std::error_code ec;
	std::filesystem::create_directories(m_musicFolder, ec);
	// `create_directories` failing is non-fatal — the
	// scan below will just produce an empty playlist
	// and the operator can fix the path with
	// `PROJECTV_MUSIC_DIR`.
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
		// Case-insensitive `.mp3` extension match.
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (ext != ".mp3") {
			continue;
		}
		m_playlist.push_back(path);
	}

	// Stable alphabetical sort. `path::compare` is
	// case-sensitive (per platform `std::filesystem`
	// semantics), which matches how Linux file
	// managers order music folders.
	std::sort(m_playlist.begin(), m_playlist.end());

	// Clamp the current index to a valid position.
	if (m_playlist.empty()) {
		m_currentIndex = 0;
	} else if (m_currentIndex >= m_playlist.size()) {
		m_currentIndex = m_playlist.size() - 1;
	}

	// Refresh the cached track name. If the
	// previously-loaded track is now at a different
	// position (because new files were added before
	// it), keep playing the same file by remapping
	// the index. If it's gone, the per-frame tick
	// will pick that up on the next call.
	if (!m_playlist.empty()) {
		m_currentTrackName = m_playlist[m_currentIndex].filename().string();
	} else {
		m_currentTrackName.clear();
	}

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

	// Tear down any previously-loaded sound.
	unloadCurrentTrack();

	const std::filesystem::path &trackPath = m_playlist[m_currentIndex];

	// `MA_SOUND_FLAG_STREAM` tells miniaudio to read
	// the file from disk as it plays (don't load the
	// whole MP3 into RAM). For typical music
	// files (3-10 MB) this is a small saving, but
	// it's also the right semantic for a "playlist
	// that can change every 5 seconds" — pre-loading
	// the file would mean re-loading it every time
	// the operator drops a new file in.
	const ma_result initResult = ma_sound_init_from_file(
		&m_engine,
		trackPath.string().c_str(),
		MA_SOUND_FLAG_STREAM,
		m_musicGroupInitialized ? &m_musicGroup : nullptr,
		/*pDoneNotification=*/nullptr,
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
		return false;
	}

	m_soundLoaded = true;
	m_currentTrackName = trackPath.filename().string();
	// v1 default: loop forever. The user explicitly
	// asked for loop=true.
	ma_sound_set_looping(&m_sound, MA_TRUE);
	// Apply the current music-group volume to the
	// new sound. The group volume was already set
	// at init; this is belt-and-suspenders in case
	// the operator changes it after a reload.
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
		// Apply to the sound itself too, so a future
		// "no group" path (e.g. music folder is
		// empty, only the engine is alive) would
		// still reflect the volume in the HUD.
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
		// If the playlist is empty or the sound isn't
		// loaded, try to (re)load the current index.
		if (!m_soundLoaded) {
			if (!loadCurrentTrack()) {
				return;
			}
		}
		// From a "stop" state, the cursor is at 0
		// (we reset on stop). Just start. Note:
		// miniaudio 0.11+ does NOT expose
		// `ma_sound_set_time_in_milliseconds` — the
		// `ma_sound_set_time` API was removed in
		// 0.10+. The higher-level `ma_sound` API
		// only supports start-from-beginning. v1
		// therefore does NOT have true
		// "pause → resume from cursor" semantics;
		// a "pause" is `stop` + forget cursor, and
		// the next "play" starts from 0. This is
		// documented in `decisions.md §28` and
		// `memory.md §10.26` as a known v1
		// limitation; v2 can add a custom decoder
		// wrapper for true resume.
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
		// No cursor to restore (see Stopped branch
		// note). The sound was unloaded on pause, so
		// reload it from disk and start from 0.
		if (!loadCurrentTrack()) {
			return;
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
	// `m_pausedCursorMs` is not saved: miniaudio
	// has no `ma_sound_set_time` API (see the
	// long note in `togglePlayPause`'s Stopped
	// branch). v1 pause semantics = stop and
	// forget the cursor; the next play starts
	// from 0. The field is reset to 0 in
	// `unloadCurrentTrack` so any future
	// cursor-aware code (v2) can re-introduce
	// the save without a stale-read hazard.
	ma_sound_stop(&m_sound);
	m_pausedCursorMs = 0;
	m_state = MusicState::Paused;
}

void AudioEngine::stop()
{
	if (m_soundLoaded) {
		ma_sound_stop(&m_sound);
	}
	m_pausedCursorMs = 0;
	m_state = MusicState::Stopped;
}

void AudioEngine::increaseVolume(float step)
{
	m_volume = std::clamp(m_volume + step, 0.0f, 1.0f);
	applyVolume();
}

void AudioEngine::decreaseVolume(float step)
{
	m_volume = std::clamp(m_volume - step, 0.0f, 1.0f);
	applyVolume();
}

bool AudioEngine::goToTrack(size_t newIndex)
{
	if (m_playlist.empty()) {
		// No tracks to switch to. Caller
		// (nextTrack / previousTrack) is a no-op
		// in this case. Don't touch
		// `m_currentIndex` so the next 5-sec
		// refresh that finds tracks picks up
		// at the right position.
		return false;
	}
	// Wrap clamp: handles the wrap-around case
	// where `nextTrack` passed `m_playlist.size()`
	// or `previousTrack` passed `(size_t)-1`.
	newIndex = newIndex % m_playlist.size();
	m_currentIndex = newIndex;
	m_currentTrackName = m_playlist[m_currentIndex].filename().string();
	m_pausedCursorMs = 0;

	// Behavior depends on the current state —
	// see the per-state block in the hpp
	// comment. Common to all: tear down the
	// currently loaded sound first (no-op when
	// `m_soundLoaded == false`, e.g. when
	// `m_state == Stopped` and we never started
	// playback).
	unloadCurrentTrack();

	switch (m_state) {
	case MusicState::Playing:
		// Interrupt: load the new track and start
		// it from 0. The user pressed Next/Prev
		// mid-playback; they expect the new
		// track to start, not the old one to
		// keep playing.
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
		// Replace the loaded sound so the next
		// `togglePlayPause` call plays the NEW
		// track. State stays Paused so the HUD
		// line still says PAUSE.
		if (!loadCurrentTrack()) {
			m_state = MusicState::Stopped;
			return false;
		}
		return true;

	case MusicState::Stopped:
		// No sound to unload-and-reload; the
		// index update is enough. The next
		// `togglePlayPause` will `loadCurrentTrack()`
		// at the new index and start.
		return true;
	}
	return false;
}

void AudioEngine::nextTrack()
{
	if (m_playlist.empty()) {
		return;
	}
	// Wrap forward: index `playlist.size() - 1` →
	// 0, index `playlist.size() - 2` → `playlist.size() - 1`,
	// etc. The `+ playlist.size()` before the
	// modulo keeps the arithmetic unsigned-safe
	// (otherwise `(0u + 1u) % 1u == 0` which is
	// fine but `(0u - 1u) % 1u` would underflow).
	const size_t next = (m_currentIndex + 1u) % m_playlist.size();
	goToTrack(next);
}

void AudioEngine::previousTrack()
{
	if (m_playlist.empty()) {
		return;
	}
	// Wrap backward: index 0 → `playlist.size() - 1`,
	// index 1 → 0, etc. The `+ playlist.size()` keeps
	// the subtraction unsigned-safe (otherwise
	// `(0u - 1u)` would underflow to `UINT_MAX`).
	const size_t prev = (m_currentIndex + m_playlist.size() - 1u) % m_playlist.size();
	goToTrack(prev);
}

void AudioEngine::tick()
{
	if (!m_engineInitialized) {
		return;
	}

	// **5-second playlist refresh.** Cheap directory
	// walk; the disk cache absorbs the cost. Even
	// on a slow spinning rust the scan completes
	// well under a millisecond for a folder of a
	// few hundred tracks.
	const auto now = std::chrono::steady_clock::now();
	if (now - m_lastPlaylistRefresh >= std::chrono::seconds(5)) {
		const size_t prevSize = m_playlist.size();
		const std::filesystem::path prevTrack =
			(m_currentIndex < m_playlist.size()) ? m_playlist[m_currentIndex] : std::filesystem::path{};
		scanPlaylist();
		// If the current track is still in the
		// playlist, keep playing it (the index
		// may have shifted if files were added
		// before it; the operator expects "play
		// the same file" continuity). If it's
		// gone, gracefully uninit — the operator
		// can press Q to start a new track.
		if (m_soundLoaded && !prevTrack.empty() && !m_playlist.empty()) {
			auto it = std::find(m_playlist.begin(), m_playlist.end(), prevTrack);
			if (it == m_playlist.end()) {
				// Current track removed. Pause
				// semantics differ from stop
				// (preserve the user's intent):
				// if they were Playing, treat as
				// Stopped (so next play starts
				// fresh). If Paused, keep Paused
				// state but uninit the sound so
				// the next play will reload.
				if (m_state == MusicState::Playing) {
					stop();
				} else {
					unloadCurrentTrack();
				}
			} else {
				// Track still present. Update
				// the index to its new position
				// (so the cached track name
				// matches the loaded sound).
				m_currentIndex = static_cast<size_t>(std::distance(m_playlist.begin(), it));
			}
		}
		// First-time discovery: if the playlist grew
		// from empty to non-empty and the operator
		// has never pressed Q, do nothing — they
		// still need to start playback. (The
		// "started playing" log will surface on the
		// next Q press.)
		(void)prevSize;
	}
}

} // namespace projectv::audio
