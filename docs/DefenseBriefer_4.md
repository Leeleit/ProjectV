# DefenseBriefer_4.md — Памятка Тиммейта 4: Физика и walk controller

**Участник:** [Имя Тимейта 4]
**Слот:** 7:00–8:30 (1:30 минуты)
**Что покрываю:** интеграция Jolt, walk/creative/spectator режимы, walk controller (edge grace, sneak, авто-прыжок), voxel raycast
**Что НЕ покрываю:** выбор библиотек (le1t), стек (Тиммейт 1), voxel-мир (Тиммейт 2), рендеринг (Тиммейт 3), демо (Тиммейт 5)

---

## 1. Шапка выступления

> «Добрый день, меня зовут **[Имя Тимейта 4]**, я расскажу про физику и контроллер игрока.»

---

## 2. Что говорить verbatim (1:30, ~220 слов)

> «Физика в ProjectV построена на библиотеке Jolt Physics — это современный движок для твёрдых тел, детерминированный, с поддержкой SIMD и многопоточности, лицензия MIT. Jolt используется для обнаружения столкновений и контроллера персонажа, а наш собственный voxel-решатель **augments** Jolt-сторону — это зафиксировано в наших инженерных решениях.
>
> В игре три режима управления. **Walk** — обычная ходьба с гравитацией, игрок стоит на вокселях. **Creative** — полёт с поддержкой столкновений, при больших скоростях шаги дробятся на подшаги. **Spectator** — режим наблюдателя, пролетает сквозь стены, не реагирует на паузу. Клавиша F4 переключает режимы по кругу, двойное нажатие Space переключает между creative и walk.
>
> Walk-контроллер имеет несколько специальных возможностей. **Edge grace** — это когда игрок стоит на тонком крае, контроллер не дёргает его вверх-вниз при микро-перепаде высот. В коде это `kWalkEdgeGraceFrames = 4` фрейма плюс `kWalkFootSupportEdgeGraceScore = 0.2f` — то есть счётчик фреймов и score, не дистанция в метрах. **Sneak** — при удержании Shift игрок не прилипает к стене при попытке зайти за угол. **Авто-прыжок** — при подходе к блоку высотой 1 игрок автоматически перепрыгивает, включается клавишей J, отключается F12. Контроль в воздухе — Minecraft-подобный по умолчанию, реалистичный с фиксацией направления.
>
> Размещение и удаление блоков работает через voxel raycast — это 3D DDA-трассировка луча через чанки, которая возвращает точку попадания и нормаль. Правый клик ставит блок в смежной ячейке, левый — убирает тот, в который попал луч.
>
> Для отладки есть замедление `[` и `]`, покадровый шаг `\`, сброс скорости `\``. Это разные оси от паузы P. Передаю слово коллеге.»

---

## 3. Понятия (12 терминов, чтобы понимать что говоришь)

| Термин | Что это в одном предложении |
|---|---|
| **Jolt Physics** | Физический движок (JPH::PhysicsSystem), MIT, детерминированный, SIMD. |
| **CharacterVirtual** | Класс Jolt для персонажа, **используется** для collision detection (НЕ заменён voxel solver). |
| **Voxel solver** | Наш решатель в `PhysicsWorld.cpp`, **augments** JPH::CharacterVirtual foot support (per `decisions.md §6`). |
| **Walk mode** | Обычная ходьба, гравитация, ground support. |
| **Creative mode** | Полёт, collision с подшагами для высоких скоростей. |
| **Spectator mode** | Noclip, пролетает сквозь стены, игнорирует паузу. |
| **Edge grace** | `kWalkEdgeGraceFrames = 4` фрейма + `kWalkFootSupportEdgeGraceScore = 0.2f` (НЕ 0.1 м). |
| **Sneak** | Shift = sampled top-plane поддержка, не false-stick к стене. |
| **Auto-jump** | Авто-прыжок через блок высотой 1, J toggle, F12 delay toggle. |
| **Voxel raycast** | 3D DDA-трассировка луча через чанки для placement/removal. |
| **Substep** | Дробление deltaTime на N подшагов в creative для high-velocity пропусков чанков. |
| **timeScale** | Множитель deltaTime для замедления/ускорения симуляции. Pause (P) и timeScale=0 — разные оси. |

---

## 4. Что показывать на экране (если попросят)

**Демо 1 — walk controller (20 секунд):**
- WASD + мышь, облёт вокруг сцены.
- «Сейчас режим walk, гравитация работает, я стою на полу. Если зайти на тонкий край — edge grace держит стабильно.»

