# Q&A Log — Roadmap v2.0 (2026-06-25)

Полный журнал решений Q&A сессии оператора + root-agent, приведшей к ROADMAP v2.0.
Источник решений для всех invariants, zone charters, slice specs, validation pipeline,
mod architecture. Любые будущие изменения v2.0 — через новый explicit Q&A.

**Сессия:** 2026-06-25, plan mode → build mode.
**Operator:** project owner.
**Agent:** root-agent.
**Раундов:** 6.
**Вопросов:** ~30.
**Выходной артефакт:** `ROADMAP.md` v2.0 + `agent/knowledge.md` §38-43.

---

## Round 1 — structural contradictions (начало)

**Agent framing:** «Round 1 — самые фундаментальные. Phase 0 vs Phase 1,
multi-axis parallel vs sequential, multi-agent deployment, lockstep vs server-auth,
LuaJIT determinism, world persistence, memory budget, visual gate.»

### A1. Phase 0 vs Phase 1 — реальная граница?

**Agent:** Phase 0 и Phase 1 почти полностью перекрываются по содержанию. Это одна
фаза, искусственно разорванная? Или две с разной семантикой?

**Operator:** «Это всё нулевой этап, значит, это ошибка»

**Решение:** Phase 0+1 слиты в один этап «visual». Нет отдельного Phase 1.

### A2. Q1 «multi-axis parallel» vs строго последовательные фазы (ГЛАВНОЕ)

**Agent:** Q1 оператор выбрал D = multi-axis parallel development. Но §3 фазы 2→3→4→5
идут строго последовательно. Три варианта reconciliation: (a) sequenial, Q1 относится
только к multi-agent infra; (b) с Phase 0.5/1 стартует настоящий parallel; (c) гибрид.

**Operator:** **b**

**Решение:** Parallel-from-Phase-0.5. Z1/Z2/Z3/Z4 работают параллельно. Gates
per-zone, не глобально-последовательные.

### A3. Phase 0.5 timing

**Agent:** Phase 0.5 ставит refactor 170+ experiments после Phase 0 (DP-1) и до
Phase 1 (DP-2). Это блокирует parallel work. Может, Phase 0.5 делать параллельно с
Phase 0?

**Operator:** «можно»

**Решение:** Phase 0.5 параллельно с Phase 0 visual work (root-agent + Z4 в фоне).

### B1. Как практически запускаются 5 агентов?

**Agent:** Deployment model не описан. Каждый agent = отдельный opencode-процесс?
Один процесс с subagent tool'ом?

**Operator:** «5 отдельных opencode агентов со своим контекстом»

**Решение:** 5 parallel sessions, каждая со своим context window. Коммуникация через
файлы (`/proposals/`, `agent/workspace.md`).

### B2. CI/CD нужен сейчас, не в Phase 8

**Agent:** Phase 8 §5.2 описывает GitHub Actions для CI. Но если 5 агентов пишут код
параллельно с Phase 0.5, без CI = постоянный integration ад. Согласован?

**Operator:** «согласен»

**Решение:** CI/CD переезжает из Phase 8 в Phase 0.5.

### C1. Lockstep vs server-authority для 100-1000 игроков (ГЛАВНОЕ)

**Agent:** Phase 3 ставит lockstep. Phase 7 — Foxhole-style 100-1000 player. Но
Foxhole использует server-authoritative, не lockstep. Три варианта: (a) lockstep
для coop + server-auth для massive; (b) всё server-auth; (c) пытаться lockstep на 100+.

**Operator:** **a + c как эзотерический эксперимент**

**Решение:** Main path = lockstep для 2-8 coop + server-authoritative для massive
war (как Foxhole). Lockstep-at-100+ = esoteric Z4 research, не main path.

### C2. LuaJIT vs Phase 3 determinism

**Agent:** Phase 2 выбирает LuaJIT. Phase 3 требует determinism. JIT compilation
non-deterministic. Tension. Решить upfront или отложить?

