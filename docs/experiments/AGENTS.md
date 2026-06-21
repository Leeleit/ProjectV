# AGENTS.md — протокол исследователя `docs/experiments/`

> ## ⛔ HARDWARE PROBE BLOCKER (per operator directive 2026-06-20)
>
> Прежде чем запускать `lscpu`, `free`, `vulkaninfo`, `nvidia-smi`, `dmidecode`, `lshw`, `uname -a`,
> `cat /proc/cpuinfo` или любой другой **hardware-probe** для проверки текущего хоста — **СТОП**.
>
> **Правило:**
> 1. Прочитай [`hardware-profile.md`](./hardware-profile.md).
> 2. Если `**Captured:**` в шапке файла **<14 дней назад** (от текущей даты) — данные актуальны. **Используй файл.
     > НЕ ЗАПУСКАЙ probe-команды.**
> 3. Если `**Captured:**` **≥14 дней назад** или файл отсутствует — запусти refresh-команду из шапки файла,
     > перезапиши данные, обнови дату, **затем** работай.
> 4. При обновлении файла — зафиксировать новую дату + сообщить оператору.
>
> **Запрещено как ритуал:**
> - ❌ «просто проверю, не поменялся ли CPU» — `lscpu` / `cat /proc/cpuinfo` без необходимости.
> - ❌ «на всякий случай гляну `vulkaninfo`» — если данные уже в `hardware-profile.md` и свежие.
> - ❌ «сделаю `nvidia-smi` чтобы уточнить VRAM» — если VRAM уже в §3 файла.
> - ❌ «посмотрю `free` для RAM» — если RAM уже в §2 файла.
> - ❌ «проверю `uname` для ядра» — если kernel уже в §5 файла.
> - ❌ Любой другой probe «для уверенности» при наличии свежего `hardware-profile.md`.
>
> **Файл = single source of truth.** Дублирование probe — пустая трата времени + риск получить разные ответы
> в разных сессиях (driver state меняется между запусками).
>
> Подробности, исключения, edge-cases: §14. Hardware baseline для каждого experiment README: cross-ref
> в `experiments/_TEMPLATE/README.md §9`.

Файл стабильный, но изменяемый. Правка — **только по явной команде оператора**. Правка кода (вне моей папки) не даёт
права
править этот файл.

---

## 1. Назначение

**Роль:** frontier-исследователь и экспериментатор ProjectV.

**Что делаю:**

- Web-research по темам, релевантным движку (SOTA 2024–2026, новые API, фундаментальные работы).
- Прототипы идей, которые могут дать прирост производительности, открыть новые фичи или механику.
- Мини-бенчмарки и аналитические модели.
- Чёткие вердикты и рекомендации по интеграции в mainline.

**Что НЕ делаю:**

- Не коммичу. Не делаю `git *` вообще.
- Не правлю `src/`, `agent/`, корневой `AGENTS.md`, `TODO.md`, `docs/*` (вне моей папки).
- **Не запускаю `cmake --build`, `ctest`, бинарь mainline ProjectV** (корневой `CMakeLists.txt` →
  `build/linux-clang-*/bin/ProjectV`, `ProjectVTests`). Mainline собирает и гоняет отдельный агент
  (см. корневой `AGENTS.md §6` DoD). Моя зона — изолированный research mainline.
- **Собирать и запускать свои прототипы в `experiments/<slug>/prototype/` — разрешено и ожидается.**
  Это мой собственный код, не mainline; build-dir прототипа обязан быть **внутри** `prototype/`
  (например, `prototype/build/`), не в корне репо и не в общем `build/` рядом с mainline.
- Не «исправляю» mainline — только рекомендации через секцию `Integration recommendation` в `README.md` эксперимента.

---

## 2. Scope discipline

| Действие                                   | Разрешено                   |
|:-------------------------------------------|:----------------------------|
| Читать что угодно в репозитории            | Да                          |
| Читать web                                 | Да                          |
| Писать внутри `docs/experiments/`          | Да                          |
| Писать за пределами `docs/experiments/`    | **Нет**                     |
| `git *`                                    | **Нет**                     |
| Запускать cmake/ctest/ProjectV-бинарь (mainline, корневая сборка) | **Нет**           |
| Собирать и запускать **свои** прототипы в `experiments/<slug>/prototype/` | **Да**            |
| Копировать содержимое `agent/knowledge.md` | **Нет** (только cross-refs) |

---

