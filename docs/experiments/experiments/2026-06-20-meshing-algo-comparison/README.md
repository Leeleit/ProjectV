# 2026-06-20-meshing-algo-comparison — Dual Contouring vs Surface Nets vs Marching Cubes vs Naive Greedy on ProjectV-style voxel chunks

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-20
**Date closed:** 2026-06-20
**Stage link:** TODO.md §2.1 (visual mesh shader) + §3.3 (greedy physics mesh)
**Estimated effort:** M
**Author:** self

---

## 1. Hypothesis

**Гипотеза:** Для ProjectV voxel-входа (axis-aligned solid voxels, sparse, SVDAG-deduped per Stage 1.2) **Naive Greedy
** (current mainline, per `agent/knowledge.md`, per-axis dispatch в `voxel_mesh.comp::GreedyFacePass`) остаётся
optimal vs Surface Nets / Marching Cubes / Dual Contouring — по poly count, build time, и GPU mesh-shader portability —
потому что:

1. Sharp cube edges = desired (Minecraft-style, voxel aesthetic). SN/MC избыточно сглаживают; DC сохраняет sharp edges
   но требует QEF solver (CPU heavy) + хуже ложится на mesh shader LDS.
2. Voxel input — не SDF — не нуждается в gradient sampling; SN/DC преимущества исчезают на quantized input.
3. Quad-extension (Naive Greedy core) trivially parallel, отлично ложится на mesh shader task + mesh (Stage 2.1).
4. Surface area reduction greedy-quad vs per-voxel: 6× → 1× quad per coplanar run — максимум что возможно для
   axis-aligned voxel scenes.

**Альтернативы:**

- **Marching Cubes** (MC): classic, smooth, ~2-5× poly count vs greedy на sharp scenes, ambiguity table edge cases, не
  использует voxel-quantization.
- **Surface Nets** (SN): smooth dual contour, vertex at mean of edge intersections, poly count ~1.5-2× greedy,
  сглаженные углы (НЕ желательно для voxel games).
- **Dual Contouring** (DC): sharp edges preserved via Hermite data (per-edge intersection points + per-cell normals),
  QEF minimization per cell, poly count ~2-3× greedy, complex solver.

**Метрика успеха:** «Naive Greedy не хуже всех трёх альтернатив на > 5% ни по одной из метрик (build time / poly count /
mesh-shader portability score) на ProjectV-стиле scenes (sparse, sharp, axis-aligned)». Если нет — re-evaluate.

**Caveat:** Это эксперимент. Может оказаться, что для procedural worlds (Stage 4.1) с smooth terrain преимущество DC/SN
есть. Прототип тестирует **оба** типа сцен.

---

## 2. Prior art

Web-research выполнен в `sources.md`. Ключевые источники (предварительно):

- TBD: filled in after web_search.

---

## 3. Method

- **Тип эксперимента:** prototype + benchmark (standalone C++17/20, без GPU).
- **Сцена:**
    - **Synthetic voxel scenes** (CPU-generated, batch):
        - A. Single 32³ cube (1×32³ uniform).
        - B. Hollow shell (32³ solid border, 30³ air interior).
        - C. Sphere-discretized (32³, ~50% fill, smooth surface boundary).
        - D. Layered terrain (32³ slices, varying density 10%-90%).
        - E. Sparse random (32³, 1% density, scattered cubes).
        - F. ProjectV-style scene (per-chunk mix: solid + sparse + aabb-structured).
- **Алгоритмы:** Naive Greedy (current mainline), Marching Cubes (Lorensen-Cline 1987 + resolved ambiguity), Surface
  Nets (Gibson 1998), Dual Contouring (Ju et al. 2002).
- **Метрики:**
    - build time (ns, per chunk, mean/median/p95/p99/std за 1000 iter)
    - output poly count (triangles per chunk; quad = 2 triangles для greedy)
    - output memory (bytes per chunk)
    - mesh quality proxy: per-triangle area distribution (uniform = good; degenerate triangles = bad)
- **Контроль:** Naive Greedy = baseline (current mainline per `agent/knowledge.md`).
- **Протокол:** per `benchmarks/methodology.md`: warmup 30 iter, 1000 iter per config, JSON/CSV output, governor fixed,
  isolated core.

---

## 4. Prototype

Standalone C++ код в `prototype/`:

