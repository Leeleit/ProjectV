// Пример: Multi-window архитектура редактора вокселей на SDL3
// Демонстрирует управление несколькими окнами, drag & drop, docking, hotkeys
// Документация: docs/sdl/use-cases.md, docs/sdl/performance.md

#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Типы окон редактора
enum class WindowType {
	MAIN_VIEWPORT,	  // Основной 3D вид
	CHUNK_BROWSER,	  // Браузер чанков
	MATERIAL_EDITOR,  // Редактор материалов
	ENTITY_INSPECTOR, // Инспектор сущностей
	CONSOLE,		  // Консоль отладки
	PERFORMANCE,	  // Графики производительности
	TOOL_PALETTE	  // Панель инструментов
};

// Состояние окна редактора
struct EditorWindow {
	SDL_Window *window = nullptr;
	WindowType type;
	std::string title;
	int width = 0, height = 0;
	int pos_x = 0, pos_y = 0;
	bool visible = true;
	bool focused = false;
	bool docked = false;
	int dock_id = -1;						// ID док-позиции (если docked)
	float bg_color[3] = {0.1f, 0.1f, 0.1f}; // Цвет фона окна

	// Callback для рендеринга содержимого окна
	std::function<void(EditorWindow *)> render_callback;

	// Callback для обработки событий окна
	std::function<bool(EditorWindow *, SDL_Event *)> event_callback;

	// Пользовательские данные (для конкретного типа окна)
	void *user_data = nullptr;

	// Состояние drag & drop
	bool is_dragging = false;
	int drag_offset_x = 0, drag_offset_y = 0;

	EditorWindow(SDL_Window *w, WindowType t, const std::string &ttl) : window(w), type(t), title(ttl)
	{
		if (w) {
			SDL_GetWindowSize(w, &width, &height);
			SDL_GetWindowPosition(w, &pos_x, &pos_y);
		}
	}

	~EditorWindow()
	{
		if (window) {
			SDL_DestroyWindow(window);
		}
	}
};

// Менеджер окон редактора
class WindowManager {
  private:
	std::vector<std::unique_ptr<EditorWindow>> windows;
	std::unordered_map<SDL_Window *, EditorWindow *> window_map;
	SDL_Window *main_window = nullptr;

	// Состояние docking системы
	struct DockArea {
		int x, y, width, height;
		bool occupied = false;
		EditorWindow *occupied_by = nullptr;
	};
	std::vector<DockArea> dock_areas;

	// Состояние drag & drop между окнами
	EditorWindow *dragging_window = nullptr;
	SDL_Cursor *default_cursor = nullptr;
	SDL_Cursor *drag_cursor = nullptr;

  public:
	WindowManager()
	{
		default_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
		drag_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

		// Инициализируем док-области (пока простой грид 2x2)
		const int screen_width = 1920;
		const int screen_height = 1080;
		const int dock_width = screen_width / 2;
		const int dock_height = screen_height / 2;

		for (int y = 0; y < 2; y++) {
			for (int x = 0; x < 2; x++) {
				dock_areas.push_back({x * dock_width, y * dock_height, dock_width, dock_height, false, nullptr});
			}
		}
	}

	~WindowManager()
	{
		if (default_cursor)
			SDL_DestroyCursor(default_cursor);
		if (drag_cursor)
			SDL_DestroyCursor(drag_cursor);
	}

	// Создание нового окна редактора
	EditorWindow *createWindow(WindowType type, const std::string &title, int width, int height, int flags = 0)
	{
		SDL_Window *sdl_window = SDL_CreateWindow(title.c_str(), width, height, flags | SDL_WINDOW_RESIZABLE);
		if (!sdl_window) {
			SDL_Log("Failed to create window: %s", SDL_GetError());
			return nullptr;
		}

		auto window = std::make_unique<EditorWindow>(sdl_window, type, title);
		EditorWindow *ptr = window.get();

		// Устанавливаем callback'и в зависимости от типа окна
		setupWindowCallbacks(ptr);

		windows.push_back(std::move(window));
		window_map[sdl_window] = ptr;

		if (!main_window) {
			main_window = sdl_window;
		}

		SDL_Log("Created editor window: %s (%dx%d)", title.c_str(), width, height);
		return ptr;
	}

