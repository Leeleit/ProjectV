# Быстрый старт JoltPhysics

🟢 **Уровень 1: Начинающий**

## Шаг 1: CMake

```cmake
add_subdirectory(external/JoltPhysics)
target_link_libraries(YourApp PRIVATE Jolt)
```

### Опции CMake

```cmake
# Включить детерминированную симуляцию (опционально)
set(JPH_CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)

# Отключить debug renderer (для релиза)
set(JPH_DEBUG_RENDERER OFF CACHE BOOL "" FORCE)

# Отключить профилирование
set(JPH_PROFILE_ENABLED OFF CACHE BOOL "" FORCE)
```

## Шаг 2: Инициализация

```cpp
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>

int main() {
    // 1. Регистрация аллокатора (обязательно)
    JPH::RegisterDefaultAllocator();

    // 2. Создание фабрики (обязательно)
    JPH::Factory::sInstance = new JPH::Factory();

    // 3. Регистрация типов форм (обязательно)
    JPH::RegisterTypes();

    // ... создание PhysicsSystem ...

    // Очистка
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    return 0;
}
```

## Шаг 3: Создание PhysicsSystem

```cpp
// Временный аллокатор для симуляции
JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);  // 10 MB

// Создание физической системы
JPH::PhysicsSystem physics_system;
physics_system.Init(
    1024,   // max_bodies — максимум тел
    0,      // num_body_mutexes — 0 = автоматически
    1024,   // max_body_pairs — максимум пар для broad phase
    1024,   // max_contact_constraints — максимум контактов
    nullptr,  // broad_phase_layer_interface (упрощённый режим)
    nullptr,  // object_vs_broadphase_layer_filter
    nullptr   // object_vs_object_layer_filter
);

// Установка гравитации
physics_system.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
```

## Шаг 4: Создание тела

```cpp
// Создание формы (куб 1x1x1)
JPH::BoxShapeSettings box_settings(JPH::Vec3(0.5f, 0.5f, 0.5f));
JPH::ShapeSettings::ShapeResult box_result = box_settings.Create();

if (!box_result.IsValid()) {
    // Ошибка создания формы
    return;
}

JPH::ShapeRefC box_shape = box_result.Get();

// Настройки тела
JPH::BodyCreationSettings body_settings(
    box_shape,                                    // Форма
    JPH::RVec3(0.0f, 10.0f, 0.0f),               // Позиция
    JPH::Quat::sIdentity(),                      // Вращение
    JPH::EMotionType::Dynamic,                   // Тип движения
    0                                            // Object layer
);

// Создание тела
JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();
JPH::BodyID body_id = body_interface.CreateAndAddBody(
    body_settings,
    JPH::EActivation::Activate
);
```

## Шаг 5: Цикл симуляции

```cpp
// В игровом цикле
void update(float delta_time) {
    // Обновление физики
    // collision_steps — количество шагов коллизии на кадр (обычно 1)
    physics_system.Update(
        delta_time,
        1,  // collision_steps
        &temp_allocator,
        nullptr  // job_system (nullptr = однопоточный режим)
    );
}
```

## Шаг 6: Получение результатов

```cpp
JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

// Позиция и вращение
JPH::RVec3 position = body_interface.GetCenterOfMassPosition(body_id);
JPH::Quat rotation = body_interface.GetRotation(body_id);

// Скорость
JPH::Vec3 velocity = body_interface.GetLinearVelocity(body_id);
JPH::Vec3 angular_velocity = body_interface.GetAngularVelocity(body_id);
```

## Полный пример

```cpp
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Core/TempAllocator.h>
#include <iostream>

int main() {
    // Инициализация
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // Аллокатор
    JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

    // Физическая система
    JPH::PhysicsSystem physics_system;
    physics_system.Init(1024, 0, 1024, 1024);
    physics_system.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    // Создание пола (статический куб)
    JPH::BoxShapeSettings floor_settings(JPH::Vec3(10.0f, 0.5f, 10.0f));
    JPH::ShapeRefC floor_shape = floor_settings.Create().Get();

    JPH::BodyCreationSettings floor_body_settings(
        floor_shape,
        JPH::RVec3(0.0f, -0.5f, 0.0f),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        0
    );

    physics_system.GetBodyInterfaceNoLock().CreateAndAddBody(
        floor_body_settings,
        JPH::EActivation::DontActivate
    );

    // Создание падающего куба
    JPH::BoxShapeSettings box_settings(JPH::Vec3(0.5f, 0.5f, 0.5f));
    JPH::ShapeRefC box_shape = box_settings.Create().Get();

    JPH::BodyCreationSettings box_body_settings(
        box_shape,
        JPH::RVec3(0.0f, 10.0f, 0.0f),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        0
    );

    JPH::BodyID box_id = physics_system.GetBodyInterfaceNoLock().CreateAndAddBody(
        box_body_settings,
        JPH::EActivation::Activate
    );

    // Симуляция
    float delta_time = 1.0f / 60.0f;
    for (int i = 0; i < 300; i++) {  // 5 секунд
        physics_system.Update(delta_time, 1, &temp_allocator, nullptr);

        JPH::RVec3 pos = physics_system.GetBodyInterfaceNoLock()
            .GetCenterOfMassPosition(box_id);

        if (i % 60 == 0) {  // Каждую секунду
            std::cout << "Time: " << (i / 60.0f) << "s, "
                      << "Position: (" << pos.GetX() << ", "
                      << pos.GetY() << ", " << pos.GetZ() << ")\n";
        }
    }

    // Очистка
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    return 0;
}
```

