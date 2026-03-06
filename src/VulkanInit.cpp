// ------ include--блок ------
#include "VulkanInit.hpp"

// STL блок (стандартные библиотеки)
#include <algorithm> // std::clamp, std::max
#include <array>
#include <cstring> // std::strcmp
#include <memory>
#include <string>
#include <vector>
// --- Конец include-блока ---

inline constexpr char PROJECT_NAME[] = "ProjectV v0.0.1";

#ifndef NDEBUG
static constexpr bool kEnableValidation = true;
#else
static constexpr bool kEnableValidation = false;
#endif

static constexpr std::array<const char *, 1> kValidationLayers{"VK_LAYER_KHRONOS_validation"};

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
{
	SDL_Log("Vulkan validation: [%u][%u] %s",
			static_cast<unsigned>(messageSeverity),
			static_cast<unsigned>(messageTypes),
			pCallbackData && pCallbackData->pMessage ? pCallbackData->pMessage : "no message");
	return VK_FALSE;
}

static bool CheckValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> available(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, available.data());

	for (const char *requiredLayer : kValidationLayers) {
		bool found = false;
		for (const auto &layer : available) {
			if (std::strcmp(requiredLayer, layer.layerName) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			SDL_Log("Missing validation layer: %s", requiredLayer);
			return false;
		}
	}
	return true;
}

static VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = DebugCallback;
	return info;
}

static bool CreateDebugMessenger(AppState *state)
{
	// if (!kEnableValidation) {
	// 	return true;
	// }

	const VkDebugUtilsMessengerCreateInfoEXT info = MakeDebugMessengerCreateInfo();
	if (vkCreateDebugUtilsMessengerEXT(state->instance, &info, nullptr, &state->debugMessenger) != VK_SUCCESS) {
		SDL_Log("vkCreateDebugUtilsMessengerEXT failed");
		return false;
	}
	return true;
}

// --- Функции управления Swapchain ---

// Эта функция CleanupSwapchain — критически важная часть любого Vulkan-приложения. Она отвечает за безопасное уничтожение объектов, зависящих от Swapchain, перед тем как либо пересоздать их (например, после изменения размера окна), либо полностью закрыть приложение. Почему этот код нужен? В Vulkan объекты тесно связаны друг с другом «цепочкой зависимостей»: framebuffer использует ImageView. ImageView использует Image (которые принадлежат Swapchain). Swapchain — это сама цепочка образов. Если вы попытаетесь просто уничтожить Swapchain, не удалив предварительно Framebuffer и ImageView, Vulkan выдаст ошибку (Validation Layers будут ругаться), потому что объекты всё ещё «ссылаются» на ресурсы, которые вы пытаетесь удалить. Почему это сделано в отдельной функции? Вы могли заметить, что этот код вызывается в двух ситуациях: при изменении размера окна (RecreateSwapchain): когда пользователь растягивает окно, старые изображения становятся неактуальными (они физически больше не соответствуют размеру окна). Мы вызываем CleanupSwapchain, а затем createSwapchain. При закрытии приложения: в cleanupAppState мы очищаем всё до конца. Главный урок этого кода: в Vulkan порядок имеет значение. Всегда уничтожайте «верхнеуровневые» объекты (Framebuffers) перед «низкоуровневыми» (Swapchain). Если вы попытаетесь удалить Swapchain раньше, чем Framebuffer, вы нарушите правила игры Vulkan, и драйвер может завершить работу приложения с ошибкой.
void CleanupSwapchain(AppState *state)
{
	if (!state->device)
		return; // Это страховка. Если функция вызвана на этапе, когда инициализация устройства (GPU) ещё не завершилась или уже была очищена, мы ничего не делаем. Это предотвращает попытку обращения к nullptr (краш)

	// Здесь мы делаем то же самое для ImageView. Важно: VkImage (сами текстуры) удалять не нужно. Они принадлежат Swapchain'у, и когда вы удалите сам Swapchain, видеодрайвер сам корректно освободит память, занятую изображениями
	for (const VkImageView iv : state->swapchainImageViews) {
		if (iv)
			vkDestroyImageView(state->device, iv, nullptr);
	}
	state->swapchainImageViews.clear();
	state->swapchainImages.clear();

	// Это финальный аккорд. Мы удаляем сам объект Swapchain. Установка state->swapchain = VK_NULL_HANDLE нужна для того, чтобы случайно не попытаться использовать этот указатель (хэндл) снова в других частях программы — если это произойдет, программа просто упадет с понятной ошибкой, а не попытается работать с «битой» памятью
	if (state->swapchain) {
		vkDestroySwapchainKHR(state->device, state->swapchain, nullptr);
		state->swapchain = VK_NULL_HANDLE;
	}
	state->extent = {};
}

