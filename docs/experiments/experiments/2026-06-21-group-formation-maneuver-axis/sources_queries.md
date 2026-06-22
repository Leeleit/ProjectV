# Web-search query batch — `2026-06-21-group-formation-maneuver-axis`

> Web-search (Exa) HTTP 429 persistent → запрошен operator batch fallback per
> `agent/knowledge.md Part B §9` line 1424 fallback list.
> См. также backlog.md §In progress reservation запись.

**Slug:** `2026-06-21-group-formation-maneuver-axis`
**Topic:** Group formation movement & slot allocation (column/line/wedge/echelon), per-unit steering +
cohesion maintenance, combat casualty reduction, ORCA-style collision avoidance для 256+ units
в formation pattern.

**Метод работы с batch:** оператор выполняет 10 запросов, возвращает top-10 results per query
(title + url + snippet) → я фильтрую unique authoritative sources (academic, primary dev blog,
game AI postmortem, GitHub code) → 8-12 primary sources → `sources.md` + `webfetch` для
верификации цитат (год, автор, точное assertion).

---

## Query 1 (canonical formation AI)

```
Reynolds 1987 flocks herds schools formation extension steering behaviors game AI
```

**Why:** Craig Reynolds canonical formation / boids paper + sequels. Прямое расширение steering
behaviors на formation maintenance (separation/alignment/cohesion + leader/follower).

## Query 2 (slot allocation)

```
virtual anchor fluid based slot allocation RTS game formation 256 units
```

**Why:** Slot allocation algorithm — anchor-based vs fluid-based vs hash-grid. Kinetik 2026
blog post (в backlog ссылка). Цель: найти 2-3 современных бенчмарка для slot assignment.

## Query 3 (Warno / Eugen Systems AI)

```
Warno Eugen Systems formation AI tactical pathfinding platoon wedge echelon
```

**Why:** Warno — modern RTS-симулятор с детальной formation system (platoons in column/line/wedge).
Eugen Systems известны глубокой AI документацией (Wargame series, Steel Division).

## Query 4 (SupCom formation system)

```
Supreme Commander Gas Powered Games formation AI cohesion squad pathfinding
```

**Why:** SupCom = canonical reference для "hundreds of units in formation". Сравнить
flow-field-based pathfinding (per closed `flow-field-pathfinding-10k-units` [yes]) с
formation-aware extensions.

## Query 5 (ORCA formation collision avoidance)

```
Optimal Reciprocal Collision Avoidance ORCA formation group 256 agents benchmark
```

** Why:** ORCA — стандартный алгоритм collision avoidance для groups (van den Berg 2008/2010).
Fallback для tight scenarios (city streets, narrow bridges). Benchmark costs.

## Query 6 (Map Marker algorithm)

```
SBGames 2021 map marker algorithm formation combat casualties reduction
```

**Why:** SBGames 2021 paper (per backlog §In progress reference) — Map Marker = pathfinding
enhancement. Цель: измерить impact на combat casualties в formation contexts.

## Query 7 (Massive Software formation, Andersson 2008)

```
Massive Software Andersson 2008 formation crowd simulation slot allocation
```

**Why:** Massive = industry-standard crowd/formation simulation (Lord of the Rings trilogy).
Andersson 2008 paper — academic reference для formation в больших группах.

## Query 8 (HoI4 organization / combat width)

```
Hearts of Iron 4 organization combat width formation width penetration
```

**Why:** HoI4 — лучший пример "formation как тактическая механика" (combat width 80-120,
поддерживающие vs атакующие дивизии в formation). Формализм: 2D grid, slot = combat position.

## Query 9 (synthetic benchmarks / methodology)

```
formation movement benchmark CPU cost 256 units 2024 2025 2026 microbenchmark
```

**Why:** Найти готовые бенчмарки (Box2D, Unity DOTS, Flecs + formation, Godot 4.x formation).
Методологическая опора для моего standalone C++26 CPU prototype.

## Query 10 (game AI formation postmortem / devblog)

```
game AI postmortem formation cohesion Total War Creative Assembly Halo 2 formation
```

**Why:** Devblog/postmortem (Creative Assembly Total War, Bungie Halo 2 per Isla 2005,
Guerrilla Killzone, Creative Assembly Warhammer). Конкретные production numbers.

---

## Progress

- [ ] Query 1 (Reynolds)
- [ ] Query 2 (slot allocation)
- [ ] Query 3 (Warno)
- [ ] Query 4 (SupCom)
- [ ] Query 5 (ORCA)
- [ ] Query 6 (Map Marker)
- [ ] Query 7 (Massive Software)
- [ ] Query 8 (HoI4)
- [ ] Query 9 (benchmarks)
- [ ] Query 10 (game AI postmortem)

## After batch

После получения top-10 results per query → 80-100 unique URLs → фильтрую до ~12 primary
(academic/conference/GDC/devblog/GitHub) → `webfetch` для verification цитат
(title/abstract/figure/key assertion) → `sources.md` с verified per AGENTS.md §13.1.
