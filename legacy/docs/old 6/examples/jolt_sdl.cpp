// Пример: интеграция JoltPhysics с SDL3 в ProjectV
// Демонстрирует создание окна SDL, инициализацию JoltPhysics и физическую симуляцию
// Документация: docs/joltphysics/integration.md, docs/joltphysics/flecs-integration.md

#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <iostream>
#include <thread>

// Отключаем предупреждения, которые генерирует Jolt
JPH_SUPPRESS_WARNINGS

// Все символы Jolt в пространстве имён JPH
using namespace JPH;

// Используем литералы _r для Real
using namespace JPH::literals;

// Также используем STL
using namespace std;

// Структура для хранения состояния приложения
struct AppState {
	SDL_Window *window = nullptr;

	// JoltPhysics компоненты
	PhysicsSystem *physics_system = nullptr;
	TempAllocatorImpl *temp_allocator = nullptr;
	JobSystemThreadPool *job_system = nullptr;

	// Фильтры для JoltPhysics
	class BPLayerInterfaceImpl *broad_phase_layer_interface = nullptr;
	class ObjectVsBroadPhaseLayerFilterImpl *object_vs_broadphase_layer_filter = nullptr;
	class ObjectLayerPairFilterImpl *object_vs_object_layer_filter = nullptr;

	// Тела физического мира
	JPH::BodyID floor_body_id = JPH::BodyID::cInvalidBodyID;
	JPH::BodyID sphere_body_id = JPH::BodyID::cInvalidBodyID;

	// Счётчик кадров для демонстрации
	uint64_t frame_count = 0;
};

// Слои объектов для JoltPhysics
namespace Layers {
static constexpr ObjectLayer NON_MOVING = 0;
static constexpr ObjectLayer MOVING = 1;
static constexpr ObjectLayer NUM_LAYERS = 2;
}; // namespace Layers

// Широкие фазы
namespace BroadPhaseLayers {
static constexpr BroadPhaseLayer NON_MOVING(0);
static constexpr BroadPhaseLayer MOVING(1);
static constexpr uint NUM_LAYERS(2);
}; // namespace BroadPhaseLayers

// Callback для трассировки Jolt (вывод в консоль)
static void TraceImpl(const char *inFMT, ...)
{
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);
	cout << "[Jolt] " << buffer << endl;
}

#ifdef JPH_ENABLE_ASSERTS
// Callback для ассертов Jolt
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{
	cout << "[Jolt Assert] " << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage ? inMessage : "")
		 << endl;
	return true;
}
#endif