// Эта функция — «сердце» графического конвейера Vulkan. Именно здесь создаются поверхности для рисования, которые мы видим на мониторе. В Vulkan нельзя рисовать прямо «в окно» — нужно создавать Swapchain (цепочку образов), куда GPU будет записывать готовые кадры
bool createSwapchain(AppState *state)
{
	VkSurfaceCapabilitiesKHR caps; // Мы запрашиваем у видеодрайвера, что он вообще умеет делать с нашим окном (например, минимальный/максимальный размер, поддерживаемые трансформации — поворот экрана, отражение и т.д.)
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physicalDevice, state->surface, &caps);

	if (caps.currentExtent.width != 0xFFFFFFFF) { // Если width не равен 0xFFFFFFFF, значит, оконный менеджер (ОС) жестко диктует нам размер. Мы обязаны его принять
		state->extent = caps.currentExtent;
	} else { // Если равно 0xFFFFFFFF (спецзначение Vulkan), значит, мы свободны выбрать размер окна в рамках ограничений caps. Мы получаем текущий размер окна из SDL (SDL_GetWindowSizeInPixels) и «зажимаем» его (std::clamp) в границы, которые разрешает видеокарта (minImageExtent — maxImageExtent)
		int w = 1280, h = 720;
		SDL_GetWindowSizeInPixels(state->window, &w, &h);
		state->extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
		state->extent.width = std::clamp(state->extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
		state->extent.height = std::clamp(state->extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	}

	// Если окно свернуто, его размер равен (0,0). Vulkan запрещает создавать Swapchain с нулевым размером. Мы просто выходим, ничего не делая, пока окно не станет видимым снова
	if (state->extent.width == 0 || state->extent.height == 0) {
		return true;
	}

	// Мы спрашиваем видеокарту: «Какие режимы вывода кадров ты поддерживаешь?». Пытаемся найти MAILBOX (лучший, низкая задержка), но если его нет — всегда будет FIFO (базовый VSync), который поддерживается всегда по спецификации
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(state->physicalDevice, state->surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(state->physicalDevice, state->surface, &presentModeCount, presentModes.data());
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const VkPresentModeKHR pm : presentModes) {
		if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = pm;
			break;
		}
	}

	// Swapchain — это конвейер. Мы хотим минимум 2 кадра (один рисуем, второй выводим). Но мы не можем запросить больше, чем позволяет драйвер (maxImageCount)
	uint32_t imageCount = std::max(2u, caps.minImageCount);
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
		imageCount = caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainInfo;
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = state->surface;							  // К какому окну привязываем
	swapchainInfo.minImageCount = imageCount;						  // Сколько картинок в конвейере
	swapchainInfo.imageFormat = state->swapchainFormat;				  // Формат цвета
	swapchainInfo.imageColorSpace = state->swapchainColorSpace;		  // Пространство цвета (sRGB)
	swapchainInfo.imageExtent = state->extent;						  // Размеры
	swapchainInfo.imageArrayLayers = 1;								  // Только 1 слой (не стерео)
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	  // Будем рисовать на них
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;		  // Изображения принадлежат только этой очереди
	swapchainInfo.preTransform = caps.currentTransform;				  // Поворот экрана (если мобилка)
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Игнорировать альфа-канал окна
	swapchainInfo.presentMode = presentMode;						  // Режим презентации (Mailbox или FIFO)
	swapchainInfo.clipped = VK_TRUE;								  // Не считай пиксели, которые не видны (за другими окнами)

	// Здесь мы отправляем данные в драйвер. Если он скажет «не могу», вернем false
	if (vkCreateSwapchainKHR(state->device, &swapchainInfo, nullptr, &state->swapchain) != VK_SUCCESS) {
		SDL_Log("vkCreateSwapchainKHR failed");
		return false;
	}

	// Мы создали Swapchain, но не знаем, сколько именно изображений он нам выделил (мы просили minImageCount, но драйвер мог дать больше). Мы спрашиваем у него точное число и сохраняем хэндлы VkImage
	uint32_t actualImageCount = 0;
	vkGetSwapchainImagesKHR(state->device, state->swapchain, &actualImageCount, nullptr);
	state->swapchainImages.resize(actualImageCount);
	vkGetSwapchainImagesKHR(state->device, state->swapchain, &actualImageCount, state->swapchainImages.data());

	// Здесь мы создаем "окна" для доступа к сырым пикселям из Swapchain
	state->swapchainImageViews.resize(actualImageCount); // Выделяем память в векторе под количество образов (ImageView), полученных из Swapchain
	for (uint32_t i = 0; i < actualImageCount; ++i) {	 // Проходим циклом по каждому образу, чтобы для каждого создать свой ImageView
		VkImageViewCreateInfo viewInfo = {};			 // Структура-инструкция, описывающая, как смотреть на текстуру
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = state->swapchainImages[i]; // Привязываем ImageView к конкретному образу (VkImage), который мы получили из Swapchain
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = state->swapchainFormat;						  // Формат пикселей должен совпадать с форматом Swapchain (например, B8G8R8A8_SRGB)
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Настройка доступа к данным внутри изображения (subresourceRange). Указываем, что мы работаем только с цветовым компонентом
		viewInfo.subresourceRange.baseMipLevel = 0;						  // Первый уровень детализации — базовый (0)
		viewInfo.subresourceRange.levelCount = 1;						  // Количество уровней (мы не используем mip-map'ы для вывода на экран)
		viewInfo.subresourceRange.baseArrayLayer = 0;					  // Начальный слой массива (baseArrayLayer) (изображение не является массивом слоев)
		viewInfo.subresourceRange.layerCount = 1;

		// Вызываем Vulkan для создания ImageView
		if (vkCreateImageView(state->device, &viewInfo, nullptr, &state->swapchainImageViews[i]) != VK_SUCCESS) {
			SDL_Log("vkCreateImageView failed");
			return false;
		}
	}
	return true;
}

