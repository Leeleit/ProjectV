# STATUS — 2026-06-20-vulkan-fps-pacing-vk-ext

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-20 — research complete (5 batch web queries, 8 key sources + 3 supplementary,
dev host validation via `vulkaninfo`). README §1-§7 filled with refined hypothesis + analytical
cost model. `sources.md` created. §3 prototype deferred (analytical literature sufficient для
integration recommendation). Single-pass sync per `AGENTS.md §13.5`: `backlog.md §In progress → §Closed`,
`INDEX.md §5 → §6`, meshing-algo-comparison sync fix (parallel-session close).

**Superseded (2026-06-21):** this experiment's measurement gap
(«Конкретные p99 frame variance numbers под Wayland compositor не измерены в этом эксперименте
(prototype deferred)» per old README §6 Verdict) filled by
**[`2026-06-21-vulkan-fps-pacing-wayland-prototype/`](../2026-06-21-vulkan-fps-pacing-wayland-prototype/)**
(claimed `2026-06-21` by self per `AGENTS.md §13.1` + §13.7 explicit supersede). New experiment adds
`VK_KHR_present_mode_fifo_latest_ready` (ratified 2025-03-18, не covered в old experiment) + Mesa 26.2
benchmark numbers (std-dev 0.9 → 0.3 ms Wayland → direct) + `low_latency_layer` cross-vendor data
(Phoronix 2026-05-17) + measured Wayland prototype (vs old analytical-only). Old experiment references
retained — литературная база остаётся валидной.
**Blocker:** нет.
**Verdict:** **`mixed`** — analytical literature valid; prototype deferred. Direction validated
(`VK_EXT_present_timing` = SOTA); dev host полностью supports все relevant extensions +
features enabled (`vulkaninfo 2026-06-20` confirms). **Mixed потому что measured Wayland-specific
p99 frame variance numbers отсутствуют** (Mesa 26.2 benchmark на KHR_display direct-display,
другие условия). Mainline prototype = expected follow-up.
**Next tick:** нет (concluded). Re-evaluation trigger: Stage 3.1 GPU Fluid CA pipeline integration
(cross-frame latency contract per `agent/workspace.md §2` + `agent/knowledge.md`) → mainline
prototype + measured Wayland p99 variance numbers.

**Operator override (per `docs/experiments/AGENTS.md §13.6`):** 2026-06-20, пользователь дал
инструкцию «выбирай тему или придумывай свою и исследуй» + «не выбирай work-stealing-job-system».
Previous reservation `2026-06-20-work-stealing-job-system` (m, Stage 4.1/6.1, claimed earlier
this session) released back to `research/backlog.md §Open`. Fresh claim:
`2026-06-20-vulkan-fps-pacing-vk-ext` (m, Stage 0 / independent).

**Sync state (per `docs/experiments/AGENTS.md §13.5`, single-pass):**

- `backlog.md §In progress` → entry moved to `§Closed` with verdict=mixed + `work-stealing-job-system`
  reservation released.
- `backlog.md §Closed` — `meshing-algo-comparison` sync fix r2 (was stale в §In progress) +
  `vulkan-fps-pacing-vk-ext` added.
- `INDEX.md §5` — active entry удалена (no active reservations 2026-06-20 EOD).
- `INDEX.md §6` — `meshing-algo-comparison` row added (verdict=mixed) +
  `vulkan-fps-pacing-vk-ext` row added (verdict=mixed).
- `experiments/2026-06-20-meshing-algo-comparison/STATUS.md` — left as-is (concluded-verdict-mixed,
  parallel-session update).
