// Пример: fastgltf — работа с анимациями glTF
// Документация: docs/fastgltf/concepts.md (раздел Animations & Skinning)
//
// Уровень сложности: 🟡 (средний)

#include <chrono>
#include <cstdio>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <vector>

// Структура для хранения информации об анимации
struct AnimationInfo {
	std::string name;
	size_t channelCount = 0;
	size_t samplerCount = 0;
	float duration = 0.0f; // максимальное время анимации в секундах
};

// Вспомогательная функция для извлечения времени анимации
float getAnimationDuration(const fastgltf::Asset *asset, const fastgltf::Animation &animation)
{
	float maxTime = 0.0f;

	for (const auto &sampler : animation.samplers) {
		if (sampler.inputAccessor < asset->accessors.size()) {
			const auto &inputAccessor = asset->accessors[sampler.inputAccessor];

			// Получаем максимальное значение времени из accessor'а
			if (inputAccessor.type == fastgltf::AccessorType::Scalar) {
				// Читаем значения времени
				std::vector<float> times(inputAccessor.count);
				fastgltf::copyFromAccessor<float>(asset, inputAccessor, times.data());

				// Находим максимальное время
				for (float time : times) {
					if (time > maxTime) {
						maxTime = time;
					}
				}
			}
		}
	}

	return maxTime;
}

// Функция для интерполяции значений анимации
template <typename T>
T interpolateAnimation(const fastgltf::AnimationSampler &sampler, const std::vector<float> &times,
					   const std::vector<T> &values, float currentTime, size_t componentCount)
{

	// Находим ключевые кадры между которыми находится currentTime
	size_t prevIndex = 0;
	size_t nextIndex = 0;

	for (size_t i = 0; i < times.size(); ++i) {
		if (times[i] <= currentTime) {
			prevIndex = i;
		}
		if (times[i] >= currentTime) {
			nextIndex = i;
			break;
		}
	}

	// Если достигли конца анимации, возвращаем последнее значение
	if (nextIndex == 0 || prevIndex == nextIndex) {
		return values[prevIndex * componentCount];
	}

	// Линейная интерполяция
	float t1 = times[prevIndex];
	float t2 = times[nextIndex];
	float factor = (currentTime - t1) / (t2 - t1);

	T result;
	for (size_t i = 0; i < componentCount; ++i) {
		float v1 = values[prevIndex * componentCount + i];
		float v2 = values[nextIndex * componentCount + i];
		result[i] = v1 + factor * (v2 - v1);
	}

	return result;
}

