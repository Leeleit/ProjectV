# Fluid CA — column + hole + displace + elevated sidewalk

Date: 2026-07-17  
Status: implemented (CPU `UpdateFluidCA`; `ProjectVFluidCATests` green)  
Supersedes: `2026-07-16-fluid-ca-pressure-gate-design.md` (archived attempt)  
Executor: CPU `VoxelWorld::UpdateFluidCA` only (GPU research path stays OFF)

---

## 1. Цель (простым языком)

Бинарная вода (клетка целиком Fluid или нет), которая ведёт себя так:

1. **Ровная опора, дырок рядом нет** → стоим.
2. **Рядом дырка** (пустое место ниже или на том же этаже с опорой) → идём туда.
3. **Сидим на воде** → сначала двигается **низ** столба (ищет дырку / вытесняет соседа), верх проседает на его место.
4. **Снизу по бокам места нет** (вытеснить некого / некуда) → **верх** сам детерминированно уходит **вбок** на свободную
   клетку своего этажа (с опорой снизу) или шагом вниз-вбок.

Без Φ, lex-гейтов высоты, `placeYOnColumn` и запрета «height-1 не ходит».

---

## 2. Модель данных

| Понятие      | Значение                                                                |
|:-------------|:------------------------------------------------------------------------|
| Клетка       | `Air` / `Fluid` / `Solid` (Glass, Floor, …)                             |
| Масса        | `count(Fluid)`; за тик только обмен Air↔Fluid                           |
| `fluidFill`  | 0/255 storage; **не** участвует в логике CA v1                          |
| Колонна      | максимальный вертикальный run `Fluid` в (x,*,z)                         |
| База колонны | `Fluid`, снизу **не** `Fluid` (Air или Solid)                           |
| Elevated     | `Fluid`, снизу тоже `Fluid` (не база)                                   |
| Соседство XZ | Chebyshev ≤ 1 → до **8** соседей (3×3 без центра), диагонали **входят** |
| Рендер       | полный куб Fluid — ок для v1                                            |

---

## 3. Инварианты

| ID  | Правило                                                                       |
|:----|:------------------------------------------------------------------------------|
| M1  | Масса: только Air↔Fluid                                                       |
| M2  | Solid непроницаем                                                             |
| M3  | В тике сначала гравитация (FALL), потом горизонталь/слив                      |
| M4  | Нет подъёма: для любого хода агента `destY ≤ restY` агента                    |
| M5  | На этаже `restY` дырка только с опорой (Solid/Fluid снизу)                    |
| M8  | На этаже `restY−1` опора **не** обязательна (шаг в пропасть → дальше FALL)    |
| M6  | Детерминизм: фиксированный scan + claim + фиксированный порядок сторон        |
| M7  | Один исполнитель: CPU `UpdateFluidCA`                                         |
| M9  | Drain/displace-агент колонны — только **база**; при сдвиге claim всей колонны |
| M10 | Если база не освободила место, elevated может уйти вбок сам (фаза 4)          |

---

## 4. Дырка (единый предикат)

Агент в `(x, restY, z)` рассматривает кандидата `(nx, destY, nz)`:

**Кандидат валиден**, если одновременно:

1. Внутри мира; в `next` это **Air**; клетка **не claimed** (ни как dest, ни как уезжающий source).
2. `destY ∈ {restY − 1, restY}` (запрещён `restY + 1` и выше — анти-холм).
3. Опора под dest:
    - `destY == restY` → снизу Solid **или** Fluid (и опора не claimed как уезжающий source в этом тике);
    - `destY == restY − 1` → снизу Solid / Fluid / **Air** (unsupported разрешён).
4. Dest не Solid.

**Выбор среди кандидатов:**

1. Минимальный `destY` (сначала яма/пропасть, потом тот же этаж).
2. Tie → фиксированный порядок 8 смещений (константа, не hash от тика):

```
// (dx, dz) — порядок обязателен для детерминизма
( 0, -1),  // N
( 1,  0),  // E
( 0,  1),  // S
(-1,  0),  // W
( 1, -1),  // NE
( 1,  1),  // SE
(-1,  1),  // SW
(-1, -1),  // NW
```

Проверки Air/опоры — по **`next` после предыдущих фаз**, не по сырому `read` вслепую.

---

## 5. Операции

### 5.1 MoveCell (height-1 или elevated)

```
next[src] = Air
next[dest] = Fluid
claim(src, dest)
moved += 1
```

### 5.2 DrainBase (колонна H ≥ 2)

База уходит в `dest`, столб сдвигается вниз на 1:

```
// column: (x, bottomY..topY, z), bottomY == restY, H = topY - bottomY + 1
next[dest] = Fluid
for y = bottomY .. topY-1:
    next[x, y, z] = Fluid   // бывший y+1
next[x, topY, z] = Air
claim(dest + все клетки колонны bottomY..topY)
moved += 1   // один логический ход базы; масса сохранена
```

Эквивалент правилу (3): низ ушёл в дырку, всё выше просело на одну клетку.

