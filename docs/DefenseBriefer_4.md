# Памятка Тиммейта 4 — Физика и walk-контроллер (говорит T6 Планы)

**Участник:** [Имя Тимейта 4]
**Слот на сцене:** 4:00–4:30 (30 секунд) — T6 Планы и Завершение
**Твоя реальная компетенция:** Физика (Jolt, walk-контроллер, edge grace, sneak, автопрыжок, режимы)
**Что НЕ твоё (к кому перенаправлять в Q&A):** стек/демо — к le1t; вступление — к Тиммейту 1; воксельный мир — к Тиммейту 2; рендеринг — к Тиммейту 3; ассеты/аудио — к Тиммейту 5; все баги — к le1t

---

## 1. Шапка выступления

> «Здравствуйте. Меня зовут [Имя Тимейта 4], я закрою выступление — расскажу про планы команды и подведу итог.»

---

## 2. Что говорить дословно (~55-65 русских слов, 0:30)

> «Здравствуйте. Что касается планов на будущее. Сейчас мы завершили фазу MVP — минимально жизнеспособного продукта. В следующих фазах мы планируем добавить сетевой мультиплеер, перенести все расчеты жидкости полностью на видеокарту для еще большей скорости, а также создать удобный API для написания пользовательских модов. Мы достигли большинства поставленных целей из ТЗ.
>
> Наша таблица с распределением ролей представлена на экране. Спасибо за внимание, готовы ответить на вопросы!»

---

## 3. Понятия (6 терминов, чтобы понимать что говоришь)

| Термин | Что это |
|---|---|
| MVP | Минимально жизнеспособный продукт (Minimum Viable Product) |
| Phase 4-9 | Пост-MVP фазы развития: сеть, SVO, fluid, частицы, моддинг, стратегия |
| Сетевой мультиплеер | Совместная игра нескольких игроков по сети |
| Клеточный автомат | Алгоритм, где каждая ячейка обновляется по правилам соседей |
| Modding API | Интерфейс для написания пользовательских модификаций |
| Сервер-авторитарный | Сетевая модель, где сервер — единственный источник истины |

---

## 4. Что показывать на экране

1. **Слайд «Распределение ролей»** — таблица из `docs/DefenseReport.md §12`:
   - le1t: Архитектура, Vulkan, C++26, DOD
   - Тиммейт 1: Сборка метрик, тесты, пресеты
   - Тиммейт 2: Воксельный мир, чанки, мешинг, статик-ассерты
   - Тиммейт 3: Рендеринг, шейдеры, TAA, CSM, AOCC
   - Тиммейт 4: Физика, walk-контроллер, Jolt, edge grace
   - Тиммейт 5: Ассеты, аудио, snapshot, meshopt

---

## 5. Твоя настоящая компетенция (для Q&A): Физика и walk-контроллер

**Это то, что ты реально знаешь. На сцене ты говоришь про планы, но на вопросы комиссии отвечаешь по своей компетенции.**

**Ключевые файлы:**
- `src/physics/PhysicsWorld.hpp` — API:
  - `CreatePhysicsState()` / `DestroyPhysicsState()`
  - `SyncPhysicsWorld(physics, world)` — sync вокселей с Jolt collision shapes
  - `RaycastPhysicsWorld(physics, origin, direction, maxDistance)`
  - `DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` — наш собственный воксельный решатель
  - `TickWalkCharacter(physics, world, camera, input, dt)` — main walk tick
  - `TickCreativeCharacter` — creative mode
  - `SetPhysicsWalkAutoJumpEnabled(physics, bool)` — toggle auto-jump
  - `GetPhysicsWalkDebugInfo(physics)` — диагностика walk
  - `SetPhysicsWalkAirControlMode` / `GetPhysicsWalkAirControlMode`

**Jolt Physics (MIT, deterministic, SIMD):**
- Сторонняя библиотека (vendored как `external/jolt/`)
- Используется для общей физики твёрдых тел
- `JPH::CharacterVirtual` для детекции столкновений персонажа
- Наш собственный код дополняет Jolt для опоры игрока на блоки (для edge grace, sneak, автопрыжка)

**3 режима управления:**
- **Walk** — обычная ходьба с гравитацией (`TickWalkCharacter`)
- **Creative** — полёт с поддержкой столкновений (`TickCreativeCharacter`)
- **Spectator** — режим наблюдателя, пролетает сквозь стены (noclip)
- Переключение: `F4` (`ToggleControlMode`), двойной `Space` — toggle walk ↔ creative

**Walk controller features:**