// Интерфейс для маппинга object layers → broadphase layers
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
  public:
	BPLayerInterfaceImpl()
	{
		mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

	virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
	{
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char *GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
	{
		switch ((BroadPhaseLayer::Type)inLayer) {
		case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
			return "NON_MOVING";
		case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
			return "MOVING";
		default:
			JPH_ASSERT(false);
			return "INVALID";
		}
	}
#endif

  private:
	BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// Фильтр для object layer vs broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
  public:
	virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1) {
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

// Фильтр для object layer vs object layer
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
  public:
	virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
	{
		switch (inObject1) {
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

// Инициализация JoltPhysics
bool InitializeJoltPhysics(AppState *app)
{
	cout << "Initializing JoltPhysics..." << endl;

	// 1. Регистрация аллокатора по умолчанию
	RegisterDefaultAllocator();

	// 2. Установка callback-ов трассировки и ассертов
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

	// 3. Создание фабрики
	Factory::sInstance = new Factory();

	// 4. Регистрация всех типов физики
	RegisterTypes();

	// 5. TempAllocator для временных выделений
	app->temp_allocator = new TempAllocatorImpl(10 * 1024 * 1024); // 10 MB

	// 6. JobSystem для многопоточной симуляции
	app->job_system = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

	// 7. Создание фильтров слоёв
	app->broad_phase_layer_interface = new BPLayerInterfaceImpl();
	app->object_vs_broadphase_layer_filter = new ObjectVsBroadPhaseLayerFilterImpl();
	app->object_vs_object_layer_filter = new ObjectLayerPairFilterImpl();

	// 8. Создание PhysicsSystem
	app->physics_system = new PhysicsSystem();
	app->physics_system->Init(1024, // Максимальное количество тел
							  0,	// Количество мьютексов (0 = по умолчанию)
							  1024, // Максимальное количество пар тел
							  1024, // Максимальное количество контактов
							  *app->broad_phase_layer_interface, *app->object_vs_broadphase_layer_filter,
							  *app->object_vs_object_layer_filter);

	cout << "JoltPhysics initialized successfully." << endl;
	return true;
}

// Создание физической сцены
bool CreatePhysicsScene(AppState *app)
{
	if (!app->physics_system)
		return false;

	BodyInterface &body_interface = app->physics_system->GetBodyInterface();

	cout << "Creating physics scene..." << endl;

	// 1. Создание статического пола
	BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));
	floor_shape_settings.SetEmbedded();

	ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
	ShapeRefC floor_shape = floor_shape_result.Get();

	BodyCreationSettings floor_settings(floor_shape, RVec3(0.0_r, -1.0_r, 0.0_r), Quat::sIdentity(),
										EMotionType::Static, Layers::NON_MOVING);

	Body *floor = body_interface.CreateBody(floor_settings);
	app->floor_body_id = floor->GetID();
	body_interface.AddBody(app->floor_body_id, EActivation::DontActivate);

	// 2. Создание динамической сферы
	BodyCreationSettings sphere_settings(new SphereShape(0.5f), RVec3(0.0_r, 10.0_r, 0.0_r), Quat::sIdentity(),
										 EMotionType::Dynamic, Layers::MOVING);

	app->sphere_body_id = body_interface.CreateAndAddBody(sphere_settings, EActivation::Activate);

	// Задаём начальную скорость вниз
	body_interface.SetLinearVelocity(app->sphere_body_id, Vec3(0.0f, -2.0f, 0.0f));

	// 3. Оптимизация широкой фазы
	app->physics_system->OptimizeBroadPhase();

	cout << "Physics scene created:" << endl;
	cout << "  - Floor (static): ID " << app->floor_body_id.GetIndex() << endl;
	cout << "  - Sphere (dynamic): ID " << app->sphere_body_id.GetIndex() << endl;

	return true;
}

// Очистка JoltPhysics
void CleanupJoltPhysics(AppState *app)
{
	cout << "Cleaning up JoltPhysics..." << endl;

	if (app->physics_system) {
		BodyInterface &body_interface = app->physics_system->GetBodyInterface();

		// Удаление тел
		if (!app->sphere_body_id.IsInvalid()) {
			body_interface.RemoveBody(app->sphere_body_id);
			body_interface.DestroyBody(app->sphere_body_id);
		}

		if (!app->floor_body_id.IsInvalid()) {
			body_interface.RemoveBody(app->floor_body_id);
			body_interface.DestroyBody(app->floor_body_id);
		}
	}

	// Очистка Jolt
	if (Factory::sInstance) {
		UnregisterTypes();
		delete Factory::sInstance;
		Factory::sInstance = nullptr;
	}

	// Очистка выделенных ресурсов
	delete app->physics_system;
	app->physics_system = nullptr;
	delete app->temp_allocator;
	app->temp_allocator = nullptr;
	delete app->job_system;
	app->job_system = nullptr;
	delete app->broad_phase_layer_interface;
	app->broad_phase_layer_interface = nullptr;
	delete app->object_vs_broadphase_layer_filter;
	app->object_vs_broadphase_layer_filter = nullptr;
	delete app->object_vs_object_layer_filter;
	app->object_vs_object_layer_filter = nullptr;

	cout << "JoltPhysics cleanup complete." << endl;
}

// Обновление физики (вызывается каждый кадр)
void UpdatePhysics(AppState *app, float delta_time)
{
	if (!app->physics_system)
		return;

	// Обновление физики с одним шагом столкновений
	const int collision_steps = 1;
	app->physics_system->Update(delta_time, collision_steps, app->temp_allocator, app->job_system);

	// Каждые 60 кадров выводим информацию о сфере
	if (app->frame_count % 60 == 0 && !app->sphere_body_id.IsInvalid()) {
		BodyInterface &body_interface = app->physics_system->GetBodyInterface();

		RVec3 position = body_interface.GetCenterOfMassPosition(app->sphere_body_id);
		Vec3 velocity = body_interface.GetLinearVelocity(app->sphere_body_id);
		bool is_active = body_interface.IsActive(app->sphere_body_id);

		cout << "Frame " << app->frame_count << ": Sphere position = (" << position.GetX() << ", " << position.GetY()
			 << ", " << position.GetZ() << "), active = " << (is_active ? "yes" : "no (sleeping)") << endl;
	}
}

// SDL_AppInit: инициализация приложения
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	cout << "ProjectV: JoltPhysics + SDL3 Integration Example" << endl;
	cout << "=============================================" << endl;

	// Создание состояния приложения
	auto *app = new AppState();

	// Инициализация SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		delete app;
		return SDL_APP_FAILURE;
	}

	// Создание окна
	app->window = SDL_CreateWindow("ProjectV - JoltPhysics Demo", 1280, 720, SDL_WINDOW_VULKAN);
	if (!app->window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		delete app;
		return SDL_APP_FAILURE;
	}

	// Инициализация JoltPhysics
	if (!InitializeJoltPhysics(app)) {
		SDL_Log("Failed to initialize JoltPhysics");
		SDL_DestroyWindow(app->window);
		SDL_Quit();
		delete app;
		return SDL_APP_FAILURE;
	}

	// Создание физической сцены
	if (!CreatePhysicsScene(app)) {
		SDL_Log("Failed to create physics scene");
		CleanupJoltPhysics(app);
		SDL_DestroyWindow(app->window);
		SDL_Quit();
		delete app;
		return SDL_APP_FAILURE;
	}

	*appstate = app;
	cout << "\nApplication initialized successfully." << endl;
	cout << "Controls:" << endl;
	cout << "  - ESC: Exit" << endl;
	cout << "  - Close window: Exit" << endl;
	cout << "  - Sphere will fall and eventually go to sleep" << endl;
	cout << "  - Physics info printed every second (60 frames)\n" << endl;

	return SDL_APP_CONTINUE;
}

