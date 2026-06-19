# AGENTS.md

Стабильный протокол работы AI-агента в репозитории `ProjectV`. Содержит **только инварианты**;
tactical rules (build / smoke / tracy / warning cleanup) живут в `agent/decisions.md §4`,
stack conventions — в `agent/memory.md §10-§11`.

---

## 1. Назначение и правило изменений

1. Файл стабильный, но **не** неизменяемый — история в git.
2. Правка — **только по явной команде оператора** в текущей сессии. Правка кода не даёт права править AGENTS.md.
3. Правка = обычный commit (auto для non-fix; см. §6.1); draft-approval loop не требуется.

---

## 2. Project metadata

- **Name:** `ProjectV`.
- **Target:** reproducible interactive voxel MVP.
- **Stack:** C++26, Vulkan 1.4, Data-Oriented Design (SoA по умолчанию).
- **Presets:**
  - `windows-clang-debug` (dev, `clang-cl.exe` + MSVC STL)
  - `linux-clang-debug` (dev, native `clang` 22 + `lld` 22 + libc++)
  - `linux-clang-release` / `windows-clang-release` (conservative per `decisions.md §4`)
- **Near-term emphasis:** demo-scene / look-dev foundation + targeted walk/controller feel, **не** gameplay-loop.
- **Priority:** сохранение рабочего контекста важнее скорости.

---

## 3. Sources of truth (от высшего к низшему)

