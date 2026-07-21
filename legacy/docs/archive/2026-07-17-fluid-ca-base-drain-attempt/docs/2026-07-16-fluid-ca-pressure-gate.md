# Fluid CA pressure-gate — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:
> executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Остановить осцилляцию одиночного Fluid на ровной опоре: горизонтальный spread только при давлении сверху (
сосед `y+1` = Fluid).

**Architecture:** Одна проверка в CPU `UpdateFluidCA` перед блоком cardinal spread. Тесты TDD: сначала settle +
pressure-drain (fail), потом gate, потом починка устаревших ожиданий. GPU-шейдер не трогаем.

**Tech Stack:** C++26, `VoxelWorld` / `UpdateFluidCA`, `ProjectVFluidCATests`, preset `windows-clang-debug`.

**Spec:** `docs/superpowers/specs/2026-07-16-fluid-ca-pressure-gate-design.md`

## Global Constraints

- Бинарный Fluid (без уровней / 0.1 м / испарения) — этот инкремент.
- Детерминизм: z→y→x, `claimed`, integer-only (как `agent/knowledge.md` §8).
- Не менять `src/shaders/fluid_ca.comp`.
- Коммиты — **только** по явной команде оператора (`AGENTS.md` §5.1); в шагах «Commit» — предложить message, не
  выполнять без приказа.
- Перф/DoD на Debug: `build/windows-clang-debug`.

## File map

| File                            | Role                                                    |
|---------------------------------|---------------------------------------------------------|
| `src/voxel/VoxelWorldFluid.cpp` | Pressure-gate перед horizontal spread (~строки 151–182) |
| `tests/FluidCATests.cpp`        | Settle / drain / переписать wander-тесты / main()       |
| `agent/knowledge.md` §8         | Одна строка про pressure-gate                           |
| `agent/workspace.md`            | Короткий snapshot после закрытия (DoD)                  |

---

### Task 1: Failing tests — settle + pressure drain

**Files:**

- Modify: `tests/FluidCATests.cpp`
- Test: `ProjectVFluidCATests`

**Interfaces:**

- Consumes: `MakeFluidCATestWorld`, `UpdateFluidCA`, `SetVoxelMaterial`, `GetVoxelMaterial`,
  `VoxelMaterial::{Fluid,FloorWhite,Air,Glass}`
- Produces: `TestFluidCALoneCellOnSupportStaysPut`, `TestFluidCAColumnPressureDrainsHorizontally` (+ регистрация в
  `main`)

- [ ] **Step 1: Добавить тест settle одиночной клетки**

Вставить рядом с `TestFluidCARestingOnFloorStaysPut` (после ~строки 121):

```cpp
void TestFluidCALoneCellOnSupportStaysPut(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);

	for (int tick = 0; tick < 10; ++tick) {
		const uint32_t moved = UpdateFluidCA(world);
		EXPECT_EQ(context, 0u, moved);
		EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 1, 2}));
		EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(world, {2, 0, 2}));
	}
	EXPECT_EQ(context, 1u, world.stats.fluidVoxelCount);
}
```

- [ ] **Step 2: Добавить тест pressure drain столбца**

```cpp
void TestFluidCAColumnPressureDrainsHorizontally(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 6, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Fluid, nullptr);

	const uint32_t initialCount = world.stats.fluidVoxelCount;
	EXPECT_EQ(context, 2u, initialCount);

	bool sawHorizontalNeighbour = false;
	for (int tick = 0; tick < 20; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, initialCount, world.stats.fluidVoxelCount);
		const bool nxp = GetVoxelMaterial(world, {3, 1, 2}) == VoxelMaterial::Fluid;
		const bool nxm = GetVoxelMaterial(world, {1, 1, 2}) == VoxelMaterial::Fluid;
		const bool nzp = GetVoxelMaterial(world, {2, 1, 3}) == VoxelMaterial::Fluid;
		const bool nzm = GetVoxelMaterial(world, {2, 1, 1}) == VoxelMaterial::Fluid;
		if (nxp || nxm || nzp || nzm) {
			sawHorizontalNeighbour = true;
			break;
		}
	}
	EXPECT_TRUE(context, sawHorizontalNeighbour);
}
```

- [ ] **Step 3: Зарегистрировать оба теста в `main()`**

Перед `TestFluidCARestingOnFloorStaysPut(context);`:

```cpp
	TestFluidCALoneCellOnSupportStaysPut(context);
	TestFluidCAColumnPressureDrainsHorizontally(context);
```

