# Workspace

Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`. Roadmap — `TODO.md`.

Updated: `2026-06-20` — **agent-file-consolidation r0 commits A-D** (session `session-2026-06-20T-agent-file-consolidation-L1L2-r0`). Per operator directive + `AGENTS.md §1` explicit approval. Commits: A `3c148e3` (L1 additive: `tools/verify_section_anchors.sh` + TODO.md semantic anchors + COMMENTS.md footer), B `f1eeb6a` (L2.1+2.4: `agent/knowledge.md` ~1860 lines, merged from `memory.md` + `decisions.md` + `ARCHIVE-INDEX.md`), C pending (L2.2+1.5: this file + `agent/active-sessions.md` → archived), D pending (L2.3: `agent/session-checklist.md` → inline in `AGENTS.md §9`).

---

## 1. Now

- **Project phase:** `pre-MVP alpha / working vertical slice`.
- **Active sub-plan:** `TODO.md` Roadmap v1 (dependency-aware, 6 Stages, GPU-driven, flat list with detailed per-item approach, supersedes Tier 0..5 r0 per `2026-06-20` rewrite + dependency-mismatch reordering). Cross-ref: `agent/knowledge.md` Part A §29 (Tier 0..5 r0 OUTDATED marker).
- **Most recent closed milestones** (one-line summary; full detail в archive `legacy/docs/archive/agent-sessions/2026-06-week-3.md`):
  - `2026-06-20` — TODO.md rewrite → dependency-aware 6-Stage GPU-driven roadmap v1 (commit `6709ca9`).
  - `2026-06-20` — agent-file-consolidation r0 commits A-D (`3c148e3`, `f1eeb6a`, this commit C, D pending).
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
- The next concrete mainline feature gap is the **Stage 1** of new TODO roadmap: Sparse 64-trees (1.1) → SVDAG (1.2) → async audio scan (1.3) before any Stage 2-5 GPU geometry work can begin (dependency-aware ordering, per TODO.md `How to read this file`).
- The current shadow limitation is now explicit and accepted: glass does not cast shadows in the mainline sun-shadow pass (`agent/knowledge.md` Part A §15 [First sun-shadow path](#15-first-sun-shadow-path)). `Fluid` casts as an opaque shadow-map caster; physical/tinted glass shadows remain future R&D.
- The new contact-shadow and `AOCC` baselines are intentionally bounded and local: both use short forward-shader voxel DDA traces against the existing packed world payload, not separate screen-space passes and not replacements for the current CSM layer. The new local point light is also bounded: authored, inverse-square, and shadowed by a short opaque-only voxel DDA term until a separate local shadow-map/cubemap step is worth adding.

## 3. Next Steps

1. For any further sun/contact shadow work, keep the new stricter close-out rule: inspected runtime captures are required, not sidecar numbers alone. Use `FINAL` + `SHDW` at minimum, and include `CSM` / `CTSH` when those paths are involved.
2. The local occlusion and bounded local-light-shadow slices can now pause unless live captures show a specific defect; the next truly different layer is real local shadow-map/cubemap infrastructure (Stage 5.2 in new TODO), while full `SSAO/GTAO` should still wait for a real depth/normal screen-space path.
3. Do not reintroduce fake glass shadow surrogates unless there is a real, separately scoped transparent-shadow path.
4. **Stage 1 priority for new work** (per dependency-aware TODO): Sparse 64-trees (1.1) → SVDAG (1.2) → async audio (1.3) — no Stage 2-5 work can start until Stage 1 is in mainline.

## 4. Risks

- Documentation will drift again if session history starts leaking back into `TODO.md` or `agent/`. Per L1.5 directive: all closed sessions archived to `legacy/docs/archive/agent-sessions/2026-06-week-3.md`, only current open session in `agent/active-sessions.md` (pre-`workspace.md` merge).
- Parallel `build/test/smoke` in the same build tree is still unsafe; smoke should be sequential and only when it is a relevant lifecycle/Vulkan check.
- `walk` tuning without replay/HUD/Tracy evidence will regress back toward synthetic-case patching.
- **`2026-06-10` destructive-git-checkout incident** (см. `agent/knowledge.md` Part B §10.11). Working rule: перед `git checkout -- .` — `cp` или `git stash push -m "KEEP_..."`. Pattern `git checkout -- .` + `git stash drop` **destructive** для uncommitted work предыдущих сессий.
- При работе с `agent/active-sessions.md` или `agent/workspace.md` соблюдать `AGENTS.md §7.2.8` (shared infra, edit **только своей** записи / APPEND-only).

---

## 5. Active tasks (current open sessions)

Append-only ledger. По состоянию на `2026-06-20` consolidation r0 (L1.5), per user directive «удалить все сессии, кроме текущей» — все closed сессии архивированы в `legacy/docs/archive/agent-sessions/2026-06-week-3.md`. После L2.2 этот файл → `agent/workspace.md §5 (Active tasks)`. Текущая консолидирующая сессия `agent-file-consolidation-L1L2-r0` зарегистрирована ниже.

### session-2026-06-20T-agent-file-consolidation-L1L2-r0

- **id:** `2026-06-20T-agent-file-consolidation-L1L2-r0`
- **started-at:** 2026-06-20T11:20:00Z
- **agent:** MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** **Full consolidation of `agent/` directory per operator directive `2026-06-20`.** Plan: 5 sequential commits (L1 + L2.1 + L2.2 + L2.3 + close-routine). All old `agent/memory.md` + `agent/decisions.md` + `agent/ARCHIVE-INDEX.md` → `agent/knowledge.md`. All old `agent/status.md` + `agent/active-sessions.md` → `agent/workspace.md` (after this commit). `agent/session-checklist.md` → inline in `AGENTS.md §9` (Commit D). Closed sessions from `agent/active-sessions.md` → `legacy/docs/archive/agent-sessions/2026-06-week-3.md` (L1.5). All `§N` cross-refs preserved verbatim per `AGENTS.md §1` operator approval. Anchor script `tools/verify_section_anchors.sh` added in Commit A.
- **files-touched-intent:**
  - **NEW:** `agent/knowledge.md` (~1860 lines, merged from memory.md + decisions.md + ARCHIVE-INDEX.md)
  - **NEW:** `agent/workspace.md` (~250 lines target, this file)
  - **NEW:** `tools/verify_section_anchors.sh` (anchor verification bash script)
  - **NEW:** `legacy/docs/archive/agent-sessions/2026-06-week-3.md` (1214 lines, all pre-`2026-06-20` closed sessions)
  - **DELETE:** `agent/memory.md`, `agent/decisions.md`, `agent/ARCHIVE-INDEX.md`, `agent/status.md`, `agent/session-checklist.md`
  - **EDIT:** `AGENTS.md` (§3 sources-of-truth list, §4 classification table, §9 session-checklist inlined)
  - **EDIT:** `TODO.md` (anchor-form cross-refs, Commit A)
  - **EDIT:** `COMMENTS.md` (footer + L-anchor audit, Commit A)
  - **EDIT:** `CHANGELOG.md` (consolidation entry, Commit E)
  - **НЕ ТРОГАЮ (per `AGENTS.md §6.5` scope discipline):** никакого production кода (`src/`, `tests/`, `external/`, `legacy/` кроме нового archive файла, `docs/`, `build/`, `tools/` кроме нового verify script, `CMakePresets.json`, корневой `CMakeLists.txt`, `src/CMakeLists.txt`, шейдеры), operator's dirty tree, чужие uncommitted.
- **status:** open
- **notes:**
  - **Pre-flight findings:** HEAD `76465ea` (todo-rewrite close-routine), +39 над origin/master, working tree clean (только `repomix-output.xml` untracked, не в scope). Safety-net patch `/tmp/before_agent_consolidation_<ts>.patch` saved (0 bytes — clean tree).
  - **Plan approval:** 5 questions answered (scope = L1+L2; anchor format = script-based; AGENTS.md = explicit operator approval granted; commit pacing = mega-atomic all 5; L1.5 = delete all sessions except current).
  - **Sequential commits** (no pauses between, per mega-atomic pacing): (A) L1 + verify script + COMMENTS footer. (B) L2.1+2.4 = knowledge.md merge. (C) L2.2+1.5 = workspace.md merge + closed-sessions archive. (D) L2.3 = session-checklist inline in AGENTS.md §9. (E) Close-routine + post-consolidation verification.
  - **Honest scope:** только doc-файлы + 1 new bash script + 1 new archive file. 0 production code touched. Build green не нужен (docs-only per `AGENTS.md §7.3.1`).
  - **Commit policy per `AGENTS.md §6.9`:** user explicit `go` = all 5 commits, no per-commit «Commit?» pauses.
  - **Cross-refs:** `TODO.md`, `CHANGELOG.md`, `AGENTS.md` (hub files), `agent/knowledge.md` (formerly decisions.md + memory.md), `agent/workspace.md` (new), `legacy/docs/archive/agent-sessions/2026-06-week-3.md` (new archive).

---

## 6. Recent closed sessions

> **Archived 2026-06-20** в `legacy/docs/archive/agent-sessions/2026-06-week-3.md` per L1.5
> user directive «удалить все сессии, кроме текущей». Все pre-`2026-06-20` closed session
> records (commit hashes, file-touched-intent, per-session notes) сохранены там для
> historical reference. Git log = source of truth для commit hashes.
>
> Ранние архивы (pre-`2026-06-15`):
> - `legacy/docs/archive/agent-sessions/2026-06-week-1.md` — сессии `2026-06-11` / `2026-06-12`.

---

## 7. Archive references

- `legacy/docs/archive/agent-sessions/2026-06-week-1.md` — сессии `2026-06-11` / `2026-06-12` (pre-compress).
- `legacy/docs/archive/agent-sessions/2026-06-week-3.md` — все pre-`2026-06-20` closed sessions (L1.5 archive).
- `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md` — pre-`2026-06-15` per-session snapshots от старого `agent/status.md`.
- `legacy/docs/archive/agent-todos/2026-06-tier-0-5-r0.md` — old TODO Tier 0..5 r0 (superseded by current `TODO.md`).
- `legacy/docs/archive/agent-memory/2026-06-taa-sessions.md`, `2026-06-fluid-ca-sessions.md` — per-session detail из `memory.md` §10.12-§10.26, §12 (см. `agent/knowledge.md` Part C).
- `legacy/docs/archive/agent-*/README.md` — общий индекс (per `agent/ARCHIVE-INDEX.md` inline в knowledge.md Part C).