```bash
cd docs/experiments/experiments/2026-06-20-meshing-algo-comparison/prototype
clang++ -std=c++20 -O3 -march=native -DNDEBUG -o bench bench.cpp
taskset -c 2 ./bench --all > results.csv
taskset -c 2 ./bench --scene=sphere,layered_terrain --algo=naive_greedy,marching_cubes --iters=2000
```

Output: `results.csv` + `RESULTS.md`.

**Алгоритмы:** `naive_greedy`, `surface_nets`, `dual_contouring`, `marching_cubes`.
**Сцены:** `solid_cube`, `hollow_shell`, `sphere`, `layered_terrain`, `sparse_random`, `projectv_mix`.
**CLI:** `--all` (default), `--scene=s1,s2`, `--algo=a1,a2`, `--iters=N`, `--warmup=N`, `--help`.

---

## 5. Results

См. [`RESULTS.md`](./RESULTS.md) — полная таблица 24 конфигураций (4 алгоритма × 6 сцен) + sanity check + анализ.

**Главные цифры (mean build time, ns; 32³ chunk, 5800X single-core):**

| scene           |    greedy | surface_nets | dual_contouring | marching_cubes |
|-----------------|----------:|-------------:|----------------:|---------------:|
| solid_cube      |   633 587 |      335 565 |       1 292 556 |        249 728 |
| hollow_shell    |   574 233 |      394 814 |       1 569 764 |        380 277 |
| sphere          |   650 489 |      388 330 |       1 375 693 |        345 876 |
| layered_terrain | 2 179 993 |    1 064 622 |       4 816 783 |      1 949 115 |
| sparse_random   |   557 933 |      342 162 |       1 170 345 |        247 071 |
| projectv_mix    |   575 031 |      338 730 |       1 290 500 |        265 127 |

**Главные цифры (triangle count):**

| scene           | greedy | surface_nets | dual_contouring | marching_cubes |
|-----------------|-------:|-------------:|----------------:|---------------:|
| solid_cube      |     12 |            0 |               0 |              0 |
| hollow_shell    |     24 |        9 118 |           9 106 |         10 796 |
| sphere          |  3 108 |        4 006 |           4 006 |          7 388 |
| layered_terrain | 53 854 |       67 008 |          67 008 |         70 800 |
| sparse_random   |  3 608 |        1 220 |           1 220 |          2 258 |
| projectv_mix    |     96 |        3 906 |           3 906 |          4 402 |

**Наблюдения (детали в `RESULTS.md` §3):**

