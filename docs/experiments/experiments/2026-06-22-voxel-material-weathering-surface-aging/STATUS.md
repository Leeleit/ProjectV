# STATUS — voxel-material-weathering-surface-aging

**Status:** `concluded-verdict-yes`
**Last updated:** 2026-06-22
**Next tick:** none (closed)

| Phase | State |
|:------|:------|
| 0 | Reservation + folder + INDEX/backlog sync | **DONE** |
| 1 | Web-research (Wikipedia Weathering/Rust/Patina) | **DONE** |
| 2 | Standalone C++26 CPU prototype (~430 LoC) | **DONE** |
| 3 | Benchmark (5×5×5×1000 = 125,000 measurements) | **DONE** |
| 4 | RESULTS.md + verdict | **DONE** |
| 5 | Close: sync INDEX + backlog | **DONE** |

**Blocker:** нет (closed)

**Verdict:** `yes`. E_HybridSparse ⭐⭐⭐ universal default (0.23 ns/voxel, 0.02% frame budget per 32³ chunk).
D_HierarchicalMask ⭐ for full rebuilds (1.67 ns/voxel, 0.16%). B_PerChunkDensity for far-LOD (0.83 ns/voxel, 0.08%).
