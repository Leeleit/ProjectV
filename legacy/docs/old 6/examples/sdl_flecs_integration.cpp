// Пример: Продвинутая интеграция SDL3 событий с flecs ECS
// Демонстрирует обработку ввода, компоненты событий и системы для воксельного движка
// Документация: docs/sdl/integration.md, docs/flecs/voxel-patterns.md

#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "flecs.h"
#include <string>
#include <vector>

// Компоненты для хранения событий ввода
struct InputState {
	bool keys[SDL_NUM_SCANCODES] = {false};
	float mouse_x = 0, mouse_y = 0;
	float mouse_delta_x = 0, mouse_delta_y = 0;
	bool mouse_buttons[8] = {false};
	bool quit_requested = false;
};

// Компонент для камеры (упрощенный для примера)
struct Camera {
	float x = 0, y = 0, z = 5.0f;
	float pitch = 0, yaw = 0;
	float fov = 60.0f;
	float sensitivity = 0.1f;
	float move_speed = 5.0f;
};

// Компонент для отладочной информации
struct DebugInfo {
	std::string fps_text;
	std::string camera_pos_text;
	std::string input_summary;
	uint64_t frame_count = 0;
};

// Компонент для воксельного выделения (пример для редактора)
struct VoxelSelection {
	int32_t start_x = 0, start_y = 0, start_z = 0;
	int32_t end_x = 0, end_y = 0, end_z = 0;
	bool active = false;
	uint32_t color = 0xFF00FF00; // Зеленый
};

// Глобальные указатели (в реальном проекте лучше использовать dependency injection)
static flecs::world *g_ecs = nullptr;
static InputState *g_input = nullptr;
static SDL_Window *g_main_window = nullptr;

// Система: обновление состояния ввода из SDL событий
void InputUpdateSystem(flecs::iter &it)
{
	if (!g_input || !g_ecs)
		return;

	// Сброс дельты мыши каждый кадр
	g_input->mouse_delta_x = 0;
	g_input->mouse_delta_y = 0;

	// Обработка событий SDL (обычно делается в SDL_AppEvent)
	// В этом примере события обрабатываются в коллбэке и сохраняются в InputState
}

// Система: обработка ввода камеры
void CameraControlSystem(flecs::iter &it, Camera *cameras)
{
	if (!g_input)
		return;

	for (int i : it) {
		Camera &cam = cameras[i];

		// Обработка клавиатуры для движения камеры
		float forward = 0, right = 0, up = 0;

		if (g_input->keys[SDL_SCANCODE_W])
			forward += 1.0f;
		if (g_input->keys[SDL_SCANCODE_S])
			forward -= 1.0f;
		if (g_input->keys[SDL_SCANCODE_A])
			right -= 1.0f;
		if (g_input->keys[SDL_SCANCODE_D])
			right += 1.0f;
		if (g_input->keys[SDL_SCANCODE_SPACE])
			up += 1.0f;
		if (g_input->keys[SDL_SCANCODE_LCTRL])
			up -= 1.0f;

		// Нормализация вектора движения
		if (forward != 0 || right != 0 || up != 0) {
			float len = sqrtf(forward * forward + right * right + up * up);
			forward /= len;
			right /= len;
			up /= len;

			// Применяем скорость с учетом дельты времени
			float dt = it.delta_time();
			cam.x += forward * cam.move_speed * dt;
			cam.y += right * cam.move_speed * dt;
			cam.z += up * cam.move_speed * dt;
		}

		// Обработка мыши для вращения камеры
		if (g_input->mouse_buttons[SDL_BUTTON_RIGHT]) {
			cam.yaw += g_input->mouse_delta_x * cam.sensitivity;
			cam.pitch += g_input->mouse_delta_y * cam.sensitivity;

			// Ограничение угла pitch
			if (cam.pitch > 89.0f)
				cam.pitch = 89.0f;
			if (cam.pitch < -89.0f)
				cam.pitch = -89.0f;
		}
	}
}

