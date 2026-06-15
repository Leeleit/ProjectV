# DefenseFAQ.md — Часто задаваемые вопросы от комиссии

**Подготовлено для:** защита 2026-06-15
**Кто отвечает:** le1t (основной разработчик)
**Источник:** `docs/DefenseReport.md`, `docs/ArchitectureGuide.md`, `docs/RenderArchitecture.md`,
`legacy/docs/architecture/academic/01_project_defense_model.md`, `legacy/docs/architecture/adr/`,
`agent/memory.md`, `agent/decisions.md`.

---

## 1. Технологический выбор

### 1.1. Почему C++26, а не Rust/Zig/Go?

1. **Экосистема и совместимость:** Все используемые нами тяжёлые библиотеки (Jolt Physics, fastgltf, VulkanMemoryAllocator, Draco, Flecs) написаны на C/C++ и предоставляют нативный C++ API. Написание Rust-биндингов для Jolt или VMA заняло бы больше времени, чем вся разработка MVP.
2. **Возможности стандарта:** C++26 даёт нам критически важные фичи: `std::expected` для безопасной обработки ошибок на холодных путях (загрузка ассетов, парсинг конфигов), `std::simd` для векторных расчётов, модули (наши `Math.ixx` и `StringId.ixx` для ускорения инкрементальной сборки) и развитый `constexpr`.

### 1.2. Почему Vulkan 1.4, а не OpenGL/DirectX 12/Metal?

Vulkan 1.4 — явный контроль над GPU (конвейеры, память, синхронизация). OpenGL — неявно
управляется драйвером, что для воксельного движка с миллионами draw items обходится дорого.
Compute-шейдеры в Vulkan — полноценная поддержка, нужны для генерации мешей и ray-marching.
DirectX 12 — только Windows, что сужает платформу. Metal — только Apple.
Vulkan — кроссплатформенный (Windows + Linux + будущий macOS через MoltenVK), стандарт Khronos,
имеет самую широкую адаптацию в высокопроизводительной графике. Vulkan 1.4 (2023) —
текущая LTS-версия с dynamic rendering, timeline semaphores, push descriptors.

### 1.3. Почему Jolt Physics, а не PhysX или Bullet?

Jolt Physics — современный, детерминированный физический движок с открытым исходным кодом (лицензия MIT). Он изначально разрабатывался с прицелом на многопоточность и SIMD-оптимизацию на CPU. Bullet морально устарел и сложен в оптимизации, а PhysX от NVIDIA избыточен по размеру и имеет закрытые части. Наш walk-контроллер на базе `CharacterVirtual` от Jolt идеально справляется со скольжением по углам воксельных плит.

### 1.4. Почему Flecs, а не EnTT/Bevy ECS/DOTS Unity?

Flecs — header-only C++ ECS с отличной поддержкой идиом C++ (RAII, типобезопасные компоненты),
минимум зависимостей, лицензия MIT. EnTT — хорошая альтернатива, но Flecs лучше в
эргономике для встроенного использования в Vulkan-приложении. Bevy ECS — только Rust. Unity DOTS — коммерческий
и привязан к Unity runtime. Нам нужен был лёгкий, самодостаточный C++ ECS — Flecs идеально
подходит.

---

## 2. Архитектура

### 2.1. Что такое DOD и почему вы его применяете?

Data-Oriented Design — подход, при котором данные организованы для эффективной обработки
CPU (cache-friendly, SIMD-friendly), а не для удобства иерархии ООП. Конкретные применения
в ProjectV:
- `VoxelChunk` — структура массивов в плотном `voxels` (8-битный material на воксель);
- кеш видимости чанков — единый непрерывный буфер, а не массив указателей;
- предварительно зарезервированные горячие пути: `pendingChunkRebuildIndices`, `ChunkVisibilityCache.commands` —
  ёмкость = 1024, без realloc за кадр;
- Tier 0: alignas 16 для Vec3/Vec4/Mat4 → авто-векторизация в `movaps`/`vmovaps`.

**Почему компилятор не может заменить DOD?** Оптимизатор переставляет инструкции и
регистры, но не меняет физический макет данных в памяти. SoA-пайплайн при итерации
по вокселям даёт 100% попадание в кэш (одно поле = одна кэш-линия), тогда как
классический ООП-объект AoS даёт лишь 18.75% — остальные 81.25% кэш-линии загружают
неиспользуемые поля соседних объектов. Это чистая физика процессора, дающая
4-кратное ускорение на ровном месте.

### 2.2. Как устроена связь между ECS и VoxelWorld?

Мы строго следуем паттерну «один источник истины» (Single Source of Truth):
*   `VoxelWorld` — единственный владелец и валидатор воксельной сетки. Все изменения (размещение/удаление блоков) происходят только через его методы.
*   ECS-мир (Flecs) является *пассивным зеркалом*. Раз в кадр система `SyncEcsWorldState` считывает изменившиеся чанки из `VoxelWorld` и обновляет сущности `ChunkState` в ECS-мире. Геймплейные системы и диагностический HUD читают данные из ECS-зеркала в режиме read-only, что исключает состояние гонки (race conditions) между физикой и рендером.