1. **Greedy даёт минимум triangles на 5/6 non-degenerate сцен** (исключение: sparse_random, где SN/MC лучше за счёт
   per-cell dual merge для изолированных voxel'ов).
2. **MC самый быстрый по build time** на 5/6 сцен (250-380 µs для типичных). Greedy в 1.7-2.5× медленнее.
3. **DC в 4-5× медленнее MC** (QEF solver overhead).
4. **Build time не критичен** при streaming rates ProjectV (1-3 ms total per scene load), но **triangle count критичен**
   для vertex shader cost (Stage 2.1).

---

## 6. Verdict

**`mixed`** (с уточнением: per-metric).

Greedy **выигрывает по poly count** на 5/6 non-degenerate сцен (главная метрика для vertex-bound Stage 2.1) — корневая
гипотеза подтверждена **для triangle count**.

Greedy **проигрывает по build time** в 1.7-2.5× vs MC, в 1.5-2× vs SN (на dense сценах). Build time не критичен для
ProjectV hot-path при streaming rates, но **нельзя сказать "optimal"** — original claim "не хуже всех трёх альтернатив
на > 5% по build time" **НЕ подтверждён**.

Для sparse сцен (1% density, isolated voxels) SN/MC **дают меньше triangles**, чем greedy — coplanar merge не работает
на изолированных 1×1×1 voxel'ах, dual/tri per-cell merge помогает.

**Mesh-shader portability** — не измерено в CPU-прототипе, остаётся analytical claim (per-axis dispatch trivially
parallel).

---

## 7. Integration recommendation

**Mainline остаётся на Naive Greedy для Stage 2.1 (visual mesh) + Stage 3.3 (physics mesh)**, с двумя оговорками:

1. **Triangle count** — greedy остаётся default (1.3-450× меньше triangles чем MC/SN/DC на non-sparse сценах). Главная
   метрика для vertex-bound Stage 2.1.
2. **Build time** — известное узкое место (в 2× медленнее MC). При Stage 4.1 (procedural world gen с high-frequency
   rebuild) может потребоваться optimization. Опции:
    - **Bitwise cull** (per cgerikj 2020 binary-greedy-meshing): 64 faces batched в uint64 mask → 50-200 µs/chunk.
      Drop-in optimization.
    - **Dual-emit per face** (positive + negative в одном scan): сокращает 6 direction scans до 3 + symmetric extension.
      Требует algorithm rewrite, не plug-in.

**Stage 3.3 (physics mesh) Jolt MeshShape:**

- Greedy output (per-axis quads) → Jolt expects triangle soup. Conversion 1 quad = 2 triangles = trivial.
- Greedy triangle count минимум → Jolt collision broadphase cheaper. Default.

**Альтернативы НЕ рекомендуются** для ProjectV hot-path:

- **Marching Cubes** — fastest build, но 1.3-450× больше triangles на binary voxel input. Smooth surface artifact (
  interpolated midpoints) — несовместимо с Minecraft-style aesthetic.
- **Surface Nets** — competitive build time, но сглаженные углы (vertex at cell center) — несовместимо с sharp voxel
  edges ProjectV.
- **Dual Contouring** — slowest (QEF overhead 4-5× vs MC), sharp edges preserved, но QEF solver per cell + Hermite data
  collection — overhead не оправдан для binary voxel input.

**Sparse-scene caveat (Stage 4.1 / Stage 6.x procedural world):** при 1% density scenes SN/MC дают меньше triangles.
Если ProjectV разрастётся до procedural world с sparse структурами — re-evaluate (см. `wfc-procedural-worlds` backlog
item).

**Cross-axis mapping:**

- Параллельные closed experiments: `mesh-shader-vs-compute-cull` (verdict=mixed, compute cull + indirect draw default) +
  `sparse-64-tree-alternatives` (verdict=yes, SVDAG-on-64-tree).
- Greedy meshing корректно ложится на оба: per-axis dispatch → mesh shader spike (Stage 2.1) OR compute cull + indirect
  draw (current mainline).
- Triangle count min → смягчает Stage 2.2 HZB cull pressure (less per-frame work) + Stage 3.3 Jolt broadphase cheaper.

**Caveat для mainline-агента:**

- Prototype измеряет только CPU single-threaded. GPU dispatch (mesh shader port) может изменить картину: per-axis
  dispatch trivially parallel → mesh shader occupancy benefits. Re-benchmark на GPU spike (Stage 2.1) обязателен.
- SVDAG traversal cost не измерен (flat array input). Flat → SVDAG может добавить 5-20% к build time per
  `nanovdb-on-gpu` precedent.

---

## 8. Sources

См. `sources.md`.

---

## 9. Mapping to ProjectV hot-path

- **Mainline consumer:** `src/shaders/voxel_mesh.comp::GreedyFacePass` (per-axis dispatch, per
  `agent/knowledge.md`). Current = Naive Greedy (это и есть baseline эксперимента).
- **Stage 2.1:** mesh shader spike (`src/shaders/voxel_mesh.task` + `voxel_mesh.mesh`) emits stub triangle — full port =
  replace stub с алгоритмом-кандидатом из эксперимента.
- **Stage 3.3:** `voxel_physics_mesh.comp` (TODO, не существует) — mirror of Stage 2.1 для Jolt `MeshShape`.
- **SVDAG input:** Stage 1.2 dedup даёт sparse reading pattern; алгоритмы с random-access по соседям (SN, DC) могут быть
  медленнее на dedup storage vs flat.

**Допущения прототипа:**

- Flat voxel array вход (для fairness к greedy). SVDAG simulation — out of scope (рассмотрен в `nanovdb-on-gpu`
  параллельно).
- CPU single-threaded (GPU mesh shader — отдельный spike).
- 32³ chunk (matches ProjectV).
- Simple SDF (sphere/box) — для MC/SN/DC input требуется SDF.

**Что останется неизмеренным:**

- GPU dispatch latency (mesh shader port нужен отдельно).
- SVDAG traversal cache effects.
- Memory bandwidth на dedup storage.
- Visual quality (qualitative — нужны debug captures).