// Система: обновление отладочной информации
void DebugInfoSystem(flecs::iter &it, DebugInfo *infos)
{
	if (!g_input || !g_ecs)
		return;

	static uint64_t last_time = SDL_GetTicks();
	uint64_t current_time = SDL_GetTicks();
	uint64_t delta_time = current_time - last_time;

	for (int i : it) {
		DebugInfo &info = infos[i];
		info.frame_count++;

		// Расчет FPS
		if (delta_time > 1000) { // Каждую секунду
			float fps = (info.frame_count * 1000.0f) / delta_time;
			info.fps_text = "FPS: " + std::to_string(static_cast<int>(fps));

			// Сброс счетчика
			info.frame_count = 0;
			last_time = current_time;
		}

		// Получение компонента камеры для отображения позиции
		flecs::entity e = it.entity(i);
		if (e.has<Camera>()) {
			const Camera *cam = e.get<Camera>();
			info.camera_pos_text = "Camera: (" + std::to_string(cam->x) + ", " + std::to_string(cam->y) + ", " +
								   std::to_string(cam->z) + ")";
		}

		// Сводка по вводу
		int keys_pressed = 0;
		for (bool key : g_input->keys) {
			if (key)
				keys_pressed++;
		}

		info.input_summary = "Keys pressed: " + std::to_string(keys_pressed) + ", Mouse: (" +
							 std::to_string(g_input->mouse_x) + ", " + std::to_string(g_input->mouse_y) + ")";
	}
}

// Система: обработка воксельного выделения
void VoxelSelectionSystem(flecs::iter &it, VoxelSelection *selections)
{
	if (!g_input)
		return;

	for (int i : it) {
		VoxelSelection &sel = selections[i];

		// Начало выделения при нажатии левой кнопки мыши
		if (g_input->mouse_buttons[SDL_BUTTON_LEFT] && !sel.active) {
			sel.active = true;
			sel.start_x = static_cast<int32_t>(g_input->mouse_x);
			sel.start_y = static_cast<int32_t>(g_input->mouse_y);
			sel.start_z = 0; // В 2D примере
			sel.end_x = sel.start_x;
			sel.end_y = sel.start_y;
			sel.end_z = sel.start_z;
		}

		// Обновление выделения при движении мыши
		if (sel.active && g_input->mouse_buttons[SDL_BUTTON_LEFT]) {
			sel.end_x = static_cast<int32_t>(g_input->mouse_x);
			sel.end_y = static_cast<int32_t>(g_input->mouse_y);
		}

		// Завершение выделения при отпускании кнопки
		if (!g_input->mouse_buttons[SDL_BUTTON_LEFT] && sel.active) {
			sel.active = false;
			// В реальном проекте здесь было бы создание компонента для рендеринга выделения
		}
	}
}

// Коллбэк инициализации SDL
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	// Создаем основное окно
	g_main_window = SDL_CreateWindow("SDL3 + flecs Integration", 1280, 720, SDL_WINDOW_RESIZABLE);
	if (!g_main_window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return SDL_APP_FAILURE;
	}

	// Инициализируем ECS мир
	g_ecs = new flecs::world();
	g_input = new InputState();

	// Регистрируем компоненты
	g_ecs->component<InputState>();
	g_ecs->component<Camera>();
	g_ecs->component<DebugInfo>();
	g_ecs->component<VoxelSelection>();

	// Создаем сущность для глобального состояния ввода
	flecs::entity input_entity = g_ecs->entity("global_input");
	input_entity.set<InputState>({});

	// Создаем сущность камеры
	flecs::entity camera_entity = g_ecs->entity("main_camera");
	camera_entity.set<Camera>({});
	camera_entity.set<DebugInfo>({});

	// Создаем сущность для воксельного выделения
	flecs::entity selection_entity = g_ecs->entity("voxel_selection");
	selection_entity.set<VoxelSelection>({});

	// Регистрируем системы
	g_ecs->system<>("InputUpdateSystem").kind(flecs::OnLoad).iter(InputUpdateSystem);
	g_ecs->system<Camera>("CameraControlSystem").kind(flecs::OnUpdate).iter(CameraControlSystem);
	g_ecs->system<DebugInfo>("DebugInfoSystem").kind(flecs::OnUpdate).iter(DebugInfoSystem);
	g_ecs->system<VoxelSelection>("VoxelSelectionSystem").kind(flecs::OnUpdate).iter(VoxelSelectionSystem);

	// Сохраняем указатели для использования в коллбэках
	*appstate = g_main_window;

	SDL_Log("SDL3 + flecs интеграция инициализирована успешно");
	SDL_Log("Управление камерой: WASD - движение, ПКМ + мышь - вращение");
	SDL_Log("ЛКМ - выделение вокселей, ESC - выход");

	return SDL_APP_CONTINUE;
}

