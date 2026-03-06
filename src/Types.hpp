#ifndef TYPES_HPP
#define TYPES_HPP

// ------ include--блок ------
#define SDL_MAIN_USE_CALLBACKS 1 // А надо ли?
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "volk.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "vma/vk_mem_alloc.h" // Хэдеры VMA
#pragma clang diagnostic pop

// STL блок (стандартные библиотеки)
#include <memory>
#include <vector>
// --- Конец include-блока ---

constexpr int MAX_FRAMES_IN_FLIGHT = 2; // Оптимум для двойной буферизации

// AppState – это структура данных состояния приложения (State Object). В Vulkan нет "глобального контекста" (как в OpenGL, где всё хранится внутри драйвера), поэтому разработчик обязан хранить все объекты Vulkan и ресурсы вручную. Почему это сделано в виде struct? Чистота кода: вы передаете одну переменную (AppState *state) в функции, вместо того чтобы передавать 20 отдельных аргументов. Пожизненное владение: если вы захотите создать второе окно или вторую графическую сцену, вы просто создадите второй экземпляр этой структуры. Безопасность: вы можете легко реализовать функцию очистки (cleanupAppState), которая последовательно уничтожит все ресурсы, указанные в этой структуре, в правильном порядке. Главный принцип Vulkan: всё, что вы создали (vkCreate...), должно быть уничтожено (vkDestroy...) в строго обратном порядке. Эта структура помогает не забыть ни один ресурс.
struct AppState {
	// 1. Управление окном и системным состоянием
	SDL_Window *window = nullptr; // Ссылка на окно ОС. Нужна, чтобы в любой момент можно было изменить размер, заголовок или закрыть его
	bool windowResized = false;	  // Флаг-сигнализатор. Когда пользователь тянет за край окна, SDL шлёт событие. Мы меняем этот флаг на true, чтобы в следующей итерации SDL_AppIterate понять: "Опа, размер изменился, нужно пересоздать Swapchain (цепочку образов)"

	// 2. Ядро Vulkan (Инфраструктура). Эти объекты — основа Vulkan. Без них ничего не работает
	VkInstance instance = VK_NULL_HANDLE;					  // Точка входа в библиотеку Vulkan
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE; // Поле для дебага с помощью Validation Layers
	VkSurfaceKHR surface = VK_NULL_HANDLE;					  // Связующее звено между Vulkan и окном (SDL)
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;		  // Физическая видеокарта
	VkDevice device = VK_NULL_HANDLE;						  // и её логическое представление в программе
	VkQueue queue = VK_NULL_HANDLE;							  // Очередь команд. Видеокарта работает асинхронно, мы «кидаем» ей приказы в эту очередь
	uint32_t queueFamilyIndex = 0;							  // индекс семейства очередей на видеокарте, который говорит Vulkan: “вот из какого типа очередей я хочу создавать queue, commandPool и с какими очередями будут совместимы мои command buffer’ы”
	VmaAllocator allocator = VK_NULL_HANDLE;				  // Вспомогательный инструмент для управления памятью видеокарты (аллокатор)

	// 3. Цепь изображений
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;								 // Сама цепочка (обычно 2 или 3 изображения, которые мы «крутим» перед пользователем)
	VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;							 // Информация о формате пикселей (например, RGBA)
	VkColorSpaceKHR swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // Цветовое пространство sRGB
	VkExtent2D extent = {};													 // Текущее разрешение окна в пикселях
	std::vector<VkImage> swapchainImages;									 // Массивы изображений, куда мы рисуем
	std::vector<VkImageView> swapchainImageViews;							 // и их «виды» (view), через которые мы получаем доступ к пикселям

	// 4. Отрисовка
	VkCommandPool commandPool = VK_NULL_HANDLE; // "Бассейн" для хранения команд отрисовки

	// 5. Синхронизация (Самое важное для CPU/GPU). Поскольку CPU и GPU работают параллельно, нам нужны «светофоры» и «заборы», чтобы они не столкнулись
	uint32_t currentFrame = 0; // Добавляем индекс текущего кадра

	// Массивы вместо одиночных объектов
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores; // Говорит GPU: "подожди, пока я получу новый кадр из Swapchain"
	std::vector<VkSemaphore> renderFinishedSemaphores; // Говорит GPU: "подожди, пока я закончу рисовать, прежде чем показывать кадр на мониторе"
	std::vector<VkFence> inFlightFences;			   // "Забор", который блокирует CPU, пока GPU не закончит отрисовку текущего кадра полностью. Это предотвращает отправку новой команды, пока не выполнена старая

	~AppState();
};

#endif
