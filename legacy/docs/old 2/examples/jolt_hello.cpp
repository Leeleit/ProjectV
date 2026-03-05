// Пример JoltPhysics для ProjectV
// Минимальный standalone пример на основе HelloWorld.cpp

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

// STL includes
#include <cstdarg>
#include <iostream>
#include <thread>

// Отключаем предупреждения, которые генерирует Jolt
JPH_SUPPRESS_WARNINGS

// Все символы Jolt в пространстве имён JPH
using namespace JPH;

// Используем литералы _r для Real (float/double в зависимости от JPH_DOUBLE_PRECISION)
using namespace JPH::literals;

// Также используем STL
using namespace std;

// Callback для трассировки (вывод в консоль)
static void TraceImpl(const char *inFMT, ...)
{
	// Форматирование сообщения
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Вывод в консоль
	cout << buffer << endl;
}

#ifdef JPH_ENABLE_ASSERTS
// Callback для ассертов
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{
	// Вывод в консоль
	cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << endl;

	// Возвращаем true, чтобы прервать выполнение
	return true;
};
#endif // JPH_ENABLE_ASSERTS

// Слои объектов
namespace Layers {
static constexpr ObjectLayer NON_MOVING = 0; // Статические объекты (пол, стены)
static constexpr ObjectLayer MOVING = 1;	 // Динамические объекты (сфера, кубы)
static constexpr ObjectLayer NUM_LAYERS = 2; // Общее количество слоёв
}; // namespace Layers

// Класс, определяющий, могут ли два слоя объектов сталкиваться
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
  public:
	virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
	{
		switch (inObject1) {
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING; // Статика сталкивается только с динамикой
		case Layers::MOVING:
			return true; // Динамика сталкивается со всем
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

// Широкие фазы (отдельные деревья bounding volume)
namespace BroadPhaseLayers {
static constexpr BroadPhaseLayer NON_MOVING(0); // Дерево для статических объектов
static constexpr BroadPhaseLayer MOVING(1);		// Дерево для динамических объектов
static constexpr uint NUM_LAYERS(2);			// Количество широких фаз
}; // namespace BroadPhaseLayers

// Интерфейс для маппинга object layers → broadphase layers
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
  public:
	BPLayerInterfaceImpl()
	{
		// Создаём таблицу маппинга
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
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

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

// Простой пример слушателя активации тел
class MyBodyActivationListener : public BodyActivationListener {
  public:
	virtual void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override
	{
		cout << "Body activated: " << inBodyID.GetIndex() << endl;
	}

	virtual void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override
	{
		cout << "Body deactivated (sleep): " << inBodyID.GetIndex() << endl;
	}
};

// Основная функция
int main(int argc, char **argv)
{
	// 1. Регистрация аллокатора по умолчанию
	RegisterDefaultAllocator();

	// 2. Установка callback-ов трассировки и ассертов
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

	// 3. Создание фабрики
	Factory::sInstance = new Factory();

	// 4. Регистрация всех типов физики
	RegisterTypes();

	// 5. TempAllocator для временных выделений во время обновления физики
	TempAllocatorImpl temp_allocator(10 * 1024 * 1024); // 10 MB

	// 6. JobSystem для многопоточной симуляции
	JobSystemThreadPool job_system(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

	// 7. Создание фильтров слоёв
	BPLayerInterfaceImpl broad_phase_layer_interface;
	ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
	ObjectLayerPairFilterImpl object_vs_object_layer_filter;

	// 8. Инициализация PhysicsSystem
	PhysicsSystem physics_system;
	physics_system.Init(cMaxBodies = 1024,			   // Максимальное количество тел
						cNumBodyMutexes = 0,		   // Количество мьютексов (0 = по умолчанию)
						cMaxBodyPairs = 1024,		   // Максимальное количество пар тел
						cMaxContactConstraints = 1024, // Максимальное количество контактов
						broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

	// 9. Установка слушателя активации тел (опционально)
	MyBodyActivationListener body_activation_listener;
	physics_system.SetBodyActivationListener(&body_activation_listener);

	// 10. Получение интерфейса для работы с телами
	BodyInterface &body_interface = physics_system.GetBodyInterface();

	// 11. Создание статического пола
	cout << "Creating floor..." << endl;

	// Настройки формы пола (большой бокс 100x1x100)
	BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));
	floor_shape_settings.SetEmbedded(); // Объект на стеке, не удалять при счёте ссылок = 0

	// Создание формы
	ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
	ShapeRefC floor_shape = floor_shape_result.Get(); // Получаем ShapeRefC

	// Настройки тела
	BodyCreationSettings floor_settings(floor_shape, RVec3(0.0_r, -1.0_r, 0.0_r), // Позиция
										Quat::sIdentity(),						  // Ориентация (без вращения)
										EMotionType::Static,					  // Статическое тело
										Layers::NON_MOVING);					  // Слой

	// Создание тела
	Body *floor = body_interface.CreateBody(floor_settings);

	// Добавление тела в мир (без активации, т.к. статическое)
	body_interface.AddBody(floor->GetID(), EActivation::DontActivate);

	// 12. Создание динамической сферы
	cout << "Creating sphere..." << endl;

	// Создание сферы (сокращённая версия)
	BodyCreationSettings sphere_settings(new SphereShape(0.5f),		 // Форма с радиусом 0.5
										 RVec3(0.0_r, 2.0_r, 0.0_r), // На высоте 2 единицы
										 Quat::sIdentity(),
										 EMotionType::Dynamic, // Динамическое тело
										 Layers::MOVING);	   // Слой движущихся

	// Создание и добавление тела в мир (с активацией)
	BodyID sphere_id = body_interface.CreateAndAddBody(sphere_settings, EActivation::Activate);

	// Задаём начальную скорость вниз
	body_interface.SetLinearVelocity(sphere_id, Vec3(0.0f, -5.0f, 0.0f));

	// 13. Оптимизация широкой фазы (не делать каждый кадр!)
	physics_system.OptimizeBroadPhase();

	// 14. Цикл симуляции
	const float cDeltaTime = 1.0f / 60.0f; // 60 Гц
	uint step = 0;

	cout << "\nStarting simulation..." << endl;
	cout << "Sphere will fall onto the floor and eventually go to sleep.\n" << endl;

	while (body_interface.IsActive(sphere_id)) {
		++step;

		// Получение позиции и скорости сферы
		RVec3 position = body_interface.GetCenterOfMassPosition(sphere_id);
		Vec3 velocity = body_interface.GetLinearVelocity(sphere_id);

		// Вывод информации каждые 10 шагов
		if (step % 10 == 0) {
			cout << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", "
				 << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", "
				 << velocity.GetZ() << ")" << endl;
		}

		// Обновление физики
		// Для стабильности при больших шагах времени можно делать несколько collision steps
		const int cCollisionSteps = 1;
		physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);
	}

	cout << "\nSphere is asleep. Simulation complete." << endl;

	// 15. Очистка
	cout << "Cleaning up..." << endl;

	body_interface.RemoveBody(sphere_id);
	body_interface.DestroyBody(sphere_id);

	body_interface.RemoveBody(floor->GetID());
	body_interface.DestroyBody(floor->GetID());

	UnregisterTypes();
	delete Factory::sInstance;
	Factory::sInstance = nullptr;

	cout << "Done." << endl;
	return 0;
}
