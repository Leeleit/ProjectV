// Пример: минимальное воспроизведение звука через ma_engine (High-level API)
// Документация: docs/miniaudio/quickstart.md
//
// Запуск: example_miniaudio_playback <audio_file.wav>

#include "miniaudio.h"
#include <cstdio>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::fprintf(stderr, "Usage: %s <audio_file.wav>\n", argv[0]);
		return 1;
	}

	ma_result result;
	ma_engine engine;

	result = ma_engine_init(nullptr, &engine);
	if (result != MA_SUCCESS) {
		std::fprintf(stderr, "ma_engine_init failed: %s\n", ma_result_description(result));
		return 1;
	}

	ma_engine_play_sound(&engine, argv[1], nullptr);
	std::printf("Playing %s. Press Enter to quit...\n", argv[1]);
	std::getchar();

	ma_engine_uninit(&engine);
	return 0;
}
