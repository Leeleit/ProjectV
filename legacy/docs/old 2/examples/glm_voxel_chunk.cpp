// Пример: glm — оптимизированные трансформации для воксельного чанка
// Документация: docs/glm/quickstart.md
//
// Этот пример демонстрирует оптимизированные операции с glm для воксельного движка:
// 1. Трансформация чанка (матрица модели)
// 2. Массовые вычисления позиций вокселей
// 3. Использование кватернионов для камеры
// 4. Оптимизация памяти через SoA (Structure of Arrays)

#include <chrono>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

// Константы для воксельного чанка
constexpr int CHUNK_SIZE = 16; // 16×16×16 вокселей
constexpr int TOTAL_VOXELS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

// Структура массивов (SoA) для оптимизированного доступа к памяти
struct VoxelChunkSoA {
	std::vector<float> positions_x; // X компоненты позиций
	std::vector<float> positions_y; // Y компоненты позиций
	std::vector<float> positions_z; // Z компоненты позиций
	glm::mat4 chunk_transform;		// Трансформация всего чанка

	VoxelChunkSoA()
	{
		positions_x.reserve(TOTAL_VOXELS);
		positions_y.reserve(TOTAL_VOXELS);
		positions_z.reserve(TOTAL_VOXELS);
	}
};

// Структура массивов (AoS) для сравнения производительности
struct VoxelChunkAoS {
	std::vector<glm::vec3> positions; // Массив структур
	glm::mat4 chunk_transform;

	VoxelChunkAoS() { positions.reserve(TOTAL_VOXELS); }
};

// Генерация тестовых данных для чанка
void generate_chunk_data(VoxelChunkSoA &soa_data, VoxelChunkAoS &aos_data)
{
	printf("Генерация данных для чанка %d×%d×%d...\n", CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);

	for (int z = 0; z < CHUNK_SIZE; ++z) {
		for (int y = 0; y < CHUNK_SIZE; ++y) {
			for (int x = 0; x < CHUNK_SIZE; ++x) {
				// Локальные координаты вокселя в чанке
				float local_x = static_cast<float>(x);
				float local_y = static_cast<float>(y);
				float local_z = static_cast<float>(z);

				// SoA формат
				soa_data.positions_x.push_back(local_x);
				soa_data.positions_y.push_back(local_y);
				soa_data.positions_z.push_back(local_z);

				// AoS формат
				aos_data.positions.emplace_back(local_x, local_y, local_z);
			}
		}
	}

	// Устанавливаем трансформацию чанка (сдвиг в мире)
	soa_data.chunk_transform = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 5.0f, 10.0f));
	aos_data.chunk_transform = soa_data.chunk_transform;
}

// Трансформация вокселей с использованием SoA (оптимизированно)
void transform_voxels_soa(VoxelChunkSoA &data, const glm::mat4 &transform)
{
	for (size_t i = 0; i < data.positions_x.size(); ++i) {
		// Создаём вектор из отдельных компонент
		glm::vec4 pos(data.positions_x[i], data.positions_y[i], data.positions_z[i], 1.0f);

		// Применяем трансформацию
		glm::vec4 transformed = transform * pos;

		// Сохраняем обратно в SoA
		data.positions_x[i] = transformed.x;
		data.positions_y[i] = transformed.y;
		data.positions_z[i] = transformed.z;
	}
}

// Трансформация вокселей с использованием AoS (стандартно)
void transform_voxels_aos(VoxelChunkAoS &data, const glm::mat4 &transform)
{
	for (auto &pos : data.positions) {
		glm::vec4 pos_homogeneous(pos, 1.0f);
		glm::vec4 transformed = transform * pos_homogeneous;
		pos = glm::vec3(transformed);
	}
}

// Пример использования кватернионов для камеры
glm::mat4 create_camera_view(const glm::vec3 &camera_position, const glm::vec3 &camera_target, float yaw_degrees,
							 float pitch_degrees)
{
	// Создаём кватернион для поворота камеры
	glm::quat yaw_rotation =
		glm::angleAxis(glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f) // Вращение вокруг оси Y
		);

	glm::quat pitch_rotation =
		glm::angleAxis(glm::radians(pitch_degrees), glm::vec3(1.0f, 0.0f, 0.0f) // Вращение вокруг оси X
		);

	// Комбинируем вращения
	glm::quat camera_rotation = yaw_rotation * pitch_rotation;

	// Преобразуем кватернион в матрицу
	glm::mat4 rotation_matrix = glm::mat4_cast(camera_rotation);

	// Создаём матрицу вида камеры
	glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), -camera_position);

	return rotation_matrix * translation_matrix;
}

