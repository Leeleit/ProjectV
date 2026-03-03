// Пример: fastgltf — работа с материалами, PBR и расширениями glTF
// Документация: docs/fastgltf/concepts.md (раздел Materials & PBR)
//
// Уровень сложности: 🟡 (средний)

#include <cstdio>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <unordered_map>
#include <vector>

// Структура для хранения информации о материале
struct MaterialInfo {
	std::string name;

	// PBR металичность-шероховавость
	struct PBRInfo {
		fastgltf::math::fvec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		int32_t baseColorTexture = -1;
		int32_t metallicRoughnessTexture = -1;
		int32_t normalTexture = -1;
		int32_t occlusionTexture = -1;
		int32_t emissiveTexture = -1;
		fastgltf::math::fvec3 emissiveFactor = {0.0f, 0.0f, 0.0f};
	} pbr;

	// Расширения
	bool hasClearcoat = false;
	bool hasTransmission = false;
	bool hasSheen = false;
	bool hasSpecular = false;
};

// Вспомогательная функция для проверки наличия расширения
bool hasExtension(const fastgltf::Asset &asset, const std::string &extensionName)
{
	return std::find(asset.extensionsUsed.begin(), asset.extensionsUsed.end(), extensionName) !=
		   asset.extensionsUsed.end();
}

