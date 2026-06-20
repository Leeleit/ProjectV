# agent/active-sessions.md

Append-only ledger активных и недавно завершённых AI-agent сессий в `ProjectV`.
Используется для координации между параллельными сессиями и для arbitration
при конфликте scope (см. `AGENTS.md` §7.2.6).

**Это НЕ источник истины** для архитектурных решений — для этого `agent/knowledge.md Part A`.
Здесь только оперативный signal «кто сейчас что трогает», чтобы параллельные
агенты не вытирали работу друг друга.

---

## Контракт использования

Каждый агент **обязан**:

1. **При старте сессии** — дописать запись со статусом `open` в секцию
   «Активные сессии» ниже.
2. **При auto-close** (после успешного `git commit` per `AGENTS.md §8.1`) — обновить
   **свою** запись: `status: open → closed`, проставить `closed-at` (ISO 8601 UTC) и
   `commit-hash` (SHA), затем перенести в секцию «Закрытые сессии».
   При manual hold-open (см. `AGENTS.md §8.1` keep-open criteria) — запись остаётся
   `open`, в `notes` добавляется `held-open: <criterion>` или `multi-commit-plan: <step>/<total>`.
3. **При abort** — пометить `aborted` + причина, не удалять запись. Safety-net patch в
   `/tmp/` оставить с `POST-COMMIT <sha>` footer (per §8.1 п.5).

См. также `AGENTS.md §9` (секции «Старт» / «Post-commit close-routine»).
Параллельный запуск нескольких сессий с **пересекающимся** scope —
аномалия, требует arbitration через пользователя (§7.2.6). Файлы `agent/*` (кроме
`AGENTS.md`) — **shared infrastructure** (§7.2.8), не claim'ить эксклюзивно.

---

## Формат записи

| Поле | Описание |
|---|---|
| `id` | Уникальный идентификатор сессии (timestamp ISO 8601 + короткий суффикс) |
| `started-at` | Время старта в ISO 8601 (UTC) |
| `agent` | Тип / модель агента (например, `MiniMax-M3`) |
| `operator` | Пользователь-оператор (например, `le1t`) |
| `branch` | Текущая git-ветка |
| `scope` | Краткое описание атомарной подзадачи (см. AGENTS.md §7.2.6.1) |
| `files-touched-intent` | Список файлов / путей, которые планируется править |
| `status` | `open` / `closed` / `aborted` |
| `closed-at` | (только для `closed`/`aborted`) Время завершения в ISO 8601 (UTC) |
| `commit-hash` | (только для `closed`) SHA коммита, закрывшего работу; или `uncommitted` |
| `notes` | Свободное примечание (конфликты, blockers, cross-refs) |
| `held-open` | (опц.) Если сессия не закрыта после успешного commit — какой keep-open criterion сработал (`multi-commit-plan` / `operator-next-step` / `continues:<reason>`) |
| `multi-commit-plan` | (опц.) `<step>/<total>` для multi-commit сабтасков (e.g. `1/3`); обязательно, если в `scope` прописана последовательность sub-commits |

**Append-only правила:**

- Новые записи добавлять **сверху** соответствующей секции.
- Не редактировать чужие записи retroactively (даже если они «устарели») —
  лучше создать новую запись с `supersedes: <id>`.
- Не удалять закрытые записи из этого файла — при необходимости
  переносить в `legacy/docs/archive/agent-sessions/`.
- Свою `open` запись можно править по ходу работы (добавлять notes, обновлять
  scope/files-touched-intent). Чужие записи — read-only.

---

## Активные сессии (status: open)

## Активные сессии (status: open)

<!-- Append-only ledger. По состоянию на `2026-06-20` consolidation r0 (L1.5),
     per user directive «удалить все сессии, кроме текущей» — все closed сессии
     архивированы в `legacy/docs/archive/agent-sessions/2026-06-week-3.md`.
     После L2.2 этот файл → `agent/workspace.md §5 (Active tasks)`.
     Текущая консолидирующая сессия `agent-file-consolidation-L1L2-r0` будет
     зарегистрирована в Commit E (§8.1 close-routine). -->

### session-2026-06-20T-agent-file-consolidation-L1L2-r0

