# Defense Competency FAQ — T6 (Планы и Завершение)

**Slot:** T6 (4:00–4:30, Участник 6 = le1t, slides 11-12-13: Ограничения+Команда+Закрытие)
**Кто говорит:** Тиммейт 4
**Реальная компетенция:** Физика (Jolt, walk-контроллер, edge grace, sneak, автопрыжок, режимы)
**Out of scope (к кому перенаправлять в Q&A):** выбор Jolt — к T2 (le1t); стек/сборка — к T1; воксельный мир — к T3; рендеринг — к T4; ассеты/аудио — к T5; все баги — к T2.

---

## 1. Verbatim твоей речи (T6)

> «Здравствуйте. Говоря об ограничениях: пять пунктов требований, включая систему частиц и октодеревья SVO, перенесены в роадмап [T2.md, T5.md]. Известная проблема гонки дескрипторов при смене сцен временно локализована через принудительное ожидание девайса на CPU [T2.md, T4.md]. Безопасность гарантирована использованием открытого исходного кода и проверенных MIT-зависимостей [T2.md].
>
> **[Переход на Слайд 12 — Команда и личный вклад]**
>
> На слайде представлено распределение ролей нашей команды из шести человек. Каждый участник отвечает за свой изолированный программный модуль, а координацию общей системной интеграции и архитектуру осуществляет тимлид.
>
> **[Переход на Слайд 13 — Выводы + Вопросы]**
>
> Подводя итог: MVP полностью готов, тридцать восемь требований ТЗ закрыты с нулевым уровнем предупреждений компилятора [T2.md]. Мы создали воспроизводимый и надежный графический фундамент для дальнейших исследований. Спасибо за внимание, мы готовы ответить на ваши вопросы.»

## 2. Кто ты

**Легенда:** ты отвечал за физику — Jolt integration, walk-контроллер, edge grace, sneak, auto-jump, режимы движения, voxel-character collision.

**На сцене:** ты говоришь T6 (Планы и Закрытие) — последний, отвечаешь за финальное слово команды.

**На Q&A:** ты отвечаешь на вопросы про **физику, walk-контроллер, режимы, edge grace, sneak, автопрыжок, Phase 4 (networking)**.

---

## 3. Твоя компетенция: Физика и walk-контроллер

**Файлы:**
- `src/physics/PhysicsWorld.hpp` / `src/physics/PhysicsWorld.cpp` — main API
- `src/app/Camera.hpp` / `src/app/Camera.cpp` — камера (взаимодействует с walk через TickWalkCharacter)
- `src/app/InputActions.cpp` — input bindings для walk

**PhysicsWorld API (`PhysicsWorld.hpp:50-90`):**
- `PhysicsState *CreatePhysicsState()` — создать Jolt state
- `void DestroyPhysicsState(physics)` — уничтожить
- `bool SyncPhysicsWorld(physics, world)` — sync collision shapes с вокселями
- `PhysicsRaycastHit RaycastPhysicsWorld(physics, origin, direction, maxDistance)` — Jolt-уровень
- `void ResetWalkCharacter(physics)` — reset позиции
- `bool SnapWalkCharacterToCamera(physics, world, camera)` — teleport walk к камере
- `bool SnapCreativeCharacterToCamera(physics, world, camera)` — teleport creative
- `bool TickWalkCharacter(physics, world, camera, input, dt)` — main walk tick
- `bool TickCreativeCharacter(physics, world, camera, input, dt)` — creative tick
- `bool DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` — **наш собственный воксельный решатель**, дополняет Jolt
- `SetPhysicsWalkAutoJumpEnabled(physics, bool)` — toggle
- `PhysicsWalkDebugInfo GetPhysicsWalkDebugInfo(physics)` — диагностика

**Jolt Physics:**
- Сторонняя библиотека (vendored `external/jolt/`)
- MIT, детерминированный, SIMD-оптимизирован
- Используется для общей физики твёрдых тел
- `JPH::CharacterVirtual` для детекции столкновений персонажа
- **Наш собственный код** дополняет Jolt для опоры игрока на блоки (edge grace, sneak, auto-jump) — Jolt не знает про «опору на 1 блок» в стиле Minecraft