void analyzeMaterials(const fastgltf::Asset *asset)
{
	if (!asset || asset->materials.empty()) {
		std::printf("В файле нет материалов (используются материалы по умолчанию)\n");
		return;
	}

	std::vector<MaterialInfo> materials;
	materials.reserve(asset->materials.size());

	// 1. Анализ расширений, используемых в файле
	std::printf("Используемые расширения в файле:\n");
	for (const auto &ext : asset->extensionsUsed) {
		std::printf("  • %s\n", ext.c_str());
	}
	std::printf("\n");

	// 2. Обработка каждого материала
	for (const auto &gltfMaterial : asset->materials) {
		MaterialInfo material;

		// Имя материала
		if (gltfMaterial.name) {
			material.name = *gltfMaterial.name;
		} else {
			material.name = "material_" + std::to_string(materials.size());
		}

		// 3. PBR металичность-шероховавость
		const auto &pbr = gltfMaterial.pbrData;
		material.pbr.baseColorFactor = pbr.baseColorFactor;
		material.pbr.metallicFactor = pbr.metallicFactor;
		material.pbr.roughnessFactor = pbr.roughnessFactor;
		material.pbr.emissiveFactor = gltfMaterial.emissiveFactor;

		// Текстуры
		if (pbr.baseColorTexture) {
			material.pbr.baseColorTexture = static_cast<int32_t>(pbr.baseColorTexture->textureIndex);
		}
		if (pbr.metallicRoughnessTexture) {
			material.pbr.metallicRoughnessTexture = static_cast<int32_t>(pbr.metallicRoughnessTexture->textureIndex);
		}
		if (gltfMaterial.normalTexture) {
			material.pbr.normalTexture = static_cast<int32_t>(gltfMaterial.normalTexture->textureIndex);
		}
		if (gltfMaterial.occlusionTexture) {
			material.pbr.occlusionTexture = static_cast<int32_t>(gltfMaterial.occlusionTexture->textureIndex);
		}
		if (gltfMaterial.emissiveTexture) {
			material.pbr.emissiveTexture = static_cast<int32_t>(gltfMaterial.emissiveTexture->textureIndex);
		}

		// 4. Проверка расширений материала
		material.hasClearcoat = gltfMaterial.clearcoat.has_value();
		material.hasTransmission = gltfMaterial.transmission.has_value();
		material.hasSheen = gltfMaterial.sheen.has_value();
		material.hasSpecular = gltfMaterial.specular.has_value();

		materials.push_back(std::move(material));
	}

	// 5. Вывод информации о материалах
	std::printf("Загружено материалов: %zu\n\n", materials.size());

	for (const auto &material : materials) {
		std::printf("Материал: %s\n", material.name.c_str());
		std::printf("  BaseColor: (%.2f, %.2f, %.2f, %.2f)\n", material.pbr.baseColorFactor[0],
					material.pbr.baseColorFactor[1], material.pbr.baseColorFactor[2], material.pbr.baseColorFactor[3]);
		std::printf("  Metallic: %.2f, Roughness: %.2f\n", material.pbr.metallicFactor, material.pbr.roughnessFactor);

		if (material.pbr.baseColorTexture >= 0) {
			std::printf("  BaseColor Texture: %d\n", material.pbr.baseColorTexture);
		}
		if (material.pbr.metallicRoughnessTexture >= 0) {
			std::printf("  MetallicRoughness Texture: %d\n", material.pbr.metallicRoughnessTexture);
		}
		if (material.pbr.normalTexture >= 0) {
			std::printf("  Normal Texture: %d\n", material.pbr.normalTexture);
		}

		// Расширения
		if (material.hasClearcoat)
			std::printf("  [Расширение] Clearcoat\n");
		if (material.hasTransmission)
			std::printf("  [Расширение] Transmission\n");
		if (material.hasSheen)
			std::printf("  [Расширение] Sheen\n");
		if (material.hasSpecular)
			std::printf("  [Расширение] Specular\n");

		std::printf("\n");
	}

	// 6. Статистика по расширениям
	int clearcoatCount = 0, transmissionCount = 0, sheenCount = 0, specularCount = 0;
	for (const auto &material : materials) {
		if (material.hasClearcoat)
			clearcoatCount++;
		if (material.hasTransmission)
			transmissionCount++;
		if (material.hasSheen)
			sheenCount++;
		if (material.hasSpecular)
			specularCount++;
	}

	std::printf("Статистика расширений:\n");
	if (clearcoatCount > 0)
		std::printf("  Clearcoat: %d материалов\n", clearcoatCount);
	if (transmissionCount > 0)
		std::printf("  Transmission: %d материалов\n", transmissionCount);
	if (sheenCount > 0)
		std::printf("  Sheen: %d материалов\n", sheenCount);
	if (specularCount > 0)
		std::printf("  Specular: %d материалов\n", specularCount);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::fprintf(stderr, "Использование: %s <path/to/model.gltf|glb>\n", argv[0]);
		std::fprintf(stderr, "\nРекомендуемые модели для демонстрации материалов:\n");
		std::fprintf(stderr, "  • models/MetalRoughSpheres.gltf (PBR материалы)\n");
		std::fprintf(stderr, "  • models/SpecGlossVsMetalRough.gltf (два материала)\n");
		std::fprintf(stderr, "  • models/FlightHelmet.glb (сложные материалы)\n");
		return 1;
	}

	std::filesystem::path gltfPath = argv[1];

	// Загрузка данных glTF
	auto data = fastgltf::GltfDataBuffer::FromPath(gltfPath);
	if (data.error() != fastgltf::Error::None) {
		std::fprintf(stderr, "Ошибка загрузки файла: %s\n", gltfPath.string().c_str());
		return 1;
	}

	// Парсинг glTF с поддержкой расширений
	fastgltf::Parser parser;

	// Настройка парсера для поддержки расширений
	fastgltf::Extensions extensions;
	extensions.KHR_materials_clearcoat = true;
	extensions.KHR_materials_transmission = true;
	extensions.KHR_materials_sheen = true;
	extensions.KHR_materials_specular = true;

	auto asset =
		parser.loadGltf(data.get(), gltfPath.parent_path(), fastgltf::Options::LoadExternalBuffers, extensions);

	if (asset.error() != fastgltf::Error::None) {
		std::fprintf(stderr, "Ошибка парсинга glTF: %s\n", fastgltf::to_underlying(asset.error()));
		return 1;
	}

	// Анализ материалов
	analyzeMaterials(asset.get_ptr());

	return 0;
}
