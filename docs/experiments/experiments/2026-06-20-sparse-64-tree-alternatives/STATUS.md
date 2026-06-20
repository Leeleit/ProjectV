# STATUS — sparse-64-tree-alternatives

**Phase:** concluded-verdict-yes
**Last action:** 2026-06-20 — analysis complete, verdict `yes`. README.md заполнен всеми 9 секциями (Hypothesis, Prior
art, Method, Prototype, Results, Verdict, Integration recommendation, Sources, Mapping to ProjectV hot-path).
**Next tick:** по запросу оператора (закрыто)
**Blocker:** нет

---

## Progress log

- 2026-06-20 — открыт. Прочитан TODO.md Stage 1.1, 1.2; `src/voxel/Sparse64Tree.hpp` (393 строки, header-only,
  parallel-path уже работает); `src/voxel/VoxelWorld.hpp/cpp` (parallel path через `IsSparse64StorageEnabled()` /
  `PROJECTV_SPARSE_64_STORAGE` env var); `tests/Sparse64TreeTests.cpp` (14 sub-tests, coverage: depth/slot
  encoding/empty/set/get/mixed/OOB); `legacy/docs/architecture/adr/0002-svo-storage.md` (legacy 8-ary SVO,
  historical).
- 2026-06-20 — web research (4 batch queries, 32 results total). Verified key SOTA claims: dubiousconst282 2024-10
  (Tree64 = 0.62 B/voxel, 182 Mrays/s), eisenwave (tetrahexacontree = 40-60% faster than regular SVO), Aokana
  2025-05 (uses exactly our per-chunk SVDAG + 4×4×4 leaves + 64-bit bitmask design), Kämpe 2013 (SVDAG dedup
  28-576×), HashDAG 2020 / GPU-SVDAG-Editing PG 2024 (persistent SVDAG — future R&D), OpenVDB 13.0.0 / NanoVDB
  (VFX-targeted, not chunked gameplay).
- 2026-06-20 — verdict `yes`. Все три corner-cases (mutation, sparse DAG, GPU traversal) не упираются в design
  choice 64-tree. Sparse 64-tree + SVDAG-on-per-chunk + lazy dedup — это SOTA-паттерн, подтверждённый
  dubiousconst282 (2024), eisenwave (2024), Aokana (2025-05). Альтернативы (VDB/NanoVDB, BR-tree/BIH, octree
  regression, HashDAG) или не подходят для workload (VDB = VFX dense), или хуже по бенчмаркам (octree 40-60%
  медленнее), или overkill для Stage 1.1 (HashDAG = Stage 3.1+ territory).
- 2026-06-20 — `README.md` записан. `Integration recommendation` с 4 пунктами (default flip, SVDAG policy, GPU SSBO
  packing helper, doc archive). Estimated effort mainline: S + M + XS + XS = ~5-7 days total.

---

## Notes

- **Sparse 64-tree + SVDAG per chunk** — это **именно** то, что делает Aokana 2025 (arxiv 2505.02017), самый свежий
  академический reference (May 2025) для GPU-driven open-world voxel rendering. Наш план совпадает с SOTA.
- **OpenVDB — другой use case**: VFX (level sets, fluid sim, fog volumes), 4-level B+-tree, dense tile leaves. **Не
  для чанковых gameplay-сцен**. Pull in OpenVDB = ~100k LoC dep + paradigm mismatch + нет выигрыша на наших
  размерах (32³-128³ per region).
- **HashDAG (Phyronnaz 2020) + GPU-SVDAG-Editing (PG 2024)** — mature для mutable SVDAG на GPU, но их sweet spot
  = large single SVDAG edited on GPU. У нас per-chunk SVDAG (Aokana pattern), где mutation = per-chunk rebuild
  = sparse64tree standard policy. **Future R&D**, не Stage 1.
- **Наш `Sparse64Tree.hpp` уже содержит SVDAG machinery** (`SetDeduplicationEnabled` line 102-124, hash + multimap
  line 224-313). Stage 1.2 = policy (per-chunk `isStatic` + N-tick threshold), не data structure.
- **GPU SSBO packing**: при upload на GPU нужно **strip structuralHash** (8 B per node, CPU-only for dedup).
  Дёшево (1 commit), но **defer to Stage 2.1** (mesh shader reading SVDAG) — не нужно сейчас.
- **3% bandwidth savings** на GPU node = small but free; **2× ray traversal speedup** от bitmask-popcnt pattern
  per dubiousconst282 — вот главный win.
- **TODO.md Stage 1.1 acceptance criteria** (byte-equal output, ctest 16/16, MeshingStress TracyPlot ≥5%, 8× memory
  reduction on 10× empty chunks) — все достижимы с текущим кодом. Моя работа — design pre-validation, **не**
  measurement. Mainline owns integration + verification.
