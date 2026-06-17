# AGENTS.md

Стабильный протокол работы AI-агента в репозитории `ProjectV`. Документ **может быть изменён только по явной команде
пользователя** в текущей сессии; обычные правки кода этого не требуют.

---

## 1. Мета — как менять этот документ

1. Этот файл — **стабильный**, а не «неизменяемый»: история правок ведётся в git.
2. Изменить его можно **только по явной команде пользователя** в текущей сессии. Правка кода, даже если она
   противоречит текущему AGENTS.md, **не даёт** права автоматически править AGENTS.md.
3. Перед изменением — показать diff-черновик в чате (что добавится / что удалится / что изменится) и применить сразу.
   Commit идёт как обычный код (auto по §8.1, pre-commit gates §7.3.1). Явная команда пользователя в текущей сессии
   остаётся обязательным триггером — draft-approval loop больше не требуется.
4. Любая правка AGENTS.md — это **обычный коммит** с commit message по контракту §7.2.5.
5. Не дублировать содержимое AGENTS.md в `agent/`, `docs/`, `TODO.md` — см. §6 (anti-duplication).

---

## 2. Project metadata

- **Name:** `ProjectV`.
- **Target:** reproducible interactive voxel MVP.
- **Stack:** C++26, Vulkan 1.4, Data-Oriented Design (SoA по умолчанию).
- **Platforms:** `windows-clang-debug` (`clang-cl.exe`, основной dev tree), `linux-clang-debug` (native `clang` 22 +
  `lld` 22 + libstdc++ 16, baseline-initialized 2026-06-09). Windows и Linux dev trees сосуществуют, presets
  изолированы.
- **Priority:** сохранение рабочего контекста важнее скорости. Сессионная гигиена и явная фиксация решений —
  обязательны.
- **Near-term emphasis:** demo-scene / look-dev foundation + targeted walk/controller feel, не gameplay-loop.

---

## 4. Sources of truth

При конфликте приоритет (от высшего к низшему):

1. **Код** (`.cpp`, `.hpp`, `.ixx`, шейдеры, тесты) — абсолютный приоритет при оценке реальности. Если код говорит одно,
   а документ другое — документ неправ.
2. **`TODO.md`** — живой roadmap, активные приоритеты, риски, чекбоксы.
3. **`AGENTS.md`** — этот документ, протокол.
4. **`agent/memory.md`** — долговечные repo-specific факты, аппаратные и архитектурные лимиты, run-time observations.
5. **`agent/status.md`** — короткий снимок текущего состояния.
6. **`agent/decisions.md`** — зафиксированные инженерные и архитектурные договорённости.
7. **`agent/session-checklist.md`** — обязательный чеклист старта/завершения сессии.
8. **`docs/VulkanSDK-Linux-Docs-1.4.350.1/`** — вендорная документация Vulkan 1.4 (правило: **читать ДО rg/grep'а
   headers / `vulkaninfo`** — это уже стоило часа в `agent/memory.md §10.7`).
9. **`legacy/docs/philosophy/`** — принципы (обязательное чтение перед спорным инженерным выбором).
10. **`legacy/docs/standards/`** — конкретные правила (`cmake/`, `cpp/`, `git/`).
11. **`legacy/docs/libraries/`** — per-library reference (SDL, Jolt, volk, VMA, tracy, flecs, fmt, glm, fastgltf, draco,
    RmlUi, freetype, zstd, glaze, meshoptimizer, miniaudio, stdexec, imgui).
12. **`legacy/docs/architecture/`** — текущий дизайн, ADRs, спекулятивный `future/`.
13. **`legacy/docs/{guides,tutorials,examples}/`** — обучающие материалы.
14. **`legacy/docs/archive/roadmaps/`** — исторические планы. **Не источник истины**; использовать только для понимания
    «почему раньше решили иначе».

`legacy/docs` — единый унифицированный корень; параллельных `latest` / `old` деревьев не поддерживается. Канонические
точки входа в `legacy/docs/libraries/<lib>/` — `01_reference.md` + `02_integration.md`, остальной корпус — глубже.

`README.md` — **не** источник истины для архитектурных решений; корень для пользовательского overview — `README_NEW.md`.
Не править `README.md` без явной просьбы.