- **id:** `2026-06-20T-agent-file-consolidation-L1L2-r0`
- **started-at:** 2026-06-20T11:20:00Z
- **closed-at:** 2026-06-20T16:35:00Z
- **commit-hash:** `f5dad16` — `chore(agent): §8.1 close-routine + post-consolidation verification (commits A-D)` (5 commits total in slice: `3c148e3`, `f1eeb6a`, `1bf096f`, `4f5f379`, `f5dad16`)
- **agent:** MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** **Full consolidation of `agent/` directory per operator directive `2026-06-20`.** Plan: 5 sequential commits (L1 + L2.1 + L2.2 + L2.3 + close-routine). All old `agent/memory.md` + `agent/decisions.md` + `agent/ARCHIVE-INDEX.md` → `agent/knowledge.md`. All old `agent/status.md` + `agent/active-sessions.md` (closed sessions) → `agent/workspace.md` + `legacy/docs/archive/agent-sessions/2026-06-week-3.md`. `agent/session-checklist.md` → inline in `AGENTS.md §9` (Commit D). All `§N` cross-refs preserved verbatim per `AGENTS.md §1` operator approval. Anchor script `tools/verify_section_anchors.sh` added in Commit A.
- **files-touched-intent:**
  - **NEW:** `agent/knowledge.md` (~1860 lines, merged from memory.md + decisions.md + ARCHIVE-INDEX.md)
  - **NEW:** `agent/workspace.md` (~250 lines, merged from status.md + active-sessions.md after L1.5 archive)
  - **NEW:** `tools/verify_section_anchors.sh` (anchor verification bash script)
  - **NEW:** `legacy/docs/archive/agent-sessions/2026-06-week-3.md` (1214 lines, all pre-`2026-06-20` closed sessions)
  - **DELETE:** `agent/memory.md`, `agent/decisions.md`, `agent/ARCHIVE-INDEX.md`, `agent/status.md`, `agent/session-checklist.md`
  - **EDIT:** `AGENTS.md` (§3 sources-of-truth list, §4 classification table, §9 session-checklist inlined, ~10 cross-refs updated)
  - **EDIT:** `TODO.md` (anchor-form cross-refs Commit A + bulk file-path replacement Commit E)
  - **EDIT:** `COMMENTS.md` (footer + L-anchor audit Commit A)
  - **EDIT:** `CHANGELOG.md` (consolidation entry Commit E + header cross-refs)
  - **EDIT:** `README_NEW.md` (bulk file-path replacement Commit E)
  - **APPEND-ONLY:** `agent/active-sessions.md` (эта запись, will be marked closed in this commit)
  - **НЕ ТРОГАЮ (per `AGENTS.md §6.5` scope discipline):** никакого production кода (`src/`, `tests/`, `external/`, `legacy/` кроме нового archive файла, `docs/`, `build/`, `tools/` кроме нового verify script, `CMakePresets.json`, корневой `CMakeLists.txt`, `src/CMakeLists.txt`, шейдеры), operator's dirty tree, чужие uncommitted.
- **status:** closed
- **post-commit notes:**
  - **Stuck loop limit per `AGENTS.md §6.7`:** 0 compile iterations (docs-only, no code touched). Well within 3-4 limit.
  - **Scope discipline clean:** 5 commits touching only doc-files + 1 new bash script + 1 new archive file. 0 production code, 0 submodules, 0 uncommitted-included.
  - **Anchor verification final:** 17 refs checked, 0 broken (`tools/verify_section_anchors.sh` exit 0).
  - **Safety-net cleanup:** `/tmp/before_agent_consolidation_20260620T112047Z.patch` (0 bytes — clean tree pre-work, deleted per §7).
  - **Close-routine per `AGENTS.md §8.1`:** (1) `git rev-parse HEAD` → `f5dad16`. (2) Эта запись обновлена: `status: open → closed`, `closed-at` + `commit-hash` проставлены, `notes` раздел переименован в `post-commit notes` (per §8.1 п.3). (3) Все 5 commits сессии зафиксированы в `CHANGELOG.md` 2026-06-20 §Changed. (4) Safety-net удалён. (5) Запись остаётся в блоке «Активные сессии» со статусом `closed` (transfer в «Закрытые сессии» не требуется, т.к. эта сессия — единственная в файле post-L1.5, и у archived-closed сессий отдельный файл).
  - **Cross-refs:** `agent/knowledge.md`, `agent/workspace.md`, `AGENTS.md §9`, `legacy/docs/archive/agent-sessions/2026-06-week-3.md`, `tools/verify_section_anchors.sh`.

---

## Закрытые сессии (status: closed)

> **Archived 2026-06-20** в `legacy/docs/archive/agent-sessions/2026-06-week-3.md` per L1.5
> user directive «удалить все сессии, кроме текущей». Все pre-`2026-06-20` closed session
> records (commit hashes, file-touched-intent, per-session notes) сохранены там для
> historical reference. Git log = source of truth для commit hashes.
>
> Ранние архивы (pre-`2026-06-15`):
> - `legacy/docs/archive/agent-sessions/2026-06-week-1.md` — сессии `2026-06-11` / `2026-06-12`.