## 3. Источники истины (внутренний приоритет, от высшего к низшему)

1. **Код ProjectV** (`.cpp`, `.hpp`, `.ixx`, шейдеры, тесты) — главный по реальности.
2. **Корневой `AGENTS.md`** — протокол mainline-агента.
3. **`TODO.md`** — roadmap и активные стадии; чтобы понимать, **куда** mainline собирается двигаться, и не дублировать.
4. **`docs/`** (архитектура, рендер, билд, профайлинг, source_layout) — как устроен mainline сейчас.
5. **`legacy/docs/philosophy/`** — особенно:
    - `01_foundation/03_decision-making.md` — дизайн-эвристики.
    - `02_paradigms/02_dod-philosophy.md` — DoD.
    - `03_domain/01_optimization-philosophy.md` — перф-философия.
    - `03_domain/04_testing-philosophy.md` — покрытие тестами.
    - `03_domain/05_math-and-space.md` — геометрия/пространство.
6. **`agent/knowledge.md`** — контекст для понимания; **не копировать**.
7. **Web** (Exa + fallbacks) — для свежих API, фич, SOTA, бестпрактик.

---

## 4. Web search (обязателен)

По аналогии с корневым `AGENTS.md §5.3` — для сложных тем сначала web-search, **затем** код.

Когда **обязательно** искать:

- Свежие API / расширения Vulkan (mesh shaders, RT, bindless, DEC, video codec).
- Состояние SOTA по теме эксперимента (что нового за последние 12–24 месяца).
- Сравнения подходов (например, Sparse 64-tree vs VDB vs BVH).
- Библиотеки для прототипа (есть ли готовые бенчмарки, baseline-реализации).
- Вендор-специфика (NVIDIA/AMD/Intel RT, mesh shader, conservative raster).

Когда **НЕ искать**:

- Тривиальные задачи (переименование, очевидный рефакторинг).
- Локальный код ProjectV — читается через `rg` / `read`.

**Фоллбеки** при сбое основного web_search: указаны в `agent/knowledge.md Part B §9` (если недоступен — уведомить
оператора).

---

## 5. Структура папки

```
docs/experiments/
├── AGENTS.md                       # этот файл
├── INDEX.md                        # снимок состояния + реестр
├── README.md                       # короткий human-orientation
├── research/
│   └── backlog.md                  # канбан гипотез (open / in-progress / closed)
├── benchmarks/
│   └── methodology.md              # стандарт измерений
└── experiments/
    ├── _TEMPLATE/                  # шаблон одного эксперимента
    │   └── README.md
    └── YYYY-MM-DD-<slug>/          # каждый эксперимент
        ├── README.md               # единый файл-источник истины
        ├── STATUS.md               # текущее состояние
        ├── sources.md              # опц., если много ссылок
        └── prototype/              # опц., самодостаточный код
```

**Slug:** `YYYY-MM-DD-<topic-kebab-case>`. Дата открытия + тема.

---

## 6. Статусы эксперимента

| Статус                    | Когда                                                                       |
|:--------------------------|:----------------------------------------------------------------------------|
| `open`                    | Гипотеза зафиксирована в `README.md`, работа не начата.                     |
| `in-progress`             | Идёт research / prototype / benchmark.                                      |
| `concluded-verdict-yes`   | Гипотеза подтверждена; рекомендуется интеграция.                            |
| `concluded-verdict-no`    | Гипотеза не подтверждена; интеграция не рекомендуется.                      |
| `concluded-verdict-mixed` | Подтверждена частично; нужны условия / ограничения.                         |
| `parked`                  | Перспективно, но не сейчас (Stage ещё не подошёл / нет ресурса).            |
| `abandoned`               | Идея снята (SOTA ушёл вперёд, дубликат, нерелевантно).                      |
| `blocked`                 | Нужен ответ/решение от mainline или оператора; зафиксировать что блокирует. |

Переходы — в `STATUS.md` короткой строкой + дата.

---

## 7. Формат эксперимента

См. `experiments/_TEMPLATE/README.md`. Обязательные секции:

1. **Hypothesis** — что предполагаю; какое преимущество; какие альтернативы.
2. **Prior art** — web-research, ключевые источники (сначала web-search, затем верификация цитат).
3. **Method** — как проверяю.
4. **Prototype** — где код; как воспроизвести; что измерял (если есть).
5. **Results** — цифры, наблюдения, ограничения.
6. **Verdict** — yes / no / mixed / parked / abandoned.
7. **Integration recommendation** — что mainline должен сделать (или не делать), куда это ложится в `TODO.md`, с
   рисками.
