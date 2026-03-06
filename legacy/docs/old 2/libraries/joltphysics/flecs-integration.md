# Интеграция с ECS (flecs)

**🟡 Уровень 2: Средний**

> ⚠️ **Контент перемещён**
>
> Вся информация об интеграции JoltPhysics с ECS flecs была перемещена в общий файл интеграции для лучшей организации и
> избежания дублирования.

## Где найти информацию?

Все материалы по интеграции JoltPhysics с ECS flecs теперь доступны
в [projectv-integration.md](projectv-integration.md):

- [Архитектура интеграции](projectv-integration.md#архитектура-интеграции)
- [Интеграция с ECS (flecs)](projectv-integration.md#интеграция-с-ecs-flecs)
- [Полная система интеграции](projectv-integration.md#полная-система-интеграции)
- [Проблема: Интеграция с flecs ECS](projectv-integration.md#проблема-интеграция-с-flecs-ecs)

## Что включает интеграция?

Интеграция JoltPhysics с flecs в ProjectV охватывает следующие аспекты:

### Компоненты

- `JoltBody` — обёртка для `JPH::BodyID`
- `PhysicsProperties` — физические свойства сущности
- `PhysicsForces` — силы, применяемые к сущности

### Системы

- **PreUpdate**: Синхронизация ECS → Jolt (кинематические тела)
- **PostUpdate**: Синхронизация Jolt → ECS (динамические тела)
- **Cleanup**: Уничтожение физических тел при удалении сущностей

### Управление жизненным циклом

- Автоматическое создание тел при добавлении компонентов
- Удаление тел при удалении сущностей
- Синхронизация трансформаций между системами

## Краткий пример

```cpp
// Создание сущности с физикой
flecs::entity player = world.entity()
    .set<JoltBody>({player_body_id})
    .set<Transform>({position, rotation})
    .set<PhysicsProperties>({mass, friction, restitution});

// Система синхронизации физики
world.system<Position, Rotation, const JoltBody>("SyncPhysicsToECS")
    .kind(flecs::PostUpdate)
    .each([physics](Position& p, Rotation& r, const JoltBody& b) {
        // Обновить трансформацию из физики
    });
```

## Быстрые ссылки

- [projectv-integration.md](projectv-integration.md) — полная интеграция JoltPhysics с ProjectV
- [Основные понятия](concepts.md) — архитектурные концепции JoltPhysics
- [Быстрый старт](quickstart.md) — минимальные примеры использования
- [Интеграция](integration.md) — общая интеграция JoltPhysics в проекты

← [На главную документации](../README.md)