	// Настройка callback'ов для типа окна
	void setupWindowCallbacks(EditorWindow *win)
	{
		switch (win->type) {
		case WindowType::MAIN_VIEWPORT:
			win->render_callback = [](EditorWindow *w) {
				// В реальном проекте здесь был бы Vulkan рендеринг 3D сцены
				SDL_SetRenderDrawColor(SDL_GetRenderer(w->window), 30, 30, 40, 255);
				SDL_RenderClear(SDL_GetRenderer(w->window));

				// Рисуем сетку для отладки
				SDL_SetRenderDrawColor(SDL_GetRenderer(w->window), 60, 60, 80, 255);
				for (int x = 0; x < w->width; x += 50) {
					SDL_RenderDrawLine(SDL_GetRenderer(w->window), x, 0, x, w->height);
				}
				for (int y = 0; y < w->height; y += 50) {
					SDL_RenderDrawLine(SDL_GetRenderer(w->window), 0, y, w->width, y);
				}

				// Текст в центре окна
				std::string text = "3D Viewport - " + std::to_string(w->width) + "x" + std::to_string(w->height);
				// В реальном проекте здесь была бы отрисовка текста через SDL_ttf

				SDL_RenderPresent(SDL_GetRenderer(w->window));
			};
			break;

		case WindowType::CHUNK_BROWSER:
			win->render_callback = [](EditorWindow *w) {
				SDL_SetRenderDrawColor(SDL_GetRenderer(w->window), 25, 25, 30, 255);
				SDL_RenderClear(SDL_GetRenderer(w->window));

				// Имитация списка чанков
				SDL_SetRenderDrawColor(SDL_GetRenderer(w->window), 70, 130, 180, 255);
				for (int i = 0; i < 10; i++) {
					SDL_Rect rect = {20, 20 + i * 40, w->width - 40, 30};
					SDL_RenderFillRect(SDL_GetRenderer(w->window), &rect);
				}

				SDL_RenderPresent(SDL_GetRenderer(w->window));
			};
			break;

		case WindowType::TOOL_PALETTE:
			win->render_callback = [](EditorWindow *w) {
				SDL_SetRenderDrawColor(SDL_GetRenderer(w->window), 35, 35, 40, 255);
				SDL_RenderClear(SDL_GetRenderer(w->window));

				// Имитация панели инструментов
				const char *tools[] = {"Select", "Voxel Add", "Voxel Remove", "Paint", "Move", "Scale"};
				SDL_SetRenderDrawColor(SDL_GetRenderer(w->window), 80, 80, 100, 255);

				for (size_t i = 0; i < 6; i++) {
					SDL_Rect rect = {10, 10 + static_cast<int>(i) * 50, w->width - 20, 40};
					SDL_RenderFillRect(SDL_GetRenderer(w->window), &rect);
				}

				SDL_RenderPresent(SDL_GetRenderer(w->window));
			};
			break;

		default:
			// Дефолтный рендеринг для других окон
			win->render_callback = [](EditorWindow *w) {
				SDL_SetRenderDrawColor(SDL_GetRenderer(w->window), static_cast<int>(w->bg_color[0] * 255),
									   static_cast<int>(w->bg_color[1] * 255), static_cast<int>(w->bg_color[2] * 255),
									   255);
				SDL_RenderClear(SDL_GetRenderer(w->window));

				// Отображение заголовка типа окна
				std::string type_str;
				switch (w->type) {
				case WindowType::MATERIAL_EDITOR:
					type_str = "Material Editor";
					break;
				case WindowType::ENTITY_INSPECTOR:
					type_str = "Entity Inspector";
					break;
				case WindowType::CONSOLE:
					type_str = "Console";
					break;
				case WindowType::PERFORMANCE:
					type_str = "Performance";
					break;
				default:
					type_str = "Editor Window";
				}

				// В реальном проекте здесь была бы отрисовка текста
				SDL_RenderPresent(SDL_GetRenderer(w->window));
			};
			break;
		}
	}