// Пример вычисления проекционной матрицы для Vulkan
glm::mat4 create_vulkan_projection(float fov_degrees, float aspect_ratio, float near_plane, float far_plane)
{
// Для Vulkan используем глубину [0, 1]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

	return glm::perspective(glm::radians(fov_degrees), aspect_ratio, near_plane, far_plane);
}

int main()
{
	printf("=== Пример glm для воксельного движка ===\n\n");

	// Инициализация данных
	VoxelChunkSoA chunk_soa;
	VoxelChunkAoS chunk_aos;
	generate_chunk_data(chunk_soa, chunk_aos);

	printf("Сгенерировано %zu вокселей\n", chunk_soa.positions_x.size());

	// Тестирование производительности SoA vs AoS
	printf("\n--- Тестирование производительности ---\n");

	auto start = std::chrono::high_resolution_clock::now();
	transform_voxels_soa(chunk_soa, chunk_soa.chunk_transform);
	auto end = std::chrono::high_resolution_clock::now();
	auto soa_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	start = std::chrono::high_resolution_clock::now();
	transform_voxels_aos(chunk_aos, chunk_aos.chunk_transform);
	end = std::chrono::high_resolution_clock::now();
	auto aos_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	printf("SoA трансформация: %lld мкс\n", soa_duration.count());
	printf("AoS трансформация: %lld мкс\n", aos_duration.count());
	printf("Ускорение SoA: %.2fx\n", static_cast<double>(aos_duration.count()) / soa_duration.count());

	// Пример камеры с кватернионами
	printf("\n--- Пример камеры с кватернионами ---\n");

	glm::vec3 camera_pos(0.0f, 10.0f, 20.0f);
	glm::vec3 camera_target(0.0f, 0.0f, 0.0f);
	float yaw = 45.0f;	  // Поворот вокруг Y
	float pitch = -15.0f; // Наклон вниз

	glm::mat4 view_matrix = create_camera_view(camera_pos, camera_target, yaw, pitch);
	glm::mat4 proj_matrix = create_vulkan_projection(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);

	printf("Матрица вида создана (размер: %zu байт)\n", sizeof(view_matrix));
	printf("Матрица проекции создана (размер: %zu байт)\n", sizeof(proj_matrix));

	// Пример использования value_ptr для передачи в буфер
	printf("\n--- Пример передачи данных в Vulkan ---\n");

	struct UniformBufferObject {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	} ubo;

	ubo.model = chunk_soa.chunk_transform;
	ubo.view = view_matrix;
	ubo.proj = proj_matrix;

	// Получаем указатели на данные для копирования в GPU
	const float *model_ptr = glm::value_ptr(ubo.model);
	const float *view_ptr = glm::value_ptr(ubo.view);
	const float *proj_ptr = glm::value_ptr(ubo.proj);

	printf("Указатель на матрицу модели: %p\n", static_cast<const void *>(model_ptr));
	printf("Указатель на матрицу вида: %p\n", static_cast<const void *>(view_ptr));
	printf("Указатель на матрицу проекции: %p\n", static_cast<const void *>(proj_ptr));
	printf("Размер UniformBufferObject: %zu байт\n", sizeof(ubo));

	// Демонстрация работы с отдельными векторами
	printf("\n--- Работа с векторами ---\n");

	glm::vec3 voxel_position(1.0f, 2.0f, 3.0f);
	glm::vec3 voxel_normal(0.0f, 1.0f, 0.0f);

	float dot_product = glm::dot(voxel_position, voxel_normal);
	glm::vec3 normalized = glm::normalize(voxel_position);

	printf("Исходный вектор: (%.2f, %.2f, %.2f)\n", voxel_position.x, voxel_position.y, voxel_position.z);
	printf("Скалярное произведение с нормалью: %.2f\n", dot_product);
	printf("Нормализованный вектор: (%.2f, %.2f, %.2f)\n", normalized.x, normalized.y, normalized.z);
	printf("Длина исходного вектора: %.2f\n", glm::length(voxel_position));

	printf("\n=== Пример завершён ===\n");
	printf("Рекомендации для воксельного движка:\n");
	printf("1. Используйте SoA для массовых операций\n");
	printf("2. Кэшируйте матрицы проекции и вида\n");
	printf("3. Используйте кватернионы для плавных поворотов камеры\n");
	printf("4. Используйте value_ptr для передачи данных в Vulkan\n");

	return 0;
}
