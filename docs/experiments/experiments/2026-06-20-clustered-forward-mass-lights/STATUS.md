# STATUS — 2026-06-20-clustered-forward-mass-lights

**Status:** concluded-verdict-yes
**Stage:** TODO.md §5 (GI & Temporal, depends on §1.2 SVDAG)
**Phase:** complete

## Last action (2026-06-20 EOD)

- Web-research complete: ~30 sources верифицированы (5 batch queries) — Harada 2012
  Forward+, Olsson 2012 Clustered Shading, themaister 2020 Granite (subgroup
  optimizations), logdahl 2025 (10k lights), WebGPU 2025 benchmarks (CIS5650 + 5
  forks), voxel-specific (Black_Key, Vyatkin 2024).
- Standalone CPU prototype `prototype/bench.cpp` (single file, ~480 LoC) compiled clean
  (no warnings) on Clang 22.1.6.
- 13 measurement configs (sparse + dense scenarios, 3 grid resolutions, 4 light counts).
- **Key finding:** 16×9×24 / 5000 dense lights = 69% clusters overflow soft cap 1024.
  → Soft cap must be ≥2048 OR light prioritization policy required.
- Cross-validation с published GPU numbers (within 5-10× — consistent с scalar→SIMT).
- Per-fragment analytical model: **100× speedup vs 1000-light uniform array**.
- GPU projected: 0.1-0.5 ms cluster build at 1000 lights (well within 16.67 ms frame budget).

## Next action

- (none — experiment complete, doc sync pass ниже)

## Doc sync (2026-06-20 EOD)

- INDEX.md §1: см. entry "2026-06-20-clustered-forward-mass-lights (verdict=yes)".
- INDEX.md §5: cleared (experiment closed, no active reservations).
- INDEX.md §6: added row.
- INDEX.md §8: appended Last update paragraph.
- research/backlog.md: moved entry to §Closed (per §13.5).

## Blocker

- Нет. Wait for mainline action.

## ETA

- Experiment closed. Mainline effort estimate: M (Steps 1-3 = 3-4 sessions).