**3 режима управления:**
- **Walk** — обычная ходьба с гравитацией (`TickWalkCharacter`)
- **Creative** — полёт с поддержкой столкновений (`TickCreativeCharacter`)
- **Spectator** — режим наблюдателя, пролетает сквозь стены (noclip)
- Переключение: `F4` (`ToggleControlMode`, cycle walk → creative → spectator)
- Двойной `Space` — toggle walk ↔ creative (быстрый)

### 3.1. Алгоритм 12 — Walk-контроллер (walk controller)

**Где:** `src/physics/PhysicsWorld.{hpp,cpp}` → `UpdateWalkGroundSupport`, `TryAutoJump`, `BuildWalkEdgeGraceUpdateSettings`.
**Архитектура:** `JPH::CharacterVirtual` **используется** для collision detection (капсула, прокси), **voxel solver augments** foot support (per `decisions.md §6`). Это не "voxel solver вместо Jolt" — Jolt остаётся основой.

**Алгоритм:**

### Ground support

1. `JPH::CharacterVirtual::ExtendedUpdate` — Jolt side: continuous collision detection с custom `BuildWalkEdgeGraceUpdateSettings()` (настройка `mWalkExtendedUpdateSettings`, изменяет поведение extended update для edge grace).
2. `UpdateWalkGroundSupport` (наш код) — **augment** Jolt-результата:
   - Sample top-plane в `feetPosition + (0, -stepHeight, 0)`.
   - Voxel lookup в `VoxelWorld`:
     - `Air`/`Fluid` → нет ground
     - `Glass`/`FloorWhite`/`FloorGray` → ground, support height = top Y
3. **Edge grace** (per `PhysicsWorld.cpp`):
   - `constexpr uint32_t kWalkEdgeGraceFrames = 4` — допуск в **фреймах** (не метрах!).
   - `constexpr float kWalkFootSupportEdgeGraceScore = 0.2f` — порог score (не дистанция).
   - `kWalkFootSupportMovingEdgeGraceScore = 0.5f` — для движущегося игрока.
   - Логика: `physics.walkEdgeGraceFramesRemaining` счётчик, при `supportScore < EdgeGraceScore` → `walkEdgeGraceFramesRemaining = 4` (4 фрейма grace). Не «дёргать» Y вверх-вниз при микро-перепаде.
4. **Sneak (Shift):** sampled top-plane (1 точка, не 4), без false-stick к стене. Файл: `walkSneakShape` (внутренний JPH::Shape), `walkSneakActive` flag.

### Auto-jump

1. Триггер: `J` toggle ON (InputAction).
2. Каждый кадр: `FindWalkTopSupportCandidate` — ищем forward voxel на уровне 1 блок выше ground.
3. `IsWalkAutoJumpRiseInRange(autoJumpRise)` — проверка, что rise в допустимом диапазоне.
4. Delay (F12 InputAction): если `walkAutoJumpDelayEnabled` ON, отсчёт начинается только когда `reached == true`. Иначе — мгновенно.
5. Manual jump (Space): обнуляет delay accumulator.

### Air control

- `ToggleWalkAirControlMode` (F11 InputAction): MinecraftLike (default) — WASD в воздухе, momentum = Jolt velocity. Realistic — W-only, фиксация направления.
- F11 InputAction **shadowed** defense r0 bypass (F11 = hot shader reload), но для defense demo walk modes toggle не на demo path.

**3 режима (F4 `ToggleControlMode`):**
- **walk:** grounded authority, edge grace, sneak, air control.
- **creative:** полёт, collision substepped (`TickCreativeCharacter` для substepping high-velocity).
- **spectator:** noclip, ignore physics, ignore pause (per `PhysicsWorld.cpp`).

**Edge cases:**
- Переключение creative ↔ walk: двойной Space.
- Auto-jump OFF: ручной Space = vanilla.
- Pause (`P`) vs `timeScale=0` — разные оси (per `decisions.md §26`).
- Edge grace — `kWalkEdgeGraceFrames = 4` (фреймы!), **НЕ** 0.1 м.

**Walk controller features (per `PhysicsWalkDebugInfo` struct, `PhysicsWorld.hpp:19-40`):**

**Edge grace (допуск тонкого края):**
- Контроллер **не дёргает** игрока на тонких краях (часть стопы на блоке, часть на воздухе)
- `supportState = EdgeGrace` — диагностический enum
- `edgeGraceFramesRemaining` (u32) — grace-таймер, сколько кадров ещё действует
- Реализован в `src/physics/PhysicsWorld.cpp` через grace-таймеры

