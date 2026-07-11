# Что такое игровой движок

Документ описывает, что такое игровой движок, какие у него слои, какие
подсистемы обязательны, а какие — нет.

---

## Движок ≠ игра

**Игра** — контент: уровни, персонажи, сюжет, баланс, скрипты.

**Движок** — программа, которая этот контент интерпретирует в реальном
времени: загружает ассеты, симулирует мир, рендерит кадры, обрабатывает
ввод, играет звук.

Одно без другого не работает. Игру можно написать без движка, но это
долго и дорого. Движок без игры — фреймворк.

---

## Канонические слои движка

Эти слои сложились в индустрии к середине 2000-х и с тех пор
воспроизводятся от проекта к проекту.

### 1. Foundation (фундамент)

Низкоуровневые сервисы.

- **Память:** аллокаторы, контейнеры, строки, отладочные хуки.
- **Concurrency:** job system, fibers, lock-free структуры, async/await.
- **Конфигурация и логирование.**
- **Build system и dev tools.**

См. [14_compiler.md](14_compiler.md), [15_compile-time.md](15_compile-time.md),
[16_memory.md](16_memory.md), [17_error-handling.md](17_error-handling.md).

### 2. Resource Manager

Загрузка и выгрузка ассетов: текстуры, меши, шейдеры, звуки, скрипты.

Ключевая идея: ассеты живут дольше, чем кадр; их нужно кешировать,
стримить, версионировать.

См. [24_data-flow.md](24_data-flow.md), [25_strings.md](25_strings.md).

### 3. Game Object Model

Способ представить «всё, что существует в мире».

- **Object-Oriented:** классы с наследованием.
- **Component-Based:** объекты как контейнеры компонентов.
- **ECS (Entity Component System):** данные отделены от поведения.

См. [22_ecs.md](22_ecs.md), [21_dod.md](21_dod.md).

### 4. Renderer

Самая большая и самая ресурсоёмкая подсистема.

- **API:** OpenGL, DirectX 11/12, Vulkan, Metal, WebGPU.
- **Pipeline:** forward, deferred, clustered, meshlet-based.
- **Lighting:** forward+, clustered deferred, DDGI/VCT/RTX.
- **Post-processing:** tonemapping, AA, bloom, DOF.

См. [31_vulkan.md](31_vulkan.md), [32_voxel-data.md](32_voxel-data.md),
[30_optimization.md](30_optimization.md).

### 5. Physics & Simulation

Симуляция физики: коллизии, rigid body, soft body, fluid, разрушение.

- Внешние библиотеки: PhysX, Bullet, Jolt, Havok.
- Или собственная GPU-симуляция.

См. [23_concurrency.md](23_concurrency.md), [30_optimization.md](30_optimization.md).

### 6. Animation

Skeletal animation, blend trees, motion matching, IK.

### 7. Audio

2D/3D-звук, микширование, эффекты, стриминг.

### 8. AI

Pathfinding (A*, navmesh), decision making (behavior trees, GOAP,
utility systems, BT + blackboard), crowd simulation.

### 9. UI/HUD

2D-интерфейс: меню, HUD, диалоги, инвентарь.

### 10. Networking (опционально)

Для мультиплеера: client-server, rollback netcode, lag compensation,
replication.

### 11. Tools

Редактор уровней, asset browser, profiler, debug visualizer.

### 12. Scripting / Modding (опционально)

Lua, Python, AngelScript, Visual Scripting. Расширение движка без
перекомпиляции.

---

## Game Loop (главный цикл)

Каждый кадр движок проходит один и тот же цикл:

```
while (running) {
    1. Process input          // 1-2 мс, на GPU/ввод
    2. Update game state      // 5-10 мс, fixed timestep, мультипоточный
    3. Render frame           // 5-10 мс, GPU-bound
    4. Present                // vsync, swapchain
}
```

Шаг 2 критичен: см. [35_time-and-determinism.md](35_time-and-determinism.md)
(фиксированный шаг vs переменный).

---

## Subsystem ≠ Library

Движок — не набор библиотек. Это **интегрированная система**, где
каждая подсистема знает о других. Resource Manager знает про Renderer;
Renderer знает про Game Object Model; Physics знает про Game Object
Model.

Фреймворк — «используй меня». Движок — «я уже всё связал».

---

## Пример: реализация в ProjectV

В ProjectV реализованы: Foundation (C++26, libc++), Resource Manager,
ECS (flecs v4.1), Renderer (Vulkan 1.4), Physics (JoltPhysics v5.5),
UI (Dear ImGui), Profiling (Tracy 0.13.1), Audio (miniaudio). AI и
Networking — в roadmap.

---

## Источники и дальнейшее чтение

- **Jason Gregory — Game Engine Architecture**, 4ed (CRC Press, апрель
  2026, ISBN 9781041162599). Двухтомник, ~1100 стр.
  <https://www.gameenginebook.com/>
- **Game++ blog** (PVS-Studio, 2026) — серия «C++, game engines, and
  architectures».
  <https://pvs-studio.com/en/blog/posts/1361/>
- **AMD Game Engineering team — How do I become a graphics programmer?**
  <https://gpuopen.com/learn/how_do_you_become_a_graphics_programmer/>
- [10_manifesto.md](10_manifesto.md) — фундаментальные принципы.
- [22_ecs.md](22_ecs.md) — ECS.
- [31_vulkan.md](31_vulkan.md) — Vulkan.
- [35_time-and-determinism.md](35_time-and-determinism.md) — fixed step.