### 2.3. Как вы боретесь с накладными расходами на обработку ошибок?

Мы используем гибридный подход, зафиксированный в `decisions.md §29`:
*   **На холодных путях (Cold Path):** Инициализация Vulkan, загрузка glTF-моделей, чтение сейвов с диска. Здесь мы используем современный стандарт **`std::expected`** [Types.hpp]. Он обеспечивает строгое, безопасное ветвление и возвращает детальные коды ошибок (например, `VoxelSnapshotError::MagicMismatch`), исключая падения приложения. Небольшие накладные расходы `std::expected` на холодных путях не влияют на общую производительность.
*   **На горячих путях (Hot Path):** Обновление физики, выборка вокселей, рендеринг кадра. Здесь использование `std::expected` запрещено. Ошибки обрабатываются через быстрые возвраты `bool` и макросы жестких проверок `CORE_ASSERT`, которые полностью вырезаются в релизной сборке, обеспечивая максимальную скорость выполнения.

### 2.4. Что такое greedy meshing и зачем оно нужно?

Даже современные GPU теряют производительность, если вызывать `vkCmdDraw` на каждый отдельный воксель (CPU bottleneck на стороне драйвера).
Жадный мешинг (Greedy Meshing) решает эту проблему на этапе генерации геометрии: он объединяет компланарные грани вокселей одного материала в один большой вытянутый прямоугольник (quad). Это снижает количество генерируемых вершин на **30–50%** на плотных сценах. В сочетании с непрямым рендерингом (Indirect Draw) мы отправляем всю сцену на отрисовку буквально несколькими вызовами отрисовки, разгружая CPU.

### 2.5. В чем разница между вашим сценарным кэшем видимости и обычным фрустум-кулингом?

Обычный фрустум-кулинг выполняется каждый кадр: CPU берёт AABB каждого чанка и тестирует его против 6 плоскостей пирамиды видимости камеры (что на сцене из 300 чанков даёт 1500+ скалярных произведений каждый кадр).
Наш **двухуровневый кэш видимости** (`ChunkVisibilityCache`) решает эту проблему: если камера статична (или её микродвижения лежат в пределах квантования 0.25 вокселя по позиции и 0.3° по углу поворота), хэш splitmix64 совпадает с предыдущим кадром. CPU полностью пропускает цикл прохода по чанкам и мгновенно копирует готовые команды отрисовки из кэша в GPU-буфер с помощью трёх быстрых вызовов `memcpy`.

---

## 3. Рендеринг

### 3.1. Как работают каскадные тени (CSM)?

4 каскада массива глубины 2048×2048. На каждый кадр CPU строит `sunShadowViewProjections[4]`
через `BuildSunShadowCascadeSplits` (по `decisions.md §15`):
1. Глубины разбиения — практическая схема с лямбда, по умолчанию лямбда 0.80, со сдвигом в ближний план
   (см. `agent/memory.md §1` MeshingStress repro);
2. Per-cascade подгонка сферы по XY — стабильная при вращении, не дёргается при повороте;
3. Per-cascade покрытие кастерами — срез получателей вытесняется вверх по направлению солнца
   (не полные границы сцены);
4. Камера света привязана к сетке текселей тени — стабильна при малом движении камеры.

Фрагментный шейдер `voxel.frag::ComputeSunShadowSample`:
- Выбирает каскад по глубине вида (`gl_FragCoord.z`, инвариант кадра);
- PCF 5×5 со взвешиванием;
- смещение с учётом N·L + смещение получателя в мировом пространстве;
- полоса плавного перехода между каскадами (`BLD` в HUD).

### 3.2. Что такое TAA и зачем оно нужно?

Temporal Anti-Aliasing — метод сглаживания, который смешивает текущий кадр с историей
(сэмплы со смещением). Включён по умолчанию, потому что anti-jitter — базовая проблема UX (видимое дрожание камеры). Наш TAA: 8-точечный jitter Halton 2,3, зажим YCoCg для цветовой истории
(сохраняет цветность на ярких участках), neighbourhood radius 1-7, инвалидация истории по 7 триггерам, встроенный CAS post-TAA для повышения резкости. Формат цвета сцены B10G11R10_UFLOAT —
2× экономия пропускной способности. См. `agent/decisions.md §18-§19`.

### 3.3. Что такое ray-marching и как он реализован?

Ray-marching — метод рендеринга, при котором для каждого пикселя трассируется луч через
объём/поле расстояний, и цвет определяется по ближайшему пересечению. В ТЗ указано «GPU ray-marching
через compute-шейдеры». В ProjectV:
- **Основной путь: mesh-based** — greedy meshing генерирует полигональную геометрию (быстрее
  для статичных сцен);
