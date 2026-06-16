# Памятка Тиммейта 2 — Воксельный мир (говорит T3 Архитектура)

**Участник:** [Имя Тимейта 2]
**Слот на сцене:** 2:00–2:40 (40 секунд) — T3 Архитектура и качество кода
**Твоя реальная компетенция:** Воксельный мир (чанки, мешинг, fluid CA, snapshot)
**Что НЕ твоё (к кому перенаправлять в Q&A):** стек/демо — к le1t; вступление — к Тиммейту 1; рендеринг — к Тиммейту 3; физика — к Тиммейту 4; ассеты/аудио — к Тиммейту 5; все баги — к le1t

---

## 1. Шапка выступления

> «Здравствуйте. Меня зовут [Имя Тимейта 2], я расскажу про то, как устроен движок внутри.»

---

## 2. Что говорить дословно (~75-85 русских слов, 0:40)

> «Здравствуйте. Несколько слов о том, что внутри. Мир разбит на чанки 8 на 8 на 8, воксели лежат одним плоским массивом — это даёт кэш-дружелюбный доступ. Мешинг считает compute-шейдер на видеокарте: жадно склеивает соседние грани одного материала в четырёхугольники для производительности. Физика — библиотека Jolt, наш собственный код дополняет её для коллизий блоков. Для отладки данные дублируются в систему компонентов. В коде повсюду статик-ассерты: на этапе компиляции проверяют размеры структур и контракты алгоритмов, чтобы ничего не сдвигалось случайно. Передаю слово.»

---

## 3. Понятия (8 терминов, чтобы понимать что говоришь)

| Термин | Что это |
|---|---|
| Чанк (chunk) | Куб 8×8×8 = 512 вокселей, единица памяти и мешинга |
| Плоский массив | Все воксели в одной непрерывной полосе памяти, без вложенных структур |
| Кэш-дружелюбный доступ | Чтение подряд идущих данных попадает в процессорный кэш, без промахов |
| Compute-шейдер | Программа, которая выполняется на видеокарте в общем графическом конвейере |
| Жадный мешинг (greedy meshing) | Склеивание соседних граней одного материала в один четырёхугольник |
| 6 проходов по осям | Мешинг делается для каждой из 6 направлений грани (плюс-минус X, Y, Z) |
| Библиотека Jolt | Сторонняя библиотека физики твёрдых тел |
| Статик-ассерт (static_assert) | Проверка, которая срабатывает при компиляции, а не при запуске |

---

## 4. Что показывать на экране

1. **Схема воксельного мира** (куб чанка + плоский массив) — если есть на слайде.
2. **Вывод компилятора** с примером `static_assert` — показать 1-2 строки из `src/voxel/VoxelWorld.hpp` (например `static_assert(sizeof(Int3) == 12)`).
3. **HUD приложения** — на нём видно `CHUNKS: 27`, иллюстрирует плоский массив чанков в действии.

---

## 5. Твоя настоящая компетенция (для Q&A): Воксельный мир

**Это то, что ты реально знаешь. На сцене ты говоришь про архитектуру, но на вопросы комиссии отвечаешь по своей компетенции.**

**Ключевые файлы:**
- `src/voxel/VoxelWorld.hpp` — `VoxelChunk` (32 B), `Int3` (12 B), `VoxelMaterial` (1 B enum), `VoxelScenePreset` (5 пресетов)
- `src/voxel/VoxelWorld.cpp` — `CreateVoxelSceneWorld`, `BuildVoxelLabSceneWorld`, `UpdateFluidCA`, snapshot save/load
- `src/voxel/VoxelRaycast.{hpp,cpp}` — DDA raycast для placement/removal
- `src/voxel/VoxelInteraction.{hpp,cpp}` — placement/removal через input
- `src/voxel/SceneConfig.{hpp,cpp}` — JSON config (env-overrides)
- `src/voxel/VoxelMaterials.{hpp,cpp}` — `VoxelSceneLighting` UBO (624 B), 5 материалов
- `src/shaders/voxel_mesh.comp` — compute-шейдер greedy meshing (6 проходов)