### 5.3 TryHoleMove(agent)

Собрать кандидатов по §4 для `restY = agent.y`, выбрать лучший, выполнить MoveCell или DrainBase в зависимости от роли
агента.  
Вернуть `true`, если ход сделан.

---

## 6. Порядок тика (полный алгоритм)

```
read materials → next[]
claimed[] = false
moved = 0

───────── Phase 1: FALL ─────────
scan y = minY .. maxY-1, затем z, x:          // снизу вверх
  cell = (x,y,z)
  if next[cell] != Fluid or claimed[cell]: continue
  below = (x,y-1,z)
  if !inside(below): continue
  if next[below] == Air and !claimed[below] and !claimed[cell]:
      next[cell]=Air; next[below]=Fluid
      claim(cell, below); moved += 1

───────── Phase 2: BASE_HOLE ─────────
// Базы идут в уже существующую дырку (Air)
scan y, z, x (снизу вверх):
  if next[cell] != Fluid or claimed[cell]: continue
  if below is Fluid: continue                  // не база
  if below is Air: continue                   // должен был упасть в Phase 1
  restY = y
  H = высота Fluid-колонны вверх от cell (по next, пока Fluid)
  if TryHoleMove as base (H==1 → MoveCell, H≥2 → DrainBase):
      // done for this column
  else:
      // нет дырки — возможно Phase 3

───────── Phase 3: BASE_DISPLACE ─────────
// Низ столба H≥2 просит соседа на том же этаже отойти в дырку
scan y, z, x (снизу вверх):
  if next[cell] != Fluid or claimed[cell]: continue
  if below is Fluid: continue
  H = column height from cell
  if H < 2: continue                          // вытеснение только от давления столба
  // повторно: если дырка уже появилась (край ушёл в Phase 2) — просто DrainBase
  if TryHoleMove(base): continue

  for each side in SIDE_ORDER:                // детерминированно
      npos = (x+dx, y, z+dz)
      if next[npos] != Fluid or claimed[npos]: continue
      // просим отойти только «лужу» height-1 (база с H==1)
      if below(npos) is Fluid: continue       // не трогаем чужой elevated
      nH = column height at npos
      if nH != 1: continue
      if !TryHoleMove(neighbor as height-1): continue
      // сосед ушёл → его клетка Air; база сливается туда
      dest = npos                             // гарантированно Air + опора/правила после ухода
      if dest still valid hole for base (Air, unclaimed support rules):
          DrainBase(cell → dest)
          break
      // если dest почему-то невалиден — ищем следующего соседа

───────── Phase 4: ELEVATED_SIDEWALK ─────────
// Снизу места не сделали — верх уходит вбок сам
scan y, z, x (снизу вверх):
  if next[cell] != Fluid or claimed[cell]: continue
  if below is not Fluid: continue             // только elevated
  restY = y                                   // этаж ЭТОЙ клетки, не базы
  // агент = одна клетка (не DrainBase): TryHoleMove → MoveCell
  // кандидаты: restY-1 и restY по §4 (вбок / вниз-вбок; не вверх)
  TryHoleMove(elevated as single cell)

───────── Commit ─────────
write next → world materials / fluidFill 0|255
обновить fluidCAAabb / stats.fluidVoxelCount
return moved
```

### Почему такой порядок

| Фаза        | Зачем                                                  |
|:------------|:-------------------------------------------------------|
| 1 FALL      | Висящие садятся; M3                                    |
| 2 BASE_HOLE | Обычный слив/растекание в готовую пустоту              |
| 3 DISPLACE  | Правило (3): низ под давлением выталкивает соседа лужи |
| 4 ELEVATED  | Правило (4): если низ не смог — верх сам уходит вбок   |

Bottom-up по Y: нижняя вода двигается раньше; верх в том же тике либо проседает через DrainBase, либо на следующем тике
падает в освободившееся Air.

---

## 7. Поведение на ключевых сценах

### 7.1 Одна клетка над Air

Phase 1: −1 Y / тик.

### 7.2 Столб на полу + дырка сбоку (пол / яма)

Phase 2: база → дырка, DrainBase, высота колонны −1 за тик.

### 7.3 Плоская лужа 3×3, снаружи Air, без башни

Phase 2: края (height-1) видят Air на `restY` → расползаются, пока есть куда.  
Если все дырки заняты/нет → `moved==0` (стабильная лужа произвольной формы на полу).

### 7.4 Лужа 3×3 + одна вода сверху в центре (открытый пол)

1. Края Phase 2 уходят наружу **или** центр Phase 3 просит край отойти.
2. Появляется Air у базы центра → DrainBase → башня проседает.
3. Повторять, пока maxH не станет 1 (нужно ≥10 клеток пола — лужа обязана расшириться).

### 7.5 Лужа 3×3 + капля сверху, **по бокам снизу места нет**

Стены / замкнутый карман / нет Air на этаже пола в досягаемости вытеснения:

- Phase 2–3: базы не находят дырок, соседей вытеснить некуда.
- Phase 4: капля в центре на `y+1` видит сбоку Air на своём этаже с опорой = Fluid лужи → **детерминированно** (порядок
  сторон) переезжает на соседнюю колонну.
- Второй слой расползается по поверхности лужи.
- Полностью сплющить в один слой **нельзя**, если `count(Fluid) >` числа доступных клеток пола — это не баг, а
  сохранение массы (M1).

### 7.6 Кратер глубины ≥ 2

Кромка: Phase 2/4 шаг на `restY−1` без опоры (M8) → следующие тики FALL до дна.

### 7.7 Пробой сферы / стекло

Solid не туннелится (M2). После появления дырки — fall + hole + displace + elevated sidewalk до settle.

---

## 8. Чего сознательно нет

- Φ_h / lex / period-2 хаки высоты
- `placeYOnColumn` / укладка на `top+1` соседа
- «lone height-1 никогда не ходит»
- Fractional fill в логике
- Ход глубже чем `restY−1` за один шаг (вместо этого FALL)
- Вытеснение elevated-соседей и колонн H≥2 в Phase 3 (v1: только height-1 лужа)
- Sparse active-set (позже; v1 = fluid AABB + next/claimed)

---

## 9. Анти-осцилляция (v1 — минимально)

- Claim + порядок фаз убивают same-tick swap.
- Vacated source в том же тике не является dest для других.
- Многотиковый период-2 `F . F` ↔ `. F F` на полу: **не** чинить Φ; если проявится в тестах/ASCII — добавить узкое
  правило только для same-level height-1: ход разрешён при строгом росте числа Fluid-соседей у dest (не трогает
  `destY < restY` и Phase 4 на поверхности лужи без необходимости).
- В v1 **не** включать это правило по умолчанию, пока тест 8 не красный на реальном settle.

---

## 10. Сложность и реализация

- Память тика: `next` материалов (или packed) + `claimed` bitset по AABB fluid (expand +1 по XZ и −1..0 по Y для дырок).
- Время: O(AABB volume) на фазу; Phase 3/4 фактически O(число баз / elevated).
- Файлы: `src/voxel/VoxelWorldFluid.cpp` (+ при необходимости private helpers в том же TU / `VoxelWorldInternal.hpp`).
- Тесты: восстановить target `ProjectVFluidCATests` → `tests/FluidCATests.cpp` под этот контракт.
- Документы после merge в код: `agent/knowledge.md` §8, `agent/workspace.md`.

---

## 11. Тесты-якоря

| #  | Сценарий                                    | Ожидание                                                            |
|:---|:--------------------------------------------|:--------------------------------------------------------------------|
| 1  | Одна Fluid над Air                          | −1 Y / тик; масса = 1                                               |
| 2  | Колонна H=3 на полу + Air сбоку на полу     | база уходит; H падает                                               |
| 3  | Плоская лужа без дырок (закрытый карман)    | после settle `moved==0`                                             |
| 4  | Лужа + Air с полом снаружи                  | расползание; maxH → 1                                               |
| 5  | 3×3 лужа + 1 сверху в центре, открытый пол  | за конечное N тиков maxH=1; масса=10                                |
| 6  | 3×3 лужа + 1 сверху, стены без пола снаружи | Phase 4: верх уходит вбок по поверхности; масса=10; maxH≥2 допустим |
| 7  | Кратер depth≥2, Fluid на ободе              | масса на дне за конечное N                                          |
| 8  | Сфера/стекло break (fixture)                | не туннелит Solid; settle maxH≤2 (или ≤1 если геометрия позволяет)  |
| 9  | Масса до/после каждого тика равна           | M1                                                                  |
| 10 | Два прогона одинакового мира                | побитово одинаковый next (M6)                                       |

---

## 12. Соответствие правилам оператора

| Правило оператора                     | Где в алгоритме                            |
|:--------------------------------------|:-------------------------------------------|
| Ровная опора → стоим                  | Нет кандидата в §4 → все фазы skip         |
| Дырка → идём                          | Phase 2 / 4 TryHoleMove                    |
| На воде: сначала низ                  | Phase 2–3 на базе; DrainBase сдвигает верх |
| Нет места снизу по бокам → верх вбок  | Phase 4 ELEVATED_SIDEWALK                  |
| Шаг в неподдерживаемый Air на restY−1 | M8 в предикате дырки                       |
| Детерминизм                           | scan order + SIDE_ORDER + claim            |

---

## 13. Вердикт по подходу

**B′+displace+elevated** — один каркас:

- агент колонны = база;
- дырка = Air с правилами опоры/пропасти;
- давление сверху = displace соседа height-1;
- тупик снизу = elevated sidewalk.

Подходы C (equalize/Φ) и A (каждая клетка без колонны) — отклонены.

---

## 14. Approval gate

Перед кодом оператор подтверждает этот документ (особенно Phase 3 только height-1 neighbor и Phase 4 при тупике
снизу).  
После approval → implementation plan: `docs/superpowers/plans/2026-07-17-fluid-ca-column-hole-displace.md`.