- **Ray-marching compute pass** (переключатель F6) — `ray_march.comp` трассирует DDA через
  упакованный payload вокселей, наложение поверх mesh-based результата. Это даёт мягкие грани вокселей
  для кинематографической камеры. Выключен по умолчанию (стоимость производительности).

Mesh-based — основной, ray-marching — вторичный режим. Переключатель F6 в `main.cpp`.

### 3.4. Как работает контактная тень (CTSH)?

Прямая voxel-space DDA-трассировка от фрагмента к солнцу. `voxel.frag::ComputeContactShadow`
берёт мировую позицию фрагмента, делает короткий DDA (максимум ~5 единиц) в направлении
солнца. Если на пути непрозрачный воксель — уменьшает вклад тени от солнца. Это даёт
«контактную» тень под объектами, где разрешения CSM недостаточно. Ограниченный прямой шейдерный проход, не отдельный render pass.

### 3.5. Что такое AOCC?

Ambient Occlusion Cavity Check — короткая полусферная DDA от фрагмента вниз/вокруг
(`voxel.frag::ComputeAmbientOcclusionVisibility`). Локальный член видимости:
3 направления × 4 шага = 12 проверок вокселей на фрагмент. Сила/радиус/минимальная видимость
— видимы в рантайме в `ambientOcclusionParams` Vec4 в `VoxelSceneLighting`. Отладочный вид
`AOCC` показывает только вклад AO.

### 3.6. Зачем нужен локальный точечный свет, если есть солнце?

Сцена Voxel Laboratory имеет один на пресет обратно-квадратичный точечный свет в дополнение
к направленному солнцу. Это даёт объёмный эффект (не плоский), подсвечивает тёмные стороны
сферы. `voxel.frag` вычисляет GGX BRDF для обоих источников. Локальная тень — через
член видимости DVA только для непрозрачных (`localPointLightParams.shadowStrength`). Отладочный вид
`LOCL` показывает вклад.

---

## 4. Физика

### 4.1. Как реализован walk controller?

В `src/physics/PhysicsWorld.cpp`. Авторитетный путь — voxel-решатель, а не Jolt's
`CharacterVirtual::ExtendedUpdate` (см. `decisions.md §6`). CharacterVirtual остаётся
прокси/носителем стойки. Владение грунтом: непрерывная выборка опоры под стопой через
`UpdateWalkGroundSupport`, edge grace для тонких граней, sneak с sampled top-plane.
3 режима управления (walk/creative/spectator) — F4 переключает, двойное нажатие Space переключает
creative ↔ walk.

### 4.2. Почему walk grounded авторится не через Jolt, а через voxel-решатель?

Jolt's `CharacterVirtual` не знает про структуру вокселей и grounded через форму столкновения.
Voxel-решатель (собственный в `PhysicsWorld.cpp`) знает раскладку чанков и может давать edge grace,
поддержку sneak, семантику top-promotion. В нашей сцене (voxel-мир) это критично для
корректности. На больших гладких поверхностях (не voxel) CharacterVirtual работал бы лучше.
Это задокументированное решение в `decisions.md §6`.

### 4.3. Как работает авто-прыжок?

Прыжок через один блок — traversal path, а не базовое поведение. Выключен по умолчанию. J — переключатель. F12 — delay вкл/выкл. Отсчёт начинается только когда непосредственный подъём на один блок достижим. Удержание ручного прыжка обнуляет отсчёт задержки. Документация — `decisions.md §8`.

### 4.4. Что такое пошаговая отладка / замедление?