- [ ] **Step 4: Собрать и прогнать — ожидать FAIL на settle**

```powershell
cmake --build build/windows-clang-debug --target ProjectVFluidCATests
ctest --test-dir build/windows-clang-debug -R ProjectVFluidCATests --output-on-failure
```

Expected: FAIL — `TestFluidCALoneCellOnSupportStaysPut` (`moved != 0` и/или клетка уехала). Drain может PASS уже на
старом коде (столб и так растекается).

- [ ] **Step 5: Предложить commit (не выполнять без приказа оператора)**

```
test(fluid): add settle and pressure-drain expectations

Lock lone-cell settle and column horizontal drain before pressure-gate.
```

---

### Task 2: Pressure-gate в `UpdateFluidCA`

**Files:**

- Modify: `src/voxel/VoxelWorldFluid.cpp` (блок spread ~151–182)
- Test: `ProjectVFluidCATests`

**Interfaces:**

- Consumes: локальные `next`, `index`, `lx/ly/lz`, `width/height/depth`
- Produces: spread выполняется только если `ly + 1 < height` и `next[index(lx, ly+1, lz)] == Fluid`

- [ ] **Step 1: Вставить gate перед horizontal spread**

Заменить блок начиная с `{` после fall/`continue` (сейчас безусловный spread) на:

```cpp
				{
					const bool hasPressureAbove =
						(ly + 1 < height) &&
						(next[index(lx, ly + 1, lz)] ==
						 static_cast<uint8_t>(VoxelMaterial::Fluid));
					if (!hasPressureAbove) {
						continue;
					}

					const uint32_t h = lx * 73856093u ^ ly * 19349663u ^ lz * 83492791u;
					constexpr int sides[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
					const int startSide = static_cast<int>(h & 0x3u);

					const int dirs[2] = {startSide, startSide + 1 & 0x3};
					int spreadDir = -1;
					for (int d = 0; d < 2; ++d) {
						const int sideIdx = dirs[d];
						const int nlx = lx + sides[sideIdx][0];
						const int nlz = lz + sides[sideIdx][1];
						if (nlx < 0 || nlx >= width || nlz < 0 || nlz >= depth) {
							continue;
						}
						const size_t neighbourIdx = index(nlx, ly, nlz);
						if (next[neighbourIdx] == static_cast<uint8_t>(VoxelMaterial::Air)) {
							spreadDir = d;
							break;
						}
					}
					if (spreadDir >= 0) {
						const int sideIdx = dirs[spreadDir];
						const int nlx = lx + sides[sideIdx][0];
						const int nlz = lz + sides[sideIdx][1];
						const size_t neighbourIdx = index(nlx, ly, nlz);
						next[idx] = static_cast<uint8_t>(VoxelMaterial::Air);
						next[neighbourIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);
						claimed[idx] = 1u;
						claimed[neighbourIdx] = 1u;
						++movedCount;
					}
				}
```

Комментарий — одной строкой после `hasPressureAbove` check, если нужен:
`// pressure-gate: lone cell on support does not wander`.

- [ ] **Step 2: Прогнать новые тесты**

```powershell
cmake --build build/windows-clang-debug --target ProjectVFluidCATests
ctest --test-dir build/windows-clang-debug -R ProjectVFluidCATests --output-on-failure
```

Expected: `TestFluidCALoneCellOnSupportStaysPut` и `TestFluidCAColumnPressureDrainsHorizontally` PASS. Возможны FAIL на
старых wander-тестах — это Task 3.

- [ ] **Step 3: Предложить commit**

```
fix(fluid): gate horizontal spread on vertical pressure

Lone fluid on support settles; columns still drain sideways.
```

---

### Task 3: Починить устаревшие wander-ожидания

**Files:**

- Modify: `tests/FluidCATests.cpp` — `TestFluidCASpreadsToCardinalNeighbour`,
  `TestFluidCAFluidDoesNotFallThroughPlatform`, при необходимости `TestFluidCARestingOnFloorStaysPut` /
  `TestFluidCASpreadIsDeterministic`
- Test: `ProjectVFluidCATests`

**Interfaces:**

- Consumes: тот же `UpdateFluidCA`
- Produces: тесты соответствуют pressure-gate (не требуют бродячую lone cell)

- [ ] **Step 1: Переписать `TestFluidCASpreadsToCardinalNeighbour`**

