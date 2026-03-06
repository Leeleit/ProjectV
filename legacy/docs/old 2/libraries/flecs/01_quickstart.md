# Быстрый старт flecs

🟢 **Уровень 1: Начинающий**

Минимальный пример для начала работы с flecs.

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/flecs)

add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp PRIVATE flecs::flecs_static)
```

## main.cpp (C++)

```cpp
#include <flecs.h>
#include <iostream>

// Компоненты — обычные структуры
struct Position { float x, y; };
struct Velocity { float x, y; };

int main() {
    // Создаём world
    flecs::world ecs;

    // Создаём систему
    ecs.system<Position, const Velocity>("Move")
        .each([](flecs::entity e, Position& p, const Velocity& v) {
            p.x += v.x;
            p.y += v.y;
            std::cout << e.name() << " moved to {" << p.x << ", " << p.y << "}\n";
        });

    // Создаём сущность с компонентами
    ecs.entity("MyEntity")
        .set<Position>({10.0f, 20.0f})
        .set<Velocity>({1.0f, 0.5f});

    // Игровой цикл
    while (ecs.progress(1.0f / 60.0f)) {
        // Выход по ecs.quit() или внешнему условию
        static int frame = 0;
        if (++frame > 3) {
            ecs.quit();
        }
    }

    // World уничтожается автоматически (RAII)
    return 0;
}
```

## Результат

```
MyEntity moved to {11, 20.5}
MyEntity moved to {12, 21}
MyEntity moved to {13, 21.5}
```

## Ключевые моменты

| Концепт       | Пример                                                           |
|---------------|------------------------------------------------------------------|
| **World**     | `flecs::world ecs;` — контейнер для всех данных ECS              |
| **Component** | `struct Position { float x, y; };` — обычная структура           |
| **System**    | `ecs.system<T...>().each([](...){})` — функция над entities      |
| **Entity**    | `ecs.entity("Name")` — создать именованную сущность              |
| **Set**       | `entity.set<Position>({x, y})` — добавить компонент со значением |
| **Progress**  | `ecs.progress(dt)` — выполнить один кадр (pipeline)              |

## Иерархия

```cpp
// Родитель и ребёнок
auto parent = ecs.entity("Parent");
auto child = ecs.entity("Child").child_of(parent);

// При удалении родителя удалятся все дети
parent.destruct();
```

## Интеграция с SDL3

Полный пример интеграции flecs ECS с SDL3 callback архитектурой:

```cpp
// ProjectV Example: flecs ECS + SDL3 Basic
// Description: Базовая интеграция flecs ECS с SDL3 callback архитектурой.
//              Демонстрирует: components, systems, prefabs, hierarchy, player input.

#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "flecs.h"

#include <memory>

// ============================================================================
// Компоненты
// ============================================================================

struct Position {
	float x = 0.0f;
	float y = 0.0f;
};

struct Velocity {
	float x = 0.0f;
	float y = 0.0f;
};

struct Health {
	float current = 100.0f;
	float max = 100.0f;
};

// Тэги для маркировки сущностей
struct TagPlayer {};
struct TagEnemy {};

// ============================================================================
// Префабы и иерархия
// ============================================================================

static flecs::entity create_enemy_prefab(flecs::world &ecs)
{
	return ecs.prefab("EnemyPrefab").set<Velocity>({0.5f, 0.0f}).set<Health>({100.0f, 100.0f}).add<TagEnemy>();
}

static flecs::entity spawn_enemy(flecs::world &ecs, flecs::entity prefab, float x, float y)
{
	return ecs.entity().is_a(prefab).set<Position>({x, y});
}

// ============================================================================
// Состояние приложения (RAII)
// ============================================================================

struct AppState {
	SDL_Window *window = nullptr;
	flecs::world ecs;
	flecs::entity prefab_enemy;
	std::uint64_t frame = 0;

	~AppState()
	{
		if (window) {
			SDL_DestroyWindow(window);
		}
		SDL_Quit();
	}
};

// ============================================================================
// SDL callbacks
// ============================================================================