**Sneak (Shift = LShift/RShift, через `InputAction::MoveDown`):**
- Режим скрытности — игрок **не прилипает к стене за углом**
- `sneakActive` (bool) — флаг
- `sneakSupportGraceFramesRemaining` (u32) — grace-таймер
- `cachedSneakSupportValid` (bool), `feetInsideCachedSneakSupport` (bool), `cachedSneakSupportReferenceFeetY` (float) — кэшированная опора для sneak

**Auto-jump (J, через `InputAction::ToggleWalkAutoJump`):**
- При включении, контроллер каждый кадр проверяет: есть ли впереди блок высотой 1, можно ли перепрыгнуть
- `autoJumpEnabled` (bool) — флаг
- `autoJumpDelayFramesRemaining` (u32) — задержка после прыжка
- `autoJumpDelayEnabled` (bool) — флаг задержки (`F12` = `ToggleWalkAutoJumpDelay`)
- Позволяет не спамить прыжками при fast movement

**Ground takeoff grace:**
- `groundTakeoffGraceFramesRemaining` — при отрыве от земли контроллер не сразу теряет состояние "grounded"
- `groundTakeoffCached` (bool) — закэшировано ли

**Ledge release grace:**
- `ledgeReleaseGraceFramesRemaining` — игнорирование кратковременной потери опоры на тонких краях

**WalkAirControlMode (F11, `InputAction::ToggleWalkAirControlMode`):**
- Переключатель: «насколько сильно игрок может влиять на направление в воздухе»
- `GetPhysicsWalkAirControlMode(physics)` / `SetPhysicsWalkAirControlMode`

**Говорить:**
- «JPH::CharacterVirtual + voxel solver augment (per `decisions.md §6`); Jolt для collision detection, наш solver — для foot support».
- «Edge grace = `kWalkEdgeGraceFrames = 4` фрейма + score 0.2 (НЕ 0.1 м)».
- «Sneak, auto-jump — фичи для voxel мира, не generic character».
- «3 режима, F4 переключает, двойной Space ↔ creative».

**Voxel raycast для character:**
- `DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` — проверяет, пересекается ли AABB персонажа с заданным вокселем
- Используется в `VoxelInteraction::CanPlaceInteractionVoxelBox` для предотвращения placement внутрь игрока
- `InteractionState` хранит `placementVoxel`, `placementChunkCoord`, `placementChunkMin/Max`, `placementChunkDirty/Active/Index`, `placementChunkNonAirVoxelCount`, `placementVoxelInChunk`

### 3.2. Алгоритм 15 — Интеграция с Jolt (CharacterVirtual + voxel solver)

**Где:** `src/physics/PhysicsWorld.{hpp,cpp}` (обёртка).
**Проблема:** Jolt — generic physics engine, не знает про воксели. Нужен мост.

**Интеграция:**
- `JPH::PhysicsSystem` для rigid bodies.
- `JPH::CharacterVirtual` как proxy для character (collision detection).
- **Ground authority** = voxel solver (см. §12), не `CharacterVirtual::ExtendedUpdate`.
- **Static voxel world** = `JPH::Body` с `JPH::Shape` per solid voxel (lazy creation, cached).
- **Voxel edits** = invalidate Jolt body cache для affected chunks, recreate.

**Substepping (creative):**
- High velocity в creative может skip чанки за один шаг.
- `TickCreativeCharacter` разбивает deltaTime на N substeps, clamp velocity на каждый.

**Говорить:**
- «JPH::CharacterVirtual как proxy, voxel solver авторитетный для ground».
- «Static voxel world = JPH::Body per solid voxel, lazy cached».
- «Substepping в creative для high-velocity пропусков чанков».

**PhysicsWalkDebugInfo (struct, `PhysicsWorld.hpp:19-40`):**
```cpp
struct PhysicsWalkDebugInfo {
    bool valid = false;
    PhysicsWalkSupportDebugState supportState;  // Air / Grounded / EdgeGrace
    std::array<float, 3> feetPosition{};
    float footSupportScore = 0.0f;
    uint32_t footSupportHitSamples = 0;
    uint32_t footSupportTotalSamples = 0;
    uint32_t edgeGraceFramesRemaining = 0;
    uint32_t groundTakeoffGraceFramesRemaining = 0;
    uint32_t sneakSupportGraceFramesRemaining = 0;
    uint32_t ledgeReleaseGraceFramesRemaining = 0;
    uint32_t autoJumpDelayFramesRemaining = 0;
    bool groundTakeoffCached = false;
    bool cachedSneakSupportValid = false;
    bool feetInsideCachedSneakSupport = false;
    bool sneakActive = false;
    bool jumpLockActive = false;
    bool suppressPassiveSlide = false;
    bool autoJumpEnabled = false;
    bool autoJumpDelayEnabled = true;
    float cachedSneakSupportReferenceFeetY = 0.0f;
};
```