1. **Код** (`.cpp`, `.hpp`, `.ixx`, шейдеры, тесты) — абсолютный приоритет при оценке реальности.
2. **`TODO.md`** — roadmap, активные приоритеты, риски, чекбоксы.
3. **`AGENTS.md`** — этот файл, протокол.
4. **`agent/memory.md`** — долговечные repo-specific факты, лимиты, run-time observations, stack conventions (§10-§11).
5. **`agent/status.md`** — короткий снимок текущего состояния.
6. **`agent/decisions.md`** — зафиксированные инженерные/архитектурные договорённости.
7. **`agent/session-checklist.md`** — чеклист старта/завершения.
8. **`docs/VulkanSDK-Linux-Docs-1.4.350.1/`** — вендорная документация Vulkan 1.4 (читать **до** rg/grep'а headers / `vulkaninfo`).
9. **`legacy/docs/{philosophy,standards,libraries,architecture}/`** — обязательное чтение перед спорным инженерным выбором. Единый корень, без параллельных `latest` / `old` деревьев.

`README.md` — **не** источник истины для архитектурных решений; канонический root overview — `README_NEW.md`. Не править `README.md` без явной просьбы.

---

## 4. Классификация информации

| Что | Куда |
|---|---|
| Roadmap, приоритеты, риски, чекбоксы | `TODO.md` |
| Глобальные правила автоматизации | `AGENTS.md` |
| Долговечные технические факты, лимиты, run-time observations, stack conventions | `agent/memory.md` |
| Снимок текущего состояния | `agent/status.md` |
| Инженерные/архитектурные договорённости | `agent/decisions.md` |
| Чеклист старта/завершения | `agent/session-checklist.md` |

**Запрещено:** дублировать roadmap из `TODO.md` в `agent/`; дублировать правила из `AGENTS.md` в `TODO.md` / `agent/` / `docs/`; создавать параллельные `latest` / `old` деревья в `legacy/docs/`; пересказывать содержимое AGENTS/TODO/agent/* в чате и считать это «документацией».

**Сокращение:** любой документ должен быть сокращён, если это возможно без потери технического смысла.

---

## 5. Старт сессии (≤1 мин)

- [ ] Прочитать `TODO.md` + `agent/memory.md` + `agent/status.md` + `agent/decisions.md`.
- [ ] `git status -uall` — **один раз**.
- [ ] `agent/active-sessions.md` — есть ли parallel work.

**Не нужно:** перечитывать всё подряд, искать §n ради ссылок, проверять dirty tree несколько раз.

---

## 6. Во время работы

### 6.1 Commit message

```
<type>(<scope>): <short imperative summary>

<1-3 строки body: что и почему, не как>
```

- **type:** `feat` / `fix` / `refactor` / `perf` / `docs` / `test` / `build` / `chore` / `revert`.
- **scope** (опц.): `shadow` / `walk` / `voxel` / `render` / `ecs` / `platform` / `agent` / `docs` / `ci` / ...
- **summary:** ≤72 символа, imperative mood, без точки.
- **body:** 1-3 строки, wrap ~72.

**Правила по типам:**
- `type=fix` → обязательное **явное подтверждение оператора** (visual verify / ctest / repro / иной domain check). Без confirm — сессия `open`, `BLOCKED: fix-confirm`.
- `type ∈ {feat, refactor, perf, docs, test, build, chore, revert}` → **auto**, без подтверждения.

**Запрещено:** `Co-authored-by:` и любые trailers в commit message. Агент — opencode, не claude/cline/cursor; попытки указать соавторство — fabrication. Аналогично `Signed-off-by:` без явного требования оператора.

**`Refs:`** — допустимо, но **не** обязательно. Если пишете — конкретные §-номера (`agent/memory.md §10.11`, `decisions.md §4`) или issue/PR#. Не превращайте commit message в список cross-refs ради ритуала.

### 6.2 Что НЕ делать как ритуал

- `git status -uall` в середине задачи (хватит 1 раза в начале сессии).
- `git diff > /tmp/before_*.patch` (только перед destructive git op).
- `ProjectVRuntimeSmoke` для не-Vulkan правок (правило в `decisions.md §4`).
- 80-строчная секция в `agent/status.md` после каждого commit (1 строка или 0; детали — в `agent/active-sessions.md`, contracts — в `agent/decisions.md`).
- Поиск правильных §n для `Refs:` в non-fix commit.
- Safety-net patch в `/tmp/` после каждого commit (только при destructive op).
- `git add -A` (всегда `git add <path>` явно).
- 5-документный sync-pass после каждого коммита. Обновлять **только** то, что реально изменилось.
- Auto-close сессии после каждого коммита (см. §7).

### 6.3 Web search (Exa) — обязателен для сложных тем

Агент **не должен** пытаться решить сложную тему только из своей памяти или `agent/memory.md`. Использовать `web_search` (Exa / `https://search.exa.ai` / fallbacks из `agent/memory.md §9`) для:

- **Свежие API-изменения библиотек** (Vulkan, Jolt, flecs, fmt, tracy, volk, VMA) — **до** погружения в `external/` headers. Особенно при bump submodule.
- **C++26 фичи** (P-numbers, stdlib status, ABI нюансы, compiler support matrix) — **до** заявлений «это работает / не работает в Clang 22 / MSVC».
- **Vendor-specific behavior** (NVIDIA / AMD / Intel GPU, драйвера, validation layers, `VK_LAYER_*` поведение).
- **Best practices** для новой подсистемы (mesh shaders, SVO, render graph, JPH API drift, ECS patterns).
- **External tools / API** (`gh` CLI, GitHub API, vulkaninfo, Tracy UI binding).

**Когда НЕ искать:** тривиальные задачи (rename, refactor в очевидный паттерн, edit уже знакомого файла). Конкретный код в `src/` / `tests/` / `external/` — `rg` / `read` быстрее web search.

**Правило: первая мысль «наверное» / «должно быть» / «помню, что» на сложную тему → web search сначала, потом код.** Не мучиться, пытаясь решить по памяти.

### 6.4 Git safety

**Требуют operator confirm** (не auto):
- `git rebase` (любая форма, в т.ч. `pull --rebase`).
- `git reset --hard` для отката к прошлому состоянию.
- `git revert` существующего коммита, не упомянутого в задаче.
- `git push` / `git push --force` в любой remote.
- Удаление веток: `git branch -D`, `git push origin --delete`.
- Network publish: `gh pr create`, `gh issue create`, `gh release create`, `npm/cargo/pypi publish`, email, постинг в issue tracker.
- `sudo`, `chmod -R 777`, `mkfs`, `dd of=/dev/...`, `> /etc/...`, запись в `/boot` / `/usr` / `/var` за пределами проекта и `$HOME/le1t`.
- `rm -rf` для путей, не прочитанных/проверенных в этой же сессии.

**Safety-net workflow перед destructive на грязном дереве** (если в дереве есть чужие/предыдущие uncommitted изменения, а нужен destructive op) — **обязателен** (инцидент `2026-06-10` — P0.2 LINEAR fix был потерян через `git checkout -- .` + `git stash drop`):
1. `git status -uall` — зафиксировать, что есть.
2. `git diff > /tmp/before_<op>_<ts>.patch` — **всегда** сохранять полный diff.
3. `git diff --stat` — показать оператору, что именно потеряется.
4. Дождаться подтверждения, **потом** destructive.

**Сравнение с HEAD без деструктивного удаления:** `git diff HEAD -- <path>`, `git show HEAD:<path>`, `git diff HEAD -- <path> > /tmp/head_orig.patch` — затем вручную отобрать, что мержить.

### 6.5 Multi-agent coordination

Над проектом может работать **более одного агента одновременно** — это нормально. Параллельно выданные subtasks **должны иметь непересекающиеся scope** (разные файлы / слои / presets). **Scope discipline:** не коммитить файлы вне своего `files-touched-intent` — `git add <path>` явно, не `git add -A`.

**Что может пойти не так:**
1. Два агента пишут в один файл — race на уровне ФС.
2. Разные файлы, конфликтующие решения — merge conflict.
3. Destructive-операция поверх uncommitted work другого (см. §6.4).

**При конфликте (merge conflict / overwrite уже случился):**
1. Не паниковать. Сначала `git status`, `git diff`, `git log -p` для обеих веток.
2. Определить владельца по `agent/active-sessions.md` (timestamp + scope) — **не** для обвинений, для понимания намерений.
3. Manual merge с оператором. Автоматический merge агентом **не** делается.
4. После merge — запись в `agent/decisions.md` о конфликте и резолюции.

**Если при старте новой сессии выясняется, что часть предыдущей работы не попала в HEAD** — спросить оператора, **не выдумывать** narrative про «другой агент откатил работу». Сначала проверить `git reflog` / `git fsck` / `/tmp/before_*.patch` (см. `agent/memory.md §10.11`).

**Файлы-хабы**, которых следует избегать при параллельной работе без явной договорённости: `TODO.md`, `AGENTS.md` (см. §1), shader headers с shared structs (`SceneLightingBuffer`, `GraphicsPushConstants`), корневой `CMakeLists.txt`.

**Координация через `agent/active-sessions.md`** — append-only ledger. При старте дописать `(timestamp, scope, files-touched-intent)`; при завершении — закрыть запись (`status: closed` + commit-hash).

### 6.6 Shared `agent/` файлы

Файлы в `agent/` (кроме самого `AGENTS.md` — он подчиняется §1) — **shared infrastructure**. Чужие uncommitted изменения в `agent/*` — **нормально**, не стирай их.

| Файл | Назначение | Правило |
|---|---|---|
| `agent/active-sessions.md` | Ledger активных/закрытых сессий | Edit **только своей** записи; чужие — read-only. |
| `agent/status.md` | Snapshot | APPEND новой секции (`§N`) или UPDATE **своей**. Не стирай чужую. |
| `agent/memory.md` | Долговечные факты | APPEND нового `§N`; не переписывай чужие retroactively. |
| `agent/decisions.md` | Договорённости | APPEND нового `§N`; старое — immutable, новое может `supersede:` старое. |
| `agent/session-checklist.md` | Чеклист | Read-only contract; менять только при изменении протокола. |

**Что НЕ делать:**
- Перетирать чужие uncommitted в `agent/*` под предлогом «освежить».
- `git checkout -- agent/status.md` чтобы «откатить свой detour», если в файле чужие правки.
- Claim'ить `agent/*` целиком как «свой scope».
- Удалять safety-net patch'и других сессий из `/tmp/`.

### 6.7 Code quality

Fix, don't silence. Suppression (в коде, IDE-конфиге, CMake, `.clangd`) запрещён как способ «починить» проблему. Если DFA/IDE false-positive — точечно и только нужную строчку.

При спорных инженерных выборах приоритет отдаётся принципам из `legacy/docs/philosophy/`.

### 6.8 Subagent delegation (≤5 параллельных read-only)

**Можно:** grep/regex по корпусу, explore по 3+ связанным файлам (поиск определений, паттернов, конвенций), сводка по `legacy/docs/` для конкретной темы, параллельный анализ нескольких build trees.

**Нельзя:**
- Любые модификации (`write_to_file` / `replace_in_file` / `execute_command` с `requires_approval: true`).
- Destructive git.
- Секреты (`~/.hermes/profiles/projectv/.env`).
- Решения, требующие `agent/decisions.md` / `agent/memory.md`.
- **Фундаментальные/монументальные работы** — где нужно много знать в одном контексте, чтобы видеть скрытые нюансы (subagent теряет общую картину при разбиении задачи).

**Валидация:** каждый вывод subagent проверяется точечным `read_file` / `rg` в основном агенте перед использованием. Subagent может ошибиться или применить устаревший rule.

### 6.9 Pre-commit gate

Перед `git commit` проверить: правила §6.1 (commit message + type-dependent), §6.4 (destructive), §6.5 (scope discipline).

При непрохождении gate — commit не выполняется, сессия `open`, `notes: BLOCKED: <gate>`.

---

## 7. Конец сессии

Сессия = одна логическая работа, может включать несколько коммитов. **Auto-close после каждого коммита запрещён** — 3 минуты overhead на открытие сессии + 3 минуты на закрытие = 6 минут на каждый checkpoint.

**После каждого коммита в сессии:**
- (Опц.) `agent/active-sessions.md` — обновить `notes` если есть прогресс по scope.
- (Опц.) `agent/status.md` — 1 строка если milestone.

**В конце работы** (после финального коммита): агент пишет «**Что дальше?**» и ждёт оператора. Оператор решает:
- «закрыть» → сессия `status: closed` в `agent/active-sessions.md`, перенести в «Закрытые сессии» + `commit-hash` финального коммита.
- дать следующую задачу → та же сессия продолжается, или новая (если scope меняется).

Safety-net patch в `/tmp/before_*_<ts>.patch` — оставлять **только** если в этой сессии был destructive op на грязном дереве (правило в §6.4).

**Edge cases (сессия остаётся `open`):**
- `git commit` fail / hook reject → лог ошибки в `notes` активной сессии, retry.
- Pre-commit gate не прошёл → `notes: BLOCKED: <gate>`.
- Build не зелёный → `notes: BLOCKED: build`.

**Manual abort (без «Что дальше?»):** `status: aborted` + причина в `notes`, запись остаётся в «Активные сессии».

---

## 8. Definition of done

- [ ] Build green на охватываемой платформе (если менялся код/CMake/build config; для docs-only — не нужен).
- [ ] Pre-commit gate (правила §6.1, §6.4, §6.5) пройден.
- [ ] `agent/status.md` отражает фактическое состояние (1 строка post-commit).
- [ ] Если сессия включала destructive на грязном дереве — `/tmp/*.patch` сохранён, destructive подтверждена оператором (правило в §6.4).
