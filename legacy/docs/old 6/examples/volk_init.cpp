// Пример: инициализация volk для ProjectV
// Документация: docs/volk/quickstart.md
//
// Два варианта использования:
// 1. Минимальная инициализация (базовый пример)
// 2. Полная интеграция с SDL3 для ProjectV (реальный сценарий)

#define VK_NO_PROTOTYPES
#include "volk.h"
#include <cstdio>

// Базовый пример: минимальная инициализация volk
void basicVolkInit()
{
	printf("=== Базовая инициализация volk ===\n");

	VkResult result = volkInitialize();
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Ошибка: Vulkan loader не найден. Установите Vulkan SDK или драйвер с Vulkan.\n");
		return;
	}

	uint32_t version = volkGetInstanceVersion();
	if (version != 0) {
		printf("Vulkan loader обнаружен: %u.%u.%u\n", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version),
			   VK_VERSION_PATCH(version));
	} else {
		printf("Vulkan loader обнаружен, но версия неизвестна\n");
	}

	printf("Инициализация volk успешна! Готово к созданию VkInstance.\n");
}

// Расширенный пример: интеграция с SDL3 для ProjectV
// Демонстрирует реальное использование в воксельном движке
#ifdef HAS_SDL3
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

void advancedVolkSDLIntegration()
{
	printf("=== Расширенная интеграция volk + SDL3 для ProjectV ===\n");

	// 1. Инициализация SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "Ошибка SDL_Init: %s\n", SDL_GetError());
		return;
	}

	// 2. Создание окна с поддержкой Vulkan
	SDL_Window *window = SDL_CreateWindow("ProjectV: Vulkan + volk + SDL3", 1280, 720, SDL_WINDOW_VULKAN);
	if (!window) {
		fprintf(stderr, "Ошибка создания окна SDL: %s\n", SDL_GetError());
		SDL_Quit();
		return;
	}

	// 3. Инициализация volk
	if (volkInitialize() != VK_SUCCESS) {
		fprintf(stderr, "Ошибка: volkInitialize не удалась\n");
		SDL_DestroyWindow(window);
		SDL_Quit();
		return;
	}

	printf("volk инициализирован успешно\n");

	// 4. Получение расширений Vulkan через SDL
	unsigned int extensionCount = 0;
	const char **extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

	printf("Получено расширений Vulkan через SDL: %u\n", extensionCount);
	for (unsigned int i = 0; i < extensionCount; i++) {
		printf("  %u: %s\n", i + 1, extensions[i]);
	}

	// 5. Создание VkInstance (упрощённый пример)
	// В реальном ProjectV здесь создаётся полный VkInstanceCreateInfo
	printf("Готово к созданию VkInstance с volk...\n");

	// 6. В реальном коде здесь:
	//    - Создание VkInstance
	//    - volkLoadInstance(instance)
	//    - Создание поверхности через SDL_Vulkan_CreateSurface
	//    - Создание VkDevice и volkLoadDevice(device)
	//    - Интеграция с VMA, Tracy и другими библиотеками ProjectV

	printf("Пример интеграции volk + SDL3 завершён успешно!\n");
	printf("Для полной реализации см. примеры:\n");
	printf("  - example_vulkan_triangle.cpp\n");
	printf("  - example_black_window.cpp\n");

	// Очистка
	SDL_DestroyWindow(window);
	SDL_Quit();
}
#endif

int main(int argc, char *argv[])
{
	printf("ProjectV: Пример инициализации volk\n");
	printf("====================================\n");

	// Всегда выполняем базовую инициализацию
	basicVolkInit();

	// Проверяем аргументы для расширенного примера
	bool runAdvanced = false;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--sdl") == 0 || strcmp(argv[i], "-s") == 0) {
			runAdvanced = true;
			break;
		}
	}

#ifdef HAS_SDL3
	if (runAdvanced) {
		advancedVolkSDLIntegration();
	} else {
		printf("\nДля запуска расширенного примера с SDL3 выполните:\n");
		printf("  %s --sdl\n", argv[0]);
		printf("\nУбедитесь, что SDL3 доступна в системе и определён HAS_SDL3.\n");
	}
#else
	printf("\nРасширенный пример с SDL3 недоступен (HAS_SDL3 не определён).\n");
	printf("Для сборки с SDL3 добавьте в CMake: target_link_libraries(example_volk_init PRIVATE SDL3::SDL3)\n");
	printf("и определите HAS_SDL3 через target_compile_definitions.\n");
#endif

	printf("\nДополнительная документация:\n");
	printf("- [Быстрый старт](../docs/volk/quickstart.md)\n");
	printf("- [Архитектура мета-загрузчика](../docs/volk/concepts.md)\n");
	printf("- [10 практических сценариев](../docs/volk/use-cases.md)\n");
	printf("- [Бенчмарки производительности](../docs/volk/performance.md)\n");

	return 0;
}