**Хоткеи walk (`InputActions.cpp:119-181`):**
- `W` `A` `S` `D` — движение
- `Space` — прыжок (`MoveUp`)
- `LShift` / `RShift` — sneak (`MoveDown`, через speed slow не наоборот)
- `LCTRL` / `RCTRL` — speed boost (`SpeedBoost`, 3× = 40 m/s × 3 = 120 m/s)
- `LALT` / `RALT` — speed slow (`SpeedSlow`, 0.25× = 10 m/s)
- `F4` — toggle walk/creative/spectator (`ToggleControlMode`)
- `F11` — toggle walk air control mode (`ToggleWalkAirControlMode`)
- `J` — toggle auto-jump (`ToggleWalkAutoJump`)
- `F12` — toggle auto-jump delay (`ToggleWalkAutoJumpDelay`)
- `F3` — reset camera (`ResetCamera`)
- `TAB` — toggle relative mouse mode (`ToggleRelativeMouseMode`)
- `P` — toggle pause (`TogglePause`)
- `[` / `]` — decrease / increase time scale (`DecreaseTimeScale` / `IncreaseTimeScale`)
- `\` — step single frame (`StepSingleFrame`)
- `` ` `` — reset time scale (`ResetTimeScale`)

**Камера (`src/app/Camera.hpp:5-33`):**
- `InitializeCamera(camera, simulation, input)` — init
- `HandleCameraEvent(camera, input, event)` — SDL events
- `ConsumeCameraLookInput(camera, input)` — read mouse deltas
- `TickCamera(camera, input, dt)` — per-frame
- `GetCameraForwardVector(camera)` — для look direction
- `GetCameraVisibleSceneMaxDistance(camera)` — для frustum cull
- `BuildGraphicsPushConstants(camera, extent, taaJitterNdcX, taaJitterNdcY)` — для shader
- `BuildChunkCullingParameters(camera, extent, maxDistance)` — для С-ядра

**Camera constants (`Camera.cpp:29-34`):**
```cpp
constexpr float kMinMoveSpeed = 2.0f;
constexpr float kMaxMoveSpeed = 40.0f;
constexpr float kBoostMoveSpeedMultiplier = 3.0f;
constexpr float kSlowMoveSpeedMultiplier = 0.25f;
constexpr float kMaxLookPitchRadians = 1.553343f;  // ~89°
constexpr float kMainlineVisibleSceneMaxDistance = 64.0f;
```

**SyncPhysicsWorld:**
- Один раз за кадр (или реже) — синхронизирует Jolt collision shapes с `VoxelWorld.voxels`
- Возвращает `bool` — успех/неуспех (cold path)
- Increment `editVersion` → invalidate Jolt shapes для затронутых вокселей

**Phase 4 (networking) — что в плане:**
- Server-authoritative + client prediction
- `SnapWalkCharacterToCamera` может использоваться для client prediction
- `SyncPhysicsWorld` будет происходить на сервере, broadcast результатов
- Phase 4 follow-up

---

## 4. Hotkeys в твоей зоне

- `W` `A` `S` `D` — движение
- `Space` — прыжок (`MoveUp`)
- Двойной `Space` — toggle walk ↔ creative
- `LShift` / `RShift` — sneak (`MoveDown`)
- `LCTRL` / `RCTRL` — speed boost (×3)
- `LALT` / `RALT` — speed slow (×0.25)
- `F4` — toggle walk/creative/spectator (`ToggleControlMode`)
- `F11` — toggle walk air control mode (`ToggleWalkAirControlMode`)
- `J` — toggle auto-jump (`ToggleWalkAutoJump`)
- `F12` — toggle auto-jump delay (`ToggleWalkAutoJumpDelay`)
- `F3` — reset camera (`ResetCamera`)
- `TAB` — toggle relative mouse mode (`ToggleRelativeMouseMode`)
- `P` — toggle pause (`TogglePause`)
- `[` / `]` — decrease / increase time scale
- `\` — step single frame
- `` ` `` — reset time scale
- Левый клик — removal
- Правый клик — placement

