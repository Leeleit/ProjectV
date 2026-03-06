// main.cpp – главная функция, собственно говоря. Тут будет крутиться главный цикл и здесь будут подключаться модули

// --- include-блок ---
#define NOMINMAX				 // Отключение Шindows макросов, т.к. они портят std::min, std::max функции
#define VK_NO_PROTOTYPES		 // Для volk, чтобы не было конфликтов
#define SDL_MAIN_USE_CALLBACKS 1 // Чтобы обходиться без main()
#include "SDL3/SDL.h" // SDL3 – библиотека для рисования окон и костяк для обработки рендера (с помощью него создаются базовые функции: event, iterate)
#include "SDL3/SDL_main.h"	  // Выше инклюдили базовые хэдеры, а тут – хэдеры для main цикла
#include "SDL3/SDL_vulkan.h"  // Хэдеры для поддержки работы с Vulkan
#include "volk.h"			  // Volk. В нём уже инклюдится Vulkan
#define VMA_IMPLEMENTATION	  // Генерация определений для VMA
#include "vma/vk_mem_alloc.h" // Хэдеры VMA

// STL блок (стандартные библиотеки)
#include <algorithm>
// #include <cstring> // Пока не надо
#include <vector>
// --- Конец include-блока ---

SDL_AppResult SDL_AppInit(void **appstate, [[maybe_unused]] int argc,
						  [[maybe_unused]] char *argv[]) // Функция инициализации SDL и окна. Вызывается один раз.
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {					// Инициализация видео
		SDL_Log("SDL_Init failed: %s", SDL_GetError()); // Если ошибка, выводим лог об ошибке
		return SDL_APP_FAILURE;							// и безопасно прекращаем работу программы
	}

	SDL_Window *window = SDL_CreateWindow(
		"ProjectV", 1280, 720, SDL_WINDOW_VULKAN); // Создание окна с флагом Vulkan, чтобы SDL понимал, что внутри этого
												   // окна будет картинка рендериться с помощью Vulkan
	if (!window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	*appstate = window; // Предположение: Нужно это окно засунуть в долгоживущую переменную, чтобы оно не закрылось при
						// окончании данной функции.
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent([[maybe_unused]] void *appstate, SDL_Event *event) // Функция обработки событий
{
	if (event->type == SDL_EVENT_QUIT) // Обработка выхода через завершение процесса
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) // Обработка выхода через закрытие окна
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_KEY_DOWN &&
		event->key.key == SDLK_ESCAPE) // Обработка выхода через нажатие клавиши Escape
		return SDL_APP_SUCCESS;
	return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppIterate([[maybe_unused]] void *appstate) // Функция итераций, самая главная функция в движке: она вызывается в
												// цикле и держит программу открытой и работающей
{
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, [[maybe_unused]] SDL_AppResult result) // Функция завершения работы
{
	// What the fuck is that?
	SDL_Window *window = static_cast<SDL_Window *>(appstate);
	if (window)
		SDL_DestroyWindow(window);
}

// А вы знали, что пояснение за код в этом же коде – это антипаттерн? В остальных файлах я ничего объяснять не буду,
// только смысл и принцип работы