---

## 6. Anti-duplication / classification

Tactical rules (verification policy, smoke policy, tracy build policy, warning-cleanup policy) живут в
`decisions.md §4` (Build / verification contract). AGENTS.md хранит **только инварианты**, не повторяющие
`decisions.md`, `TODO.md` или `agent/*`. Если возникает сомнение «писать ли это в AGENTS.md?» — это сигнал, что писать
надо в `decisions.md` или в один из `agent/*`, а здесь оставить ссылку.

Перед записью новой информации — **классифицировать** её по матрице:

| Что | Куда |
|-------------------------------------------------------------- --| ------------------------------|
| Roadmap, приоритеты, риски, чекбоксы | `TODO.md`                    |
| Глобальные правила автоматизации | `AGENTS.md`                  |
| Долговечные технические факты, аппаратные/архитектурные лимиты | `agent/memory.md`            |
| Снимок текущего состояния сессии | `agent/status.md`            |
| Инженерные/архитектурные договорённости | `agent/decisions.md`         |
| Чеклист старта/завершения сессии | `agent/session-checklist.md` |

**Запрещено:**

- Дублировать roadmap из `TODO.md` в `agent/`.
- Дублировать системные правила из `AGENTS.md` в `TODO.md`, `agent/`, `docs/`.
- Создавать параллельные `latest` / `old` деревья в `legacy/docs/`.
- Пересказывать содержимое `AGENTS.md` / `TODO.md` / `agent/*` в чате и считать это «документацией».

**Сокращение:** любой документ должен быть сокращён, если это возможно без потери технического смысла.

---

## 7. Operational pipeline

### 7.1 Initialization (старт сессии)

Обязательный порядок — в `agent/session-checklist.md` (секция «Старт сессии»). Кратко: прочитать `TODO.md` +
`agent/memory.md` + `agent/status.md`; для задач по рендеру/памяти/оптимизации/структурам/workflow — также
`agent/decisions.md`; проверить `git status -uall`; проверить `agent/active-sessions.md` на parallel work;
классифицировать `[mainline | extension | R&D]`.

### 7.2 Execution (выполнение)

**Общие правила:**

- Запрещено хранить технические решения **только** в переписке (диалоге).
- Новые риски и развилки фиксируются в `TODO.md` **немедленно** по ходу обнаружения.
- Параллельный запуск нескольких `build` / `test` / `smoke` команд в одном build tree **ЗАПРЕЩЕН**. Только
  последовательно.
- При спорных инженерных выборах приоритет отдаётся принципам из `legacy/docs/philosophy/`.

#### 7.2.1 Subagent delegation policy

`use_subagents` — до 5 параллельных read-only агентов. Использовать:

- **Можно** делегировать:
    - Массовую разведку по 3+ связанным файлам (поиск определений, паттернов использования, конвенций).
    - Grep/regex-задачи по большому корпусу.
    - Сводку по `legacy/docs/`, `docs/VulkanSDK-Linux-Docs-...` для конкретной темы.
    - Параллельный анализ нескольких build trees.
- **Нельзя** делегировать:
    - Любые модификации (`write_to_file`, `replace_in_file`, `execute_command` с `requires_approval: true`).
    - Destructive git-операции.
    - Решения, требующие знания `agent/decisions.md` или `agent/memory.md` (subagent может не иметь доступа к ним или
      неправильно интерпретировать).
    - Операции с `~/.hermes/profiles/projectv/.env` или любыми секретами.
- **Валидация** результата subagent: каждый вывод subagent проверяется точечным `read_file` / `rg` в основном агенте
  перед тем, как опираться на него. Subagent может ошибиться, цитировать устаревший код, или применить устаревший
  конвенционный rule.

#### 7.2.2 Safety protocol (общий, не только git)

**Категорически запрещено без явного подтверждения пользователя в текущей сессии:**

- Деструктивные git-команды (см. §7.2.4).
- Сетевые публикации: `git push` в любой remote, `gh pr create`, `gh issue create`, `gh release create`, `npm publish`,
  `cargo publish`, `pypi upload`, отправка email, постинг в issue tracker.
