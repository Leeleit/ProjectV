# Workspace

Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`. Roadmap — `TODO.md`.

Updated: `2026-06-20` — **agent-file-consolidation r0 commits A-F** + Cleanup A `chore(agent): delete active-sessions.md — single source of truth = workspace.md §5`. Per operator directive + `AGENTS.md §1` explicit approval. Commits: A `3c148e3` (L1 additive), B `f1eeb6a` (L2.1+2.4 knowledge.md), C `1bf096f` (L2.2+1.5 workspace.md + closed-sessions archive), D `4f5f379` (L2.3 session-checklist inline), E `f5dad16` (bulk replace + final verification), F `1a845af` (close-routine). **Cleanup A (this slice)** — `agent/active-sessions.md` физически удалён; consolidation session record перенесён в `legacy/docs/archive/agent-sessions/2026-06-week-3.md` per L1.5 directive.

---

## 1. Now

- **Project phase:** `pre-MVP alpha / working vertical slice`.
- **Active sub-plan:** `TODO.md` Roadmap v1 (dependency-aware, 6 Stages, GPU-driven, flat list with detailed per-item approach, supersedes Tier 0..5 r0 per `2026-06-20` rewrite + dependency-mismatch reordering). Cross-ref: `agent/knowledge.md` Part A §29 (Tier 0..5 r0 OUTDATED marker).
- **Most recent closed milestones** (one-line summary; full detail в archive `legacy/docs/archive/agent-sessions/2026-06-week-3.md`):
  - `2026-06-20` — agent-file-consolidation r0 (commits A-F `3c148e3`...`1a845af`) + Cleanup A: `agent/active-sessions.md` removed; single source of truth = `workspace.md §5`.
  - `2026-06-20` — TODO.md rewrite → dependency-aware 6-Stage GPU-driven roadmap v1 (commit `6709ca9`).
  - `2026-06-20` — Tier 2 mainline C++20 modules + `import std;` blocked (commit `44362d1`).
  - `2026-06-20` — Tier 5.7 + 5.4 + 5.2 + 5.10 batch (commit `72eca66`).
  - `2026-06-20` — Tier 0.B/C/D/E hot-path Vec3/FrustumCull/inplace/kernel (commits `cf4b535`, `af69d06`, `bafecf9`, `08de29d`).
  - `2026-06-19` — Inspection sweep v3 + defense docs (commits `09ea3a4`, `1db35ee`, `bf2822f`, `d641967`).
  - `2026-06-18` — Windows host build r0 (multi-commit `69b1726` lineage).
- `walk` controller tuning работает on Tier 5 follow-up; replay-first diagnosis already covered by `PhysicsWalkDebugInfo` + `TracyPlot`.
- Linux dev baseline: clang 22.1.6 native + lld 22.1.6 + libstdc++ 16.1.1 + SDL3 3.4.10 + Vulkan 1.4.350, см. `agent/knowledge.md` Part B §5-§9 (formerly `memory.md`).

## 2. Nearest Gap

- The old P0 process reminders are no longer left open in `TODO.md`: replay-first controller diagnosis, developer-only runtime smoke, and the current warning-cleanup closure are already treated as established baseline, not as unfinished work.
- Runtime smoke policy changed: `ProjectVRuntimeSmoke` is now a targeted lifecycle/Vulkan check, not a default DoD step for every lighting/material/doc change.
- Tracy-profiler build policy changed: `build/windows-clang-debug-tracy-profiler` is no longer part of routine verification. Use it only for explicit Tracy/profiling work or when the user asks for that build specifically.
- If another warning-cleanup pass is needed, regenerate `Problems/` first instead of continuing from the current XML export.
- The next concrete mainline feature gap is the **Stage 1** of new TODO roadmap: Sparse 64-trees (1.1) → SVDAG (1.2) → async audio scan (1.3) before any Stage 2-5 GPU geometry work can begin (dependency-aware ordering, per `TODO.md` `How to read this file`).
- The current shadow limitation is now explicit and accepted: glass does not cast shadows in the mainline sun-shadow pass (`agent/knowledge.md` Part A §15 [First sun-shadow path](#15-first-sun-shadow-path)). `Fluid` casts as an opaque shadow-map caster; physical/tinted glass shadows remain future R&D.
- The new contact-shadow and `AOCC` baselines are intentionally bounded and local: both use short forward-shader voxel DDA traces against the existing packed world payload, not separate screen-space passes and not replacements for the current CSM layer. The new local point light is also bounded: authored, inverse-square, and shadowed by a short opaque-only voxel DDA term until a separate local shadow-map/cubemap step is worth adding.

## 3. Next Steps

1. For any further sun/contact shadow work, keep the new stricter close-out rule: inspected runtime captures are required, not sidecar numbers alone. Use `FINAL` + `SHDW` at minimum, and include `CSM` / `CTSH` when those paths are involved.
2. The local occlusion and bounded local-light-shadow slices can now pause unless live captures show a specific defect; the next truly different layer is real local shadow-map/cubemap infrastructure (Stage 5.2 in new TODO), while full `SSAO/GTAO` should still wait for a real depth/normal screen-space path.
3. Do not reintroduce fake glass shadow surrogates unless there is a real, separately scoped transparent-shadow path.
4. **Stage 1 priority for new work** (per dependency-aware TODO): Sparse 64-trees (1.1) → SVDAG (1.2) → async audio (1.3) — no Stage 2-5 work can start until Stage 1 is in mainline.

## 4. Risks

- Documentation will drift again if session history starts leaking back into `TODO.md` or `agent/`. Per L1.5 directive: all closed sessions archived to `legacy/docs/archive/agent-sessions/2026-06-week-3.md`; `agent/active-sessions.md` physically removed (Cleanup A); single source of truth = `workspace.md §5`.
- Parallel `build/test/smoke` in the same build tree is still unsafe; smoke should be sequential and only when it is a relevant lifecycle/Vulkan check.
- `walk` tuning without replay/HUD/Tracy evidence will regress back toward synthetic-case patching.
- **`2026-06-10` destructive-git-checkout incident** (см. `agent/knowledge.md` Part B §10.11). Working rule: перед `git checkout -- .` — `cp` или `git stash push -m "KEEP_..."`. Pattern `git checkout -- .` + `git stash drop` **destructive** для uncommitted work предыдущих сессий.
- При работе с `agent/workspace.md` или `agent/knowledge.md` соблюдать `AGENTS.md §7.2.8` (shared infra, edit **только своей** записи / APPEND-only).

---

## 5. Active tasks (current open sessions)

Empty per L1.5 user directive «удалить все сессии, кроме текущей» (extended to consolidation session after Cleanup A `2026-06-20`). See `legacy/docs/archive/agent-sessions/2026-06-week-3.md` для последней закрытой сессии (`session-2026-06-20T-agent-file-consolidation-L1L2-r0`).

Append-only ledger. Когда стартует новая сессия, она регистрируется здесь по формату:

```markdown
### session-YYYY-MM-DDTHH-MM-SSZ-<short-id>-r0

