# STATUS — 2026-06-21-sub-chunk-layers

**Phase:** closed.

**Status:** concluded-verdict-mixed.

**Last action (2026-06-21):**

- Experiment closed with verdict=`mixed`. Web-research complete (3 batch queries, ~14 sources верифицированы
  включая Minecraft-1.18+ Java ChunkSection + Bedrock SubChunk 4D + scrayos SHARD layered format + Tunact124
  ATLAS AARF columnar + Cubyz CaveMap + Hytale NStagedChunkGenerator + Vulkan Guide Ascendant chunk layers
  + maguirekrist production-grade voxel pipeline).
- Standalone C++26 CPU prototype built (`prototype/sub_chunk_bench.cpp` ~870 LoC, `clang++ 22.1.6 -O3
  -march=native`, build green). 5 designs (A_Monolithic / B_Palette / C_FixedLayer_L2 / D_FixedLayer_L4
  + E_Hybrid reserved for follow-up) × 5 scenes × 5 seeds × 1000 iter = 100 measurements.
- Memory savings validated: **B_Palette 96% для uniform, 84% для 2-material, 71% для 4-material** vs
  monolithic. **D_L4 73-92%, C_L2 71-84%.** Build cost overhead 30-55×, but absolute 1-6 µs vs 50 µs
  Stage 4.1 budget. Mutation cost overhead 5-70%, but absolute 10-19 ns. Mesh vertex count identical
  (layout-orthogonal). Layer boundary count 28-155 explicit transitions for layered (vs 0 for monolithic)
  = semantic gain для VCT anti-leak + per-layer LOD + selective rebuild.

**Next action:** none — experiment closed. Integration recommendation в README §7.

**Blocker:** нет.

**Date closed:** 2026-06-21 (same session, single experiment).

**Verdict:** `mixed` — paletted/layered designs win на memory axis (73-96% savings) + layer-boundary
semantic axis, lose на build cost axis (acceptable per Stage 4.1 budget headroom) + mutation cost axis
(negligible absolute cost). **Recommendation:** B_Palette для uniform chunks (96% savings), D_L4 для
biome/cave chunks (73-79% savings + 28-62 transitions), C_L2 для finer biome granularity (71-84% + 80-155
transitions); A_Monolithic as fallback для sparse chunks + legacy compatibility.

**Cross-refs:**

- `experiments/2026-06-21-sub-chunk-layers/README.md` — full hypothesis + method + results + integration
  recommendation.
- `experiments/2026-06-21-sub-chunk-layers/prototype/` — standalone C++26 CPU benchmark
  (`sub_chunk_bench.cpp` + `CMakeLists.txt` + `README.md` + `build/results_all.csv` +
  `build/summary_means.csv`).
- `experiments/2026-06-21-sub-chunk-layers/sources.md` — 14 верифицированных sources.
- `research/backlog.md §Closed` — sync per `AGENTS.md §13.5`.
- `INDEX.md §6 Recent closed sessions` + `INDEX.md §8 Last update`.
- `hardware-profile.md §1/§2` — CPU + RAM baseline.
- `TODO.md §4.1` (GPU Noise & World Gen) + `§4.2` (LOD) + `§5.1` (VCT) — integration target stages.
- `2026-06-21-wfc-procedural-worlds` (orthogonal axis: gen-strategy) + `2026-06-20-nanovdb-on-gpu`
  (complementary: outer SVO storage) + `2026-06-21-gpu-procedural-noise-compute-kernels` (complementary:
  per-layer noise queries) + `2026-06-20-svdag-vs-vdb-memory-throughput` (complementary: isStatic flag
  amortizes build cost) + `2026-06-20-dec-pipelines-async-compute` (foundation: async populate).
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% integration threshold
  (memory savings 73-96% validated, well above threshold).
- `agent/knowledge.md` — 3-step migration precedent для integration.
- `benchmarks/methodology.md` — measurement protocol followed.
