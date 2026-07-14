# ProjectV Roadmap v2.0 — стратегический план развития

> **Назначение:** долгоживущий стратегический документ. Отвечает на вопрос
> «куда мы идём и зачем». Tactical tasks (конкретные sub-tasks, milestones) живут в
> `TODO.md`. Engineering contracts (что не меняется) живут в `agent/knowledge.md`.
> Per-session narrative живёт в `agent/workspace.md`. Этот документ = **обёртка** над
> ними.
>
> **Статус:** APPROVED v2.0 (2026-06-25, после 6 раундов Q&A с оператором).
> v1.0 (1913 строк, sequential phase model) — superseded. История перехода → §19.
> Любые изменения этого документа — только по явной команде оператора (per `AGENTS.md §1`).
>
> **Связь с sources of truth (per `AGENTS.md §3` + `agent/knowledge.md §0`):**
> ```
> 1. Код (.cpp/.hpp/.ixx + шейдеры + тесты)   ← абсолютный приоритет (реальность)
> 2. AGENTS.md                                 ← протокол
> 3. agent/knowledge.md                        ← engineering contracts (что не меняется)
> 4. agent/workspace.md                        ← snapshot текущего состояния
> 5. ROADMAP.md (этот файл)                    ← стратегическое направление
> 6. docs/VulkanSDK-Linux-Docs-1.4.350.1/      ← vendor docs
> 7. TODO.md                                   ← tactical roadmap (sub-tasks)
> ```
>
> **Cross-refs на origin документы:**
> - `AGENTS.md §2` — Project metadata (vision, stack, capabilities)
> - `AGENTS.md §5` — operator policy, multi-agent coordination, protocol
> - `TODO.md §0-1` — Status-quo, RTX-only pivot 2026-06-22, RTX-driven milestones
> - `agent/knowledge.md §2` — Hardware target policy
> - `agent/knowledge.md §37-43` — post-v2.0 engineering contracts (LuaJIT, memory,
    > shared API, mods, validation, branching)
> - `docs/experiments/INDEX.md` — 170+ closed experiments registry
> - `docs/experiments/research/backlog.md` — hypothesis канбан
> - `legacy/docs/philosophy/README.md` — design principles
> - `docs/QA_LOG_roadmap_v2.md` — полный журнал 6 раундов Q&A (источник решений v2.0)

---

## §1. Vision & Invariants

### §1.1 Vision (унаследована из v1.0, формулировка актуальна)

**ProjectV — мета-игра и творческая мастерская**, не mass-market и не попсовый продукт
для извлечения прибыли. Это **pet-project с душой**, бесконечный долгострой,
строящийся стахановскими темпами благодаря современному пайплайну программирования
и AI-агентам как force multiplier.

Игрок получает:

- **Minecraft-class voxel core** — бесшовная работа с блоками (build/break/edit),
  полигональными моделями (glTF assets), аналитическими примитивами (сферы/цилиндры/
  конические секции через SDF+CSG). Hybrid world representation без видимых швов.
- **Survival + combat** — ходить, бегать, стрелять, убивать, ездить, копать, строить,
  транспортировать.
- **Иерархическое управление** — squad → platoon → battalion → theater → state. Один
  игрок может командовать на любом уровне абстракции (далёкое будущее).
- **Управление государством** — политика, армия, наука, внутренняя политика.
  HoI4-style grand strategy слой поверх tactical combat (далёкое будущее).
- **God-mode наблюдение** — AI сам играет: ведёт войны, придумывает тактические
  манёвры, формирует стратегические планы, исследует ведение войны, формирует
  государства, генерирует лор целого мира. Игрок наблюдает как бог (далёкое будущее).
- **Любые сеттинги** — modern war, high-fantasy, dark-fantasy, isekai, sci-fi,
  постапокалипсис, средневековье, и т.д. Data-driven content axis позволяет
  hot-swap сеттингов без перекомпиляции.
- **Multiplayer full-spectrum** — от small coop (2-8 игроков, host-based) до massive
  persistent war (100-1000+ игроков, single-shard Foxhole-style). **Deferred
  post-почти-всего per I.12.**

**Что НЕ является целью:**

- Не Minecraft-клон (есть свои приоритеты — RTX visuals, AI, persistence, multiplayer).
- Не чистый военный симулятор (военная тематика — одно из направлений, не единственное).
- Не Roblox (нет low-quality mass-market, нет asset-store-monetization).
- Не tech demo (цель — играбельный билд, а не показ возможностей).
- Не MVP-oriented product (per I.14: playable = асимптота, не near-term gate).

### §1.2 Invariants (что не меняется)

Утверждены оператором. Любое изменение требует нового explicit Q&A.

**I.1 Hardware target (v1.0 Q3, подтверждён).** NVIDIA RTX 20/30/40/50 series.
Non-RTX = hard-fail с понятным error message. Никаких fallback paths, никаких уступок
legacy hardware. Pet-project = pet-project.

**I.2 Pet-project scope (v1.0 Q3, подтверждён).** Нет non-RTX fallback, нет поддержки
старых GPU/CPUs «ради вежливости». Готовность к серьёзным structural changes ради
долгосрочной жизнеспособности.

**I.3 Технологическая свобода (v1.0 Q3, подтверждён).** C++26 + Vulkan 1.4 — текущий
стек, но **можно другие языки/API/библиотеки, если оправдывает эффективность,
производительность, современность**. Rust для specific sub-systems? Mojo для compute
kernels? Slang вместо GLSL? — открыто для рассмотрения в любой фазе. Не догма, а
стартовая точка.

**I.4 DOD/SoA как default (v1.0 Q3, подтверждён).** Не догма, а стартовая точка.
Готов к пересмотру в пользу альтернативных парадигм, если оправдано.

**I.5 Visual Quality Bar (v1.0 Q17, подтверждён).** 1080p × 120 FPS на RTX 3060 Ti как
минимальный таргет. VoxelLab должен выглядеть как AAA-игра, не как tech demo. Это
**baseline quality**, не maxed settings.

**I.6 Visual Identity Gate (v1.0 Q16, подтверждён).** Content axis (data-driven
definitions, scenarios, mods) жёстко заблокирован до достижения visual quality gate.
Пока графика «игрушечная» — content work не начинается. **Gate criterion = оператор's
subjective approval** (per Round 1 D3: «я есмь критерий: мне понравится – завершаем,
не понравится – продолжаем работать над графикой»). Метрик нет и не нужно.

**I.7 Multiplayer = first-class, но deferred (v1.0 Q2/Q5 → v2.0 Round 3 F1).**
Single-player и multiplayer равноправные first-class citizens, **но MP сейчас не в
приоритете**. Сначала single-player sandbox + survival + combat + AI, потом MP.
Архитектурное разделение `ProjectVClient` + `ProjectVServer` — обязательно когда MP
возвращается в scope (per Q14).

**I.8 AI-агенты как force multiplier (v1.0 Q15, подтверждён + Round 4 G1).**
Multi-agent parallel work — конкурентное преимущество. Но **phased rollout**:
1 сессия (Phase 0.5, root-agent) → 2 зоны (post-Phase-0) → 3 зоны (post-survival-
-slice) → опционально 4-5 если cadence доказан. «Стахановские темпы через AI-агентов».

**I.9 Release = gated by completionist state (v1.0 Q6 → v2.0 Round 5 H4+H5).**
Никакого публичного релиза пока проект не станет «абсолютно всё сделано». **Playable
= асимптота, не near-term gate.** После достижения: open-source (A) + public demo
build (B) + Steam Early Access (C) — последовательно.

**I.10 Quarterly Review (v1.0 Q19, подтверждён).** Open-ended pacing без жёстких дат.
Каждый квартал — review roadmap по факту: что заняло больше/меньше времени, что
заблокировано, какие новые hypothesis'ы появились, нужны ли корректировки zone
boundaries.

**I.11 LuaJIT mode-switch determinism contract (Round 2 C2, new in v2.0).**
LuaJIT как единственный scripting runtime, режим выбирается per-path:

- Single-player / sandbox → JIT on (max performance)
- Small coop (2-8, lockstep) → `-joff` interpreter (deterministic, всё ещё быстро)
- Massive persistent war (server-auth) → JIT on сервера, клиенты не крутят sim
- Esoteric 100+ lockstep research → `-joff` + cross-platform testing (Z4 only)
  Per `agent/knowledge.md §38`.

**I.12 Multiplayer deferred (Round 3 F1, new in v2.0).** MP сейчас совсем не в
приоритете. Сначала single-player sandbox + survival slice + combat + AI. MP
возвращается в scope пост-устойчивого-single-player. Z3 priority: combat → AI →
(далеко) MP.