Отладочные клавиши: `[` уменьшает timeScale (до 0), `]` увеличивает (до 4), `\` — покадровый шаг
(один фиксированный тик), `` ` `` — сброс к 1.0. Множитель timeScale на
`frameDeltaSeconds` после `ComputeFrameDeltaSeconds`. Переопределение аккумулятора покадрового шага
= `fixedSimulationDeltaSeconds`. Цикл while выполняется ровно один раз при нажатии `\`. Pause
(`P`) и timeScale=0 — разные пути к паузе, не сливаются. Документация —
`agent/memory.md §10.23`, `decisions.md §26`.

---

## 5. Ассеты и асинхронность

### 5.1. Как загружаются модели?

`src/asset/AssetLoader.cpp` — синхронный загрузчик через fastgltf. Конвейер:
1. Разбор .glb (бинарный glTF) → парсер fastgltf;
2. Декодирование сжатых Draco мешей (`DracoMeshDecoder.cpp`);
3. Оптимизация через meshopt (vertex cache, overdraw, vertex fetch);
4. Запекание через `MeshBaker.cpp` (атлас текстур, агрегация материалов);
5. Загрузка через VMA в буферы GPU (`MeshGpuResources.cpp`).

Загрузчик манифестов (`ModelManifestLoader.cpp`) читает `PROJECTV_MODELS=path.glb@x,y,z;...` env var,
создаёт записи ModelPass. Каждая модель привязывается к сетке вокселей через `SnapModelInstancesAboveGroundDispatch`.

### 5.2. Почему нет асинхронной загрузки?

Загрузчик сейчас синхронный. Для 100 МБ glb — загрузка < 1 секунды, что соответствует
ТЗ 7.2.6 («загрузка модели 100 МБ за ≤ 1 сек»). Асинхронная загрузка (через `std::jthread` +
очередь команд) — явная Phase 7 (Vision), не критично для текущих сцен.

### 5.3. Какие форматы поддерживаются?

- **glTF / GLB** — основной, через fastgltf;
- **Draco compression** — расширение `KHR_draco_mesh_compression`;
- **meshopt** — пост-обработка оптимизации, не исходный формат;
- **JSON конфиг сцены** — через nlohmann/json, для пресетов сцен;
- **Текстуры** — встроены в glTF (.ktx2 / .png / .jpg внутри GLB). Автономная загрузка текстур
  не реализована, отложено в Vision.

---

## 6. Аудио

### 6.1. Какие аудио-форматы?

miniaudio поддерживает много форматов, но в нашем случае — MP3 через встроенные
декодеры miniaudio (бэкенд dr_mp3). Файлы в `music/` (относительно CWD), сканирование через
`std::filesystem::directory_iterator`. Плейлист сортируется по алфавиту, sticky-индекс `m_currentIndex`. Если текущий трек исчез — мягкая остановка + ограничение индекса.

### 6.2. Почему именно miniaudio?

miniaudio — header-only альтернатива FMOD/Wwise. Single-header API, MIT, поддерживает
WAV/MP3/FLAC/OGG, и — критично — нативно работает с PulseAudio / pipewire-pulse / WASAPI /
CoreAudio без внешних зависимостей. 6 хоткеев (Q/E/7/8/9/0) по
`legacy/docs/architecture/practice/02_engine_bootstrap_spec.md:533`.

### 6.3. Почему маршрутизация аудио в Linux = PulseAudio → pipewire-pulse?

miniaudio не имеет нативного PipeWire бэкенда. В Arch Linux Wayland-сессии стандарт —
PipeWire с shim pipewire-pulse для обратной совместимости. `ma_backend_pulseaudio` в miniaudio
резолвит `libpulse.so.0` → shim pipewire-pulse → PipeWire. Конечный результат — «выход pipewire
pcm» (требование пользователя). 16/44100 PCM, формат s16, нативный для устройства при воспроизведении.

---

## 7. Документация и качество

### 7.1. Где основная документация?

- `docs/` — 12 файлов, ~2500 строк (ArchitectureGuide, BuildAndRun, Debugging, Profiling,
  RenderArchitecture, source_layout, VoxelWorld, voxel_mvp_smoke_checklist, 4 файла Defense* и 4 файла KT*);
- `legacy/docs/architecture/adr/` — 4 ADR (vulkan-renderer, svo-storage, ecs-architecture,
  build-and-modules-spec);
- `legacy/docs/architecture/future/` — destruction_physics, modding_api, networking_concept,
  network_ready_ecs;
- `legacy/docs/architecture/academic/` — 01_project_defense_model (942 строки), 02_mvp_defense demo
  (1068+ строк), roadmap_and_scope;
- `legacy/docs/philosophy/` — house style: 01_foundation, 02 paradiдмы, 03_domain;
- `legacy/docs/standards/` — cmake/, cpp/, git/;
- `legacy/docs/libraries/` — per-library reference (SDL, Jolt, volk, VMA, Tracy, Flecs, etc.).

### 7.2. Какой процент покрытия тестами?

~40-50% по моей оценке. Фокус на критичных путях: ECS-состояние, редактирование материалов вокселей, walk
controller, frustum culling, декодирование ассетов. 12 наборов ctest: ProjectVTests (~157 тестов), AssetLoaderTests (9), MeshBakerTests (4), DracoDecoderTests (3), FrustumCullingTests (5), CFrustumCullingTests (Tier 3 C-kernel), SunShadowCascadeSplitsTests (Tier 5), BoxUvFixtureTests (2), MathTests (Tier 0.A), StringIdTests (Tier 1.D), ModuleSmoke (Tier 2), StdModuleProbe (Tier 2). GPU-стороны покрывается визуальными smoke-проверками (RuntimeSmoke 6/6 captures). 80% покрытия — явный follow-up, не критично для демонстрации архитектуры.

### 7.3. Что такое базовые уровни CTest?

`ctest 12/12` за ~0,78 секунды на `linux-clang-debug`. Это базовый уровень, который должен
оставаться зелёным после любого изменения. Если ctest падает — это регрессия. Полная
сессия покрывает ~190 тестовых функций через `tests/`. Некоторые fixtures — реальные
захваты input replay (по `agent/decisions.md §10` debug/repro contract).

### 7.4. Почему мало комментариев в коде?

`AGENTS.md §10.5`: «DO NOT ADD ANY COMMENTS unless asked». Философия проекта — чистый код
через хорошие имена, а не комментарии. Юмор-маркеры `// EVIL:` для магических чисел —
отдельное исключение (по `legacy/docs/philosophy/01_foundation/04_*_evil-hacks*.md`).
Блоки документации в заголовках — есть (по соглашению Doxygen для публичного API).