// Коллбэк обработки событий SDL
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (!g_input)
		return SDL_APP_CONTINUE;

	switch (event->type) {
	case SDL_EVENT_QUIT:
		g_input->quit_requested = true;
		return SDL_APP_SUCCESS;

	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		g_input->quit_requested = true;
		return SDL_APP_SUCCESS;

	case SDL_EVENT_KEY_DOWN:
		if (event->key.key == SDLK_ESCAPE) {
			g_input->quit_requested = true;
			return SDL_APP_SUCCESS;
		}
		if (event->key.scancode < SDL_NUM_SCANCODES) {
			g_input->keys[event->key.scancode] = true;
		}
		break;

	case SDL_EVENT_KEY_UP:
		if (event->key.scancode < SDL_NUM_SCANCODES) {
			g_input->keys[event->key.scancode] = false;
		}
		break;

	case SDL_EVENT_MOUSE_MOTION:
		g_input->mouse_delta_x = event->motion.xrel;
		g_input->mouse_delta_y = event->motion.yrel;
		g_input->mouse_x = event->motion.x;
		g_input->mouse_y = event->motion.y;
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (event->button.button < 8) {
			g_input->mouse_buttons[event->button.button] = true;
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (event->button.button < 8) {
			g_input->mouse_buttons[event->button.button] = false;
		}
		break;
	}

	return SDL_APP_CONTINUE;
}

// Коллбэк итерации приложения (игровой цикл)
SDL_AppResult SDL_AppIterate(void *appstate)
{
	if (!g_ecs || !g_input)
		return SDL_APP_CONTINUE;

	// Запускаем обновление ECS мира с фиксированным временным шагом
	const float fixed_dt = 1.0f / 60.0f; // 60 FPS
	g_ecs->progress(fixed_dt);

	// В реальном проекте здесь был бы рендеринг
	// Для примера просто выводим отладочную информацию в заголовок окна

	static uint32_t last_update = 0;
	uint32_t current_time = SDL_GetTicks();

	// Обновляем заголовок окна раз в 100 мс
	if (current_time - last_update > 100) {
		last_update = current_time;

		// Получаем отладочную информацию из ECS
		flecs::filter<DebugInfo> f = g_ecs->filter<DebugInfo>();
		f.each([](flecs::entity e, DebugInfo &info) {
			std::string title =
				"SDL3 + flecs | " + info.fps_text + " | " + info.camera_pos_text + " | " + info.input_summary;
			SDL_SetWindowTitle(g_main_window, title.c_str());
		});
	}

	return g_input->quit_requested ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

// Коллбэк завершения приложения
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	SDL_Log("Завершение приложения...");

	// Очищаем ECS ресурсы
	if (g_ecs) {
		delete g_ecs;
		g_ecs = nullptr;
	}

	// Очищаем состояние ввода
	if (g_input) {
		delete g_input;
		g_input = nullptr;
	}

	// Закрываем окно
	if (g_main_window) {
		SDL_DestroyWindow(g_main_window);
		g_main_window = nullptr;
	}

	SDL_Quit();
	SDL_Log("Приложение завершено");
}