- **id:** `YYYY-MM-DDTHH-MM-SSZ-<short-id>-r0`
- **started-at:** YYYY-MM-DDTHH:MM:SSZ
- **agent:** MiniMax-M3 (or model id)
- **operator:** le1t
- **branch:** master
- **scope:** ...
- **files-touched-intent:** ...
- **status:** open
- **notes:** ...
```

См. `AGENTS.md §9.1` (Инициализация) для полного протокола регистрации. После close-routine per `AGENTS.md §8.1` запись переносится в `legacy/docs/archive/agent-sessions/<week>.md`.

---

## 6. Recent closed sessions

> **Archived 2026-06-20** в `legacy/docs/archive/agent-sessions/2026-06-week-3.md` per L1.5
> user directive «удалить все сессии, кроме текущей» (extended to consolidation session after Cleanup A). Все closed session
> records (commit hashes, file-touched-intent, per-session notes) сохранены там для
> historical reference. Git log = source of truth для commit hashes.
>
> **Latest closed:** `session-2026-06-20T-agent-file-consolidation-L1L2-r0` —
> consolidation slice (6 commits `3c148e3` ... `1a845af`), archived per Cleanup A.
>
> Ранние архивы (pre-`2026-06-15`):
> - `legacy/docs/archive/agent-sessions/2026-06-week-1.md` — сессии `2026-06-11` / `2026-06-12`.

---

## 7. Archive references

- `legacy/docs/archive/agent-sessions/2026-06-week-1.md` — сессии `2026-06-11` / `2026-06-12` (pre-compress).
- `legacy/docs/archive/agent-sessions/2026-06-week-3.md` — все pre-`2026-06-20` closed sessions + `session-2026-06-20T-agent-file-consolidation-L1L2-r0` (L1.5 archive + Cleanup A).
- `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md` — pre-`2026-06-15` per-session snapshots от старого `agent/status.md`.
- `legacy/docs/archive/agent-todos/2026-06-tier-0-5-r0.md` — old TODO Tier 0..5 r0 (superseded by current `TODO.md`).
- `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md`, `2026-06-fluid-ca-sessions.md` — per-session detail из `memory.md` §10.12-§10.26, §12 (см. `agent/knowledge.md` Part C).
- `legacy/docs/archive/agent-*/README.md` — общий индекс (per `agent/ARCHIVE-INDEX.md` inline в knowledge.md Part C).