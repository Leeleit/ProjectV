# STATUS — `2026-06-20-rt-shadows-vs-csm`

**Phase:** concluded-verdict-mixed.
**Started:** 2026-06-20.
**Closed:** 2026-06-20 (single session).
**Last action:** Все 4 sync points complete per §13.5:

- `experiments/2026-06-20-rt-shadows-vs-csm/README.md` — 800 строк, все 9 секций заполнены.
- `experiments/2026-06-20-rt-shadows-vs-csm/STATUS.md` — этот файл.
- `INDEX.md §1/§5/§6/§8` — все updated (Just-closed entry, active session cleared, table row, Last update entry).
- `research/backlog.md §Open` → claimed + closed (запись переехала в §Closed); §In progress → empty.

**Blocker:** нет.
**Re-evaluation triggers:** Stage 4.3 lift draw distance (128+ chunks BLAS pool budget),
Blackwell consumer adoption (RTX 50 series 8× RT throughput → 8-ray soft shadow default flip
per `optimization-philosophy.md` 5% threshold), future RDNA 5 / Intel Celestial arch changes,
MoltenVK `VK_KHR_deferred_host_operations` adoption if ProjectV targets Apple Silicon.

**Verdict summary:** `mixed` — Hybrid CSM (sun, current `decisions.md §15` path) +
RTX shadow rays (`VK_KHR_ray_query`, feature-flagged additive per `TODO.md §5.2`) для local
lights + per-pixel contact shadow detail. Quality gain > 5% per
`optimization-philosophy.md` для non-sun-dominated scenes (cave, lava, magic-heavy); < 5%
для sun-dominated outdoor (CSM dominant). VRAM cost 8-23 MiB на dev host (well under 5%
budget). BLAS rebuild bottleneck async via `VK_KHR_deferred_host_operations` +
`dec-pipelines-async-compute` pattern. Cross-vendor: NVIDIA Blackwell/RDNA 4/Battlemage
= full benefit; Ampere/RDNA 3 = limited; Turing/Alchemist = feature-flagged OFF. Estimated
mainline effort: **M** (~770 LoC, 3-step migration per `knowledge.md §30.4`).

**Lighting axis complete:** `vct-vs-rt-cutoff` (closed verdict=mixed, GI cutoff) +
`clustered-forward-mass-lights` (closed verdict=yes, light SSBO array) + this (shadows).
Stage 5 foundation (nanovdb-on-gpu yes) + cutoffs + lights + shadows все closed same-day
`2026-06-20`.

**Cross-axis closure:** 18+ experiments closed same-day `2026-06-20` = full Stage 1.x/2.x/3.x/
5.x/6.x optimization landscape (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async/
gi-cutoff/clustered-lights/vis-buffer/frame-pacing/job-scheduling/shadows/...).