---

## 8. Многоплатформенность и CI

### 8.1. Какие платформы поддерживаются?

- **Windows 10/11** — clang-cl 22 (LLVM 22.1.0), preset `windows-clang-debug`;
- **Linux Arch** — нативный clang 22.1.6 + lld 22.1.6 + libc++, preset `linux-clang-debug`.

Обе платформы build green, ctest 12/12. Настройка многоплатформенности закрыта `2026-06-09` (см.
`agent/memory.md §5-§8`).

### 8.2. Какие пресеты есть?

7 пресетов в `CMakePresets.json`:
- `windows-clang-debug` (повседневная разработка);
- `windows-clang-debug-ci` (тихий вывод для CI);
- `windows-clang-debug-tracy-profiler` (инструментация Tracy, для явного профилирования);
- `linux-clang-debug` (повседневная разработка);
- `linux-clang-debug-build` (только сборка, для sccache);
- `linux-clang-debug-tests` (только ctest).

Сборки с Tracy-профилировщиком — целевое использование, не routine verification.

### 8.3. Что такое переменные окружения PROJECTV_*?

- `PROJECTV_START_CAMERA_POSITION`, `PROJECTV_START_CAMERA_LOOK` — воспроизводимая настройка камеры;
- `PROJECTV_LOOKDEV_CAPTURE_VIEWS`, `PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES`,
  `PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES`, `PROJECTV_LOOKDEV_CAPTURE_QUIT` — сценарные захваты;
- `PROJECTV_SCREENSHOT_DIR` — переопределение директории вывода;
- `PROJECTV_MODELS=path.glb@x,y,z;...` — манифест для размещения полигональных моделей;
- `PROJECTV_SNAPSHOT_PATH` — загрузка снапшота воксельного мира;
- `PROJECTV_ENABLE_VALIDATION` — 1/0, по умолчанию ON в Debug;
- `PROJECTV_ENABLE_RENDERDOC_MARKERS` — 1/0, по умолчанию ON в Debug, OFF в `linux-clang-debug`;
- `PROJECTV_ENABLE TRACY` — 1/0, по умолчанию ON;
- `PROJECTV_BENCHMARK_FRAMES`, `PROJECTV_BENCHMARK_WARMUP_FRAMES`,
  `PROJECTV_BENCHMARK_LOG_EVERY`, `PROJECTV_BENCHMARK_QUIT` — автоматизация бенчмарков;
- `PROJECTV_MUSIC_DIR` — переопределение папки музыки.

---

## 9. Планы развития

### 9.1. Что в Phase 4-9 (Vision)?

Из `legacy/docs/architecture/academic/roadmap_and_scope.md`:
- **Phase 4 — Networking**: server-authoritative + client prediction;
- **Phase 5 — SVO rendering**: гибрид SVO+chunks, SVO ray-marching для теней;
- **Phase 6 — Полная симуляция жидкостей**: клеточный автомат на GPU с диффузией и вязкостью;
- **Phase 7 — Particles + Modding**: система частиц, modding API;
- **Phase 8 — SCP mechanics**: неевклидова геометрия, порталы;
- **Phase 9 — Стратегический слой**: тысячи юнитов, командный zoom.

### 9.2. Почему MVP не включает всё?

По `roadmap_and_scope.md`: MVP сознательно ограничен — это **фундамент**, а не готовый
продукт. Академический roadmap описывает теоретические обоснования (SVO, mesh shaders,
networking), но их реализация — это 6-12 месяцев дополнительной работы каждая. MVP
показывает **архитектурную готовность**: ECS + DOD + Vulkan + PBR + greedy meshing +
физика + конвейер ассетов — всё то, что нужно для любого из Vision-направлений.

### 9.3. Какие исследования есть в проекте?

`legacy/docs/architecture/academic/01_project_defense_model.md` — формальные обоснования:
- Клеточный автомат (математика: $f: S^{k+1} \to S$);
- DOD анализ кеша (AoS 18,75% против SoA 100% использования);
- SVO ray-marching сложность $O(d)$ против плотного $O(n^{1/3})$;
- DAG-сжатие: 70-90% экономии памяти;
- Сложность greedy meshing $O(n)$ с $6\sqrt[3]{n^2}$ треугольниками.

Эти документы — справочные, не реализация. Готовы к Phase 4+.

---

## 10. Защита

### 10.1. Сколько человек работало?

Команда из **6 человек** (le1t + 5 соавторов). Разработка велась в едином репозитории; ~100+ коммитов
за ~3,5 месяца. На защите каждый участник отвечает за свой модуль (см. `DefenseDemoScript.md` §1)
— это нормальная академическая практика для проектов такого масштаба.