// RecreateSwapchain — это «спасательный круг» вашего приложения. Она нужна для того, чтобы игра не вылетала и не искажала картинку, когда пользователь меняет размер окна или сворачивает его. В Vulkan объекты Swapchain неизменяемы (immutable). Вы не можете просто сказать: "Измени размер этих картинок на 800x600". Вы обязаны полностью удалить старую цепочку картинок и создать новую с нужными параметрами. Обычно она вызывается в двух случаях: событие изменения размера: когда пользователь тянет за край окна (SDL_EVENT_WINDOW_RESIZED); ошибка устаревания: когда вы пытаетесь получить следующий кадр (vkAcquireNextImageKHR) или вывести его (vkQueuePresentKHR), а видеокарта возвращает ошибку VK_ERROR_OUT_OF_DATE_KHR. Это означает, что размер окна изменился "за спиной" у Vulkan, и текущий Swapchain больше не пригоден
bool RecreateSwapchain(AppState *state)
{
	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(state->window, &w, &h); // Мы обращаемся к библиотеке SDL, чтобы узнать реальный размер клиентской области окна в пикселях. Это важно, так как размер Swapchain должен строго соответствовать текущему размеру окна
	if (w == 0 || h == 0) {							  // Если пользователь свернул окно, его размеры становятся равны нулю. В Vulkan запрещено создавать объекты (Swapchain/Framebuffers) с размерами (0, 0) — это вызовет ошибку. Мы проверяем этот случай и просто возвращаем true (типа "всё в порядке, просто подождем"), не выполняя никаких действий по пересозданию
		return true;
	}

	vkDeviceWaitIdle(state->device); // Это критически важная строка. Мы просим видеокарту: "Пожалуйста, закончь всё, что ты сейчас делаешь". Почему это нужно? Мы собираемся уничтожить объекты (Swapchain, ImageView, Framebuffer), которые видеокарта прямо сейчас может использовать для отрисовки последнего кадра. Если мы удалим их "на лету", приложение упадет (Access Violation). Эта функция блокирует поток CPU, пока GPU полностью не освободит ресурсы
	CleanupSwapchain(state);		 // Мы вызываем функцию, которую разбирали ранее. Она проходит по всем существующим Framebuffer и ImageView и удаляет их, а затем удаляет сам Swapchain. Теперь ресурсы чисты, и мы готовы создать новые, соответствующие новому размеру окна
	return createSwapchain(state);	 // После того как старые ресурсы удалены, мы вызываем функцию, которая: заново создает VkSwapchainKHR с новыми размерами w и h; заново создает все VkImageView; заново создает все VkFramebuffer
}