---

## 5. Глоссарий (твоя зона)

**JOLT (JOLT Physics)** — MIT, deterministic, SIMD-оптимизирован. Vendored как `external/jolt/`. Альтернативы: PhysX (NVIDIA, проприетарный), Bullet (устарел).

**JPH::CHARACTERVIRTUAL** — Jolt'овский класс для character controller. Используется для коллизий персонажа.

**JPH (Jolt namespace)** — `JPH::Body`, `JPH::CharacterVirtual`, `JPH::BroadPhaseLayer` и др.

**WALK_CONTROLLER** — собственный код (поверх JPH::CharacterVirtual) для опоры игрока на блоки. Edge grace, sneak, auto-jump.

**EDGE_GRACE** — допуск тонкого края. Контроллер не дёргает игрока на 1-wide мостах. `edgeGraceFramesRemaining` (u32) — grace-таймер.

**SNEAK** — Shift. Игрок не прилипает к стене за углом. `sneakActive` + `sneakSupportGraceFramesRemaining`.

**AUTO_JUMP** — клавиша `J`. Контроллер прыгает на 1-блок впереди. `autoJumpEnabled` + `autoJumpDelayFramesRemaining`.

**WALK_AIR_CONTROL_MODE** — клавиша `F11`. Переключатель: насколько сильно игрок влияет на направление в воздухе.

**GROUND_TAKEOFF_GRACE** — при отрыве от земли контроллер не сразу теряет "grounded".

**LEDGE_RELEASE_GRACE** — игнорирование кратковременной потери опоры на тонких краях.

**CREATIVE_MODE** — полёт с поддержкой столкновений. `TickCreativeCharacter`. Двойной Space ↔ walk.

**SPECTATOR_MODE** — noclip, пролетает сквозь стены. `ToggleControlMode` cycle.

**JPH_BROAD_PHASE_LAYER** — broad phase layer для collision filtering в Jolt.

**JPH_NARROW_PHASE_QUERY** — narrow phase (точная проверка коллизий).

**KINEMATIC_CHARACTER** — JPH::CharacterVirtual — kinematic, не подвержен гравитации/импульсам напрямую (управляется walk controller).

**RAYCAST_PHYSICS** — `RaycastPhysicsWorld(physics, origin, direction, maxDistance) → PhysicsRaycastHit`. Jolt-уровень (не voxel).

**DOES_PHYSICS_CHARACTER_OVERLAP_VOXEL** — `VoxelCharacterOverlapVoxel(physics, camera, voxel) → bool`. **Наш собственный воксельный решатель**, дополняет Jolt для опоры игрока на блоки.

**SYNC_PHYSICS_WORLD** — `SyncPhysicsWorld(physics, world) → bool`. Один раз за кадр. Sync collision shapes из `VoxelWorld.voxels` в Jolt.

**PHYSICSWALKDEBUGINFO** — struct с diagnostics: supportState, grace-таймеры, sneak flags, jump lock, cached support.

**VOXEL_CHARACTER_COLLISION** — наш собственный код проверки `DoesPhysicsCharacterOverlapVoxel`. Используется в `VoxelInteraction::CanPlaceInteractionVoxelBox`.

**GRACE_TIMER** — frames remaining для grace-периода. Decrement per frame, expire когда = 0.

**PHASE_4_NETWORKING** — server-authoritative + client prediction. Snap + Reconcile.

---

## 6. Реалистичные вопросы (5-7)

**Q1. Почему Jolt, а не PhysX или Bullet?**
- Jolt — MIT, современный, детерминированный, SIMD-оптимизирован
- Bullet устарел, PhysX избыточен + проприетарный
- per `decisions.md` и `agent/memory.md`

**Q2. Как работает edge grace?**
- Контроллер **не дёргает** игрока на тонких краях
- `supportState = EdgeGrace` когда опора нечёткая (часть стопы на блоке)
- `edgeGraceFramesRemaining` — таймер, сколько кадров ещё действует
- Без edge grace: игрок дёргается на тонких блоках (1-wide bridge)