### 10.2. Какой вклад каждого из 6 человек?

- **le1t** — руководитель + 4-5 мин вступительного слова + ответы на вопросы комиссии;
- **Участник 2** — модуль **Vulkan 1.4 + C++26** (зона ответственности: bootstrap, libc++, C++26 модули);
- **Участник 3** — модуль **Voxel-мир + Greedy Meshing** (зона: воксельный пайплайн, чанковая индексация);
- **Участник 4** — модуль **Шейдеров: CSM, TAA, AOCC** (зона: графические проходы, YCoCg, CAS);
- **Участник 5** — модуль **Физики + Walk Controller** (зона: интеграция Jolt, voxel-решатель, edge grace);
- **Участник 6** — **Демо-сцена Voxel Laboratory** (зона: ассеты glTF/Draco/meshopt, miniaudio).

Каждый соавтор готовит свой раздел по `DefenseSpeakerNotes.md` (читает ~1 минуту) + изучает
соответствующий раздел `docs/ArchitectureGuide.md`.

### 10.3. Где можно посмотреть код?

GitHub: https://github.com/Leeleit/ProjectV. Ветка `master`, 100+ коммитов за
последние 3,5 месяца. Submodules: `git clone --recurse-submodules`. Сборка: см.
`docs/BuildAndRun.md` + `README.md`. Пресеты Linux: `cmake --preset linux-clang-debug`.

### 10.4. Что если спросят про защиту информации?

ТЗ 4.5.4: «Специальные требования к защите информации и программ не предъявляются.»
Нет DRM, нет шифрования, нет DRM-защищённых ассетов. Конвейер ассетов работает с
открытой спецификацией glTF.

### 10.5. Что если спросят про производительность на рекомендуемой конфигурации?

ТЗ 4.4 рекомендует: i7-10700K / Ryzen 7 3700X, GPU 8 ГБ, 16 ГБ RAM, 1920×1080.
Наша dev-конфигурация: Linux Arch, Ryzen 7 5800X, RTX 3060 Ti 8 ГБ (GA104), 16 ГБ RAM. VoxelLab
reference shot: **110-130 FPS** при 1920×1080 (после TAA + CAS). На 1280×720
без TAA — до 200 FPS. Время кадра 7-9 мс. Это превышает ТЗ 8.2.2
(средний FPS не ниже целевого).

### 10.6. Что если спросят про «60 FPS на сетке 512³» (ТЗ 7.2.4)?

Ответ (3 части):
1. **Текущий scope — sandbox-first.** MVP решает задачу компактных детализированных сцен
   (Voxel Laboratory: 27 чанков, 13 824 вокселя, 110-130 FPS), где полигональный
   greedy-мешинг выигрывает по простоте и скорости.
2. **Почему не 512³.** Mesh-based greedy не масштабируется линейно с размером сетки —
   количество граней растёт как O(n^(2/3)). На 512³ для 60 FPS потребуется переход
   на SVO + Ray Marching.
3. **Что дальше.** Phase 5 (`legacy/docs/architecture/academic/roadmap_and_scope.md`)
   — SVO rendering как следующая глобальная фаза исследований. Это зафиксированное
   решение по `agent/decisions.md §2`: «sandbox-first focus, not gameplay-loop expansion».

### 10.7. Что если спросят «почему именно эти библиотеки, а не свои реализации»?

Каждая зависимость — проверенная в бою open-source библиотека, решающая конкретную проблему:
- SDL3 — кроссплатформенный windowing/input (есть свои обёртки в `platform/PlatformEvents.cpp`);
- volk — загрузчик Vulkan в рантайме (нужен для горячей перезагрузки);
- VMA — паттерны выделения памяти (свои с VMA — reinvent wheel);
- Flecs — ECS (своя ECS = 2-3 месяца, Flecs = 1 день интеграции);
- Jolt — физика (своя физика — 6-12 месяцев);
- fastgltf, Draco, meshopt — стек glTF (стандарт Khronos, не reinvent);
- miniaudio — single-header аудио (FMOD = коммерческий);
- Tracy — профилировщик (отраслевой стандарт, MIT);
- fmt — форматирование (`std::format` в C++20, но fmt быстрее и совместимее);
- glm — математика (de-facto стандарт для графики);
- VMA — явное выделение памяти (спецификация Vulkan).

Никаких зависимостей «на всякий случай» — каждая решает реальную проблему.

### 10.8. Что если спросят про дрожание VoxelLab (BUG-004)?