**5 материалов:** `Air` (0), `Glass` (1), `Fluid` (2), `FloorWhite` (3), `FloorGray` (4). По 1 байту на воксель.

**Чанк 8×8×8 = 512 вокселей = 512 B = 2 SSE-регистра = влезает в L1 кэш (32 KB на Zen 3).**

**VoxelLab (демо-сцена):** пол-шахматка 18×18, стеклянный шар радиуса 6 вокруг (0, 8, 0), жидкость внутри (fluidFillLevel 0.7), 3 якоря (правый куб 4×4×1, левый столбик 2×2×5, передний 1×1×3) для стабильных теней. Генерируется процедурно.

**5 scene presets:** `VoxelLab` (default), `FlatBenchmark` (плоский пол), `TransparencyStress` (Glass-колонны), `ChunkGrid` (маркеры по углам), `MeshingStress` (большой объём). Переключение: F5.

**Greedy meshing:** `voxel_mesh.comp:613-619` — 6 per-axis greedy passes, merge adjacent cells with same exposed state into single W×H quad. Packing (W, H) в 6+6 бит = 12 бит → max quad extent = 64 вокселя. `kMaxChunkExtentForGreedy` fallback на per-voxel emission для oversized chunks.

**Fluid CA:** `UpdateFluidCA` в `VoxelWorld.cpp:1284-1643`. 1 tick = 1 cell per gravity, bottom-up y-pass, double-buffered snapshot, claimed-tracking. Spread rule restored 2026-06-13 (per `agent/decisions.md §30`). Deterministic: no FP, no syscalls, no atomics.

**Voxel raycast (DDA):** `VoxelRaycastHit { hasHit, hasPlacementVoxel, voxel, placementVoxel, hitNormal, material, distance }`. `placementVoxel` — предыдущая ячейка (для placement).

**Snapshot PVSNAP01:** magic `PVSNAP01` (8 B), 80-B header (`version=1`, `voxelByteCount`, `scenePreset`, `config`, `min`, `maxExclusive`, `editVersion`). `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot` возвращают `std::expected<bool, VoxelSnapshotError>`. Хоткеи: F6 save, F7 load.

**Статик-ассерты (compile-time contracts):**
- `static_assert(sizeof(Int3) == 12)` — `VoxelWorld.hpp:42`
- `static_assert(sizeof(VoxelChunk) == 32)` — `VoxelWorld.hpp:52`
- `static_assert(sizeof(VoxelSceneLighting) == 624)` — `VoxelMaterials.hpp:140`
- `static_assert(offsetof(VoxelSceneLighting, prevViewProjectionMatrix) == 528)` — `VoxelMaterials.hpp:159`
- Много других в `VoxelWorld.hpp`, `VoxelMaterials.hpp`, `SceneResources.hpp`, `Renderer.cpp` — всего **~30+ static_asserts** в коде ядра

Подробнее — `docs/DefenseCompetency_FAQ.md §2` (textbook для Тиммейта 2).

---

## 6. Вне зоны ответственности (к кому перенаправлять в Q&A)

| Вопрос про… | Говори |
|---|---|
| DOD layout / `alignas(16)` / SoA | «Архитектурное решение — к le1t» |
| C++26 фичи / std::simd / std::expected / модули | «К le1t» |
| Build / Clang / CMake / ctest | «К Тиммейту 1» |
| CSM / PCF / TAA / AOCC / шейдеры рендера | «К Тиммейту 3» |
| Walk controller / Jolt / edge grace | «К Тиммейту 4» |
| glTF / Draco / meshopt / miniaudio / snapshot save | «К Тиммейту 5» |
| BUG-005 cycle scene race | «К le1t (InputAction F5)» |
| Hot shader reload (клавиша 1) | «К le1t» |
| Демо VoxelLab / FPS / сцена | «К le1t» |
| Phase 4-9 / roadmap | «К Тиммейту 4 (он закрывает)» |

---

**Конец памятки.** Перед защитой: прочитать §2 вслух 3 раза с таймером, уложиться в 0:40 ± 5 секунд. §5 прочитать отдельно для Q&A.