// ------------------------------------
// --- Инициализация и очистка основного стейта ---

// InitVulkan — это самый масштабный этап подготовки. Здесь мы «знакомим» наше приложение с видеокартой, создаем «холсты» для рисования и подготавливаем инструменты синхронизации
bool InitVulkan(AppState *state)
{
	// 1. Начальная инициализация
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return false;
	}

	state->window = SDL_CreateWindow(PROJECT_NAME, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!state->window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return false;
	}

	// 2. Загрузка Vulkan (Volk)
	if (volkInitialize() != VK_SUCCESS) {
		SDL_Log("volkInitialize failed");
		return false;
	}

	// 3. Создание экземпляра (Instance). Vulkan по своей природе не знает, что такое «окно» или «монитор». Он кроссплатформенный. Чтобы связать Vulkan с конкретной оконной системой (Windows, Linux, macOS), нужны специальные расширения экземпляра (Instance Extensions)
	Uint32 extCount = 0;
	const char *const *sdlExtNames = SDL_Vulkan_GetInstanceExtensions(&extCount); // Функция SDL_Vulkan_GetInstanceExtensions опрашивает SDL: «Какие расширения тебе нужны, чтобы вывести Vulkan-картинку в моё окно?». Она возвращает количество (extCount) и массив строк (sdlExtNames) с именами этих расширений (например, VK_KHR_surface и VK_KHR_win32_surface)
	if (!sdlExtNames) {
		SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
		return false;
	}

	std::vector<const char *> instanceExtensions(sdlExtNames, sdlExtNames + extCount); // SDL возвращает нам «сырой» указатель на массив строк (C-style array). Чтобы нам было удобнее работать (например, если мы захотим добавить свои расширения позже, например, VK_EXT_debug_utils), мы копируем эти указатели в std::vector. Это просто подготовка данных в удобном для C++ формате

	if (kEnableValidation) {
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		if (!CheckValidationLayerSupport()) {
			SDL_Log("Validation layers requested, but not available");
			return false;
		}
	}

	VkApplicationInfo appInfo{};						// Vulkan требует заполнить информацию о вашем приложении для драйвера
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // В Vulkan каждая структура начинается с поля sType, которое говорит драйверу: "Это структура типа VK_STRUCTURE_TYPE_APPLICATION_INFO". Это нужно для безопасности, чтобы драйвер не перепутал структуру с чем-то другим
	appInfo.apiVersion = VK_API_VERSION_1_4;			// Мы указываем, что хотим использовать Vulkan 1.4. Драйвер будет знать, какие функции нам разрешено вызывать

	// Это главная "анкета" для создания VkInstance (объекта, который управляет всей библиотекой Vulkan в памяти)
	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;												 // Мы передаем указатель на appInfo, созданный шагом выше
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()); // Мы передаем количество расширений и сам массив строк (instanceExtensions.data()), которые мы получили от SDL. Драйвер прочитает эти имена и активирует нужные модули связи с окном
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (kEnableValidation) {
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = kValidationLayers.data();

		debugCreateInfo = MakeDebugMessengerCreateInfo();
		instanceCreateInfo.pNext = &debugCreateInfo;
	}

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &state->instance) != VK_SUCCESS) { // Первый аргумент — наша заполненная "анкета" (instanceCreateInfo). Второй (nullptr) — это аллокатор памяти (мы разрешаем Vulkan самому выделять память, поэтому nullptr). Третий (&state->instance) — это указатель, куда Vulkan запишет хэндл (уникальный идентификатор) созданного инстанса
		SDL_Log("vkCreateInstance failed");
		return false;
	}

	volkLoadInstance(state->instance); // Теперь, после этого вызова, все функции Vulkan (которые начинаются с vk...) стали "рабочими" и их можно вызывать

	if (!CreateDebugMessenger(state)) {
		return false;
	}

	// 4. Создание Surface (Поверхности)
	if (!SDL_Vulkan_CreateSurface(state->window, state->instance, nullptr, &state->surface)) {
		SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		return false;
	}

	// 5. Выбор физического устройства (Видеокарты). Сначала узнаем, сколько всего видеокарт (Physical Devices) в системе поддерживают Vulkan. Передаем nullptr вместо массива, чтобы Vulkan просто записал количество в deviceCount
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(state->instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		SDL_Log("No physical devices found");
		return false;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount); // Теперь создаем вектор нужного размера и вызываем функцию второй раз, чтобы реально получить дескрипторы видеокарт в массив devices
	vkEnumeratePhysicalDevices(state->instance, &deviceCount, devices.data());

	// Инициализируем индекс семейства очередей невалидным (максимальным) значением. Он будет служить флагом: "нашли мы подходящую очередь или еще нет"
	state->queueFamilyIndex = UINT32_MAX;
	for (VkPhysicalDevice pd : devices) { // Перебираем все найденные видеокарты по очереди
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(pd, &props);

		// Проверяем, поддерживает ли GPU Vulkan 1.4
		if (props.apiVersion < VK_API_VERSION_1_4) {
			continue; // Ищем следующую видеокарту
		}

		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, nullptr); // Узнаем количество доступных семейств очередей для ТЕКУЩЕЙ видеокарты (снова двойной вызов)
		std::vector<VkQueueFamilyProperties> families(familyCount);			 // Получаем свойства всех семейств очередей этой видеокарты
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &familyCount, families.data());
		for (uint32_t i = 0; i < familyCount; ++i) { // Перебираем все семейства очередей текущей видеокарты
			// Современный способ проверки презентации. Проверяем, умеет ли это семейство очередей выводить картинку на нашу поверхность окна (surface)
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, state->surface, &presentSupport);
			if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) { // Нам нужно семейство очередей, которое одновременно: поддерживает графические операции (VK_QUEUE_GRAPHICS_BIT) и умеет выводить кадры на экран (presentSupport == VK_TRUE)
				// Бинго! Нашли подходящую видеокарту и нужную очередь. Сохраняем их в state
				state->physicalDevice = pd;
				state->queueFamilyIndex = i;
				break;
			}
		}
		if (state->queueFamilyIndex != UINT32_MAX) // Если мы нашли подходящее семейство (индекс изменился), значит мы нашли и нужную видеокарту. Выходим из внешнего цикла
			break;
	}
	if (state->queueFamilyIndex == UINT32_MAX) {
		SDL_Log("No suitable physical device found");
		return false;
	}

	// 6. Выбор формата цвета (формат пикселей + цветовое пространство) для swapchain
	uint32_t formatCount = 0; // Узнаём, сколько форматов поверхности (surface) поддерживает связка: (physicalDevice, surface). Сначала получаем только число (formatCount)
	vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, nullptr);
	if (formatCount == 0)
		return false;

	// Теперь реально запрашиваем список форматов
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, formats.data());

	// Значение по умолчанию: берём первый доступный формат. Это "fallback", если предпочтительный формат/цветовое пространство не найдём
	state->swapchainFormat = formats[0].format;
	state->swapchainColorSpace = formats[0].colorSpace;

	// Перебираем все доступные варианты. Ищем VK_FORMAT_B8G8R8A8_SRGB для автоматической гамма-коррекции при записи в Swapchain
	for (const auto &[format, colorSpace] : formats) {
		if (format == VK_FORMAT_B8G8R8A8_SRGB && colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			state->swapchainFormat = format;
			state->swapchainColorSpace = colorSpace;
			break;
		}
	}

	// 7. Поддержка Portability Subset (macOS). Vulkan-стиль: сначала узнаём количество элементов, затем запрашиваем массив
	uint32_t devExtCount = 0;
	vkEnumerateDeviceExtensionProperties(state->physicalDevice, nullptr, &devExtCount, nullptr);

	std::vector<VkExtensionProperties> availableDevExts(devExtCount); // Выделяем массив под свойства расширений устройства

	// Второй вызов: реально заполняем массив availableDevExts
	vkEnumerateDeviceExtensionProperties(state->physicalDevice, nullptr, &devExtCount, availableDevExts.data());

	// Ищем среди расширений специальное: "VK_KHR_portability_subset"
	bool hasPortabilitySubset = false;
	for (const auto &[extensionName, specVersion] : availableDevExts) {
		if (std::strcmp(extensionName, "VK_KHR_portability_subset") == 0) {
			hasPortabilitySubset = true;
			break;
		}
	}

	std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // Список расширений, которые мы будем включать при создании VkDevice. VK_KHR_SWAPCHAIN_EXTENSION_NAME обязателен для swapchain

	// Если portability subset доступен на устройстве — добавляем его в список включаемых
	if (hasPortabilitySubset) {
		deviceExtensions.push_back("VK_KHR_portability_subset");
	}

	// Параметры создания очереди (queue) логического устройства.
	float queuePriority = 1.0f; // приоритет очереди (0..1 обычно используют)
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = state->queueFamilyIndex; // выбранное семейство очередей (из шага 5)
	queueInfo.queueCount = 1;							  // создаём 1 очередь из этого семейства
	queueInfo.pQueuePriorities = &queuePriority;		  // указатель на массив приоритетов

	// Параметры создания логического устройства VkDevice. В Vulkan 1.4 фичи Dynamic Rendering и Synchronization2 встроены в ядро, но по умолчанию они выключены. Мы создаём структуру для их активации
	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = VK_TRUE; // Включаем рендеринг без RenderPass
	features13.synchronization2 = VK_TRUE; // Включаем улучшенную систему барьеров памяти

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &features13; // <--- Привязываем структуру фич к созданию устройства
	deviceCreateInfo.queueCreateInfoCount = 1;

	deviceCreateInfo.pQueueCreateInfos = &queueInfo;										 // указатель на queueInfo
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); // сколько device-расширений включаем
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();						 // массив C-строк с именами расширений

	// 8. Создание логического устройства (Device). // Создаём логическое устройство VkDevice из выбранной видеокарты (VkPhysicalDevice) и заранее подготовленного VkDeviceCreateInfo (очереди, расширения, фичи и т.д.)
	if (vkCreateDevice(state->physicalDevice, &deviceCreateInfo, nullptr, &state->device) != VK_SUCCESS) {
		SDL_Log("vkCreateDevice failed");
		return false;
	}
	volkLoadDevice(state->device); // // Volk: подгружает device-level функции (vkCmd*, vkCreateSwapchainKHR и т.п.) в соответствии с тем, какие расширения/версия реально включены в state->device

	vkGetDeviceQueue(state->device, state->queueFamilyIndex, 0, &state->queue); // Достаём хэндл очереди из логического устройства. queueFamilyIndex — какое семейство очередей (нашли на этапе выбора физ. устройства), 0 — индекс очереди внутри семейства (мы создавали queueCount = 1, значит это единственная очередь)

	// 9. Инициализация VMA (Менеджера памяти). VMA нужно знать, какими vk* функциями пользоваться. Если у тебя функции подгружены через Volk, то VMA можно “накормить” ими автоматически
	VmaAllocatorCreateInfo allocInfo = {};
	allocInfo.physicalDevice = state->physicalDevice; // VMA нужно знать, с какой видеокартой она работает (для выбора memory types и т.п.)
	allocInfo.device = state->device;				  // И логическое устройство, через которое будут создаваться/управляться VkBuffer/VkImage и память
	allocInfo.instance = state->instance;			  // Также нужен VkInstance (часть VMA-запросов и интеграций использует инстанс)
	allocInfo.vulkanApiVersion = VK_API_VERSION_1_4;  // Какая версия Vulkan считается целевой для VMA

	VmaVulkanFunctions vulkanFunctions = {}; // Структура с указателями на Vulkan-функции, которые VMA будет вызывать внутри себя (создание буферов/изображений, выделение/привязка памяти, etc.)

	// Хелпер из VMA: заполняет vulkanFunctions, используя Volk как источник адресов функций. Обычно это избавляет от ручного заполнения vkGetInstanceProcAddr/vkGetDeviceProcAddr
	if (vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions) != VK_SUCCESS) {
		SDL_Log("vmaImportVulkanFunctionsFromVolk failed");
		return false;
	}
	allocInfo.pVulkanFunctions = &vulkanFunctions; // Кладём указатель на таблицу функций в allocInfo, чтобы vmaCreateAllocator мог её использовать

	// Создаём сам аллокатор VMA. На выходе получаем state->allocator, который потом используешь для vmaCreateBuffer/vmaCreateImage и т.п.
	if (vmaCreateAllocator(&allocInfo, &state->allocator) != VK_SUCCESS) {
		SDL_Log("vmaCreateAllocator failed");
		return false;
	}

	// 10 блока здесь нет, он убежал прочь.

	// 11. Подготовка Swapchain. Тут обычно вызывается отдельная функция, так как создание swapchain и framebuffers — это большой кусок кода, который к тому же нужно перевызывать при изменении размера окна
	if (!createSwapchain(state))
		return false;

	// 12. Создание Command Pool и Buffer. В Vulkan мы не отправляем команды отрисовки напрямую видеокарте. Мы записываем их в Command Buffer, а потом отправляем целиком. Command Buffer выделяется из Command Pool
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = state->queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // command buffer’ы, выделенные из этого pool, можно сбрасывать по одному отдельно, а не только целиком через reset всего pool’а

	if (vkCreateCommandPool(state->device, &poolInfo, nullptr, &state->commandPool) != VK_SUCCESS) {
		SDL_Log("vkCreateCommandPool failed");
		return false;
	}

	state->commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo cmdAllocInfo = {}; // Выделяем Command Buffer из созданного пула
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = state->commandPool;		  // Указываем, к какому семейству очередей будут принадлежать команды из этого пула
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // PRIMARY означает, что этот буфер можно напрямую отправлять в очередь на выполнение. (Вторичные буферы могут вызываться только из первичных)
	cmdAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	if (vkAllocateCommandBuffers(state->device, &cmdAllocInfo, state->commandBuffers.data()) != VK_SUCCESS) {
		return false;
	}

	// 13. Объекты синхронизации. Видеокарта и процессор работают асинхронно. Чтобы они не мешали друг другу (например, чтобы мы не начали рисовать поверх кадра, который еще выводится на экран), нужны Семафоры (GPU-GPU синхронизация) и Фенсы/Заборы (GPU-CPU синхронизация)
	state->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	state->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	state->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Флаг SIGNALED_BIT означает, что забор ИЗНАЧАЛЬНО открыт. Когда мы запускаем первый кадр, процессор будет ждать (vkWaitForFences), пока предыдущий кадр не дорисуется. Но для первого кадра "предыдущего" нет! Если забор будет закрыт, мы зависнем навсегда на самом первом кадре

	// imageAvailableSemaphore — сигнализирует, что swapchain выдал нам пустую картинку и в нее можно рисовать. renderFinishedSemaphore — сигнализирует, что мы закончили рисовать и картинку можно отдавать на монитор. inFlightFence — блокирует процессор, не давая ему записывать новые команды, пока видеокарта не закончит текущий кадр
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(state->device, &semaphoreInfo, nullptr, &state->imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(state->device, &semaphoreInfo, nullptr, &state->renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(state->device, &fenceInfo, nullptr, &state->inFlightFences[i]) != VK_SUCCESS) {
			return false;
		}
	}
	return true;
}