**Operator:** «мне всё равно, что использовать, лишь бы оправдывало себя, надо решить»

**Решение:** Agent сделал websearch, нашёл чистое решение (mode-switch per path).
Подробнее Round 2 C2.

### C3. DLSS Open Question contradiction

**Agent:** Phase 0 sub-milestone делает Streamline/DLSS integration сейчас. Но §8.2
Q4 ставит «DLAA / Streamline integration» как Open Question для Phase 1.

**Operator:** «прямо сейчас агент параллельно делает, считай, что перед началом работ
по этому roadmap, всё уже сделано по графике»

**Решение:** Phase 0 DLSS work = pre-roadmap baseline. Q4 закрыт.

### D1. World persistence architecture — отсутствует

**Agent:** Phase 7 требует persistent world. Architecture не описана. Database?
Flat snapshots? Journal?

**Operator:** «не знаю, надо исследовать: типа postgres как универсальный инструмент
крут, но есть какой-нибудь dragonfly, тот же sqlite (хоть он и говно), надо подумать»

**Решение:** Z4 research task. Agent предложил scope (5 backends benchmark).
Подробнее Round 2 D1.

### D2. Memory budget breakdown на 100-player server

**Agent:** Phase 6 DoD: «200 MB/player». 100 × 200 = 20 GB. Phase 7 (1000 player) =
200 GB. На 16GB сервер нереально.

**Operator:** «нет никакого сервера на 16гб, я просто сказал, что должна быть
возможность headless projectv server ставить на сервера, агент сгаллюцинировал, надо
уточнить реальные бюджеты и ориентируйся на мой компьютер в качестве сервера,
спецификации читай в docs/experiments/hardware-profile.md»

**Решение:** Agent прочитал hardware-profile (Ryzen 7 5800X, 64 GiB RAM). 100 players
× 256-384 MiB = 32 GiB реально. 1000+ требует datacenter. Подробнее Round 2 D2.

### D3. Visual gate «не игрушечно» — нужен критерий

**Agent:** Без concrete metric оператор может застрять или пропустить gate.

**Operator:** «я есмь критерий: мне понравится – завершаем, не понравится – продолжаем
работать над графикой.»

**Решение:** Subjective gate. Метрик нет и не нужно.

---

## Round 2 — research-informed follow-ups

### C2. LuaJIT determinism — нашёл чистое решение