**Edge grace (допуск тонкого края)** — контроллер **не дёргает** игрока на тонких краях, когда опора нечёткая (часть стопы на блоке, часть на воздухе). Реализован в `src/physics/PhysicsWorld.cpp` через grace-таймеры. Параметр `edgeGraceFramesRemaining` показывает, сколько кадров ещё действует grace.

**Sneak (Shift)** — режим скрытности. Игрок **не прилипает к стене за углом**. Клавиша `LShift`/`RShift` (`MoveDown` через `InputAction`). Параметры: `sneakActive`, `sneakSupportGraceFramesRemaining`, `cachedSneakSupportValid`, `feetInsideCachedSneakSupport`.

**Auto-jump (J)** — при включении (`ToggleWalkAutoJump`), контроллер каждый кадр проверяет: есть ли впереди блок высотой 1, можно ли перепрыгнуть. Параметр `autoJumpDelayFramesRemaining` — задержка после прыжка (`ToggleWalkAutoJumpDelay` — `F12`).

**Ground takeoff grace** — `groundTakeoffGraceFramesRemaining` — при отрыве от земли контроллер не сразу теряет состояние «grounded».
**Ledge release grace** — `ledgeReleaseGraceFramesRemaining` — игнорирование кратковременной потери опоры на тонких краях.

**WalkAirControlMode** — переключатель (клавиша `F11`): «насколько сильно игрок может влиять на направление в воздухе».

**Voxel raycast для character:** `DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` — проверяет, пересекается ли AABB персонажа с заданным вокселем. Используется в `VoxelInteraction` для предотвращения placement внутрь игрока.

**PhysicsWalkDebugInfo (struct):** `valid`, `supportState` (`Air`/`Grounded`/`EdgeGrace`), `feetPosition`, `footSupportScore`, `footSupportHitSamples`/`footSupportTotalSamples`, все grace-таймеры, `sneakActive`, `jumpLockActive`, `autoJumpEnabled`, `autoJumpDelayEnabled`.

**Хоткеи walk:** `WASD` — движение, `Space` — прыжок (`MoveUp`), `F4` — режимы, `LShift`/`RShift` — sneak (`MoveDown`), `J` — автопрыжок, `F11` — air control mode, `F12` — auto-jump delay, `LCTRL`/`RCTRL` — speed boost (`SpeedBoost`, 3×), `LALT`/`RALT` — speed slow (`SpeedSlow`, 0.25×), `P` — пауза, `[`/`]` — замедление/ускорение времени, `\` — покадровый шаг, `` ` `` — сброс time scale.

**PhysicsWorld.h API (полный):**
- `CreatePhysicsState() → PhysicsState*` — создать Jolt state
- `DestroyPhysicsState(physics)` — уничтожить
- `SyncPhysicsWorld(physics, world) → bool` — синхронизировать collision shapes с вокселями
- `RaycastPhysicsWorld → PhysicsRaycastHit { hasHit, voxel, position, normal, distance }`
- `ResetWalkCharacter(physics)` — reset позиции
- `SnapWalkCharacterToCamera(physics, world, camera) → bool` — teleport walk
- `SnapCreativeCharacterToCamera(physics, world, camera) → bool`
- `GetPhysicsWorldSyncVersion(physics) → uint64_t` — dirty-флаг для ECS sync

Подробнее — `docs/DefenseCompetency_FAQ.md §4` (textbook для Тиммейта 4).

---

## 6. Вне зоны ответственности (к кому перенаправлять в Q&A)

| Вопрос про… | Говори |
|---|---|
| Почему Jolt, а не PhysX/Bullet | «Архитектурное решение — к le1t» |
| DOD / hot-cold split | «К le1t» |
| Стек/Clang/cmake/ctest | «К Тиммейту 1» |
| Voxel-мир / чанки | «К Тиммейту 2» |
| Тени / TAA / AOCC | «К Тиммейту 3» |
| Ассеты / аудио / snapshot | «К Тиммейту 5» |
| BUG-005 cycle scene race | «К le1t» |
| Hot shader reload F11 | «Render, к le1t» |
| JSON config / snapshot PVSNAP01 | «К le1t» |
| Phase 4-9 подробно (SVO, fluid GPU) | «К le1t — он знает детали roadmap» |

---

**Конец памятки.** Перед защитой: прочитать §2 вслух 3 раза с таймером, уложиться в 0:30 ± 5 секунд. §5 — самая длинная секция, изучить отдельно. После твоего выступления — комиссия задаёт вопросы, отвечает le1t (он ведущий) с привлечением соответствующего тиммейта при необходимости.