8. **Sources** — список ссылок.

`STATUS.md` — короткий (5–15 строк): фаза, последнее действие, блокер, дата следующего тика.

---

## 8. Бенчмарк-методология

Если эксперимент включает измерения, прототип обязан следовать `benchmarks/methodology.md`. Краткие инварианты:

- Минимальный harness: warm-up + N замеров, отдельные прогоны для mean/median/p95/std.
- Изоляция: фиксированный governor / CPU affinity / pinned core (если возможно).
- Без шумных соседей: фиксировать окружение.
- Формат вывода: machine-readable (CSV/JSON) + human-readable сводка.
- Привязка к ProjectV: как именно результат мапится на hot-path движка (даже если прототип standalone).

---

## 9. Self-audit / tool availability

Что у меня есть:

- `read_file`, `rg` / `grep`, `glob` — для чтения кода ProjectV.
- `web_search` (Exa) + `webfetch` — для web-research.
- `bash` — для ad-hoc проверок внутри моей папки (`ls`, `wc`, `du`, `find` по `docs/experiments/`).
- `write_to_file`, `edit` — для создания/правки файлов **внутри** `docs/experiments/`.
- `task` (subagent) — для параллельного research (только read-only, как в корневом `AGENTS.md §5.8`).

Чего у меня **нет** / запрещено:

- `git *` — полностью запрещён.
- Изменения файлов за пределами `docs/experiments/`.
- Запуск `cmake --build` / `ctest` / бинарника **mainline ProjectV** (корневая сборка).
- Сборка и запуск **моих** прототипов в `experiments/<slug>/prototype/` — **разрешены** (build-dir внутри
  `prototype/`, например `prototype/build/`; не использовать корневой `build/`).

При сбое инструмента — не строить гипотез «почему», зафиксировать в `STATUS.md` эксперимента и позвать оператора.

---

## 10. Definition of done (для одного эксперимента)

Эксперимент считается завершённым, если:

- [ ] Все 8 секций `README.md` заполнены (или осознанно помечены «N/A»).
- [ ] `STATUS.md` отражает актуальное состояние.
- [ ] `INDEX.md` обновлён: эксперимент появился в соответствующей секции.
- [ ] `research/backlog.md` обновлён: галочка проставлена, slug закрыт.
- [ ] Если есть прототип — он воспроизводим (команды запуска в `README.md` или `prototype/README.md`).
- [ ] Integration recommendation написан так, чтобы mainline-агент мог забрать его без дополнительных вопросов.

---

## 11. Что НЕ делать как ритуал

- Синхронизация всех документов после каждого тика. Обновлять **только** то, что изменилось в рамках текущего
  эксперимента.
- Копирование больших блоков из `agent/knowledge.md` к себе. Только cross-refs (`agent/knowledge.md §X.Y`).
- Превращение `README.md` эксперимента в талмуд: писать коротко, по сути, со ссылками.
- Ложные вердикты «yes» без измерений. Без цифр — статус `mixed` или `parked`.

---

## 13. Topic reservation protocol (added 2026-06-20 per operator request)

**Цель:** дать оператору и параллельным агентам возможность запускать несколько экспериментов параллельно без
конфликтов scope.

**Прецедент:** `2026-06-20` — параллельная сессия завершила `mesh-shader-vs-compute-cull`
(`concluded-verdict-mixed`) одновременно с моим `sparse-64-tree-alternatives` (`concluded-verdict-yes`). Темы
различны → конфликта не было, но **отсутствие формального protocol — случайность, не design**. Этот раздел —
страховка на будущее.

### 13.1 Claim process (обязательно перед стартом)

Перед стартом нового эксперимента (из `research/backlog.md` или по явной команде оператора) агент **обязан**:

1. Проверить `research/backlog.md §In progress` — нет ли уже эксперимента по смежной теме (anti-duplicate).
2. Проверить `INDEX.md §5 Active experiments` — синхронизация с возможно-concurrent сессиями (другой агент мог
   не обновить `backlog.md`, но создать `experiments/<slug>/`).
3. Проверить `ls docs/experiments/experiments/` — те же соображения (parallel session могла оставить папку
   без обновлённого backlog/INDEX).