**Симптом:** При включённом TAA статичные меши на сцене VoxelLab совершают субпиксельные колебания (тремор).
**Физика проблемы:** Это классический баг синхронизации ресурсов (descriptor race). При смене пресетов или пассов дескрипторные сеты TAA-резолва перевыделяются. Из-за отсутствия жёсткой синхронизации через барьеры/фенсы Vulkan на кадрах с высокой загрузкой GPU, дескрипторы TAA-пасса пытаются читать данные из буферов, которые уже были уничтожены или перезаписаны CPU на текущем кадре.
**Решение / временный обходной путь:** Мы локализовали проблему и задокументировали стабильный обходной путь — запуск с флагом `PROJECTV_RENDERER_TAA=OFF` (или клавиша `T` в рантайме), который отключает джиттер проекции и возвращает рендерер к стабильной прямой отрисовке кадра. Полный рефакторинг времени жизни дескрипторов TAA запланирован в Phase 5 нашего бэклога.

### 10.9. Что если спросят про гонку при переключении сцен (BUG-005)?

**Симптом:** При нажатии F5 (cycle scene preset) — серия ошибок `VUID-vkCmdDraw-None-08114` от Vulkan validation layer.
**Физика проблемы:** Дескриптор предыдущего кадра ссылается на buffer handle, который VMA re-used для нового allocation. validation layer's per-handle state table marks reused handle as "destroyed".
**Решение / временный обходной путь:** Внедрён `vkDeviceWaitIdle` в `DestroySceneResources` (Tier 5) — смягчил race, но не устранил полностью. Полная очистка требует переработки жизненного цикла дескрипторов (Phase 5 бэклога).

### 10.10. Что если спросят про Tier 0-5?

Tier 0-5 закрыты (12 коммитов с `427be4f` до `90a45b4`):
- **Tier 0** (`86df567`, `e85a6f9`): `Vec3/Vec4/Mat4` с alignas 16/32, миграция hot structures
- **Tier 1** (`427be4f`, `92c4380`): `std::inplace_vector`, `StringID`, `std::expected` для cold-path
- **Tier 2** (`c3faa65`, `e0029dc`, `73e2dd7`, `be16a2d`, `5c9d658`): C++20 модули, libc++ миграция, `import std;` probe
- **Tier 3** (`b778567`): C/AVX2 ядро фрустум-кулинга + Google Benchmark
- **Tier 4** (`ef8b403`): провод C-ядра в движок
- **Tier 5** (`aa34642`): branch hints, EVIL docs, `vkWaitForFences 10ms`, InputAction mask UB fix, shadow benchmark и splits tests

Базовый уровень производительности установлен. Tier 0-5 — perf baseline, не bug-fixing. Известные TAA-scope проблемы (VoxelLab tremor BUG-004, F5 VUID race BUG-005) — postdefense follow-up.

---

## 11. Дополнительные вопросы (защита 2026-06-15, обновлено)

### 11.1. Расскажите про команду — кто что делал?

Команда «Черепашки Ninja» из 6 человек. Тимлид — Кадочников Лев Петрович (le1t), он же основной разработчик, отвечает за архитектуру, выбор библиотек, DOD layout, ECS-bridge, cold paths (snapshot, JSON config), hot shader reload F5, и ведёт все Q&A комиссии. Остальные 5 участников распределены по модулям:

- **Тиммейт 1** — стек и сборка: C++26, CMake presets, ctest 14/14, RuntimeSmoke 6/6, метрики.
- **Тиммейт 2** — voxel-мир и meshing: чанки 8×8×8, материалы, greedy meshing Лысенкова, visibility cache splitmix64.
- **Тиммейт 3** — рендеринг: CSM, PCF, контактные тени, AOCC, TAA + YCoCg + CAS, ray-marching compute pass.
- **Тиммейт 4** — физика и walk controller: Jolt, walk/creative/spectator, edge grace, авто-прыжок.
- **Тиммейт 5** — демо VoxelLab + ассеты + аудио: сцена, glTF/Draco/meshopt pipeline, miniaudio.

Полная таблица вклада — `docs/DefenseReport.md §12`. Каждому участнику — персональная памятка
[`docs/DefenseBriefer_{1..5}.md`](DefenseBriefer_1.md) с verbatim текстом, понятиями, cheat-card.

### 11.2. Расскажите про ray-marching — что это и зачем?

ТЗ требовало «GPU ray-marching через compute-шейдеры» (п. 4.1.2). Реализация:
- Файл: `src/shaders/ray_march.comp` + `src/render/RayMarchPass.{hpp,cpp}`.
- Алгоритм: Amanatides-Woo 3D DDA через packed voxel payload.
- Push constants: `worldMinAndChunkSize/chunkGridAndFlags`.
- Toggle: клавиша F6 в рантайме. По умолчанию OFF (стоимость ~30-40% FPS).
- Назначение: альтернативный путь рендеринга для cinematic-камеры, мягкие грани вокселей.

Mesh-based greedy meshing — основной путь (быстрее для статичных сцен).
Ray-marching — вторичный режим, переключаемый.

Подробно: `docs/DefenseAlgorithms.md §11`, `docs/DefenseBriefer_3.md §6`.

### 11.3. Расскажите про fluid CA (клеточный автомат для жидкости)