## Типы движения тел

| Тип           | Описание                                  | Примеры          |
|---------------|-------------------------------------------|------------------|
| **Static**    | Неподвижное, бесконечная масса            | Пол, стены       |
| **Kinematic** | Движение задаётся вручную, толкает другие | Платформы, двери |
| **Dynamic**   | Подчиняется физике (силы, гравитация)     | Кубы, персонажи  |

## Полный пример: Консольное приложение

Полный standalone пример физической симуляции (без SDL):

```cpp
// ProjectV Example: JoltPhysics Hello
// Description: Минимальный пример физической симуляции с JoltPhysics.
//              Демонстрирует: layers, broad phase, body creation, simulation loop.

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
#include <memory>
#include <print>
#include <thread>

// Отключаем предупреждения, которые генерирует Jolt
JPH_SUPPRESS_WARNINGS

// Все символы Jolt в пространстве имён JPH
using namespace JPH;

// Используем литералы _r для Real (float/double в зависимости от JPH_DOUBLE_PRECISION)
using namespace JPH::literals;

// ============================================================================
// Слои объектов
// ============================================================================
namespace Layers {
static constexpr ObjectLayer NON_MOVING = 0; // Статические объекты (пол, стены)
static constexpr ObjectLayer MOVING = 1;	 // Динамические объекты (сфера, кубы)
static constexpr ObjectLayer NUM_LAYERS = 2; // Общее количество слоёв
} // namespace Layers

// ============================================================================
// Широкие фазы (отдельные деревья bounding volume)
// ============================================================================
namespace BroadPhaseLayers {
static constexpr BroadPhaseLayer NON_MOVING(0); // Дерево для статических объектов
static constexpr BroadPhaseLayer MOVING(1);		// Дерево для динамических объектов
static constexpr uint NUM_LAYERS(2);			// Количество широких фаз
} // namespace BroadPhaseLayers

// ============================================================================
// Callback для трассировки (вывод в консоль)
// ============================================================================
static void TraceImpl(const char *inFMT, ...)
{
	std::va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	std::vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);
	std::println("[Jolt] {}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
// Callback для ассертов
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{
	std::println("{}:{}: ({}) {}", inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");
	return true; // Прервать выполнение
}
#endif // JPH_ENABLE_ASSERTS

// ============================================================================
// Фильтр для object layer vs object layer
// ============================================================================
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

// ============================================================================
// Интерфейс для маппинга object layers → broadphase layers
// ============================================================================
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
		switch (static_cast<BroadPhaseLayer::Type>(inLayer)) {
		case static_cast<BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING):
			return "NON_MOVING";
		case static_cast<BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):
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

// ============================================================================
// Фильтр для object layer vs broadphase layer
// ============================================================================
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

// ============================================================================
// Слушатель активации тел
// ============================================================================
class MyBodyActivationListener : public BodyActivationListener {
  public:
	virtual void OnBodyActivated(const BodyID &inBodyID, uint64 /*inBodyUserData*/) override
	{
		std::println("Body activated: {}", inBodyID.GetIndex());
	}

	virtual void OnBodyDeactivated(const BodyID &inBodyID, uint64 /*inBodyUserData*/) override
	{
		std::println("Body deactivated (sleep): {}", inBodyID.GetIndex());
	}
};

// ============================================================================
// RAII обёртка для Factory (автоматическая очистка)
// ============================================================================
struct FactoryHolder {
	std::unique_ptr<Factory> factory;

	FactoryHolder()
	{
		factory = std::make_unique<Factory>();
		Factory::sInstance = factory.get();
		RegisterTypes();
	}

	~FactoryHolder()
	{
		UnregisterTypes();
		Factory::sInstance = nullptr;
	}
};

// ============================================================================
// Основная функция
// ============================================================================
int main(int /*argc*/, char ** /*argv*/)
{
	std::println("ProjectV: JoltPhysics Hello Example");
	std::println("=====================================");

	// 1. Регистрация аллокатора по умолчанию
	RegisterDefaultAllocator();

	// 2. Установка callback-ов трассировки и ассертов
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

	// 3. Создание фабрики (RAII)
	FactoryHolder factory_holder;

	// 4. TempAllocator для временных выделений во время обновления физики
	TempAllocatorImpl temp_allocator(10 * 1024 * 1024); // 10 MB

	// 5. JobSystem для многопоточной симуляции
	JobSystemThreadPool job_system(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	// 6. Создание фильтров слоёв
	BPLayerInterfaceImpl broad_phase_layer_interface;
	ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
	ObjectLayerPairFilterImpl object_vs_object_layer_filter;

	// 7. Инициализация PhysicsSystem
	PhysicsSystem physics_system;
	physics_system.Init(cMaxBodies = 1024,			   // Максимальное количество тел
						cNumBodyMutexes = 0,		   // Количество мьютексов (0 = по умолчанию)
						cMaxBodyPairs = 1024,		   // Максимальное количество пар тел
						cMaxContactConstraints = 1024, // Максимальное количество контактов
						broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

	// 8. Установка слушателя активации тел
	MyBodyActivationListener body_activation_listener;
	physics_system.SetBodyActivationListener(&body_activation_listener);

	// 9. Получение интерфейса для работы с телами
	BodyInterface &body_interface = physics_system.GetBodyInterface();

	// 10. Создание статического пола
	std::println("Creating floor...");

	BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));
	floor_shape_settings.SetEmbedded(); // Объект на стеке, не удалять при счёте ссылок = 0

	ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
	ShapeRefC floor_shape = floor_shape_result.Get();

	BodyCreationSettings floor_settings(floor_shape, RVec3(0.0_r, -1.0_r, 0.0_r), // Позиция
										Quat::sIdentity(),						  // Ориентация
										EMotionType::Static,					  // Статическое тело
										Layers::NON_MOVING);					  // Слой

	Body *floor = body_interface.CreateBody(floor_settings);
	body_interface.AddBody(floor->GetID(), EActivation::DontActivate);

	// 11. Создание динамической сферы
	std::println("Creating sphere...");

	// Используем ShapeSettings для корректного управления памятью
	SphereShapeSettings sphere_shape_settings(0.5f);
	ShapeSettings::ShapeResult sphere_shape_result = sphere_shape_settings.Create();
	ShapeRefC sphere_shape = sphere_shape_result.Get();

	BodyCreationSettings sphere_settings(sphere_shape, RVec3(0.0_r, 2.0_r, 0.0_r), // На высоте 2 единицы
										 Quat::sIdentity(),
										 EMotionType::Dynamic, // Динамическое тело
										 Layers::MOVING);	   // Слой движущихся

	BodyID sphere_id = body_interface.CreateAndAddBody(sphere_settings, EActivation::Activate);

	// Задаём начальную скорость вниз
	body_interface.SetLinearVelocity(sphere_id, Vec3(0.0f, -5.0f, 0.0f));

	// 12. Оптимизация широкой фазы
	physics_system.OptimizeBroadPhase();

	// 13. Цикл симуляции
	constexpr float cDeltaTime = 1.0f / 60.0f; // 60 Гц
	uint step = 0;

	std::println("");
	std::println("Starting simulation...");
	std::println("Sphere will fall onto the floor and eventually go to sleep.");
	std::println("");

	while (body_interface.IsActive(sphere_id)) {
		++step;

		// Получение позиции и скорости сферы
		RVec3 position = body_interface.GetCenterOfMassPosition(sphere_id);
		Vec3 velocity = body_interface.GetLinearVelocity(sphere_id);

		// Вывод информации каждые 10 шагов
		if (step % 10 == 0) {
			std::println("Step {}: Position = ({:.2f}, {:.2f}, {:.2f}), Velocity = ({:.2f}, {:.2f}, {:.2f})", step,
						 position.GetX(), position.GetY(), position.GetZ(), velocity.GetX(), velocity.GetY(),
						 velocity.GetZ());
		}

		// Обновление физики
		constexpr int cCollisionSteps = 1;
		physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);
	}

	std::println("");
	std::println("Sphere is asleep. Simulation complete.");

	// 14. Очистка
	std::println("Cleaning up...");

	body_interface.RemoveBody(sphere_id);
	body_interface.DestroyBody(sphere_id);
	body_interface.RemoveBody(floor->GetID());
	body_interface.DestroyBody(floor->GetID());

	std::println("Done.");
	return 0;
}
```

---

## Следующие шаги

- **Слои коллизий** — настройка фильтров для управления столкновениями
- **Формы** — различные геометрические примитивы
- **Многопоточность** — использование JobSystem для производительности