**Демо 2 — creative mode (15 секунд):**
- Двойной Space → creative.
- Полёт по сцене, проход сквозь... нет, collision работает.
- «Это creative, я могу летать. Видите — сталкиваюсь со стеклянным шаром. Substepping не даёт пролететь сквозь при высокой скорости.»

**Демо 3 — spectator (10 секунд):**
- F4 → spectator.
- «Это spectator, пролетаю сквозь всё, как noclip. И пауза P на spectator не действует.»

**Демо 4 — placement/removal (20 секунд):**
- Левый клик — убрать блок из пола.
- Правый клик — поставить обратно.
- «Voxel raycast — 3D DDA через чанки. Видите: editVersion инкрементируется, чанк становится dirty, через кадр greedy meshing пересчитывается.»

**Демо 5 — debug controls (15 секунд):**
- `[` замедление, `]` ускорение, `\` покадровый шаг, `` ` `` сброс.
- «Это отладочные клавиши для inspection. Pause P и timeScale=0 — разные оси, не путаем.»

---

## 5. Out of scope — куда отправлять вопросы

| Вопрос про… | Говори |
|---|---|
| Почему Jolt, а не PhysX/Bullet | «Архитектурное решение le1t, обоснование в DefenseFAQ» |
| DOD / hot-cold split в физике | «Архитектура, le1t» |
| Voxel-мир, чанки, материалы | «К Тиммейту 2» |
| Тени / рендеринг | «К Тиммейту 3» |
| Билд-система / ctest | «К Тиммейту 1» |
| Демо VoxelLab / ассеты | «К Тиммейту 5» |
| Fluid CA | «Это часть воксельного мира, le1t» |

---

## 6. Если попросят «расскажите подробнее» (что вы можете раскрыть)

### Если спрашивают «почему walk authority = voxel solver, не Jolt»:
> «Jolt `CharacterVirtual` авторизует grounded через форму коллизии (capsule/box), не знает про структуру вокселей. Для плоских поверхностей это OK, но в voxel-мире нужны edge grace (тонкие края), sneak (sampled top-plane), top-promotion. **Мы НЕ заменили Jolt — `JPH::CharacterVirtual` остаётся для collision detection (капсула, прокси), наш voxel solver AUGMENTS его для foot support** (per `decisions.md §6`). Voxel solver знает раскладку чанков и может давать voxel-семантику поверх Jolt.'s capsule-based detection. Конкретно: `JPH::CharacterVirtual::ExtendedUpdate` с `BuildWalkEdgeGraceUpdateSettings()` (наша настройка) + наш `UpdateWalkGroundSupport` поверх для voxel-specific ground query.»

### Если спрашивают «что такое edge grace»:
> «Когда игрок стоит на краю блока, voxel'и под стопой могут быть чуть разной высоты (1/8 вокселя = 12.5 см). Без edge grace контроллер дёргал бы игрока вверх-вниз при micro-movement. В коде это `kWalkEdgeGraceFrames = 4` фрейма плюс `kWalkFootSupportEdgeGraceScore = 0.2f` — то есть счётчик фреймов и score, НЕ дистанция в метрах. Подобрано эмпирически.»

### Если спрашивают «как работает авто-прыжок»:
> «При включении J, каждый кадр проверяется: есть ли впереди блок высотой 1 (т.е. voxel над уровнем земли + воздух над ним). Если да — scheduled jump. Delay (F12): если ON, отсчёт начинается только когда reached=true. Иначе — мгновенно. Manual Space обнуляет delay. Полезно для parkour-style навигации.»

### Если спрашивают «зачем substepping в creative»:
> «В creative игрок может лететь быстро. Если deltaTime большой, Jolt может пропустить коллизию (туннелирование) — особенно через тонкие стенки. TickCreativeCharacter разбивает deltaTime на N подшагов (обычно 4-8), clamp'ит velocity. Стоимость: больше физических тиков, но нет прохождений сквозь стены.»

### Если спрашивают «в чём разница между creative и spectator»:
> «Creative — это полёт с collision, как в Minecraft creative mode. Сталкиваешься с блоками, можешь стоять на них, гравитация применяется. Spectator — это noclip, пролетаешь сквозь всё, как в Minecraft spectator mode. Другое: spectator игнорирует паузу P (полезно для cinematic-камеры). Переключение: F4 циклически walk → creative → spectator → walk. Двойной Space ↔ walk ↔ creative.»

### Если спрашивают «что такое voxel raycast»:
> «Amanatides-Woo 3D DDA, но через чанки (не через воксели). Старт в позиции камеры, направление = forward. На каждом шаге — переход в следующий чанк, lookup voxel в чанке. Возвращает hit point + normal. Normal используется для placement: ставим блок в adjacent ячейке по нормали. Max iterations: 64.»

