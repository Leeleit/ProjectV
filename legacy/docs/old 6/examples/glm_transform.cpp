// Пример: glm — матрица трансформации (translate, rotate, scale)
// Документация: docs/glm/quickstart.md

#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main()
{
	glm::vec3 position(1.0f, 0.0f, 0.0f);

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, position);
	model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));

	glm::vec4 point(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 transformed = model * point;

	std::printf("Transformed: %.2f, %.2f, %.2f\n", transformed.x, transformed.y, transformed.z);

	return 0;
}
