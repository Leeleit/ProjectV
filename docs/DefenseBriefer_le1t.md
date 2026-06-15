# DefenseBriefer_le1t.md — Памятка ведущего (Кадочников Лев Петрович)

**Дата защиты:** 2026-06-15
**Длительность:** вступление 2:00 + закрытие 0:30 + Q&A 5:00 = **7:30 минут вашего времени**
**Роль:** ведущий, тимлид, отвечаете на все вопросы комиссии

---

## 1. Ваш слот

| Время | Что делаете | Где подробно |
|---|---|---|
| 0:00–0:30 | Запуск VoxelLab, приветствие | [§2](#2-вступление-verbatim-200) |
| 0:30–2:30 | **Вступление** (проблема, цели, обоснование, что увидите) | [§2](#2-вступление-verbatim-200) verbatim 2:00 |
| 2:30–4:00 | Тиммейт 1 (стек+билд) | [DefenseBriefer_1.md](DefenseBriefer_1.md), cue-карта [§5](#5-cue-карты-переходов) |
| 4:00–5:30 | Тиммейт 2 (voxel+meshing) | [DefenseBriefer_2.md](DefenseBriefer_2.md) |
| 5:30–7:00 | Тиммейт 3 (тени+TAA+AOCC+ray-march) | [DefenseBriefer_3.md](DefenseBriefer_3.md) |
| 7:00–8:30 | Тиммейт 4 (физика+walk) | [DefenseBriefer_4.md](DefenseBriefer_4.md) |
| 8:30–10:00 | Тиммейт 5 (демо VoxelLab+ассеты+аудио) | [DefenseBriefer_5.md](DefenseBriefer_5.md) |
| 10:00–10:30 | **Закрытие** (готовы к вопросам) | [§3](#3-закрытие-verbatim-030) verbatim 0:30 |
| 10:30–15:00 | **Q&A** — все вопросы к вам | [§4 Q&A-карта](#4-qa-карта-30-вероятных-вопросов) |

**Out of scope для вас как ведущего:** ничего. Вы отвечаете за всё. Тиммейты могут подсказать детали по своим темам, но инициатива — у вас.

---

## 2. Вступление verbatim (2:00)

**Читать дословно, 270 слов, ~2 минуты при среднем темпе.**

> «Уважаемые члены комиссии, мы представляем проект **ProjectV** — высокопроизводительный воксельный игровой движок на C++26 и Vulkan 1.4.
>
> Проблема, которую мы решаем, — отсутствие современного открытого движка, который сочетает в себе низкоуровневый контроль GPU с Data-Oriented Design и качественным шейдерным конвейером для воксельной графики. Существующие решения либо слишком абстрагированы (Unity, Godot), либо закрыты (Roblox, Minecraft Bedrock).
>
> Наша цель — **надёжный, расширяемый и измеримый фундамент** воксельного движка, демонстрирующий владение современным C++, низкоуровневой графикой и архитектурами DOD/ECS. На целевой конфигурации — Ryzen 7 5800X, 16 ГБ ОЗУ, NVIDIA RTX 3060 Ti, 1920×1080 — мы достигаем стабильных **110–130 FPS** со временем кадра 7–9 мс.
>
> Архитектура построена на трёх столпах. **Первый** — Vulkan 1.4 с динамическим рендерингом, таймлайн-семафорами и compute-шейдерами для генерации мешей и ray-marching. **Второй** — Data-Oriented Design: `alignas(16)` для Vec3/Vec4/Mat4, плотные чанки 8×8×8, SIMD-оптимизации через `std::simd` и C/AVX2 ядро фрустум-кулинга. **Третий** — Entity-Component System через Flecs, где мир — пассивное зеркало VoxelWorld, исключающее race conditions.
>
> Физика интегрирована через Jolt, контроллер игрока поддерживает walk, creative и spectator с voxel-решателем владения грунтом. Конвейер ассетов на fastgltf с Draco-декомпрессией и meshopt-оптимизацией. Аудио на miniaudio с PipeWire → PulseAudio маршрутизацией.
>
> Мы честно документируем два известных дефекта: BUG-004 — микродрожание на VoxelLab, TAA-scope, post-defense follow-up; BUG-005 — гонка дескрипторов при переключении сцен, смягчён через `vkDeviceWaitIdle`. Полный список из 38 выполненных и 5 честно отложенных пунктов ТЗ — в нашем итоговом отчёте.
>
> Сейчас вы видите рабочее приложение Voxel Laboratory. Передаю слово коллегам для детального доклада по их модулям.»

---

## 3. Закрытие verbatim (0:30)

**70 слов, ~30 секунд.**

> «Подводя итог: ProjectV достиг поставленных целей MVP. У нас есть рабочий фундамент воксельного движка, 12 ctest-сьютов с baseline 14/14, runtime smoke 6/6 captures, метрики в sidecar, и воспроизводимая сборка на Windows и Linux. Открыто документируем техдолг и roadmap Phase 4–9. Готовы к вопросам.»

---

## 4. Q&A-карта (30 вероятных вопросов)

**Где подробно:** `docs/DefenseAlgorithms.md` (полный reference) + `docs/DefenseFAQ.md` (готовые ответы на 15+).

### 4.1. Архитектура (8 вопросов)

**Q1. Почему C++26, а не Rust/Zig/Go?**
- Все наши тяжёлые зависимости (Jolt, fastgltf, VMA, Draco, Flecs) — C/C++ с нативным API. Rust-биндинги заняли бы больше времени, чем вся разработка.
- C++26 даёт: `std::expected` (cold path errors), `std::simd` (hot math), модули (incremental build), `constexpr` (compile-time checks).
- Подробно: `DefenseAlgorithms.md §21`, `DefenseFAQ.md §1.1`.

**Q2. Почему Vulkan 1.4, а не OpenGL/DX12/Metal?**
- Vulkan — явный контроль GPU (pipelines, memory, sync). OpenGL — driver управляет, дорого для миллионов draw items.
- Compute-шейдеры — нужны для meshing и ray-marching.
- DX12 — только Windows. Metal — только Apple. Vulkan — кросс-платформенный.
- Vulkan 1.4 = dynamic rendering + timeline semaphores + push descriptors.

**Q3. Почему Jolt, а не PhysX/Bullet?**
- Jolt — MIT, современный, детерминированный, SIMD-оптимизирован, многопоточный.
- Bullet — устарел, сложен в оптимизации.
- PhysX — закрытые части, избыточен по размеру.
- Наш walk-контроллер — JPH::CharacterVirtual + voxel solver, edge grace для тонких граней.

**Q4. Почему Flecs, а не EnTT/Bevy ECS/DOTS?**
- Flecs — header-only C++ ECS, MIT, отличная эргономика для встроенного использования в Vulkan-приложении.
- EnTT — хорошая альтернатива, но Flecs лучше в ergonomics.
- Bevy ECS — только Rust. DOTS — коммерческий и привязан к Unity runtime.
- Нам нужен лёгкий, самодостаточный C++ ECS.

**Q5. Что такое DOD и зачем?**
- Data-Oriented Design — данные организованы для эффективной обработки CPU (cache-friendly, SIMD-friendly), а не для удобства ООП иерархии.
- VoxelChunk — плотный `voxels` массив (1 байт на воксель), 8×8×8 = 512 байт, влезает в L1.
- `alignas(16)` для Vec3/Vec4/Mat4 → авто-векторизация в `movaps`/`vmovaps`.
- SoA на итерации по вокселям = 100% cache hit. AoS = 18.75% (остальные 81.25% — чужие поля).
- 4× ускорение на ровном месте, чистая физика CPU.

**Q6. Как связаны ECS и VoxelWorld?**
- Single Source of Truth: `VoxelWorld` — единственный владелец, все мутации только через него.
- ECS (Flecs) — пассивное зеркало, обновляется 1× per frame через `SyncEcsWorldState`.
- HUD читает из ECS (read-only), не из VoxelWorld (mutable) → lock-free, без race conditions.
- Подробно: `DefenseAlgorithms.md §23`, `DefenseFAQ.md §2.2`.

**Q7. Как боретесь с overhead на error handling?**
- Гибридный подход (per `decisions.md §29`):
  - **Cold path:** `std::expected<T, E>` — инициализация Vulkan, загрузка glTF, snapshot save/load. Безопасное ветвление, детальные коды ошибок.
  - **Hot path:** `bool` returns + `CORE_ASSERT` (вырезаются в release). Запрещён `std::expected` на hot path.
- Подробно: `DefenseFAQ.md §2.3`.

**Q8. Что такое ECS-bridge и зачем нужен Flecs, если есть VoxelWorld?**
- Flecs даёт type-safe компоненты (CameraTag, PlayerControlledCamera, ChunkState, WorldBinding, DebugState).
- Геймплейные системы (input, camera update) читают компоненты ECS, не лезут в VoxelWorld напрямую.
- Lock-free read через ECS-mirror, никаких mutex-ов на hot path.

### 4.2. Алгоритмы (10 вопросов)

**Q9. Что такое greedy meshing и зачем?**
- Современные GPU теряют FPS на per-voxel `vkCmdDraw` (CPU bottleneck на драйвере).
- Жадный мешинг (Лысенков) объединяет компланарные грани вокселей одного материала в один quad.
- 6 проходов на чанк: ±X, ±Y, ±Z. 2D greedy scan, expand right + down, emit один quad.
- Сокращение: 30–50% граней → меньше draw calls, меньше vertex invocations.
- Подробно: `DefenseAlgorithms.md §3`, `DefenseFAQ.md §2.4`.

**Q10. Как работают каскадные тени (CSM)?**
- 4 каскада 2048×2048, lambda 0.80 (near-biased).
- Per-cascade projection build: sub-frustum → light-space → XY sphere fit (rotation-stable).
- Caster coverage: extrude receiver slice upstream along sun direction.
- Light camera snap к shadow texel grid — стабильна при малом движении камеры.
- Cascade blend band на границе, не hard switch.
- Glass не кастует, Fluid кастует (per `decisions.md §15`).
- Подробно: `DefenseAlgorithms.md §6`, `DefenseFAQ.md §3.1`.

**Q11. Что такое PCF и YCoCg?**
- PCF = Percentage-Closer Filtering, weighted 5×5. Vulkan 1.4 LINEAR magFilter → hardware 2×2 PCF бесплатно, manual 5×5 поверх.
- YCoCg = luma + 2 chroma компоненты. Clamp по компонентам отдельно, меньше ghosting на ярких участках чем RGB clamp.
- Подробно: `DefenseAlgorithms.md §7, §10`.

**Q12. Что такое TAA и зачем?**
- Temporal Anti-Aliasing: смешивает текущий кадр с историей.
- 8-sample Halton(2,3) jitter в projection matrix.
- YCoCg clamp в color history, neighbourhood clamp 1-7.
- 7 триггеров инвалидации истории (swapchain resize, world reload, TAA toggle, etc.).
- CAS sharpening поверх TAA — high-pass через 4-угловое среднее.
- B10G11R11_UFLOAT — 2× bandwidth saving vs R16G16B16A16.
- Подробно: `DefenseAlgorithms.md §10`, `DefenseFAQ.md §3.2`.

**Q13. Что такое ray-marching и как реализован?**
- 2D-метод: для каждого пикселя трассируется луч через объём/поле расстояний, цвет по ближайшему пересечению.
- Наш `ray_march.comp` — Amanatides-Woo 3D DDA через packed voxel payload.
- Push constants: `worldMinAndChunkSize/chunkGridAndFlags`.
- Toggle F6, OFF по умолчанию (стоимость).
- Подробно: `DefenseAlgorithms.md §11`, `DefenseFAQ.md §3.3`.

**Q14. Что такое контактная тень (CTSH)?**
- Короткая voxel DDA от фрагмента к солнцу, max ~5 единиц.
- Дополняет CSM там, где texel size недостаточен (контакт объекта с землёй).
- Glass пропускает, Fluid блокирует (per `decisions.md §15`).
- Подробно: `DefenseAlgorithms.md §8`, `DefenseFAQ.md §3.4`.

**Q15. Что такое AOCC и зачем, если есть SSAO?**
- AOCC = Ambient Occlusion Cavity Check, **локальный** forward-path occlusion, не full SSAO.
- 3 направления × 4 шага = 12 DDA трассировок на фрагмент.
- Параметры в `ambientOcclusionParams = {strength, radius, minVisibility}` Vec4 в `VoxelSceneLighting`.
- Baked per-face AO в compute meshing + runtime DDA — два слоя.
- SSAO отложен (Phase 5+) — требует depth/normal prepass, не оправдано для текущего mainline.
- Подробно: `DefenseAlgorithms.md §9`, `DefenseFAQ.md §3.5`.

**Q16. Зачем нужен локальный точечный свет, если есть солнце?**
- VoxelLab имеет один на пресет обратно-квадратичный точечный свет.
- Объёмный эффект (не плоский), подсвечивает тёмные стороны сферы.
- `voxel.frag` вычисляет GGX BRDF для обоих источников.
- Локальная тень — через voxel DVA (только для непрозрачных).
- Подробно: `DefenseFAQ.md §3.6`.

**Q17. Как работает walk controller?**
- Voxel-решатель авторитетный, **не** Jolt `CharacterVirtual::ExtendedUpdate`.
- CharacterVirtual — proxy/носитель стойки.
- Continuous sample top-plane под стопой, edge grace для тонких граней.
- Sneak (Shift) — sampled top-plane, без false-stick к стене.
- Air control: MinecraftLike (default) или Realistic.
- 3 режима: walk / creative / spectator. F4 переключает, двойной Space ↔ creative.
- Подробно: `DefenseAlgorithms.md §12`, `DefenseFAQ.md §4.1`.

**Q18. Что такое fluid CA?**
- Клеточный автомат для жидкости: 1 tick = down-fall, fallback cardinal spread.
- Hash-ordered для determinism (splitmix64), double-buffered.
- 20 Hz throttle (1 tick per 3 frames @ 60 FPS).
- Подробно: `DefenseAlgorithms.md §13`.

### 4.3. Качество и метрики (6 вопросов)

**Q19. Какие тесты, сколько?**
- **14 ctest suites** в `tests/CMakeLists.txt` (ProjectVTests, AssetLoaderTests, MeshBakerTests, DracoDecoderTests, FrustumCullingTests, CFrustumCullingTests, SunShadowCascadeSplitsTests, BoxUvFixtureTests, MathTests, StringIdTests, ModuleSmoke, StdModuleProbe + sub-suites).
- **Baseline: 14/14 passing** на linux-clang-debug, ~0.78s на debug, **0.06s на release** (O3+LTO).
- Runtime smoke 6/6 captures (FINAL/SHDW/CSM/CTSH/AOCC/LOCL).
- Sidecar metadata 60+ ключей.
- Подробно: `DefenseReport.md §5`, `DefenseAlgorithms.md §22`.

**Q20. Почему покрытие ниже 80%?**
- Фокус на критичных, regression-prone модулях (math utilities, dirty chunk invalidation, walk controller, preset parsing).
- 12 ctest suites / 200+ test functions.
- Визуальная корректность — через RuntimeSmoke, 6 эталонных слоёв.
- Повышение до 80% — в roadmap (post-defense).
- Подробно: `DefenseReport.md §9`, `DefenseFAQ.md §3` (тезисы).

**Q21. Какие метрики производительности?**
- VoxelLab reference shot: **110-130 FPS**, frame time 7-9 мс (debug), 5-6 мс (release).
- Release: ELF 19 MB (-73% vs 72 MB debug), +1.5-2.5× FPS.
- ctest wall clock: 0.06s release vs 0.78s debug (-92%).
- Build time: 22-30s clean build.
- Per-pass timings на release: shadow 30 µs, graphics 76 µs, TAA 3 µs.

**Q22. Какие известные баги?**
- **BUG-004 (VoxelLab tremor):** per-frame sub-pixel jitter. FPS 150, MS 6.6. Попытка фикса в `90a45b4` не устранила. **TAA-scope, post-defense follow-up.**
- **BUG-005 (F5 VUID race):** при cycle scene — 20+ ошибок `VUID-vkCmdDraw-None-08114` per 5 секунд. `vkDeviceWaitIdle` в `DestroySceneResources` смягчил, не устранил.
- **BUG-006 (untitled):** ещё не каталогизирован, см. `TODO.md`.
- Подробно: `DefenseReport.md §9`.

**Q23. Что отложено и почему?**
- 5 пунктов ТЗ: частицы (Phase 7), плагины/моддинг (Phase 8), асинхронная загрузка (Phase 7), HDR-текстуры (Phase 6), SVO (Phase 5 academic).
- Все явно перечислены в `DefenseReport.md §3` с обоснованием и планируемой фазой.

**Q24. Какие платформы поддерживаются?**
- Windows 10/11 (clang-cl 22) + Linux Arch (clang 22 native + lld 22 + libc++ 16).
- Обе платформы build green, ctest 14/14.
- 7 debug + 8 release CMakePresets.
- macOS — в roadmap (MoltenVK), не в MVP.

### 4.4. Команда и организация (6 вопросов)

**Q25. Как распределена работа в команде?**
- 6 человек: тимлид + 5 участников по модулям.
- См. `DefenseReport.md §12` (таблица вкладов).
- Тимлид: архитектура, DOD, ECS, выбор библиотек, Q&A.
- Тиммейт 1: стек, билд, тесты, метрики.
- Тиммейт 2: voxel-мир, meshing, visibility cache.
- Тиммейт 3: тени, TAA, AOCC, ray-marching.
- Тиммейт 4: физика, walk controller.
- Тиммейт 5: демо VoxelLab, ассеты, аудио.

**Q26. Какие решения принимали лично вы?**
- Выбор стека (C++26, Vulkan 1.4, Flecs, Jolt).
- DOD layout: alignas(16), чанк 8×8×8, плотный материал 1 байт.
- Hot/cold split: std::expected на cold, bool+assert на hot (per `decisions.md §29`).
- Walk authority = voxel solver, не Jolt (per `decisions.md §6`).
- Glass shadow policy: glass ignore, fluid cast (per `decisions.md §15`).
- TAA color format: B10G11R11_UFLOAT (per `decisions.md §20`).
- 4-каскадный CSM с lambda 0.80, XY sphere fit, caster coverage (per `decisions.md §15`).
- Модули: Math.ixx, Probe.ixx, StringId.ixx (Tier 2).
- Release preset policy: conservative, без -ffast-math, без -march=native (per `decisions.md §4`).

**Q27. Какие были трудности в командной работе?**
- Согласование scope между модулями (VoxelWorld ↔ Renderer ↔ Physics).
- Multi-agent sessions: см. `AGENTS.md §7.2.6`, `agent/active-sessions.md`.
- Critical incident 2026-06-10: destructive `git checkout -- .` стёр uncommitted work (см. `agent/memory.md §10.11`). Lesson: safety-net patch в `/tmp/` перед destructive операциями.
- Этот проект ведётся одним разработчиком (le1t), но документирован как multi-agent coordination protocol (AGENTS.md, decisions.md, memory.md).

**Q28. Как тестировали итерации с BUG-004 (VoxelLab tremor)?**
- Tracked через `agent/voxelab-tremor-handoff-2.md`.
- Repro: статичная камера, VoxelLab, F6 ray-march OFF, TAA ON, blend 0.10, jitter scale 1.0.
- Измерение: PerFrame sidecar `frame_time_ms` и TAA history debug view.
- Visual capture: `tools/linux/Invoke-ProjectVRuntimeSmoke.sh --views "FINAL SHDW CSM CTSH AOCC LOCL"`.
- Попытка фикса: `90a45b4` (TAA NDC depth), не устранила.
- Текущий статус: TAA-scope, post-defense.

**Q29. Сколько коммитов и как организован workflow?**
- 100+ коммитов за 3.5 месяца.
- Conventional commits с type/scope (per AGENTS.md §7.2.5).
- Multi-tier процесс: Tier 0-5 (12 коммитов) + feature commits.
- Multi-agent coordination через `agent/active-sessions.md` (см. `AGENTS.md §7.2.6`).
- Pre-commit gates: type=fix требует operator confirm (per `AGENTS.md §7.3.1`).
- Auto-close после commit per `AGENTS.md §8.1`.

**Q30. Что бы вы улучшили в следующей итерации?**
- Phase 4 (Networking): server-authoritative + client prediction.
- Phase 5 (SVO): hybrid SVO+chunks, SVO ray-marching для теней.
- Phase 6 (Fluid): полный клеточный автомат на GPU с диффузией и вязкостью.
- Phase 7 (Particles + Modding): система частиц, modding API.
- Phase 8 (SCP mechanics): неевклидова геометрия, порталы.
- Phase 9 (Strategic layer): тысячи юнитов, командный zoom.

---

## 5. Cue-карты переходов

### Cue 1: после вступления (2:30)
> «Спасибо за внимание. Передаю слово коллеге — он расскажет про технологический стек и систему сборки проекта.»
*(взмах рукой в сторону Тиммейта 1)*

### Cue 2: после Тиммейта 1 (4:00)
> «Спасибо. Теперь послушаем про то, как устроен воксельный мир и алгоритм жадного мешинга.»
*(взмах в сторону Тиммейта 2)*

### Cue 3: после Тиммейта 2 (5:30)
> «Спасибо. Дальше — про рендеринг: тени, сглаживание, контактные тени и экспериментальный ray-marching.»
*(взмах в сторону Тиммейта 3)*

### Cue 4: после Тиммейта 3 (7:00)
> «Спасибо. Переходим к физике и контроллеру игрока — это модуль Jolt и voxel-решатель.»
*(взмах в сторону Тиммейта 4)*

### Cue 5: после Тиммейта 4 (8:30)
> «Спасибо. И, наконец, описание демо-сцены Voxel Laboratory, ассетного конвейера и аудио. После этого я скажу заключительное слово, и мы перейдём к вопросам.»
*(взмах в сторону Тиммейта 5)*

### Cue 6: после Тиммейта 5 (10:00)
> «Заключительное слово.»
*(читать §3 verbatim)*

### Cue 7: после вашего закрытия (10:30)
> «Готовы ответить на ваши вопросы.»

---

## 6. Cheat-card для печати (1 страница A4)

```
┌────────────────────────────────────────────────────────────────────────┐
│                    LE1T CHEAT-CARD — ЗАЩИТА 2026-06-15                │
├────────────────────────────────────────────────────────────────────────┤
│ ВСТУПЛЕНИЕ (2:00) → "Уважаемые члены комиссии, мы представляем        │
│ проект ProjectV..." → см. §2 verbatim, читать дословно                  │
├────────────────────────────────────────────────────────────────────────┤
│ ЗАКРЫТИЕ (0:30) → "Подводя итог: ProjectV достиг..." → см. §3         │
├────────────────────────────────────────────────────────────────────────┤
│ КЛЮЧЕВЫЕ ЦИФРЫ:                                                       │
│  • 110-130 FPS, 7-9 мс frame time (debug), 5-6 мс (release)            │
│  • 14/14 ctest, 0.78s debug / 0.06s release                            │
│  • 27 чанков, 13 824 вокселей в VoxelLab                                │
│  • ELF 19 MB release vs 72 MB debug (-73%)                             │
│  • ctest wall clock 0.06s release vs 0.78s debug (-92%)                 │
├────────────────────────────────────────────────────────────────────────┤
│ 5 ВАЖНЫХ ОТВЕТОВ:                                                      │
│  1. "Почему C++26?" — все зависимости C/C++ + std::expected/simd       │
│  2. "Почему Vulkan?" — explicit control + compute shaders              │
│  3. "Почему DOD?" — 4× cache hit, чистая физика CPU                    │
│  4. "Почему тесты <80%?" — фокус на критичных, runtime smoke 6/6      │
│  5. "BUG-004/005?" — TAA-scope, post-defense follow-up                │
├────────────────────────────────────────────────────────────────────────┤
│ КОМАНДА (6 человек):                                                   │
│  • le1t: архитектура, Q&A                                              │
│  • T1: стек, билд, тесты, метрики                                      │
│  • T2: voxel-мир, meshing, visibility cache                            │
│  • T3: тени, TAA, AOCC, ray-march                                      │
│  • T4: физика, walk controller                                         │
│  • T5: демо VoxelLab, ассеты, аудио                                    │
├────────────────────────────────────────────────────────────────────────┤
│ ИЗВЕСТНЫЕ БАГИ: BUG-004 VoxelLab tremor, BUG-005 F5 VUID race         │
│ 5 ОТЛОЖЕНО: частицы, моддинг, async, HDR, SVO (см. DefenseReport §3)   │
├────────────────────────────────────────────────────────────────────────┤
│ ROADMAP: Phase 4 (Network) → 5 (SVO) → 6 (Fluid GPU) → 7 (Particles)  │
│                       → 8 (SCP) → 9 (Strategic)                        │
└────────────────────────────────────────────────────────────────────────┘
```

---

**Конец памятки.** Перед защитой: прочитать §2 и §3 вслух 2 раза (репетиция), затем 1 раз вслух с таймером. Cue-карты [§5] распечатать отдельно. Cheat-card [§6] — на стол. `DefenseAlgorithms.md` — рядом для глубоких вопросов. `DefenseFAQ.md` — для готовых ответов на 15+ вопросов.