### Если спрашивают «что такое sneak и зачем»:
> «Sneak (Shift) = sampled top-plane: контроллер сэмплирует только 1 точку под стопой (а не 4 угла). Это даёт стабильную поддержку на тонких краях без false-stick к стене. Без sneak игрок заходит за угол, контроллер думает «тут стена», прижимает к стене — неестественно. Со sneak — проходит мимо стены, контактируя только с тем, под чем стоит.»

### Если спрашивают «что такое edge grace численно»:
> «Эмпирические параметры в `PhysicsWorld.cpp:117-141`: `kWalkEdgeGraceFrames = 4` фрейма (НЕ 0.1 м) + `kWalkFootSupportEdgeGraceScore = 0.2f` (для движущегося `kWalkFootSupportMovingEdgeGraceScore = 0.5f`). Логика: `walkEdgeGraceFramesRemaining` счётчик; при `supportScore < EdgeGraceScore` → `walkEdgeGraceFramesRemaining = 4` (4 фрейма grace). Подобрано так, чтобы микро-перепады на плотных сценах (1/8 вокселя = 12.5 см) не дёргали игрока.»

### Если спрашивают «3 режима — какие hotkeys»:
> «F4 — циклическое переключение walk → creative → spectator → walk. Двойной Space (быстро 2 раза) — toggle walk ↔ creative. J — toggle авто-прыжка. F12 — toggle delay для авто-прыжка. P — пауза (timeScale=0, но это другая ось, чем `[`/`]`). `[` — замедление (до 0), `]` — ускорение (до 4×), `\` — покадровый шаг (1 фиксированный тик), `` ` `` — сброс к 1×.»

---

## 7. Cheat-card для печати (1 страница A4)

```
┌────────────────────────────────────────────────────────────────────────┐
│         BRIEFER 4 — Физика и walk controller (1:30)                   │
├────────────────────────────────────────────────────────────────────────┤
│ НАЧАЛО: "Добрый день, меня зовут [Имя Тимейта 4], я расскажу про      │
│          физику и контроллер игрока."                                  │
├────────────────────────────────────────────────────────────────────────┤
│ КЛЮЧЕВЫЕ ФАКТЫ:                                                        │
│  • Jolt Physics: MIT, SIMD, deterministic                              │
│  • CharacterVirtual = proxy, voxel solver = авторитетный                │
│  • 3 режима: walk / creative / spectator                                │
│  • F4 — циклически, двойной Space ↔ walk ↔ creative                    │
│  • Edge grace 0.1м, sneak (Shift), auto-jump (J)                       │
│  • Voxel raycast: 3D DDA через чанки                                    │
│  • Substepping в creative для high-velocity                             │
│  • Hotkeys: WASD, Space, J, F12, P, [, ], \, `, Shift, F4              │
├────────────────────────────────────────────────────────────────────────┤
│ 3 РЕЖИМА:                                                              │
│  • Walk: гравитация, ground support, edge grace                         │
│  • Creative: полёт, collision, substepping                              │
│  • Spectator: noclip, ignore pause, cinematic                           │
├────────────────────────────────────────────────────────────────────────┤
│ OUT OF SCOPE → le1t: выбор Jolt, DOD                                    │
│              → T1: билд-система                                         │
│              → T2: voxel-мир                                            │
│              → T3: рендеринг                                            │
│              → T5: демо                                                 │
├────────────────────────────────────────────────────────────────────────┤
│ ЕСЛИ СПРОСЯТ ГЛУБЖЕ:                                                   │
│  • Voxel solver vs Jolt: decisions.md §6, edge grace нужны              │
│  • Edge grace: 0.1м, микро-перепады не дёргают                          │
│  • Auto-jump: J toggle, F12 delay, scheduled по forward voxel          │
│  • Substepping: 4-8 подшагов в creative, anti-tunneling                 │
│  • Creative vs spectator: collision vs noclip, pause P                │
│  • Voxel raycast: Amanatides-Woo 3D DDA, hit + normal                  │
│  • Sneak: Shift, sampled top-plane, anti-false-stick                   │
│  • Hotkeys: F4 cycle, двойной Space, J/F12, P, [/], \, `               │
└────────────────────────────────────────────────────────────────────────┘
```

---

**Конец памятки.** Перед защитой: прочитать §2 вслух 3 раза с таймером, уложиться в 1:30 ± 5 секунд. Cheat-card [§7] распечатать.
