# agent/ARCHIVE-INDEX.md

Single source of truth для navigation в **archived service-file content**.

## Контекст

`AGENTS.md §6` (anti-duplication) требует, чтобы:
- `agent/memory.md` хранил **только** долговечные repo-specific факты
- `agent/status.md` хранил **только** короткий снимок текущего состояния
- `agent/active-sessions.md` хранил **только** recent open/closed сессии
- `agent/decisions.md` хранил **только** действующие engineering договорённости

Per-session audit log ("X landed on date Y with build green, ctest N/N, smoke M/M") накапливался в этих файлах до `2026-06-15` и **вытеснен в `legacy/docs/archive/agent-*/`** с сохранением section numbering, чтобы external cross-refs (TODO.md, AGENTS.md, decisions.md) резолвились через этот index.

## Mapping table

| Original section / session id | Archive file |
|---|---|
| `agent/memory.md` §10.12 (TAA infra) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.12` |
| `agent/memory.md` §10.13 (TAA offscreen) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.13` |
| `agent/memory.md` §10.14 (TAA wiring) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.14` |
| `agent/memory.md` §10.15 (TAA close-out) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.15` |
| `agent/memory.md` §10.16 (TAA ladder) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.16` |
| `agent/memory.md` §10.17 (TAA 1.2+1.3) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.17` |
| `agent/memory.md` §10.18 (TAA 1.7) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.18` |
| `agent/memory.md` §10.19 — M5.2 | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.19` |
| `agent/memory.md` §10.19 — two-level cache | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.19` |
| `agent/memory.md` §10.20 (model triplanar) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.20` |
| `agent/memory.md` §10.21 (TAA 1.5 per-layer) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.21` |
| `agent/memory.md` §10.22 (greedy meshing) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.22` |
| `agent/memory.md` §10.23 (frame-step) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.23` |
| `agent/memory.md` §10.24 (per-pass timings) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.24` |
| `agent/memory.md` §10.26 (audio engine) | `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md#10.26` |
| `agent/memory.md` §12 (Fluid CA audit) | `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12` |
| `agent/memory.md` §12.1 (CA pause + V-sync) | `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.1` |
| `agent/memory.md` §12.2 (V hotkey auto-detect) | `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.2` |
| `agent/memory.md` §12.3 (V cycle walk fix) | `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.3` |
| `agent/status.md` §5 (TAA A2) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#5` |
| `agent/status.md` §6 (P1 shadow fix) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#6` |
| `agent/status.md` §7 (TAA 1.1) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#7` |
| `agent/status.md` §8 (TAA 1.4+5.1+M5.2+6) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#8` |
| `agent/status.md` §9 (Handoff) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#9` |
| `agent/status.md` §10 (TAA 1.2+1.3) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#10` |
| `agent/status.md` §11 (TAA 1.7) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#11` |
| `agent/status.md` §12 (TAA 1.5) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#12` |
| `agent/status.md` §13 (Low-level perf) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#13` |
| `agent/status.md` §14 (greedy meshing) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#14` |
| `agent/status.md` §15 (M5.1d asset) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#15` |
| `agent/status.md` §15 (frame-step) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#15` |
| `agent/status.md` §16 (per-pass timings) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#16` |
| `agent/status.md` §18 (audio engine) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#18` |
| `agent/status.md` §19 (music HUD) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#19` |
| `agent/status.md` §20 (hardcore perf) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#20` |
| `agent/status.md` §15-19 (KT defense, LaTeX) | `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#15` |
| `agent/active-sessions.md` (24 sessions from `2026-06-12` / `2026-06-11`) | `legacy/docs/archive/agent-sessions/2026-06-week-1.md` |

## Что осталось live (для быстрого navigation)

- `agent/memory.md` — §1-9 (runtime/walk/build facts, Linux baseline), §10 (Shadow-quality), §10.11 (Per-corner AO), §11 (Hardcore perf plan), §10.27 (Agent protocol rewrite). **Total: ~552 строк / 87 KB**.
- `agent/status.md` — §1-4 (Now/Gap/Next/Risks), §21-§23 (current open sessions), §99 (rollup of past closed). **Total: ~264 строк / 25 KB**.
- `agent/active-sessions.md` — header + 12 most recent closed sessions. **Total: ~688 строк / 82 KB**.
- `agent/decisions.md` — все §1-§30 contracts (full). **Total: ~961 строк / 150 KB**.

## Reversal instructions

Если по historical audit понадобится исходный verbose content, **не revert'ить этот коммит**. Вместо этого:

1. Открыть archive file в `legacy/docs/archive/agent-*/`.
2. Section anchor работает: `#10.12`, `#12`, `#5`, и т.д. (markdown anchor — slug from header text; для precision — поиск по тексту в файле).
3. Скопировать нужный фрагмент, не restore весь файл (anti-duplication §6).

При необходимости развернуть секцию обратно в live — открыть PR с revert-подсекцией, с явным обоснованием «почему именно эта секция нужна в live».

## Сжатие метрик

| File | Before (2026-06-15 pre-compress) | After (post-compress) | Reduction |
|---|---|---|---|
| `agent/memory.md` | 205 KB / 1763 строк | 87 KB / 554 строк | -58% |
| `agent/status.md` | 124 KB / 1141 строк | 25 KB / 264 строк | -80% |
| `agent/active-sessions.md` | 198 KB / 1465 строк | 82 KB / 688 строк | -58% |
| `agent/decisions.md` | 150 KB / 959 строк | 150 KB / 961 строк | -0% (contracts kept) |
| **Total live** | **677 KB / 5328 строк** | **344 KB / 2467 строк** | **-49%** |
| `legacy/docs/archive/agent-*/` (new) | 0 | 328 KB / 2891 строк | +328 KB |
| **Total on disk** | 677 KB | 672 KB | -1% |

**Главный выигрыш — не на диске, а в cognitive load при чтении live-файлов.**
Каждый live файл теперь 1-2 экрана (264-961 строк вместо 959-1763), что соответствует
его контракту в `AGENTS.md §6` (memory = долговечные факты, status = короткий snapshot,
decisions = действующие договорённости). Archive files содержат полный per-session
detail для случая, когда он действительно нужен.

## Связанные ссылки

- `AGENTS.md` §4 (sources of truth), §6 (anti-duplication classification), §7.2.6 (multi-agent), §7.2.8 (shared `agent/` files), §7.3.1 (pre-commit gate, type=docs auto), §8.1 (auto-close routine).
- `legacy/docs/archive/agent_memory_§10_shadow_audit_2026-06-09.md` — pre-existing archive example, тот же формат.
- `legacy/docs/archive/agent_status_now_2026-06-10_pre_compaction.md` — pre-existing archive example, тот же формат.