	// Обработка событий для всех окон
	bool handleEvent(SDL_Event *event)
	{
		switch (event->type) {
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
			SDL_Window *sdl_win = SDL_GetWindowFromID(event->window.windowID);
			auto it = window_map.find(sdl_win);
			if (it != window_map.end()) {
				// Удаляем окно (в реальном редакторе была бы проверка на сохранение)
				SDL_Log("Closing window: %s", it->second->title.c_str());

				// Если это главное окно - выходим из приложения
				if (sdl_win == main_window) {
					return false; // Сигнал к завершению
				}

				// Удаляем окно из контейнеров
				window_map.erase(it);
				auto &vec = windows;
				vec.erase(std::remove_if(vec.begin(), vec.end(),
										 [sdl_win](const auto &win) { return win->window == sdl_win; }),
						  vec.end());

				SDL_DestroyWindow(sdl_win);
			}
			break;
		}

		case SDL_EVENT_WINDOW_FOCUS_GAINED: {
			SDL_Window *sdl_win = SDL_GetWindowFromID(event->window.windowID);
			auto it = window_map.find(sdl_win);
			if (it != window_map.end()) {
				it->second->focused = true;
				// Можно обновить UI (например, выделение активного окна)
			}
			break;
		}

		case SDL_EVENT_WINDOW_FOCUS_LOST: {
			SDL_Window *sdl_win = SDL_GetWindowFromID(event->window.windowID);
			auto it = window_map.find(sdl_win);
			if (it != window_map.end()) {
				it->second->focused = false;
			}
			break;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN: {
			if (event->button.button == SDL_BUTTON_LEFT) {
				// Проверяем, началось ли перетаскивание окна
				SDL_Window *sdl_win = SDL_GetWindowFromID(event->button.windowID);
				auto it = window_map.find(sdl_win);
				if (it != window_map.end() && !it->second->docked) {
					it->second->is_dragging = true;
					it->second->drag_offset_x = event->button.x;
					it->second->drag_offset_y = event->button.y;
					dragging_window = it->second;
					SDL_SetCursor(drag_cursor);
				}
			}
			break;
		}

		case SDL_EVENT_MOUSE_BUTTON_UP: {
			if (event->button.button == SDL_BUTTON_LEFT) {
				if (dragging_window) {
					dragging_window->is_dragging = false;
					dragging_window = nullptr;
					SDL_SetCursor(default_cursor);

					// Проверяем docking при отпускании
					checkDocking(event->button.windowID, event->button.x, event->button.y);
				}
			}
			break;
		}

		case SDL_EVENT_MOUSE_MOTION: {
			if (dragging_window && dragging_window->is_dragging) {
				// Перемещаем окно вместе с мышью
				int new_x = event->motion.x - dragging_window->drag_offset_x;
				int new_y = event->motion.y - dragging_window->drag_offset_y;
				SDL_SetWindowPosition(dragging_window->window, new_x, new_y);

				// Обновляем позицию в структуре
				dragging_window->pos_x = new_x;
				dragging_window->pos_y = new_y;
			}
			break;
		}

		case SDL_EVENT_KEY_DOWN: {
			// Глобальные горячие клавиши редактора
			handleGlobalHotkeys(event->key);
			break;
		}
		}

		return true; // Продолжаем работу
	}

	// Проверка docking при отпускании окна
	void checkDocking(uint32_t window_id, int mouse_x, int mouse_y)
	{
		SDL_Window *sdl_win = SDL_GetWindowFromID(window_id);
		auto it = window_map.find(sdl_win);
		if (it == window_map.end() || it->second->docked)
			return;

		EditorWindow *win = it->second;

		// Проверяем попадание в док-области
		for (size_t i = 0; i < dock_areas.size(); i++) {
			DockArea &area = dock_areas[i];
			if (!area.occupied && mouse_x >= area.x && mouse_x < area.x + area.width && mouse_y >= area.y &&
				mouse_y < area.y + area.height) {

				// Docking окна в эту область
				win->docked = true;
				win->dock_id = static_cast<int>(i);
				area.occupied = true;
				area.occupied_by = win;

				// Позиционируем окно в док-области
				SDL_SetWindowPosition(win->window, area.x, area.y);
				SDL_SetWindowSize(win->window, area.width, area.height);
				SDL_SetWindowResizable(win->window, false);

				SDL_Log("Window docked to area %zu", i);
				return;
			}
		}
	}

	// Обработка глобальных горячих клавиш
	void handleGlobalHotkeys(const SDL_KeyboardEvent &key)
	{
		bool ctrl = (key.mod & SDL_KMOD_CTRL) != 0;
		bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;

		switch (key.key) {
		case SDLK_n: // Ctrl+N - новое окно 3D вьюпорта
			if (ctrl && !shift) {
				createWindow(WindowType::MAIN_VIEWPORT, "3D Viewport 2", 800, 600);
			}
			break;

		case SDLK_t: // Ctrl+T - панель инструментов
			if (ctrl) {
				createWindow(WindowType::TOOL_PALETTE, "Tool Palette", 300, 400);
			}
			break;

		case SDLK_b: // Ctrl+B - браузер чанков
			if (ctrl) {
				createWindow(WindowType::CHUNK_BROWSER, "Chunk Browser", 400, 600);
			}
			break;

		case SDLK_f4: // Alt+F4 - закрыть все окна
			if (key.mod & SDL_KMOD_ALT) {
				SDL_Log("Closing all windows...");
				// В реальном редакторе была бы проверка на сохранение
				windows.clear();
				window_map.clear();
			}
			break;

		case SDLK_f12: // F12 - переключить видимость всех окон
			for (auto &win : windows) {
				win->visible = !win->visible;
				SDL_ShowWindow(win->window, win->visible ? SDL_TRUE : SDL_FALSE);
			}
			break;
		}
	}