Заменить тело на проверку «без pressure — нет spread» (имя можно оставить или переименовать в
`TestFluidCALoneCellDoesNotSpreadWithoutPressure`; если переименовать — обновить `main`):

```cpp
void TestFluidCALoneCellDoesNotSpreadWithoutPressure(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);

	const uint32_t moved = UpdateFluidCA(world);
	EXPECT_EQ(context, 0u, moved);
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 1, 2}));
	EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {2, 0, 2}));
	EXPECT_EQ(context, 1u, world.stats.fluidVoxelCount);
}
```

В `main`: заменить вызов `TestFluidCASpreadsToCardinalNeighbour` на новое имя (если переименовали). Удалить старую
функцию, чтобы не осталось мёртвого кода.

- [ ] **Step 2: Исправить `TestFluidCAFluidDoesNotFallThroughPlatform`**

```cpp
void TestFluidCAFluidDoesNotFallThroughPlatform(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);

	const uint32_t moved = UpdateFluidCA(world);
	EXPECT_EQ(context, 0u, moved);
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(world, {2, 0, 2}));
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 1, 2}));
	EXPECT_EQ(context, 1u, world.stats.fluidVoxelCount);
}
```

- [ ] **Step 3: Усилить `TestFluidCARestingOnFloorStaysPut` (позиция)**

Добавить внутрь цикла тиков:

```cpp
		EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 1, 2}));
```

- [ ] **Step 4: Прогнать полный FluidCA suite**

```powershell
cmake --build build/windows-clang-debug --target ProjectVFluidCATests
ctest --test-dir build/windows-clang-debug -R ProjectVFluidCATests --output-on-failure
```

Expected: `ProjectVFluidCATests passed` / ctest green. Если `TestFluidCASpreadIsDeterministic` падает — прогнать
глазами: после gate lone cell не двигается, snapshot должен стать стабильнее; поправить только если assertion ломается (
ожидание «движение» → «неподвижность»).

- [ ] **Step 5: Предложить commit**

```
test(fluid): align FluidCA tests with pressure-gate

Drop lone-cell wander expectations; assert settle on support.
```

---

### Task 4: Docs + DoD snapshot

**Files:**

- Modify: `agent/knowledge.md` §8
- Modify: `agent/workspace.md` §1 (короткая строка)
- Spec DoD checkboxes в `docs/superpowers/specs/2026-07-16-fluid-ca-pressure-gate-design.md`

**Interfaces:**

- Consumes: реализованное правило
- Produces: актуальный contract + workspace note

- [ ] **Step 1: Дописать в `agent/knowledge.md` §8 после CPU reference**

```markdown
- **Pressure-gate (2026-07-16):** horizontal spread только если клетка
  непосредственно сверху = `Fluid`; одиночный Fluid на опоре settled
  (`VoxelWorldFluid.cpp`). Уровни / испарение / GPU parity — later.
```

- [ ] **Step 2: Строка в `agent/workspace.md` §1 Now**

```markdown
**2026-07-16 Fluid CA pressure-gate:** lone Fluid on support settles;
column drain via vertical pressure only. Spec:
`docs/superpowers/specs/2026-07-16-fluid-ca-pressure-gate-design.md`.
```

- [ ] **Step 3: Отметить DoD checkboxes в spec**

Все пункты DoD → `[x]` после green ctest.

- [ ] **Step 4: Финальная верификация**

```powershell
cmake --build build/windows-clang-debug --target ProjectVFluidCATests
ctest --test-dir build/windows-clang-debug -R ProjectVFluidCATests --output-on-failure
```

Expected: PASS. (Полный `ctest` без `-R` — по желанию оператора; минимум — FluidCA.)

- [ ] **Step 5: Предложить commit**

```
docs(fluid): record pressure-gate contract

Knowledge §8 + workspace snapshot for lone-cell settle rule.
```

---

## Self-review (plan vs spec)

| Spec requirement             | Task                                 |
|------------------------------|--------------------------------------|
| Pressure-gate CPU rule       | Task 2                               |
| Lone settle test             | Task 1 + 3                           |
| Column drain test            | Task 1                               |
| Fix wander tests             | Task 3                               |
| knowledge.md §8              | Task 4                               |
| No GPU shader change         | Global + нет task на `fluid_ca.comp` |
| windows-clang-debug green    | Tasks 1–4 commands                   |
| Levels / evaporation / cliff | Out of scope — нет tasks             |

Placeholders: нет. Имена тестов согласованы между Task 1 и 3.