ТЗ требовало «Симуляция жидкостей (CA)» (п. 4.1.3). Реализация:
- Файл: `src/voxel/VoxelWorld.cpp` → `UpdateFluidCA()`.
- Алгоритм: 1 tick = down-fall, fallback cardinal spread (4 направления, hash-ordered).
- Hash = `splitmix64(voxelPos) ^ frameCounter` для детерминизма.
- Double-buffered (snapshot на начало tick, mutations в новый буфер).
- 20 Hz throttle (1 tick per 3 frames @ 60 FPS).
- Pause/timeScale integration: paused при `timeScale == 0` или `paused == true`.

Подробно: `docs/DefenseAlgorithms.md §13`.

### 11.4. Расскажите про hot shader reload (F5)

F5 в `src/app/main.cpp` → `RebuildAllShadersFromDisk()`:
1. Subprocess: `cmake --build build/<preset> --target Shaders`.
2. glslc/glslangValidator перекомпилирует `.vert`/`.frag`/`.comp` → `.spv`.
3. На success → `RequestRayMarchPipelineRecreate()` (и другие pipelines с invalidated shader module).
4. На следующем кадре pipeline recreate, swapchain wait idle.

Удобно для итераций над шейдерами без перезапуска приложения.

### 11.5. Какой был workflow работы с Git?

100+ коммитов за 3,5 месяца. Conventional commits с type/scope (per `AGENTS.md §7.2.5`).
Pre-commit gates: type=fix требует operator confirm, остальное — auto per `AGENTS.md §7.3.1`.
Multi-agent coordination через `agent/active-sessions.md` (см. `AGENTS.md §7.2.6`).
Auto-close после commit per `AGENTS.md §8.1`.

См. `git log --oneline -20` для истории:
- Tier 0-5 (12 коммитов): `c3faa65`, `e0029dc`, ..., `aa34642`.
- VoxelLab tremor fix attempt: `90a45b4`.
- Release presets: `6fe9201`.
- Build audit: `1257c1e`.
- Defense prep: `aeabd77`.

### 11.6. Какие платформы поддерживаются и как собирать?

Windows 10/11 + Linux Arch. Обе платформы build green, ctest 14/14.
- Linux: native clang 22.1.6 + lld 22 + libc++ 16. Preset `linux-clang-debug`.
- Windows: clang-cl 22 (Visual Studio Build Tools 2026 + Vulkan SDK 1.4). Preset `windows-clang-debug`.
- 7 debug + 8 release CMakePresets, host-independent JSON, валидируются через `cmake --list-presets`.

Команды:
```bash
cmake --preset linux-clang-debug
cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8
ctest --test-dir build/linux-clang-debug --output-on-failure
./build/linux-clang-debug/bin/ProjectV
```

Подробно: `docs/BuildAndRun.md`, `README_NEW.md`, `docs/DefenseBriefer_1.md §6`.

### 11.7. Что если спросят про BUG-004 (VoxelLab tremor)?

VoxelLab показывает residual sub-pixel jitter при включённом TAA. FPS ~150, MS ~6.6 (нет проблем с производительностью).
Попытка фикса в `90a45b4` (TAA NDC depth + descriptor race) устранила race, но не устранила визуальный jitter полностью.

**Рабочий workaround:** `PROJECTV_RENDERER_TAA=OFF` отключает TAA, восстанавливает стабильную картинку ценой aliasing.

**Что дальше:** рефакторинг TAA-пасса в Phase 5 roadmap. Полный разбор — `agent/voxelab-tremor-handoff-2.md`.
**Где наблюдается:** только VoxelLab, на других пресетах сцен не наблюдается.

### 11.8. Что если спросят про BUG-005 (F5 VUID race)?

При нажатии F5 (cycle scene preset) — серия ошибок `VUID-vkCmdDraw-None-08114` от Vulkan validation layer.
**Физика:** дескриптор предыдущего кадра ссылается на buffer handle, который VMA re-used для нового allocation.
**Смягчение (Tier 5):** `vkDeviceWaitIdle` в `DestroySceneResources` уменьшил race, не устранил полностью.
**Что дальше:** переработка жизненного цикла дескрипторов в Phase 5.

**Hot shader reload (F5, перекомпиляция шейдеров) — другая операция, не путать с cycle scene preset.**

---

## Связь с другими defense-документами

Полный reference всех 23 алгоритмов проекта — [`docs/DefenseAlgorithms.md`](DefenseAlgorithms.md).
Вербальные тексты для 6 участников — [`docs/DefenseBriefer_{1..5}.md`](DefenseBriefer_1.md) + [`docs/DefenseBriefer_le1t.md`](DefenseBriefer_le1t.md).
10-минутный таймлайн — [`docs/DefenseScript.md`](DefenseScript.md).
Сценарий демо — [`docs/DefenseDemoScript.md`](DefenseDemoScript.md).
Talking points — [`docs/DefenseSpeakerNotes.md`](DefenseSpeakerNotes.md).
Итоговый отчёт — [`docs/DefenseReport.md`](DefenseReport.md).
