# Decisions

Краткий журнал решений, которые должны переживать отдельные сессии.

---

## 2026-04-07

### Источник истины для планирования

Решение:

- главным roadmap считается корневой `TODO.md`;
- `agent/` хранит постоянную память агента;
- legacy-планы больше не считаются главным планом реализации.

Почему:

- legacy-документы уже расходятся с реальным состоянием кода;
- проекту нужен один живой backlog, а не несколько конкурирующих.

### Главный путь проекта

Решение:

- mainline проекта — интерактивный graphics-based voxel MVP;
- большие эксперименты живут отдельно как R&D backlog.

Почему:

- проект уже имеет работающий vertical slice;
- ближайшая наибольшая ценность — не новый фундамент, а живая интерактивность и стабильная демонстрация.

### Обязательная постоянная память агента

Решение:

- в репозитории создаётся папка `agent/`, и агент обязан читать и обновлять её на каждой заметной сессии.

Почему:

- без этого агент будет терять контекст между задачами;
- статус проекта, инженерные решения и рабочий фокус должны переживать один ответ и одну сессию.

### Философия внедрения новых систем

Решение:

- новые зависимости, подсистемы и архитектурные слои добавляются только по факту практической пользы;
- философия проекта применяется прагматично, а не догматично.

Почему:

- проекту нужен живой MVP, а не коллекция заготовленных абстракций;
- даже хорошие идеи вредны, если появляются раньше своего времени.

### Интерактивное редактирование мира

Решение:

- интерактивный mainline использует отдельный CPU `VoxelRaycast` на плотном `VoxelWorld` и явный `InteractionState`;
- runtime remove/place block идёт напрямую через `SetVoxelMaterial`, без нового gameplay/framework слоя;
- один voxel edit помечает dirty регион `3x3x3`, чтобы rebuild гарантированно захватывал соседние чанки на границах.

Почему:

- это закрывает честный interactive slice без нового renderer/gameplay refactor;
- явный `InteractionState` сохраняет данные selection/placement для следующего шага с highlight/HUD;
- border-safe dirty marking важнее микроэкономии на этом этапе, потому что устраняет визуально неверные chunk rebuild'ы на границах.

### Visual feedback для interaction

Решение:

- block highlight и crosshair реализуются отдельным debug overlay pipeline поверх основного voxel pass;
- selection прокидывается в `FrameRenderData`, а overlay-геометрия генерируется в shader'е из `gl_VertexIndex`, без новых vertex/index buffers и без вмешательства в meshing path.

Почему:

- это даёт видимый feedback для picking, не трогая packed scene buffers и compute meshing;
- overlay path хорошо переживает swapchain recreate и остаётся изолированным от main voxel rendering;
- для mainline MVP это дешевле и безопаснее, чем встраивать selection-highlight в основной voxel material/render path.

### Debug HUD без `imgui`

Решение:

- debug HUD реализуется как фиксированный read-only overlay поверх существующего voxel/debug pass, без подключения `imgui`;
- текст HUD собирается на CPU в host-mapped vertex buffer из встроенного bitmap/stroke-подобного шрифта и рисуется отдельным graphics pipeline;
- HUD использует уже существующие `DebugStats`, `CameraState` и `InteractionState`, плюс hotkey toggle, вместо новой UI-системы.
- screen-space координаты HUD мапятся в Vulkan NDC с учётом обычного positive-height viewport, поэтому верх экрана соответствует отрицательному `Y` в NDC.

Почему:

- это закрывает ближайший mainline milestone из `TODO.md` без ввода новой тяжёлой зависимости;
- отдельный HUD pipeline не вмешивается в meshing path и легко переживает swapchain recreate;
- для текущего объёма UI фиксированный overlay проще, дешевле и честнее, чем тащить editor/debug framework раньше времени.
- это убирает перевёрнутый/bottom-anchored HUD без ввода negative viewport или отдельной матрицы для overlay.