4. **Если свободно:**
   a. Переместить slug из `§Open` в `§In progress` в `research/backlog.md`.
   b. Заполнить обязательные поля reservation (см. §13.2).
   c. Создать `experiments/<slug>/{README.md, STATUS.md}` со статусом `in-progress` per §6.
   d. Обновить `INDEX.md §5 Active experiments` (одна строка).
5. **Если занят:**
   a. Эскалировать оператору (per §13.3).
   b. **Не** начинать работу до разрешения.

### 13.2 Reservation record format

Запись в `research/backlog.md §In progress`:

```markdown
- [ ] **<slug>** — <priority (h/m/l)>, <stage link (TODO.md §X.Y или `independent`)>.
  **Agent:** <`self` / `operator-direct` / `subagent-<task_id>`>.
  **Started:** YYYY-MM-DD.
  **ETA:** YYYY-MM-DD или `next tick` или `по запросу оператора`.
  **Blocker:** <нет / описание>.
  **Hypothesis (one-line):** <что проверяем, какое преимущество>.
  **Scope (paths):** <какие файлы/папки внутри `docs/experiments/` будут тронуты>.
```

### 13.3 Conflict resolution

Если два агента racing за один slug (timestamp в `backlog.md` — first-write-wins):

1. **Прекратить race.** Второй агент останавливает работу, не пишет в `experiments/<slug>/`.
2. **Эскалировать оператору.** Приоритет оператора > протокол.
3. **Альтернативы для второго агента:**
    - Взять adjacent slug (related topic, не дубль).
    - Подождать closure первого (если первая задача короткая).
    - Operator override: явная команда «возьми этот slug» (агент обязан выполнить + зафиксировать override в
      `STATUS.md`).
4. **Зафиксировать конфликт** в обоих `STATUS.md` (если уже созданы) или эскалировать без них.

### 13.4 Subagent reservations

Per корневой `AGENTS.md §5.8` — subagents read-only. Subagent **не может** создавать reservations. Если subagent
нашёл тему, которая warrants отдельный эксперимент — возвращает finding в parent, parent решает claim (через §13.1).

### 13.5 Reservation lifecycle (sync с §6)

| Reservation state             | STATUS.md status         | INDEX.md section       |
|:------------------------------|:-------------------------|:-----------------------|
| `§Open` в `backlog.md`        | (none, нет папки)        | (none)                 |
| `§In progress` в `backlog.md` | `open` или `in-progress` | §5 Active              |
| `§Closed` в `backlog.md`      | `concluded-verdict-*`    | §6 Recent closed       |
| `§Rejected` в `backlog.md`    | `abandoned`              | (none)                 |
| `§In progress` + blocked      | `blocked`                | §5 Active (с пометкой) |

**Sync-обязательство:** при смене статуса обновлять **все три места** за одну операцию (single-pass), не
растягивать sync на несколько тиков.

### 13.6 Operator override

Оператор может явно проинструктировать: «запусти эксперимент по slug X даже если reservation conflict». Агент
**обязан** выполнить (operator > protocol), но **обязан** зафиксировать override в `STATUS.md`:
`Operator override: <причина, дата, ссылка на команду оператора>`.

### 13.7 Anti-duplicate sentinel

При старте любого нового эксперимента (пункт 1 §13.1, обновлённая версия):

```bash
rg -l "experiments/<slug>" docs/experiments/ 2>/dev/null
ls docs/experiments/experiments/<slug>/ 2>/dev/null
```

Если результат непустой — slug уже занят (старый или активный), выбрать другой или явно supersede старый
(зафиксировать в `STATUS.md` старого и нового).

### 13.8 Operator multi-agent invocation pattern

Если оператор хочет запустить **N параллельных** экспериментов сразу:

1. Перечислить N slug-ов в одной команде (явно).
2. Указать приоритеты (h/m/l) — если несколько h, оператор указывает порядок.
3. Каждый агент в каждой параллельной сессии выполняет §13.1 (включая sentinel §13.7).
4. Если один из N уже running (per `backlog.md §In progress` или `INDEX.md §5`) — этот slot пропускается,
   не блокирует остальные.
5. Если несколько агентов racing за один из N (race condition в момент старта) — применять §13.3.

---

## 14. Hardware profile reference

> **⛔ См. STOP-блок в начале файла.** Прежде чем запускать любой hardware-probe (`lscpu`, `free`, `vulkaninfo`,
> `nvidia-smi`, `dmidecode`, `lshw`, `uname -a`, `cat /proc/cpuinfo`) — прочитай `hardware-profile.md` и проверь
> дату в шапке. Свежие данные (<14 дней) — используй файл, **не запускай probe**. Устаревшие (≥14 дней) — refresh
> из шапки файла, **затем** работай.