- `chmod -R 777 /` и аналоги, изменение прав на системные пути.
- `mkfs`, `dd of=/dev/...`, `> /etc/...`, запись в `/boot`, `/usr`, `/var` — за пределами проекта и `$HOME/le1t`.
- `rm -rf` для путей, которые не были прочитаны/проверены в этой же сессии.
- `sudo` (в этой песочнице нет TTY, агент не может ввести пароль).

**Перед любой операцией из списка выше** — спросить пользователя текстом и дождаться подтверждения.

#### 7.2.4 Git safety protocol

**Категорически запрещено** без явного подтверждения пользователя:

- `git rebase` (любой формы, в т.ч. `pull --rebase`).
- `git reset --hard` для отката к **прошлому** состоянию (для текущего unstaged — см. ниже).
- `git revert` существующего коммита, не упомянутого в задаче.
- `git push --force` в любой remote.
- `git commit` — см. §7.3.1 / §8.1 (auto при выполнении pre-commit gates; `type=fix` требует operator confirm).
- `git push` без явного подтверждения.
- Удаление веток: `git branch -D`, `git push origin --delete`.

**Uncommitted work (грязное дерево) — критический инцидент 2026-06-10:**

- Случай: дерево грязное, коммит нельзя (работа не завершена), а нужен destructive git-action.
- **Запрещено:** `git checkout -- .` + `git stash drop` как «откатить неудачный detour» — это **уничтожает**
  uncommitted-but-working изменения из предыдущих сессий (так был потерян P0.2 LINEAR fix, см.
  `agent/memory.md §10.11`).