SDL_AppResult SDL_AppInit(void **appstate, int /*argc*/, char ** /*argv*/)
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	auto app = std::make_unique<AppState>();

	app->window = SDL_CreateWindow("flecs + SDL3 - ProjectV", 1280, 720, 0);
	if (!app->window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	flecs::world &ecs = app->ecs;

	// --- Системы ---

	// Движение: обновляем позицию по скорости
	ecs.system<Position, const Velocity>("MoveSystem").each([](Position &p, const Velocity &v) {
		p.x += v.x;
		p.y += v.y;
	});

	// Регенерация здоровья: только у врагов
	ecs.system<Health>("HealthRegenSystem").with<TagEnemy>().each([](Health &h) {
		h.current = (h.current + 0.1f < h.max) ? h.current + 0.1f : h.max;
	});

	// Отладочный вывод: раз в 60 кадров
	ecs.system<const Position, const Health>("DebugSystem")
		.with<TagEnemy>()
		.iter([](flecs::iter &it, const Position *p, const Health *h) {
			static std::uint64_t last_print = 0;
			auto frame_count = it.world().get_info()->frame_count_total;
			if (frame_count - last_print < 60) {
				return;
			}
			last_print = frame_count;

			SDL_Log("=== Enemies: %d ===", it.count());
			for (auto i : it) {
				SDL_Log("  [%s] pos=(%.1f, %.1f) hp=%.0f/%.0f", it.entity(i).name().c_str(), p[i].x, p[i].y,
						h[i].current, h[i].max);
			}
		});

	// --- Префаб и сущности ---
	app->prefab_enemy = create_enemy_prefab(ecs);

	// Игрок
	ecs.entity("Player")
		.set<Position>({640.0f, 360.0f})
		.set<Velocity>({0.0f, 0.0f})
		.set<Health>({250.0f, 250.0f})
		.add<TagPlayer>();

	// Враги из префаба
	spawn_enemy(ecs, app->prefab_enemy, 100.0f, 200.0f).set_name("Enemy_1");
	spawn_enemy(ecs, app->prefab_enemy, 300.0f, 150.0f).set_name("Enemy_2");
	spawn_enemy(ecs, app->prefab_enemy, 500.0f, 400.0f)
		.set_name("Enemy_Fast")
		.set<Velocity>({2.0f, 1.0f}); // Переопределяем скорость

	// Иерархия: дочерние сущности
	flecs::entity turret = ecs.entity("Turret").set<Position>({200.0f, 200.0f});
	ecs.entity("Barrel").child_of(turret).set<Position>({0.0f, 20.0f}); // Локальная позиция

	SDL_Log("ECS initialized: %lld entities", static_cast<long long>(ecs.count<Position>()));

	*appstate = app.release();
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
		return SDL_APP_SUCCESS;
	}

	// Управление игроком: WASD
	if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
		auto *app = static_cast<AppState *>(appstate);
		flecs::world &ecs = app->ecs;

		ecs.query<Velocity>().with<TagPlayer>().build().each([&event](Velocity &v) {
			const float speed = 3.0f;
			if (event->type == SDL_EVENT_KEY_DOWN) {
				switch (event->key.key) {
				case SDLK_W:
					v.y = -speed;
					break;
				case SDLK_S:
					v.y = speed;
					break;
				case SDLK_A:
					v.x = -speed;
					break;
				case SDLK_D:
					v.x = speed;
					break;
				default:
					break;
				}
			} else {
				switch (event->key.key) {
				case SDLK_W:
				case SDLK_S:
					v.y = 0.0f;
					break;
				case SDLK_A:
				case SDLK_D:
					v.x = 0.0f;
					break;
				default:
					break;
				}
			}
		});
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	auto *app = static_cast<AppState *>(appstate);
	app->ecs.progress(1.0f / 60.0f);
	app->frame++;
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult /*result*/)
{
	auto *app = static_cast<AppState *>(appstate);
	if (app) {
		delete app;
	}
}
```

### Ключевые моменты интеграции

| Паттерн                  | Описание                                                 |
|--------------------------|----------------------------------------------------------|
| **RAII**                 | AppState с автоматической очисткой в деструкторе         |
| **Callback архитектура** | SDL_MAIN_USE_CALLBACKS для интеграции с flecs.progress() |
| **Префабы**              | `ecs.prefab()` + `is_a()` для создания шаблонов          |
| **Иерархия**             | `child_of()` для родительско-дочерних отношений          |
| **Query в event**        | Динамический query для обработки ввода                   |

---

## C API

```c
#include <flecs.h>

typedef struct { float x, y; } Position;
typedef struct { float x, y; } Velocity;

void Move(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 0);
    Velocity *v = ecs_field(it, Velocity, 1);

    for (int i = 0; i < it->count; i++) {
        p[i].x += v[i].x;
        p[i].y += v[i].y;
    }
}

int main() {
    ecs_world_t *world = ecs_init();

    // Регистрация компонентов
    ECS_COMPONENT(world, Position);
    ECS_COMPONENT(world, Velocity);

    // Создание системы
    ECS_SYSTEM(world, Move, EcsOnUpdate, Position, Velocity);

    // Создание сущности
    ecs_entity_t e = ecs_new(world);
    ecs_set(world, e, Position, {10, 20});
    ecs_set(world, e, Velocity, {1, 0.5});

    // Цикл
    while (ecs_progress(world, 1.0f/60.0f)) {}

    ecs_fini(world);
    return 0;
}
```