void analyzeAnimations(const fastgltf::Asset *asset)
{
	if (!asset || asset->animations.empty()) {
		std::printf("В файле нет анимаций\n");
		return;
	}

	std::vector<AnimationInfo> animations;
	animations.reserve(asset->animations.size());

	// 1. Анализ каждой анимации
	for (const auto &gltfAnimation : asset->animations) {
		AnimationInfo info;

		// Имя анимации
		if (gltfAnimation.name) {
			info.name = *gltfAnimation.name;
		} else {
			info.name = "animation_" + std::to_string(animations.size());
		}

		info.channelCount = gltfAnimation.channels.size();
		info.samplerCount = gltfAnimation.samplers.size();
		info.duration = getAnimationDuration(asset, gltfAnimation);

		animations.push_back(std::move(info));
	}

	// 2. Вывод общей информации
	std::printf("Загружено анимаций: %zu\n\n", animations.size());

	for (const auto &anim : animations) {
		std::printf("Анимация: %s\n", anim.name.c_str());
		std::printf("  Каналов: %zu\n", anim.channelCount);
		std::printf("  Сэмплеров: %zu\n", anim.samplerCount);
		std::printf("  Длительность: %.3f сек\n", anim.duration);
		std::printf("\n");
	}

	// 3. Детальный анализ первой анимации (для демонстрации)
	if (!asset->animations.empty()) {
		const auto &firstAnim = asset->animations[0];

		std::printf("Детальный анализ первой анимации '%s':\n", animations[0].name.c_str());

		for (size_t i = 0; i < firstAnim.channels.size(); ++i) {
			const auto &channel = firstAnim.channels[i];
			const auto &sampler = firstAnim.samplers[channel.samplerIndex];

			std::printf("  Канал %zu:\n", i);

			// Индекс узла (если есть)
			if (channel.nodeIndex) {
				std::printf("    Узел: %zu\n", *channel.nodeIndex);
			} else {
				std::printf("    Узел: (не указан)\n");
			}

			// Тип анимации
			std::printf("    Путь анимации: ");
			switch (channel.path) {
			case fastgltf::AnimationPath::Translation:
				std::printf("Translation (позиция)\n");
				break;
			case fastgltf::AnimationPath::Rotation:
				std::printf("Rotation (вращение)\n");
				break;
			case fastgltf::AnimationPath::Scale:
				std::printf("Scale (масштаб)\n");
				break;
			case fastgltf::AnimationPath::Weights:
				std::printf("Weights (веса морфинга)\n");
				break;
			default:
				std::printf("Unknown\n");
				break;
			}

			// Тип интерполяции
			std::printf("    Интерполяция: ");
			switch (sampler.interpolation) {
			case fastgltf::AnimationInterpolation::Linear:
				std::printf("Linear (линейная)\n");
				break;
			case fastgltf::AnimationInterpolation::Step:
				std::printf("Step (ступенчатая)\n");
				break;
			case fastgltf::AnimationInterpolation::CubicSpline:
				std::printf("CubicSpline (кубический сплайн)\n");
				break;
			default:
				std::printf("Unknown\n");
				break;
			}

			// Информация о accessor'ах
			const auto &inputAccessor = asset->accessors[sampler.inputAccessor];
			const auto &outputAccessor = asset->accessors[sampler.outputAccessor];

			std::printf("    Временные ключи: accessor %zu (%zu значений)\n", sampler.inputAccessor,
						inputAccessor.count);
			std::printf("    Значения: accessor %zu (%zu значений)\n", sampler.outputAccessor, outputAccessor.count);

			std::printf("\n");
		}
	}

	// 4. Пример простого воспроизведения анимации
	if (!asset->animations.empty() && !asset->animations[0].channels.empty()) {
		std::printf("Пример воспроизведения анимации:\n");

		const auto &anim = asset->animations[0];
		const auto &firstChannel = anim.channels[0];
		const auto &sampler = anim.samplers[firstChannel.samplerIndex];

		// Загружаем временные ключи
		const auto &timeAccessor = asset->accessors[sampler.inputAccessor];
		std::vector<float> times(timeAccessor.count);
		fastgltf::copyFromAccessor<float>(asset, timeAccessor, times.data());

		// Загружаем значения
		const auto &valueAccessor = asset->accessors[sampler.outputAccessor];

		// Определяем тип значений на основе пути анимации
		switch (firstChannel.path) {
		case fastgltf::AnimationPath::Translation: {
			// Translation: vec3
			std::vector<fastgltf::math::fvec3> translations(valueAccessor.count / 3);
			fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset, valueAccessor, translations.data());

			std::printf("  Translation values loaded: %zu\n", translations.size());
			if (!translations.empty()) {
				std::printf("  First translation: (%.3f, %.3f, %.3f)\n", translations[0][0], translations[0][1],
							translations[0][2]);
			}
			break;
		}
		case fastgltf::AnimationPath::Rotation: {
			// Rotation: quat (vec4)
			std::vector<fastgltf::math::fvec4> rotations(valueAccessor.count / 4);
			fastgltf::copyFromAccessor<fastgltf::math::fvec4>(asset, valueAccessor, rotations.data());

			std::printf("  Rotation values loaded: %zu\n", rotations.size());
			if (!rotations.empty()) {
				std::printf("  First rotation: (%.3f, %.3f, %.3f, %.3f)\n", rotations[0][0], rotations[0][1],
							rotations[0][2], rotations[0][3]);
			}
			break;
		}
		case fastgltf::AnimationPath::Scale: {
			// Scale: vec3
			std::vector<fastgltf::math::fvec3> scales(valueAccessor.count / 3);
			fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset, valueAccessor, scales.data());

			std::printf("  Scale values loaded: %zu\n", scales.size());
			if (!scales.empty()) {
				std::printf("  First scale: (%.3f, %.3f, %.3f)\n", scales[0][0], scales[0][1], scales[0][2]);
			}
			break;
		}
		default:
			std::printf("  Unsupported animation path for demo\n");
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::fprintf(stderr, "Использование: %s <path/to/animated-model.gltf|glb>\n", argv[0]);
		std::fprintf(stderr, "\nРекомендуемые модели для демонстрации анимаций:\n");
		std::fprintf(stderr, "  • models/AnimatedMorphCube.gltf (морфинг анимация)\n");
		std::fprintf(stderr, "  • models/AnimatedTriangle.gltf (простая анимация)\n");
		std::fprintf(stderr, "  • models/RiggedSimple.gltf (скелетная анимация)\n");
		std::fprintf(stderr, "  • models/Fox.glb (комплексная анимация)\n");
		return 1;
	}

	std::filesystem::path gltfPath = argv[1];

	// Загрузка данных glTF
	auto data = fastgltf::GltfDataBuffer::FromPath(gltfPath);
	if (data.error() != fastgltf::Error::None) {
		std::fprintf(stderr, "Ошибка загрузки файла: %s\n", gltfPath.string().c_str());
		return 1;
	}

	// Парсинг glTF
	fastgltf::Parser parser;
	auto asset = parser.loadGltf(data.get(), gltfPath.parent_path(), fastgltf::Options::LoadExternalBuffers);

	if (asset.error() != fastgltf::Error::None) {
		std::fprintf(stderr, "Ошибка парсинга glTF: %s\n", fastgltf::to_underlying(asset.error()));
		return 1;
	}

	// Анализ анимаций
	analyzeAnimations(asset.get_ptr());

	return 0;
}