- **Разрешённый workflow** перед destructive операцией на грязном дереве:
    1. `git status -uall` — зафиксировать, что есть.
    2. `git diff > /tmp/before_drop_<timestamp>.patch` — **всегда** сохранять полный diff.
    3. Если конкретные файлы надо сохранить — `cp <file> /tmp/<file>.keep` **или**
       `git stash push -m "KEEP_<описание>" -- <path>`.
    4. **Показать пользователю** что именно будет потеряно (`git diff --stat`, размер patch'а) и дождаться
       подтверждения.
    5. Только после подтверждения — destructive операция.
- Сравнение с HEAD без деструктивного удаления: `git diff HEAD -- <path>`, `git show HEAD:<path>`,
  `git diff HEAD -- <path> > /tmp/head_orig.patch` — затем вручную отобрать, что мержить.
- (Удалено: единый auto-commit gate перенесён в §7.3.1.)

#### 7.2.5 Commit message contract

Формат:

```
<type>(<scope>): <short imperative summary>

<longer body explaining the what and why, not the how>

Refs: <AGENTS.md §n, agent/memory.md §n, issue/PR number, etc.>

Co-authored-by: если применимо
```

- **type**: `feat`, `fix`, `refactor`, `perf`, `docs`, `test`, `build`, `chore`, `revert` (по conventional commits, но *
  *без слеша в scope** — корпоративный стандарт не требует).
- **scope** (опционально): `shadow`, `walk`, `voxel`, `render`, `ecs`, `platform`, `agent`, `docs`, `ci`, etc.
- **summary**: до ~72 символов, imperative mood ("add", "fix", "remove"), без точки в конце.
- **body**: 1–3 строки, объясняет **что** и **почему**, не **как**. Wrap ~72 символа.
- **Refs**: ссылки на конкретные секции документов, где зафиксирована договорённость или регрессия.

Пример:

```
fix(shadow): use LINEAR magFilter for shadow sampler to enable hardware 2x2 PCF

Vulkan spec 1.4 §20.2.4: LINEAR filtering on depth-shadow samplers produces
hardware 2x2 PCF, removing discrete 0/1 bands on cascade seams. NEAREST
forced per-pixel comparisons and made cascade transitions look like
stair-steps.

Refs: agent/memory.md §10.11
```

**Auto-execute:** commit выполняется автоматически при выполнении pre-commit gates (§7.3.1); `type=fix` требует
operator confirm (что фикс работает — visual / ctest / repro). §7.2.5 contract остаётся обязательным.

#### 7.2.6 Multi-agent concurrent work policy

Над проектом может работать **более одного агента одновременно**: несколько параллельных сессий, разработчик +
агент, агент + CI-бот. Это **нормальный** сценарий, а не исключение. Параллельно выданные подзадачи **должны иметь
непересекающиеся scope** (разные файлы / слои / presets).

**Что может пойти не так (conflict scenarios):**

1. **Два агента пишут в один файл** — race на уровне ФС. Типичный случай: оба правят `TODO.md` или `agent/status.md` в
   конце сессии.
2. **Разные файлы, конфликтующие решения** — один переименовывает класс, другой в это время расширяет вызовы. Merge
   conflict в git.
3. **Destructive-операция поверх uncommitted work другого** — сценарий из §7.2.4: агент A делает `git checkout -- .`
   чтобы «откатить detour», не зная, что агент B держит в дереве недокоммиченный прогресс. Документация инцидента —
   `agent/memory.md §10.11`.

**Протоколы снижения рисков:**

- **Перед началом работы** (§7.1 шаг 5) — посмотреть свежесть `agent/status.md` и `git status -uall`. Если в дереве
  чужие uncommitted изменения, **не относящиеся к вашей подзадаче** — оставить их нетронутыми. Не делать `git add -A`,
  не делать `git checkout -- <file>` для файлов вне scope.
- **Scope discipline.** Если выданная подзадача требует править файл, который, по вашим данным, уже правит другой
  агент (общий `TODO.md`, shared shader struct из `agent/memory.md §10.8`, корневой `CMakeLists.txt`) — **сообщить
  пользователю** и попросить serialization: дождаться завершения другой сессии или явно поделить scope.
- **Координация через `agent/active-sessions.md`.** Append-only ledger активных сессий. При старте агент дописывает
  `(timestamp, scope, files-touched-intent)`; при завершении — закрывает запись (статус `closed` + commit hash). Файл —
  primary signal для arbitration при конфликте scope.
- **Файлы-хабы (high-contention)**, которых следует избегать при параллельной работе без явной договорённости:
  `TODO.md`, `AGENTS.md` (см. §1), shader headers с shared structs (`SceneLightingBuffer`, `GraphicsPushConstants`),
  корневой `CMakeLists.txt`. Файлы `agent/*` (кроме самого `AGENTS.md`) — **не** хабы, а shared infra
  (см. §7.2.8): конкурентный edit разрешён, не claim'ить эксклюзивно.
- **При завершении сессии** (§8.1) — **обязательно** обновить `agent/active-sessions.md` (закрыть свою запись) **сразу
  после** commit. Иначе другой агент не увидит, что scope освободился.

**Conflict resolution (merge conflict / overwrite уже случился):**

1. **Не паниковать.** Сначала `git status`, `git diff`, `git log -p` для обеих веток.
2. **Определить владельца** по `agent/active-sessions.md` (timestamp + scope) — это **не** для обвинений, а для
   понимания намерений проигравшей стороны diff'а.
3. **Manual merge с пользователем.** Автоматический merge агентом не делается — решения о приоритетах scope'ов принимает
   человек.
4. **После merge** — запись в `agent/decisions.md` о конфликте и резолюции, чтобы следующая сессия видела «почему так».

**Что НЕ делать:**

- `git pull --rebase` (см. §7.2.4).
- `git checkout -- <file>` для файлов вне своей подзадачи.
- Тихо перезаписывать чужие uncommitted изменения, даже если они «кажутся мусором» — это мог быть прогресс, ещё не
  дошедший до commit.
- Плодить новые файлы-ledger'ы — использовать `agent/active-sessions.md`.

#### 7.2.6.1 Atomic subtask

Атомарная подзадача — единица работы агента. Границы:

- Один commit (один §7.2.5 commit message), один `files-touched-intent` в `agent/active-sessions.md`, один
  измеримый outcome (build green, ctest baseline, или visual verify). Сессия решает строго одну подзадачу.
- Параллельно выданные подзадачи имеют **непересекающиеся scope** — разные файлы / слои / presets. Файлы-хабы
  (`TODO.md`, `AGENTS.md`, shared shader structs, корневой `CMakeLists.txt`) — arbitration через пользователя,
  см. §7.2.6 «Scope discipline».
- Несколько связанных правок в одном логическом refactor'е — один commit, не несколько. Revert на «первую
  половину» не нужен: либо катится весь refactor, либо ни один из его шагов.

#### 7.2.7 Code quality: fix, don't silence

**Запрещено глушить ошибки и варнинги.** Любые формы suppression
— в коде, в IDE-конфиге, в CMake, в `.clangd` — запрещены как
способ «починить» проблему. Если проблема реална — чинить код.
Если это DFA/IDE false-positive, можно заглушить, но только точечно и только нужную строчку.

#### 7.2.8 Shared `agent/` files

Файлы в `agent/` (за исключением самого `AGENTS.md` — он подчиняется §1) — это **общая
инфраструктура**, а не «claimed scope» одной сессии. Любая активная сессия **может** писать в них
по ходу работы, **не дожидаясь** завершения других сессий и **не арбитрируя scope** через
пользователя.

| Файл                         | Назначение                                         | Правило конкурентного edit                                                                      |
|------------------------------|----------------------------------------------------|-------------------------------------------------------------------------------------------------|
| `agent/active-sessions.md`   | Ledger активных/закрытых сессий                    | Edit **только своей** записи; чужие записи — read-only. См. header файла.                       |
| `agent/status.md`            | Snapshot текущего состояния                        | APPEND новая секция (следующий номер `§N`) или UPDATE **своей** секции. Не стирай чужую секцию. |
| `agent/memory.md`            | Долговечные факты / лимиты / run-time observations | APPEND новый `§N`; не переписывай чужие секции retroactively.                                   |
| `agent/decisions.md`         | Архитектурные договорённости                       | APPEND новый `§N`; старое решение — immutable, новое может `supersede:` старое (явная ссылка).  |
| `agent/session-checklist.md` | Чеклист старта/завершения                          | Read-only contract; менять только при изменении протокола.                                      |
| `AGENTS.md`                  | Stable protocol doc                                | По §1 — отдельный contract, не shared.                                                          |

**Главное правило:** если у тебя в `git status -uall` уже есть чужие uncommitted изменения
в `agent/status.md` или `agent/memory.md` — **это нормально**. Ты просто пишешь **в свою** секцию,
не делаешь `git checkout -- <file>` и не перетираешь чужие правки. Если видишь, что твоя
секция уже частично занята (другая активная сессия тоже обновляет) — перейди в `notes` и явно
скоррелируй, а не делай `git pull`/`merge` через агента.

**Что НЕ делать:**

- Перетирать чужие uncommitted изменения в `agent/*` под предлогом «освежить» или
  «привести в порядок» — это мог быть прогресс, ещё не дошедший до commit (см. `agent/memory.md
  §10.11`).
- Делать `git checkout -- agent/status.md` чтобы «откатить свой detour», если в файле есть
  чужие правки.
- Claim'ить `agent/status.md` целиком как «свой scope» — это shared инфраструктура.
- Удалять safety-net patch'и других сессий из `/tmp/`.

**Связь с §7.2.6:** файлы-хабы, которых **следует избегать** при параллельной работе без явной
договорённости — `TODO.md`, `AGENTS.md` (см. §1), shader headers с shared structs
(`SceneLightingBuffer`, `GraphicsPushConstants`), корневой `CMakeLists.txt`. `agent/*` (кроме
самого `AGENTS.md`) — **не** hub, а shared infra.

### 7.3 Verification (закрытие подзадачи)

Tactical verification rules (build/test policy, smoke policy, tracy build policy, warning-cleanup policy) живут в
`decisions.md §4` (Build / verification contract) и `agent/session-checklist.md` (старт/завершение). Здесь фиксируем
только **формальный инвариант**: build green на охватываемой платформе; `ctest` / scripted captures /
`ProjectVRuntimeSmoke` применять по решению, принятому в `decisions.md §4`, а не как ритуал на каждое закрытие.

**Visual / render tasks (дополнительно, не ритуал):**

- Инспектировать **финальный кадр** и релевантные debug-view frames (`SHDW`, `CSM`, `CTSH`, `AOCC`, `LOCL`, `AMB`).
  Только sidecar metadata — недостаточно (см. `agent/memory.md §1` про contact-shadow landing).
- Captures под `build/<preset>/lookdev-captures/<name>/`.

**Static analysis:**

- Checked-in `Problems/*.xml` — это hint, не source of truth. Перед warning cleanup — регенерировать `Problems/`. Строки
  в XML устаревают за один refactor pass.

#### 7.3.1 Pre-commit gate

Перед `git commit` (auto-flow по §8.1) агент обязан проверить:

1. **§7.2.5 message готов** — type, scope (опц.), short summary, body, Refs.
2. **Scope discipline** — `git status -uall` не содержит чужих uncommitted файлов вне моего
   `files-touched-intent` (см. §7.2.6, §7.2.8). При наличии — arbitration через оператора или
   сессия остаётся `open` с `notes: BLOCKED: scope-collision`.
3. **Type-dependent gate:**
    - `type = fix` — обязательное **явное подтверждение оператора** что фикс работает
      (visual verify, ctest-сценарий, repro, или иной domain check). Без confirm — сессия
      `open`, `notes: BLOCKED: fix-confirm`. **Причина:** agent склонен коммитить фиксы,
      которые не проверены в продакшен-условиях (visual / repro).
    - все прочие type (`feat`, `refactor`, `perf`, `docs`, `test`, `build`, `chore`, `revert`)
      — auto.
4. **Destructive операции** (rebase, push, force-push, reset --hard, revert, branch delete, network
   publish, sudo, rm -rf unverified, и т.д.) — **всегда** требуют operator confirm, не auto.
   См. §7.2.2 / §7.2.4. Pre-commit gate про auto-commit, не про эти операции.

При непрохождении gate — commit не выполняется, сессия `open`, в `notes` фиксируется какой gate
заблокировал. Edge cases (commit fail / hook reject / параллельный агент / build не зелёный) — см. §8.1.

### 7.4 Synchronization (sync с документами)

После изменения кода:

1. Обновить чекбоксы и риски в `TODO.md`.
2. Актуализировать `agent/status.md` (snapshot).
3. Если выявлен перманентный лимит / свойство движка — `agent/memory.md`.
4. Если изменилось архитектурное соглашение — `agent/decisions.md` (новый инкремент).
5. Если задача затрагивает `AGENTS.md` — сначала согласование с пользователем (см. §1).

---

## 8. Session-end protocol

Перед принудительным перезапуском сессии: см. `agent/session-checklist.md` →
секция «Post-commit close-routine». Этот документ фиксирует только два **обязательных инварианта**:

1. **Код либо чистый, либо uncommitted work сохранён** в `/tmp/*.patch` или `git stash` с
   описательным именем (см. §7.2.4).
2. **Commit выполнен автоматически** при выполнении pre-commit gates (§7.3.1); §7.2.5 contract
   применяется. Сразу после успешного commit запускается close-routine (см. §8.1).

Всё остальное (TODO.md, status.md, memory.md, decisions.md) обновляется по необходимости
из `session-checklist.md`, а не как ритуал.

### 8.1. Auto-close после commit

Сессия **по умолчанию** закрывается автоматически сразу после успешного commit. Manual hold-open
возможен только при выполнении одного из keep-open критериев (см. ниже).

**Close-routine (5 шагов, выполняется последовательно):**

1. `git rev-parse HEAD` → сохранить SHA в `commit-hash`.
2. `agent/active-sessions.md`: `status: open → closed`, проставить `closed-at` (ISO 8601), `commit-hash`,
   перенести запись из «Активные сессии» в «Закрытые сессии» (append-only ledger, см. header файла).
3. `agent/status.md` — обновить snapshot (§7.4).
4. `agent/memory.md` / `decisions.md` / `TODO.md` — по §7.4.
5. Safety-net patch в `/tmp/before_*_<ts>.patch` — **оставить**, добавить footer
   `POST-COMMIT <sha>` (теперь это fallback для следующей сессии, а не «uncommitted work»).

**Keep-open критерии** (любой из → сессия остаётся `open`):

- **Multi-commit sub-plan.** В `scope` явно прописана последовательность sub-commits
  (e.g. «Tier 0.A → 0.B → 0.C») и не все sub-commits сделаны. Marker в active-sessions:
  `multi-commit-plan: <step>/<total>`.
- **Operator next-step.** В последнем сообщении оператора есть явный next-step той же подзадачи
  («теперь сделай X», «дальше Y»). Agent продолжает.
- **`continues: <reason>` marker.** В `notes` текущей записи active-sessions явно стоит hold-open
  marker (например, для multi-day сабтасков).

При keep-open — в `notes` добавить `held-open: <criterion>` (какой из трёх применился).

**Edge cases (commit не происходит / сессия остаётся `open`):**

- Pre-commit gate (§7.3.1) не прошёл → `open` + `notes: BLOCKED: <gate>`.
- `git commit` fail / hook reject → `open`, в `notes` лог ошибки, retry после фикса.
- Параллельный агент с пересекающимся scope (см. §7.2.6) → `open`, arbitration через оператора.
- Build не зелёный → commit не выполняется, `open` + `notes: BLOCKED: build`.

**Manual abort (без commit):** `status: aborted` + причина в `notes`, запись остаётся в «Активные
сессии» с явным маркером, не переносится в «Закрытые сессии».

### 8.2. Что агент НЕ должен путать с «потерянной работой»

Multi-agent coordination — **текущий** контракт проекта (см. §7.2.6,
`agent/active-sessions.md`). Если в начале новой сессии выясняется, что часть предыдущей работы
не попала в HEAD, **это не «потеря»** — это нормальное состояние uncommitted-ветки предыдущей
сессии (одной или нескольких), и она лежит в `git status -uall` + в safety-net patch'ах
`/tmp/before_*.patch`. Списывать uncommitted правки на «другого агента» без проверки — fabrication.

Прежде чем делать такое заявление — **проверить `git reflog` / `git fsck` / `/tmp/*.patch`**,
и:

- если следы есть в `git reflog` или `/tmp/` — пометить как «незакоммиченная работа предыдущей
  сессии, не утеряна»;
- если другая сессия действительно откатила работу (видно в `git reflog` / `agent/active-sessions.md`)
  — **сообщить оператору** и попросить arbitration, не выдумывать narrative;
- если действительно никаких следов нет — пометить как «причина неясна, нужна проверка
  оператора», а не «кто-то откатил».

---

## 9. Definition of done

Определяется `decisions.md §4` (Build / verification contract) + `agent/session-checklist.md`
(секция «Завершение»). Здесь фиксируем только:

- [ ] Build green на охватываемой платформе.
- [ ] Pre-commit gate (§7.3.1) пройден: §7.2.5 message + scope discipline + (для `type=fix`) operator confirm.
- [ ] `agent/status.md` отражает фактическое состояние на момент закрытия сессии.
- [ ] Если сессия включала git-операции поверх uncommitted work — `/tmp/*.patch` сохранён,
  destructive-операция подтверждена пользователем (см. §7.2.4).

---

## 10. Stack conventions

Точные правила для стека проекта. Проверять перед модификацией соответствующих подсистем.

### 10.1 C++26 baseline

- `CMAKE_CXX_STANDARD 26` задан в `CMakeLists.txt:29`. `CMAKE_CXX_STANDARD_REQUIRED ON` в `CMakePresets.json`.
- **`target_compile_features(cxx_std_26)` не используется** — стандарт идёт глобально.
- **C++26 modules (`.ixx`) — applied in mainline since 2026-06-13** (TODO Tier 2 closure, commits `c3faa65`+
  `e0029dc`+`73e2dd7`+`be16a2d`+`5c9d658`). `src/core/Math.ixx`, `Probe.ixx`, `StringId.ixx` live; `import std;`
  probe работает в `linux-clang-debug`. CMake 3.28+ `FILE_SET CXX_MODULES` + `clang-cl 22` / `clang 22` parity
  verified. **Не** тащить `.ixx` в файлы с `#include` Vulkan/SDL/flecs/Jolt headers (оставлять в `#include` через
  `target_include_directories`).
- **Правило:** новые modules — `target_sources(... PRIVATE FILE_SET CXX_MODULES FILES ...)` + `CMAKE_CXX_SCAN_FOR_MODULES
  ON` (CMake 3.28+). CMake 4.x на mainline, проверено 3.30+.
- Header conventions: `#pragma once`; `<cstddef>` перед `<cstdint>` (libstdc++ 16 не тянет `size_t` транзитивно); явные
  `<cstring>` для `std::mem*` / `std::str*` (на MSVC транзитивно, на libstdc++ нет — но глобальный `-include cstring`
  уже стоит, см. `agent/memory.md §6`).
- Constexpr / consteval — поощряются для compile-time проверок.

### 10.2 Vulkan 1.4 / loader / memory

- **Loader:** `volk` (`external/volk/`), `VK_NO_PROTOTYPES` глобально, `VOLK_STATIC_DEFINES` — platform-gated в root
  `CMakeLists.txt:51-57` (`WIN32 → VK_USE_PLATFORM_WIN32_KHR`, `APPLE → VK_USE_PLATFORM_MACOS_MVK`,
  `ANDROID → VK_USE_PLATFORM_ANDROID_KHR`, иначе `VK_USE_PLATFORM_XCB_KHR`).
- **Memory:** `VulkanMemoryAllocator` (VMA), submodule layout: `external/VulkanMemoryAllocator/include/vk_mem_alloc.h`
  (**не** `vma/vk_mem_alloc.h` — это layout Vulkan SDK под Windows, в Linux не работает).
- **Validation layers:** `PROJECTV_ENABLE_VALIDATION=ON` в debug-presets. На Linux пакет `vulkan-validation-layers` надо
  ставить отдельно (см. `agent/memory.md §5, §8`).
- **Вопросы по Vulkan семантике** — **читать `docs/VulkanSDK-Linux-Docs-1.4.350.1/` ДО rg/grep'а headers / `vulkaninfo`
  **. Это уже стоило часов в `agent/memory.md §10.7`.
- **Структуры с shader-контрактом** (например, `VoxelSceneLighting` ↔ `SceneLightingBuffer` в шейдерах) — **порядок и
  размер полей должен совпадать байт-в-байт** во всех шейдерах и C++ (`voxel.frag`, `voxel_shadow.vert`,
  `voxel_mesh.comp`). Сдвиг в shadow-pass разрушает cascade matrices (см. `agent/memory.md §1, §10.8`).
- **Push constants** — порядок полей в C++ struct выводить из шейдера, а не из `FramePreparation.cpp` (последний следует
  struct, не наоборот). См. `agent/memory.md §10.8` (GraphicsPushConstants incident).

### 10.3 Platform / presets

- Windows presets: `windows-clang-debug`, `windows-clang-debug-ci`, `windows-clang-debug-tracy-profiler`.
  `CMAKE_CXX_COMPILER = clang-cl.exe`.
- Linux presets: `linux-clang-debug`, `linux-clang-debug-build`, `linux-clang-debug-tests`. Native clang 22,
  `CMAKE_LINKER_TYPE=LLD`, **без** MSVC-флагов.
- Build tree zoo — в `agent/memory.md §9` финал. Tracy build trees собирать **только** при изменениях Tracy-конфигурации
  или явной просьбе.
- Параллельный `build` / `test` / `smoke` в одном build tree **запрещён**.

### 10.4 Зависимости

- Submodules в `external/`. Не заменять submodule на системный пакет без согласования.
- CPM (`CPM_SOURCE_CACHE`) — в `build/cpm-source-cache`; используется для **части** third-party.
- **Jolt include contract:** `<Jolt/Jolt.h>` должен идти **раньше** других Jolt headers. Иначе `JPH_*` macros/typedefs
  ломаются, `PhysicsWorld.cpp` не компилируется.
- **Shader compile path:** `glslc` или `glslangValidator`.

### 10.5 SoA / DoD

- Новые структуры — **SoA** по умолчанию. AoS — только если есть явная причина (маленький size, hot path, BR-friendly).
- Итерация — индексная, не iterator-based, если только размер hot loop не оправдывает iterator.
- Бэст-сделанный single-TU test runner в `tests/VoxelWorldTests.cpp`: исторически содержал
  `// ReSharper disable CppDFAUnreachableFunctionCall` — это была ошибка, не прецедент. Если потребуется DFA-clean
  harness — рефакторить в сторону direct calls из `main()`, не добавлять suppressions (см. §7.2.7). Существующий
  suppression должен быть вычищен в отдельной подзадаче.