**Agent:** Websearch подтвердил (LuaJIT issue #1153, 2024; issue #1008 `-bd` flag):

- LuaJIT JIT = non-deterministic
- LuaJIT `-joff` (interpreter) = deterministic, ~5-10× faster than Lua 5.4
- Factorio reference.

**Предложение:** mode-switch per path:

- Single-player / sandbox → JIT on
- Small coop (lockstep) → `-joff`
- Server-authoritative → JIT on сервера
- Esoteric 100+ lockstep → `-joff` + cross-platform testing

**Operator:** «даю добро, но боюсь, что текущий агент запутается, если ты будешь
обновлять workspace, knowledge файлы»

**Решение:** Contract approved → `agent/knowledge.md §38`. Agent не обновляет
workspace/knowledge без необходимости (в plan mode нельзя; в build mode один
скоординированный pass).

### D1. Persistence backend — proposing Z4 research task

**Agent:** 5 backends compared (Postgres / SQLite / Dragonfly-Redis / LMDB / custom
binary / journal+snapshot). Hybrid architecture proposed. Z4 research scope: benchmark

+ prototype + architecture + recovery.

**Operator:** «да»

**Решение:** Z4 task #1 scoped.

### D2. Memory budget — пересчитан под operator's machine

**Agent:** Hardware-profile: Ryzen 7 5800X, 64 GiB RAM, RTX 3060 Ti. Realistic
dedicated server = 100 players × 256-384 MiB = 32 GiB. 1000+ требует datacenter.

**Operator:** «если что, физически у мня 64гб оперативы, 1000 игроков – это
необозримое будущее, принимаю»

**Решение:** Memory budget зафиксирован в `agent/knowledge.md §39`. 16GB hallucination
удалена из roadmap.

### E1. Конкретное наполнение зон (parallel с Phase 0.5)

**Agent:** Предложил начальное распределение work per zone (Z1 persistence+streaming,
Z2 JSON+LuaJIT+SDF, Z3 server-auth MP foundation, Z4 persistence research).

**Operator:** «пойдёт, но мультиплеер сейчас совсем не в приоритете, например; что за
shared api? Что это?; да, Z1»

**Решение:**

- Zone composition принято
- **MP deprioritized** (см. F1 в Round 3)
- Shared API concept объяснен в Round 3 F2
- Z1 owns shared API

### E2. Per-zone gates vs global phase gates

**Agent:** При A2=b текущая gate-таблица некорректна. Per-zone gates proposed.

**Operator:** «пойдёт»

**Решение:** Per-zone gates (§14 ROADMAP v2.0).

### E3. Audio subsystem — критический пробел

**Agent:** 7+ closed audio experiments без integration path. Proposed: Z1 (core
engine), Z3 (gameplay audio), Z4 (research).

**Operator:** «Согласен»

**Решение:** Audio integration path зафиксирован.

### E4. Mod conflict resolution algorithm (Phase 2 dependency)

**Agent:** 4 options (Skyrim-style / Doom-style / Factorio-style / HoI4-style).

**Operator:** «не хочу опираться на чужое, хочу чтобы было невозможно получить ошибки
при несовместимых модах, то есть на этапе применения модов отклонять моды, если
появляются несоответствия в версии, дублировании файлов, а решение всех проблем на
разработчиков полагать: делать проверки по типу "если установлен вместе с этим модом
мод X, который заменяет определённые модельки или наоборот этот мод заменяет модельки
у X, то создаётся запрос на игнорирование ненужных моделей в пользу загрузки нужной
модели", тогда уйдут проблемы с override и сортировкой, но появляется проблема с двумя
независимыми модами, которые у одной модельки заменяют несколько деталей, например,
тогда надо придумать систему наследования модов, то есть есть мод, есть на странице
мода ссылки на форки мода, которые несовместимы по определению с корневым модом, я
вообще не против универсальности: можно и сделать умную склейку конфликтующих файлов,
можно в hot-load менять модельки, можно вручную написать hotfix, который дружит
конфликты прямиком в игре»

**Решение:** Гибридная custom architecture: load-time rejection + smart ignore +
inheritance + smart merge + hot-load + manual hotfix. Подробнее Round 3 F3.

### E5. Operator's playability cadence

**Agent:** Phase 4 DoD = «30+ min session». Когда звать оператора на playtest?

**Operator:** «надо придумать, да, а то мне совсем не хочется вручную всегда
проверять, нужен чёткий пайплайн проверок, нужны обширные возможности для автономной
проверки чуть ли не всего, надо подумать над этим»

**Решение:** Validation pipeline (4 layers) proposed в Round 3 F4.

### E6. Critical Phase 0.5 deliverables

**Agent:** Список из 7 deliverables.

**Operator:** «всё, и style guide с branching strategy»

**Решение:** Phase 0.5 deliverables состав зафиксирован + style guide + branching
strategy добавлены.

---

## Round 3 — concretization

### F1. MP deprioritization — пересобираем приоритеты Z3

**Agent:** Раз MP не в приоритете, Z3 priorities: P1 combat MVP, P2 basic AI, P3 1
vehicle, P4 deferred lockstep, P5 deferred server-auth.

**Operator:** «y»

**Решение:** Z3 переориентирован на combat+AI. MP полностью deferred.

### F2. Shared API — что это

**Agent:** Объяснил: стабильные C++ headers / module interfaces, через которые
Z-агенты обращаются к подсистемам других зон. Z1 owns voxel/world/asset, Z3 owns
entity/combat/AI, Z2 uses через LuaJIT bindings.

**Operator:** «y»

**Решение:** Shared API concept принят → `agent/knowledge.md §40`.

### F3. Mod system architecture

**Agent:** Структурировал operator's vision (E4) в architecture: manifest format,
resource addressing, load pipeline, merge semantics per type.

**Operator:** «good»

**Решение:** Architecture зафиксирована → `agent/knowledge.md §41` + ROADMAP §9.

### F4. Autonomous validation pipeline

**Agent:** 4 layers (per-commit, per-proposal, per-milestone, per-gate). Smoke test
с screenshot perceptual hash diff.

**Operator:** «в layer1 в smoke test скриншот не по hash должен сверяться, а по самому
скриншоту: модели умеют видеть изображения, ты – нет, в layer 3 30 минут слишком много,
вообще на стресс не проверяй, 1 hour idle это уже маразм какой-то, остальное норм,
принимай»

**Решение (revised в Round 4 G2):** Layer 1 без vision diff, layer 3 без stress,
operator явно отказался от AI vision diff tool позже.

### F5. Branching strategy

**Agent:** Hybrid (zone branches + feature branches + root-agent merge).

**Operator:** «пойдёт, hybrid»

**Решение:** `agent/knowledge.md §43`.

### F6. Style guide scope

**Agent:** Что включить (C++ style, GLSL, naming, mod manifest, test naming, etc).

**Operator:** «принимаю, но желательно всю фазу 0.5 чтобы ты писал, потому что ты самый
умный агент, а в остальном будут поглупее агенты работать. Вообще, возможно ли это
реализовать мне с 5 opencode сессиями открытыми с агентами? Мне в любом случае придётся
вручную проверять их каждые 5 минут, смотреть, что они не застряли и всё правильно
делают.»

**Решение:** Style guide scope принят. Operator хочет root-agent (smartest) для всей
Phase 0.5. Поднят critical feasibility question — отвечен в Round 4 G1.

---

## Round 4 — feasibility & execution

### G1. Feasibility 5 параллельных opencode-сессий (critical)

**Agent:** Честный ответ: возможно, но не сейчас, и не 5. Проблемы: operator attention
bottleneck, dumber agents risk, coordination overhead. Предложил phased rollout:
1 → 2 → 3 → опц. 5 сессий по мере готовности validation pipeline.

**Operator:** **a** (phased rollout)

**Решение:** `ROADMAP v2.0 §2.1`.

### G2. Revised F4 Validation Pipeline

**Agent:** Updated per operator feedback (drop vision diff — operator изменил мнение,
drop stress, drop 1h idle).

**Operator:** «принимается, кроме ai vision diff tool»

**Решение:** Final validation pipeline = `agent/knowledge.md §42` + ROADMAP §8. Layer
1 smoke test = crash/log check только (no vision diff, no hash).

### G3. Phase 0.5 execution plan

**Agent:** Предложил порядок (12 deliverables), ROADMAP rewrite в конце.

**Operator:** «не ок, надо roadmap обновить в самом начале, ты уже забыл мой первый
запрос? G3.b — делаем всё, что там сложного? Просто прочитал backlog+index и делаешь
mv папок, зная, что куда; G3.c — на самом деле это сейчас вообще не нужно, надо
отложить это надолго»

**Решение:**

- ROADMAP v2.0 rewrite = FIRST in Phase 0.5 (не в конце)
- Refactor experiments = full migration (не lazy)
- Mod system core = **DEFERRED** (не в Phase 0.5)

### G4. Phase 0 exit criteria

**Agent:** Как именно operator даёт visual approval (live session / screenshots /
video / all).

**Operator:** «не твоё дело, Phase 0 – не моё дело, я повторяю.»

**Решение:** Drop. Phase 0 = operator's domain полностью. D3 = «я есмь критерий»
достаточно.

---

## Round 5 — strategic depth

### H1. Persistence research — первые шаги Z4

**Agent:** Z4 first task: survey 5 backends, workload model, metrics, output.

**Operator:** «y»

**Решение:** Z4 task #1 scoped.

### H2. Content vertical slices — глубина (Phase 6)

**Agent:** 4 slices MVP specs (modern war / fantasy / god-mode / survival).

**Operator:** «MVP не соответствует моим ожиданиям: Modern War – 1 роль пусть будет
пока, просто soldier, остальное норм; Fantasy – пусть пока не будет в планах, как и
god-mode: это слишком тяжело и не нужно сейчас; последовательно, начиная с survival»

**Решение:**

- Modern War: 1 role (soldier)
- **Fantasy slice: OUT из планов**
- **God-mode slice: OUT из планов**
- Последовательно, survival first
- `ROADMAP v2.0 §1.2 I.13` + §6 + §7

### H3. AI stack realism

**Agent:** Symbolic hot path + LLM cold path, realism assessment per layer.

**Operator:** «я изначально хотел, чтобы быстрые решения (тактика на поле брани,
спасение раненых, выбор действия здесь и сейчас) принимал символьный ии, а долгие –
LLM; как mvp пойдёт; event log + log каждой отдельной фракции со всеми выборами, но
знай, что весь H3 – это далёкое будущее, сейчас это не нужно реализовывать»

**Решение:** Symbolic + LLM hybrid architecture. Far future. `ROADMAP v2.0 §10`.

### H4. Public release strategy

**Agent:** 4 options (private до playable / open-source раньше / позже / никогда).

**Operator:** **a**

**Решение:** Private до completionist state. `ROADMAP v2.0 §16`.

### H5. Z2 critical path to playable

**Agent:** 8 steps Z2 path. Critical path к playable.

**Operator:** «считай, что playable – это вершина, до которой не дотянешься, пока не
сделаешь абсолютно всё.»

**Решение:** **Playable = асимптота, не gate.** Концепция «playable gate» упразднена.
`ROADMAP v2.0 §1.2 I.14`.

---

## Round 6 — final clarifications

### I1. Старый ROADMAP v1.0 — судьба?

**Agent:** (a) архивировать, (b) сохранить как есть, (c) inline merge.

**Operator:** **c**

**Решение:** v2.0 замещает v1.0 in-place в `/ROADMAP.md`.

### I2. TODO.md — судьба?

**Agent:** (a) оставить, (b) refactor, (c) поглотить.

**Operator:** **a**

**Решение:** TODO.md не трогать.

### I3. agent/knowledge.md — сколько новых contracts?

**Agent:** 6 новых contracts предложил.

**Operator:** **a** (все 6 как новые §-номера)

**Решение:** §38-43 добавлены в `agent/knowledge.md`.

### I4. Survival slice — dependencies

**Agent:** Список dependencies (voxel editing, day/night, weather, resources,
crafting, progression, AI, etc).

**Operator:** «y»

**Решение:** Dependencies зафиксированы в `ROADMAP v2.0 §6`.

### I5. Финальная готовность

**Agent:** После ответов на I1-I4 всё готово для rewrite.

**Operator:** «go.»

**Решение:** Build mode активирован. Этот файл + ROADMAP v2.0 + knowledge.md updates
созданы.

---

## Cross-refs

- `ROADMAP.md` v2.0 §17 — Q&A Log summary table
- `ROADMAP.md` v2.0 §19 — History (v1.0 → v2.0 transition)
- `agent/knowledge.md` §38-43 — engineering contracts из Q&A
- `agent/workspace.md` — per-session snapshot (updated после build pass)
- `AGENTS.md §1` — rule changes (этот файл = operator-approved)

## Maintenance

| Date       | Change                             | Author     |
|------------|------------------------------------|------------|
| 2026-06-25 | Initial creation (6-round Q&A log) | root-agent |
| TBD        | Quarterly review updates           | TBD        |