**Один файл = single source of truth для hardware data:** [`hardware-profile.md`](./hardware-profile.md).

**Что там:** CPU (модель, кэши, ISA-флаги), RAM, GPU (модель, VRAM, частоты), Vulkan extensions (subset
релевантный ProjectV), OS/kernel, toolchain (clang/lld/cmake/SDL3/glslc/Tracy), storage (NVMe/HDD), per-stage
references.

**Когда читать:** перед любым измерением или hardware-specific assertion. В частности:

- Перед `lscpu`/`free`/`vulkaninfo`/`nvidia-smi` — **сначала проверить** `hardware-profile.md`. Если данные уже
  там и актуальны — не повторять probe. Если host менялся (новый GPU / CPU / swap) — обновить файл + сообщить
  оператору.
- Перед формулировкой hypothesis о perf / cache / VRAM — cross-ref к §8 «Per-stage references» (какие данные
  релевантны для какой стадии ProjectV).

**Когда обновлять (только если реально изменилось):**

- Host менялся (upgrade, swap, переустановка, второй GPU) → перезапустить refresh-команду из шапки
  `hardware-profile.md`, перезаписать файл, зафиксировать дату в шапке.
- Новая стадия ProjectV попадает в scope → добавить строку в §8 «Per-stage references».

**Anti-ritual (явный список запрещённых действий):**

- ❌ **Не дублировать** данные из `hardware-profile.md` в README эксперимента. Использовать cross-ref.
- ❌ **Не прогонять** `vulkaninfo`/`lscpu`/`nvidia-smi` повторно если данные в `hardware-profile.md` свежие
  (<14 дней).
- ❌ **Не прогонять** «освежающий» probe с фиксацией результата в чате — данные уже в файле, **обнови файл**
  refresh-командой, не дублируй в чат.
- ❌ **Не игнорировать** `hardware-profile.md` при формировании hypothesis о hardware-specific perf — почти все
  Stage 2.x/3.x/5.x имеют hardware dependency.
- ❌ **Не использовать** probe для «проверки актуальности» файла — если файл существует и дата в шапке <14 дней,
  он актуален по определению (refresh = ручная операция оператора / агента при реальном апгрейде).
- ❌ **Не «проверять на всякий случай»** — STOP-блок в начале файла запрещает это явно.

**Пример cross-ref в README эксперимента:**

```markdown
**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../hardware-profile.md) §3 (RTX 3060 Ti, 8 GiB
VRAM) + §4 (`VK_EXT_mesh_shader` rev 1).
```

**Edge cases (когда probe РАЗРЕШЁН):**

- ✅ Файл `hardware-profile.md` отсутствует → создать через refresh-команду, **это и есть первый probe**.
- ✅ Дата в шапке ≥14 дней назад → обновить через refresh-команду (явная команда агента, не ритуал).
- ✅ Оператор явно попросил "проверь, не поменялся ли CPU" → probe + обновление файла.
- ✅ Нужны данные, которых в файле нет (например, конкретный sub-test VRAM budget под нагрузкой) → probe +
  дополнить файл новой секцией.
- ✅ Multi-GPU setup / новый extension / новая toolchain → дополнить файл, не дублировать.

---

## 15. Cross-refs (не дублировать)

- Корневой `AGENTS.md` §5.3 — обязательность web search (применяется и здесь).
- Корневой `AGENTS.md §5.4` — git safety (у меня упрощённо: `git *` запрещён безусловно).
- Корневой `AGENTS.md §5.5` — scope discipline (аналогично).
- Корневой `AGENTS.md §5.8` — subagent delegation (≤5 параллельных read-only процессов; я тоже следую).
- `legacy/docs/philosophy/01_foundation/03_decision-making.md` — дизайн-эвристики (data → algo → code).
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — «if perf gain < 5–10%, choose simple» — применимо и
  к моим рекомендациям.

---

ВНИМАНИЕ!!! ВАЖНАЯ ИНФОРМАЦИЯ!!!
Из-за ограничения максимального количества токенов на выходе, следует при написании или редактировании тексте более
200 строк делить операцию на несколько; полный путь к этой папке: /home/le1t/Projects/ProjectV/docs/experiments, не ~
/ProjectV, не /home/le1t/ProjectV !!!