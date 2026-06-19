#include "audio/AudioEngine.hpp"

#include "core/RuntimeDiagnostics.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <cctype>

/// \brief **Custom deleter for `AudioEnginePtr` in
///
/// \details
///  `core/Types.hpp`, at global scope to match the

///  `DestroyEcsState` / `DestroyPhysicsState`

///  pattern.** The deleter is a free function in a

///  TU where `AudioEngine` is complete, so the

///  `unique_ptr` instantiation in `Types.hpp` can

///  stay header-only. `~AudioEngine()` already calls

///  `shutdown()`, so the deleter is just `delete`.

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
		std::ranges::transform(ext, ext.begin(),
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
	/// \brief **Format = 16-bit signed PCM at 44.1 kHz
	///
	/// \details
	///  stereo,** per the v1 spec. miniaudio's engine

	///  config exposes `sampleRate` and `channels`

	///  directly; the `playback.format` substruct is

	///  `ma_device_config`-only, so the engine picks

	///  the device's native format (typically

	///  `ma_format_s16` on built-in audio; the user

	///  gets "16-bit at 44.1" on any sane Linux

	///  desktop). If a future slice needs to force a

	///  specific format, it has to drop to the

	///  lower-level `ma_device` API. miniaudio

	///  resamples the source MP3 to `config.sampleRate`

	///  (44.1 kHz here) on the way out.

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

	/// \brief **Music group** holds the bus-level volume
	///
	/// \details
	///  (separate from any SFX/Ambient group that may

	///  come later). `MA_SOUND_FLAG_STREAM` would

	///  apply per-sound, but the group itself is just a

	///  volume bus — sounds attach to it via the `group`

	///  parameter of `ma_sound_init_from_file`.

	const ma_result groupResult =
		ma_sound_group_init(&m_engine, /*flags=*/0, /*pParentGroup=*/nullptr, &m_musicGroup);
	if (groupResult != MA_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Audio",
			"AudioEngine.init.ma_sound_group_init",
			fmt::format("ma_sound_group_init failed: {}", ma_result_description(groupResult)));
		/// \brief Engine itself is still usable; just fall
		///
		/// \details
		///  back to per-sound volume. The current v1

		///  has no SFX group, so per-sound is fine.

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

std::expected<size_t, projectv::audio::AudioLoadError> AudioEngine::loadMusicFolder(const std::filesystem::path &folderPath)
{
	m_musicFolder = folderPath;
	if (m_musicFolder.empty()) {
		return std::unexpected(projectv::audio::AudioLoadError::PreconditionFailed);
	}
	std::error_code createEc;
	std::filesystem::create_directories(m_musicFolder, createEc);
	if (createEc) {
		/// \brief `create_directories` failing is non-fatal — the
		///
		/// \details
		///  scan below will just produce an empty playlist

		///  and the operator can fix the path with

		///  `PROJECTV_MUSIC_DIR`. We surface the variant for

		///  visibility but the caller can still treat

		///  "0 tracks" as a valid outcome.

		runtime::LogRuntimeFailure("Audio", "loadMusicFolder.create_directories", createEc.message());
		return std::unexpected(projectv::audio::AudioLoadError::FolderCreateFailed);
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
		/// \brief Case-insensitive `.mp3` extension match.
		std::string ext = path.extension().string();
		std::ranges::transform(ext, ext.begin(),
							   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (ext != ".mp3") {
			continue;
		}
		m_playlist.push_back(path);
	}

	/// \brief Stable alphabetical sort.
	///
	/// \details
	/// `path::compare` is
	///  case-sensitive (per platform `std::filesystem`

	///  semantics), which matches how Linux file

	///  managers order music folders.

	std::ranges::sort(m_playlist);

	/// \brief Clamp the current index to a valid position.
	if (m_playlist.empty()) {
		m_currentIndex = 0;
	} else if (m_currentIndex >= m_playlist.size()) {
		m_currentIndex = m_playlist.size() - 1;
	}

	/// \brief Refresh the cached track name.
	///
	/// \details
	/// If the
	///  previously-loaded track is now at a different

	///  position (because new files were added before

	///  it), keep playing the same file by remapping

	///  the index. If it's gone, the per-frame tick

	///  will pick that up on the next call.

	if (!m_playlist.empty()) {
		m_currentTrackName = m_playlist[m_currentIndex].filename().string();
	} else {
		m_currentTrackName.clear();
	}
	/// \brief Re-parse artist / title from the new name.
	///
	/// \details
	///  Called every scan, not per frame, so the cost

	///  is amortized over the 5-second refresh

	///  interval. `updateCurrentTrackMetadata` is the

	///  single source of truth for keeping the

	///  artist/title cache in sync with the name.

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

	/// \brief Tear down any previously-loaded sound.
	unloadCurrentTrack();

	const std::filesystem::path &trackPath = m_playlist[m_currentIndex];

	/// \brief `MA_SOUND_FLAG_STREAM` tells miniaudio to read
	///
	/// \details
	///  the file from disk as it plays (don't load the

	///  whole MP3 into RAM). For typical music

	///  files (3-10 MB) this is a small saving, but

	///  it's also the right semantic for a "playlist

	///  that can change every 5 seconds" — pre-loading

	///  the file would mean re-loading it every time

	///  the operator drops a new file in.

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
	/// \brief v1 default:
	///
	/// \details
	/// loop forever. The user explicitly
	///  asked for loop=true.

	ma_sound_set_looping(&m_sound, MA_TRUE);
	/// \brief Apply the current music-group volume to the
	///
	/// \details
	///  new sound. The group volume was already set

	///  at init; this is belt-and-suspenders in case

	///  the operator changes it after a reload.

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
		/// \brief Apply to the sound itself too, so a future
		///
		/// \details
		///  "no group" path (e.g. music folder is

		///  empty, only the engine is alive) would

		///  still reflect the volume in the HUD.

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
		/// \brief If the playlist is empty or the sound isn't
		///
		/// \details
		///  loaded, try to (re)load the current index.

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
			/// \brief Non-fatal:
			///
			/// \details
			/// the cursor stays where it
			///  was, which is the pre-fix behavior

			///  (same as if the operator pressed Q

			///  to resume instead of E). Logged for

			///  debuggability. MA_NOT_IMPLEMENTED

			///  is the common case for some

			///  streaming protocols; we don't log

			///  that as a failure.

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
		/// \brief No tracks to switch to.
		///
		/// \details
		/// Caller
		///  (nextTrack / previousTrack) is a no-op

		///  in this case. Don't touch

		///  `m_currentIndex` so the next 5-sec

		///  refresh that finds tracks picks up

		///  at the right position.

		return false;
	}
	/// \brief Wrap clamp:
	///
	/// \details
	/// handles the wrap-around case
	///  where `nextTrack` passed `m_playlist.size()`

	///  or `previousTrack` passed `(size_t)-1`.

	newIndex = newIndex % m_playlist.size();
	m_currentIndex = newIndex;
	m_currentTrackName = m_playlist[m_currentIndex].filename().string();
	m_pausedCursorMs = 0;

	/// \brief Behavior depends on the current state —
	///
	/// \details
	///  see the per-state block in the hpp

	///  comment. Common to all: tear down the

	///  currently loaded sound first (no-op when

	///  `m_soundLoaded == false`, e.g. when

	///  `m_state == Stopped` and we never started

	///  playback).

	unloadCurrentTrack();

	switch (m_state) {
	case MusicState::Playing:
		/// \brief Interrupt:
		///
		/// \details
		/// load the new track and start
		///  it from 0. The user pressed Next/Prev

		///  mid-playback; they expect the new

		///  track to start, not the old one to

		///  keep playing.

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
		/// \brief Replace the loaded sound so the next
		///
		/// \details
		///  `togglePlayPause` call plays the NEW

		///  track. State stays Paused so the HUD

		///  line still says PAUSE.

		if (!loadCurrentTrack()) {
			m_state = MusicState::Stopped;
			return false;
		}
		return true;

	case MusicState::Stopped:
		/// \brief No sound to unload-and-reload; the
		///
		/// \details
		///  index update is enough. The next

		///  `togglePlayPause` will `loadCurrentTrack()`

		///  at the new index and start.

		return true;
	}
	return false;
}

void AudioEngine::nextTrack()
{
	if (m_playlist.empty()) {
		return;
	}
	/// \brief Wrap forward:
	///
	/// \details
	/// index `playlist.size() - 1` →
	///  0, index `playlist.size() - 2` → `playlist.size() - 1`,

	///  etc. The `+ playlist.size()` before the

	///  modulo keeps the arithmetic unsigned-safe

	///  (otherwise `(0u + 1u) % 1u == 0` which is

	///  fine but `(0u - 1u) % 1u` would underflow).

	const size_t next = (m_currentIndex + 1u) % m_playlist.size();
	goToTrack(next);
}

void AudioEngine::previousTrack()
{
	if (m_playlist.empty()) {
		return;
	}
	/// \brief Wrap backward:
	///
	/// \details
	/// index 0 → `playlist.size() - 1`,
	///  index 1 → 0, etc. The `+ playlist.size()` keeps

	///  the subtraction unsigned-safe (otherwise

	///  `(0u - 1u)` would underflow to `UINT_MAX`).

	const size_t prev = (m_currentIndex + m_playlist.size() - 1u) % m_playlist.size();
	goToTrack(prev);
}

void AudioEngine::tick()
{
	if (!m_engineInitialized) {
		return;
	}

	/// \brief **5-second playlist refresh.** Cheap directory
	///
	/// \details
	///  walk; the disk cache absorbs the cost. Even

	///  on a slow spinning rust the scan completes

	///  well under a millisecond for a folder of a

	///  few hundred tracks.

	const auto now = std::chrono::steady_clock::now();
	if (now - m_lastPlaylistRefresh >= std::chrono::seconds(5)) {
		const size_t prevSize = m_playlist.size();
		const std::filesystem::path prevTrack =
			m_currentIndex < m_playlist.size() ? m_playlist[m_currentIndex] : std::filesystem::path{};
		scanPlaylist();
		/// \brief If the current track is still in the
		///
		/// \details
		///  playlist, keep playing it (the index

		///  may have shifted if files were added

		///  before it; the operator expects "play

		///  the same file" continuity). If it's

		///  gone, gracefully uninit — the operator

		///  can press Q to start a new track.

		if (m_soundLoaded && !prevTrack.empty() && !m_playlist.empty()) {
			const auto it = std::ranges::find(m_playlist, prevTrack);
			if (it == m_playlist.end()) {
				/// \brief Current track removed.
				///
				/// \details
				/// Pause
				///  semantics differ from stop

				///  (preserve the user's intent):

				///  if they were Playing, treat as

				///  Stopped (so next play starts

				///  fresh). If Paused, keep Paused

				///  state but uninit the sound so

				///  the next play will reload.

				if (m_state == MusicState::Playing) {
					stop();
				} else {
					unloadCurrentTrack();
				}
			} else {
				/// \brief Track still present.
				///
				/// \details
				/// Update
				///  the index to its new position

				///  (so the cached track name

				///  matches the loaded sound).

				m_currentIndex = static_cast<size_t>(std::distance(m_playlist.begin(), it));
			}
		}
		/// \brief First-time discovery:
		///
		/// \details
		/// if the playlist grew
		///  from empty to non-empty and the operator

		///  has never pressed Q, do nothing — they

		///  still need to start playback. (The

		///  "started playing" log will surface on the

		///  next Q press.)

		(void)prevSize;
	}
}

void AudioEngine::updateCurrentTrackMetadata()
{
	/// \brief Re-parse the cached name into the HUD-visible
	///
	/// \details
	///  artist / title pair. The helper writes

	///  `"-" / "<stem>"` when there is no ` - `

	///  separator, so the HUD always has a

	///  non-empty title to show (the artist may be

	///  `"-"`).

	ParseArtistTitle(m_currentTrackName, m_currentArtist, m_currentTitle);
}

float AudioEngine::positionSeconds() const
{
	/// \brief Guard:
	///
	/// \details
	/// `ma_sound_get_cursor_in_seconds`
	///  reads from `m_sound`, which is uninitialized

	///  when `m_soundLoaded == false`. The contract

	///  for the HUD is "0.0 when no sound is

	///  loaded"; that's also what miniaudio would

	///  return for a not-yet-started sound.

	if (!m_soundLoaded) {
		return 0.0f;
	}
	float cursorSeconds = 0.0f;
	const ma_result result = ma_sound_get_cursor_in_seconds(&m_sound, &cursorSeconds);
	if (result != MA_SUCCESS) {
		/// \brief Decoder is in a bad state (e.g.
		///
		/// \details
		/// malformed
		///  stream). The HUD will render this as

		///  `POS 0:00 / mm:ss`; not a hard error.

		return 0.0f;
	}
	return cursorSeconds;
}

float AudioEngine::durationSeconds() const
{
	/// \brief Same guard as `positionSeconds`.
	///
	/// \details
	/// The MP3
	///  decoder typically exposes the full stream

	///  length for `MA_SOUND_FLAG_STREAM` sources

	///  (it reads the file's frame count on the

	///  first `ma_sound_init_from_file`), so a

	///  failure here usually means a malformed

	///  header; the HUD will fall back to

	///  `POS 1:42 / --:--`.

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