**I.13 Slice scope reduction (Round 5 H2, new in v2.0).** Vertical slices в scope:
**survival (first)** → **modern war упрощённый (1 role = soldier, later)**. Fantasy
slice и god-mode slice **выведены из планов** на неопределённо долгое время («слишком
тяжело и не нужно сейчас»).

**I.14 Playable = asymptote, не gate (Round 5 H5, new in v2.0).** Концепция «playable
gate» (v1.0 §3.4 Phase 4) **упразднена**. Playable = асимптота, до которой не
дотянуться, пока не сделаешь абсолютно всё. Roadmap = continuous development с
per-zone gates, без global release-oriented phase numbering.

### §1.3 Anti-Goals (явно НЕ делаем)

- **Не поддерживаем non-RTX hardware.** Не тратим время на CSM fallback.
- **Не делаем web/mobile port.** Mobile = non-RTX по определению.
- **Не делаем asset-store-monetization.** Это не Roblox. Моддинг — да, через
  открытые JSON + workshop later. Магазин скинов по $5 — нет.
- **Не превращаем в tutorial-heavy onboarding.** Документация + community wiki +
  scenario tutorials. Не in-game handholding.
- **Не делаем F2P / battle pass / live service.** Один раз купил — играешь.
- **Не делаем Fantasy slice пока survival+modern war не готовы** (per I.13).
- **Не делаем God-mode slice пока survival+modern war не готовы** (per I.13).
- **Не делаем Massive persistent war (1000+)** на operator hardware — требует
  datacenter rental или sharding (per §11 + §13 Risk Register).
- **Не делаем playable MVP** — playable = асимптота, не milestone (per I.14).

---

## §2. Multi-Agent Model

### §2.1 Phased rollout (Round 4 G1 = a)

Текущая модель (v1.0): 5 агентов с самого старта. **Проблема:** operator attention
bottleneck («проверять каждые 5 минут» = выгорание), coordination overhead, dumber
agents risk. Решение v2.0: **поэтапный rollout**.

| Phase                               | Модель                                                                                  | Когда                                                 |
|-------------------------------------|-----------------------------------------------------------------------------------------|-------------------------------------------------------|
| **Phase 0.5** (infra)               | **1 сессия: root-agent + operator**                                                     | сейчас, до Phase 0 visual exit                        |
| **Phase 1** (post-Phase-0 visual)   | **2 параллельных сессии:** root (с Z1+Z4 work) + Z2-agent                               | post visual approval                                  |
| **Phase 2-3** (post-survival-slice) | **3 параллельных:** root+Z1, Z2, Z3                                                     | когда shared API stabilised + survival slice в работе |
| **Phase 4+** (опционально)          | **4-5 сессий** если cadence доказан и validation pipeline снимает «5-минутные проверки» | далёкое будущее                                       |

**Ключевое условие для следующего этапа:** validation pipeline (§8) работает на
текущем этапе достаточно хорошо, что operator не проверяет каждые 5 мин. Если
проверяет — расширение количества сессий откладывается.

**Composition (когда все 4 зоны активны):**

| #      | Агент                            | Роль                        | Scope                                                                               | File root                                          |
|--------|----------------------------------|-----------------------------|-------------------------------------------------------------------------------------|----------------------------------------------------|
| **R**  | **Root-agent**                   | Integrator                  | merge, conflict resolution, sync с оператором                                       | `/` (корень)                                       |
| **Z1** | **Zone 1**                       | Core engine specialist      | voxel storage, meshing, physics, RTX rendering, AA, persistence                     | `/core-engine/` (or `/src/core-engine/`)           |
| **Z2** | **Zone 2**                       | Sandbox/creative specialist | programmable voxels, scripting, data-driven content, SDF/CSG, asset pipeline, mods  | `/sandbox-creative/` (or `/src/sandbox-creative/`) |
| **Z3** | **Zone 3**                       | Gameplay specialist         | combat, vehicles, AI stack, scenarios, multiplayer (deferred)                       | `/gameplay-systems/` (or `/src/gameplay-systems/`) |
| **Z4** | **`docs/experiments/` (Zone 4)** | Frontier research           | новые техники (NeRF-GS, neural rendering, ML), horizon scan, experimental mechanics | `/docs/experiments/`                               |

**Распределение работы:**

- 4 Z-агента работают одновременно над своими направлениями (когда все активны).
- Root-agent координирует, интегрирует, разрешает конфликты, синхронизирует с оператором.
- Оператор работает напрямую с root-agent.
- Z-агенты работают в своих зонах, никогда не трогают чужие файлы (per §2.3).

### §2.2 Partitioning proposal (Variant α, утверждён v1.0 Q20)

**Z1 `core-engine/`** — Shared low-level движок:

- Voxel storage: `Sparse64Tree`, `NanoVdbFlattenResult`, GPU mirror
- Meshing: GPU greedy mesher, mesh shaders pattern C
- Physics: Jolt integration, GreedyMerger, Incremental sync
- World gen: GPU noise, LOD downsampling, draw distance
- RTX-driven rendering: shadows (BLAS+TLAS+voxel-aware intersection), AO (ray query),
  GI (DDGI probes), refraction (ray query), multi-bounce GI
- TAA / AA pipeline: SPIR-V variants, YCoCg, CAS, motion vectors, DLAA/Streamline
- Post-FX: tonemap (Reinhard→ACES), bloom, aerial perspective
- Lighting: BRDF, material model, scene presets, VoxelSceneLighting SSBO
- Fluid CA: GPU ping-pong, ECS tick routing
- Async compute + timeline semaphores
- Persistence layer (post §11 Z4 research)
- Cross-cutting: SSBO byte-exact invariant, frame pipeline ordering

**Z2 `sandbox-creative/`** — Player agency + content pipeline:

- Programmable voxels: LuaJIT integration (per `agent/knowledge.md §38` mode-switch)
- Data-driven definitions: JSON для materials, factions, weapons, vehicles, scenarios
- SDF/CSG layer (research-closed `sdf-subtractive-modeling-ui` → mainline)
- Hybrid world: voxel + glTF models + analytic primitives integration
- Asset pipeline: glTF + Draco + meshoptimizer (уже есть, доработка)
- Mods / workshop: manifest format, content mount/unmount, dependencies (per §9,
  implementation DEFERRED)
- Scenario editor: JSON mission format, trigger system, scripting bindings
- Save/load: snapshot round-trip, progression persistence
- Custom faction/terrain/vehicle editors

**Z3 `gameplay-systems/`** — Combat + AI + (далеко) multiplayer:

- Combat: weapons (ballistic, missile, melee), damage model, HP, suppression
- Vehicles: aircraft (fixed-wing + VTOL), helicopters, tanks, naval, amphibious
- AI stack (per §10): symbolic hot path (BT + GOAP) + LLM cold path (lore, strategic)
- God-mode observer (DEFERRED per I.13)
- Multiplayer (DEFERRED per I.12): lockstep foundation (2-8), AOI, dedicated server
- Networking architecture: `ProjectVClient` + `ProjectVServer`, headless mode
- Persistent war (DEFERRED): sector control, supply, production, logistics, campaigns

**Z4 `docs/experiments/`** — Frontier research:

- Активные frontier hypothesis'ы для следующих фаз
- Horizon scan: NeRF/3DGS hybrid, ML rendering, neural voxels
- Экспериментальные mechanics (closed research → mainline candidates)
- Persistence backend research (per §11, task #1)
- LuaJIT-at-100+ lockstep esoteric (per §12.3)
- Web-research, прототипы, мини-бенчмарки

### §2.3 Scope discipline и Shared API

**Каждый Z-агент имеет право писать только в свою папку:**

- Z1 пишет в `/core-engine/` и `/src/core-engine/` (+ shared API headers в `/src/shared/`
  с root-agent approval per `agent/knowledge.md §40`)
- Z2 пишет в `/sandbox-creative/` и `/src/sandbox-creative/`
- Z3 пишет в `/gameplay-systems/` и `/src/gameplay-systems/`
- Z4 пишет в `/docs/experiments/` (только в своей папке)

**Cross-zone writes запрещены.** Если Z-агенту нужно использовать API из другой зоны —
он читает `src/shared/<api>.hpp` (read-only для него), пишет proposal в
`/proposals/<feature>.md` если нужно изменение, root-agent review'ит.

**Shared headers** (`src/shared/*.hpp` per `agent/knowledge.md §40`) — read-only для
всех Z-агентов, кроме владельца зоны. Любое изменение = root-agent approval + все
Z-агенты notified.

**Conflict prevention через byte-exact `static_assert`** (per `agent/knowledge.md §16`):
если два Z-агента независимо добавят поле в shared SSBO — compile-time error на
integration. Это хорошо: ловит конфликты до runtime.

Per `AGENTS.md §5.5` Multi-agent coordination — merge conflicts разрешаются только
ручным merge'ем operator + root-agent.

### §2.4 Merge Strategy (Round 3 F5 = hybrid branching)

Per `agent/knowledge.md §43`. Hybrid: long-lived zone branches + short-lived feature
branches inside zones + root-agent periodic merge to main.

```
main (root-agent integrator, всегда green)
├── z1/core-engine (long-lived, Z1 владелец)
│   ├── feature/persistence-layer (short-lived per task)
│   └── ...
├── z2/sandbox-creative (long-lived, Z2 владелец)
├── z3/gameplay-systems (long-lived, Z3 владелец)
└── z4/experiments (long-lived, Z4 владелец, = docs/experiments refactor)
```

**Workflow:**

```
Z-agent (в своей папке)              Root-agent                    Operator
       │                                  │                            │
       ├── работает                        │                            │
       ├── пишет proposal ────────────────►│                            │
       │   (proposals/<feature>.md)        │                            │
       ├── пишет код в свою зону           │                            │
       ├── пишет тесты в свою зону         │                            │
       │                                  │                            │
       │                                  ├── review proposals          │
       │                                  ├── прогон ctest (все зоны)  │
       │                                  ├── L2 validation             │
       │                                  ├── resolve conflicts         │
       │                                  ├── merge в mainline          │
       │                                  ├───────────────────────────►│
       │                                  │   status update             │
       │                                  │◄───────────────────────────┤
       │                                  │   operator feedback         │
       │                                  ├── commit + sync             │
       │◄─────────────────────────────────┤                            │
       │   merge complete notification     │                            │
```

**Sync-точки:** root-agent прогоняет `ctest` всех зон + L2 validation на каждой
sync-точке. Failure → freeze merge до resolve. Success → commit + notify всех
Z-агентов.

### §2.5 Refactor 170+ experiments (Round 1 A3 + Q22 = B, parallel с Phase 0)

170+ closed экспериментов в `docs/experiments/` мигрируются по тематике в зоны:

| Текущая локация                                         | Новая локация                                    | Объём      |
|---------------------------------------------------------|--------------------------------------------------|------------|
| Tier 0 (foundation, optimization)                       | `core-engine/research/` (Z1)                     | ~30 файлов |
| Tier 1 (AI, weapons, vehicles, sensors)                 | `gameplay-systems/research/ai-combat/` (Z3)      | ~50 файлов |
| Tier 2 (AI tactical, AI strategy, perception)           | `gameplay-systems/research/ai-tactical/` (Z3)    | ~40 файлов |
| Tier 3 (economy, sandbox, content, scenarios)           | `sandbox-creative/research/` (Z2)                | ~30 файлов |
| Tier 4 (UI, audio, polish)                              | `gameplay-systems/research/ui-audio/` (Z3)       | ~20 файлов |
| Horizon scan (NeRF-GS, neuromorphic, ML rendering)      | `docs/experiments/` (Z4, frontier)               | ~10 файлов |
| Meta / process experiments (anti-cheat, lockstep, etc.) | `docs/experiments/` (Z4, integration candidates) | ~20 файлов |

**Timing:** parallel с Phase 0 visual work (root-agent в фоне). Не блокирует visual
approval. **Mechanics:** просто `mv` папок + update cross-refs в INDEX/backlog. Per
operator Round 4 G3.b: «просто прочитал backlog+index и делаешь mv папок, зная, что
куда».

**DoD:**

- Все 170+ файлов либо мигрированы, либо архивированы
- `INDEX.md` обновлён с новой структурой
- `backlog.md` / `backlog_closed.md` синхронизированы
- Все cross-refs обновлены на новые пути
- 39/39 tests passing (никаких regression)
- Build green

---

## §3. Phase 0.5 — Infrastructure (до старта parallel zones)

**Цель:** подготовить инфраструктуру для multi-agent parallel work.

**Статус:** в плане. Стартует когда operator скажет «выходи из plan mode» (после
окончания Q&A сессии, в которой создан этот v2.0).

**Owner:** solo root-agent + operator (per G1 Phase 0.5 model).

### §3.1 Deliverables (Round 4 G3 corrected)

| #      | Deliverable                                                                                       | Est.                  |
|--------|---------------------------------------------------------------------------------------------------|-----------------------|
| **1**  | **`ROADMAP.md` v2.0** (этот файл, уже пишется)                                                    | 1 сессия              |
| **2**  | `agent/knowledge.md` updates (§38-43, source-of-truth ranking)                                    | сделано в этой сессии |
| **3**  | `docs/MEMORY_BUDGET.md` (детализация `agent/knowledge.md §39`)                                    | 1 час                 |
| **4**  | `docs/STYLE_GUIDE.md` (multi-zone conventions, LuaJIT, resource addressing)                       | 2-3 часа              |
| **5**  | Shared API stubs в `src/shared/` (per `agent/knowledge.md §40`)                                   | 3-4 часа              |
| **6**  | CI/CD: `.githooks/pre-commit`, `tools/ci/run-l1.sh` (smoke = crash/log check, **no vision diff**) | 1-2 сессии            |
| **7**  | Per-zone test targets в CMake (`ctest -L z1/z2/z3/z4`)                                            | 1 сессия              |
| **8**  | Branching setup: zone branches + `/proposals/README.md` + multi-agent coord files                 | 2 часа                |
| **9**  | Refactor 170+ experiments (`mv` per §2.5 + cross-refs)                                            | 1-2 сессии            |
| **10** | Persistence research kickoff (Z4 task #1, prototype)                                              | 1-2 сессии            |
| **11** | `docs/QA_LOG_roadmap_v2.md` (история решений Q&A)                                                 | 1 час                 |

**Опционально (если operator подтверждает):**

- Mod manifest format reference implementation (DEFERRED per G3.c, **не делать в 0.5**)

### §3.2 Order of execution

1. ROADMAP v2.0 (этот файл) + knowledge.md updates — **сейчас**
2. QA_LOG_roadmap_v2.md — **сейчас (источник решений)**
3. Style guide + memory budget docs — быстро, documentation
4. Shared API stubs — нужно до старта Z2 (для LuaJIT bindings)
5. CI/CD + per-zone test targets — нужно до старта любого Z-агента
6. Branching setup + proposals infrastructure — нужно до старта любого Z-агента
7. Refactor experiments — параллельно с Phase 0 visual (root-agent в фоне)
8. Persistence research kickoff — последний (Z4 task)

### §3.3 DoD

- ROADMAP.md v2.0 = этот файл, approved
- `agent/knowledge.md` дополнен §38-43 + source-of-truth обновлён
- Все Phase 0.5 deliverables созданы
- 39/39 tests passing (zero regressions)
- Build green, validation clean (DDGI descriptor warnings = pre-existing, OK)
- Operator готов к Phase 0 visual approval (когда сам решит — per I.6 + G4)

---

## §4. Zone Charters

### §4.1 Z1 core-engine

**Scope:** shared low-level engine. Подробнее per §2.2 Z1.

**Owner (when activated):** root-agent в Phase 1 (post-Phase-0 visual), затем
отдельный Z1-agent в Phase 2+.

**Initial roadmap (Phase 1, post-Phase-0 visual):**

1. Engine stability для continuous sessions (1+ hour без crash)
2. Frame budget profiling (Tracy), target 1080p × 120 FPS на VoxelLab
3. Voxel streaming для large worlds (вне draw distance)
4. Persistence layer integration (post §11 Z4 research)
5. Determinism contract для future lockstep (когда MP в scope)
6. Asset streaming optimization

**Cross-zone dependencies:**

- Z2/Z3/Z4 используют shared API Z1 (per `agent/knowledge.md §40`)
- Z4 research outputs → Z1 integration (persistence, new techniques)

### §4.2 Z2 sandbox-creative

**Scope:** player agency + content pipeline. Подробнее per §2.2 Z2.

**Owner:** Z2-agent в Phase 1 (первый отдельный zone-agent после root).

**Critical path (post-Phase-0.5, к survival slice):**

1. JSON schemas (materials, factions, weapons, vehicles, scenarios)
2. LuaJIT integration с mode-switch contract (`agent/knowledge.md §38`)
3. SDF/CSG MVP (3 primitives, boolean ops)
4. Data-driven loader (cold-reload)
5. Sandbox voxel tools (place/remove/paint/copy-paste)
6. Save/load для sandbox worlds (player-side persistence)
7. Mod manifest MVP — **DEFERRED** per G3.c (не нужен для survival slice)
8. First scenario: `sandbox_default.json`

**Cross-zone dependencies:**

- Z1 shared API (voxel, world, asset)
- Z3 entity API для scripting bindings
- Z4 research outputs (новые techniques)

### §4.3 Z3 gameplay-systems

**Scope:** combat + AI + (далеко) multiplayer. Подробнее per §2.2 Z3.

**Owner:** Z3-agent в Phase 2-3 (post-survival-slice starts).

**Initial roadmap (Phase 2-3):**

1. Combat MVP (1 weapon class, damage model, HP)
2. Basic AI (hostile wildlife via behavior trees, per `hierarchical-tactical-ai-btree`)
3. 1 vehicle class (танк или простая машина, не aircraft)
4. Player progression (HP/stamina/hunger для survival)
5. **(DEFERRED per I.12)** Lockstep MP для 2-8 coop
6. **(DEFERRED per I.12)** Server-authoritative MP, headless server

**Cross-zone dependencies:**

- Z1 shared API (voxel для projectile collision, world для spawn)
- Z2 data-driven definitions (weapons, vehicles, factions)
- Z4 AI research (LLM integration для cold path, далёкое будущее)

### §4.4 Z4 frontier research

**Scope:** frontier research. Подробнее per §2.2 Z4.

**Owner:** root-agent в Phase 1 (до Phase 2-3), затем Z4-agent.

**Initial research tasks:**

1. **Persistence backend research** (per §11, task #1): benchmark Postgres / SQLite /
   LMDB / Dragonfly / custom binary на synthetic voxel workload. Output: integration
   recommendation для Z1.
2. **LuaJIT-at-100+ lockstep esoteric** (per §12.3): research-only, не main path.
3. **NeRF/3DGS prototype** (closed `nerf-gs-in-realtime-voxel` → integration
   candidate): C_HybridStatic_Plus_VoxelDynamic как Stage 5.x opt-in.
4. **Audio techniques integration candidates** (closed experiments → Z1/Z3).
5. **Horizon scan**: новые rendering/AI/netcode techniques.

**Cross-zone dependencies:**

- Читает что угодно (read-only)
- Пишет только в `docs/experiments/`
- Output = research reports + integration recommendations в `README.md` экспериментов

---

## §5. Per-Zone Roadmaps (critical paths)

**Принцип (per I.14):** нет глобальных phase transitions. Каждая зона имеет свой
roadmap, свои gates, свой темп. Sync через root-agent integrator.

### §5.1 Z1 critical path

```
Phase 0.5 done
  ↓
[Engine stability для continuous sessions]
  ↓
[Frame budget: 1080p × 120 FPS на VoxelLab]
  ↓
[Voxel streaming для large worlds]
  ↓ (parallel с Z2 LuaJIT work)
[Persistence layer integration (post §11 Z4 research)]
  ↓
[Asset streaming optimization]
  ↓
[Determinism contract для future lockstep]
  ↓
[MP-ready engine] (когда MP возвращается в scope per I.12)
```

### §5.2 Z2 critical path к survival slice

**Это главный путь к первой играбельной вертикали (survival slice, per I.13).**

```
Phase 0.5 done
  ↓
[JSON schemas: materials + scenarios first, остальные потом]
  ↓
[LuaJIT integration с mode-switch (JIT for single-player)]
  ↓
[SDF/CSG MVP (3 primitives: sphere, box, cylinder; boolean ops)]
  ↓
[Data-driven loader (cold-reload через runtime/content/)]
  ↓
[Sandbox voxel tools (place/remove/paint/copy-paste)]
  ↓
[Save/load для sandbox worlds]
  ↓
[First scenario: sandbox_default.json]
  ↓
[Z3 combat + AI integration]
  ↓
[Day/night + weather integration (closed experiments)]
  ↓
[Resource gathering + crafting (closed experiments + new work)]
  ↓
[Hostile wildlife AI (Z3)]
  ↓
[Player progression: HP/stamina/hunger (Z3)]
  ↓
[Inventory UI]
  ↓
🎯 [SURVIVAL SLICE WORKS END-TO-END]
```

**Note:** Mod system (per §9) — DEFERRED, не нужен для survival slice.

### §5.3 Z3 critical path

```
Phase 0.5 done
  ↓ (после Z2 first scenario sandbox_default.json)
[Combat MVP: 1 weapon class (ballistic), damage model, HP]
  ↓
[Basic AI: hostile wildlife via behavior trees]
  ↓
[1 vehicle class: танк или простая машина]
  ↓
[Player progression для survival: HP/stamina/hunger]
  ↓
[Sensor stack для tactical AI (radar/IRST/acoustic — closed experiments)]
  ↓
[Suppression mechanics (closed experiment D_AccumulatorThreshold)]
  ↓
[IFF (closed experiment B)]
  ↓
[Tactical AI scenarios (urban combat, fire-team coordination)]
  ↓
[Strategic AI (sector control, supply, production — closed experiments)]
  ↓
[Autonomous factions (per §10, far future)]
  ↓ (DEFERRED per I.12)
[Lockstep MP для 2-8 coop]
  ↓
[Server-authoritative MP, headless server]
  ↓
[Massive persistent war]
```

### §5.4 Z4 critical path

```
Phase 0.5 done
  ↓
[Persistence backend research #1 (Postgres/SQLite/LMDB/Dragonfly/custom)]
  ↓ (parallel)
[LuaJIT-at-100+ lockstep esoteric research]
  ↓ (parallel)
[NeRF/3DGS prototype integration candidate]
  ↓ (parallel)
[Audio techniques integration candidates]
  ↓
[Horizon scan continuous]
```

---

## §6. Survival Slice Spec (первый vertical slice, per I.13)

**Цель:** первый end-to-end играбельный scenario в ProjectV. Single-player sandbox +
survival mechanics.

**Зависимости (Round 5 I4 = y):**

| Subsystem                              | Status                                                                     | Source                      |
|----------------------------------------|----------------------------------------------------------------------------|-----------------------------|
| Voxel editing (place/break/paint)      | ✅ есть                                                                     | mainline                    |
| Day/night cycle                        | 🔬 closed experiment `day-night-cycle-celestial-mechanics` (C ⭐ Keplerian) | integration candidate       |
| Weather                                | 🔬 closed `weather-svo-metafield`                                          | integration candidate       |
| Resource gathering (mining/chopping)   | 🔬 closed `resource-harvesting-economy` (C+D)                              | integration candidate       |
| Crafting system                        | 🆕 нет                                                                     | new Z2 work                 |
| Player progression (HP/stamina/hunger) | 🆕 нет                                                                     | new Z3 work                 |
| Hostile wildlife AI                    | 🔬 closed `hierarchical-tactical-ai-btree` (D ⭐)                           | integration candidate       |
| Sleep/spawn cycle wildlife             | 🆕 нет                                                                     | new Z3 work                 |
| Inventory UI                           | 🆕 нет                                                                     | new Z2 work                 |
| Save/load progression                  | ✅ snapshot round-trip есть (knowledge.md §23)                              | extended для survival state |

**MVP definition:**

- 1 biome (forest или plains, procedural)
- Voxel editing работает (build/break/place/paint) для 5+ material types
- Day/night cycle визуально работает + влияет на gameplay (night = опасно)
- Weather: хотя бы 1 тип (rain или snow)
- Resource gathering: 3+ resource types (wood, stone, ore)
- Crafting tree: 10+ recipes (tools, building blocks, basic weapons)
- Player progression: HP, stamina, hunger работает
- Hostile wildlife: 2+ types (animals daytime, aggressive night)
- Inventory UI: place/drop/use items
- Save/load: round-trip работает, progression persist'ится
- **Цель:** 30-60 минутная end-to-end сессия survival loop

**Integration path:**

1. Z2 finishes sandbox_default.json (without survival)
2. Z2 adds crafting + inventory UI
3. Z3 adds combat MVP + wildlife AI + player progression
4. Z2 integrates day/night + weather (closed experiments)
5. Z2 integrates resource gathering (closed experiment)
6. Z1 ensures engine stability + persistence
7. Operator playtests at each milestone (per §8 L4)

### §6.1 Anti-goals для survival slice

- Не делаем farming (далеко, post-MVP)
- Не делаем building shelter requirements (квадратный дом считается)
- Не делаем multiple biomes (1 biomes достаточно)
- Не делаем multiplayer coop (per I.12)
- Не делаем mods (per §9 DEFERRED)

---

## §7. Modern War Slice Spec (post-survival, упрощённый per I.13)

**Цель:** второй vertical slice. Combat-focused scenario. **Реалистично после того,
как survival slice устойчиво работает + combat MVP + AI MVP готовы в Z3.**

**Упрощённый scope (Round 5 H2):**

- **1 role: soldier** (не 3+, как было в v1.0)
- 1 карта 4×4 km (procedural или hand-crafted)
- 2 factions (allied vs axis, abstract)
- 1 vehicle class (танк или APC)
- Capture point mode (как Foxhole)
- 30-min match

**Зависимости:**

- Z3 combat MVP ready
- Z3 vehicle physics (closed `tank-terrain-interaction-physics`)
- Z3 tactical AI (для enemy faction)
- Z2 scenario format ready
- Z1 map streaming для 4×4 km

**MVP definition:**

- Spawn как soldier, ходить, стрелять
- 1 vehicle type можно водить
- Capture points работают (захват/удержание)
- Enemy AI soldiers (tactical AI)
- Match ends after 30 min или capture-all-points
- Restart match работает

**Deferred для modern war slice:**

- Multiplayer (per I.12)
- Persistent war (далёкое будущее)
- Multiple roles (operator crew, pilot — later)
- Multiple vehicle types (later)
- Logistics (later)

---

## §8. Validation Pipeline (Round 4 G2, per `agent/knowledge.md §42`)

| Layer                | Trigger                          | Что проверяет                                                                                                                                                                | Длительность        |
|----------------------|----------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------|
| **L1 per-commit**    | каждый commit в любую зону       | clang-tidy + clang-format --check + build всех preset'ов + ctest всех зон + Vulkan validation layer clean + smoke (1-min run VoxelLab, crash/log check, **без vision diff**) | ~1 мин              |
| **L2 per-proposal**  | каждый `/proposals/<feature>.md` | cross-zone integration tests + Tracy perf regression (frame p95 ≤ baseline × 1.10, VRAM ≤ budget, RAM ≤ budget) + ASan/LSan + save/load round-trip checksum                  | ~5 мин              |
| **L3 per-milestone** | zone milestone claim             | **LookDev captures всех scene presets → report** (без operator review на этом layer). Краткий stability check (5-min run без crash, **no stress test**, no 1-hour idle).     | ~10 мин             |
| **L4 per-gate**      | zone gate claim                  | **Operator playtest / visual review** (operator's subjective call per §1.2 I.6)                                                                                              | operator's decision |

**Infrastructure:**

- **CI runner:** GitHub Actions для public phase (post-release); локальный
  `.githooks/pre-commit` + cron для private phase.
- **Test scene runner:** `tests/integration/` с declarative YAML scenarios.
- **Tracy baseline:** `tools/tracy-baseline/` хранит эталонные frame captures.
- **Crash log collector:** structured crash reports в `runtime/crashes/`.

**Operator-removed items (per Round 4 G2):**

- ❌ 30-min stress test
- ❌ 1-hour idle memory leak check
- ❌ Perceptual hash screenshot comparison
- ❌ AI vision diff tool
- ❌ Determinism test (пока MP не в scope)

**Принцип:** validation pipeline = ответ на «5 минут проверок» оператором. Без него
multi-agent model не работает. С ним — operator нужен только на L4 subjective approval.

---

## §9. Mod System Architecture (DEFERRED implementation, per G3.c)

**Статус:** architecture зафиксирована (operator-approved Round 3 F3 + `agent/knowledge.md
§41`), **implementation отложена на неопределённо долгое время** (per Round 4 G3.c:
«на самом деле это сейчас вообще не нужно, надо отложить это надолго»).

**Принципы (per operator vision):**

1. Load-time rejection несовместимых модов.
2. Developer responsibility за декларацию конфликтов.
3. Smart ignore для декларированных совместимых конфликтов.
4. Inheritance: форки ссылаются на root-мод, по умолчанию несовместимы.
5. Smart merge для частичных замен.
6. Hot-load в sandbox mode.
7. Manual hotfix UI в игре.

**Manifest format:** per `agent/knowledge.md §41`. JSON с metadata, dependencies,
provides, overrides, conflicts, forks, compatibility.

**Resource addressing:** `@<mod_id>:<resource_path>` pattern.

**Load pipeline:** enumerate → dependency DAG → topological sort → version check →
resource override graph → conflict policy → load → unresolved report.

**Resource merge semantics per type:**

- Material: deep-merge keys
- glTF model: per-mesh replacement
- LuaJIT script: full replace
- Texture: full replace
- Scenario: extend-only

**Implementation timing:** post-survival-slice (когда Z2 дойдёт до mod loading
milestone). Точный timing = operator's call.

---

## §10. AI Architecture (far future, Round 5 H3)

**Статус:** далёкое будущее. Сейчас не реализовывать.

**Принцип (per operator vision Round 5 H3):**

- **Быстрые решения (тактика на поле брани, спасение раненых, выбор действия здесь и
  сейчас) → символьный AI** (behavior trees, GOAP, utility AI)
- **Долгие решения (стратегия, lore generation, faction decision-making) → LLM**

### §10.1 Symbolic AI (hot path)

- Behavior trees + blackboard (closed `hierarchical-tactical-ai-btree` D ⭐)
- Squad coordination (closed `fire-coordination-multiple-units`)
- Suppression mechanics (closed `suppression-mechanics` D_AccumulatorThreshold)
- IFF (closed `iff-friendly-fire-prevention` B ⭐)
- Tactical decisions (urban combat, retreat, flank)

**Implementation:** Z3, Flecs ECS integration, ~660 LoC migration per closed experiment.

### §10.2 LLM AI (cold path)

- Faction strategic decisions (war declaration, alliances, diplomacy)
- Lore generation (civilization-style history, narrative events)
- Tech tree choices (strategic level)

**Implementation:** Z4 research + Z3 integration. **LLM integration через operator's
API key**, не на player machine (per Round 5 H3.a — operator inclination, confirmed).

### §10.3 Event log

- Global event log (all major events)
- Per-faction log (all choices, all decisions, all reasoning traces)
- Per-unit log опционально (для debugging)

**Для god-mode slice (DEFERRED per I.13):** visual AI decision inspection — click по
faction → видишь её strategy/state. Сейчас не в scope.

### §10.4 MVP definition (Round 5 H3.b)

- 4 AI factions на persistent sector map
- Каждая фракция имеет strategic state (territory, resources, diplomacy)
- Tactical AI для battles (symbolic)
- LLM для strategic decisions + lore generation
- 1+ hour симуляции без player input
- Event log + per-faction log сохраняются
- **Цель:** emergent history observable через logs

**Реалистичность layers:**

| Слой                                      | Реалистичность                           | Implementation     |
|-------------------------------------------|------------------------------------------|--------------------|
| Tactical AI (BT, squad coord)             | ✅ реально, closed experiments есть       | Z3                 |
| Strategic AI (sector capture, production) | ✅ реально, closed experiments есть       | Z3                 |
| Faction decision-making (war, alliances)  | ⚠️ частично, требует domain logic        | Z3 hybrid          |
| Lore generation                           | ❌ не реально без LLM                     | Z4 LLM integration |
| Emergent history                          | ⚠️ частично, event log + post-processing | Z3 + Z4            |

---

## §11. Persistence Strategy (post Z4 research, Round 2 D1)

**Статус:** Z4 task #1, kickoff в Phase 0.5.

**Research scope:**

| Backend                                                     | Плюсы                                             | Минусы                                    | Когда подходит                          |
|-------------------------------------------------------------|---------------------------------------------------|-------------------------------------------|-----------------------------------------|
| **Postgres**                                                | universal, robust, SQL, replication               | Heavy per-query, connection overhead      | Player state, faction metadata, economy |
| **SQLite**                                                  | embedded, zero-config, ACID                       | Operator назвал «говно»; concurrency weak | Single-player saves, mod storage        |
| **Dragonfly / Redis**                                       | in-memory KV, multi-threaded (Dragonfly), AOF/RDB | RAM-bound, less query-able                | Hot state cache, AOI player tracking    |
| **LMDB**                                                    | embedded KV, memory-mapped, very fast, ACID       | Single-writer, B+tree limitations         | Voxel chunk cache, snapshot store       |
| **Custom binary snapshots** (уже есть per knowledge.md §23) | Zero deps, max control, voxel-friendly            | No querying, manual replication           | Voxel world, entity state               |
| **Append-only journal + periodic snapshot**                 | Classic game-server pattern, replay-friendly      | Journal growth, compaction needed         | Tick state, replay/anti-cheat           |

**Реалистичная архитектура (preliminary, до research):**

- **Voxel world** → custom binary snapshots (уже есть) + delta journal для mutations
- **Player state** → Postgres (queriable, replication для multi-shard)
- **Hot runtime state** → in-process (не DB вообще)
- **AOI/interest tracking** → in-process spatial hashgrid, не DB

**Research tasks:**

1. Benchmark: throughput voxel mutation persistence (tick-by-tick vs periodic vs event-driven)
2. Prototype: Postgres vs LMDB vs custom для player state (10k players × N writes/sec)
3. Architecture proposal: что куда, hybrid layering
4. Recovery: crash recovery для persistent war (1-week uptime target)

**Output:** integration recommendation → Z1 implements persistence layer в `src/shared/persistence_api.hpp`.

---

## §12. Netcode Strategy (DEFERRED per I.12)

**Статус:** MP сейчас не в приоритете (per I.12). Стратегия зафиксирована для
future reference.

### §12.1 Lockstep для 2-8 coop (Round 1 C1 = a)

- Pure lockstep (per closed `lockstep-state-sync-hybrid-netcode` A_PureLockstep ⭐)
- 48-92 KB/s/player (в бюджете)
- Determinism contract: fixed timestep (60 Hz), FPU mode `_controlfp(_PC_24)` на x86,
  hardware determinism на ARM
- LuaJIT determinism: `-joff` interpreter mode (per `agent/knowledge.md §38`)
- CRC32 per snapshot для divergence detection

**Когда:** post-survival-slice, post-modern-war-slice, когда single-player stable.

### §12.2 Server-authoritative для massive persistent war (Round 1 C1 = a)

- Server-authoritative state sync (как Foxhole на самом деле)
- Server = single source of truth, клиенты — thin presentation
- AOI: spatial hashgrid per `interest-management-aoi-battle`
- 256-384 MiB/player (per `agent/knowledge.md §39`)
- 100 игроков на operator host (64 GiB RAM), 1000+ требует datacenter

**Когда:** далёкое будущее, post-stable-MP-foundation.

### §12.3 Esoteric 100+ lockstep (Round 1 C1 = c, Z4 research only)

- Lockstep-at-100+ как research experiment, **не main path**
- LuaJIT `-joff` + extensive cross-platform testing
- Likely unproven at scale (Foxhole не использует lockstep)
- Z4 research task, не mainline integration

---

## §13. Risk Register (updated для v2.0)

### §13.1 Технические риски

| Risk                                             | Severity | Mitigation                                                                                  |
|--------------------------------------------------|----------|---------------------------------------------------------------------------------------------|
| RTX-only path: non-RTX GPU = hard-fail           | M        | Сознательное решение. README чётко указывает hardware requirements                          |
| 5-agent merge conflicts на shared headers        | M        | `static_assert` byte-exact + root-agent integrator + shared API discipline (§2.3)           |
| Visual quality bar не достигается на RTX 3060 Ti | H        | Phase 0 polish iterations; если не получается — пересмотр target hardware                   |
| Persistence architecture wrong choice            | M        | Z4 research upfront (§11), benchmark before integration                                     |
| Lockstep determinism на разном hardware          | H        | Phase 3 (когда MP в scope) = фиксированный FPU mode + LuaJIT -joff + cross-platform testing |
| 1000+ AI units performance                       | M        | Phase 5 LOD для AI, multi-threading, gradual scaling                                        |
| Survival slice dependencies не собираются вместе | M        | Per-zone gates + root-agent coordination                                                    |
| LuaJIT determinism breach                        | M        | Mode-switch contract (§38), `-joff` enforced в lockstep path                                |

### §13.2 Процессные риски

| Risk                                                    | Severity | Mitigation                                                                    |
|---------------------------------------------------------|----------|-------------------------------------------------------------------------------|
| Operator burnout (soul project, бесконечный долгострой) | H        | Quarterly review; per-zone gates (не глобальные); opportunity для breaks      |
| AI-agent context overflow                               | M        | Per `AGENTS.md §5.8`: ≤5 параллельных read-only subagents; bounded zone scope |
| Документация устаревает                                 | M        | Этот roadmap + quarterly review; cross-refs в §18; source-of-truth дисциплина |
| Pet-project превращается в обязательство                | M        | I.2 + I.14 invariants; возможность «заморозить» проект на любой точке         |
| Multi-agent coordination overhead                       | M        | Phased rollout (§2.1); validation pipeline (§8) снимает «5-минутные проверки» |
| Dumber Z-agents делают глупости                         | M        | Style guide + shared API + clear zone READMEs; L1-L2 validation catches       |

### §13.3 Контентные риски

| Risk                                       | Severity | Mitigation                                                                          |
|--------------------------------------------|----------|-------------------------------------------------------------------------------------|
| Survival slice скучный без цели            | M        | Day/night + weather + AI wildlife + crafting = emergent gameplay                    |
| Modern war slice = «ещё одна военная игра» | M        | Unique selling points: AI автономные фракции + data-driven сеттинги + sandbox voxel |
| AI-driven lore «роботизированный»          | L        | Phase 10 LLM integration; operator review каждого milestone                         |
| Persistence ломается при больших worlds    | M        | Z4 research upfront; snapshot + journal hybrid                                      |

### §13.4 Бизнес / community риски

| Risk                                                | Severity | Mitigation                                      |
|-----------------------------------------------------|----------|-------------------------------------------------|
| Open-source после release: fork без контрибьюшнов   | L        | CLA если нужен; community-friendly governance   |
| Steam EA expectations: игроки ожидают polished game | M        | Early Access disclosure; explicit roadmap       |
| Hardware target меняется (новые RTX)                | M        | Quarterly review мониторит vendor announcements |
| NVIDIA exclusivity: AMD/Intel Arc пропускают        | L        | Сознательное решение per I.1                    |

### §13.5 Risk review cadence

- **Quarterly review** — пересмотр всех risks
- **Per-zone gate** — risk re-assessment перед каждым zone gate
- **Major external event** (NVIDIA announce, Vulkan spec change) — ad-hoc risk review

---

## §14. Per-Zone Decision Points (no global DPs per I.14)

**Принцип:** нет глобальных phase transitions (per I.14). Каждый Z-агент имеет свой
gate, operator approval требуется только на L4 (subjective per §1.2 I.6).

| DP      | Зона           | Что                                      | Документация                                     |
|---------|----------------|------------------------------------------|--------------------------------------------------|
| DP-1    | (Phase 0.5)    | Multi-agent infra готов                  | Build green + tests passing + cross-refs updated |
| DP-2    | Phase 0 visual | Visual quality bar (operator's eye)      | Operator's subjective approval per I.6           |
| DP-Z1-1 | Z1             | Engine stability для continuous sessions | 1+ hour без crash                                |
| DP-Z1-2 | Z1             | Frame budget met                         | 1080p × 120 FPS на VoxelLab                      |
| DP-Z2-1 | Z2             | JSON schemas approved                    | Operator review schemas                          |
| DP-Z2-2 | Z2             | LuaJIT mode-switch работает              | JIT single-player test passes                    |
| DP-Z2-3 | Z2             | First scenario sandbox_default.json      | Operator playtests                               |
| DP-Z3-1 | Z3             | Combat MVP                               | 1 weapon + damage model + HP work                |
| DP-Z3-2 | Z3             | Basic AI                                 | 2+ hostile wildlife types work                   |
| DP-Z4-1 | Z4             | Persistence research output              | Integration recommendation ready                 |
| DP-SURV | (cross-zone)   | Survival slice works end-to-end          | 30+ min continuous survival session              |
| DP-MW   | (cross-zone)   | Modern war slice works end-to-end        | 30-min match works                               |

**Quarterly Decision Points (per I.10):**

| QDP   | Что            | Когда                       |
|-------|----------------|-----------------------------|
| QDP-1 | Q3 2026 review | 2026-09-25 (~3 мес от v2.0) |
| QDP-2 | Q4 2026 review | 2026-12-25                  |
| QDP-3 | Q1 2027 review | 2027-03-25                  |
| QDP-4 | Q2 2027 review | 2027-06-25                  |

**Каждый QDP пересматривает:**

- Zone progress (что заняло больше / меньше)
- Risk register (новые риски)
- Validation pipeline (нужны ли новые layers)
- Open questions (что закрылось / появилось)
- Scope (всё ещё achievable?)
- Multi-agent cadence (расширять количество сессий?)

---

## §15. Quarterly Review

**Cadence:** каждый QDP per §14.

**Что review'ится:**

- Прогресс по зонам vs оценочные сроки
- Risk register updates
- Новые closed experiments (Z4) → integration candidates
- Hardware/vendor changes (новые RTX, драйвера, Vulkan spec)
- Operator morale / burnout check
- Multi-agent model cadence (расширять / сужать?)

**Output:** обновления этого roadmap'а (по operator approval), обновления
`agent/knowledge.md` (engineering contracts), обновления `agent/workspace.md`.

**Log:** `docs/QUARTERLY_REVIEW.md` (если объём вырастет) или секция в
`agent/workspace.md`.

---

## §16. Public Release Track (far future per I.9 + I.14)

**Принцип:**

```
private development (continuous, asymptote-бесконечный)
    │
    ├── Phase 0 (visual) → Phase 0.5 (infra) → parallel zones → survival slice
    │   → modern war slice → ... → «абсолютно всё сделано» (asymptote)
    │
    └──[когда operator решает «всё готово»]──> public release
                                                    │
                                                    ├─ A: open-source GitHub
                                                    ├─ B: public demo build
                                                    └─ C: Steam Early Access
```

**Private development = current state и обозримое будущее.**

### §16.1 A: Open-source на GitHub

**Когда:** post-operator's-«всё готово» approval.

**Что включает:**

- LICENSE файл (MIT / Apache 2.0 / BSL — отдельное решение)
- Public GitHub repository
- Comprehensive README
- CI/CD pipeline (GitHub Actions)
- Issue / PR templates
- CONTRIBUTING.md, SECURITY.md
- CHANGELOG.md

### §16.2 B: Public demo build

**Когда:** после A или параллельно.

**Что включает:**

- Precompiled binaries (Linux + Windows)
- itch.io или GitHub Releases
- Demo scenario (limited content showcase)
- «Try before you contribute» experience

### §16.3 C: Steam Early Access

**Когда:** long-term post-A+B.

**Что включает:**

- Steam page
- Steam SDK integration (achievements, workshop, multiplayer)
- Workshop: mod distribution
- Early Access pricing (one-time purchase, не F2P)
- Roadmap to 1.0 publicly visible

### §16.4 Anti-goals для release

- F2P / battle pass / live service
- In-game purchases
- Engagement metrics / telemetry без consent
- DRM (Steam DRM достаточно)
- Anti-piracy beyond Steamworks

---

## §17. Q&A Log (6 раундов, оператор + root-agent, 2026-06-25)

Полный журнал решений: `docs/QA_LOG_roadmap_v2.md`. Краткая сводка ниже.

### Round 1 — structural contradictions

| Q  | Topic                                    | Answer                                             | Implication                                                       |
|----|------------------------------------------|----------------------------------------------------|-------------------------------------------------------------------|
| A1 | Phase 0 vs Phase 1                       | «Это всё нулевой этап, значит, это ошибка»         | Phase 0+1 слиты в один «visual» этап                              |
| A2 | Multi-axis parallel vs sequential phases | **b**                                              | Parallel-from-Phase-0.5, per-zone gates                           |
| A3 | Phase 0.5 timing                         | «можно»                                            | Phase 0.5 параллельно с Phase 0                                   |
| B1 | 5 agents deployment                      | «5 отдельных opencode агентов со своим контекстом» | 5 parallel sessions, file-based coordination                      |
| B2 | CI/CD timing                             | «согласен»                                         | CI/CD переезжает из Phase 8 в Phase 0.5                           |
| C1 | Lockstep vs server-auth                  | **a + c**                                          | Lockstep для 2-8, server-auth для massive, esoteric 100+ research |
| C2 | LuaJIT determinism                       | «надо решить»                                      | Mode-switch contract (§38)                                        |
| C3 | DLSS Open Question                       | «уже делается»                                     | Q4 closed, pre-roadmap baseline                                   |
| D1 | Persistence                              | «надо исследовать»                                 | Z4 task #1 (§11)                                                  |
| D2 | Memory budget                            | «нет 16GB сервера, 64GB у меня»                    | 100 players на 64GB host (§39)                                    |
| D3 | Visual gate                              | «я есмь критерий»                                  | Subjective, no metrics                                            |

### Round 2 — research-informed follow-ups

| Q    | Topic                      | Answer                                                         | Implication                                        |
|------|----------------------------|----------------------------------------------------------------|----------------------------------------------------|
| C2   | LuaJIT contract approve    | «даю добро»                                                    | §38 зафиксирован                                   |
| D1   | Persistence research scope | «да»                                                           | Z4 task scoped                                     |
| D2   | Memory budget              | «физически 64ГБ, 1000 игроков – необозримое будущее, принимаю» | §39 зафиксирован                                   |
| E1.a | Zone composition           | «пойдёт, но MP не приоритет»                                   | Z3 переориентируется на combat+AI                  |
| E1.b | Shared API owner           | «что за shared api?»                                           | Объяснено, owner = Z1 per `agent/knowledge.md §40` |
| E2   | Per-zone gates             | «пойдёт»                                                       | Принято                                            |
| E3   | Audio subsystem            | «Согласен»                                                     | Z1 (core) + Z3 (gameplay audio) + Z4 (research)    |
| E4   | Mod conflict resolution    | detailed vision                                                | F3 architecture (§9)                               |
| E5   | Playability cadence        | «надо придумать»                                               | Validation pipeline (§8)                           |
| E6   | Phase 0.5 deliverables     | «всё + style guide + branching»                                | §3.1 состав зафиксирован                           |

### Round 3 — concretization

| Q  | Topic                   | Answer                          | Implication                            |
|----|-------------------------|---------------------------------|----------------------------------------|
| F1 | MP deprioritization     | «y»                             | Z3 priority: combat → AI → (далеко) MP |
| F2 | Shared API concept      | «y»                             | §40 зафиксирован                       |
| F3 | Mod system architecture | «good»                          | §9 + §41 зафиксированы                 |
| F4 | Validation pipeline     | L1 без vision diff, drop stress | §8 + §42 зафиксированы                 |
| F5 | Branching               | «hybrid»                        | §43 зафиксирован                       |
| F6 | Style guide scope       | «ты пиши Phase 0.5»             | solo root-agent для Phase 0.5          |

### Round 4 — feasibility & execution

| Q    | Topic                 | Answer                             | Implication                       |
|------|-----------------------|------------------------------------|-----------------------------------|
| G1   | 5-session feasibility | **a** (phased rollout)             | §2.1 model                        |
| G2   | Revised validation    | «принимается кроме ai vision diff» | §8 финал                          |
| G3.a | Phase 0.5 order       | «roadmap first, ты забыл?»         | §3.2 reordered, ROADMAP v2.0 = #1 |
| G3.b | Refactor experiments  | «делаем всё»                       | §2.5 full migration               |
| G3.c | Mod system core       | «отложить надолго»                 | §9 DEFERRED                       |
| G4   | Phase 0 exit mechanic | «не твоё дело»                     | Drop, operator's domain           |

### Round 5 — strategic depth

| Q  | Topic                            | Answer                                                              | Implication           |
|----|----------------------------------|---------------------------------------------------------------------|-----------------------|
| H1 | Persistence research first steps | «y»                                                                 | §11 Z4 task #1 scoped |
| H2 | Vertical slices                  | Modern War 1 role, Fantasy+God-mode OUT, survival first             | §6 + §7 + I.13        |
| H3 | AI stack                         | Symbolic hot + LLM cold, far future                                 | §10                   |
| H4 | Public release                   | **a**                                                               | §16                   |
| H5 | Playable                         | «вершина, до которой не дотянуться, пока не сделаешь абсолютно всё» | I.14                  |

### Round 6 — final clarifications

| Q  | Topic                      | Answer                     | Implication                              |
|----|----------------------------|----------------------------|------------------------------------------|
| I1 | Old ROADMAP v1.0 fate      | **c** (inline merge)       | Этот файл = v2.0, замещает v1.0 in-place |
| I2 | TODO.md fate               | **a** (оставить как есть)  | TODO.md не трогать                       |
| I3 | Knowledge.md new contracts | **a** (все 6 как §-номера) | §38-43 добавлены                         |
| I4 | Survival deps              | «y»                        | §6 dependencies зафиксированы            |
| I5 | Ready to rewrite           | «go»                       | Этот файл пишется                        |

---

## §18. Cross-refs

### §18.1 Sources of truth (per `AGENTS.md §3` + `agent/knowledge.md §0`)

```
1. Код (.cpp/.hpp/.ixx + шейдеры + тесты)   ← абсолютный приоритет (реальность)
2. AGENTS.md                                 ← протокол работы
3. agent/knowledge.md                        ← engineering contracts (что не меняется)
4. agent/workspace.md                        ← snapshot текущего состояния
5. ROADMAP.md (этот файл)                    ← стратегическое направление
6. docs/VulkanSDK-Linux-Docs-1.4.350.1/      ← vendor docs
7. TODO.md                                   ← tactical roadmap (sub-tasks)
8. docs/experiments/INDEX.md                 ← research registry
9. docs/experiments/research/backlog.md      ← hypothesis канбан
10. legacy/docs/philosophy/                  ← design principles
```

### §18.2 Cross-refs на конкретные документы

**AGENTS.md:**

- §1 — назначение и правило изменений
- §2 — Project metadata
- §3 — sources of truth ranking
- §5.1 — commit message format
- §5.3 — web search обязателен
- §5.5 — multi-agent coordination (extended в §2 этого файла)
- §5.7 — code quality, комментарии
- §6 — Definition of Done
- §7 — рабочий чеклист
- §8 — communication & planning discipline

**agent/knowledge.md:**

- §0 — sources of truth ranking
- §1 — Document boundaries (updated для v2.0)
- §2 — Hardware target
- §6 — Voxel storage
- §14 — RTX shadows
- §15 — RTX GI: DDGI
- §16 — SSBO byte-exact (КРИТИЧНО для multi-agent)
- §23 — Snapshot round-trip
- §37 — NVIDIA Streamline integration
- §38 — LuaJIT mode-switch determinism (NEW v2.0)
- §39 — Memory budget per scenario (NEW v2.0)
- §40 — Shared API boundaries (NEW v2.0)
- §41 — Mod manifest format (NEW v2.0)
- §42 — Validation pipeline (NEW v2.0)
- §43 — Branching strategy (NEW v2.0)

**TODO.md:**

- §0-1 — Status-quo и RTX-only pivot
- §5.2 — RTX-driven milestones (closed)
- §7.x — post-RTX-shadow polish (active)

**docs/experiments/:**

- INDEX.md — реестр 170+ closed экспериментов
- research/backlog.md — канбан гипотез (69 open)
- research/backlog_closed.md — closed исследования
- benchmarks/methodology.md — стандарт измерений
- AGENTS.md — протокол research-агента (Z4 в v2.0)

**legacy/docs/philosophy/:**

- README.md — общая философия
- 01_foundation/03_decision-making.md — дизайн-эвристики
- 02_paradigms/02_dod-philosophy.md — DoD
- 03_domain/01_optimization-philosophy.md — перф-философия

### §18.3 Reference на closed эксперименты (для zone integration)

**Survival slice (§6) integration candidates:**

- `2026-06-22-day-night-cycle-celestial-mechanics` (C ⭐) → day/night
- `2026-06-22-procedural-voxel-tree-generation` (mixed) → trees
- `2026-06-22-procedural-voxel-resource-deposits` (mixed) → resources
- `2026-06-22-resource-harvesting-economy` (mixed) → harvesting
- `2026-06-22-voxel-water-flow-ca` (mixed) → water
- `2026-06-22-voxel-heat-conduction-cost` (mixed) → temperature
- `2026-06-21-hierarchical-tactical-ai-btree` (mixed, D ⭐) → wildlife AI
- `2026-06-22-procedural-voxel-material-audio` (yes) → audio feedback

**Modern war slice (§7) integration candidates:**

- `2026-06-21-ballistic-projectile-simulation` → ballistics
- `2026-06-22-tank-terrain-interaction-physics` → tank physics
- `2026-06-22-suppression-mechanics` (D ⭐) → suppression
- `2026-06-22-iff-friendly-fire-prevention` (B ⭐) → IFF
- `2026-06-22-player-roles-hierarchy` (D ⭐) → roles (later, multi-role expansion)
- `2026-06-22-engineer-capabilities-system` (C ⭐) → engineers (later)
- `2026-06-22-sector-strategic-map-system` → sector control
- `2026-06-22-capture-repair-enemy-equipment` (C ⭐) → capture mechanics
- `2026-06-22-minefield-laying-clearing` → minefields

**Persistence research (§11):**

- (new Z4 research, no direct closed experiment)

**AI stack (§10):**

- `2026-06-21-hierarchical-tactical-ai-btree` (mixed, D ⭐) → behavior trees
- `2026-06-22-drone-swarm-tactics` → swarm tactics
- `2026-06-22-urban-combat-tactics-ai` → urban tactics
- `2026-06-22-tech-tree-research-system` (E ⭐) → tech tree
- `2026-06-22-supply-logistics-simulation` → logistics
- `2026-06-22-factory-production-system` → production

**Multiplayer (§12, DEFERRED):**

- `2026-06-21-lockstep-state-sync-hybrid-netcode` (mixed) → lockstep transport
- `2026-06-21-interest-management-aoi-battle` (mixed) → AOI
- `2026-06-21-persistent-war-server-architecture` (yes) → server architecture
- `2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer` (mixed) → anti-cheat

**Audio (cross-zone):**

- `2026-06-21-audio-raytracing-voxel-sdf` (mixed) → propagation
- `2026-06-22-ambient-battlefield-audio` → ambient
- `2026-06-22-explosion-acoustic-variety` → explosions
- `2026-06-22-large-scale-spatial-audio-battle` → spatial
- `2026-06-22-procedural-engine-sound` → vehicle audio
- `2026-06-22-radio-communication-audio` → radio
- `2026-06-22-procedural-voxel-material-audio` → material physics audio

---

## §19. History

### v2.0 (2026-06-25, этот файл)

**Trigger:** post-Q&A сессия с оператором (6 раундов, ~30 вопросов).

**Что изменилось от v1.0:**

- §1 Vision: minor updates (MP deferred, playable = asymptote)
- §1.2 Invariants: **+4 новых** (I.11 LuaJIT mode-switch, I.12 MP deferred,
  I.13 slice scope reduction, I.14 playable = asymptote)
- §1.3 Anti-Goals: extended (fantasy/god-mode slices OUT, playable MVP OUT)
- §2 Multi-agent: **phased rollout** (1 → 2 → 3 → опц. 5), не 5 сразу
- §3 Phases: **упразднена sequential phase model** (Phase 1-8 sequential);
  replaced на Phase 0 (visual, operator's domain) + Phase 0.5 (infra) +
  continuous zone work
- §4 Quality Gates: replaced на **per-zone gates** (no global DPs except DP-1/DP-2)
- §5 Release Strategy: moved to far future (per I.9 + I.14)
- §6 Cross-refs: updated
- §7 Risk Register: updated для v2.0 realities
- §8 Open Questions: **resolved** (most answered в Q&A)
- §9 Decision Points: replaced на per-zone DPs (§14)
- §10 Appendices: cleaned up
- §11 History: этот раздел
- **NEW sections:** §6 Survival Slice, §7 Modern War Slice, §9 Mod Architecture,
  §10 AI Architecture, §11 Persistence, §12 Netcode Strategy, §17 Q&A Log

**Author:** root-agent (этот файл), operator-approved через 6 раундов Q&A.

### v1.0 (2026-06-25, superseded)

**Status:** superseded by v2.0 в том же файле (per Round 6 I1=c inline merge).

**Содержание:** 1913 строк, 8 sequential phases (Phase 0-8), global quality gates,
5-agent model с самого старта, 4 vertical slices (modern war / fantasy / god-mode /
survival), playable gate как Phase 4 DoD.

**Где посмотреть:** git history этого файла (commit prior to v2.0 rewrite).

### Pre-reset roadmaps

`legacy/docs/archive/2026-06-24-pre-reset-snapshot/` — historical artifact, не цитировать
как authoritative.

---

## §20. Заключение

ProjectV — это **soul project**, бесконечный долгострой на стахановских темпах через
AI-агентов. Этот roadmap v2.0 — попытка структурировать путь, с явными decision
points для оператора на каждом zone gate.

**Ключевая философия v2.0:**

- **Playable = асимптота**, не near-term gate. Roadmap = continuous development.
- **Multi-agent = phased rollout**, не 5 сессий сразу. Validation pipeline решает.
- **Survival slice first**, modern war потом. Fantasy/god-mode — далёкое будущее.
- **MP deferred**. Сначала single-player sandbox + combat + AI.
- **Per-zone gates**, не глобальные phase transitions.

**Стахановские темпы через AI-агентов + дисциплина.** Это наше конкурентное
преимущество. Используем его, но без выгорания оператора.

— root-agent, 2026-06-25, post-6-round-Q&A
