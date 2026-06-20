# STATUS — 2026-06-20-dec-pipelines-async-compute

**Phase:** concluded-verdict-yes
**Last action:** 2026-06-20 — experiment closed; verdict `yes`. README + sources.md +
prototype/pipeline_overlap_analysis.md complete. Single-pass sync per `docs/experiments/AGENTS.md §13.5` to
`research/backlog.md §Closed` + `INDEX.md §6`.
**Next tick:** по запросу оператора. Open follow-up: re-validate June 2025 NVIDIA driver bug if/when Stage 2.1
mesh-shader path is enabled (currently feature-flagged off per `mesh-shader-vs-compute-cull` verdict=mixed).
**Blocker:** нет.

---

## Progress log

- `2026-06-20` — opened per §13.1-13.5 single-pass. Claim moved from `research/backlog.md §Open` to
  `§In progress`; `INDEX.md §1` + `§5` + `§8` updated; `experiments/2026-06-20-dec-pipelines-async-compute/`
  folder created with `STATUS.md` + `README.md` (initial draft, hypothesis + method + scope).
- `2026-06-20` — web research session per `docs/experiments/AGENTS.md §4` (8 web searches, 6+
  fallbacks per `agent/knowledge.md §9`). Cross-vendor matrix built (NVIDIA Ampere/Ada/Blackwell + AMD
  RDNA2/3/4 + Intel Arc Alchemist/Battlemage + Arm Mali TBDR). 2024-2026 SOTA established.
- `2026-06-20` — `pipeline_overlap_analysis.md` written (per-pass dependency graph, Amdahl-style upper
  bound, queue topology recommendation, sync point matrix).
- `2026-06-20` — `sources.md` written (full reference list with dates + authors + verification status).
  `README.md` finalized with 8 required sections + 2 additional (Operator handoff notes, continuity
  cross-refs).
- `2026-06-20` — experiment closed. Verdict: **`yes`**. Single-pass sync per §13.5: claim moved
  `research/backlog.md §In progress` → `§Closed`; `INDEX.md §5` → `§6`; `INDEX.md §1` §Now updated to
  reference closed state; `INDEX.md §8` Last update appended.

---

## Notes

- Cross-axis experiment: **sync/scheduling** axis, orthogonal к memory (svdag-vs-vdb) и layout
  (cache-oblivious-chunk-tree).
- Direct impact: `agent/knowledge.md §30.4` (Stage 3.1 contract) — sync model now concretized as
  `vkQueueSubmit2` + timeline semaphores + dedicated async-compute queue, not just «Pipeline barrier
  для swap ping-pong».
- Synergy: `bindless-descriptor-overhead` Phase E (RTX BLAS async build) — shared queue manager.
- Verification: web research only (no prototype harness). Per `benchmarks/methodology.md §5`, analytical
  model with vendor matrix + Amdahl-style upper bound = sufficient for `yes` verdict. If mainline needs
  on-host measurement, the natural harness is documented in `README.md §4` and
  `pipeline_overlap_analysis.md §9` for future follow-up.
- **Notable finding (forward-looking):** the local ProjectV doc
  `legacy/docs/architecture/practice/00_engine-structure.md:483` says «`VK_KHR_synchronization2` (core
  in 1.4)» but per Khronos it is actually **core in Vulkan 1.3** (2020-12-03). This is a minor
  inaccuracy with no functional impact (ProjectV targets 1.3+ per TODO §A1; 1.3 has it as core, 1.4
  inherits). Documented in `sources.md §5` cross-refs for future fix.
- **Caveat for future mesh-shader enablement:** the June 2025 NVIDIA driver bug
  (`forums.developer.nvidia.com/t/weird-async-compute-behavior/336090`) is specific to mesh-shading +
  async-compute-started-before-raster. ProjectV's current Stage 2.1 path is compute cull (per
  `mesh-shader-vs-compute-cull` verdict=mixed, default = compute), so bug does NOT apply. If mesh-shader
  is ever enabled, re-validate.
- **Tooling:** web search (Exa per AGENTS.md §4) + 6+ fallbacks per `agent/knowledge.md §9` line 1424.
  All sources verified per `docs/experiments/AGENTS.md §2` (date, author, context).
