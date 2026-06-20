# STATUS — 2026-06-20-mesh-shader-vs-compute-cull

**Phase:** concluded (2026-06-20).
**Verdict:** `mixed` (compute cull + indirect draw = correct default; mesh shader = feature-flagged optional, not
default).
**Last action:** CPU-side analytical model compiled + run successfully. `STATUS.md` + `INDEX.md` + `research/backlog.md`
updated.

**Artifacts:**

- `README.md` — 9 sections (hypothesis, prior art, method, prototype, results, verdict, integration recommendation,
  sources, mapping to ProjectV hot-path).
- `prototype/cache_dispatch_model.cpp` — standalone CPU-side analytical model, C++26, compiles clean (Clang 22.1.x,
  `-O3 -march=native -DNDEBUG`), deterministic output.
- 16 web-research sources (academic + industry + vendor + voxel-specific), all 2024-2026, verified.

**Next tick:** None — closed session. Verdict is binding for Stage 2.1 design unless re-evaluation trigger fires.

**Re-evaluation trigger:** Stage 4.3 (128+ chunks draw distance) per `TODO.md §4.3` acceptance — when bandwidth savings
scales proportionally (4-8 MB at 128 chunks vs 2-4 MB at 64). May cross 5% perf threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. At that point, re-measure Pattern C vs Pattern A on
real MeshingStress scene with ProjectV workload.

**Blocker:** None.

**Scope discipline:** No `git *`, no modifications outside `docs/experiments/2026-06-20-mesh-shader-vs-compute-cull/`.
Per `docs/experiments/AGENTS.md §2`.