**Q3. Как работает auto-jump?**
- При включении (`J`), контроллер каждый кадр проверяет: есть ли впереди блок высотой 1
- `autoJumpDelayFramesRemaining` — задержка после прыжка
- `autoJumpDelayEnabled` — toggle через `F12`

**Q4. Как работает sneak?**
- При зажатом Shift, игрок **не прилипает к стене за углом**
- `sneakActive` — флаг, `sneakSupportGraceFramesRemaining` — grace-таймер
- `cachedSneakSupportValid` — закэширована ли опора для sneak

**Q5. Какие режимы и как переключать?**
- Walk / Creative / Spectator — `F4` cycle
- Walk ↔ Creative — двойной `Space`
- Creative — полёт с collision
- Spectator — noclip (через стены)

**Q6. Какая максимальная скорость?**
- Base: 2.0-40.0 m/s (зависит от input)
- Boost (`LCTRL`): ×3 = 40 × 3 = 120 m/s
- Slow (`LALT`): ×0.25 = 10 m/s
- Sneak: тот же slow что и LShift

**Q7. Как работает creative mode?**
- Полёт с поддержкой столкновений (`TickCreativeCharacter`)
- Двойной `Space` — toggle walk ↔ creative
- Можно подлететь к любой точке сцены
- Voxel placement/removal работает так же

---

## 7. Каверзные вопросы (3-5)

**Q8. Что произойдёт, если игрок стоит на 1-блок-wide мосте?**
- Edge grace включается: контроллер не дёргает
- Если опора полностью теряется — fall (гравитация)
- `ledgeReleaseGraceFramesRemaining` — кратковременная потеря опоры игнорируется

**Q9. Чем отличается `groundTakeoffGrace` от `edgeGrace`?**
- `groundTakeoff` — при отрыве от земли (прыжок) контроллер не сразу теряет "grounded"
- `edgeGrace` — на тонких краях контроллер не дёргает
- Разные grace-таймеры, разные ситуации

**Q10. Как Phase 4 (networking) изменит walk controller?**
- `SnapWalkCharacterToCamera` — для client prediction (предсказание позиции)
- `SyncPhysicsWorld` — на сервере, broadcast результатов клиентам
- Reconciliation: если предсказание не совпало с server state — snap обратно
- per Phase 4 в roadmap

**Q11. Можно ли прыгнуть на 2-блок высоту через auto-jump?**
- Нет, auto-jump только для 1-блок
- Для 2+ блоков — нужен manual `Space` в нужный момент
- Иначе это не «опора на блок», а прыжок как таковой

---

## 8. Хронология (релевантные события)

**2026-04-12 (M1):** `AudioEngine` + `miniaudio` integration. (Не физика, но JPH CharacterVirtual используется параллельно.)

**2026-04-13 (Tier 1.B):** `std::expected<T, E>` migration на холодных путях. Physics state init переведён.

**2026-04-13 (Walk Edge Physics post-mortem):** упомянут в `agent/memory.md` — первоначальный walk controller на 1-wide bridge дёргался. Решено через edge grace + ground takeoff grace + ledge release grace.

**2026-04-13 (Hardcore perf r0):** Phase 0 = doc only. ctest baseline 14/14. Walk controller в покрытии.

**Phase 4 (networking, future):** server-authoritative + client prediction. `SnapWalkCharacterToCamera` + `SyncPhysicsWorld` integration. Post-MVP.

**2026-04-15 (Post-WBV-r1):** hotkey relocate 1/2/3 — `F11` → walk air control, `F12` → auto-jump delay, `J` → auto-jump toggle. Walk-контроллер хоткеи окончательно зафиксированы.

---

## 9. Out of scope (Q&A redirect)

| Вопрос про… | Говори |
|---|---|
| Почему Jolt, а не PhysX/Bullet | «К T2 (le1t)» |
| DOD / hot-cold split | «К T2 (le1t)» |
| Стек/Clang/cmake/ctest | «К T1» |
| Voxel-мир / чанки | «К T3» |
| Тени / TAA / AOCC / рендеринг | «К T4» |
| Ассеты / аудио / snapshot | «К T5» |
| BUG-005 cycle scene race | «К T2 (le1t)» |
| Hot shader reload (клавиша 1) | «К T2 (le1t)» |
| JSON config / snapshot PVSNAP01 | «К T2 (le1t)» |
| Phase 5-9 (SVO, fluid GPU, частицы, моддинг, стратегия) подробно | «К T2 (le1t)» |