	// Рендеринг всех окон
	void renderAll()
	{
		for (auto &win : windows) {
			if (win->visible) {
				// Создаем рендерер если его нет
				if (!SDL_GetRenderer(win->window)) {
					SDL_CreateRenderer(win->window, nullptr, 0);
				}

				// Вызываем callback рендеринга
				if (win->render_callback) {
					win->render_callback(win.get());
				}
			}
		}
	}

	// Получение количества окон
	size_t getWindowCount() const { return windows.size(); }

	// Получение главного окна
	SDL_Window *getMainWindow() const { return main_window; }
};

// Глобальный менеджер окон
static WindowManager *g_window_manager = nullptr;

// Коллбэк инициализации SDL
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	// Инициализируем менеджер окон
	g_window_manager = new WindowManager();

	// Создаем основную компоновку окон редактора
	EditorWindow *main_viewport =
		g_window_manager->createWindow(WindowType::MAIN_VIEWPORT, "3D Viewport - ProjectV Editor", 1280, 720);

	EditorWindow *tool_palette =
		g_window_manager->createWindow(WindowType::TOOL_PALETTE, "Tools", 300, 400, SDL_WINDOW_UTILITY);

	EditorWindow *chunk_browser =
		g_window_manager->createWindow(WindowType::CHUNK_BROWSER, "Chunk Browser", 400, 600, SDL_WINDOW_UTILITY);

	// Позиционируем окна
	if (main_viewport) {
		SDL_SetWindowPosition(main_viewport->window, 100, 100);
	}
	if (tool_palette) {
		SDL_SetWindowPosition(tool_palette->window, 1400, 100);
	}
	if (chunk_browser) {
		SDL_SetWindowPosition(chunk_browser->window, 1400, 550);
	}

	// Сохраняем менеджер окон в appstate
	*appstate = g_window_manager;

	SDL_Log("ProjectV Editor инициализирован");
	SDL_Log("Окна созданы: %zu", g_window_manager->getWindowCount());
	SDL_Log("Горячие клавиши:");
	SDL_Log("  Ctrl+N - новое 3D окно");
	SDL_Log("  Ctrl+T - панель инструментов");
	SDL_Log("  Ctrl+B - браузер чанков");
	SDL_Log("  Alt+F4 - закрыть все");
	SDL_Log("  F12 - переключить видимость окон");
	SDL_Log("  ЛКМ + перетаскивание - перемещение окон");
	SDL_Log("  Бросить окно в сетку - docking");

	return SDL_APP_CONTINUE;
}

// Коллбэк обработки событий SDL
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (!g_window_manager)
		return SDL_APP_CONTINUE;

	// Обработка глобальных событий
	switch (event->type) {
	case SDL_EVENT_QUIT:
		return SDL_APP_SUCCESS;

	case SDL_EVENT_KEY_DOWN:
		if (event->key.key == SDLK_ESCAPE) {
			return SDL_APP_SUCCESS;
		}
		break;
	}

	// Передаем событие в менеджер окон
	bool should_continue = g_window_manager->handleEvent(event);
	return should_continue ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

// Коллбэк итерации приложения (игровой цикл)
SDL_AppResult SDL_AppIterate(void *appstate)
{
	if (!g_window_manager)
		return SDL_APP_CONTINUE;

	// Рендерим все окна
	g_window_manager->renderAll();

	// Небольшая задержка для снижения нагрузки на CPU
	SDL_Delay(16); // ~60 FPS

	return SDL_APP_CONTINUE;
}

// Коллбэк завершения приложения
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	SDL_Log("Завершение редактора...");

	// Очищаем менеджер окон
	if (g_window_manager) {
		delete g_window_manager;
		g_window_manager = nullptr;
	}

	SDL_Quit();
	SDL_Log("Редактор завершен");
}