// SDL_AppEvent: обработка событий
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (event->type == SDL_EVENT_QUIT)
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
		return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
		return SDL_APP_SUCCESS;

	return SDL_APP_CONTINUE;
}

// SDL_AppIterate: основной цикл (вызывается каждый кадр)
SDL_AppResult SDL_AppIterate(void *appstate)
{
	auto *app = static_cast<AppState *>(appstate);

	// Увеличиваем счётчик кадров
	app->frame_count++;

	// Обновление физики (фиксированный шаг времени для стабильности)
	const float delta_time = 1.0f / 60.0f; // 60 Гц
	UpdatePhysics(app, delta_time);

	// Здесь обычно идёт рендеринг через Vulkan
	// Для этого примера просто выводим точку прогресса в консоль
	if (app->frame_count % 120 == 0) {
		cout << "." << flush;
	}

	return SDL_APP_CONTINUE;
}

// SDL_AppQuit: очистка ресурсов
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	auto *app = static_cast<AppState *>(appstate);

	cout << "\n\nShutting down..." << endl;
	cout << "Total frames: " << app->frame_count << endl;

	// Очистка JoltPhysics
	CleanupJoltPhysics(app);

	// Очистка SDL
	if (app->window) {
		SDL_DestroyWindow(app->window);
		app->window = nullptr;
	}

	SDL_Quit();

	// Очистка состояния приложения
	delete app;

	cout << "Application shutdown complete." << endl;
}

// Примечания для интеграции в реальный проект:
// 1. Для реального рендеринга нужно инициализировать Vulkan и рендерить сцену
// 2. Можно использовать flecs ECS для управления сущностями (см. flecs-integration.md)
// 3. Delta time лучше вычислять на основе реального времени, а не фиксированного
// 4. Для сложных сцен используйте JobSystem JoltPhysics для многопоточной симуляции
// 5. Добавьте обработку ввода для управления физическими телами
